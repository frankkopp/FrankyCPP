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

#ifndef FRANKYCPP_TUNINGSTATE_H
#define FRANKYCPP_TUNINGSTATE_H

//=============================================================================
// TuningState.h - Checkpoint Persistence for Texel Tuning
//=============================================================================
//
// Captures the complete state of a tuning run after each coordinate descent
// pass, enabling:
//   - Resuming interrupted runs (overnight failures, etc.)
//   - Comparing results across runs with different datasets/settings
//   - Loading previous results as starting points for new runs
//
// The state is serialized to human-readable YAML using yaml-cpp.
// All fields needed to reconstruct the tuner state are included:
//   - Completed pass count, K value, MSE scores
//   - All parameter name→value pairs (flat, including expanded arrays)
//   - Dataset path and timestamp for provenance
//
// Usage:
//   // Save after each pass:
//   TuningState state;
//   state.captureFromParams(params);
//   state.completedPasses = passNumber;
//   state.saveToYaml("checkpoint_pass5.yaml");
//
//   // Resume:
//   auto state = TuningState::loadFromYaml("checkpoint_pass5.yaml");
//   state.restoreToParams(params);
//   // continue from state.completedPasses + 1
//
// Thread Safety:
//   TuningState is a simple data struct — not thread-safe by design.
//   Saving/loading is done on the main thread between passes.
//
//=============================================================================

#include "tuning/optimizer/TuningParameter.h"

#include <string>
#include <vector>

namespace tuning {

  /// Checkpoint state for a Texel tuning run.
  struct TuningState {
    int completedPasses    = 0;       ///< Number of completed coordinate descent passes
    double bestTrainMSE    = 0.0;     ///< Best (lowest) training MSE achieved
    double bestTestMSE     = 0.0;     ///< Best (lowest) test MSE achieved (0 if no test set)
    double K               = 1.0;     ///< Scaling constant used for sigmoid
    std::string datasetPath;          ///< Path to the training dataset file
    std::string timestamp;            ///< ISO-8601 timestamp when checkpoint was saved

    /// Parameter values: each entry is (name, value).
    /// For array elements, name includes the index (e.g., "KING_SAFETY_TABLE[3]").
    /// Order matches the TuningParameter vector.
    std::vector<std::pair<std::string, int>> paramValues;

    /// Captures current parameter values from the tuning parameter vector.
    /// @param params  The parameter vector to snapshot
    void captureFromParams(const std::vector<TuningParameter>& params);

    /// Restores parameter values from this state into the parameter vector.
    /// Matches by parameter name. Logs warnings for mismatched/missing params.
    /// @param params  The parameter vector to restore into
    /// @return Number of parameters successfully restored
    int restoreToParams(std::vector<TuningParameter>& params) const;

    /// Serializes this state to a YAML file.
    /// The file is human-readable and suitable for inspection/editing.
    /// @param path  Output file path
    void saveToYaml(const std::string& path) const;

    /// Deserializes a TuningState from a YAML checkpoint file.
    /// @param path  Input file path
    /// @return The loaded TuningState
    /// @throws std::runtime_error if the file cannot be read or parsed
    [[nodiscard]] static TuningState loadFromYaml(const std::string& path);

    /// Returns the current time as an ISO-8601 string (YYYY-MM-DD HH:MM:SS).
    [[nodiscard]] static std::string currentTimestamp();
  };

} // namespace tuning

#endif // FRANKYCPP_TUNINGSTATE_H
