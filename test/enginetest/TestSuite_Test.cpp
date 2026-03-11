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

#include "Test_Utils.h"
#include "common/CrashHandler.h"
#include "common/Logging.h"
#include "engine/UciOptions.h"
#include "enginetest/TestSuite.h"
#include "init.h"
#include "types/types.h"
#include "version.h"

#include <filesystem>
#include <gtest/gtest.h>
using testing::Eq;

using namespace engine;
using namespace chess;
using namespace config;
using namespace common;
using namespace enginetest;


class TestSuite_Test : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;

    // Ensure evaluation settings are restored to defaults for this fixture,
    // as other tests (e.g., EvaluatorTest) toggle EvalConfig globals.
    ConfigManager::instance().resetToDefaults();

    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::debug);
    Logger::get().TT_LOG->set_level(spdlog::level::warn);
    Logger::get().EVAL_LOG->set_level(spdlog::level::warn);
    Logger::get().UCI_LOG->set_level(spdlog::level::info);
    Logger::get().TSUITE_LOG->set_level(spdlog::level::info);

    // Install crash handler to generate minidumps on access violations
    crashhandler::install("./crash_dumps");
  }

  static void TearDownTestSuite() {
    crashhandler::uninstall();
  }

protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(TestSuite_Test, readFile) {
  constexpr milliseconds moveTime{0};
  constexpr Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/franky_tests.epd";
  const TestSuite ts{moveTime, depth, filePath};
  ASSERT_EQ(13, ts.getTestCases().size());
}

TEST_F(TestSuite_Test, franky_test) {
#ifndef NDEBUG
  GTEST_SKIP() << "Skipping in debug builds";
#endif
  CONFIG_OVERRIDE(s.USE_BOOK = false);
  constexpr auto moveTime{5s};
  constexpr Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/franky_tests.epd";
  TestSuite ts{moveTime, depth, filePath};
  ts.runTestSuite();
  ASSERT_EQ(13, ts.getLastResult().counter);
  // ASSERT_EQ(13, ts.getLastResult().successCounter);
}

// Date:       2025-10-13 13:17:50 v0.6
// Successful: 14  (70 %)
// Failed:     6   (30 %)
TEST_F(TestSuite_Test, mate_test) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  constexpr milliseconds moveTime{15s};
  constexpr Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/mate_test_suite.epd";
  TestSuite ts{moveTime, depth, filePath};
  ts.runTestSuite();
}

// Date:       2025-10-09 17:29:56 v0.6
// Successful: 193 (96 %)
// Failed:     8   (3 %)
TEST_F(TestSuite_Test, wac_test) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  constexpr milliseconds moveTime{5s};
  constexpr Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/wac.epd";
  TestSuite ts{moveTime, depth, filePath};
  ts.runTestSuite();
}

// 9.10.2025 v0.6
// Successful: 181 (52 %)
// Failed:     164 (47 %)
TEST_F(TestSuite_Test, crafty_test) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  constexpr milliseconds moveTime{5s};
  constexpr Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/crafty_test.epd";
  TestSuite ts{moveTime, depth, filePath};
  ts.runTestSuite();
}

// 9.10.2025 v0.6
// Successful: 521 (67 %)
// Failed:     248 (32 %)
TEST_F(TestSuite_Test, ecm98_test) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  CONFIG_OVERRIDE(s.USE_BOOK = false);
  constexpr milliseconds moveTime{5s};
  constexpr Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/ecm98.epd";
  TestSuite ts{moveTime, depth, filePath};
  ts.runTestSuite();
}

