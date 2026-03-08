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
#include "common/ThreadPool.h"
#include "enginetest/EpdParser.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>

using namespace common;
using namespace chess;
using namespace enginetest;

namespace arena {

  namespace {
    /// Parse engine name string into separate name and version components
    /// Input: "FrankyCPP v0.5" or "FrankyGo 1.2.3" or "Stockfish 16"
    /// Returns: pair<name, version> e.g., {"FrankyCPP", "v0.5"}
    std::pair<std::string, std::string> parseEngineName(const std::string& fullName) {
      // Try to match pattern: "Name vX.Y" or "Name X.Y" or "Name vX.Y.Z"
      const std::regex versionPattern(R"(^(.+?)\s+(v?\d+(?:\.\d+)*(?:[.-]?\w+)?)$)");
      std::smatch match;

      if (std::regex_match(fullName, match, versionPattern)) {
        return {match[1].str(), match[2].str()};
      }

      // No version found, return full name as name and empty version
      return {fullName, ""};
    }

    /// Extract clean test suite name from config name
    /// Input: "franky_tests_v0.5" or "WAC" or "mate_test_v1.1"
    /// Returns: "franky_tests" or "WAC" or "mate_test"
    /// Note: This is for backward compatibility with old-style config names
    std::string extractTestSuiteName(const std::string& configName, const std::string& epdPath) {
      // Prefer deriving from EPD filename if available
      if (!epdPath.empty()) {
        const std::filesystem::path path(epdPath);
        std::string stem = path.stem().string();
        // Remove common suffixes like "_test", "_suite"
        // But keep the meaningful name
        return stem;
      }

      // Fall back to parsing config name - remove engine version suffixes
      const std::regex suffixPattern(R"(^(.+?)(?:_v\d+(?:\.\d+)*|_FrankyGo|_Stockfish)$)");
      std::smatch match;

      if (std::regex_match(configName, match, suffixPattern)) {
        return match[1].str();
      }

      return configName;
    }
  }// anonymous namespace

  TestSuiteRunner::TestSuiteRunner(const ArenaConfig& config)
      : arenaConfig(config) {
  }

  TestSuiteResult TestSuiteRunner::runTestSuite(const TestSuiteConfig& suiteConfig) const {
    // Auto-select based on parallelWorkers config
    if (suiteConfig.parallelWorkers <= 1) {
      return runTestSuiteSequential(suiteConfig);
    }
    return runTestSuiteParallel(suiteConfig, suiteConfig.parallelWorkers);
  }

