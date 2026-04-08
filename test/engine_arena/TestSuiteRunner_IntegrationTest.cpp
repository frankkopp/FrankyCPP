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
// TestSuiteRunner_IntegrationTest.cpp - Integration Tests for TestSuiteRunner
//=============================================================================
//
// Phase 7: Testing & Error Handling
//
// Comprehensive integration tests for external UCI engine test suite execution.
// Tests cover:
//   - Full EPD suite execution with external engine
//   - Multiple suites in sequence
//   - Position isolation modes
//   - Error handling scenarios
//   - Result structure validation
//
// Note: These tests require a built FrankyCPP engine executable.
// Tests will be skipped if the engine is not found.
//
//=============================================================================

#include "Test_Utils.h"
#include "engine_arena/ArenaConfig.h"
#include "engine_arena/ArenaResults.h"
#include "engine_arena/TestSuiteRunner.h"
#include "init.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace arena;
using namespace chess;

namespace {

  // Helper: Get path to the built engine executable for testing
  std::string getTestEnginePath() {
    // Try cmake-build-win-release first (typical Windows CLion build)
    std::string path = "cmake-build-win-release/src/FrankyCPP_v1.1.exe";
    if (std::filesystem::exists(path)) {
      return path;
    }

    // Try relative path from test executable location
    path = "../src/FrankyCPP_v1.1.exe";
    if (std::filesystem::exists(path)) {
      return path;
    }

    // Try current directory
    path = "FrankyCPP_v1.1.exe";
    if (std::filesystem::exists(path)) {
      return path;
    }

    return ""; // Not found
  }

  // Helper: Create a minimal test EPD file for testing
  std::string createTestEpdFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (!file) {
      throw std::runtime_error("Failed to create test EPD file: " + filename);
    }
    file << content;
    file.close();
    return filename;
  }

  // Helper: Cleanup test files
  void cleanupTestFile(const std::string& filename) {
    if (std::filesystem::exists(filename)) {
      std::filesystem::remove(filename);
    }
  }

} // anonymous namespace

class TestSuiteRunnerIntegrationTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }

protected:
  void SetUp() override {
    // Check if test engine is available
    testEnginePath = getTestEnginePath();
    if (testEnginePath.empty()) {
      GTEST_SKIP() << "Test engine not found. Skipping integration tests.\n"
                   << "Build the project first: cmake --build cmake-build-win-release";
    }

    std::cout << "Using test engine: " << testEnginePath << std::endl;
  }

  void TearDown() override {
    // Cleanup any test EPD files created
    for (const auto& file : testFilesToCleanup) {
      cleanupTestFile(file);
    }
    testFilesToCleanup.clear();
  }

  std::string testEnginePath;
  std::vector<std::string> testFilesToCleanup;
};

//=============================================================================
// Test 1: Full EPD Suite Against External Engine
//=============================================================================

TEST_F(TestSuiteRunnerIntegrationTest, FullSuite_StartingPosition) {
  // Create a minimal test EPD with starting position
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string testEpdPath = "test_starting_position.epd";
  testFilesToCleanup.push_back(testEpdPath);

  createTestEpdFile(testEpdPath,
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4 d4 Nf3; id \"Starting Position\";\n");

  // Create config
  ArenaConfig config;
  config.version    = "v1.1";
  config.resultsDir = "./results";

  TestSuiteConfig suiteConfig;
  suiteConfig.name             = "test_starting_position";
  suiteConfig.epdPath          = testEpdPath;
  suiteConfig.enginePath       = testEnginePath;
  suiteConfig.timePerMove      = milliseconds{100}; // Fast for testing
  suiteConfig.maxDepth         = static_cast<Depth>(5);
  suiteConfig.isolatePositions = true;
  suiteConfig.debugMode        = true;       // Enable debug output for tests
  suiteConfig.commandLineArgs  = "--nobook"; // Example: disable opening book via command-line

  // Run test suite
  TestSuiteRunner runner(config);
  arena::TestSuiteResult result = runner.runTestSuite(suiteConfig);

  // Validate result structure
  EXPECT_EQ(result.arenaVersion, "v1.1");
  EXPECT_FALSE(result.testSuiteName.empty());
  EXPECT_FALSE(result.timestamp.empty());
  EXPECT_FALSE(result.engineName.empty());
  EXPECT_EQ(result.enginePath, testEnginePath);

  EXPECT_EQ(result.totalTests, 1);
  EXPECT_GE(result.passed + result.failed, 0); // Should have evaluated
  EXPECT_EQ(result.skipped, 0);

  // With --nobook flag, engine MUST search and return nodes > 0
  EXPECT_GT(result.totalNodes, 0U) << "Engine should have searched (book disabled)";
  EXPECT_GT(result.totalTimeMs, 0) << "Search should take measurable time";

  // Check details
  ASSERT_EQ(result.details.size(), 1u);
  EXPECT_FALSE(result.details[0].testId.empty());
  EXPECT_FALSE(result.details[0].actual.empty()); // Must have returned a move
  EXPECT_GT(result.details[0].nodes, 0u) << "Engine should have searched nodes";
  EXPECT_GT(result.details[0].timeMs, 0) << "Search should take measurable time";
}

