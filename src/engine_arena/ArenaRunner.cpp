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
#include "ConsoleColors.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

// Suppress false positive Clangd warning about nlohmann/json template instantiation
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wc++20-extensions"
#endif

#include <nlohmann/json.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace arena {

using json = nlohmann::json;
using std::chrono::system_clock;

ArenaRunner::ArenaRunner(const ArenaConfig& config)
    : arenaConfig(config)
    , testSuiteRunner(config)
    , matchRunner(config)
    , resultWriter(config.resultsDir) {
}

void ArenaRunner::runAll() {
  std::cout << "\n===================================================================" << std::endl;
  std::cout << "Engine Arena - Full Run Mode" << std::endl;
  std::cout << "===================================================================" << std::endl;
  std::cout << "Version: " << arenaConfig.version << std::endl;
  std::cout << "Test Suites: " << arenaConfig.testSuites.size() << std::endl;
  std::cout << "Matches: " << arenaConfig.matches.size() << std::endl;
  std::cout << "===================================================================" << std::endl;

  // Run test suites if configured
  if (!arenaConfig.testSuites.empty()) {
    std::cout << "\n--- Running Test Suites ---" << std::endl;
    runTestSuitesOnly();
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

void ArenaRunner::runTestSuitesOnly() {
  std::cout << "\n===================================================================" << std::endl;
  std::cout << "Running Test Suites Only" << std::endl;
  std::cout << "===================================================================" << std::endl;
  std::cout << "Number of suites: " << arenaConfig.testSuites.size() << std::endl;
  std::cout << "===================================================================" << std::endl;

  // Pass callback that saves results immediately after each suite completes
  const auto results = testSuiteRunner.runAllTestSuites(
      [this](const TestSuiteResult& result) {
        std::string jsonPath = resultWriter.writeTestSuiteResult(result);
        std::cout << "  -> Saved: " << jsonPath << std::endl;
      });

  // Results already saved via callback, just print summary
  std::cout << "\n===================================================================" << std::endl;
  std::cout << "Test Suites Complete - " << results.size() << " results saved" << std::endl;
  std::cout << "===================================================================" << std::endl;
}

void ArenaRunner::runMatchesOnly() {
  std::cout << "\n===================================================================" << std::endl;
  std::cout << "Running Matches Only" << std::endl;
  std::cout << "===================================================================" << std::endl;
  std::cout << "Number of matches: " << arenaConfig.matches.size() << std::endl;
  std::cout << "===================================================================" << std::endl;

  auto results = matchRunner.runAllMatches();

  // Save results
  std::cout << "\nSaving match results..." << std::endl;
  for (const auto& result : results) {
    std::string jsonPath = resultWriter.writeMatchResult(result);
    std::cout << "  Saved: " << jsonPath << std::endl;
  }

  std::cout << "\n===================================================================" << std::endl;
  std::cout << "Matches Complete" << std::endl;
  std::cout << "===================================================================" << std::endl;
}

//=============================================================================
// Reporting Implementation (Phase 1-3)
//=============================================================================

std::string ArenaRunner::getCurrentTimestamp() {
  const auto now = system_clock::now();
  auto time_t = system_clock::to_time_t(now);
  std::tm tm{};

#ifdef _WIN32
  localtime_s(&tm, &time_t);
#else
  localtime_r(&time_t, &tm);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
  return oss.str();
}

std::string ArenaRunner::getIsoTimestamp() {
  const auto now = system_clock::now();
  auto time_t = system_clock::to_time_t(now);
  std::tm tm{};

#ifdef _WIN32
  localtime_s(&tm, &time_t);
#else
  localtime_r(&time_t, &tm);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

std::string ArenaRunner::formatNumber(int64_t value) {
  std::string str = std::to_string(std::abs(value));
  std::string result;
  int count = 0;
  for (auto it = str.rbegin(); it != str.rend(); ++it) {
    if (count > 0 && count % 3 == 0) {
      result = ',' + result;
    }
    result = *it + result;
    ++count;
  }
  return value < 0 ? "-" + result : result;
}

std::string ArenaRunner::formatNodes(double nodes) {
  if (nodes >= 1e9) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (nodes / 1e9) << "B";
    return oss.str();
  }
  if (nodes >= 1e6) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (nodes / 1e6) << "M";
    return oss.str();
  }
  if (nodes >= 1e3) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (nodes / 1e3) << "K";
    return oss.str();
  }
  return std::to_string(static_cast<int64_t>(nodes));
}

