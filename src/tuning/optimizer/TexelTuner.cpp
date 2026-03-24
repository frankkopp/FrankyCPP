// FrankyCPP
// Copyright (c) 2018-2026 Frank Kopp
//
// MIT License
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "tuning/optimizer/TexelTuner.h"

#include "chesscore/Position.h"
#include "common/Logging.h"
#include "common/ThreadPool.h"
#include "config/ConfigManager.h"
#include "engine/Evaluator.h"
#include "types/types.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <future>
#include <numeric>
#include <stdexcept>
#include <thread>

using namespace chess;

namespace tuning {

  // =========================================================================
  // Construction / destruction
  // =========================================================================

  TexelTuner::TexelTuner() = default;

  TexelTuner::~TexelTuner() = default;

  // =========================================================================
  // Eval config overrides
  // =========================================================================

  void TexelTuner::setupEvalOverrides() {
    CONFIG_OVERRIDE_START()
      // Must evaluate all terms for every position — no early exit
      e.USE_LAZY_EVAL = false;
      // Disable pawn TT — avoids stale cache entries after param changes
      e.USE_PAWN_TT = false;
      // Enable optional eval terms so the tuner can optimize (or zero out) their weights
      e.USE_SPACE_EVAL        = true;
      e.USE_CONNECTED_ROOKS   = true;
      e.USE_MINOR_CONNECTIVITY = true;
    CONFIG_OVERRIDE_END();
  }

  // =========================================================================
  // Evaluator creation
  // =========================================================================

  void TexelTuner::createEvaluator() {
    threadEvaluators_.clear();
    threadEvaluators_.push_back(std::make_unique<engine::Evaluator>());
    numThreads_ = 1;
    // No PawnTT attached — USE_PAWN_TT is disabled via setupEvalOverrides()
  }

  void TexelTuner::createEvaluators(int numThreads) {
    // Clamp to [1, hardware_concurrency]
    const int maxThreads = static_cast<int>(std::thread::hardware_concurrency());
    numThreads = std::clamp(numThreads, 1, maxThreads > 0 ? maxThreads : 1);

    threadEvaluators_.clear();
    threadEvaluators_.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i) {
      threadEvaluators_.push_back(std::make_unique<engine::Evaluator>());
      // No PawnTT attached — USE_PAWN_TT is disabled via setupEvalOverrides()
    }

    // Create thread pool (replacing any existing one)
    threadPool_ = std::make_unique<common::ThreadPool>(numThreads);
    numThreads_ = numThreads;

