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
// Core Capabilities:
//   - sigmoid(K, eval): maps eval → expected outcome [0, 1]
//   - computeMSE(dataset, K): single-threaded MSE over a dataset
//   - computeMSEParallel(dataset, K): multi-threaded MSE (one Evaluator per thread)
//   - computeActivationFlags(dataset): board-state analysis → param group bitset
//   - computeAndCacheErrors(dataset, K): full eval pass populating per-entry cache
//   - computeMSEIncremental(dataset, K, group): re-eval only affected entries
//   - tuneK(dataset): ternary search for optimal scaling constant
//   - tuneParameters(): coordinate descent with incremental MSE optimization
//   - setupEvalOverrides(): disables lazy eval / pawn TT for tuning
//
// Eval Perspective (Critical):
//   evaluate() returns side-to-move perspective. Dataset labels are from
//   White's perspective. The tuner negates the eval when Black is to move:
//     whiteRelativeEval = (nextPlayer == WHITE) ? rawEval : -rawEval
//
// Thread Safety:
//   - computeMSEParallel() dispatches chunks to a ThreadPool. Each worker
//     thread uses its own Evaluator instance and stack-local Position.
//     The dataset and EvalConfigData are read-only during MSE computation.
//   - Parameter mutation (applyToConfig) happens on the main thread only,
//     strictly between MSE computations — no data races.
//   - Partial sums are sorted before aggregation for deterministic FP results.
//
// Usage:
//   TexelTuner tuner;
//   tuner.setupEvalOverrides();
//   tuner.createEvaluators(4);             // 4 worker threads
//   double K = tuner.tuneK(dataset);       // uses parallel MSE automatically
//   tuner.tuneParameters(train, &test, params);  // coordinate descent
//
//=============================================================================

#include "tuning/optimizer/TuningDataset.h"
#include "tuning/optimizer/TuningParameter.h"

#include <memory>
#include <vector>

namespace common {
  class ThreadPool;
}

namespace engine {
  class Evaluator;
}

namespace tuning {

  class TexelTuner {

    /// Evaluator instances — one per worker thread for parallel MSE.
    /// threadEvaluators_[0] is also used for single-threaded computeMSE().
    /// Created by createEvaluators(). No PawnTT attached.
    std::vector<std::unique_ptr<engine::Evaluator>> threadEvaluators_;

    /// Thread pool for parallel MSE computation (reused across all calls).
    /// Created by createEvaluators(). nullptr if single-threaded.
    std::unique_ptr<common::ThreadPool> threadPool_;

    /// Number of worker threads for parallel MSE.
    int numThreads_ = 0;

    /// Scaling constant K (tuned via tuneK(), default 1.0).
    double K_ = 1.0;

    /// Running total of squared errors across all entries (for incremental MSE).
    /// Set by computeAndCacheErrors(), updated by updateCacheForGroup().
    /// Mutable because incremental MSE logically "reads" the dataset but updates bookkeeping.
    mutable double totalSquaredError_ = 0.0;

  public:
    TexelTuner();
    ~TexelTuner();

    // Non-copyable, non-movable (owns evaluators and thread pool)
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

    /// Creates a single evaluator instance (single-threaded mode).
    /// Must be called after setupEvalOverrides().
    void createEvaluator();

    /// Creates N evaluator instances and a thread pool for parallel MSE.
    /// Must be called after setupEvalOverrides(). Replaces any existing evaluators.
    /// @param numThreads  Number of worker threads (clamped to [1, hardware_concurrency])
    void createEvaluators(int numThreads);

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

    /// Computes mean squared error over a dataset using multiple threads.
    /// Partitions the dataset into chunks, one per worker thread. Each thread
    /// uses its own Evaluator and stack-local Position.
    /// Partial sums are sorted before aggregation for deterministic FP results.
    /// Falls back to single-threaded computeMSE() if numThreads_ <= 1.
    /// @param dataset  The dataset to evaluate (read-only during computation)
    /// @param K        Scaling constant
    /// @return MSE value (matches single-threaded result within FP tolerance)
    [[nodiscard]] double computeMSEParallel(const TuningDataset& dataset, double K) const;

    // =========================================================================
    // Incremental MSE optimization (activation flags)
    // =========================================================================

    /// Analyzes board state for each entry to determine which parameter groups
    /// affect its evaluation. Sets the activeParamGroups bitset per entry.
    /// Must be called once before using computeMSEIncremental().
    /// Uses thread pool if available for parallel processing.
    /// @param dataset  Dataset to analyze (entries modified in place)
    void computeActivationFlags(TuningDataset& dataset) const;