  std::vector<TestSuiteResult> TestSuiteRunner::runAllTestSuites(
    const SuiteResultCallback& onSuiteComplete) const {
    std::vector<TestSuiteResult> results;

    // Expand testSuiteRuns into individual TestSuiteConfigs
    const auto testSuites = arenaConfig.expandTestSuiteRuns();
    results.reserve(testSuites.size());

    std::cout << "\n===================================================================" << std::endl;
    std::cout << "Running All Test Suites" << std::endl;
    std::cout << "===================================================================" << std::endl;
    std::cout << "Engine Version: " << arenaConfig.version << std::endl;
    std::cout << "Number of Test Suites: " << testSuites.size() << std::endl;
    std::cout << "===================================================================" << std::endl;

    int suiteNumber = 0;
    for (const auto& suiteConfig : testSuites) {
      suiteNumber++;
      std::cout << "\n[" << suiteNumber << "/" << testSuites.size() << "] ";

      try {
        TestSuiteResult result = runTestSuite(suiteConfig);

        // Invoke callback immediately after suite completes (for crash resilience)
        if (onSuiteComplete) {
          onSuiteComplete(result);
        }

        results.push_back(std::move(result));
      } catch (const std::exception& e) {
        std::cerr << "\nERROR: Failed to run test suite '" << suiteConfig.name << "': "
                  << e.what() << std::endl;
        throw;// Re-throw to allow caller to handle
      }
    }

    // Print summary
    std::cout << "\n===================================================================" << std::endl;
    std::cout << "All Test Suites Complete" << std::endl;
    std::cout << "===================================================================" << std::endl;

    int totalTests      = 0;
    int totalPassed     = 0;
    int totalFailed     = 0;
    uint64_t totalNodes = 0;
    int64_t totalTime   = 0;

    for (const auto& result : results) {
      totalTests += result.totalTests;
      totalPassed += result.passed;
      totalFailed += result.failed;
      totalNodes += result.totalNodes;
      totalTime += result.totalTimeMs;

      // Include engine name and version in summary
      std::string engineInfo = result.engineName;
      if (!result.engineVersion.empty()) {
        engineInfo += " " + result.engineVersion;
      }

      std::cout << "  " << result.testSuiteName << " (" << engineInfo << "): "
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

  TestSuiteResult TestSuiteRunner::runTestSuiteSequential(const TestSuiteConfig& suiteConfig) const {
    std::cout << "\n==================================================================" << std::endl;
    std::cout << "Running Test Suite: " << suiteConfig.name << " (sequential)" << std::endl;
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
    result.arenaVersion = arenaConfig.version;
    result.timestamp    = getCurrentTimestamp();
    result.epdPath      = suiteConfig.epdPath;
    result.enginePath   = suiteConfig.enginePath;
    result.totalTests   = static_cast<int>(epdTests.size());
    result.passed       = 0;
    result.failed       = 0;
    result.skipped      = 0;
    result.totalNodes   = 0;
    result.totalTimeMs  = 0;

    // Start external UCI engine
    std::cout << "\nStarting UCI engine..." << std::endl;
    if (!suiteConfig.commandLineArgs.empty()) {
      std::cout << "Command-line arguments: " << suiteConfig.commandLineArgs << std::endl;
    }
    if (suiteConfig.debugMode) {
      std::cout << "Debug mode: ENABLED (printing all UCI communication)" << std::endl;
    }
    if (!suiteConfig.uciOptions.empty()) {
      std::cout << "UCI options: " << suiteConfig.uciOptions << std::endl;
    }

    // Pass uciOptions to constructor so they are sent BEFORE isready (before engine initializes)
    UCIEngine engine(suiteConfig.enginePath, suiteConfig.commandLineArgs,
                     suiteConfig.debugMode, suiteConfig.uciOptions);

    // Use configured engine version if provided, otherwise try to parse from UCI name
    result.engineName = engine.getEngineName();
    if (!suiteConfig.engineVersion.empty()) {
      result.engineVersion = suiteConfig.engineVersion;
    }
    else {
      // Fallback: try to parse version from UCI name (not reliable)
      auto [parsedName, parsedVersion] = parseEngineName(engine.getEngineName());
      result.engineName                = parsedName;
      result.engineVersion             = parsedVersion;
    }

    // Extract clean test suite name
    result.testSuiteName = extractTestSuiteName(suiteConfig.name, suiteConfig.epdPath);

    // Propagate tag from config
    result.tag = suiteConfig.tag;

    std::cout << "Engine: " << result.engineName;
    if (!result.engineVersion.empty()) {
      std::cout << " " << result.engineVersion;
    }
    std::cout << std::endl;
    std::cout << "Test Suite: " << result.testSuiteName << std::endl;
    if (!result.tag.empty()) {
      std::cout << "Tag: " << result.tag << std::endl;
    }


    std::cout << "\n------------------------------------------------------------------" << std::endl;

    // Execute tests
    int testNumber = 0;
    for (const auto& test : epdTests) {
      testNumber++;

      TestCaseDetail detail;
      detail.testId = test.getId().empty() ? ("Test" + std::to_string(testNumber)) : test.getId();
      detail.fen    = test.getFen();

      // Format expected result based on test type
      if (test.getType() == TestType::DM) {
        detail.expected = "mate " + std::to_string(test.getMateDepth());
      }
      else if (test.getType() == TestType::BM) {
        detail.expected = "bm " + test.getTargetMoves().str();
      }
      else if (test.getType() == TestType::AM) {
        detail.expected = "am " + test.getTargetMoves().str();
      }
      else {
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
          detail.nodes  = 0;
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
          detail.nodes  = searchResult.nodes;
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
        detail.nodes  = searchResult.nodes;
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
        }
        else if (test.getType() == TestType::AM) {
          // Avoid Move: engine move must NOT match any of the expected moves
          std::vector<std::string> expectedMoveStrings;
          for (const auto& move : test.getTargetMoves()) {
            expectedMoveStrings.push_back(move.str());
          }
          testPassed = !matchesExpectedMove(searchResult.bestMove, expectedMoveStrings, position);
        }
        else if (test.getType() == TestType::DM) {
          // Direct Mate: check if score indicates mate in expected depth
          if (searchResult.score != VALUE_NONE && searchResult.score.isCheckMate()) {
            // Calculate mate distance in moves from the score
            // Mate scores: VALUE_CHECKMATE (10000) - ply_to_mate
            // Example: VALUE_CHECKMATE - 4 = mate in 2 moves (4 ply)
            const int absScore    = abs(static_cast<int>(searchResult.score));
            const int mateInPly   = static_cast<int>(VALUE_CHECKMATE) - absScore;
            const int mateInMoves = (mateInPly + 1) / 2;// Convert ply to moves
            testPassed            = (mateInMoves <= test.getMateDepth());
          }
          else {
            testPassed = false;
          }
        }

        // Store result
        detail.passed = testPassed;
        if (testPassed) {
          result.passed++;
          std::cout << "PASS";
        }
        else {
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
        detail.nodes  = 0;
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

  TestSuiteResult TestSuiteRunner::runTestSuiteParallel(
    const TestSuiteConfig& suiteConfig,
    int numWorkers) const {

    std::cout << "\n==================================================================" << std::endl;
    std::cout << "Running Test Suite: " << suiteConfig.name << " (parallel: " << numWorkers << " workers)" << std::endl;
    std::cout << "==================================================================" << std::endl;
    std::cout << "EPD Path:          " << suiteConfig.epdPath << std::endl;
    std::cout << "Time per Move:     " << suiteConfig.timePerMove.count() << "ms" << std::endl;
    std::cout << "Max Depth:         " << suiteConfig.maxDepth << std::endl;
    std::cout << "Engine:            " << suiteConfig.enginePath << std::endl;
    std::cout << "Position Isolation: " << (suiteConfig.isolatePositions ? "enabled" : "disabled") << std::endl;

    // Warn if debugMode is enabled - it's ignored in parallel mode because
    // interleaved UCI output from multiple engines would be unreadable
    if (suiteConfig.debugMode) {
      std::cout << "WARNING: debugMode is ignored in parallel execution (use parallelWorkers=1 for UCI logging)" << std::endl;
    }
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
    const int totalPositions = static_cast<int>(epdTests.size());
    std::cout << "Loaded " << totalPositions << " test positions" << std::endl;

    // Engine name (captured from first engine)
    std::string engineName;

    // Progress tracking
    std::atomic completedCount{0};
    std::atomic passedCount{0};
    std::atomic failedCount{0};
    std::mutex coutMutex;

    auto startTime = steady_clock::now();

    // Pre-create engines for each worker - avoid thread_local which causes
    // shutdown issues on Windows (thread_local destructors can deadlock when
    // joining threads during thread exit)
    std::cout << "\nCreating " << numWorkers << " UCI engine instances..." << std::endl;
    if (!suiteConfig.uciOptions.empty()) {
      std::cout << "UCI options: " << suiteConfig.uciOptions << std::endl;
    }
    std::vector<std::unique_ptr<UCIEngine>> engines;
    engines.reserve(numWorkers);
    for (int w = 0; w < numWorkers; ++w) {
      // Pass uciOptions to constructor so they are sent BEFORE isready (before engine initializes)
      // debugMode is false for parallel execution (output would be interleaved)
      auto engine = std::make_unique<UCIEngine>(suiteConfig.enginePath, suiteConfig.commandLineArgs,
                                                false, suiteConfig.uciOptions);

      // Capture engine name from first engine
      if (w == 0) {
        engineName = engine->getEngineName();
      }

      engines.push_back(std::move(engine));
    }
    std::cout << "Engine: " << engineName << std::endl;

    // Track which engine each worker thread should use
    std::atomic<int> nextEngineIndex{0};
    std::mutex engineMapMutex;

    // Create thread pool and enqueue all positions
    std::cout << "\nStarting ThreadPool with " << numWorkers << " workers..." << std::endl;
    ThreadPool pool(numWorkers);
    std::vector<std::future<TestCaseDetail>> futures;
    futures.reserve(totalPositions);

    for (int i = 0; i < totalPositions; ++i) {
      futures.push_back(pool.enqueue([this, &suiteConfig, &epdTests, &engines,
                                      &nextEngineIndex, &engineMapMutex,
                                      &completedCount, &passedCount,
                                      &failedCount, &coutMutex, i, totalPositions]() -> TestCaseDetail {
        // Get thread ID and map to an engine (thread_local for index only - not the engine itself)
        thread_local int myEngineIndex = -1;
        if (myEngineIndex == -1) {
          std::lock_guard lock(engineMapMutex);
          myEngineIndex = nextEngineIndex++;
        }

        UCIEngine& engine = *engines[myEngineIndex];

        // Access test by reference from the vector (no copy needed)
        const EpdTest& test = epdTests[i];

        // Run the test
        TestCaseDetail detail = runSinglePosition(
          engine, test, suiteConfig, i + 1);

        // Update counters
        if (detail.passed) {
          ++passedCount;
        }
        else {
          ++failedCount;
        }

        // Progress update
        const int completed = ++completedCount;
        if (completed % 10 == 0 || completed == totalPositions) {
          std::lock_guard lock(coutMutex);
          std::cout << "\r  Progress: " << completed << "/" << totalPositions
                    << " (" << (completed * 100 / totalPositions) << "%)"
                    << " [" << passedCount << " passed, " << failedCount << " failed]"
                    << std::flush;
        }

        return detail;
      }));
    }

    // Collect results in order
    std::vector<TestCaseDetail> results;
    results.reserve(totalPositions);
    uint64_t totalNodes = 0;
    int64_t totalTimeMs = 0;

    for (auto& future : futures) {
      TestCaseDetail detail = future.get();
      totalNodes += detail.nodes;
      totalTimeMs += detail.timeMs;
      results.push_back(std::move(detail));
    }

    // IMPORTANT: Stop the thread pool BEFORE destroying engines
    // This ensures worker threads are joined before engines are destroyed
    pool.stop();

    // Now explicitly destroy engines (in the main thread, not during thread exit)
    std::cout << "\nShutting down UCI engines..." << std::endl;
    engines.clear();

    auto endTime    = steady_clock::now();
    auto wallTimeMs = std::chrono::duration_cast<milliseconds>(
                        endTime - startTime)
                        .count();

    std::cout << std::endl;// New line after progress

    // Build final result
    TestSuiteResult result;
    result.arenaVersion  = arenaConfig.version;
    result.timestamp     = getCurrentTimestamp();
    result.testSuiteName = extractTestSuiteName(suiteConfig.name, suiteConfig.epdPath);
    result.epdPath       = suiteConfig.epdPath;
    result.enginePath    = suiteConfig.enginePath;

    // Use configured engine version if provided, otherwise try to parse from UCI name
    if (!suiteConfig.engineVersion.empty()) {
      result.engineName    = engineName;// Use full UCI name
      result.engineVersion = suiteConfig.engineVersion;
    }
    else {
      // Fallback: try to parse version from UCI name (not reliable)
      auto [parsedEngineName, parsedEngineVersion] = parseEngineName(engineName);
      result.engineName                            = parsedEngineName;
      result.engineVersion                         = parsedEngineVersion;
    }

    // Propagate tag from config
    result.tag = suiteConfig.tag;    result.totalTests  = totalPositions;
    result.passed      = passedCount;
    result.failed      = failedCount;
    result.skipped     = 0;
    result.totalNodes  = totalNodes;
    result.totalTimeMs = totalTimeMs;
    result.details     = std::move(results);

    // Print summary
    std::cout << "\n------------------------------------------------------------------" << std::endl;
    std::cout << "Test Suite Complete: " << result.testSuiteName << " (parallel)" << std::endl;
    std::cout << "  Engine:       " << result.engineName;
    if (!result.engineVersion.empty()) {
      std::cout << " " << result.engineVersion;
    }
    std::cout << std::endl;
    std::cout << "  Total Tests:  " << result.totalTests << std::endl;
    std::cout << "  Passed:       " << result.passed << " ("
              << (result.totalTests > 0 ? (result.passed * 100.0 / result.totalTests) : 0.0)
              << "%)" << std::endl;
    std::cout << "  Failed:       " << result.failed << std::endl;
    std::cout << "  Skipped:      " << result.skipped << std::endl;
    std::cout << "  Total Nodes:  " << result.totalNodes << std::endl;
    std::cout << "  Engine Time:  " << result.totalTimeMs << "ms (sum of all positions)" << std::endl;
    std::cout << "  Wall Time:    " << wallTimeMs << "ms (actual elapsed)" << std::endl;
    std::cout << "  Speedup:      " << (result.totalTimeMs > 0 ? static_cast<double>(result.totalTimeMs) / wallTimeMs : 0.0)
              << "x" << std::endl;
    std::cout << "==================================================================" << std::endl;

    return result;
  }

  TestCaseDetail TestSuiteRunner::runSinglePosition(
    UCIEngine& engine,
    const EpdTest& test,
    const TestSuiteConfig& config,
    int testNumber) {

    TestCaseDetail detail;
    detail.testId = test.getId().empty() ? ("Test" + std::to_string(testNumber)) : test.getId();
    detail.fen    = test.getFen();

    // Format expected result based on test type
    if (test.getType() == TestType::DM) {
      detail.expected = "mate " + std::to_string(test.getMateDepth());
    }
    else if (test.getType() == TestType::BM) {
      detail.expected = "bm " + test.getTargetMoves().str();
    }
    else if (test.getType() == TestType::AM) {
      detail.expected = "am " + test.getTargetMoves().str();
    }
    else {
      detail.expected = "unknown";
    }

    try {
      // Clear engine state between positions if isolation is enabled
      if (config.isolatePositions) {
        engine.newGame();
      }

      // Set position
      if (!engine.setPosition(test.getFen())) {
        detail.actual = "ERROR: Failed to set position";
        detail.passed = false;
        detail.nodes  = 0;
        detail.timeMs = 0;
        return detail;
      }

      // Execute search
      UCISearchResult searchResult = engine.search(config.timePerMove, config.maxDepth);

      // Check if engine returned a move
      if (searchResult.bestMove.empty()) {
        detail.actual = "ERROR: No move returned";
        detail.passed = false;
        detail.nodes  = searchResult.nodes;
        detail.timeMs = searchResult.time.count();
        return detail;
      }

      // Store actual move and statistics
      detail.actual = searchResult.bestMove;
      detail.nodes  = searchResult.nodes;
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
      }
      else if (test.getType() == TestType::AM) {
        // Avoid Move: engine move must NOT match any of the expected moves
        std::vector<std::string> expectedMoveStrings;
        for (const auto& move : test.getTargetMoves()) {
          expectedMoveStrings.push_back(move.str());
        }
        testPassed = !matchesExpectedMove(searchResult.bestMove, expectedMoveStrings, position);
      }
      else if (test.getType() == TestType::DM) {
        // Direct Mate: check if score indicates mate in expected depth
        if (searchResult.score != VALUE_NONE && searchResult.score.isCheckMate()) {
          const int absScore    = abs(static_cast<int>(searchResult.score));
          const int mateInPly   = static_cast<int>(VALUE_CHECKMATE) - absScore;
          const int mateInMoves = (mateInPly + 1) / 2;
          testPassed            = (mateInMoves <= test.getMateDepth());
        }
        else {
          testPassed = false;
        }
      }

      detail.passed = testPassed;

    } catch (const std::exception& e) {
      detail.actual = std::string("ERROR: ") + e.what();
      detail.passed = false;
      detail.nodes  = 0;
      detail.timeMs = 0;
    }

    return detail;
  }

  std::string TestSuiteRunner::getCurrentTimestamp() {
    const auto now = system_clock::now();
    auto time_t    = system_clock::to_time_t(now);
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


}// namespace arena
