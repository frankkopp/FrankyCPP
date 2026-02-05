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

void ArenaRunner::compareVersions(const std::string& version1, const std::string& version2) {
  std::cout << "\n===================================================================" << std::endl;
  std::cout << "Version Comparison: " << version1 << " vs " << version2 << std::endl;
  std::cout << "===================================================================" << std::endl;

  // Load results for both versions
  std::cout << "Loading results for " << version1 << "..." << std::endl;
  auto suites1 = loadTestSuiteResults(version1);
  auto matches1 = loadMatchResults(version1);
  std::cout << "  Test suites: " << suites1.size() << std::endl;
  std::cout << "  Matches: " << matches1.size() << std::endl;

  std::cout << "Loading results for " << version2 << "..." << std::endl;
  auto suites2 = loadTestSuiteResults(version2);
  auto matches2 = loadMatchResults(version2);
  std::cout << "  Test suites: " << suites2.size() << std::endl;
  std::cout << "  Matches: " << matches2.size() << std::endl;

  // Generate comparison report
  std::string report = generateComparisonReport(version1, version2, suites1, suites2, matches1, matches2);

  // Print report to console
  std::cout << "\n" << report << std::endl;

  // Save report to file
  std::string reportPath = saveComparisonReport(version1, version2, report);
  std::cout << "\nComparison report saved to: " << reportPath << std::endl;

  std::cout << "\n===================================================================" << std::endl;
  std::cout << "Comparison Complete" << std::endl;
  std::cout << "===================================================================" << std::endl;
}

std::map<std::string, TestSuiteResult> ArenaRunner::loadTestSuiteResults(const std::string& engineId) {
  std::map<std::string, TestSuiteResult> results;

  const std::filesystem::path testsuitesDir = std::filesystem::path(arenaConfig.resultsDir) / "testsuites";

  if (!std::filesystem::exists(testsuitesDir)) {
    std::cout << "  Warning: No testsuites directory found" << std::endl;
    return results;
  }

  // Find all JSON files matching the engine identifier pattern
  // Format: {TestSuite}_{EngineId}_{Timestamp}.json (e.g., WAC_FrankyCPP-v0.5_20260205.json)
  for (const auto& entry : std::filesystem::directory_iterator(testsuitesDir)) {
    if (entry.path().extension() != ".json") continue;

    // Load JSON file
    try {
      std::ifstream file(entry.path());
      // NOLINTNEXTLINE - Clangd false positive about nlohmann::json template instantiation
      json data = json::parse(file);

      TestSuiteResult result;

      // Parse new JSON format
      result.arenaVersion = data["arenaVersion"];
      result.timestamp = data["timestamp"];
      result.testSuiteName = data["testSuite"]["name"];
      result.epdPath = data["testSuite"]["epdPath"];
      result.engineName = data["engine"]["name"];
      result.engineVersion = data["engine"]["version"];
      result.enginePath = data["engine"]["path"];

      // Build engine identifier for matching
      std::string resultEngineId = result.engineName + "-" + result.engineVersion;

      // Check if this result matches the requested engine identifier
      if (!engineId.empty() && resultEngineId.find(engineId) == std::string::npos &&
          engineId.find(resultEngineId) == std::string::npos) {
        continue;  // Skip non-matching engines
      }

      // Parse summary section
      const auto& summary = data["summary"];
      result.totalTests = summary["totalTests"];
      result.passed = summary["passed"];
      result.failed = summary["failed"];
      result.skipped = summary["skipped"];
      result.totalNodes = summary["totalNodes"];
      result.totalTimeMs = summary["totalTimeMs"];

      // Parse test case details
      if (data.contains("details")) {
        for (const auto& detail : data["details"]) {
          TestCaseDetail testDetail;
          testDetail.testId = detail["testId"];
          testDetail.fen = detail["fen"];
          testDetail.expected = detail["expected"];
          testDetail.actual = detail["actual"];
          testDetail.passed = detail["passed"];
          testDetail.nodes = detail["nodes"];
          testDetail.timeMs = detail["timeMs"];
          result.details.push_back(testDetail);
        }
      }

      // Use testSuiteName + engineId as key
      // If multiple results exist for same suite/engine, keep the latest
      std::string key = result.testSuiteName + "_" + resultEngineId;
      auto existing = results.find(key);
      if (existing == results.end() || result.timestamp > existing->second.timestamp) {
        results[key] = result;
      }

    } catch (const std::exception& e) {
      std::cerr << "  Warning: Failed to load " << entry.path().filename().string() << ": " << e.what() << std::endl;
    }
  }

  return results;
}

