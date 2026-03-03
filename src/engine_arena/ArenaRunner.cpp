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
      : arenaConfig(config), testSuiteRunner(config), matchRunner(config), resultWriter(config.resultsDir) {
  }

  void ArenaRunner::runAll() const {
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

  void ArenaRunner::runTestSuitesOnly() const {
    std::cout << "\n===================================================================" << std::endl;
    std::cout << "Running Test Suites Only" << std::endl;
    std::cout << "===================================================================" << std::endl;
    std::cout << "Number of suites: " << arenaConfig.testSuites.size() << std::endl;
    std::cout << "===================================================================" << std::endl;

    // Pass callback that saves results immediately after each suite completes
    const auto results = testSuiteRunner.runAllTestSuites(
      [this](const TestSuiteResult& result) {
        const std::string jsonPath = resultWriter.writeTestSuiteResult(result);
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
    auto time_t    = system_clock::to_time_t(now);
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
    auto time_t    = system_clock::to_time_t(now);
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

  std::string ArenaRunner::formatNumber(const int64_t value) {
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

  std::string ArenaRunner::formatNodes(const double nodes) {
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

  std::string ArenaRunner::formatTime(const double timeMs) {
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

  std::string ArenaRunner::formatDelta(const int delta) {
    if (delta > 0) return "+" + std::to_string(delta);
    if (delta < 0) return std::to_string(delta);
    return "0";
  }

  std::string ArenaRunner::formatDeltaPercent(const double delta) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    if (delta > 0) oss << "+";
    oss << delta << "%";
    return oss.str();
  }

  ReportData ArenaRunner::loadAllResults() const {
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
        result.arenaVersion  = jsonData["arenaVersion"];
        result.timestamp     = jsonData["timestamp"];
        result.testSuiteName = jsonData["testSuite"]["name"];
        result.epdPath       = jsonData["testSuite"]["epdPath"];
        result.engineName    = jsonData["engine"]["name"];
        result.engineVersion = jsonData["engine"]["version"];
        result.enginePath    = jsonData["engine"]["path"];

        // Parse summary section
        const auto& summary = jsonData["summary"];
        result.totalTests   = summary["totalTests"];
        result.passed       = summary["passed"];
        result.failed       = summary["failed"];
        result.skipped      = summary["skipped"];
        result.totalNodes   = summary["totalNodes"];
        result.totalTimeMs  = summary["totalTimeMs"];

        // Skip details for reporting (not needed, saves memory)
        // result.details can be populated if needed

        // Get engine identifier
        EngineId engineId            = result.getEngineId();
        const std::string& suiteName = result.testSuiteName;

        // Add to data structures
        data.testSuites.insert(suiteName);
        data.engines.insert(engineId);

        // Check if we already have a result for this suite/engine
        auto& suiteMap = data.suiteResults[suiteName];
        auto existing  = suiteMap.find(engineId);
        if (existing == suiteMap.end() || result.timestamp > existing->second.timestamp) {
          // Keep the latest result
          suiteMap[engineId] = result;
        }

      } catch (const std::exception& e) {
        std::cerr << "  Warning: Failed to load " << entry.path().filename().string()
                  << ": " << e.what() << std::endl;
      }
    }

    // Also load match results
    loadMatchResults(data);

    return data;
  }

  std::set<EngineId> ArenaRunner::listAvailableEngines() const {
    ReportData data = loadAllResults();
    return data.engines;
  }

  void ArenaRunner::loadMatchResults(ReportData& data) const {
    const std::filesystem::path matchesDir = std::filesystem::path(arenaConfig.resultsDir) / "matches";

    if (!std::filesystem::exists(matchesDir)) {
      // No matches directory - this is ok, just means no matches have been run
      return;
    }

    // Load all JSON files from matches directory
    for (const auto& entry : std::filesystem::directory_iterator(matchesDir)) {
      if (entry.path().extension() != ".json") continue;

      try {
        std::ifstream file(entry.path());
        json jsonData = json::parse(file);

        MatchResult result;

        // Parse new JSON format
        result.arenaVersion = jsonData["arenaVersion"];
        result.timestamp    = jsonData["timestamp"];

        // Match info
        result.matchName   = jsonData["match"]["name"];
        result.timeControl = jsonData["match"]["timeControl"];
        result.rounds      = jsonData["match"]["rounds"];

        // Engine 1
        result.engine1Name    = jsonData["engine1"]["name"];
        result.engine1Version = jsonData["engine1"]["version"];
        result.engine1Path    = jsonData["engine1"]["path"];

        // Engine 2
        result.engine2Name    = jsonData["engine2"]["name"];
        result.engine2Version = jsonData["engine2"]["version"];
        result.engine2Path    = jsonData["engine2"]["path"];

        // Results
        const auto& results  = jsonData["results"];
        result.engine1Wins   = results["engine1Wins"];
        result.engine2Wins   = results["engine2Wins"];
        result.draws         = results["draws"];
        result.engine1Score  = results["engine1Score"];
        result.engine2Score  = results["engine2Score"];
        result.eloDifference = results["eloDifference"];

        // Additional data
        result.pgnPath    = jsonData["pgnPath"];
        result.durationMs = jsonData["durationMs"];

        // Add engines to set
        data.engines.insert(result.getEngine1Id());
        data.engines.insert(result.getEngine2Id());

        // Get match key
        std::string matchKey = result.getMatchKey();

        // Check if we already have a result for this match pair
        auto existing = data.matchResults.find(matchKey);
        if (existing == data.matchResults.end() || result.timestamp > existing->second.timestamp) {
          // Keep the latest result
          data.matchResults[matchKey] = result;
        }

      } catch (const std::exception& e) {
        std::cerr << "  Warning: Failed to load " << entry.path().filename().string()
                  << ": " << e.what() << std::endl;
      }
    }
  }

  std::string ArenaRunner::generateBaselineReport(const ReportData& data) {
    std::ostringstream report;

    // Helper to pad/truncate string to exact width
    auto fixedWidth = [](const std::string& str, const size_t width) -> std::string {
      if (str.length() >= width) {
        return str.substr(0, width - 2) + "..";
      }
      return str + std::string(width - str.length(), ' ');
    };


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
    std::vector engines(data.engines.begin(), data.engines.end());

    // For each test suite, show all engines side by side
    for (const auto& suiteName : data.testSuites) {
      // Column widths
      constexpr size_t COL_NODES  = 12;
      constexpr size_t COL_TIME   = 12;
      constexpr size_t COL_RATE   = 10;
      constexpr size_t COL_SOLVED = 12;
      constexpr size_t COL_ENGINE = 28;

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
      std::ranges::sort(sortedResults,
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
        }
        else {
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

  std::string ArenaRunner::generateMatchBaselineReport(const ReportData& data) {
    std::ostringstream report;

    // Helper to pad/truncate string to exact width
    auto fixedWidth = [](const std::string& str, const size_t width) -> std::string {
      if (str.length() >= width) {
        return str.substr(0, width - 2) + "..";
      }
      return str + std::string(width - str.length(), ' ');
    };

    // Column widths
    constexpr size_t COL_PAIR  = 42;
    constexpr size_t COL_GAMES = 10;
    constexpr size_t COL_SCORE = 12;
    constexpr size_t COL_WDL   = 16;
    constexpr size_t COL_ELO   = 12;

    // Header
    report << "\n";
    report << Color::color(Color::BOLD);
    report << "================================================================================\n";
    report << "MATCH RESULTS - All Engine Pairs\n";
    report << "================================================================================\n";
    report << Color::color(Color::RESET);

    if (!data.hasMatchResults()) {
      report << "\nNo match results found.\n";
      report << "Run matches first: FrankyCPP_Arena --matches\n";
      return report.str();
    }

    report << "--------------------------------------------------------------------------------\n";
    report << fixedWidth("Engine Pair", COL_PAIR)
           << fixedWidth("Games", COL_GAMES)
           << fixedWidth("Score", COL_SCORE)
           << fixedWidth("W/D/L", COL_WDL)
           << fixedWidth("ELO Diff", COL_ELO) << "\n";
    report << "--------------------------------------------------------------------------------\n";

    // Display all matches
    for (const auto& match : data.matchResults | std::views::values) {
      // Engine pair column
      std::string pairStr = match.getEngine1Id().toDisplayString() + " vs " + match.getEngine2Id().toDisplayString();
      report << fixedWidth(pairStr, COL_PAIR);

      // Games column
      report << fixedWidth(std::to_string(match.getTotalGames()), COL_GAMES);

      // Score column (from engine1's perspective)
      std::ostringstream scoreStr;
      scoreStr << std::fixed << std::setprecision(1) << match.getEngine1WinRate() << "%";
      report << fixedWidth(scoreStr.str(), COL_SCORE);

      // W/D/L column
      std::string wdlStr = std::to_string(match.engine1Wins) + "/" + std::to_string(match.draws) + "/" + std::to_string(match.engine2Wins);
      report << fixedWidth(wdlStr, COL_WDL);

      // ELO difference column
      std::ostringstream eloStr;
      if (match.eloDifference > 0) eloStr << "+";
      eloStr << std::fixed << std::setprecision(0) << match.eloDifference;
      report << fixedWidth(eloStr.str(), COL_ELO);

      report << "\n";
    }

    report << "================================================================================\n";

    return report.str();
  }

  std::string ArenaRunner::generateMatchComparisonReport(
    const ReportData& data,
    const EngineId& targetEngine,
    const std::vector<EngineId>& baselineEngines) {

    std::ostringstream report;

    // Helper to pad/truncate string to exact width
    auto fixedWidth = [](const std::string& str, const size_t width) -> std::string {
      if (str.length() >= width) {
        return str.substr(0, width - 2) + "..";
      }
      return str + std::string(width - str.length(), ' ');
    };

    // Column widths
    constexpr size_t COL_OPPONENT = 28;
    constexpr size_t COL_GAMES    = 10;
    constexpr size_t COL_SCORE    = 12;
    constexpr size_t COL_WDL      = 16;
    constexpr size_t COL_ELO      = 12;
    constexpr size_t COL_VS       = 14;

    // Resolve target engine using flexible matching
    auto resolvedTarget   = data.findEngine(targetEngine);
    EngineId actualTarget = resolvedTarget ? *resolvedTarget : targetEngine;

    // Determine which baselines to use
    std::vector<EngineId> baselines;
    if (baselineEngines.empty()) {
      // Use all engines as baselines
      for (const auto& engine : data.engines) {
        if (engine != actualTarget) {
          baselines.push_back(engine);
        }
      }
    }
    else {
      // Resolve each baseline engine
      for (const auto& baseline : baselineEngines) {
        if (auto resolved = data.findEngine(baseline)) {
          baselines.push_back(*resolved);
        }
        else {
          // Use as-is if not found (will show as missing in results)
          baselines.push_back(baseline);
        }
      }
    }

    // Header
    report << "\n";
    report << Color::color(Color::BOLD);
    report << "================================================================================\n";
    report << "MATCH COMPARISON: " << actualTarget.toDisplayString() << " vs Baselines\n";
    report << "================================================================================\n";
    report << Color::color(Color::RESET);

    if (!data.hasMatchResults()) {
      report << "\nNo match results found.\n";
      return report.str();
    }

    // Find primary baseline (first in list)
    const EngineId* primaryBaseline = baselines.empty() ? nullptr : &baselines[0];

    report << "--------------------------------------------------------------------------------\n";
    report << fixedWidth("Opponent", COL_OPPONENT)
           << fixedWidth("Games", COL_GAMES)
           << fixedWidth("Score", COL_SCORE)
           << fixedWidth("W/D/L", COL_WDL)
           << fixedWidth("ELO", COL_ELO);
    if (primaryBaseline) {
      report << fixedWidth("vs " + primaryBaseline->toDisplayString(), COL_VS);
    }
    report << "\n";
    report << "--------------------------------------------------------------------------------\n";

    // Get primary baseline's ELO for comparison
    double primaryBaselineElo = 0.0;
    bool hasPrimaryBaseline   = false;
    if (primaryBaseline) {
      if (const MatchResult* primaryMatch = data.getMatch(actualTarget, *primaryBaseline)) {
        // Calculate ELO from target's perspective
        if (primaryMatch->getEngine1Id() == actualTarget) {
          primaryBaselineElo = primaryMatch->eloDifference;
        }
        else {
          primaryBaselineElo = -primaryMatch->eloDifference;
        }
        hasPrimaryBaseline = true;
      }
    }

    // Display matches against each baseline
    bool hasResults = false;
    for (const auto& opponent : baselines) {
      const MatchResult* match = data.getMatch(actualTarget, opponent);
      if (!match) continue;

      hasResults = true;

      // Determine if target is engine1 or engine2
      bool targetIsEngine1 = (match->getEngine1Id() == actualTarget);

      // Opponent column
      report << fixedWidth(opponent.toDisplayString(), COL_OPPONENT);

      // Games column
      report << fixedWidth(std::to_string(match->getTotalGames()), COL_GAMES);

      // Score column (from target's perspective)
      double targetScore = targetIsEngine1 ? match->getEngine1WinRate() : (100.0 - match->getEngine1WinRate());
      std::ostringstream scoreStr;
      scoreStr << std::fixed << std::setprecision(1) << targetScore << "%";
      report << fixedWidth(scoreStr.str(), COL_SCORE);

      // W/D/L column (from target's perspective)
      int targetWins     = targetIsEngine1 ? match->engine1Wins : match->engine2Wins;
      int opponentWins   = targetIsEngine1 ? match->engine2Wins : match->engine1Wins;
      std::string wdlStr = std::to_string(targetWins) + "/" + std::to_string(match->draws) + "/" + std::to_string(opponentWins);
      report << fixedWidth(wdlStr, COL_WDL);

      // ELO column (from target's perspective)
      double targetElo = targetIsEngine1 ? match->eloDifference : -match->eloDifference;
      std::ostringstream eloStr;
      if (targetElo > 0) eloStr << "+";
      eloStr << std::fixed << std::setprecision(0) << targetElo;
      report << fixedWidth(eloStr.str(), COL_ELO);

      // vs baseline column
      // ReSharper disable once CppDFANullDereference
      if (hasPrimaryBaseline && opponent == *primaryBaseline) {
        report << fixedWidth("[baseline]", COL_VS);
      }
      else if (hasPrimaryBaseline) {
        double delta = targetElo - primaryBaselineElo;
        std::ostringstream deltaStr;
        if (delta > 0) {
          deltaStr << Color::color(Color::GREEN) << "+" << std::fixed << std::setprecision(0) << delta << " ELO" << Color::color(Color::RESET);
        }
        else if (delta < 0) {
          deltaStr << Color::color(Color::RED) << std::fixed << std::setprecision(0) << delta << " ELO" << Color::color(Color::RESET);
        }
        else {
          deltaStr << "0 ELO";
        }
        report << fixedWidth(deltaStr.str(), COL_VS + 18);// +18 for ANSI codes
      }

      report << "\n";
    }

    if (!hasResults) {
      report << "\n"
             << Color::color(Color::YELLOW) << "No match results found for " << actualTarget.toDisplayString() << Color::color(Color::RESET) << "\n";
      report << "Run matches with this engine to see comparisons.\n";
    }

    report << "================================================================================\n";

    return report.str();
  }

  std::string ArenaRunner::generateComparisonReport(
    const ReportData& data,
    const EngineId& targetEngine,
    const std::vector<EngineId>& baselineEngines) {

    std::ostringstream report;

    // Helper to pad/truncate string to exact width
    auto fixedWidth = [](const std::string& str, const size_t width) -> std::string {
      if (str.length() >= width) {
        return str.substr(0, width - 2) + "..";
      }
      return str + std::string(width - str.length(), ' ');
    };

    // Column widths
    constexpr size_t COL_LABEL  = 24;
    constexpr size_t COL_ENGINE = 16;

    // Resolve target engine using flexible matching
    auto resolvedTarget = data.findEngine(targetEngine);
    if (!resolvedTarget) {
      // Header
      report << Color::color(Color::BOLD);
      report << "================================================================================\n";
      report << "ENGINE COMPARISON REPORT\n";
      report << "================================================================================\n";
      report << Color::color(Color::RESET);
      report << "Generated: " << getIsoTimestamp() << "\n";
      report << "Comparing: " << Color::colorize(targetEngine.toDisplayString(), Color::CYAN) << "\n";
      report << "Baselines: (not resolved)\n";
      report << "================================================================================\n";

      report << "\n"
             << Color::colorize("ERROR: Target engine not found in results!", Color::RED) << "\n";
      report << "Available engines:\n";
      for (const auto& engine : data.engines) {
        report << "  - " << engine.toString() << "\n";
      }
      return report.str();
    }

    EngineId actualTarget = *resolvedTarget;

    // Determine baselines: if empty, use all engines except target
    std::vector<EngineId> baselines;
    if (baselineEngines.empty()) {
      for (const auto& engine : data.engines) {
        if (engine != actualTarget) {
          baselines.push_back(engine);
        }
      }
    }
    else {
      // Resolve each baseline engine
      for (const auto& baseline : baselineEngines) {
        if (auto resolved = data.findEngine(baseline)) {
          baselines.push_back(*resolved);
        }
        else {
          // Use as-is if not found (will show as missing in results)
          baselines.push_back(baseline);
        }
      }
    }

    // Header
    report << Color::color(Color::BOLD);
    report << "================================================================================\n";
    report << "ENGINE COMPARISON REPORT\n";
    report << "================================================================================\n";
    report << Color::color(Color::RESET);
    report << "Generated: " << getIsoTimestamp() << "\n";
    report << "Comparing: " << Color::colorize(actualTarget.toDisplayString(), Color::CYAN) << "\n";
    report << "Baselines: ";
    for (size_t i = 0; i < baselines.size(); ++i) {
      if (i > 0) report << ", ";
      report << baselines[i].toDisplayString();
    }
    report << "\n";
    report << "================================================================================\n";


    // Determine primary baseline (first in list)
    EngineId primaryBaseline = baselines.empty() ? EngineId{"", ""} : baselines[0];

    report << "\n";
    report << Color::color(Color::BOLD);
    report << "TEST SUITE RESULTS\n";
    report << Color::color(Color::RESET);
    report << "--------------------------------------------------------------------------------\n";

    // Build header row with all engines
    report << fixedWidth("", COL_LABEL);
    report << fixedWidth(actualTarget.toDisplayString(), COL_ENGINE);
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
    int totalPositions   = 0;
    int suitesImproved   = 0;
    int suitesRegressed  = 0;

    // For each test suite
    for (const auto& suiteName : data.testSuites) {
      const TestSuiteResult* targetResult = data.getResult(suiteName, actualTarget);

      // Get total tests
      int totalTests = 0;
      if (targetResult) {
        totalTests = targetResult->totalTests;
      }
      else {
        // Try to get from any baseline
        for (const auto& baseline : baselines) {
          if (const TestSuiteResult* baseResult = data.getResult(suiteName, baseline)) {
            totalTests = baseResult->totalTests;
            break;
          }
        }
      }

      // Suite name row
      report << "\n"
             << Color::color(Color::BOLD) << suiteName
             << " (" << totalTests << " positions)" << Color::color(Color::RESET) << "\n";

      // Solved row
      report << fixedWidth("  Solved:", COL_LABEL);
      if (targetResult) {
        report << fixedWidth(std::to_string(targetResult->passed), COL_ENGINE);
      }
      else {
        report << fixedWidth(Symbol::DASH, COL_ENGINE);
      }
      for (const auto& baseline : baselines) {
        if (const TestSuiteResult* baseResult = data.getResult(suiteName, baseline)) {
          report << fixedWidth(std::to_string(baseResult->passed), COL_ENGINE);
        }
        else {
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
      }
      else {
        report << fixedWidth(Symbol::DASH, COL_ENGINE);
      }
      for (const auto& baseline : baselines) {
        if (const TestSuiteResult* baseResult = data.getResult(suiteName, baseline)) {
          std::ostringstream rate;
          rate << std::fixed << std::setprecision(1) << baseResult->getPassRate() << "%";
          report << fixedWidth(rate.str(), COL_ENGINE);
        }
        else {
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
          int delta        = targetResult->passed - primaryResult->passed;
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
        }
        else if (targetResult) {
          report << fixedWidth("N/A", COL_ENGINE);
        }
        else {
          report << fixedWidth(Symbol::DASH, COL_ENGINE);
        }

        // Mark baseline column
        report << fixedWidth("baseline", COL_ENGINE);

        // Other baselines - show vs primary
        for (size_t i = 1; i < baselines.size(); ++i) {
          const TestSuiteResult* otherResult = data.getResult(suiteName, baselines[i]);
          if (otherResult && primaryResult) {
            int delta        = otherResult->passed - primaryResult->passed;
            double deltaRate = otherResult->getPassRate() - primaryResult->getPassRate();
            std::ostringstream deltaStr;
            deltaStr << formatDelta(delta) << " (" << formatDeltaPercent(deltaRate) << ")";
            std::string coloredDelta = Color::colorDelta(deltaStr.str(), delta);
            report << coloredDelta;
            if (deltaStr.str().length() < COL_ENGINE) {
              report << std::string(COL_ENGINE - deltaStr.str().length(), ' ');
            }
          }
          else {
            report << fixedWidth(Symbol::DASH, COL_ENGINE);
          }
        }
        report << "\n";
      }

      // Avg Time row
      report << fixedWidth("  Avg Time:", COL_LABEL);
      if (targetResult) {
        report << fixedWidth(formatTime(targetResult->getAvgTimeMs()), COL_ENGINE);
      }
      else {
        report << fixedWidth(Symbol::DASH, COL_ENGINE);
      }
      for (const auto& baseline : baselines) {
        if (const TestSuiteResult* baseResult = data.getResult(suiteName, baseline)) {
          report << fixedWidth(formatTime(baseResult->getAvgTimeMs()), COL_ENGINE);
        }
        else {
          report << fixedWidth(Symbol::DASH, COL_ENGINE);
        }
      }
      report << "\n";
    }

    // Summary section
    report << "\n";
    report << "--------------------------------------------------------------------------------\n";
    report << Color::color(Color::BOLD);
    report << "TEST SUITE SUMMARY (" << actualTarget.toDisplayString()
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
    }
    else if (totalImprovement < 0) {
      std::ostringstream regStr;
      regStr << totalImprovement << " positions ("
             << std::fixed << std::setprecision(1)
             << (totalPositions > 0 ? totalImprovement * 100.0 / totalPositions : 0) << "%)";
      report << "  Regression:           " << Color::colorize(regStr.str(), Color::RED) << "\n";
    }
    else {
      report << "  Change:               " << Color::colorize("No change", Color::YELLOW) << "\n";
    }

    report << "  Suites improved:      " << suitesImproved << "\n";
    report << "  Suites regressed:     " << suitesRegressed << "\n";

    // Status
    report << "  Status:               ";
    if (totalImprovement > 0 && suitesRegressed == 0) {
      report << Symbol::CHECK << " " << Color::colorize("IMPROVEMENT", Color::GREEN);
    }
    else if (totalImprovement < 0 || suitesRegressed > suitesImproved) {
      report << Symbol::CROSS << " " << Color::colorize("REGRESSION", Color::RED);
    }
    else if (totalImprovement > 0) {
      report << Symbol::WARNING << " " << Color::colorize("MIXED (some suites regressed)", Color::YELLOW);
    }
    else {
      report << Symbol::EQUAL << " " << Color::colorize("NO CHANGE", Color::YELLOW);
    }
    report << "\n";

    report << "================================================================================\n";

    return report.str();
  }


}// namespace arena