std::string ArenaRunner::formatTime(double timeMs) {
  if (timeMs >= 60000) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (timeMs / 60000) << "m";
    return oss.str();
  }
  if (timeMs >= 1000) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (timeMs / 1000) << "s";
    return oss.str();
  }
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(0) << timeMs << "ms";
  return oss.str();
}

std::string ArenaRunner::formatDelta(int delta) {
  if (delta > 0) return "+" + std::to_string(delta);
  if (delta < 0) return std::to_string(delta);
  return "0";
}

std::string ArenaRunner::formatDeltaPercent(double delta) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1);
  if (delta > 0) oss << "+";
  oss << delta << "%";
  return oss.str();
}

ReportData ArenaRunner::loadAllResults() {
  ReportData data;

  const std::filesystem::path testsuitesDir = std::filesystem::path(arenaConfig.resultsDir) / "testsuites";

  if (!std::filesystem::exists(testsuitesDir)) {
    std::cout << "  Warning: No testsuites directory found at " << testsuitesDir << std::endl;
    return data;
  }

  // Load all JSON files from testsuites directory
  for (const auto& entry : std::filesystem::directory_iterator(testsuitesDir)) {
    if (entry.path().extension() != ".json") continue;

    try {
      std::ifstream file(entry.path());
      json jsonData = json::parse(file);

      TestSuiteResult result;

      // Parse new JSON format
      result.arenaVersion = jsonData["arenaVersion"];
      result.timestamp = jsonData["timestamp"];
      result.testSuiteName = jsonData["testSuite"]["name"];
      result.epdPath = jsonData["testSuite"]["epdPath"];
      result.engineName = jsonData["engine"]["name"];
      result.engineVersion = jsonData["engine"]["version"];
      result.enginePath = jsonData["engine"]["path"];

      // Parse summary section
      const auto& summary = jsonData["summary"];
      result.totalTests = summary["totalTests"];
      result.passed = summary["passed"];
      result.failed = summary["failed"];
      result.skipped = summary["skipped"];
      result.totalNodes = summary["totalNodes"];
      result.totalTimeMs = summary["totalTimeMs"];

      // Skip details for reporting (not needed, saves memory)
      // result.details can be populated if needed

      // Get engine identifier
      EngineId engineId = result.getEngineId();
      const std::string& suiteName = result.testSuiteName;

      // Add to data structures
      data.testSuites.insert(suiteName);
      data.engines.insert(engineId);

      // Check if we already have a result for this suite/engine
      auto& suiteMap = data.suiteResults[suiteName];
      auto existing = suiteMap.find(engineId);
      if (existing == suiteMap.end() || result.timestamp > existing->second.timestamp) {
        // Keep the latest result
        suiteMap[engineId] = result;
      }

    } catch (const std::exception& e) {
      std::cerr << "  Warning: Failed to load " << entry.path().filename().string()
                << ": " << e.what() << std::endl;
    }
  }

  return data;
}

std::set<EngineId> ArenaRunner::listAvailableEngines() {
  ReportData data = loadAllResults();
  return data.engines;
}

