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
#include "engine/SearchConfig.h"
#include "init.h"
#include "types/types.h"

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
  void SetUp() override {}
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
  sl.whiteTime = 30s;
  sl.blackTime = 30s;
  fprintln("{}", str(Search::setupTimeControl(p, sl)));
  EXPECT_EQ(675ms, Search::setupTimeControl(p, sl));

  sl           = SearchLimits{};
  sl.whiteTime = 3s;
  sl.blackTime = 3s;
  fprintln("{}", str(Search::setupTimeControl(p, sl)));
  EXPECT_EQ(60ms, Search::setupTimeControl(p, sl));

  sl           = SearchLimits{};
  sl.whiteTime =30s;
  sl.whiteInc  = 1s;
  sl.blackTime = 30s;
  sl.blackInc  = 1s;
  fprintln("{}", str(Search::setupTimeControl(p, sl)));
  EXPECT_EQ(1575ms, Search::setupTimeControl(p, sl));
}

TEST_F(SearchTest, extraTime) {
  Position p{};
  SearchLimits sl{};
  Search s{};
  s.searchLimits.timeControl = true;
  s.timeLimit                = 10s;
  s.extraTime                = 0ms;
  s.addExtraTime(0.9);
  fprintln("{}", str(s.extraTime));
  EXPECT_EQ(-999ms, s.extraTime);
  s.extraTime = 0ms;
  s.addExtraTime(1.2);
  fprintln("{}", str(s.extraTime));
  EXPECT_EQ(1999ms, s.extraTime);
}

TEST_F(SearchTest, startTimer) {
  Position p{};
  SearchLimits sl{};
  Search s{};
  s.searchLimits.timeControl = true;
  s.startTime                = high_resolution_clock::now();
  s.timeLimit                = 2s;
  s.extraTime                = 1s;
  s.startTimer();
  s.timerThread.join();
  EXPECT_LT(3s, (high_resolution_clock::now() - s.startTime));
  EXPECT_GT(3.010s, (high_resolution_clock::now() - s.startTime));
}

TEST_F(SearchTest, startStopSearch) {
  SearchConfig::USE_BOOK = false;
  Position p{};
  SearchLimits sl{};
  Search s{};
  sl.infinite = true;
  s.isReady();
  s.startSearch(p, sl);
  EXPECT_TRUE(s.isSearching());
  EXPECT_FALSE(s.hasResult());
  SLEEP(nanoseconds(1s));
  s.stopSearch();
  s.waitWhileSearching();
  EXPECT_TRUE(s.hasResult());
  EXPECT_LT(1s, s.getLastSearchResult().time);
  EXPECT_GT(1s * 1.1, s.getLastSearchResult().time);
}

TEST_F(SearchTest, startTimedSearch) {
  SearchConfig::USE_BOOK = false;
  Position p{};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = 1s;
  s.isReady();
  s.startSearch(p, sl);
  EXPECT_TRUE(s.isSearching());
  EXPECT_FALSE(s.hasResult());
  s.waitWhileSearching();
  EXPECT_TRUE(s.hasResult());
  EXPECT_LT(1s - 20ms, s.getLastSearchResult().time);
  EXPECT_GT(1s * 1.1, s.getLastSearchResult().time);
}

TEST_F(SearchTest, bookMoveSearch) {
  SearchConfig::USE_BOOK = true;
  Position p{};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = 1s;
  s.isReady();
  s.startSearch(p, sl);
  EXPECT_TRUE(s.isSearching());
  s.waitWhileSearching();
  EXPECT_TRUE(s.hasResult());
  EXPECT_NE(MOVE_NONE, s.getLastSearchResult().bestMove);
  EXPECT_TRUE(s.getLastSearchResult().bookMove);
  EXPECT_LT(100us, s.getLastSearchResult().time);
  EXPECT_GT(1ms, s.getLastSearchResult().time);
}

TEST_F(SearchTest, startPonderSearch) {
  SearchConfig::USE_BOOK = false;
  SearchConfig::USE_PONDER = true;
  Position p{};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = MilliSec{1000};
  sl.ponder = true;
  s.isReady();
  TimePoint start = now();
  s.startSearch(p, sl);
  EXPECT_TRUE(s.isSearching());
  EXPECT_FALSE(s.hasResult());
  SLEEP(nanoseconds(nanoPerSec));
  s.ponderhit();
  s.waitWhileSearching();
  EXPECT_TRUE(s.hasResult());
  EXPECT_LT(nanoPerSec - 20'000'000, s.getLastSearchResult().time.count());
  EXPECT_GT(uint64_t(nanoPerSec * 1.1), s.getLastSearchResult().time.count());
  EXPECT_GT(uint64_t(nanoPerSec * 2.1), elapsedSince(start).count());
}

TEST_F(SearchTest, startNodesLimitedSearch) {
  SearchConfig::USE_BOOK = false;
  Position p{};
  SearchLimits sl{};
  Search s{};
  sl.infinite = true;
  sl.nodes = 10'000'000;
  s.isReady();
  s.startSearch(p, sl);
  EXPECT_TRUE(s.isSearching());
  EXPECT_FALSE(s.hasResult());
  s.waitWhileSearching();
  EXPECT_TRUE(s.hasResult());
  EXPECT_EQ(10'000'000, s.getLastSearchResult().nodes);
}

TEST_F(SearchTest, depthLimitedSearch) {
  SearchConfig::USE_BOOK = false;
  Position p{};
  SearchLimits sl{};
  Search s{};
  const int depth = 7;
  sl.depth = depth;
  s.isReady();
  s.startSearch(p, sl);
  EXPECT_TRUE(s.isSearching());
  EXPECT_FALSE(s.hasResult());
  s.waitWhileSearching();
  EXPECT_TRUE(s.hasResult());
  EXPECT_EQ(depth, s.getLastSearchResult().depth);
}