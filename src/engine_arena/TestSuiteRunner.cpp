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

#include "TestSuiteRunner.h"
#include "enginetest/TestSuite.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace arena {

TestSuiteRunner::TestSuiteRunner(const ArenaConfig& config)
    : arenaConfig(config) {
}

TestSuiteResult TestSuiteRunner::runTestSuite(const TestSuiteConfig& suiteConfig) {
  std::cout << "\n==================================================================" << std::endl;
  std::cout << "Running Test Suite: " << suiteConfig.name << std::endl;
  std::cout << "==================================================================" << std::endl;
  std::cout << "EPD Path:       " << suiteConfig.epdPath << std::endl;
  std::cout << "Time per Move:  " << suiteConfig.timePerMove.count() << "ms" << std::endl;
  std::cout << "Max Depth:      " << suiteConfig.maxDepth << std::endl;
  std::cout << std::endl;

  // Check if EPD file exists
  if (!std::filesystem::exists(suiteConfig.epdPath)) {
    throw std::runtime_error("EPD file not found: " + suiteConfig.epdPath);
  }

  // Create and run TestSuite
  TestSuite suite(suiteConfig.timePerMove, suiteConfig.maxDepth, suiteConfig.epdPath);
  suite.runTestSuite();

  // Get results
  const auto& internalResult = suite.getLastResult();
  const auto& testCases = suite.getTestCases();

  // Convert to arena result format with metadata
  TestSuiteResult result = convertToArenaResult(suiteConfig, internalResult, testCases);

  std::cout << "\n------------------------------------------------------------------" << std::endl;
  std::cout << "Test Suite Complete: " << suiteConfig.name << std::endl;
  std::cout << "  Total Tests:  " << result.totalTests << std::endl;
  std::cout << "  Passed:       " << result.passed << " ("
            << (result.totalTests > 0 ? (result.passed * 100.0 / result.totalTests) : 0.0)
            << "%)" << std::endl;
  std::cout << "  Failed:       " << result.failed << std::endl;
  std::cout << "  Skipped:      " << result.skipped << std::endl;
  std::cout << "  Total Nodes:  " << result.totalNodes << std::endl;
  std::cout << "  Total Time:   " << result.totalTimeMs << "ms" << std::endl;
  std::cout << "==================================================================" << std::endl;

  return result;
}

std::vector<TestSuiteResult> TestSuiteRunner::runAllTestSuites() {
  std::vector<TestSuiteResult> results;
  results.reserve(arenaConfig.testSuites.size());

  std::cout << "\n===================================================================" << std::endl;
  std::cout << "Running All Test Suites" << std::endl;
  std::cout << "===================================================================" << std::endl;
  std::cout << "Engine Version: " << arenaConfig.version << std::endl;
  std::cout << "Number of Test Suites: " << arenaConfig.testSuites.size() << std::endl;
  std::cout << "===================================================================" << std::endl;

  int suiteNumber = 0;
  for (const auto& suiteConfig : arenaConfig.testSuites) {
    suiteNumber++;
    std::cout << "\n[" << suiteNumber << "/" << arenaConfig.testSuites.size() << "] ";

    try {
      TestSuiteResult result = runTestSuite(suiteConfig);
      results.push_back(std::move(result));
    } catch (const std::exception& e) {
      std::cerr << "\nERROR: Failed to run test suite '" << suiteConfig.name << "': "
                << e.what() << std::endl;
      throw; // Re-throw to allow caller to handle
    }
  }

  // Print summary
  std::cout << "\n===================================================================" << std::endl;
  std::cout << "All Test Suites Complete" << std::endl;
  std::cout << "===================================================================" << std::endl;

  int totalTests = 0;
  int totalPassed = 0;
  int totalFailed = 0;
  uint64_t totalNodes = 0;
  int64_t totalTime = 0;

  for (const auto& result : results) {
    totalTests += result.totalTests;
    totalPassed += result.passed;
    totalFailed += result.failed;
    totalNodes += result.totalNodes;
    totalTime += result.totalTimeMs;

    std::cout << "  " << result.suiteName << ": "
              << result.passed << "/" << result.totalTests << " passed ("
              << (result.totalTests > 0 ? (result.passed * 100.0 / result.totalTests) : 0.0)
              << "%)" << std::endl;
  }

  std::cout << "-------------------------------------------------------------------" << std::endl;
  std::cout << "  TOTAL: " << totalPassed << "/" << totalTests << " passed ("
            << (totalTests > 0 ? (totalPassed * 100.0 / totalTests) : 0.0)
            << "%)" << std::endl;
  std::cout << "  Total Nodes: " << totalNodes << std::endl;
  std::cout << "  Total Time:  " << totalTime << "ms" << std::endl;
  std::cout << "===================================================================" << std::endl;

  return results;
}

std::string TestSuiteRunner::getCurrentTimestamp() {
  const auto now = system_clock::now();
  auto time_t = system_clock::to_time_t(now);
  std::tm tm;

#ifdef _WIN32
  gmtime_s(&tm, &time_t);
#else
  gmtime_r(&time_t, &tm);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

TestSuiteResult TestSuiteRunner::convertToArenaResult(
    const TestSuiteConfig& suiteConfig,
    const ::TestSuiteResult& internalResult,
    const std::vector<::Test>& testCases) const {

  TestSuiteResult result;

  // Add metadata
  result.version = arenaConfig.version;
  result.suiteName = suiteConfig.name;
  result.timestamp = getCurrentTimestamp();

  // Copy aggregate statistics
  result.totalTests = internalResult.counter;
  result.passed = internalResult.successCounter;
  result.failed = internalResult.failedCounter;
  result.skipped = internalResult.skippedCounter;
  result.totalNodes = internalResult.nodes;
  result.totalTimeMs = std::chrono::duration_cast<milliseconds>(internalResult.time).count();

  // Convert per-test details
  result.details.reserve(testCases.size());
  for (const auto& test : testCases) {
    TestCaseDetail detail;
    detail.testId = test.id.empty() ? "N/A" : test.id;
    detail.fen = test.fen;

    // Format expected result based on test type
    if (test.type == DM) {
      detail.expected = "mate " + std::to_string(test.mateDepth);
    } else if (test.type == BM) {
      detail.expected = "bm " + test.targetMoves.str();
    } else if (test.type == AM) {
      detail.expected = "am " + test.targetMoves.str();
    } else {
      detail.expected = "unknown";
    }

    // Actual move and result
    detail.actual = test.actualMove.str();
    detail.passed = (test.result == SUCCESS);
    detail.nodes = test.nodes;
    detail.timeMs = std::chrono::duration_cast<milliseconds>(test.time).count();

    result.details.push_back(std::move(detail));
  }

  return result;
}

} // namespace arena
