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

#include "common/Logging.h"
#include "engine/UciOptions.h"
#include "enginetest/TestSuite.h"
#include "init.h"
#include "types/types.h"
#include "version.h"
#include "Test_Utils.h"

#include <gtest/gtest.h>
using testing::Eq;


class TestSuite_Test : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;

    // Ensure evaluation settings are restored to defaults for this fixture,
    // as other tests (e.g., EvaluatorTest) toggle EvalConfig globals.
    engine::config::ConfigManager::instance().resetToDefaults();

    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::debug);
    Logger::get().TT_LOG->set_level(spdlog::level::warn);
    Logger::get().EVAL_LOG->set_level(spdlog::level::warn);
    Logger::get().UCI_LOG->set_level(spdlog::level::info);
    Logger::get().TSUITE_LOG->set_level(spdlog::level::info);
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
  ASSERT_EQ(13, ts.testCases.size());
}

TEST_F(TestSuite_Test, franky_test) {
  CONFIG_OVERRIDE(s.USE_BOOK = false);
  constexpr auto moveTime{3s};
  constexpr Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/franky_tests.epd";
  TestSuite ts{moveTime, depth, filePath};
  ts.runTestSuite();
  ASSERT_EQ(13, ts.getLastResult().counter);
  ASSERT_EQ(13, ts.getLastResult().successCounter);
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