std::string ArenaRunner::generateBaselineReport(const ReportData& data) {
  std::ostringstream report;

  // Helper to pad/truncate string to exact width
  auto fixedWidth = [](const std::string& str, size_t width) -> std::string {
    if (str.length() >= width) {
      return str.substr(0, width - 2) + "..";
    }
    return str + std::string(width - str.length(), ' ');
  };

  // Column widths
  constexpr size_t COL_ENGINE = 28;
  constexpr size_t COL_SOLVED = 12;
  constexpr size_t COL_RATE = 10;
  constexpr size_t COL_TIME = 12;
  constexpr size_t COL_NODES = 12;

  // Header
  report << Color::color(Color::BOLD);
  report << "================================================================================\n";
  report << "BASELINE RESULTS - All Engines\n";
  report << "================================================================================\n";
  report << Color::color(Color::RESET);
  report << "Generated: " << getIsoTimestamp() << "\n";
  report << "================================================================================\n";

  if (!data.hasResults()) {
    report << "\nNo results found.\n";
    report << "Run test suites first: FrankyCPP_Arena --testsuites\n";
    return report.str();
  }

  // List of engines sorted for consistent display
  std::vector<EngineId> engines(data.engines.begin(), data.engines.end());

  // For each test suite, show all engines side by side
  for (const auto& suiteName : data.testSuites) {
    const auto& suiteResultsMap = data.suiteResults.at(suiteName);

    // Get total tests from first result (should be same for all)
    int totalTests = 0;
    if (!suiteResultsMap.empty()) {
      totalTests = suiteResultsMap.begin()->second.totalTests;
    }

    report << "\n";
    report << Color::color(Color::BOLD);
    report << "TEST SUITE: " << suiteName << " (" << totalTests << " positions)\n";
    report << Color::color(Color::RESET);
    report << "--------------------------------------------------------------------------------\n";
    report << fixedWidth("Engine", COL_ENGINE)
           << fixedWidth("Solved", COL_SOLVED)
           << fixedWidth("Rate", COL_RATE)
           << fixedWidth("Avg Time", COL_TIME)
           << fixedWidth("Avg Nodes", COL_NODES) << "\n";
    report << "--------------------------------------------------------------------------------\n";

    // Sort engines by pass rate for this suite (best first)
    std::vector<std::pair<EngineId, const TestSuiteResult*>> sortedResults;
    for (const auto& engine : engines) {
      const TestSuiteResult* result = data.getResult(suiteName, engine);
      sortedResults.emplace_back(engine, result);
    }
    std::sort(sortedResults.begin(), sortedResults.end(),
              [](const auto& a, const auto& b) {
                if (!a.second) return false;
                if (!b.second) return true;
                return a.second->getPassRate() > b.second->getPassRate();
              });

    for (const auto& [engine, result] : sortedResults) {
      // Engine name - truncated to fit column
      report << fixedWidth(engine.toDisplayString(), COL_ENGINE);

      if (result) {
        // Solved column
        std::ostringstream solved;
        solved << result->passed << "/" << result->totalTests;
        report << fixedWidth(solved.str(), COL_SOLVED);

        // Rate column
        std::ostringstream rate;
        rate << std::fixed << std::setprecision(1) << result->getPassRate() << "%";
        report << fixedWidth(rate.str(), COL_RATE);

        // Avg Time column
        report << fixedWidth(formatTime(result->getAvgTimeMs()), COL_TIME);

        // Avg Nodes column
        report << fixedWidth(formatNodes(result->getAvgNodes()), COL_NODES);
      } else {
        report << fixedWidth(Symbol::DASH, COL_SOLVED)
               << fixedWidth(Symbol::DASH, COL_RATE)
               << fixedWidth(Symbol::DASH, COL_TIME)
               << fixedWidth(Symbol::DASH, COL_NODES);
      }
      report << "\n";
    }
  }

  report << "\n================================================================================\n";

  return report.str();
}

