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
// coordinates test suite execution, match running, and result persistence.
//
// Responsibilities:
//   - Orchestrate test suite execution via TestSuiteRunner
//   - Orchestrate match execution via MatchRunner
//   - Persist results via ResultStore
//   - Load results for reporting (delegates formatting to ReportGenerator)
//
// Usage Modes:
//   1. Run all tests:      runner.runAll()
//   2. Test suites only:   runner.runTestSuitesOnly()
//   3. Matches only:       runner.runMatchesOnly()
//   4. Load results:       runner.loadAllResults()
//   5. List engines:       runner.listAvailableEngines()
//
// Report formatting is handled by ReportGenerator (see ReportGenerator.h).
//
//=============================================================================

#include "ArenaConfig.h"
#include "ArenaResults.h"
#include "MatchRunner.h"
#include "ResultStore.h"
#include "TestSuiteRunner.h"

#include <set>

namespace arena {

  /// Main orchestrator for Engine Arena testing framework
  class ArenaRunner {
    const ArenaConfig& arenaConfig;  ///< Reference to arena configuration
    TestSuiteRunner testSuiteRunner; ///< Test suite execution engine
    MatchRunner matchRunner;         ///< Match execution engine
    ResultStore resultStore;         ///< Centralized result I/O

  public:
    /// Creates an ArenaRunner with the given configuration
    /// @param config Arena configuration
    explicit ArenaRunner(const ArenaConfig& config);

    /// Runs all configured tests (test suites and matches)
    /// @param showConfig If true, query and display engine configuration before running
    void runAll(bool showConfig = false) const;

    /// Runs only test suites (no matches)
    /// @param showConfig If true, query and display engine configuration before running
    void runTestSuitesOnly(bool showConfig = false) const;

    /// Runs only matches (no test suites)
    void runMatchesOnly() const;

    /// Loads ALL results (test suites + matches + benchmarks) into ReportData
    /// Delegates to ResultStore::loadAllResults()
    /// @return ReportData with all results organized for reporting
    [[nodiscard]] ReportData loadAllResults() const;

    /// Lists all available engines from stored results
    /// @return Set of EngineIds found in results
    [[nodiscard]] std::set<EngineId> listAvailableEngines() const;
  };

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_ARENARUNNER_H