    LOG__INFO(common::Logger::get().TUNING_LOG,
              "Created {} evaluator(s) with thread pool for parallel MSE", numThreads);
  }

  // =========================================================================
  // Sigmoid
  // =========================================================================

  // The sigmoid maps a centipawn eval to a "predicted game outcome" in [0, 1].
  //
  // Intuition:
  //   eval =    0 cp  →  0.5   (equal position → coin flip)
  //   eval = +100 cp  →  ~0.64 (one pawn up → ~64% chance White wins)
  //   eval = +400 cp  →  ~0.91 (queen up → ~91% chance White wins)
  //   eval =   -∞     →  0.0   (Black is winning)
  //   eval =   +∞     →  1.0   (White is winning)
  //
  // The formula is the same shape as the Elo expected-score formula.
  // K controls how steeply the curve rises: small K → flat curve (eval
  // doesn't predict outcomes well), large K → steep curve (small eval
  // differences imply near-certain outcomes).
  double TexelTuner::sigmoid(const double K, const double eval) {
    return 1.0 / (1.0 + std::pow(10.0, -K * eval / 400.0));
  }

  // =========================================================================
  // MSE computation (single-threaded)
  // =========================================================================

  // Mean Squared Error: measures how well the current eval parameters
  // predict actual game outcomes.
  //
  // For every position in the dataset:
  //   1. Set up the board from the stored FEN string
  //   2. Run the engine's static eval (no search — just evaluate())
  //   3. Convert eval to White's perspective (dataset labels are White-relative)
  //   4. Map eval → predicted outcome via sigmoid
  //   5. Compare predicted outcome to actual game result (1.0 / 0.5 / 0.0)
  //   6. Square the difference and accumulate
  //
  // The MSE is the average of all squared errors. Lower = better fit.
  // A perfect eval would predict exactly the game outcome for every position,
  // giving MSE = 0 (impossible in practice due to noise, blunders, etc.).
  //
  // This is the objective function that the tuner minimizes by adjusting
  // eval parameters. No search is needed — only the fast static evaluate()
  // — so millions of positions can be processed in seconds.
  double TexelTuner::computeMSE(const TuningDataset& dataset, const double K) const {
    if (threadEvaluators_.empty()) {
      throw std::logic_error("TexelTuner::computeMSE: evaluator not created — call createEvaluator() first");
    }
    if (dataset.empty()) {
      return 0.0;
    }

    // Stack-local Position for FEN reconstruction (avoids heap allocation per entry).
    // Position is ~33KB — fine on the stack for a single instance.
    Position position;

    // Use the first evaluator for single-threaded computation.
    auto& evaluator = *threadEvaluators_[0];

    double sumSquaredError = 0.0;
    const auto n = dataset.size();

    for (std::size_t i = 0; i < n; ++i) {
      const auto& entry = dataset[i];

      // Reconstruct position from FEN (reuses existing object, avoids copy)
      position.setFromFen(entry.fen);

      // Evaluate from side-to-move perspective
      const Value rawEval = evaluator.evaluate(position);

      // Convert to White-relative: negate when Black to move
      // evaluate() returns: whiteScore * nextPlayer.sign()
      // To undo: multiply by nextPlayer.sign() again
      const double whiteRelativeEval =
        static_cast<double>(static_cast<int>(rawEval)) * position.getNextPlayer().sign();

      // Apply sigmoid and compute squared error against game result
      const double predicted = sigmoid(K, whiteRelativeEval);
      const double error     = entry.result - predicted;
      sumSquaredError += error * error;
    }

    return sumSquaredError / static_cast<double>(n);
  }

  // =========================================================================
  // K-tuning via ternary search
  // =========================================================================

  // Before tuning eval parameters, we first find the best scaling constant K.
  //
  // K controls how the sigmoid maps centipawn scores to win probabilities.
  // If K is wrong, the sigmoid will systematically over- or under-predict
  // outcomes, and the parameter optimizer will chase the wrong targets.
  //
  // Ternary search: the MSE-vs-K curve is unimodal (one minimum), so we
  // can bracket the optimum and narrow it down. We split [kLow, kHigh]
  // into thirds, evaluate MSE at the two interior points, and discard
  // the outer third with the worse MSE. After 50 iterations the interval
  // is negligibly small (~10^-8) and we take the midpoint.
  //
  // K is tuned once with the current (default) eval parameters, then held
  // fixed while the coordinate descent loop optimizes the eval weights.
  // Typical result: K ≈ 1.0–1.5 for classical hand-crafted evals.
  double TexelTuner::tuneK(const TuningDataset& dataset,
                           double kLow, double kHigh,
                           const int iterations) {
    if (threadEvaluators_.empty()) {
      throw std::logic_error("TexelTuner::tuneK: evaluator not created — call createEvaluator() first");
    }

    // Use parallel MSE when multiple threads are available
    const bool useParallel = numThreads_ > 1 && threadPool_ != nullptr;

    for (int i = 0; i < iterations; ++i) {
      const double kLeft  = kLow  + (kHigh - kLow) / 3.0;
      const double kRight = kHigh - (kHigh - kLow) / 3.0;

      const double mseLeft  = useParallel ? computeMSEParallel(dataset, kLeft)  : computeMSE(dataset, kLeft);
      const double mseRight = useParallel ? computeMSEParallel(dataset, kRight) : computeMSE(dataset, kRight);

      if (mseLeft < mseRight) {
        kHigh = kRight;
      } else {
        kLow = kLeft;
      }
    }

    K_ = (kLow + kHigh) / 2.0;
    return K_;
  }

  // =========================================================================
  // MSE computation (multi-threaded)
  // =========================================================================

  // Parallel MSE splits the dataset into contiguous chunks — one per worker
  // thread — and dispatches each chunk to the ThreadPool. Each task uses its
  // own Evaluator instance (thread-local scratch variables) and a stack-local
  // Position (~33 KB) for FEN reconstruction. The dataset is read-only.
  //
  // After all tasks complete, the partial sums are sorted smallest-to-largest
  // before aggregation. This makes the floating-point addition order
  // deterministic regardless of thread scheduling, ensuring reproducible MSE
  // values across runs (important for coordinate descent convergence checks).
  double TexelTuner::computeMSEParallel(const TuningDataset& dataset, const double K) const {
    if (threadEvaluators_.empty()) {
      throw std::logic_error("TexelTuner::computeMSEParallel: evaluator not created — call createEvaluators() first");
    }
    if (dataset.empty()) {
      return 0.0;
    }

    // Fall back to single-threaded if only one thread is configured
    if (numThreads_ <= 1 || !threadPool_) {
      return computeMSE(dataset, K);
    }

    const auto n         = dataset.size();
    const auto chunkSize = (n + numThreads_ - 1) / numThreads_;

    // Dispatch one task per thread
    std::vector<std::future<double>> futures;
    futures.reserve(numThreads_);

    for (int t = 0; t < numThreads_; ++t) {
      const auto start = static_cast<std::size_t>(t) * chunkSize;
      if (start >= n) break;
      const auto end = std::min(start + chunkSize, n);

      futures.push_back(threadPool_->enqueue([this, &dataset, K, t, start, end]() -> double {
        // Each thread gets its own Evaluator (thread-local scratch variables)
        auto& evaluator = *threadEvaluators_[t];
        // Stack-local Position for FEN reconstruction (~33 KB, fine on stack)
        Position position;

        double partialSum = 0.0;
        for (auto i = start; i < end; ++i) {
          const auto& entry = dataset[i];

          position.setFromFen(entry.fen);
          const Value rawEval = evaluator.evaluate(position);

          // Convert to White-relative
          const double whiteRelativeEval =
            static_cast<double>(static_cast<int>(rawEval)) * position.getNextPlayer().sign();

          const double predicted = sigmoid(K, whiteRelativeEval);
          const double error     = entry.result - predicted;
          partialSum += error * error;
        }
        return partialSum;
      }));
    }

    // Collect partial sums
    std::vector<double> partials;
    partials.reserve(futures.size());
    for (auto& f : futures) {
      partials.push_back(f.get());
    }

    // Sort for deterministic FP aggregation (smallest to largest reduces rounding error)
    std::sort(partials.begin(), partials.end());
    const double totalError = std::accumulate(partials.begin(), partials.end(), 0.0);

    return totalError / static_cast<double>(n);
  }

  // =========================================================================
  // Coordinate descent optimizer
  // =========================================================================

  // Coordinate descent: the simplest effective optimizer for ~85 parameters.
  //
  // For each parameter in the flat vector, try incrementing by +delta and
  // decrementing by -delta. Keep whichever direction produces a lower MSE.
  // If neither direction improves, revert. One full sweep over all parameters
  // is one "pass". Repeat until no parameter improves or maxPasses is reached.
  //
  // Key properties:
  //   - Parameter changes are applied to the live EvalConfigData via
  //     TuningParameter::applyToConfig(). All evaluator instances read from
  //     the same shared EvalConfig reference, so changes take effect
  //     immediately for the next MSE computation.
  //   - No parallelism in the outer loop (parameters are tested sequentially).
  //     Parallelism is in the inner MSE computation (positions split across threads).
  //   - Test MSE is computed once per pass for overfitting monitoring.
  //     If test MSE rises while train MSE falls, the tuner is memorizing noise.
  //   - The "biggest mover" tracking shows which parameters are most sensitive,
  //     useful for debugging and for expanding parameter bounds in future runs.
  void TexelTuner::tuneParameters(const TuningDataset& trainSet,
                                  const TuningDataset* testSet,
                                  std::vector<TuningParameter>& params,
                                  const int maxPasses) {
    if (threadEvaluators_.empty()) {
      throw std::logic_error("TexelTuner::tuneParameters: evaluator not created — call createEvaluators() first");
    }
    if (trainSet.empty()) {
      LOG__WARN(common::Logger::get().TUNING_LOG, "tuneParameters: empty training set — nothing to optimize");
      return;
    }
    if (params.empty()) {
      LOG__WARN(common::Logger::get().TUNING_LOG, "tuneParameters: no parameters to optimize");
      return;
    }

    const auto& tuningLog = common::Logger::get().TUNING_LOG;
    const bool useParallel = numThreads_ > 1 && threadPool_ != nullptr;

    // Helper: compute MSE using the best available method
    const auto mse = [&](const TuningDataset& ds) {
      return useParallel ? computeMSEParallel(ds, K_) : computeMSE(ds, K_);
    };

    // Get mutable config references for parameter application.
    // applyOverrides() provides the official API for mutable access to the live config.
    config::SearchConfigData* pSearch = nullptr;
    config::EvalConfigData* pEval     = nullptr;
    config::ConfigManager::instance().applyOverrides([&](auto& s, auto& e) {
      pSearch = &s;
      pEval   = &e;
    });
    auto& search = *pSearch;
    auto& eval   = *pEval;

    // Baseline MSE
    const double baselineMSE = mse(trainSet);
    LOG__INFO(tuningLog, "Coordinate descent: {} params, {} positions, K = {:.6f}",
              params.size(), trainSet.size(), K_);
    LOG__INFO(tuningLog, "Baseline train MSE: {:.10f}", baselineMSE);
    if (testSet && !testSet->empty()) {
      const double testMSE = mse(*testSet);
      LOG__INFO(tuningLog, "Baseline test MSE:  {:.10f}", testMSE);
    }

    const auto totalStart = steady_clock::now();
    bool improved = true;
    int pass = 0;

    while (improved && pass < maxPasses) {
      improved = false;
      ++pass;

      const auto passStart = steady_clock::now();
      int paramsChanged = 0;
      std::string biggestMoverName;
      double biggestMoverDelta = 0.0;

      double currentMSE = mse(trainSet);

      for (auto& param : params) {
        const int originalValue = param.currentValue;

        // --- Try +delta ---
        const int plusValue = std::clamp(originalValue + param.delta, param.minValue, param.maxValue);
        if (plusValue != originalValue) {
          param.currentValue = plusValue;
          param.applyToConfig(search, eval);
        }
        const double msePlus = (plusValue != originalValue) ? mse(trainSet) : currentMSE;

        // --- Try -delta (from original, not from +delta) ---
        const int minusValue = std::clamp(originalValue - param.delta, param.minValue, param.maxValue);
        if (minusValue != originalValue) {
          param.currentValue = minusValue;
          param.applyToConfig(search, eval);
        }
        const double mseMinus = (minusValue != originalValue) ? mse(trainSet) : currentMSE;

        // --- Decide: keep best direction, or revert ---
        if (msePlus < currentMSE && msePlus <= mseMinus && plusValue != originalValue) {
          // Keep +delta
          param.currentValue = plusValue;
          param.applyToConfig(search, eval);
          const double delta = currentMSE - msePlus;
          currentMSE = msePlus;
          improved = true;
          ++paramsChanged;
          if (delta > biggestMoverDelta) {
            biggestMoverDelta = delta;
            biggestMoverName  = param.name;
          }
        }
        else if (mseMinus < currentMSE && minusValue != originalValue) {
          // Keep -delta (already applied)
          const double delta = currentMSE - mseMinus;
          currentMSE = mseMinus;
          improved = true;
          ++paramsChanged;
          if (delta > biggestMoverDelta) {
            biggestMoverDelta = delta;
            biggestMoverName  = param.name;
          }
        }
        else {
          // Revert to original
          param.currentValue = originalValue;
          param.applyToConfig(search, eval);
        }
      }

      // Per-pass summary
      const auto passElapsed = steady_clock::now() - passStart;
      const auto passSeconds = std::chrono::duration<double>(passElapsed).count();

      if (testSet && !testSet->empty()) {
        const double testMSE = mse(*testSet);
        LOG__INFO(tuningLog,
                  "Pass {:3d}: train MSE = {:.10f}, test MSE = {:.10f}, changed = {:3d}/{}, "
                  "biggest = {} (Δ{:.2e}), {:.1f}s",
                  pass, currentMSE, testMSE, paramsChanged, params.size(),
                  biggestMoverName.empty() ? "-" : biggestMoverName, biggestMoverDelta, passSeconds);
      }
      else {
        LOG__INFO(tuningLog,
                  "Pass {:3d}: train MSE = {:.10f}, changed = {:3d}/{}, "
                  "biggest = {} (Δ{:.2e}), {:.1f}s",
                  pass, currentMSE, paramsChanged, params.size(),
                  biggestMoverName.empty() ? "-" : biggestMoverName, biggestMoverDelta, passSeconds);
      }
    }

    const auto totalElapsed = steady_clock::now() - totalStart;
    const auto totalSeconds = std::chrono::duration<double>(totalElapsed).count();
    const double finalMSE = mse(trainSet);

    LOG__INFO(tuningLog, "Coordinate descent complete: {} passes, final train MSE = {:.10f}, "
              "improvement = {:.2e}, total time = {:.1f}s",
              pass, finalMSE, baselineMSE - finalMSE, totalSeconds);
  }

} // namespace tuning
