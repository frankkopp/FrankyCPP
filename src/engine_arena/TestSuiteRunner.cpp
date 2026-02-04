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
#include "UCIEngine.h"
#include "chesscore/MoveUtils.h"
#include "chesscore/Position.h"
#include "enginetest/EpdParser.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace arena {

TestSuiteRunner::TestSuiteRunner(const ArenaConfig& config)
    : arenaConfig(config) {
}

TestSuiteResult TestSuiteRunner::runTestSuite(const TestSuiteConfig& suiteConfig) const {
  std::cout << "\n==================================================================" << std::endl;
  std::cout << "Running Test Suite: " << suiteConfig.name << std::endl;
  std::cout << "==================================================================" << std::endl;
  std::cout << "EPD Path:          " << suiteConfig.epdPath << std::endl;
  std::cout << "Time per Move:     " << suiteConfig.timePerMove.count() << "ms" << std::endl;
  std::cout << "Max Depth:         " << suiteConfig.maxDepth << std::endl;
  std::cout << "Engine:            " << suiteConfig.enginePath << std::endl;
  std::cout << "Position Isolation: " << (suiteConfig.isolatePositions ? "enabled" : "disabled") << std::endl;
  std::cout << std::endl;

  // Validate EPD file exists
  if (!std::filesystem::exists(suiteConfig.epdPath)) {
    throw std::runtime_error("EPD file not found: " + suiteConfig.epdPath);
  }

  // Parse EPD file
  std::cout << "Parsing EPD file..." << std::endl;
  std::vector<EpdTest> epdTests = EpdParser::parseFile(suiteConfig.epdPath);
  if (epdTests.empty()) {
    throw std::runtime_error("No valid tests found in EPD file: " + suiteConfig.epdPath);
  }
  std::cout << "Loaded " << epdTests.size() << " test positions" << std::endl;

  // Initialize result structure
  TestSuiteResult result;
  result.version = arenaConfig.version;
  result.suiteName = suiteConfig.name;
  result.timestamp = getCurrentTimestamp();
  result.enginePath = suiteConfig.enginePath;
  result.totalTests = static_cast<int>(epdTests.size());
  result.passed = 0;
  result.failed = 0;
  result.skipped = 0;
  result.totalNodes = 0;
  result.totalTimeMs = 0;

  // Start external UCI engine
  std::cout << "\nStarting UCI engine..." << std::endl;
  if (!suiteConfig.commandLineArgs.empty()) {
    std::cout << "Command-line arguments: " << suiteConfig.commandLineArgs << std::endl;
  }

  UCIEngine engine(suiteConfig.enginePath, suiteConfig.commandLineArgs);
  result.engineName = engine.getEngineName();
  std::cout << "Engine: " << result.engineName << std::endl;

  // Configure engine options
  if (suiteConfig.debugMode) {
    std::cout << "Debug mode: ENABLED (printing all UCI communication)" << std::endl;
    engine.setDebugMode(true);
  }

  if (!suiteConfig.uciOptions.empty()) {
    std::cout << "Setting UCI options: " << suiteConfig.uciOptions << std::endl;
    engine.setUciOptions(suiteConfig.uciOptions);
  }

  std::cout << "\n------------------------------------------------------------------" << std::endl;

  // Execute tests
  int testNumber = 0;
  for (const auto& test : epdTests) {
    testNumber++;

    TestCaseDetail detail;
    detail.testId = test.getId().empty() ? ("Test" + std::to_string(testNumber)) : test.getId();
    detail.fen = test.getFen();

    // Format expected result based on test type
    if (test.getType() == TestType::DM) {
      detail.expected = "mate " + std::to_string(test.getMateDepth());
    } else if (test.getType() == TestType::BM) {
      detail.expected = "bm " + test.getTargetMoves().str();
    } else if (test.getType() == TestType::AM) {
      detail.expected = "am " + test.getTargetMoves().str();
    } else {
      detail.expected = "unknown";
    }

    std::cout << "[" << testNumber << "/" << epdTests.size() << "] "
              << detail.testId << ": " << std::flush;

    try {
      // Clear engine state between positions if isolation is enabled
      if (suiteConfig.isolatePositions) {
        engine.newGame();
      }

      // Set position
      if (!engine.setPosition(test.getFen())) {
        detail.actual = "ERROR: Failed to set position";
        detail.passed = false;
        detail.nodes = 0;
        detail.timeMs = 0;
        result.failed++;
        result.details.push_back(std::move(detail));
        std::cout << "FAILED (position setup error)" << std::endl;
        continue;
      }

      // Execute search
      UCISearchResult searchResult = engine.search(suiteConfig.timePerMove, suiteConfig.maxDepth);

      // Check if engine returned a move
      if (searchResult.bestMove.empty()) {
        detail.actual = "ERROR: No move returned";
        detail.passed = false;
        detail.nodes = searchResult.nodes;
        detail.timeMs = searchResult.time.count();
        result.failed++;
        result.totalNodes += searchResult.nodes;
        result.totalTimeMs += searchResult.time.count();
        result.details.push_back(std::move(detail));
        std::cout << "FAILED (no move)" << std::endl;
        continue;
      }

      // Store actual move and statistics
      detail.actual = searchResult.bestMove;
      detail.nodes = searchResult.nodes;
      detail.timeMs = searchResult.time.count();

      // Create Position for move comparison (needed for SAN conversion)
      Position position(test.getFen());

      // Evaluate test result based on test type
      bool testPassed = false;
      if (test.getType() == TestType::BM) {
        // Best Move: engine move must match one of the expected moves
        std::vector<std::string> expectedMoveStrings;
        for (const auto& move : test.getTargetMoves()) {
          expectedMoveStrings.push_back(move.str());
        }
        testPassed = matchesExpectedMove(searchResult.bestMove, expectedMoveStrings, position);
      } else if (test.getType() == TestType::AM) {
        // Avoid Move: engine move must NOT match any of the expected moves
        std::vector<std::string> expectedMoveStrings;
        for (const auto& move : test.getTargetMoves()) {
          expectedMoveStrings.push_back(move.str());
        }
        testPassed = !matchesExpectedMove(searchResult.bestMove, expectedMoveStrings, position);
      } else if (test.getType() == TestType::DM) {
        // Direct Mate: check if score indicates mate in expected depth
        if (searchResult.score != VALUE_NONE && searchResult.score.isCheckMate()) {
          // Calculate mate distance in moves from the score
          // Mate scores: VALUE_CHECKMATE (10000) - ply_to_mate
          // Example: VALUE_CHECKMATE - 4 = mate in 2 moves (4 ply)
          const int absScore = abs(static_cast<int>(searchResult.score));
          const int mateInPly = static_cast<int>(VALUE_CHECKMATE) - absScore;
          const int mateInMoves = (mateInPly + 1) / 2;  // Convert ply to moves
          testPassed = (mateInMoves <= test.getMateDepth());
        } else {
          testPassed = false;
        }
      }

      // Store result
      detail.passed = testPassed;
      if (testPassed) {
        result.passed++;
        std::cout << "PASS";
      } else {
        result.failed++;
        std::cout << "FAIL";
      }
      std::cout << " (" << searchResult.bestMove << ", "
                << searchResult.nodes << " nodes, "
                << searchResult.time.count() << "ms)" << std::endl;

      // Accumulate statistics
      result.totalNodes += searchResult.nodes;
      result.totalTimeMs += searchResult.time.count();

    } catch (const std::exception& e) {
      // Position-level error - log and continue
      detail.actual = std::string("ERROR: ") + e.what();
      detail.passed = false;
      detail.nodes = 0;
      detail.timeMs = 0;
      result.failed++;
      std::cout << "FAILED (exception: " << e.what() << ")" << std::endl;
    }

    result.details.push_back(std::move(detail));
  }

  // Print summary
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

std::vector<TestSuiteResult> TestSuiteRunner::runAllTestSuites() const {
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
  std::tm tm{};

#ifdef _WIN32
  gmtime_s(&tm, &time_t);
#else
  gmtime_r(&time_t, &tm);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}


} // namespace arena
