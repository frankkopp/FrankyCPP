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

#ifndef FRANKYCPP_TEXELTUNER_H
#define FRANKYCPP_TEXELTUNER_H

//=============================================================================
// TexelTuner.h - Texel Tuning Optimizer for Evaluation Parameters
//=============================================================================
//
// Implements Texel's Tuning Method (Peter Österlund, 2014): supervised
// regression that optimizes eval parameters by minimizing the mean squared
// error between sigmoid-mapped static evaluations and game outcomes.
//
// Sprint 6.3 Scope (core math):
//   - sigmoid(K, eval): maps eval → expected outcome [0, 1]
//   - computeMSE(dataset, K): single-threaded MSE over a dataset
//   - tuneK(dataset): ternary search for optimal scaling constant
//   - setupEvalOverrides(): disables lazy eval / pawn TT for tuning
//
// Eval Perspective (Critical):
//   evaluate() returns side-to-move perspective. Dataset labels are from
//   White's perspective. The tuner negates the eval when Black is to move:
//     whiteRelativeEval = (nextPlayer == WHITE) ? rawEval : -rawEval
//
// Thread Safety:
//   The TexelTuner is NOT thread-safe. Sprint 6.4 will add parallel MSE.
//   For now, all computation is single-threaded.
//
// Usage:
//   TexelTuner tuner;
//   tuner.setupEvalOverrides();
//   tuner.createEvaluator();
//   double mse = tuner.computeMSE(dataset, 1.0);
//   double K   = tuner.tuneK(dataset);
//
//=============================================================================

#include "tuning/optimizer/TuningDataset.h"

#include <memory>

namespace engine {
  class Evaluator;
}

namespace tuning {

  class TexelTuner {

    /// Single evaluator instance for MSE computation (single-threaded).
    /// Created by createEvaluator(). No PawnTT attached.
    std::unique_ptr<engine::Evaluator> evaluator_;

    /// Scaling constant K (tuned via tuneK(), default 1.0).
    double K_ = 1.0;

  public:
    TexelTuner();
    ~TexelTuner();

    // Non-copyable, non-movable (owns evaluator)
    TexelTuner(const TexelTuner&)            = delete;
    TexelTuner& operator=(const TexelTuner&) = delete;
    TexelTuner(TexelTuner&&)                 = delete;
    TexelTuner& operator=(TexelTuner&&)      = delete;

    /// Applies eval config overrides needed for tuning:
    /// - Disables USE_LAZY_EVAL (must evaluate all terms for every position)
    /// - Disables USE_PAWN_TT (avoids stale cache after param changes)
    /// - Enables USE_SPACE_EVAL, USE_CONNECTED_ROOKS, USE_MINOR_CONNECTIVITY
    ///   (so the tuner can optimize or zero out their weights)
    static void setupEvalOverrides();

    /// Creates the evaluator instance. Must be called after setupEvalOverrides()
    /// so the evaluator picks up the overridden config.
    void createEvaluator();

    // =========================================================================
    // Core math (public for unit testing)
    // =========================================================================

    /// Sigmoid function mapping eval to expected game outcome.
    /// σ(K, e) = 1 / (1 + 10^(-K * e / 400))
    /// @param K     Scaling constant
    /// @param eval  Evaluation in centipawns (from White's perspective)
    /// @return Expected outcome [0.0, 1.0]
    [[nodiscard]] static double sigmoid(double K, double eval);

    /// Computes mean squared error over a dataset (single-threaded).
    /// For each position: reconstruct Position from FEN, evaluate, convert to
    /// White-relative, apply sigmoid, compare to game result.
    /// @param dataset  The dataset to evaluate
    /// @param K        Scaling constant
    /// @return MSE value
    [[nodiscard]] double computeMSE(const TuningDataset& dataset, double K) const;

    /// Tunes the scaling constant K via ternary search on [kLow, kHigh].
    /// Uses the provided dataset to minimize MSE.
    /// @param dataset     The dataset to evaluate
    /// @param kLow        Lower bound for K search (default 0.5)
    /// @param kHigh       Upper bound for K search (default 2.0)
    /// @param iterations  Number of ternary search iterations (default 50)
    /// @return Optimal K value
    [[nodiscard]] double tuneK(const TuningDataset& dataset,
                               double kLow = 0.5, double kHigh = 2.0,
                               int iterations = 50);

    // =========================================================================
    // Accessors
    // =========================================================================

    /// Returns the current scaling constant K.
    [[nodiscard]] double getK() const { return K_; }

    /// Returns whether the evaluator has been created.
    [[nodiscard]] bool hasEvaluator() const { return evaluator_ != nullptr; }
  };

} // namespace tuning

#endif // FRANKYCPP_TEXELTUNER_H