std::map<std::string, MatchResult> ArenaRunner::loadMatchResults(const std::string& version) {
  std::map<std::string, MatchResult> results;

  const std::filesystem::path matchesDir = std::filesystem::path(arenaConfig.resultsDir) / "matches";

  if (!std::filesystem::exists(matchesDir)) {
    std::cout << "  Warning: No matches directory found for " << version << std::endl;
    return results;
  }

  // Find all JSON files matching the version pattern
  for (const auto& entry : std::filesystem::directory_iterator(matchesDir)) {
    if (entry.path().extension() != ".json") continue;

    const std::string filename = entry.path().filename().string();

    // Check if filename starts with the version prefix
    if (filename.substr(0, version.length()) != version) continue;

    // Load JSON file
    try {
      std::ifstream file(entry.path());
      // NOLINTNEXTLINE - Clangd false positive about nlohmann::json template instantiation
      json data = json::parse(file);

      // Parse MatchResult from JSON
      MatchResult result;
      result.version = data["version"];
      result.matchName = data["matchName"];
      result.timestamp = data["timestamp"];
      result.engine1Name = data["engines"]["engine1"];
      result.engine2Name = data["engines"]["engine2"];
      result.engine1Wins = data["results"]["engine1Wins"];
      result.engine2Wins = data["results"]["engine2Wins"];
      result.draws = data["results"]["draws"];
      result.engine1Score = data["results"]["engine1Score"];
      result.engine2Score = data["results"]["engine2Score"];
      result.eloDifference = data["results"]["eloDifference"];
      result.pgnPath = data["pgnPath"];
      result.durationMs = data["durationMs"];

      results[result.matchName] = result;

    } catch (const std::exception& e) {
      std::cerr << "  Warning: Failed to load " << filename << ": " << e.what() << std::endl;
    }
  }

  return results;
}