//=============================================================================
// Test 2: Multiple Test Types (BM, AM, DM)
//=============================================================================

TEST_F(TestSuiteRunnerIntegrationTest, MultipleTestTypes) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string testEpdPath = "test_multiple_types.epd";
  testFilesToCleanup.push_back(testEpdPath);

  // Create EPD with BM, AM, and simple position
  createTestEpdFile(testEpdPath,
                    // BM test - starting position
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4 d4; id \"BM Test\";\n"
                    // AM test - avoid moving knight to a3
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 am Na3 Nh3; id \"AM Test\";\n");

  ArenaConfig config;
  config.version = "v1.1";

  TestSuiteConfig suiteConfig;
  suiteConfig.name             = "test_multiple_types";
  suiteConfig.epdPath          = testEpdPath;
  suiteConfig.enginePath       = testEnginePath;
  suiteConfig.timePerMove      = milliseconds{100};
  suiteConfig.maxDepth         = static_cast<Depth>(5);
  suiteConfig.isolatePositions = true;
  suiteConfig.commandLineArgs  = "--nobook"; // Disable book

  TestSuiteRunner runner(config);
  arena::TestSuiteResult result = runner.runTestSuite(suiteConfig);

  EXPECT_EQ(result.totalTests, 2);
  EXPECT_EQ(result.details.size(), 2u);

  // Both tests should have been evaluated
  EXPECT_GE(result.passed + result.failed, 2);
}

//=============================================================================
// Test 3: Multiple Suites in Sequence
//=============================================================================

TEST_F(TestSuiteRunnerIntegrationTest, MultipleSequentialSuites) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string testEpd1 = "test_suite1.epd";
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string testEpd2 = "test_suite2.epd";
  testFilesToCleanup.push_back(testEpd1);
  testFilesToCleanup.push_back(testEpd2);

  createTestEpdFile(testEpd1,
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4; id \"Suite1-Test1\";\n");

  createTestEpdFile(testEpd2,
                    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1 bm e5; id \"Suite2-Test1\";\n");

  ArenaConfig config;
  config.version = "v1.1";

  // First suite
  TestSuiteConfig suite1;
  suite1.name            = "suite1";
  suite1.epdPath         = testEpd1;
  suite1.enginePath      = testEnginePath;
  suite1.timePerMove     = milliseconds{100};
  suite1.maxDepth        = static_cast<Depth>(5);
  suite1.commandLineArgs = "--nobook";

  // Second suite
  TestSuiteConfig suite2;
  suite2.name            = "suite2";
  suite2.epdPath         = testEpd2;
  suite2.enginePath      = testEnginePath;
  suite2.timePerMove     = milliseconds{100};
  suite2.maxDepth        = static_cast<Depth>(5);
  suite2.commandLineArgs = "--nobook";

  TestSuiteRunner runner(config);

  // Run both suites
  arena::TestSuiteResult result1 = runner.runTestSuite(suite1);
  arena::TestSuiteResult result2 = runner.runTestSuite(suite2);

  // Both should complete successfully
  EXPECT_EQ(result1.totalTests, 1);
  EXPECT_EQ(result2.totalTests, 1);

  // Should have different suite names
  EXPECT_FALSE(result1.testSuiteName.empty());
  EXPECT_FALSE(result2.testSuiteName.empty());
  EXPECT_NE(result1.testSuiteName, result2.testSuiteName);

  // Both should have searched nodes
  EXPECT_GT(result1.totalNodes, 0u) << "Suite 1 should have searched";
  EXPECT_GT(result2.totalNodes, 0u) << "Suite 2 should have searched";
}

//=============================================================================
// Test 4: Position Isolation Modes
//=============================================================================

TEST_F(TestSuiteRunnerIntegrationTest, PositionIsolation_Enabled) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string testEpdPath = "test_isolation.epd";
  testFilesToCleanup.push_back(testEpdPath);

  createTestEpdFile(testEpdPath,
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4; id \"Pos1\";\n"
                    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1 bm e5; id \"Pos2\";\n");

  ArenaConfig config;
  config.version = "v1.1";

  TestSuiteConfig suiteConfig;
  suiteConfig.name             = "test_isolation";
  suiteConfig.epdPath          = testEpdPath;
  suiteConfig.enginePath       = testEnginePath;
  suiteConfig.timePerMove      = milliseconds{100};
  suiteConfig.maxDepth         = static_cast<Depth>(5);
  suiteConfig.isolatePositions = true; // Enabled
  suiteConfig.commandLineArgs  = "--nobook";

  TestSuiteRunner runner(config);
  arena::TestSuiteResult result = runner.runTestSuite(suiteConfig);

  EXPECT_EQ(result.totalTests, 2);
  EXPECT_EQ(result.details.size(), 2u);
}

