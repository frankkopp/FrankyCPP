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
#include <iomanip>
#include <sstream>

using testing::Eq;

inline bool isBulkRun() {
  const auto* ut  = testing::UnitTest::GetInstance();
  const bool cond = ut && ut->test_to_run_count() > 1;
  if (cond) {
    std::cout << "Bulk run detected - limiting depth to shorten test time" << std::endl;
  }
  return cond;
}

class SearchTest : public testing::Test {
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
  Search search{};
  SearchLimits sl{};

  sl.moveTime = milliseconds{1500};
  EXPECT_EQ(1490, search.setupTimeControl(p, sl).count());

  sl           = SearchLimits{};
  sl.whiteTime = 30s;
  sl.blackTime = 30s;
  auto t_ms = search.setupTimeControl(p, sl);
  fprintln("{}", str(t_ms));
  EXPECT_GE(t_ms.count(), 600);
  EXPECT_LE(t_ms.count(), 660);

  sl           = SearchLimits{};
  sl.whiteTime = 3s;
  sl.blackTime = 3s;
  t_ms = search.setupTimeControl(p, sl);
  fprintln("{}", str(t_ms));
  EXPECT_GE(t_ms.count(), 45);
  EXPECT_LE(t_ms.count(), 50);

  sl           = SearchLimits{};
  sl.whiteTime = 30s;
  sl.whiteInc  = 1s;
  sl.blackTime = 30s;
  sl.blackInc  = 1s;
  t_ms = search.setupTimeControl(p, sl);
  fprintln("{}", str(t_ms));
  EXPECT_GE(t_ms.count(), 1390);
  EXPECT_LE(t_ms.count(), 1420);
}

TEST_F(SearchTest, extraTime) {
  Search s{};
  s.searchLimits.timeControl = true;
  s.timeLimit                = 10s;
  s.extraTimeMs              = 0;
  s.addExtraTime(0.9);
  fprintln("{}", str(milliseconds(s.extraTimeMs.load())));
  EXPECT_EQ(-1000, s.extraTimeMs.load());
  s.extraTimeMs = 0;
  s.addExtraTime(1.2);
  fprintln("{}", str(milliseconds(s.extraTimeMs.load())));
  EXPECT_EQ(2000, s.extraTimeMs.load());
}

TEST_F(SearchTest, startTimer) {
  Search s{};
  s.searchLimits.timeControl = true;
  s.startTime                = high_resolution_clock::now();
  s.timeLimit                = 2s;
  s.extraTimeMs              = 1000; // 1s
  s.startTimer();
  s.timerThread.join();
  EXPECT_LT(3s, (high_resolution_clock::now() - s.startTime));
  EXPECT_GT(3.020s, (high_resolution_clock::now() - s.startTime));
}

TEST_F(SearchTest, startStopSearch) {
  SearchConfig::USE_BOOK = false;
  const Position p{};
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
  const Position p{};
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
  EXPECT_GT(1s - 20ms, MILLISECONDS(s.getLastSearchResult().time));
  // EXPECT_GT(1s * 1.1, MILLISECONDS(s.getLastSearchResult().time));
}

TEST_F(SearchTest, bookMoveSearch) {
  SearchConfig::USE_BOOK = true;
  const Position p{};
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
  const Position p{};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = milliseconds{1000};
  sl.ponder      = true;
  s.isReady();
  const TimePoint start = currentTime();
  s.startSearch(p, sl);
  EXPECT_TRUE(s.isSearching());
  EXPECT_FALSE(s.hasResult());
  SLEEP(nanoseconds(nanoPerSec));
  s.ponderhit();
  s.waitWhileSearching();
  EXPECT_TRUE(s.hasResult());
  // 0.85 from the root complexity calculation - 20ms tolerance for code run time
  EXPECT_LT(static_cast<int64_t>(0.85 * nanoPerSec - 20'000'000), s.getLastSearchResult().time.count());
  EXPECT_GT(static_cast<int64_t>(nanoPerSec * 1.1), s.getLastSearchResult().time.count());
  EXPECT_GT(static_cast<int64_t>(nanoPerSec * 2.5), elapsedSince(start).count());
}

