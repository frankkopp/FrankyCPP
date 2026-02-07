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

#ifndef FRANKYCPP_ENGINE_ARENA_ARENARUNNER_H
#define FRANKYCPP_ENGINE_ARENA_ARENARUNNER_H

//=============================================================================
// ArenaRunner.h - Main Orchestrator for Engine Arena
//=============================================================================
//
// ArenaRunner is the main entry point for the Engine Arena framework. It
// coordinates test suite execution, match running, and version comparison.
//
// Responsibilities:
//   - Load and validate arena configuration
//   - Orchestrate test suite execution via TestSuiteRunner
//   - Orchestrate match execution via MatchRunner
//   - Compare results between different engine versions
//   - Generate summary reports and comparisons
//
// Usage Modes:
//   1. Run all tests: runner.runAll()
//   2. Test suites only: runner.runTestSuitesOnly()
//   3. Matches only: runner.runMatchesOnly()
//   4. Compare versions: runner.compareVersions("v1.1", "v1.0")
//
// Comparison Logic:
//   - Loads JSON result files from results/ directory
//   - Finds matching test suites by name
//   - Finds matching matches by name
//   - Calculates deltas (position improvements, ELO changes)
//   - Generates text report with summary statistics
//   - Saves comparison report to results/comparisons/
//
// Example Comparison Output:
//   =================================================================
//   Version Comparison: v1.1 vs v1.0
//   =================================================================
//   Test Suites:
//     WAC:           250/300 (83.3%) → 285/300 (95.0%)  [+35 positions, +11.7%]
//     franky_tests:  50/50 (100%) → 50/50 (100%)        [no change]
//
//   Matches:
//     v1.1_vs_v1.0:  Score 65-35 (ELO: +174)
//
//   Overall: v1.1 is approximately +170 ELO stronger than v1.0
//   =================================================================
//
//=============================================================================

#include "ArenaConfig.h"
#include "ArenaResults.h"
#include "TestSuiteRunner.h"
#include "MatchRunner.h"
#include "ResultWriter.h"

#include <string>

namespace arena {

/// Main orchestrator for Engine Arena testing framework
class ArenaRunner {
public:
  /// Creates an ArenaRunner with the given configuration
  /// @param config Arena configuration
  explicit ArenaRunner(const ArenaConfig& config);

  /// Runs all configured test suites and matches
  void runAll() const;

  /// Runs only test suites (no matches)
  void runTestSuitesOnly() const;

  /// Runs only matches (no test suites)
  void runMatchesOnly() const;

  //=========================================================================
  // NEW Reporting Methods (Phase 1-3)
  //=========================================================================

  /// Loads ALL test suite results into ReportData structure
  /// Groups by test suite, then by engine, keeping latest result per combo
  /// @return ReportData with all results organized for reporting
  ReportData loadAllResults() const;

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

  /// Lists all available engines from stored results
  /// @return Set of EngineIds found in results
  std::set<EngineId> listAvailableEngines() const;

  /// Loads match results into ReportData structure
  /// @param data ReportData to populate (modifies in place)
  void loadMatchResults(ReportData& data) const;

  /// Generates match baseline report showing all engine pairs
  /// @param data ReportData with loaded match results
  /// @return Formatted match report string
  static std::string generateMatchBaselineReport(const ReportData& data);

  /// Generates match comparison report for target engine vs baselines
  /// @param data ReportData with loaded match results
  /// @param targetEngine Engine to compare
  /// @param baselineEngines Baselines to compare against
  /// @return Formatted match comparison report strstatic ing
  static std::string generateMatchComparisonReport(
      const ReportData& data,
      const EngineId& targetEngine,
      const std::vector<EngineId>& baselineEngines = {});

private:
  const ArenaConfig& arenaConfig;  ///< Reference to arena configuration
  TestSuiteRunner testSuiteRunner; ///< Test suite execution engine
  MatchRunner matchRunner;         ///< Match execution engine
  ResultWriter resultWriter;       ///< Result persistence handler

  //=========================================================================
  // Formatting Utilities
  //=========================================================================

  /// Formats a number with thousands separator (e.g., 1234567 -> "1,234,567")
  static std::string formatNumber(int64_t value);

  /// Formats node count with suffix (e.g., 12400000 -> "12.4M")
  static std::string formatNodes(double nodes);

  /// Formats time in seconds (e.g., 1234 ms -> "1.2s")
  static std::string formatTime(double timeMs);

  /// Formats a delta value with sign and color hint (e.g., +5, -3)
  static std::string formatDelta(int delta);

  /// Formats a delta percentage with sign (e.g., +2.5%, -1.0%)
  static std::string formatDeltaPercent(double delta);

  /// Generates current timestamp in format YYYYMMDD_HHMMSS
  static std::string getCurrentTimestamp();

  /// Generates ISO 8601 timestamp
  static std::string getIsoTimestamp();
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_ARENARUNNER_H