    /// Full evaluation pass: evaluates all entries, caches per-entry squared errors,
    /// and stores totalSquaredError_ for use by computeMSEIncremental().
    /// Must be called before the first computeMSEIncremental() call.
    /// Uses thread pool if available.
    /// @param dataset  Dataset to evaluate (cachedSquaredError fields updated)
    /// @param K        Scaling constant
    /// @return MSE value
    double computeAndCacheErrors(TuningDataset& dataset, double K) const;

    /// Incremental MSE: only re-evaluates entries where paramGroup is active.
    /// For inactive entries, uses cachedSquaredError from the last full/update pass.
    /// Does NOT update the cache — call updateCacheForGroup() to commit changes.
    /// @param dataset     Dataset with cached errors from computeAndCacheErrors()
    /// @param K           Scaling constant
    /// @param paramGroup  Group index to re-evaluate
    /// @return New MSE value reflecting the parameter change
    [[nodiscard]] double computeMSEIncremental(const TuningDataset& dataset, double K, int paramGroup) const;

    /// Updates cached squared errors for entries where paramGroup is active.
    /// Re-evaluates those entries with current config and adjusts totalSquaredError_.
    /// Call after deciding to keep a parameter change.
    /// @param dataset     Dataset to update (cachedSquaredError fields modified)
    /// @param K           Scaling constant
    /// @param paramGroup  Group index whose active entries to refresh
    void updateCacheForGroup(TuningDataset& dataset, double K, int paramGroup) const;

    /// Tunes the scaling constant K via ternary search on [kLow, kHigh].
    /// Uses parallel MSE if evaluators were created with createEvaluators().
    /// @param dataset     The dataset to evaluate
    /// @param kLow        Lower bound for K search (default 0.5)
    /// @param kHigh       Upper bound for K search (default 2.0)
    /// @param iterations  Number of ternary search iterations (default 50)
    /// @return Optimal K value
    [[nodiscard]] double tuneK(const TuningDataset& dataset,
                               double kLow = 0.5, double kHigh = 2.0,
                               int iterations = 50);

    // =========================================================================
    // Coordinate descent optimizer
    // =========================================================================

    /// Optimizes parameters via coordinate descent to minimize MSE.
    ///
    /// For each parameter, tries ±delta and keeps the direction that reduces
    /// MSE. Repeats over all parameters until no improvement or maxPasses reached.
    /// Logs per-pass summary (train MSE, test MSE, params changed, biggest mover).
    /// Array parameters with monotonicity constraints are clamped after each change.
    /// If checkpointPath is non-empty, saves a checkpoint YAML after each pass.
    ///
    /// @param trainSet        Training dataset (MSE minimized on this)
    /// @param testSet         Optional test dataset for overfitting detection (nullptr to skip)
    /// @param params          Mutable parameter vector (values modified in place)
    /// @param maxPasses       Maximum number of full passes over all parameters (default 100)
    /// @param checkpointPath  Path for checkpoint file (empty = no checkpointing)
    /// @param datasetPath     Dataset path stored in checkpoint metadata (empty = omitted)
    /// @param startPass       Starting pass number for resume (default 0 = fresh start)
    void tuneParameters(const TuningDataset& trainSet,
                        const TuningDataset* testSet,
                        std::vector<TuningParameter>& params,
                        int maxPasses = 100,
                        const std::string& checkpointPath = "",
                        const std::string& datasetPath = "",
                        int startPass = 0);

    /// Enforces monotonicity constraints on an array parameter after modification.
    ///
    /// For NON_DECREASING arrays: clamps value so array[i] >= array[i-1].
    /// For NON_INCREASING arrays: clamps value so array[i] <= array[i-1].
    /// Only modifies the single element at param's arrayIndex; neighboring
    /// elements are used as bounds but not themselves modified.
    ///
    /// If the param is not an array element or has no monotonicity constraint,
    /// this is a no-op.
    ///
    /// @param param   The array element parameter that was just modified
    /// @param params  The full parameter vector (to look up neighbors in the same array)
    static void enforceMonotonicity(TuningParameter& param,
                                    const std::vector<TuningParameter>& params);

    // =========================================================================
    // Accessors
    // =========================================================================

    /// Returns the current scaling constant K.
    [[nodiscard]] double getK() const { return K_; }

    /// Sets the scaling constant K directly (e.g., when resuming from checkpoint).
    void setK(const double k) { K_ = k; }

    /// Returns whether at least one evaluator has been created.
    [[nodiscard]] bool hasEvaluator() const { return !threadEvaluators_.empty(); }

    /// Returns the number of worker threads configured for parallel MSE.
    [[nodiscard]] int numThreads() const { return numThreads_; }
  };

} // namespace tuning

#endif // FRANKYCPP_TEXELTUNER_H
