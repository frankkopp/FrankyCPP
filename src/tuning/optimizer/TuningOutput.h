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

#ifndef FRANKYCPP_TUNINGOUTPUT_H
#define FRANKYCPP_TUNINGOUTPUT_H

//=============================================================================
// TuningOutput.h - Output Generation for Texel Tuning Results
//=============================================================================
//
// Generates output files after tuning:
//   1. Tuned parameters YAML — flat-key format matching config/eval.yaml,
//      loadable by ConfigManager. Only includes tuned eval parameters.
//   2. Comparison report — side-by-side table of original vs tuned values
//      with delta, change %, and flags for sign-flipped / zeroed-out params.
//
// The YAML output uses the same flat-key format as config/eval.yaml:
//   SCALAR_PARAM: 42
//   ARRAY_PARAM: 0,5,15,35,70,120
//
// Array elements are coalesced back into comma-separated values from the
// flat TuningParameter vector (which has one entry per array element).
//
// Usage:
//   TuningOutput::writeParamsYaml("tuned_params.yaml", params);
//   TuningOutput::writeComparisonReport("comparison.txt", params);
//   TuningOutput::printComparisonSummary(params);
//
//=============================================================================

#include "tuning/optimizer/TuningParameter.h"

#include <string>
#include <vector>

namespace tuning {

  class TuningOutput {
  public:
    /// Writes tuned parameters to a YAML file in ConfigManager-loadable format.
    /// Only includes parameters from the Eval domain. Array elements are coalesced
    /// into comma-separated values (e.g., "KING_SAFETY_TABLE: 0,5,15,...").
    ///
    /// @param path    Output file path
    /// @param params  The tuned parameter vector
    /// @param K       Scaling constant used (written as a comment)
    static void writeParamsYaml(const std::string& path,
                                const std::vector<TuningParameter>& params,
                                double K = 0.0);

    /// Writes a side-by-side comparison report of original vs tuned values.
    /// Includes: parameter name, original value, tuned value, delta, change %.
    /// Flags sign-flipped params (⚠ SIGN FLIP) and zeroed-out params (→ ZERO).
    ///
    /// @param path    Output file path
    /// @param params  The tuned parameter vector
    static void writeComparisonReport(const std::string& path,
                                      const std::vector<TuningParameter>& params);

    /// Prints a comparison summary to stdout.
    /// Shows total changed, unchanged, sign-flipped, zeroed-out counts,
    /// and the top N movers by absolute delta.
    ///
    /// @param params   The tuned parameter vector
    /// @param topN     Number of top movers to display (default 20)
    static void printComparisonSummary(const std::vector<TuningParameter>& params,
                                       int topN = 20);

    /// Generates a comparison report as a string (for testing / logging).
    /// Same content as writeComparisonReport but returned as a string.
    ///
    /// @param params  The tuned parameter vector
    /// @return The full comparison report text
    [[nodiscard]] static std::string generateComparisonReport(
      const std::vector<TuningParameter>& params);

    /// Generates tuned YAML content as a string (for testing).
    /// Same content as writeParamsYaml but returned as a string.
    ///
    /// @param params  The tuned parameter vector
    /// @param K       Scaling constant used
    /// @return The YAML file content
    [[nodiscard]] static std::string generateParamsYaml(
      const std::vector<TuningParameter>& params,
      double K = 0.0);
  };

} // namespace tuning

#endif // FRANKYCPP_TUNINGOUTPUT_H