std::string ArenaRunner::generateComparisonReport(
    const ReportData& data,
    const EngineId& targetEngine,
    const std::vector<EngineId>& baselineEngines) {

  std::ostringstream report;

  // Helper to pad/truncate string to exact width
  auto fixedWidth = [](const std::string& str, size_t width) -> std::string {
    if (str.length() >= width) {
      return str.substr(0, width - 2) + "..";
    }
    return str + std::string(width - str.length(), ' ');
  };

  // Column widths
  constexpr size_t COL_LABEL = 24;
  constexpr size_t COL_ENGINE = 16;

  // Determine baselines: if empty, use all engines except target
  std::vector<EngineId> baselines;
  if (baselineEngines.empty()) {
    for (const auto& engine : data.engines) {
      if (!(engine == targetEngine)) {
        baselines.push_back(engine);
      }
    }
  } else {
    baselines = baselineEngines;
  }

  // Header
  report << Color::color(Color::BOLD);
  report << "================================================================================\n";
  report << "ENGINE COMPARISON REPORT\n";
  report << "================================================================================\n";
  report << Color::color(Color::RESET);
  report << "Generated: " << getIsoTimestamp() << "\n";
  report << "Comparing: " << Color::colorize(targetEngine.toDisplayString(), Color::CYAN) << "\n";
  report << "Baselines: ";
  for (size_t i = 0; i < baselines.size(); ++i) {
    if (i > 0) report << ", ";
    report << baselines[i].toDisplayString();
  }
  report << "\n";
  report << "================================================================================\n";

  if (!data.hasEngine(targetEngine)) {
    report << "\n" << Color::colorize("ERROR: Target engine not found in results!", Color::RED) << "\n";
    report << "Available engines:\n";
    for (const auto& engine : data.engines) {
      report << "  - " << engine.toString() << "\n";
    }
    return report.str();
  }

  // Determine primary baseline (first in list)
  EngineId primaryBaseline = baselines.empty() ? EngineId{"", ""} : baselines[0];

  report << "\n";
  report << Color::color(Color::BOLD);
  report << "TEST SUITE RESULTS\n";
  report << Color::color(Color::RESET);
  report << "--------------------------------------------------------------------------------\n";

  // Build header row with all engines
  report << fixedWidth("", COL_LABEL);
  report << fixedWidth(targetEngine.toDisplayString(), COL_ENGINE);
  for (const auto& baseline : baselines) {
    report << fixedWidth(baseline.toDisplayString(), COL_ENGINE);
  }
  report << "\n";

  // Underline
  report << fixedWidth("", COL_LABEL);
  report << fixedWidth("--------", COL_ENGINE);
  for (size_t i = 0; i < baselines.size(); ++i) {
    report << fixedWidth("--------", COL_ENGINE);
  }
  report << "\n";

  // Track summary statistics
  int totalImprovement = 0;
  int totalPositions = 0;
  int suitesImproved = 0;
  int suitesRegressed = 0;

  // For each test suite
  for (const auto& suiteName : data.testSuites) {
    const TestSuiteResult* targetResult = data.getResult(suiteName, targetEngine);

    // Get total tests
    int totalTests = 0;
    if (targetResult) {
      totalTests = targetResult->totalTests;
    } else {
      // Try to get from any baseline
      for (const auto& baseline : baselines) {
        const TestSuiteResult* baseResult = data.getResult(suiteName, baseline);
        if (baseResult) {
          totalTests = baseResult->totalTests;
          break;
        }
      }
    }

    // Suite name row
    report << "\n" << Color::color(Color::BOLD) << suiteName
           << " (" << totalTests << " positions)" << Color::color(Color::RESET) << "\n";

    // Solved row
    report << fixedWidth("  Solved:", COL_LABEL);
    if (targetResult) {
      report << fixedWidth(std::to_string(targetResult->passed), COL_ENGINE);
    } else {
      report << fixedWidth(Symbol::DASH, COL_ENGINE);
    }
    for (const auto& baseline : baselines) {
      const TestSuiteResult* baseResult = data.getResult(suiteName, baseline);
      if (baseResult) {
        report << fixedWidth(std::to_string(baseResult->passed), COL_ENGINE);
      } else {
        report << fixedWidth(Symbol::DASH, COL_ENGINE);
      }
    }
    report << "\n";

    // Rate row
    report << fixedWidth("  Rate:", COL_LABEL);
    if (targetResult) {
      std::ostringstream rate;
      rate << std::fixed << std::setprecision(1) << targetResult->getPassRate() << "%";
      report << fixedWidth(rate.str(), COL_ENGINE);
    } else {
      report << fixedWidth(Symbol::DASH, COL_ENGINE);
    }
    for (const auto& baseline : baselines) {
      const TestSuiteResult* baseResult = data.getResult(suiteName, baseline);
      if (baseResult) {
        std::ostringstream rate;
        rate << std::fixed << std::setprecision(1) << baseResult->getPassRate() << "%";
        report << fixedWidth(rate.str(), COL_ENGINE);
      } else {
        report << fixedWidth(Symbol::DASH, COL_ENGINE);
      }
    }
    report << "\n";

    // Delta row (vs primary baseline)
    if (!primaryBaseline.name.empty()) {
      std::string deltaLabel = "  vs " + primaryBaseline.toDisplayString() + ":";
      if (deltaLabel.length() > COL_LABEL - 1) {
        deltaLabel = "  vs baseline:";
      }
      report << fixedWidth(deltaLabel, COL_LABEL);

      const TestSuiteResult* primaryResult = data.getResult(suiteName, primaryBaseline);

      if (targetResult && primaryResult) {
        int delta = targetResult->passed - primaryResult->passed;
        double deltaRate = targetResult->getPassRate() - primaryResult->getPassRate();

        std::ostringstream deltaStr;
        deltaStr << formatDelta(delta) << " (" << formatDeltaPercent(deltaRate) << ")";

        // Color based on delta, then pad
        std::string coloredDelta = Color::colorDelta(deltaStr.str(), delta);
        report << coloredDelta;
        // Pad after colored string (color codes don't take visual space)
        if (deltaStr.str().length() < COL_ENGINE) {
          report << std::string(COL_ENGINE - deltaStr.str().length(), ' ');
        }

        // Track stats
        totalImprovement += delta;
        totalPositions += targetResult->totalTests;
        if (delta > 0) suitesImproved++;
        if (delta < 0) suitesRegressed++;

      } else if (targetResult) {
        report << fixedWidth("N/A", COL_ENGINE);
      } else {
        report << fixedWidth(Symbol::DASH, COL_ENGINE);
      }

      // Mark baseline column
      report << fixedWidth("baseline", COL_ENGINE);

      // Other baselines - show vs primary
      for (size_t i = 1; i < baselines.size(); ++i) {
        const TestSuiteResult* otherResult = data.getResult(suiteName, baselines[i]);
        if (otherResult && primaryResult) {
          int delta = otherResult->passed - primaryResult->passed;
          double deltaRate = otherResult->getPassRate() - primaryResult->getPassRate();
          std::ostringstream deltaStr;
          deltaStr << formatDelta(delta) << " (" << formatDeltaPercent(deltaRate) << ")";
          std::string coloredDelta = Color::colorDelta(deltaStr.str(), delta);
          report << coloredDelta;
          if (deltaStr.str().length() < COL_ENGINE) {
            report << std::string(COL_ENGINE - deltaStr.str().length(), ' ');
          }
        } else {
          report << fixedWidth(Symbol::DASH, COL_ENGINE);
        }
      }
      report << "\n";
    }

    // Avg Time row
    report << fixedWidth("  Avg Time:", COL_LABEL);
    if (targetResult) {
      report << fixedWidth(formatTime(targetResult->getAvgTimeMs()), COL_ENGINE);
    } else {
      report << fixedWidth(Symbol::DASH, COL_ENGINE);
    }
    for (const auto& baseline : baselines) {
      const TestSuiteResult* baseResult = data.getResult(suiteName, baseline);
      if (baseResult) {
        report << fixedWidth(formatTime(baseResult->getAvgTimeMs()), COL_ENGINE);
      } else {
        report << fixedWidth(Symbol::DASH, COL_ENGINE);
      }
    }
    report << "\n";
  }

  // Summary section
  report << "\n";
  report << "--------------------------------------------------------------------------------\n";
  report << Color::color(Color::BOLD);
  report << "TEST SUITE SUMMARY (" << targetEngine.toDisplayString()
         << " vs " << primaryBaseline.toDisplayString() << ")\n";
  report << Color::color(Color::RESET);
  report << "--------------------------------------------------------------------------------\n";

  report << "  Total positions:      " << totalPositions << "\n";

  // Improvement/regression
  if (totalImprovement > 0) {
    std::ostringstream impStr;
    impStr << "+" << totalImprovement << " positions (+"
           << std::fixed << std::setprecision(1)
           << (totalPositions > 0 ? totalImprovement * 100.0 / totalPositions : 0) << "%)";
    report << "  Improvement:          " << Color::colorize(impStr.str(), Color::GREEN) << "\n";
  } else if (totalImprovement < 0) {
    std::ostringstream regStr;
    regStr << totalImprovement << " positions ("
           << std::fixed << std::setprecision(1)
           << (totalPositions > 0 ? totalImprovement * 100.0 / totalPositions : 0) << "%)";
    report << "  Regression:           " << Color::colorize(regStr.str(), Color::RED) << "\n";
  } else {
    report << "  Change:               " << Color::colorize("No change", Color::YELLOW) << "\n";
  }

  report << "  Suites improved:      " << suitesImproved << "\n";
  report << "  Suites regressed:     " << suitesRegressed << "\n";

  // Status
  report << "  Status:               ";
  if (totalImprovement > 0 && suitesRegressed == 0) {
    report << Symbol::CHECK << " " << Color::colorize("IMPROVEMENT", Color::GREEN);
  } else if (totalImprovement < 0 || suitesRegressed > suitesImproved) {
    report << Symbol::CROSS << " " << Color::colorize("REGRESSION", Color::RED);
  } else if (totalImprovement > 0) {
    report << Symbol::WARNING << " " << Color::colorize("MIXED (some suites regressed)", Color::YELLOW);
  } else {
    report << Symbol::EQUAL << " " << Color::colorize("NO CHANGE", Color::YELLOW);
  }
  report << "\n";

  report << "================================================================================\n";

  return report.str();
}


} // namespace arena
