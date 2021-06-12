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

#include "engine/Search.h"
#include "engine/SearchConfig.h"
#include "init.h"
#include "types/types.h"

#include <engine/EvalConfig.h>
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
  Search search{};search.isReady();
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

  sl.moveTime = milliseconds{1500};
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
  sl.whiteTime = 30s;
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
  EXPECT_GT(3.020s, (high_resolution_clock::now() - s.startTime));
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
  EXPECT_GT(500ms, s.getLastSearchResult().time);
}

TEST_F(SearchTest, startPonderSearch) {
  SearchConfig::USE_BOOK   = false;
  SearchConfig::USE_PONDER = true;
  Position p{};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = milliseconds{1000};
  sl.ponder      = true;
  s.isReady();
  TimePoint start = currentTime();
  s.startSearch(p, sl);
  EXPECT_TRUE(s.isSearching());
  EXPECT_FALSE(s.hasResult());
  SLEEP(nanoseconds(nanoPerSec));
  s.ponderhit();
  s.waitWhileSearching();
  EXPECT_TRUE(s.hasResult());
  EXPECT_LT(int64_t(nanoPerSec - 20'000'000), s.getLastSearchResult().time.count());
  EXPECT_GT(int64_t(nanoPerSec * 1.1), s.getLastSearchResult().time.count());
  EXPECT_GT(int64_t(nanoPerSec * 2.5), elapsedSince(start).count());
}

TEST_F(SearchTest, startNodesLimitedSearch) {
  SearchConfig::USE_BOOK = false;
  Position p{};
  SearchLimits sl{};
  Search s{};
  sl.infinite = true;
  sl.nodes    = 10'000'000;
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
  const int depth = 5;
  sl.depth        = depth;
  s.isReady();
  s.startSearch(p, sl);
  EXPECT_TRUE(s.isSearching());
  EXPECT_FALSE(s.hasResult());
  s.waitWhileSearching();
  EXPECT_TRUE(s.hasResult());
  EXPECT_EQ(depth, s.getLastSearchResult().depth);
}

TEST_F(SearchTest, stalemate0Search) {
  SearchConfig::USE_BOOK = false;
  Position p{"6R1/8/8/8/8/5K2/R7/7k b - -"};
  SearchLimits sl{};
  Search s{};
  const int depth = 6;
  sl.depth        = depth;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  EXPECT_EQ(VALUE_DRAW, s.getLastSearchResult().bestMoveValue);
}

TEST_F(SearchTest, mate0Search) {
  SearchConfig::USE_BOOK = false;
  Position p{"8/8/8/8/8/5K2/8/R4k2 b - -"};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = 60s;
  sl.mate        = 0;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  EXPECT_EQ(-VALUE_CHECKMATE, s.getLastSearchResult().bestMoveValue);
}

TEST_F(SearchTest, mate1Search) {
  SearchConfig::USE_BOOK = false;
  Position p{"8/8/8/8/8/6K1/R7/6k1 w - - 0 8"};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = 60s;
  sl.mate        = 1;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  EXPECT_EQ(VALUE_CHECKMATE - 1, s.getLastSearchResult().bestMoveValue);
  EXPECT_TRUE(s.getLastSearchResult().mateFound);
}

TEST_F(SearchTest, mate2Search) {
  SearchConfig::USE_BOOK = false;
  Position p{"8/8/8/8/8/5K2/R7/7k w - - 0 7"};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = 60s;
  sl.mate        = 2;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  EXPECT_EQ(VALUE_CHECKMATE - 3, s.getLastSearchResult().bestMoveValue);
  EXPECT_TRUE(s.getLastSearchResult().mateFound);
}

TEST_F(SearchTest, mate3Search) {
  SearchConfig::USE_BOOK = false;
  Position p{"8/8/8/8/8/4K3/R7/6k1 w - - 0 6"};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = 60s;
  sl.mate        = 3;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  EXPECT_EQ(VALUE_CHECKMATE - 5, s.getLastSearchResult().bestMoveValue);
  EXPECT_TRUE(s.getLastSearchResult().mateFound);
}

