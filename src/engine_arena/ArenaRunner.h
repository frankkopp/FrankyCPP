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

#include <map>
#include <string>

namespace arena {

/// Main orchestrator for Engine Arena testing framework
class ArenaRunner {
public:
  /// Creates an ArenaRunner with the given configuration
  /// @param config Arena configuration
  explicit ArenaRunner(const ArenaConfig& config);

  /// Runs all configured test suites and matches
  void runAll();

  /// Runs only test suites (no matches)
  void runTestSuitesOnly();

  /// Runs only matches (no test suites)
  void runMatchesOnly();

  /// Compares results between two versions and generates report
  /// @param version1 First version to compare (e.g., "v1.1")
  /// @param version2 Second version to compare (e.g., "v1.0")
  /// @throws std::runtime_error if result files not found
  void compareVersions(const std::string& version1, const std::string& version2);

private:
  const ArenaConfig& arenaConfig;  ///< Reference to arena configuration
  TestSuiteRunner testSuiteRunner; ///< Test suite execution engine
  MatchRunner matchRunner;         ///< Match execution engine
  ResultWriter resultWriter;       ///< Result persistence handler

  /// Loads all test suite results for a specific version
  /// @param version Version to load results for (e.g., "v1.1")
  /// @return Map of suite name -> TestSuiteResult
  std::map<std::string, TestSuiteResult> loadTestSuiteResults(const std::string& version);

  /// Loads all match results for a specific version
  /// @param version Version to load results for (e.g., "v1.1")
  /// @return Map of match name -> MatchResult
  std::map<std::string, MatchResult> loadMatchResults(const std::string& version);

  /// Generates and prints comparison report
  /// @param version1 First version name
  /// @param version2 Second version name
  /// @param suites1 Test suite results for version 1
  /// @param suites2 Test suite results for version 2
  /// @param matches1 Match results for version 1
  /// @param matches2 Match results for version 2
  /// @return Formatted comparison report as string
  std::string generateComparisonReport(
      const std::string& version1,
      const std::string& version2,
      const std::map<std::string, TestSuiteResult>& suites1,
      const std::map<std::string, TestSuiteResult>& suites2,
      const std::map<std::string, MatchResult>& matches1,
      const std::map<std::string, MatchResult>& matches2);

  /// Saves comparison report to file
  /// @param version1 First version name
  /// @param version2 Second version name
  /// @param report Report content
  /// @return Path to saved report file
  std::string saveComparisonReport(
      const std::string& version1,
      const std::string& version2,
      const std::string& report);

  /// Generates current timestamp in format YYYYMMDD_HHMMSS
  /// @return Timestamp string for file naming
  static std::string getCurrentTimestamp();
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_ARENARUNNER_H