TEST_F(SearchTest, startNodesLimitedSearch) {
  SearchConfig::USE_BOOK = false;
  const Position p{};
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
  EXPECT_LE(10'000'000, s.getLastSearchResult().nodes);
  EXPECT_GE(10'000'100, s.getLastSearchResult().nodes);
}

TEST_F(SearchTest, depthLimitedSearch) {
  SearchConfig::USE_BOOK = false;
  const Position p{};
  SearchLimits sl{};
  Search s{};
  constexpr int depth = 8;
  sl.depth            = depth;
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
  const Position p{"6R1/8/8/8/8/5K2/R7/7k b - -"};
  SearchLimits sl{};
  Search s{};
  constexpr int depth = 6;
  sl.depth            = depth;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  EXPECT_EQ(VALUE_DRAW, s.getLastSearchResult().bestMoveValue);
}

TEST_F(SearchTest, mate0Search) {
  SearchConfig::USE_BOOK = false;
  const Position p{"8/8/8/8/8/5K2/8/R4k2 b - -"};
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
  const Position p{"8/8/8/8/8/6K1/R7/6k1 w - - 0 8"};
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
  const Position p{"8/8/8/8/8/5K2/R7/7k w - - 0 7"};
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
  const Position p{"8/8/8/8/8/4K3/R7/6k1 w - - 0 6"};
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
  const Position p{"8/8/8/8/8/3K4/R7/5k2 w - - 0 5"};
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
  const Position p{"8/8/8/8/4K3/8/R7/4k3 w - - 0 4"};
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
  const Position position("r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1 w kq - 10 113");
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

TEST_F(SearchTest, movesLeftBucketsOpeningVsQueenlessVsLowMaterial) {
  // Same remaining time setup for all scenarios
  Search search{};
  SearchLimits sl{};
  sl.whiteTime = 30s;
  sl.blackTime = 30s;
  sl.whiteInc  = 0s;
  sl.blackInc  = 0s;

  // Opening-like position (start position with queens): should allocate the least per-move time
  Position opening{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
  const auto t_opening = search.setupTimeControl(opening, sl);

  // Queenless middlegame: fewer movesLeft bucket -> more per-move time than opening
  Position queenless{"rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1"};
  const auto t_queenless = search.setupTimeControl(queenless, sl);

  // Very low material endgame (KPK vs K): fewest movesLeft bucket -> largest per-move time
  Position lowMat{"8/8/8/8/8/4k3/4P3/4K3 w - - 0 1"};
  const auto t_lowMat = search.setupTimeControl(lowMat, sl);

  fprintln("Opening: {} Queenless: {} LowMat: {}", str(t_opening), str(t_queenless), str(t_lowMat));

  // Expect ordering of allocated time per move: opening < queenless < low material
  EXPECT_LT(t_opening, t_queenless);
  EXPECT_LT(t_queenless, t_lowMat);
}

TEST_F(SearchTest, movesLeftRepetitionRiskIncreasesTime) {
  // Use a queenless position to keep the bucket clear
  const Position queenless_low{"rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1"};
  const Position queenless_high{"rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 80 1"}; // high half-move clock

  const Search search{};
  SearchLimits sl{};
  sl.whiteTime = 30s;
  sl.blackTime = 30s;

  const auto t_lowHmc  = search.setupTimeControl(queenless_low, sl);
  const auto t_highHmc = search.setupTimeControl(queenless_high, sl);

  fprintln("Queenless hmc=0: {} hmc=80: {}", str(t_lowHmc), str(t_highHmc));

  // With high half-move clock repetition/50-move risk, movesLeft is reduced -> more time per move
  EXPECT_LT(t_lowHmc, t_highHmc);
}

TEST_F(SearchTest, singleMoveRootStopsEarlyAtVerifyDepth) {
  // Position with exactly one legal move for White: Kb1 only
  // FEN breakdown:
  // 8: r......k  (black rook at a8, black king at h8)
  // 2: ....q...  (black queen at e2 controlling b2 and helping restrict king moves)
  // 1: K.......  (white king at a1 in check along the a-file)
  const Position p{"r6k/8/8/8/8/8/4q3/K7 w - -"};
  println(p.strBoard());

  SearchConfig::USE_BOOK = false;
  Search s{};
  s.isReady();

  SearchLimits sl{};
  sl.timeControl = true; // no time control
  sl.whiteTime = 1000s;
  sl.blackTime = 1000s;

  s.startSearch(p, sl);
  s.waitWhileSearching();

  const auto result = s.getLastSearchResult();

  // With a single legal root move and no time control, iterative deepening stops after the first iteration
  EXPECT_LT(result.time, 10s);
  EXPECT_EQ(Move(SQ_A1, SQ_B1), result.bestMove);
}

TEST_F(SearchTest, singleMoveComplexRoot) {
  // More complex single-move position: only Bxf2 is legal for White
  // FEN: White to move, in check by Black knight on f2; queen on g3 controls g1, g2, h2
  const Position p{"k7/p7/8/8/8/6q1/P4n2/4B2K w - - 0 1"};
  println(p.strBoard());

  SearchConfig::USE_BOOK = false;
  Search s{};
  s.isReady();

  SearchLimits sl{};
  sl.timeControl = true;
  sl.whiteTime   = 1000s;
  sl.blackTime   = 1000s;

  s.startSearch(p, sl);
  s.waitWhileSearching();

  const auto result = s.getLastSearchResult();
  EXPECT_LT(result.time, 10s);
  EXPECT_EQ(Move(SQ_E1, SQ_F2), result.bestMove);
}

// New test: verify and pretty-print the LMR reduction table
TEST_F(SearchTest, lmrReductionTable) {
  // Access the private static table via FRIEND_TEST
  const auto& T = Search::LMR_REDUCTION;

  // Dimensions
  ASSERT_EQ(32u, T.size()) << "Depth dimension must be 32 (0..31)";
  for (const auto& row : T) ASSERT_EQ(64u, row.size()) << "Moves dimension must be 64 (0..63)";

  // Check exact formula match for all entries and basic boundary conditions
  for (int d = 0; d < 32; ++d) {
    for (int m = 0; m < 64; ++m) {
      const int expected = 1 + (d * m * 35 + 5000) / 10000; // exact rounding of 0.0035
      EXPECT_EQ(expected, T[d][m]) << "Mismatch at d=" << d << " m=" << m;

      // Minimum reduction is 1
      EXPECT_GE(T[d][m], 1) << "Reduction below 1 at d=" << d << " m=" << m;
    }
    // movesSearched == 0 must yield 1 for all depths
    EXPECT_EQ(1, T[d][0]) << "m=0 must be 1 at d=" << d;
  }
  for (int m = 0; m < 64; ++m) {
    // depth == 0 must yield 1 for all moves
    EXPECT_EQ(1, T[0][m]) << "d=0 must be 1 at m=" << m;
  }

  // Monotonicity: non-decreasing in both dimensions
  for (int d = 0; d < 32; ++d) {
    for (int m = 0; m + 1 < 64; ++m) {
      EXPECT_LE(T[d][m], T[d][m + 1]) << "Row not non-decreasing at d=" << d << " m=" << m;
    }
  }
  for (int m = 0; m < 64; ++m) {
    for (int d = 0; d + 1 < 32; ++d) {
      EXPECT_LE(T[d][m], T[d + 1][m]) << "Column not non-decreasing at m=" << m << " d=" << d;
    }
  }

  // Known extreme
  {
    const int expected = 1 + (31 * 63 * 35 + 5000) / 10000; // should be 8
    EXPECT_EQ(expected, T[31][63]);
  }

  // Pretty print the entire table for manual inspection
  std::ostringstream oss;
  oss << "LMR_REDUCTION[depth][moves] (depth 0..31, moves 0..63)\n";
  for (int d = 0; d < 32; ++d) {
    oss << "d=" << std::setw(2) << d << ": ";
    for (int m = 0; m < 64; ++m) {
      oss << std::setw(2) << T[d][m] << (m + 1 < 64 ? ' ' : '\n');
    }
  }
  LOG__INFO(Logger::get().TEST_LOG, "{}", oss.str());
}


TEST_F(SearchTest, debug) {
  if (isBulkRun()) {
    GTEST_SKIP() << "Skipping debug test in bulk run to save time";
  }
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

  const Position p{"2rr2k1/1p2qp1p/1pn1pp2/1N6/3P4/P6P/1P2QPP1/2R2RK1 w - - 0 1"};
  SearchLimits sl{};
  Search s{};
  //  sl.timeControl = true;
  //  sl.moveTime    = 160s;
  sl.depth = 6;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
}