TEST_F(TestSuiteRunnerIntegrationTest, PositionIsolation_Disabled) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string testEpdPath = "test_no_isolation.epd";
  testFilesToCleanup.push_back(testEpdPath);

  createTestEpdFile(testEpdPath,
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4; id \"Pos1\";\n"
                    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1 bm e5; id \"Pos2\";\n");

  ArenaConfig config;
  config.version = "v1.1";

  TestSuiteConfig suiteConfig;
  suiteConfig.name             = "test_no_isolation";
  suiteConfig.epdPath          = testEpdPath;
  suiteConfig.enginePath       = testEnginePath;
  suiteConfig.timePerMove      = milliseconds{100};
  suiteConfig.maxDepth         = static_cast<Depth>(5);
  suiteConfig.isolatePositions = false; // Disabled
  suiteConfig.commandLineArgs  = "--nobook";

  TestSuiteRunner runner(config);
  arena::TestSuiteResult result = runner.runTestSuite(suiteConfig);

  EXPECT_EQ(result.totalTests, 2);
  EXPECT_EQ(result.details.size(), 2u);
}

//=============================================================================
// Test 5: Result Metadata Validation
//=============================================================================

TEST_F(TestSuiteRunnerIntegrationTest, ResultMetadata_Complete) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string testEpdPath = "test_metadata.epd";
  testFilesToCleanup.push_back(testEpdPath);

  createTestEpdFile(testEpdPath,
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4; id \"Meta Test\";\n");

  ArenaConfig config;
  config.version = "v1.1_test";

  TestSuiteConfig suiteConfig;
  suiteConfig.name            = "metadata_suite";
  suiteConfig.epdPath         = testEpdPath;
  suiteConfig.enginePath      = testEnginePath;
  suiteConfig.timePerMove     = milliseconds{100};
  suiteConfig.maxDepth        = static_cast<Depth>(5);
  suiteConfig.commandLineArgs = "--nobook";

  TestSuiteRunner runner(config);
  arena::TestSuiteResult result = runner.runTestSuite(suiteConfig);

  // Validate all metadata fields are populated
  EXPECT_EQ(result.arenaVersion, "v1.1_test");
  EXPECT_FALSE(result.testSuiteName.empty());
  EXPECT_FALSE(result.timestamp.empty());

  // Engine metadata
  EXPECT_FALSE(result.engineName.empty());
  EXPECT_EQ(result.enginePath, testEnginePath);

  // Summary statistics
  EXPECT_EQ(result.totalTests, 1);
  EXPECT_GE(result.passed, 0);
  EXPECT_GE(result.failed, 0);
  EXPECT_EQ(result.skipped, 0);
  EXPECT_GT(result.totalNodes, 0u) << "Engine should have searched";
  EXPECT_GT(result.totalTimeMs, 0) << "Search should take time";

  // Detail metadata
  ASSERT_EQ(result.details.size(), 1u);
  EXPECT_EQ(result.details[0].testId, "Meta Test");
  EXPECT_FALSE(result.details[0].fen.empty());
  EXPECT_FALSE(result.details[0].expected.empty());
  EXPECT_FALSE(result.details[0].actual.empty());
  EXPECT_GT(result.details[0].nodes, 0U) << "Should have searched nodes";
  EXPECT_GT(result.details[0].timeMs, 0) << "Should have taken time";
}

//=============================================================================
// Test 6: Empty EPD File Handling
//=============================================================================

TEST_F(TestSuiteRunnerIntegrationTest, EmptyEpdFile_ThrowsError) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string testEpdPath = "test_empty.epd";
  testFilesToCleanup.push_back(testEpdPath);

  // Create empty EPD file
  std::ofstream file(testEpdPath);
  file.close();

  ArenaConfig config;
  config.version = "v1.1";

  TestSuiteConfig suiteConfig;
  suiteConfig.name        = "empty_suite";
  suiteConfig.epdPath     = testEpdPath;
  suiteConfig.enginePath  = testEnginePath;
  suiteConfig.timePerMove = milliseconds{100};
  suiteConfig.maxDepth    = static_cast<Depth>(5);

  TestSuiteRunner runner(config);

  // Should throw error about no valid tests
  EXPECT_THROW({ runner.runTestSuite(suiteConfig); }, std::runtime_error);
}

//=============================================================================
// Test 7: Missing EPD File Handling
//=============================================================================

