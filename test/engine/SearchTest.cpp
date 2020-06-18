/*
 * MIT License
 *
 * Copyright (c) 2020 Frank Kopp
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

#include "engine/Search.h"
#include "init.h"
#include "types/types.h"

#include <engine/SearchConfig.h>
#include <gtest/gtest.h>
using testing::Eq;

class SearchTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().UCIHAND_LOG->set_level(spdlog::level::debug);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::debug);
    Logger::get().TT_LOG->set_level(spdlog::level::debug);
    Logger::get().BOOK_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {
    //SearchConfig::USE_BOOK = false;
  }
  void TearDown() override {}
};

TEST_F(SearchTest, construct) {
  Search search{};
  search.isReady();
}

TEST_F(SearchTest, resizeHash) {
  Search search{};
  search.isReady();
  SearchConfig::TT_SIZE_MB = 1024;
  search.resizeTT();
}

TEST_F(SearchTest, setupTime) {
  Position p{};
  SearchLimits sl{};

  sl.moveTime = MilliSec{1500};
  EXPECT_EQ(1480, Search::setupTimeControl(p, sl).count());

  sl           = SearchLimits{};
  sl.whiteTime = MilliSec{30000};
  sl.blackTime = MilliSec{30000};
  fprintln("{}", str(Search::setupTimeControl(p, sl)));
  EXPECT_EQ(675, Search::setupTimeControl(p, sl).count());

  sl           = SearchLimits{};
  sl.whiteTime = MilliSec{3000};
  sl.blackTime = MilliSec{3000};
  fprintln("{}", str(Search::setupTimeControl(p, sl)));
  EXPECT_EQ(60, Search::setupTimeControl(p, sl).count());

  sl           = SearchLimits{};
  sl.whiteTime = MilliSec{30000};
  sl.whiteInc  = MilliSec{1000};
  sl.blackTime = MilliSec{30000};
  sl.blackInc  = MilliSec{1000};
  fprintln("{}", str(Search::setupTimeControl(p, sl)));
  EXPECT_EQ(1575, Search::setupTimeControl(p, sl).count());
}

TEST_F(SearchTest, extraTime) {
  Position p{};
  SearchLimits sl{};
  Search s{};
  s.searchLimits.timeControl = true;
  s.timeLimit = MilliSec(10000);
  s.extraTime = MilliSec(0);
  s.addExtraTime(0.9);
  fprintln("{}", str(s.extraTime));
  EXPECT_EQ(-999, s.extraTime.count());
  s.extraTime = MilliSec(0);
  s.addExtraTime(1.2);
  fprintln("{}", str(s.extraTime));
  EXPECT_EQ(1999, s.extraTime.count());
}

TEST_F(SearchTest, startTimer) {
  Position p{};
  SearchLimits sl{};
  Search s{};
  s.searchLimits.timeControl = true;
  s.startTime = std::chrono::high_resolution_clock::now();
  s.timeLimit = MilliSec(2000);
  s.extraTime = MilliSec(1000);
  s.startTimer();
  s.timerThread.join();
  EXPECT_LT(3'000'000'000, (std::chrono::high_resolution_clock::now() - s.startTime).count());
  EXPECT_GT(3'010'000'000, (std::chrono::high_resolution_clock::now() - s.startTime).count());
}