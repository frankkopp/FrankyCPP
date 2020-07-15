/*
 * MIT License
 *
 * Copyright (c) 2018 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "init.h"
#include "common/Logging.h"
#include "types/types.h"
#include "version.h"
#include "enginetest/TestSuite.h"

#include <gtest/gtest.h>
using testing::Eq;

class TestSuite_Test : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().TSUITE_LOG->set_level(spdlog::level::info);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::warn);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TestSuite_Test, readFile) {
  MilliSec moveTime{0};
  Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/franky_tests.epd";
  TestSuite ts{moveTime, depth, filePath};
  ASSERT_EQ(13, ts.testCases.size());
}

TEST_F(TestSuite_Test, franky_test) {
  MilliSec moveTime{200ms};
  Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/franky_tests.epd";
  TestSuite ts{moveTime, depth, filePath};
  ts.runTestSuite();
}

TEST_F(TestSuite_Test, mate_test) {
  MilliSec moveTime{15s};
  Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/mate_test_suite.epd";
  TestSuite ts{moveTime, depth, filePath};
  ts.runTestSuite();
}

TEST_F(TestSuite_Test, wac_test) {
  MilliSec moveTime{5s};
  Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/wac.epd";
  TestSuite ts{moveTime, depth, filePath};
  ts.runTestSuite();
}