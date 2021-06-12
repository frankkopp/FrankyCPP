// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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

#include <ctime>

#include "Test_Fens.h"
#include "chesscore/Position.h"
#include "common/Logging.h"
#include "engine/Search.h"
#include "engine/SearchConfig.h"
#include "enginetest/SearchTreeSizeTest.h"
#include "init.h"
#include "types/types.h"

#include <gtest/gtest.h>
using testing::Eq;

class SearchTreeSizeTest_Test : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::debug);
    Logger::get().TT_LOG->set_level(spdlog::level::debug);
    Logger::get().BOOK_LOG->set_level(spdlog::level::warn);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(SearchTreeSizeTest_Test, size_test) {
  GTEST_SKIP();

  static constexpr int DEPTH = 8;
  static constexpr milliseconds MOVE_TIME{0};
  static constexpr int START_FEN = 0;
  static constexpr int END_FEN   = 15;

  // Prepare test fens
  // get sub vector of fens to test
  std::vector<std::string> allFens = Test_Fens::getFENs();
  auto iterStart                   = allFens.begin() + START_FEN;
  auto iterEnd                     = allFens.begin() + START_FEN + END_FEN;
  if (iterEnd > allFens.end()) iterEnd = allFens.end();
  if (iterStart > iterEnd) iterStart = iterEnd;
  std::vector<std::string> testFens(iterStart, iterEnd);

  // execute tests
  SearchTreeSizeTest stst(DEPTH, MOVE_TIME, testFens);
  stst.start();
}
