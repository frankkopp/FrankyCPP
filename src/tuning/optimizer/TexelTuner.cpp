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
#include "config/ConfigManager.h"
#include "engine/Evaluator.h"
#include "types/types.h"

#include <stdexcept>

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
    evaluator_ = std::make_unique<engine::Evaluator>();
    // No PawnTT attached — USE_PAWN_TT is disabled via setupEvalOverrides()
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
    if (!evaluator_) {
      throw std::logic_error("TexelTuner::computeMSE: evaluator not created — call createEvaluator() first");
    }
    if (dataset.empty()) {
      return 0.0;
    }

    // Stack-local Position for FEN reconstruction (avoids heap allocation per entry).
    // Position is ~33KB — fine on the stack for a single instance.
    Position position;

    double sumSquaredError = 0.0;
    const auto n = dataset.size();

    for (std::size_t i = 0; i < n; ++i) {
      const auto& entry = dataset[i];

      // Reconstruct position from FEN (reuses existing object, avoids copy)
      position.setFromFen(entry.fen);

      // Evaluate from side-to-move perspective
      const Value rawEval = evaluator_->evaluate(position);

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
    if (!evaluator_) {
      throw std::logic_error("TexelTuner::tuneK: evaluator not created — call createEvaluator() first");
    }

    for (int i = 0; i < iterations; ++i) {
      const double kLeft  = kLow  + (kHigh - kLow) / 3.0;
      const double kRight = kHigh - (kHigh - kLow) / 3.0;

      const double mseLeft  = computeMSE(dataset, kLeft);
      const double mseRight = computeMSE(dataset, kRight);

      if (mseLeft < mseRight) {
        kHigh = kRight;
      } else {
        kLow = kLeft;
      }
    }

    K_ = (kLow + kHigh) / 2.0;
    return K_;
  }

} // namespace tuning