// =============================================================================
// Stress test for sanitizers (ASAN/TSAN)
// Runs multiple testsuites sequentially to trigger potential memory corruption
// or data race bugs under heavy multi-threaded search load.
//
// cmake --preset win-relwithdebinfo-asan
// cmake --build cmake-build-win-relwithdebinfo-asan
// .\cmake-build-win-relwithdebinfo-asan\test\FrankyCPP_v1.6_Test.exe --gtest_filter=*stress_test_for_sanitizers*
//
// cmake --preset wsl-debug-tsan
// cmake --build cmake-build-wsl-debug-tsan
// setarch $(uname -m) -R ./cmake-build-wsl-debug-tsan/test/FrankyCPP_v1.6_Test --gtest_filter=*stress_test_for_sanitizers* 2>&1 | tee tsan_output.txt
// =============================================================================
TEST_F(TestSuite_Test, stress_test_for_sanitizers) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }

  // This test is disabled by default - enable it explicitly when running
  // under sanitizers to find memory/threading bugs.
  // Run with: --gtest_filter=*stress_test*

  // Reduce log verbosity for stress testing
  Logger::get().TEST_LOG->set_level(spdlog::level::warn);
  Logger::get().SEARCH_LOG->set_level(spdlog::level::warn);
  Logger::get().UCI_LOG->set_level(spdlog::level::warn);
  Logger::get().TSUITE_LOG->set_level(spdlog::level::warn);

  CONFIG_OVERRIDE(s.USE_BOOK = false);
  CONFIG_OVERRIDE(s.THREADS = 4);
  CONFIG_OVERRIDE(s.TT_SIZE_MB = 32);
  CONFIG_OVERRIDE(e.PAWN_TT_SIZE_MB = 32);

  // CONFIG_OVERRIDE(e.USE_PAWN_TT = false);

  // Use longer time per position to account for sanitizer overhead.
  // ASan/TSan can slow down execution by 10x or more, and initialization
  // per position takes significant time. 5 seconds ensures actual search happens.
  constexpr milliseconds moveTime{5000ms};
  constexpr Depth depth{0};

  const std::string basePath = std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/";

  // List of testsuites to run - these are the ones used in Arena testing
  const std::vector<std::string> testSuites = {
    "wac.epd",                    // 201 positions - tactical puzzles
    "crafty_test.epd",            // 345 positions - crafty test suite
    "ecm98.epd",                  // 769 positions - encyclopedia of chess middlegames
    "kaufman.epd",                // 25 positions - Kaufman test
    "silent-but-deadly.epd",      // tactical positions
    "eigenmann-rapid-engine.epd", // positional tests
  };

  int totalTests  = 0;
  int totalPassed = 0;

  for (const auto& suite : testSuites) {
    const std::string filePath = basePath + suite;

    // Check if file exists
    if (!std::filesystem::exists(filePath)) {
      LOG__WARN(Logger::get().TEST_LOG, "Testsuite not found, skipping: {}", suite);
      continue;
    }

    LOG__INFO(Logger::get().TEST_LOG, "\n========== Running testsuite: {} ==========", suite);

    auto s = ConfigManager::instance().search();
    auto e = ConfigManager::instance().eval();
    fprintln("\nConfig: Threads: {}, TT Size: {} MB, PawnTT Size: {} MB", s.THREADS, s.TT_SIZE_MB, e.USE_PAWN_TT ? e.PAWN_TT_SIZE_MB : 0);

    TestSuite ts{moveTime, depth, filePath};
    ts.runTestSuite();

    const auto& result = ts.getLastResult();
    totalTests += result.counter;
    totalPassed += result.successCounter;

    LOG__INFO(Logger::get().TEST_LOG, "Completed {}: {}/{} passed",
              suite, result.successCounter, result.counter);
  }

  LOG__INFO(Logger::get().TEST_LOG, "\n========== STRESS TEST COMPLETE ==========");
  LOG__INFO(Logger::get().TEST_LOG, "Total: {}/{} passed across all testsuites", totalPassed, totalTests);

  // Don't assert on pass rate - this test is for finding crashes, not correctness
}

// Quick stress test - smaller subset for faster sanitizer runs
TEST_F(TestSuite_Test, quick_stress_test) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  // Reduce log verbosity for stress testing
  Logger::get().TEST_LOG->set_level(spdlog::level::warn);
  Logger::get().SEARCH_LOG->set_level(spdlog::level::warn);
  Logger::get().UCI_LOG->set_level(spdlog::level::warn);
  Logger::get().TSUITE_LOG->set_level(spdlog::level::warn);

  CONFIG_OVERRIDE(s.USE_BOOK = false);

  // Use longer time to account for sanitizer overhead (init takes ~1-2s under ASan)
  constexpr milliseconds moveTime{3000ms};
  constexpr Depth depth{0};

  const std::string basePath = std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/";

  // Just run WAC which is relatively small but exercises the search well
  const std::vector<std::string> testSuites = {
    "wac.epd",     // 201 positions
    "kaufman.epd", // 25 positions
  };

  auto s = ConfigManager::instance().search();
  auto e = ConfigManager::instance().eval();
  fprintln("\nConfig: Threads: {}, TT Size: {} MB, PawnTT Size: {} MB", s.THREADS, s.TT_SIZE_MB, e.USE_PAWN_TT ? e.PAWN_TT_SIZE_MB : 0);

  for (const auto& suite : testSuites) {
    const std::string filePath = basePath + suite;
    if (!std::filesystem::exists(filePath)) continue;

    LOG__INFO(Logger::get().TEST_LOG, "Running: {}", suite);
    TestSuite ts{moveTime, depth, filePath};
    ts.runTestSuite();
  }
}
