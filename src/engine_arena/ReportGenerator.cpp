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
// ReportGenerator.cpp - Arena Report Formatting Implementation
//=============================================================================

#include "ReportGenerator.h"
#include "ConsoleColors.h"
#include "ResultStore.h"
#include "common/FormatUtils.h"
#include "common/TimeUtils.h"

#include <algorithm>
#include <iomanip>
#include <ranges>
#include <sstream>

namespace arena {

  using common::displayTimestamp;
  using common::fixedWidth;
  using common::formatDelta;
  using common::formatDeltaPercent;
  using common::formatNodes;
  using common::formatNumber;
  using common::formatPercent;
  using common::formatTime;

  //=========================================================================
  // Test Suite Baseline Report
  //=========================================================================

  std::string ReportGenerator::generateBaselineReport(const ReportData& data) {
    std::ostringstream report;

    // Header
    report << Color::color(Color::BOLD);
    report << "================================================================================\n";
    report << "BASELINE RESULTS - All Engines\n";
    report << "================================================================================\n";
    report << Color::color(Color::RESET);
    report << "Generated: " << displayTimestamp() << "\n";
    report << "================================================================================\n";

    if (!data.hasResults()) {
      report << "\nNo results found.\n";
      report << "Run test suites first: FrankyCPP_Arena --testsuites\n";
      return report.str();
    }

    // Collect engines directly from test suite result keys (not from data.engines)
    // This avoids duplicates caused by matches/benchmarks registering engine IDs
    // with slightly different names (e.g., "FrankyCPP" vs "FrankyCPP v1.3") that
    // resolve to the same results via flexible matching
    std::set<EngineId> engineSet;
    for (const auto& engineResults : data.suiteResults | std::views::values) {
      for (const auto& engineId : engineResults | std::views::keys) {
        engineSet.insert(engineId);
      }
    }
    std::vector engines(engineSet.begin(), engineSet.end());

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
        const TestSuiteResult* result = ResultStore::getResult(data, suiteName, engine);
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
          report << fixedWidth(formatPercent(result->getPassRate()), COL_RATE);

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

  //=========================================================================
  // Match Baseline Report
  //=========================================================================

  std::string ReportGenerator::generateMatchBaselineReport(const ReportData& data) {
    std::ostringstream report;

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
      const std::string pairStr = match.getEngine1Id().toDisplayString() + " vs " + match.getEngine2Id().toDisplayString();
      report << fixedWidth(pairStr, COL_PAIR);

      // Games column
      report << fixedWidth(std::to_string(match.getTotalGames()), COL_GAMES);

      // Score column (from engine1's perspective)
      report << fixedWidth(formatPercent(match.getEngine1WinRate()), COL_SCORE);

      // W/D/L column
      const std::string wdlStr = std::to_string(match.engine1Wins) + "/" + std::to_string(match.draws) + "/" + std::to_string(match.engine2Wins);
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

  //=========================================================================
  // Match Comparison Report
  //=========================================================================

  std::string ReportGenerator::generateMatchComparisonReport(
    const ReportData& data,
    const EngineId& targetEngine,
    const std::vector<EngineId>& baselineEngines) {

    std::ostringstream report;

    // Column widths
    constexpr size_t COL_OPPONENT = 28;
    constexpr size_t COL_GAMES    = 10;
    constexpr size_t COL_SCORE    = 12;
    constexpr size_t COL_WDL      = 16;
    constexpr size_t COL_ELO      = 12;
    constexpr size_t COL_VS       = 14;

    // Resolve target engine using flexible matching
    const auto resolvedTarget   = ResultStore::findEngine(data, targetEngine);
    const EngineId actualTarget = resolvedTarget ? *resolvedTarget : targetEngine;

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
        if (const auto resolved = ResultStore::findEngine(data, baseline)) {
          baselines.push_back(*resolved);
        }
        else {
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
      if (const MatchResult* primaryMatch = ResultStore::getMatch(data, actualTarget, *primaryBaseline)) {
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
      const MatchResult* match = ResultStore::getMatch(data, actualTarget, opponent);
      if (!match) continue;

      hasResults = true;

      // Determine if target is engine1 or engine2
      const bool targetIsEngine1 = (match->getEngine1Id() == actualTarget);

      // Opponent column
      report << fixedWidth(opponent.toDisplayString(), COL_OPPONENT);

      // Games column
      report << fixedWidth(std::to_string(match->getTotalGames()), COL_GAMES);

      // Score column (from target's perspective)
      const double targetScore = targetIsEngine1 ? match->getEngine1WinRate() : (100.0 - match->getEngine1WinRate());
      report << fixedWidth(formatPercent(targetScore), COL_SCORE);

      // W/D/L column (from target's perspective)
      const int targetWins     = targetIsEngine1 ? match->engine1Wins : match->engine2Wins;
      const int opponentWins   = targetIsEngine1 ? match->engine2Wins : match->engine1Wins;
      const std::string wdlStr = std::to_string(targetWins) + "/" + std::to_string(match->draws) + "/" + std::to_string(opponentWins);
      report << fixedWidth(wdlStr, COL_WDL);

      // ELO column (from target's perspective)
      const double targetElo = targetIsEngine1 ? match->eloDifference : -match->eloDifference;
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
        const double delta = targetElo - primaryBaselineElo;
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
        report << fixedWidth(deltaStr.str(), COL_VS + 18); // +18 for ANSI codes
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

  //=========================================================================
  // Test Suite Comparison Report
  //=========================================================================

  std::string ReportGenerator::generateComparisonReport(
    const ReportData& data,
    const EngineId& targetEngine,
    const std::vector<EngineId>& baselineEngines) {

    std::ostringstream report;

    // Column widths
    constexpr size_t COL_LABEL  = 24;
    constexpr size_t COL_ENGINE = 16;

    // Resolve target engine using flexible matching
    const auto resolvedTarget = ResultStore::findEngine(data, targetEngine);
    if (!resolvedTarget) {
      // Header
      report << Color::color(Color::BOLD);
      report << "================================================================================\n";
      report << "ENGINE COMPARISON REPORT\n";
      report << "================================================================================\n";
      report << Color::color(Color::RESET);
      report << "Generated: " << displayTimestamp() << "\n";
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

    const EngineId actualTarget = *resolvedTarget;

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
        if (const auto resolved = ResultStore::findEngine(data, baseline)) {
          baselines.push_back(*resolved);
        }
        else {
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
    report << "Generated: " << displayTimestamp() << "\n";
    report << "Comparing: " << Color::colorize(actualTarget.toDisplayString(), Color::CYAN) << "\n";
    report << "Baselines: ";
    for (size_t i = 0; i < baselines.size(); ++i) {
      if (i > 0) report << ", ";
      report << baselines[i].toDisplayString();
    }
    report << "\n";
    report << "================================================================================\n";

    // Determine primary baseline (first in list)
    const EngineId primaryBaseline = baselines.empty() ? EngineId{"", ""} : baselines[0];

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
      const TestSuiteResult* targetResult = ResultStore::getResult(data, suiteName, actualTarget);

      // Get total tests
      int totalTests = 0;
      if (targetResult) {
        totalTests = targetResult->totalTests;
      }
      else {
        // Try to get from any baseline
        for (const auto& baseline : baselines) {
          if (const TestSuiteResult* baseResult = ResultStore::getResult(data, suiteName, baseline)) {
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
        if (const TestSuiteResult* baseResult = ResultStore::getResult(data, suiteName, baseline)) {
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
        report << fixedWidth(formatPercent(targetResult->getPassRate()), COL_ENGINE);
      }
      else {
        report << fixedWidth(Symbol::DASH, COL_ENGINE);
      }
      for (const auto& baseline : baselines) {
        if (const TestSuiteResult* baseResult = ResultStore::getResult(data, suiteName, baseline)) {
          report << fixedWidth(formatPercent(baseResult->getPassRate()), COL_ENGINE);
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

        const TestSuiteResult* primaryResult = ResultStore::getResult(data, suiteName, primaryBaseline);

        if (targetResult && primaryResult) {
          const int delta        = targetResult->passed - primaryResult->passed;
          const double deltaRate = targetResult->getPassRate() - primaryResult->getPassRate();

          std::ostringstream deltaStr;
          deltaStr << formatDelta(delta) << " (" << formatDeltaPercent(deltaRate) << ")";

          // Color based on delta, then pad
          const std::string coloredDelta = Color::colorDelta(deltaStr.str(), delta);
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
          const TestSuiteResult* otherResult = ResultStore::getResult(data, suiteName, baselines[i]);
          if (otherResult && primaryResult) {
            const int delta        = otherResult->passed - primaryResult->passed;
            const double deltaRate = otherResult->getPassRate() - primaryResult->getPassRate();
            std::ostringstream deltaStr;
            deltaStr << formatDelta(delta) << " (" << formatDeltaPercent(deltaRate) << ")";
            const std::string coloredDelta = Color::colorDelta(deltaStr.str(), delta);
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
        if (const TestSuiteResult* baseResult = ResultStore::getResult(data, suiteName, baseline)) {
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
      const double impPct = totalPositions > 0 ? totalImprovement * 100.0 / totalPositions : 0;
      std::ostringstream impStr;
      impStr << "+" << totalImprovement << " positions (+"
             << formatPercent(impPct) << ")";
      report << "  Improvement:          " << Color::colorize(impStr.str(), Color::GREEN) << "\n";
    }
    else if (totalImprovement < 0) {
      const double regPct = totalPositions > 0 ? totalImprovement * 100.0 / totalPositions : 0;
      std::ostringstream regStr;
      regStr << totalImprovement << " positions ("
             << formatPercent(regPct) << ")";
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

  //=========================================================================
  // Engine Summary Report
  //=========================================================================

  std::string ReportGenerator::generateEngineSummary(
    const ReportData& data,
    const EngineId& engine,
    const bool showHistory) {

    std::ostringstream report;

    // Find the actual engine using flexible matching
    const auto foundEngine = ResultStore::findEngine(data, engine);
    if (!foundEngine) {
      report << "ERROR: Engine '" << engine.toString() << "' not found in results.\n\n";
      report << "Available engines:\n";
      for (const auto& e : data.engines) {
        report << "  " << e.toString() << "\n";
      }
      return report.str();
    }

    const EngineId& actualEngine = *foundEngine;

    // Get matches and benchmarks (same for both modes)
    const auto matches    = ResultStore::getMatchesForEngine(data, actualEngine);
    const auto benchmarks = ResultStore::getBenchmarksForEngine(data, actualEngine);

    // Header
    report << "===================================================================\n";
    report << "Engine Summary: " << actualEngine.toDisplayString();
    report << "\n";
    report << "===================================================================\n";

    if (showHistory) {
      // ==================== HISTORY MODE ====================
      // Show all historical runs grouped by tag

      // Collect all results for this engine (using flexible matching)
      std::vector<TestSuiteResult> allResults;
      for (const auto& [engineId, results] : data.allSuiteResults) {
        if (ResultStore::enginesMatchFlexibly(engineId, actualEngine)) {
          for (const auto& r : results) {
            allResults.push_back(r);
          }
        }
      }

      if (allResults.empty()) {
        report << "No test suite history found.\n";
      }
      else {
        // Group by date+tag (runs done on same day with same tag are one batch)
        struct RunGroup {
          std::string tag;
          std::string date;                                           // YYYY-MM-DD
          std::string latestTimestamp;                                // For sorting
          std::map<std::string, const TestSuiteResult*> suiteResults; // Latest result per suite
        };

        // Group results by date+tag, keeping only latest result per suite
        std::map<std::string, RunGroup> groupsByDateTag;
        for (const auto& r : allResults) {
          const std::string date = r.timestamp.substr(0, 10); // YYYY-MM-DD
          const std::string key  = date + "|" + r.tag;

          auto& group = groupsByDateTag[key];
          group.date  = date;
          group.tag   = r.tag;

          // Track latest timestamp for sorting
          if (group.latestTimestamp.empty() || r.timestamp > group.latestTimestamp) {
            group.latestTimestamp = r.timestamp;
          }

          // Keep only the latest result for each suite within this group
          const auto it = group.suiteResults.find(r.testSuiteName);
          if (it == group.suiteResults.end() || r.timestamp > it->second->timestamp) {
            group.suiteResults[r.testSuiteName] = &r;
          }
        }

        // Convert to vector and sort by timestamp (most recent first)
        std::vector<RunGroup> runs;
        for (auto& group : groupsByDateTag | std::views::values) {
          runs.push_back(std::move(group));
        }
        std::ranges::sort(runs, [](const RunGroup& a, const RunGroup& b) {
          return a.latestTimestamp > b.latestTimestamp;
        });

        report << "Historical Test Suite Runs:\n";
        report << "-------------------------------------------------------------------\n";

        for (const auto& run : runs) {
          // Calculate totals from the latest results per suite
          int totalPassed = 0;
          int totalTests  = 0;
          for (const auto& result : run.suiteResults | std::views::values) {
            totalPassed += result->passed;
            totalTests += result->totalTests;
          }

          const double pct = totalTests > 0 ? (totalPassed * 100.0 / totalTests) : 0.0;

          report << "  [" << run.date << "] "
                 << std::right << std::setw(4) << totalPassed << "/"
                 << std::left << std::setw(5) << totalTests
                 << " (" << std::right << std::setw(6) << formatPercent(pct) << ")"
                 << "  (" << run.suiteResults.size() << " suites)";

          if (!run.tag.empty()) {
            report << "  [" << run.tag << "]";
          }
          report << "\n";
        }
        report << "===================================================================\n";
      }
    }
    else {
      // ==================== LATEST MODE ====================
      // Show only the latest results (default behavior)

      struct SuiteData {
        std::string name;
        int passed     = 0;
        int total      = 0;
        uint64_t nodes = 0;
        int64_t timeMs = 0;
        std::string tag;
        std::string timestamp;
      };
      std::vector<SuiteData> suiteResults;

      int totalPassed     = 0;
      int totalTests      = 0;
      uint64_t totalNodes = 0;
      int64_t totalTimeMs = 0;
      std::string latestTag;
      std::string latestTimestamp;

      for (const auto& [suiteName, engineResults] : data.suiteResults) {
        for (const auto& [engineId, result] : engineResults) {
          if (ResultStore::enginesMatchFlexibly(engineId, actualEngine)) {
            SuiteData sd;
            sd.name      = suiteName;
            sd.passed    = result.passed;
            sd.total     = result.totalTests;
            sd.nodes     = result.totalNodes;
            sd.timeMs    = result.totalTimeMs;
            sd.tag       = result.tag;
            sd.timestamp = result.timestamp;
            suiteResults.push_back(sd);

            totalPassed += result.passed;
            totalTests += result.totalTests;
            totalNodes += result.totalNodes;
            totalTimeMs += result.totalTimeMs;

            if (latestTimestamp.empty() || result.timestamp > latestTimestamp) {
              latestTimestamp = result.timestamp;
              latestTag       = result.tag;
            }
            break;
          }
        }
      }

      if (!suiteResults.empty()) {
        std::string tsDisplay = latestTimestamp;
        if (tsDisplay.length() >= 16) {
          tsDisplay = tsDisplay.substr(0, 10) + " " + tsDisplay.substr(11, 5);
        }

        report << "Test Suites (" << tsDisplay << ")";
        if (!latestTag.empty()) {
          report << " [" << latestTag << "]";
        }
        report << ":\n";

        std::ranges::sort(suiteResults, [](const SuiteData& a, const SuiteData& b) {
          return a.name < b.name;
        });

        for (const auto& sd : suiteResults) {
          const double pct = sd.total > 0 ? (sd.passed * 100.0 / sd.total) : 0.0;
          report << "  " << std::left << std::setw(20) << (sd.name + ":")
                 << std::right << std::setw(4) << sd.passed << "/"
                 << std::left << std::setw(6) << sd.total
                 << "(" << std::right << std::setw(7) << formatPercent(pct, 2) << ")\n";
        }

        report << "-------------------------------------------------------------------\n";

        const double totalPct = totalTests > 0 ? (totalPassed * 100.0 / totalTests) : 0.0;
        report << "  " << std::left << std::setw(20) << "TOTAL:"
               << std::right << std::setw(4) << totalPassed << "/"
               << std::left << std::setw(6) << totalTests
               << "(" << std::right << std::setw(7) << formatPercent(totalPct, 2) << ")\n";

        report << "  Total Nodes:      " << formatNumber(static_cast<int64_t>(totalNodes)) << "\n";

        const int64_t totalSecs = totalTimeMs / 1000;
        const int hours         = static_cast<int>(totalSecs / 3600);
        const int mins          = static_cast<int>((totalSecs % 3600) / 60);
        const int secs          = static_cast<int>(totalSecs % 60);
        report << "  Total Time:       " << hours << "h " << mins << "m " << secs << "s\n";

        report << "===================================================================\n";
      }
    }

    // Matches section (same for both modes)
    if (!matches.empty()) {
      report << "\nMatches:\n";
      for (const auto* match : matches) {
        // Determine if this engine is engine1 or engine2 using flexible matching
        const EngineId engine1Id = match->getEngine1Id();
        const EngineId engine2Id = match->getEngine2Id();

        const bool isEngine1 = ResultStore::enginesMatchFlexibly(engine1Id, actualEngine);

        // Skip if this match doesn't actually involve our engine (shouldn't happen but be safe)
        if (!isEngine1 && !ResultStore::enginesMatchFlexibly(engine2Id, actualEngine)) {
          continue;
        }

        const std::string opponent = isEngine1
                                       ? engine2Id.toDisplayString()
                                       : engine1Id.toDisplayString();

        const int wins   = isEngine1 ? match->engine1Wins : match->engine2Wins;
        const int losses = isEngine1 ? match->engine2Wins : match->engine1Wins;
        const double elo = isEngine1 ? match->eloDifference : -match->eloDifference;

        report << "  vs " << std::left << std::setw(16) << opponent
               << " (" << match->timeControl << ")";
        if (!match->tag.empty()) {
          report << " [" << match->tag << "]";
        }
        report << ":  " << wins << "-" << losses
               << " (W:" << wins << " D:" << match->draws << " L:" << losses << ")";

        report << "  ";
        if (elo >= 0) {
          report << "+" << std::fixed << std::setprecision(0) << elo;
        }
        else {
          report << std::fixed << std::setprecision(0) << elo;
        }
        report << " ELO\n";
      }
      report << "===================================================================\n";
    }

    // Benchmarks section (same for both modes)
    if (!benchmarks.empty()) {
      report << "\nBenchmarks:\n";

      std::vector<const BenchmarkResult*> sortedBenchmarks = benchmarks;
      std::ranges::sort(sortedBenchmarks, [](const auto* a, const auto* b) {
        return a->timestamp > b->timestamp;
      });

      for (const auto* bench : sortedBenchmarks) {
        std::string tsDisplay = bench->timestamp;
        if (tsDisplay.length() >= 10) {
          tsDisplay = tsDisplay.substr(0, 10);
        }

        const std::string npsStr = formatNumber(static_cast<int64_t>(bench->nps));

        report << "  [" << tsDisplay << "] "
               << std::right << std::setw(12) << npsStr << " NPS"
               << "  (d" << bench->depth << ", " << bench->hashSizeMB << "MB, " << bench->threads << "T)";
        if (!bench->tag.empty()) {
          report << "  [" << bench->tag << "]";
        }
        report << "\n";
      }
      report << "===================================================================\n";
    }

    // Check if we have any data at all
    if (report.str().find("Test Suites") == std::string::npos && report.str().find("Historical") == std::string::npos && matches.empty() && benchmarks.empty()) {
      report << "No results found for engine: " << actualEngine.toDisplayString() << "\n";
    }

    return report.str();
  }

} // namespace arena