TEST_F(SearchTest, mate4Search) {
  SearchConfig::USE_BOOK = false;
  Position p{"8/8/8/8/8/3K4/R7/5k2 w - - 0 5"};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = 60s;
  sl.mate        = 4;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  EXPECT_EQ(VALUE_CHECKMATE - 7, s.getLastSearchResult().bestMoveValue);
  EXPECT_TRUE(s.getLastSearchResult().mateFound);
}

TEST_F(SearchTest, mate5Search) {
  SearchConfig::USE_BOOK      = false;
  SearchConfig::USE_ALPHABETA = true;
  Position p{"8/8/8/8/4K3/8/R7/4k3 w - - 0 4"};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = 60s;
  sl.mate        = 5;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  EXPECT_EQ(VALUE_CHECKMATE - 9, s.getLastSearchResult().bestMoveValue);
  EXPECT_TRUE(s.getLastSearchResult().mateFound);
}

TEST_F(SearchTest, quiescenceTest) {

  Search search;
  SearchLimits searchLimits;
  Position position("r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1 w kq - 10 113");
  searchLimits.depth = 2;

  SearchConfig::USE_BOOK       = false;
  SearchConfig::USE_ALPHABETA  = false;
  SearchConfig::USE_PVS        = false;
  SearchConfig::USE_TT         = false;
  SearchConfig::USE_QUIESCENCE = false;
  SearchConfig::USE_QS_SEE     = false;

  search.startSearch(position, searchLimits);
  search.waitWhileSearching();
  auto nodes1 = search.getLastSearchResult().nodes;
  auto extra1 = search.getSearchStats().currentExtraSearchDepth;

  SearchConfig::USE_QUIESCENCE = true;
  search.startSearch(position, searchLimits);
  search.waitWhileSearching();
  auto nodes2 = search.getLastSearchResult().nodes;
  auto extra2 = search.getSearchStats().currentExtraSearchDepth;

  SearchConfig::USE_QS_SEE = true;
  search.startSearch(position, searchLimits);
  search.waitWhileSearching();
  auto nodes3 = search.getLastSearchResult().nodes;
  auto extra3 = search.getSearchStats().currentExtraSearchDepth;

  LOG__INFO(Logger::get().TEST_LOG, "Nodes without Quiescence: {:L} Nodes with Quiescence: {:L} Nodes with SEE: {:L}", nodes1, nodes2, nodes3);
  LOG__INFO(Logger::get().TEST_LOG, "Extra without Quiescence: {:L} Extra with Quiescence: {:L} Extra with SEE: {:L}", extra1, extra2, extra3);

  ASSERT_GT(nodes2, nodes1);
  ASSERT_GT(extra2, extra1);
}

TEST_F(SearchTest, debug) {
  GTEST_SKIP();
  SearchConfig::TT_SIZE_MB          = 64;
  SearchConfig::USE_BOOK            = false;
  SearchConfig::USE_ALPHABETA       = false;
  SearchConfig::USE_PVS             = false;
  SearchConfig::USE_ASP             = false;
  SearchConfig::USE_TT              = false;
  SearchConfig::USE_TT_VALUE        = false;
  SearchConfig::USE_EVAL_TT         = false;
  SearchConfig::USE_MDP             = false;
  SearchConfig::USE_HISTORY_COUNTER = false;
  SearchConfig::USE_HISTORY_MOVES   = false;
  SearchConfig::USE_QUIESCENCE      = true;
  SearchConfig::USE_QS_STANDPAT_CUT = false;
  SearchConfig::USE_QS_SEE          = false;
  SearchConfig::USE_QS_TT           = false;

  EvalConfig::USE_MATERIAL   = true;
  EvalConfig::USE_POSITIONAL = true;
  EvalConfig::TEMPO          = 34;

  Position p{"2rr2k1/1p2qp1p/1pn1pp2/1N6/3P4/P6P/1P2QPP1/2R2RK1 w - - 0 1"};
  SearchLimits sl{};
  Search s{};
  //  sl.timeControl = true;
  //  sl.moveTime    = 160s;
  sl.depth = 6;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
}