TEST_F(TestSuiteRunnerIntegrationTest, MissingEpdFile_ThrowsError) {
  ArenaConfig config;
  config.version = "v1.1";

  TestSuiteConfig suiteConfig;
  suiteConfig.name        = "missing_epd";
  suiteConfig.epdPath     = "nonexistent_file.epd";
  suiteConfig.enginePath  = testEnginePath;
  suiteConfig.timePerMove = milliseconds{100};
  suiteConfig.maxDepth    = static_cast<Depth>(5);

  TestSuiteRunner runner(config);

  // Should throw error about missing file
  EXPECT_THROW({ runner.runTestSuite(suiteConfig); }, std::runtime_error);
}

//=============================================================================
// Test 8: Invalid FEN Handling
//=============================================================================

TEST_F(TestSuiteRunnerIntegrationTest, InvalidFEN_ContinuesSuite) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string testEpdPath = "test_invalid_fen.epd";
  testFilesToCleanup.push_back(testEpdPath);

  createTestEpdFile(testEpdPath,
                    // Valid position
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4; id \"Valid\";\n"
                    // Invalid FEN (missing parts)
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR bm e4; id \"Invalid\";\n"
                    // Another valid position
                    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1 bm e5; id \"Valid2\";\n");

  ArenaConfig config;
  config.version = "v1.1";

  TestSuiteConfig suiteConfig;
  suiteConfig.name        = "invalid_fen_suite";
  suiteConfig.epdPath     = testEpdPath;
  suiteConfig.enginePath  = testEnginePath;
  suiteConfig.timePerMove = milliseconds{100};
  suiteConfig.maxDepth    = static_cast<Depth>(5);

  TestSuiteRunner runner(config);
  arena::TestSuiteResult result = runner.runTestSuite(suiteConfig);

  // Should have 2 valid tests (invalid FEN skipped by parser or marked failed)
  EXPECT_GE(result.totalTests, 2);

  // Suite should complete despite invalid FEN
  EXPECT_GE(result.passed + result.failed, 2);
}

//=============================================================================
// Test 9: Stress Test - Multiple Positions
//=============================================================================

TEST_F(TestSuiteRunnerIntegrationTest, StressTest_MultiplePositions) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string testEpdPath = "test_stress.epd";
  testFilesToCleanup.push_back(testEpdPath);

  // Create EPD with 10 positions
  std::ostringstream epdContent;
  for (int i = 0; i < 10; ++i) {
    epdContent << "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
               << "bm e4 d4; id \"Stress" << i << "\";\n";
  }

  createTestEpdFile(testEpdPath, epdContent.str());

  ArenaConfig config;
  config.version = "v1.1";

  TestSuiteConfig suiteConfig;
  suiteConfig.name             = "stress_suite";
  suiteConfig.epdPath          = testEpdPath;
  suiteConfig.enginePath       = testEnginePath;
  suiteConfig.timePerMove      = milliseconds{50}; // Fast
  suiteConfig.maxDepth         = static_cast<Depth>(3);
  suiteConfig.isolatePositions = true;
  suiteConfig.commandLineArgs  = "--nobook";

  TestSuiteRunner runner(config);
  arena::TestSuiteResult result = runner.runTestSuite(suiteConfig);

  EXPECT_EQ(result.totalTests, 10);
  EXPECT_EQ(result.details.size(), 10U);

  // All tests should have been evaluated
  EXPECT_EQ(result.passed + result.failed, 10);

  // Should have accumulated statistics from searches
  EXPECT_GT(result.totalNodes, 0U) << "Should have searched across 10 positions";
  EXPECT_GT(result.totalTimeMs, 0) << "Should have taken measurable time";
}

//=============================================================================
// Test 10: Engine Name Extraction
//=============================================================================

TEST_F(TestSuiteRunnerIntegrationTest, EngineNameExtraction) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string testEpdPath = "test_engine_name.epd";
  testFilesToCleanup.push_back(testEpdPath);

  createTestEpdFile(testEpdPath,
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4; id \"Name Test\";\n");

  ArenaConfig config;
  config.version = "v1.1";

  TestSuiteConfig suiteConfig;
  suiteConfig.name            = "engine_name_test";
  suiteConfig.epdPath         = testEpdPath;
  suiteConfig.enginePath      = testEnginePath;
  suiteConfig.timePerMove     = milliseconds{100};
  suiteConfig.maxDepth        = static_cast<Depth>(5);
  suiteConfig.commandLineArgs = "--nobook";

  TestSuiteRunner runner(config);
  arena::TestSuiteResult result = runner.runTestSuite(suiteConfig);

  // Engine name should be extracted from UCI "id name" response
  EXPECT_FALSE(result.engineName.empty());

  // For FrankyCPP, should contain "FrankyCPP" and version
  EXPECT_NE(result.engineName.find("FrankyCPP"), std::string::npos);
}
