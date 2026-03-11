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

#ifndef FRANKYCPP_ENGINE_ARENA_REPORTGENERATOR_H
#define FRANKYCPP_ENGINE_ARENA_REPORTGENERATOR_H

//=============================================================================
// ReportGenerator.h - Arena Report Formatting
//=============================================================================
//
// ReportGenerator contains all report formatting logic for Engine Arena.
// All methods are static pure functions that take ReportData as input and
// return formatted report strings.
//
// Responsibilities:
//   - Format test suite baseline reports (all engines side by side)
//   - Format test suite comparison reports (target vs baselines)
//   - Format match baseline and comparison reports
//   - Format engine summary reports (latest or historical)
//
// Design:
//   All methods are static and take `const ReportData&` — no state is held.
//   Callers load data via ResultStore, then pass it to the appropriate method.
//
// Usage:
//   auto data = resultStore.loadAllResults();
//   std::cout << ReportGenerator::generateBaselineReport(data);
//   std::cout << ReportGenerator::generateComparisonReport(data, target, baselines);
//   std::cout << ReportGenerator::generateEngineSummary(data, engine, showHistory);
//
//=============================================================================

#include "ArenaResults.h"

#include <string>
#include <vector>

namespace arena {

  /// Static report formatting — all methods are pure functions of ReportData
  class ReportGenerator {
  public:
    /// Generates baseline report showing all engines side by side
    /// @param data ReportData with loaded results
    /// @return Formatted report string
    static std::string generateBaselineReport(const ReportData& data);

    /// Generates comparison report for target engine against baselines
    /// @param data ReportData with loaded results
    /// @param targetEngine Engine to compare (e.g., "FrankyCPP-v1.2-dev")
    /// @param baselineEngines Baselines to compare against (uses all if empty)
    /// @return Formatted comparison report string
    static std::string generateComparisonReport(
      const ReportData& data,
      const EngineId& targetEngine,
      const std::vector<EngineId>& baselineEngines = {});

    /// Generates match baseline report showing all engine pairs
    /// @param data ReportData with loaded match results
    /// @return Formatted match report string
    static std::string generateMatchBaselineReport(const ReportData& data);

    /// Generates match comparison report for target engine vs baselines
    /// @param data ReportData with loaded match results
    /// @param targetEngine Engine to compare
    /// @param baselineEngines Baselines to compare against
    /// @return Formatted match comparison report string
    static std::string generateMatchComparisonReport(
      const ReportData& data,
      const EngineId& targetEngine,
      const std::vector<EngineId>& baselineEngines = {});

    /// Generates engine summary showing all results for a specific engine
    /// @param data ReportData with loaded results (test suites + matches + benchmarks)
    /// @param engine Engine to show summary for (e.g., "FrankyCPP-v1.6")
    /// @param showHistory If true, show historical runs grouped by tag
    /// @return Formatted summary string
    static std::string generateEngineSummary(
      const ReportData& data,
      const EngineId& engine,
      bool showHistory = false);
  };

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_REPORTGENERATOR_H