std::string ArenaRunner::generateComparisonReport(
    const std::string& version1,
    const std::string& version2,
    const std::map<std::string, TestSuiteResult>& suites1,
    const std::map<std::string, TestSuiteResult>& suites2,
    const std::map<std::string, MatchResult>& matches1,
    const std::map<std::string, MatchResult>& matches2) {

  std::ostringstream report;

  report << "===================================================================" << std::endl;
  report << "Engine Version Comparison Report" << std::endl;
  report << "===================================================================" << std::endl;
  report << "Version 1: " << version1 << std::endl;
  report << "Version 2: " << version2 << std::endl;
  report << "Generated: " << getCurrentTimestamp() << std::endl;
  report << "===================================================================" << std::endl;

  // Test Suite Comparison
  if (!suites1.empty() || !suites2.empty()) {
    report << "\nTEST SUITE COMPARISON:" << std::endl;
    report << "-------------------------------------------------------------------" << std::endl;

    // Find all unique suite names
    std::set<std::string> allSuiteNames;
    for (const auto& [name, _] : suites1) allSuiteNames.insert(name);
    for (const auto& [name, _] : suites2) allSuiteNames.insert(name);

    for (const auto& suiteName : allSuiteNames) {
      auto it1 = suites1.find(suiteName);
      auto it2 = suites2.find(suiteName);

      report << "\n" << suiteName << ":" << std::endl;

      if (it1 != suites1.end() && it2 != suites2.end()) {
        // Both versions have this suite
        const auto& suite1 = it1->second;
        const auto& suite2 = it2->second;

        // Calculate pass rates
        const double passRate1 = suite1.totalTests > 0 ? (suite1.passed * 100.0 / suite1.totalTests) : 0.0;
        const double passRate2 = suite2.totalTests > 0 ? (suite2.passed * 100.0 / suite2.totalTests) : 0.0;

        report << "  " << version2 << " (" << suite2.engineName << "): "
               << suite2.passed << "/" << suite2.totalTests
               << " (" << std::fixed << std::setprecision(1) << passRate2 << "%)" << std::endl;
        report << "  " << version1 << " (" << suite1.engineName << "): "
               << suite1.passed << "/" << suite1.totalTests
               << " (" << std::fixed << std::setprecision(1) << passRate1 << "%)" << std::endl;

        const int deltaTests = suite1.passed - suite2.passed;
        const double deltaRate = passRate1 - passRate2;

        if (deltaTests > 0) {
          report << "  Improvement: +" << deltaTests << " positions (+"
                 << std::fixed << std::setprecision(1) << deltaRate << "%)" << std::endl;
        } else if (deltaTests < 0) {
          report << "  Regression: " << deltaTests << " positions ("
                 << std::fixed << std::setprecision(1) << deltaRate << "%)" << std::endl;
        } else {
          report << "  No change" << std::endl;
        }

        // Show timing improvement (average time per test)
        const double avgTime1 = suite1.totalTests > 0 ? (suite1.totalTimeMs / static_cast<double>(suite1.totalTests)) : 0.0;
        const double avgTime2 = suite2.totalTests > 0 ? (suite2.totalTimeMs / static_cast<double>(suite2.totalTests)) : 0.0;
        const double timeDelta = avgTime1 - avgTime2;

        if (std::abs(timeDelta) > 1.0) {
          report << "  Avg time: " << std::fixed << std::setprecision(1) << avgTime2 << "ms → " << avgTime1 << "ms ("
                 << (timeDelta > 0 ? "+" : "") << std::fixed << std::setprecision(1)
                 << timeDelta << "ms)" << std::endl;
        }

      } else if (it1 != suites1.end()) {
        // Only version 1 has this suite
        const auto& suite1 = it1->second;
        const double passRate1 = suite1.totalTests > 0 ? (suite1.passed * 100.0 / suite1.totalTests) : 0.0;
        report << "  " << version1 << ": " << suite1.passed << "/" << suite1.totalTests
               << " (" << std::fixed << std::setprecision(1) << passRate1 << "%)" << std::endl;
        report << "  " << version2 << ": NOT TESTED" << std::endl;

      } else {
        // Only version 2 has this suite
        const auto& suite2 = it2->second;
        const double passRate2 = suite2.totalTests > 0 ? (suite2.passed * 100.0 / suite2.totalTests) : 0.0;
        report << "  " << version1 << ": NOT TESTED" << std::endl;
        report << "  " << version2 << ": " << suite2.passed << "/" << suite2.totalTests
               << " (" << std::fixed << std::setprecision(1) << passRate2 << "%)" << std::endl;
      }
    }
  }

  // Match Comparison
  if (!matches1.empty() || !matches2.empty()) {
    report << "\n\nMATCH COMPARISON:" << std::endl;
    report << "-------------------------------------------------------------------" << std::endl;

    // Find all unique match names
    std::set<std::string> allMatchNames;
    for (const auto& [name, _] : matches1) allMatchNames.insert(name);
    for (const auto& [name, _] : matches2) allMatchNames.insert(name);

    for (const auto& matchName : allMatchNames) {
      auto it1 = matches1.find(matchName);

      if (it1 != matches1.end()) {
        const auto& match = it1->second;

        report << "\n" << matchName << ":" << std::endl;
        report << "  " << match.engine1Name << ": " << match.engine1Wins << " wins, "
               << match.draws << " draws, " << match.engine2Wins << " losses" << std::endl;
        report << "  " << match.engine2Name << ": " << match.engine2Wins << " wins, "
               << match.draws << " draws, " << match.engine1Wins << " losses" << std::endl;
        report << "  Score: " << std::fixed << std::setprecision(1)
               << match.engine1Score << " - " << match.engine2Score << std::endl;
        report << "  ELO Difference: " << (match.eloDifference > 0 ? "+" : "")
               << std::fixed << std::setprecision(1) << match.eloDifference << " ELO" << std::endl;
        report << "  Duration: " << std::fixed << std::setprecision(1)
               << match.durationMs / 1000.0 << " seconds" << std::endl;
      }
    }
  }

  // Overall Summary
  report << "\n\nSUMMARY:" << std::endl;
  report << "-------------------------------------------------------------------" << std::endl;

  // Calculate average ELO from matches
  if (!matches1.empty()) {
    double totalElo = 0.0;
    int matchCount = 0;
    for (const auto& [name, match] : matches1) {
      totalElo += match.eloDifference;
      matchCount++;
    }
    const double avgElo = totalElo / matchCount;

    if (avgElo > 5.0) {
      report << version1 << " is approximately " << std::fixed << std::setprecision(0)
             << "+" << avgElo << " ELO stronger than " << version2 << std::endl;
    } else if (avgElo < -5.0) {
      report << version1 << " is approximately " << std::fixed << std::setprecision(0)
             << avgElo << " ELO weaker than " << version2 << std::endl;
    } else {
      report << version1 << " and " << version2 << " are approximately equal in strength" << std::endl;
    }
  }

  // Calculate total test suite improvements
  if (!suites1.empty() && !suites2.empty()) {
    int totalImprovement = 0;
    int totalTests = 0;

    for (const auto& [name, suite1] : suites1) {
      auto it2 = suites2.find(name);
      if (it2 != suites2.end()) {
        const auto& suite2 = it2->second;
        totalImprovement += (suite1.passed - suite2.passed);
        totalTests += suite1.totalTests;
      }
    }

    if (totalImprovement > 0) {
      report << "Test suite improvement: +" << totalImprovement << " positions solved" << std::endl;
    } else if (totalImprovement < 0) {
      report << "Test suite regression: " << totalImprovement << " positions lost" << std::endl;
    } else {
      report << "Test suite results: no change" << std::endl;
    }
  }

  report << "===================================================================" << std::endl;

  return report.str();
}

std::string ArenaRunner::saveComparisonReport(
    const std::string& version1,
    const std::string& version2,
    const std::string& report) {

  // Create comparisons directory
  const std::filesystem::path comparisonsDir = std::filesystem::path(arenaConfig.resultsDir) / "comparisons";
  std::filesystem::create_directories(comparisonsDir);

  // Generate filename: v1.1_vs_v1.0_20260201_153000.txt
  std::ostringstream filename;
  filename << version1 << "_vs_" << version2 << "_" << getCurrentTimestamp() << ".txt";

  const std::filesystem::path reportPath = comparisonsDir / filename.str();

  // Write report to file
  std::ofstream file(reportPath);
  if (!file) {
    throw std::runtime_error("Failed to create comparison report file: " + reportPath.string());
  }

  file << report;
  file.close();

  return reportPath.string();
}

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

} // namespace arena
