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

//=============================================================================
// ArenaRunner.cpp - Main Orchestrator Implementation
//=============================================================================

#include "ArenaRunner.h"
#include "UCIEngine.h"

#include <iostream>
#include <set>

namespace arena {

  ArenaRunner::ArenaRunner(const ArenaConfig& config)
      : arenaConfig(config), testSuiteRunner(config), matchRunner(config), resultStore(config.resultsDir) {
  }

  void ArenaRunner::runAll(const bool showConfig) const {
    const auto& testSuites = arenaConfig.testSuites;

    std::cout << "\n===================================================================" << std::endl;
    std::cout << "Engine Arena - Full Run Mode" << std::endl;
    std::cout << "===================================================================" << std::endl;
    std::cout << "Version: " << arenaConfig.version << std::endl;
    std::cout << "Test Suites: " << testSuites.size() << std::endl;
    std::cout << "Matches: " << arenaConfig.matches.size() << std::endl;
    std::cout << "===================================================================" << std::endl;

    // Run test suites if configured
    if (!testSuites.empty()) {
      std::cout << "\n--- Running Test Suites ---" << std::endl;
      runTestSuitesOnly(showConfig);
    }

    // Run matches if configured
    if (!arenaConfig.matches.empty()) {
      std::cout << "\n--- Running Matches ---" << std::endl;
      runMatchesOnly();
    }

    std::cout << "\n===================================================================" << std::endl;
    std::cout << "Full Run Complete" << std::endl;
    std::cout << "===================================================================" << std::endl;
  }

  void ArenaRunner::runTestSuitesOnly(const bool showConfig) const {
    const auto& testSuites = arenaConfig.testSuites;

    std::cout << "\n===================================================================" << std::endl;
    std::cout << "Running Test Suites Only" << std::endl;
    std::cout << "===================================================================" << std::endl;
    std::cout << "Number of suites: " << testSuites.size() << std::endl;
    std::cout << "===================================================================" << std::endl;

    // Show engine configuration if requested
    if (showConfig && !testSuites.empty()) {
      // Group engines to avoid querying the same engine multiple times
      std::set<std::string> queriedEngines;

      for (const auto& suite : testSuites) {
        if (!queriedEngines.contains(suite.enginePath)) {
          queriedEngines.insert(suite.enginePath);

          std::cout << "\n";
          const std::string configStr = UCIEngine::queryEngineConfig(suite.enginePath, suite.commandLineArgs);
          if (!configStr.empty()) {
            std::cout << configStr;
          }
        }
      }
      std::cout << "===================================================================" << std::endl;
    }

    // Pass callback that saves results immediately after each suite completes
    const auto results = testSuiteRunner.runAllTestSuites(
      [this](const TestSuiteResult& result) {
        const std::string jsonPath = resultStore.writeTestSuiteResult(result);
        std::cout << "  -> Saved: " << jsonPath << std::endl;
      });

    // Results already saved via callback, just print summary
    std::cout << "\n===================================================================" << std::endl;
    std::cout << "Test Suites Complete - " << results.size() << " results saved" << std::endl;
    std::cout << "===================================================================" << std::endl;
  }

  void ArenaRunner::runMatchesOnly() const {
    std::cout << "\n===================================================================" << std::endl;
    std::cout << "Running Matches Only" << std::endl;
    std::cout << "===================================================================" << std::endl;
    std::cout << "Number of matches: " << arenaConfig.matches.size() << std::endl;
    std::cout << "===================================================================" << std::endl;

    const auto results = matchRunner.runAllMatches();

    // Save results
    std::cout << "\nSaving match results..." << std::endl;
    for (const auto& result : results) {
      const std::string jsonPath = resultStore.writeMatchResult(result);
      std::cout << "  Saved: " << jsonPath << std::endl;
    }

    std::cout << "\n===================================================================" << std::endl;
    std::cout << "Matches Complete" << std::endl;
    std::cout << "===================================================================" << std::endl;
  }

  ReportData ArenaRunner::loadAllResults() const {
    return resultStore.loadAllResults();
  }

  std::set<EngineId> ArenaRunner::listAvailableEngines() const {
    const ReportData data = loadAllResults();
    return data.engines;
  }

} // namespace arena
