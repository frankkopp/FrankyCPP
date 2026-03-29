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
#include "tuning/optimizer/TuningState.h"

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
#include <iostream>
#include <numeric>
#include <optional>
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
      // Optional eval terms (USE_CONNECTED_ROOKS, USE_MINOR_CONNECTIVITY) already
      // default to true. USE_SPACE_EVAL defaults to false (confirmed dead by tuner).
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

      // Progress every 10 iterations
      if ((i + 1) % 10 == 0 || i == iterations - 1) {
        const double currentK = (kLow + kHigh) / 2.0;
        const double bestMSE  = std::min(mseLeft, mseRight);
        std::cout << std::format("  K iteration {:2d}/{}: K = {:.6f} [{:.6f}, {:.6f}], MSE = {:.10f}\n",
                                 i + 1, iterations, currentK, kLow, kHigh, bestMSE) << std::flush;
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
    std::ranges::sort(partials);
    const double totalError = std::accumulate(partials.begin(), partials.end(), 0.0);

    return totalError / static_cast<double>(n);
  }

  // =========================================================================
  // Activation flags (board-state analysis)
  // =========================================================================

  // For each position, analyze the board to determine which eval parameter
  // groups could have nonzero influence. This lets the incremental MSE
  // optimization skip re-evaluation of entries unaffected by a parameter change.
  //
  // The analysis is conservative: a group is marked active if the board state
  // contains the piece types or structures that the group's eval terms examine.
  // False positives (marking active when influence is actually zero) are safe —
  // they just reduce the speedup. False negatives would cause incorrect MSE.
  //
  // Group assignment (matches TuningParameter::assignParamGroup):
  //   0  = Tempo / misc         — always active
  //   1  = Pawn structure       — active if any pawns
  //   2  = Passed pawns         — active if any pawns (passers detected internally)
  //   3  = Pawn advance         — active if any pawns
  //   4  = Bishop pair          — active if either side has 2+ bishops
  //   5  = Knight               — active if any knights
  //   6  = Bishop               — active if any bishops
  //   7  = Rook                 — active if any rooks
  //   8  = Queen                — active if any queens
  //   9  = King safety          — always active (kings always present)
  //   10 = Threats              — active if either side has non-pawn pieces
  //   11 = Space                — active if any pawns (space = controlled squares behind pawns)
  //   12 = Coordination         — active if either side has 2+ rooks or 2+ minors
  void TexelTuner::computeActivationFlags(TuningDataset& dataset) const {
    if (dataset.empty()) return;

    const auto n = dataset.size();

    // Lambda to compute flags for a range of entries
    const auto computeRange = [&dataset](const std::size_t start, const std::size_t end) {
      Position position;
      for (auto i = start; i < end; ++i) {
        auto& entry = dataset[i];
        position.setFromFen(entry.fen);
        auto& flags = entry.activeParamGroups;
        flags.reset();

        // Piece bitboards
        const Bitboard whitePawns   = position.getPieceBb(WHITE, PAWN);
        const Bitboard blackPawns   = position.getPieceBb(BLACK, PAWN);
        const Bitboard whiteKnights = position.getPieceBb(WHITE, KNIGHT);
        const Bitboard blackKnights = position.getPieceBb(BLACK, KNIGHT);
        const Bitboard whiteBishops = position.getPieceBb(WHITE, BISHOP);
        const Bitboard blackBishops = position.getPieceBb(BLACK, BISHOP);
        const Bitboard whiteRooks   = position.getPieceBb(WHITE, ROOK);
        const Bitboard blackRooks   = position.getPieceBb(BLACK, ROOK);
        const Bitboard whiteQueens  = position.getPieceBb(WHITE, QUEEN);
        const Bitboard blackQueens  = position.getPieceBb(BLACK, QUEEN);

        const bool hasPawns   = (whitePawns | blackPawns) != Bitboard{0};
        const bool hasKnights = (whiteKnights | blackKnights) != Bitboard{0};
        const bool hasBishops = (whiteBishops | blackBishops) != Bitboard{0};
        const bool hasRooks   = (whiteRooks | blackRooks) != Bitboard{0};
        const bool hasQueens  = (whiteQueens | blackQueens) != Bitboard{0};

        // Group 0: Tempo / misc — always active
        flags.set(0);

        // Groups 1-3: Pawn structure, passed pawns, pawn advance — active if any pawns
        if (hasPawns) {
          flags.set(1);
          flags.set(2);
          flags.set(3);
        }

        // Group 4: Bishop pair — active if either side has 2+ bishops
        if (whiteBishops.popcount() >= 2 || blackBishops.popcount() >= 2) {
          flags.set(4);
        }

        // Group 5: Knight — active if any knights
        if (hasKnights) flags.set(5);

        // Group 6: Bishop — active if any bishops
        if (hasBishops) flags.set(6);

        // Group 7: Rook — active if any rooks
        if (hasRooks) flags.set(7);

        // Group 8: Queen — active if any queens
        if (hasQueens) flags.set(8);

        // Group 9: King safety — always active
        flags.set(9);

        // Group 10: Threats — active if either side has non-pawn pieces
        if (hasKnights || hasBishops || hasRooks || hasQueens) {
          flags.set(10);
        }

        // Group 11: Space — active if any pawns (space = squares behind pawn chain)
        if (hasPawns) flags.set(11);

        // Group 12: Coordination — connected rooks (2+ rooks) or minor connectivity (2+ minors)
        const bool connectedRooks = whiteRooks.popcount() >= 2 || blackRooks.popcount() >= 2;
        const bool minorConnect   = (whiteKnights.popcount() + whiteBishops.popcount() >= 2) ||
                                    (blackKnights.popcount() + blackBishops.popcount() >= 2);
        if (connectedRooks || minorConnect) {
          flags.set(12);
        }
      }
    };

    // Parallel dispatch if thread pool available
    if (numThreads_ > 1 && threadPool_) {
      const auto chunkSize = (n + numThreads_ - 1) / numThreads_;
      std::vector<std::future<void>> futures;
      futures.reserve(numThreads_);

      for (int t = 0; t < numThreads_; ++t) {
        const auto start = static_cast<std::size_t>(t) * chunkSize;
        if (start >= n) break;
        const auto end = std::min(start + chunkSize, n);
        futures.push_back(threadPool_->enqueue([&computeRange, start, end] { computeRange(start, end); }));
      }
      for (auto& f : futures) { f.get(); }
    }
    else {
      computeRange(0, n);
    }

    LOG__INFO(common::Logger::get().TUNING_LOG,
              "Activation flags computed for {} entries", n);
  }

  // =========================================================================
  // Full eval pass with caching
  // =========================================================================

  // Evaluates all entries, stores per-entry squared errors in cachedSquaredError,
  // and computes totalSquaredError_ for use by computeMSEIncremental().
  // This is the "baseline" pass before coordinate descent with incremental MSE.
  double TexelTuner::computeAndCacheErrors(TuningDataset& dataset, const double K) const {
    if (threadEvaluators_.empty()) {
      throw std::logic_error("TexelTuner::computeAndCacheErrors: evaluator not created");
    }
    if (dataset.empty()) {
      totalSquaredError_ = 0.0;
      return 0.0;
    }

    const auto n = dataset.size();

    // Lambda to evaluate a range and return partial SSE
    const auto evalRange = [this, &dataset, K](const int threadIdx, const std::size_t start, const std::size_t end) -> double {
      auto& evaluator = *threadEvaluators_[threadIdx];
      Position position;
      double partialSSE = 0.0;

      for (auto i = start; i < end; ++i) {
        auto& entry = dataset[i];
        position.setFromFen(entry.fen);
        const Value rawEval = evaluator.evaluate(position);
        const double whiteRelEval =
          static_cast<double>(static_cast<int>(rawEval)) * position.getNextPlayer().sign();
        const double predicted = sigmoid(K, whiteRelEval);
        const double error     = entry.result - predicted;
        const double se        = error * error;
        entry.cachedSquaredError = se;
        partialSSE += se;
      }
      return partialSSE;
    };

    // Parallel dispatch
    if (numThreads_ > 1 && threadPool_) {
      const auto chunkSize = (n + numThreads_ - 1) / numThreads_;
      std::vector<std::future<double>> futures;
      futures.reserve(numThreads_);

      for (int t = 0; t < numThreads_; ++t) {
        const auto start = static_cast<std::size_t>(t) * chunkSize;
        if (start >= n) break;
        const auto end = std::min(start + chunkSize, n);
        futures.push_back(threadPool_->enqueue([&evalRange, t, start, end] { return evalRange(t, start, end); }));
      }

      std::vector<double> partials;
      partials.reserve(futures.size());
      for (auto& f : futures) { partials.push_back(f.get()); }
      std::ranges::sort(partials);
      totalSquaredError_ = std::accumulate(partials.begin(), partials.end(), 0.0);
    }
    else {
      totalSquaredError_ = evalRange(0, 0, n);
    }

    return totalSquaredError_ / static_cast<double>(n);
  }

  // =========================================================================
  // Incremental MSE computation
  // =========================================================================

  // Only re-evaluates entries where the given paramGroup is active.
  // For inactive entries, their cachedSquaredError contributes unchanged.
  // The result is: (totalSquaredError_ + deltaSSE) / N
  // where deltaSSE = sum of (freshSE - cachedSE) for active entries.
  //
  // This does NOT modify the cache. Call updateCacheForGroup() to commit.
  double TexelTuner::computeMSEIncremental(const TuningDataset& dataset, const double K, const int paramGroup) const {
    if (threadEvaluators_.empty()) {
      throw std::logic_error("TexelTuner::computeMSEIncremental: evaluator not created");
    }
    if (dataset.empty()) {
      return 0.0;
    }

    const auto n = dataset.size();

    // Lambda to compute deltaSSE for a range
    const auto deltaRange = [this, &dataset, K, paramGroup](const int threadIdx, const std::size_t start, const std::size_t end) -> double {
      auto& evaluator = *threadEvaluators_[threadIdx];
      Position position;
      double partialDelta = 0.0;

      for (auto i = start; i < end; ++i) {
        const auto& entry = dataset[i];
        if (!entry.activeParamGroups.test(paramGroup)) continue;

        position.setFromFen(entry.fen);
        const Value rawEval = evaluator.evaluate(position);
        const double whiteRelEval =
          static_cast<double>(static_cast<int>(rawEval)) * position.getNextPlayer().sign();
        const double predicted = sigmoid(K, whiteRelEval);
        const double error     = entry.result - predicted;
        const double freshSE   = error * error;
        partialDelta += freshSE - entry.cachedSquaredError;
      }
      return partialDelta;
    };

    double deltaSSE = 0.0;

    if (numThreads_ > 1 && threadPool_) {
      const auto chunkSize = (n + numThreads_ - 1) / numThreads_;
      std::vector<std::future<double>> futures;
      futures.reserve(numThreads_);

      for (int t = 0; t < numThreads_; ++t) {
        const auto start = static_cast<std::size_t>(t) * chunkSize;
        if (start >= n) break;
        const auto end = std::min(start + chunkSize, n);
        futures.push_back(threadPool_->enqueue([&deltaRange, t, start, end] { return deltaRange(t, start, end); }));
      }

      std::vector<double> partials;
      partials.reserve(futures.size());
      for (auto& f : futures) { partials.push_back(f.get()); }
      std::ranges::sort(partials);
      deltaSSE = std::accumulate(partials.begin(), partials.end(), 0.0);
    }
    else {
      deltaSSE = deltaRange(0, 0, n);
    }

    return (totalSquaredError_ + deltaSSE) / static_cast<double>(n);
  }

  // =========================================================================
  // Cache update for a parameter group
  // =========================================================================

  // Re-evaluates entries where paramGroup is active and updates their
  // cachedSquaredError. Also adjusts totalSquaredError_ to reflect the changes.
  // Call after deciding to keep a parameter change.
  void TexelTuner::updateCacheForGroup(TuningDataset& dataset, const double K, const int paramGroup) const {
    if (threadEvaluators_.empty()) {
      throw std::logic_error("TexelTuner::updateCacheForGroup: evaluator not created");
    }
    if (dataset.empty()) return;

    const auto n = dataset.size();

    // Lambda to update cache for a range and return the deltaSSE
    const auto updateRange = [this, &dataset, K, paramGroup](const int threadIdx, const std::size_t start, const std::size_t end) -> double {
      auto& evaluator = *threadEvaluators_[threadIdx];
      Position position;
      double partialDelta = 0.0;

      for (auto i = start; i < end; ++i) {
        auto& entry = dataset[i];
        if (!entry.activeParamGroups.test(paramGroup)) continue;

        position.setFromFen(entry.fen);
        const Value rawEval = evaluator.evaluate(position);
        const double whiteRelEval =
          static_cast<double>(static_cast<int>(rawEval)) * position.getNextPlayer().sign();
        const double predicted = sigmoid(K, whiteRelEval);
        const double error     = entry.result - predicted;
        const double freshSE   = error * error;
        partialDelta += freshSE - entry.cachedSquaredError;
        entry.cachedSquaredError = freshSE;
      }
      return partialDelta;
    };

    if (numThreads_ > 1 && threadPool_) {
      const auto chunkSize = (n + numThreads_ - 1) / numThreads_;
      std::vector<std::future<double>> futures;
      futures.reserve(numThreads_);

      for (int t = 0; t < numThreads_; ++t) {
        const auto start = static_cast<std::size_t>(t) * chunkSize;
        if (start >= n) break;
        const auto end = std::min(start + chunkSize, n);
        futures.push_back(threadPool_->enqueue([&updateRange, t, start, end] { return updateRange(t, start, end); }));
      }

      std::vector<double> partials;
      partials.reserve(futures.size());
      for (auto& f : futures) { partials.push_back(f.get()); }
      std::ranges::sort(partials);
      totalSquaredError_ += std::accumulate(partials.begin(), partials.end(), 0.0);
    }
    else {
      totalSquaredError_ += updateRange(0, 0, n);
    }
  }

  // =========================================================================
  // Monotonicity enforcement
  // =========================================================================

  // For array parameters with monotonicity constraints, clamp the value
  // at arrayIndex relative to its neighbors in the same array.
  //
  // The params vector is ordered by buildFromRegistry(): array elements are
  // contiguous and in index order (e.g., KING_SAFETY_TABLE[0], [1], ..., [15]).
  // We find the neighbors by scanning the vector for matching configName
  // and adjacent arrayIndex values.
  //
  // Only the modified element is clamped — neighbors are not changed.
  // This ensures the coordinate descent loop converges (no cascading clamps).

  void TexelTuner::enforceMonotonicity(TuningParameter& param,
                                       const std::vector<TuningParameter>& params) {
    // No-op for scalars or unconstrained params
    if (param.arrayIndex < 0 || param.monotonicity == MonotonicityConstraint::NONE) {
      return;
    }

    // Find neighbor values in the same array.
    // Since array elements are contiguous in the vector, we can search by
    // configName + arrayIndex.  Use optional to distinguish "no neighbor" from "value 0".
    std::optional<int> prevValue;
    std::optional<int> nextValue;

    for (const auto& other : params) {
      if (other.configName != param.configName || other.arrayIndex < 0) continue;
      if (other.arrayIndex == param.arrayIndex - 1) {
        prevValue = other.currentValue;
      }
      else if (other.arrayIndex == param.arrayIndex + 1) {
        nextValue = other.currentValue;
      }
    }

    if (param.monotonicity == MonotonicityConstraint::NON_DECREASING) {
      // array[i] >= array[i-1] (floor from predecessor)
      if (prevValue.has_value() && param.currentValue < prevValue.value()) {
        param.currentValue = prevValue.value();
      }
      // array[i] <= array[i+1] (ceiling from successor)
      if (nextValue.has_value() && param.currentValue > nextValue.value()) {
        param.currentValue = nextValue.value();
      }
    }
    else if (param.monotonicity == MonotonicityConstraint::NON_INCREASING) {
      // array[i] <= array[i-1] (ceiling from predecessor)
      if (prevValue.has_value() && param.currentValue > prevValue.value()) {
        param.currentValue = prevValue.value();
      }
      // array[i] >= array[i+1] (floor from successor)
      if (nextValue.has_value() && param.currentValue < nextValue.value()) {
        param.currentValue = nextValue.value();
      }
    }
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
  // Incremental MSE optimization (Sprint 6.5):
  //   Before the loop, activation flags are computed per entry based on board
  //   state. A full eval pass caches per-entry squared errors. During parameter
  //   trials, computeMSEIncremental() only re-evaluates entries where the
  //   parameter's group is active, using cached errors for the rest. After
  //   committing a change, updateCacheForGroup() refreshes the cache.
  //   This avoids redundant evaluation of positions unaffected by a change.
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
                                  const int maxPasses,
                                  const std::string& checkpointPath,
                                  const std::string& datasetPath,
                                  const int startPass) {
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

    // Helper: compute full MSE (for test set, which doesn't use incremental)
    const auto fullMSE = [&](const TuningDataset& ds) {
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

    // Compute activation flags and initial error cache (mutable cast needed for cache population)
    auto& mutableTrainSet = const_cast<TuningDataset&>(trainSet);
    std::cout << "Computing activation flags for " << trainSet.size() << " positions...\n" << std::flush;
    computeActivationFlags(mutableTrainSet);
    std::cout << "  Activation flags computed.\n" << std::flush;

    // Count active entries per group for logging
    std::array<int, NUM_PARAM_GROUPS> activeCount{};
    for (const auto & i : trainSet) {
      for (int g = 0; g < NUM_PARAM_GROUPS; ++g) {
        if (i.activeParamGroups.test(g)) ++activeCount[g];
      }
    }
    for (int g = 0; g < 13; ++g) {
      LOG__DEBUG(common::Logger::get().TUNING_LOG,
                "  Group {:2d}: {:6d}/{} entries active ({:.1f}%)",
                g, activeCount[g], trainSet.size(),
                100.0 * activeCount[g] / static_cast<double>(trainSet.size()));
    }

    // Full eval pass to populate cache
    std::cout << "Computing initial error cache...\n" << std::flush;
    const double baselineMSE = computeAndCacheErrors(mutableTrainSet, K_);
    std::cout << "  Baseline train MSE: " << std::format("{:.10f}", baselineMSE) << "\n" << std::flush;
    LOG__INFO(tuningLog, "Coordinate descent: {} params, {} positions, K = {:.6f}",
              params.size(), trainSet.size(), K_);
    LOG__INFO(tuningLog, "Baseline train MSE: {:.10f}", baselineMSE);
    if (testSet && !testSet->empty()) {
      const double testMSE = fullMSE(*testSet);
      LOG__INFO(tuningLog, "Baseline test MSE:  {:.10f}", testMSE);
    }

    const auto totalStart = steady_clock::now();
    bool improved = true;
    int pass = startPass;

    while (improved && pass < maxPasses) {
      improved = false;
      ++pass;

      const auto passStart = steady_clock::now();
      int paramsChanged = 0;
      std::string biggestMoverName;
      double biggestMoverDelta = 0.0;

      double currentMSE = totalSquaredError_ / static_cast<double>(trainSet.size());

      const auto totalParams = static_cast<int>(params.size());
      int paramIdx = 0;

      for (auto& param : params) {
        // Within-pass progress every 20 params
        if (paramIdx % 20 == 0) {
          std::cout << std::format("  Pass {:3d}: param {:3d}/{} ...\r", pass, paramIdx, totalParams) << std::flush;
        }
        ++paramIdx;

        const int originalValue = param.currentValue;
        const int group = param.paramGroup;

        // --- Try +delta ---
        const int plusValue = std::clamp(originalValue + param.delta, param.minValue, param.maxValue);
        double msePlus = currentMSE;
        if (plusValue != originalValue) {
          param.currentValue = plusValue;
          enforceMonotonicity(param, params);
          param.applyToConfig(search, eval);
          msePlus = computeMSEIncremental(trainSet, K_, group);
        }

        // --- Try -delta (from original, not from +delta) ---
        const int minusValue = std::clamp(originalValue - param.delta, param.minValue, param.maxValue);
        double mseMinus = currentMSE;
        if (minusValue != originalValue) {
          param.currentValue = minusValue;
          enforceMonotonicity(param, params);
          param.applyToConfig(search, eval);
          mseMinus = computeMSEIncremental(trainSet, K_, group);
        }

        // --- Decide: keep best direction, or revert ---
        if (msePlus < currentMSE && msePlus <= mseMinus && plusValue != originalValue) {
          // Keep +delta — re-apply (may have been overwritten by -delta trial) and update cache
          param.currentValue = plusValue;
          enforceMonotonicity(param, params);
          param.applyToConfig(search, eval);
          updateCacheForGroup(mutableTrainSet, K_, group);
          const double delta = currentMSE - msePlus;
          currentMSE = totalSquaredError_ / static_cast<double>(trainSet.size());
          improved = true;
          ++paramsChanged;
          if (delta > biggestMoverDelta) {
            biggestMoverDelta = delta;
            biggestMoverName  = param.name;
          }
        }
        else if (mseMinus < currentMSE && minusValue != originalValue) {
          // Keep -delta (already applied) — update cache
          updateCacheForGroup(mutableTrainSet, K_, group);
          const double delta = currentMSE - mseMinus;
          currentMSE = totalSquaredError_ / static_cast<double>(trainSet.size());
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

      // Per-pass summary — clear the in-line progress first
      std::cout << std::string(60, ' ') << "\r" << std::flush;
      const auto passElapsed = steady_clock::now() - passStart;
      const auto passSeconds = std::chrono::duration<double>(passElapsed).count();

      double testMSEForCheckpoint = 0.0;
      if (testSet && !testSet->empty()) {
        testMSEForCheckpoint = fullMSE(*testSet);
        const auto summary = std::format(
          "Pass {:3d}: train MSE = {:.10f}, test MSE = {:.10f}, changed = {:3d}/{}, "
          "biggest = {} (d{:.2e}), {:.1f}s",
          pass, currentMSE, testMSEForCheckpoint, paramsChanged, params.size(),
          biggestMoverName.empty() ? "-" : biggestMoverName, biggestMoverDelta, passSeconds);
        LOG__INFO(tuningLog, "{}", summary);
        std::cout << summary << "\n" << std::flush;
      }
      else {
        const auto summary = std::format(
          "Pass {:3d}: train MSE = {:.10f}, changed = {:3d}/{}, "
          "biggest = {} (d{:.2e}), {:.1f}s",
          pass, currentMSE, paramsChanged, params.size(),
          biggestMoverName.empty() ? "-" : biggestMoverName, biggestMoverDelta, passSeconds);
        LOG__INFO(tuningLog, "{}", summary);
        std::cout << summary << "\n" << std::flush;
      }

      // Save checkpoint after each pass (if path provided)
      if (!checkpointPath.empty()) {
        TuningState checkpoint;
        checkpoint.completedPasses = pass;
        checkpoint.bestTrainMSE    = currentMSE;
        checkpoint.bestTestMSE     = testMSEForCheckpoint;
        checkpoint.K               = K_;
        checkpoint.datasetPath     = datasetPath;
        checkpoint.captureFromParams(params);
        try {
          checkpoint.saveToYaml(checkpointPath);
        }
        catch (const std::exception& e) {
          LOG__WARN(tuningLog, "Failed to save checkpoint: {}", e.what());
        }
      }
    }

    const auto totalElapsed = steady_clock::now() - totalStart;
    const auto totalSeconds = std::chrono::duration<double>(totalElapsed).count();
    const double finalMSE = totalSquaredError_ / static_cast<double>(trainSet.size());

    LOG__INFO(tuningLog, "Coordinate descent complete: {} passes, final train MSE = {:.10f}, "
              "improvement = {:.2e}, total time = {:.1f}s",
              pass, finalMSE, baselineMSE - finalMSE, totalSeconds);
  }

} // namespace tuning
