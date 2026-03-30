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

#include "engine/Search.h"
#include "Test_Fens.h"
#include "Test_Utils.h"
#include "common/CrashHandler.h"
#include "common/Logging.h"
#include "config/ConfigManager.h"
#include "init.h"
#include "types/types.h"

#include <iomanip>
#include <sstream>

#include <gtest/gtest.h>

using testing::Eq;

using namespace engine;
using namespace chess;
using namespace config;
using namespace common;


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
    Logger::get().CONFIG_LOG->set_level(spdlog::level::debug);

    // Install crash handler to generate minidumps on access violations
    crashhandler::install("./crash_dumps");
  }

  static void TearDownTestSuite() {
    crashhandler::uninstall();
  }

protected:
  void SetUp() override {
    ConfigManager::instance().resetToDefaults();
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
  CONFIG_OVERRIDE(s.TT_SIZE_MB = 1024;);
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
  auto t_ms    = search.setupTimeControl(p, sl);
  fprintln("{}", str(t_ms));
  EXPECT_GE(t_ms.count(), 600);
  EXPECT_LE(t_ms.count(), 660);

  sl           = SearchLimits{};
  sl.whiteTime = 3s;
  sl.blackTime = 3s;
  t_ms         = search.setupTimeControl(p, sl);
  fprintln("{}", str(t_ms));
  EXPECT_GE(t_ms.count(), 45);
  EXPECT_LE(t_ms.count(), 50);

  sl           = SearchLimits{};
  sl.whiteTime = 30s;
  sl.whiteInc  = 1s;
  sl.blackTime = 30s;
  sl.blackInc  = 1s;
  t_ms         = search.setupTimeControl(p, sl);
  fprintln("{}", str(t_ms));
  EXPECT_GE(t_ms.count(), 1390);
  EXPECT_LE(t_ms.count(), 1420);
}

TEST_F(SearchTest, extraTime) {
  Search s{};
  s.searchLimits.timeControl = true;
  s.searchLimits.whiteTime   = 120s; // set clock high enough so clock cap doesn't interfere
  s.searchLimits.blackTime   = 120s;
  s.measuredPostStopOverheadMs = 0;  // disable overhead for pure extra-time testing
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

// In production, MAX_EXTRA_TIME_FACTOR is CONFIG_CONST (frozen) — cannot override.
TEST_F(SearchTest, extraTimeCap) {
  // Test that extra time is capped at MAX_EXTRA_TIME_FACTOR * base time
#ifndef FRANKYCPP_PRODUCTION
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.MAX_EXTRA_TIME_FACTOR = 2.0;);
#endif
  EXPECT_EQ(ConfigManager::instance().search().MAX_EXTRA_TIME_FACTOR, 2.0) << "Test assumes MAX_EXTRA_TIME_FACTOR is 2.0";

  Search s{};
  s.searchLimits.timeControl = true;
  s.searchLimits.whiteTime   = 120s; // set clock high enough so clock cap doesn't interfere
  s.searchLimits.blackTime   = 120s;
  s.measuredPostStopOverheadMs = 0;  // disable overhead for pure extra-time testing
  s.timeLimit                = 10s;
  s.extraTimeMs              = 0;

  // Add 30% five times = would be 150% without cap
  for (int i = 0; i < 5; i++) {
    s.addExtraTime(1.3); // +30% = +3s each
  }

  // Should be capped at 2x base (20s), not 5x3s = 15s
  // Actually 5 * 3s = 15s < 20s cap, so should be 15s
  fprintln("After 5x +30%: extra = {}", str(milliseconds(s.extraTimeMs.load())));
  EXPECT_EQ(15000, s.extraTimeMs.load()); // 5 * 3000ms = 15000ms

  // Add more - should hit the cap
  for (int i = 0; i < 5; i++) {
    s.addExtraTime(1.3); // +30% = +3s each, but capped
  }

  // Should be capped at 20s (2x base)
  fprintln("After 10x +30%: extra = {}", str(milliseconds(s.extraTimeMs.load())));
  EXPECT_EQ(20000, s.extraTimeMs.load()); // capped at 2x base

  // Total budget should be base + cap = 30s
  const auto totalBudget = s.timeLimit + milliseconds(s.extraTimeMs.load());
  fprintln("Total budget: {}", str(totalBudget));
  EXPECT_EQ(30s, totalBudget);
}

// Test that extra time is capped against remaining clock time (not just MAX_EXTRA_TIME_FACTOR)
TEST_F(SearchTest, extraTimeClockCap) {
  Search s{};
  s.searchLimits.timeControl = true;
  s.searchLimits.whiteTime   = 15s; // only 15s on clock
  s.searchLimits.blackTime   = 15s;
  s.measuredPostStopOverheadMs = 10; // 10ms overhead
  s.timeLimit                = 5s;   // 5s base time per move
  s.extraTimeMs              = 0;

  // MAX_EXTRA_TIME_FACTOR cap would be 2.0 * 5s = 10s
  // But clock cap is: 15000 - 10 - 5000 = 9990ms
  // So clock cap (9990ms) is tighter than factor cap (10000ms)
  for (int i = 0; i < 10; i++) {
    s.addExtraTime(1.5); // +50% = +2.5s each
  }

  // Should be capped at clock cap (9990ms), not factor cap (10000ms)
  fprintln("Extra with clock cap: {}", str(milliseconds(s.extraTimeMs.load())));
  EXPECT_EQ(9990, s.extraTimeMs.load());

  // Total budget (base + extra) should not exceed clock time minus overhead
  const auto total = s.timeLimit.count() + s.extraTimeMs.load();
  EXPECT_LE(total, 15000 - 10) << "Total budget must not exceed remaining clock minus overhead";
}

TEST_F(SearchTest, startTimer) {
  Search s{};
  s.searchLimits.timeControl = true;
  s.startTime                = high_resolution_clock::now();
  s.startSearchTime          = s.startTime; // Timer uses startSearchTime for elapsed calculation
  s.timeLimit                = 2s;
  s.extraTimeMs              = 1000; // 1s
  // Timer subtracts measuredPostStopOverheadMs from budget.
  // Set to 0 to test pure timer behavior without overhead compensation.
  s.measuredPostStopOverheadMs = 0;
  s.startTimer();
  s.timerThread.join();
  EXPECT_LT(3s, (high_resolution_clock::now() - s.startTime));
  EXPECT_GT(3.020s, (high_resolution_clock::now() - s.startTime));
}

TEST_F(SearchTest, startTimerWithOverhead) {
  // Timer should fire early by measuredPostStopOverheadMs to leave room for post-stop work
  Search s{};
  s.searchLimits.timeControl   = true;
  s.startTime                  = high_resolution_clock::now();
  s.startSearchTime            = s.startTime;
  s.timeLimit                  = 2s;
  s.extraTimeMs                = 0;
  s.measuredPostStopOverheadMs = 100; // 100ms overhead
  s.startTimer();
  s.timerThread.join();
  const auto elapsed = high_resolution_clock::now() - s.startTime;
  // Timer should fire at ~1900ms (2000ms - 100ms overhead)
  EXPECT_LT(1.890s, elapsed);
  EXPECT_GT(1.920s, elapsed);
  // Timer should have set stoppedByTimer
  EXPECT_TRUE(s.stoppedByTimer);
}

TEST_F(SearchTest, startStopSearch) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
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
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
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
  EXPECT_GT(1s + 10ms, MILLISECONDS(s.getLastSearchResult().time));
}

TEST_F(SearchTest, bookMoveSearch) {
  CONFIG_OVERRIDE(s.USE_BOOK = true;);
  const Position p{};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = 1s;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  EXPECT_TRUE(s.hasResult());
  EXPECT_NE(MOVE_NONE, s.getLastSearchResult().bestMove);
  EXPECT_TRUE(s.getLastSearchResult().bookMove);
  EXPECT_LT(100us, s.getLastSearchResult().time);
  EXPECT_GT(500ms, s.getLastSearchResult().time);
}

TEST_F(SearchTest, startPonderSearch) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.USE_PONDER = true;);
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
  EXPECT_GT(static_cast<int64_t>(nanoPerSec * 1.3), s.getLastSearchResult().time.count());
  EXPECT_GT(static_cast<int64_t>(nanoPerSec * 2.5), elapsedSince(start).count());
}

TEST_F(SearchTest, startNodesLimitedSearch) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 1;); // Single-threaded for predictable node limit behavior
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

  const auto totalNodes = s.getLastSearchResult().nodes;

  fprintln("Node-limited search: total={:L}, limit={:L}", totalNodes, sl.nodes);

  // With single thread, total nodes should be close to limit (slight overshoot from batch checking)
  EXPECT_GE(totalNodes, sl.nodes) << "Should reach node limit";
  EXPECT_LE(totalNodes, sl.nodes * 1.1) << "Should not overshoot limit significantly";
}

TEST_F(SearchTest, depthLimitedSearch) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
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
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
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
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
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
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
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
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
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
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
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
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
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

// In production, USE_ALPHABETA is CONFIG_CONST — cannot override.
TEST_F(SearchTest, mate5Search) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  EXPECT_EQ(ConfigManager::instance().search().USE_BOOK, false);
  EXPECT_EQ(ConfigManager::instance().search().USE_ALPHABETA, true);
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

// ============================================================================
// Mate Score Stability Tests
// Regression tests for mate score stability issues fixed in v1.6:
// - Step 1: NMP mate clamping (commit 4559e2e) — VALUE_CHECKMATE_THRESHOLD
//   contamination eliminated
// - Step 2: Aspiration rewrite — while(true) loop with exponential widening,
//   value-centered re-search, mate bypass, fixed UCI display
//
// These positions previously exhibited:
// - Bogus near-mate scores (+98.71 / -98.52 pawns)
// - Confirmed mates getting "lost" between iterations
// - Severe score oscillation (mate → near-mate → regular → mate)
//
// The tests verify that the final result is a checkmate score and that the
// artificial VALUE_CHECKMATE_THRESHOLD (9871 cp) does not appear as a result.
// They also serve as visual regression tests — run with verbose output to
// inspect the search trace for score stability across iterations.
// ============================================================================

// KQB vs K — Black has overwhelming material advantage.
// Previously: Found M7 at depth 4, DROPPED to +18.40 at depth 5, recovered at depth 10.
// After fix: Must find checkmate and hold it stable.
TEST_F(SearchTest, mateStability_KQBvsK) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 1;);
  const Position p{"8/8/3k1b2/8/5K2/8/8/4q3 b - - 55 193"};
  SearchLimits sl{};
  Search s{};
  sl.depth = 15;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  const auto result = s.getLastSearchResult();

  // Must find a checkmate, not a regular evaluation
  EXPECT_TRUE(result.bestMoveValue.isCheckMate())
    << "Expected checkmate score, got: " << result.bestMoveValue.str();

  // Must NOT be the artificial VALUE_CHECKMATE_THRESHOLD (NMP clamping artifact)
  EXPECT_NE(std::abs(static_cast<int>(result.bestMoveValue)),
            static_cast<int>(VALUE_CHECKMATE_THRESHOLD))
    << "Score must not be VALUE_CHECKMATE_THRESHOLD (9871 cp = +98.71 pawns artifact)";
}

// KQ vs K — Black has queen vs lone king.
// Previously: Wild oscillation through +98.71, +M44, +18.40 before finding +M7 at depth 21.
// After fix: Must find checkmate without VALUE_CHECKMATE_THRESHOLD artifacts.
TEST_F(SearchTest, mateStability_KQvsK) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 1;);
  const Position p{"4q3/k7/8/3K4/8/8/8/8 b - - 34 227"};
  SearchLimits sl{};
  Search s{};
  sl.depth = 20;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  const auto result = s.getLastSearchResult();

  // Must find a checkmate
  EXPECT_TRUE(result.bestMoveValue.isCheckMate())
    << "Expected checkmate score, got: " << result.bestMoveValue.str();

  // Must NOT be the artificial VALUE_CHECKMATE_THRESHOLD
  EXPECT_NE(std::abs(static_cast<int>(result.bestMoveValue)),
            static_cast<int>(VALUE_CHECKMATE_THRESHOLD))
    << "Score must not be VALUE_CHECKMATE_THRESHOLD (9871 cp = +98.71 pawns artifact)";
}

// KQB vs K — Position from live game where +89.98 / +89.99 artifacts appeared.
// After fix: Must find checkmate without artificial high non-mate scores.
TEST_F(SearchTest, mateStability_KQBvsK_liveGame) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 1;);
  const Position p{"8/8/3k1b2/4q3/8/3K4/8/8 b - - 69 200"};
  SearchLimits sl{};
  Search s{};
  sl.depth = 15;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  const auto result = s.getLastSearchResult();

  // Must find a checkmate
  EXPECT_TRUE(result.bestMoveValue.isCheckMate())
    << "Expected checkmate score, got: " << result.bestMoveValue.str();

  // Must NOT be the artificial VALUE_CHECKMATE_THRESHOLD
  EXPECT_NE(std::abs(static_cast<int>(result.bestMoveValue)),
            static_cast<int>(VALUE_CHECKMATE_THRESHOLD))
    << "Score must not be VALUE_CHECKMATE_THRESHOLD (9871 cp = +98.71 pawns artifact)";
}

// KQp vs KR endgame — Position where fail-soft NMP returned inflated values (8998 cp
// = +89.98 pawns). Every aspiration iteration showed +89.98 fail-high before resolving
// to +21.87. After fail-hard NMP fix, no inflated scores should appear.
// This is a visual regression test — run with verbose output to inspect the trace.
TEST_F(SearchTest, nmpInflation_KQpVsKR) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 1;);
  const Position p{"3bk3/8/8/4K3/8/4R3/p7/8 b - - 0 156"};
  SearchLimits sl{};
  Search s{};
  sl.depth = 20;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
  const auto result = s.getLastSearchResult();

  // Score must be positive (Black is winning with a passed pawn + bishop vs rook)
  EXPECT_GT(static_cast<int>(result.bestMoveValue), 0)
    << "Expected positive score for Black, got: " << result.bestMoveValue.str();

  // Score must NOT be an inflated NMP artifact (8998 cp = +89.98 or similar)
  EXPECT_LT(std::abs(static_cast<int>(result.bestMoveValue)), 5000)
    << "Score is suspiciously high for a non-mate position: " << result.bestMoveValue.str();
}

// In production, USE_ALPHABETA/USE_PVS/USE_TT/USE_QUIESCENCE/USE_QS_SEE are CONFIG_CONST.
#ifndef FRANKYCPP_PRODUCTION
TEST_F(SearchTest, quiescenceTest) {

  Search search;
  SearchLimits searchLimits;
  const Position position("r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1 w kq - 10 113");
  searchLimits.depth = 2;

  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.USE_ALPHABETA = false;);
  CONFIG_OVERRIDE(s.USE_PVS = false;);
  CONFIG_OVERRIDE(s.USE_TT = false;);
  CONFIG_OVERRIDE(s.USE_QUIESCENCE = false;);
  CONFIG_OVERRIDE(s.USE_QS_SEE = false;);

  search.startSearch(position, searchLimits);
  search.waitWhileSearching();
  auto nodes1 = search.getLastSearchResult().nodes;
  auto extra1 = search.getSearchStats().currentExtraSearchDepth;

  CONFIG_OVERRIDE(s.USE_QUIESCENCE = true;);
  search.startSearch(position, searchLimits);
  search.waitWhileSearching();
  auto nodes2 = search.getLastSearchResult().nodes;
  auto extra2 = search.getSearchStats().currentExtraSearchDepth;

  CONFIG_OVERRIDE(s.USE_QS_SEE = true;);
  search.startSearch(position, searchLimits);
  search.waitWhileSearching();
  auto nodes3 = search.getLastSearchResult().nodes;
  auto extra3 = search.getSearchStats().currentExtraSearchDepth;

  LOG__INFO(Logger::get().TEST_LOG, "Nodes without Quiescence: {:L} Nodes with Quiescence: {:L} Nodes with SEE: {:L}", nodes1, nodes2, nodes3);
  LOG__INFO(Logger::get().TEST_LOG, "Extra without Quiescence: {:L} Extra with Quiescence: {:L} Extra with SEE: {:L}", extra1, extra2, extra3);

  ASSERT_GT(nodes2, nodes1);
  ASSERT_GT(extra2, extra1);
}
#endif // FRANKYCPP_PRODUCTION

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

  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  Search s{};
  s.isReady();

  SearchLimits sl{};
  sl.timeControl = true; // no time control
  sl.whiteTime   = 1000s;
  sl.blackTime   = 1000s;

  s.startSearch(p, sl);
  s.waitWhileSearching();

  const auto result = s.getLastSearchResult();

  EXPECT_LT(result.time, 10s);
  EXPECT_EQ(Move(SQ_A1, SQ_B1), result.bestMove);
}

TEST_F(SearchTest, singleMoveComplexRoot) {
  // More complex single-move position: only Bxf2 is legal for White
  // FEN: White to move, in check by Black knight on f2; queen on g3 controls g1, g2, h2
  const Position p{"k7/p7/8/8/8/6q1/P4n2/4B2K w - - 0 1"};
  println(p.strBoard());

  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  Search s{};
  s.isReady();

  SearchLimits sl{};
  sl.timeControl = true;
  sl.whiteTime   = 1200s;
  sl.blackTime   = 1200s;

  s.startSearch(p, sl);
  s.waitWhileSearching();

  const auto result = s.getLastSearchResult();
  EXPECT_LT(result.time, 35s);
  EXPECT_EQ(Move(SQ_E1, SQ_F2), result.bestMove);
}

// In production, LMR_USE_LOG_FORMULA and LMR_LOG_BASE_DIV are CONFIG_CONST — cannot override.
#ifndef FRANKYCPP_PRODUCTION
// New test: verify and pretty-print the LMR reduction table
TEST_F(SearchTest, lmrReductionTableTest) {
  Search search{};
  search.mainThread().regenerateLmrTable(false, 0.0 /* divisor isn't used for linear */);

  const auto& T = search.mainThread().LMR_REDUCTION;

  // Dimensions
  ASSERT_EQ(32U, T.size()) << "Depth dimension must be 32 (0..31)";
  for (const auto& row : T) ASSERT_EQ(64U, row.size()) << "Moves dimension must be 64 (0..63)";

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
    constexpr int expected = 1 + (31 * 63 * 35 + 5000) / 10000; // should be 8
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
#endif // FRANKYCPP_PRODUCTION

TEST_F(SearchTest, lmrReductionTablePrint) {

  // Access the private static table via FRIEND_TEST
  // Using new defaults: logarithmic formula with divisor 1.50
#ifndef FRANKYCPP_PRODUCTION
  CONFIG_OVERRIDE(s.LMR_USE_LOG_FORMULA = true;);
  CONFIG_OVERRIDE(s.LMR_LOG_BASE_DIV = 1.25;);
#endif

  EXPECT_EQ(ConfigManager::instance().search().LMR_USE_LOG_FORMULA, true) << "Test assumes LMR_USE_LOG_FORMULA is true";
  EXPECT_EQ(ConfigManager::instance().search().LMR_LOG_BASE_DIV, 1.25) << "Test assumes LMR_LOG_BASE_DIV is 1.50";

  Search search{};
  const auto& cfg = ConfigManager::instance().search();
  search.mainThread().regenerateLmrTable(cfg.LMR_USE_LOG_FORMULA, cfg.LMR_LOG_BASE_DIV);

  const auto& T = search.mainThread().LMR_REDUCTION;

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

// IN PRDO there is not stats for singular extensions, and the config is frozen,
// so these tests are development-only.
#ifndef FRANKYCPP_PRODUCTION
TEST_F(SearchTest, singularExtension) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);

  // Enable singular extensions and set params suitable for testing
  CONFIG_OVERRIDE(s.USE_SINGULAR_EXT = true;);
  CONFIG_OVERRIDE(s.SINGULAR_MIN_DEPTH = 6;); // Lower for testing
  CONFIG_OVERRIDE(s.SINGULAR_MARGIN = 64;);
  CONFIG_OVERRIDE(s.SINGULAR_REDUCTION = 3;);

  // Test that singular extensions work correctly
  // Use a complex middlegame position where singular extensions can trigger
  Search search{};
  search.isReady();

  // Complex middlegame position with tactical possibilities
  // This position has multiple candidate moves and requires deep search
  const Position p{"r1bq1rk1/pp2ppbp/2np1np1/8/3NP3/2N1BP2/PPPQ2PP/R3KB1R w KQ - 0 9"};

  SearchLimits sl{};
  sl.depth = 16; // Deep enough to trigger singular extension
  // sl.timeControl = true;
  // sl.moveTime    = 30s;

  search.startSearch(p, sl);
  search.waitWhileSearching();

  const auto& stats = search.getSearchStats();
  fprintln("Singular searches: {}", stats.singularSearches);
  fprintln("Singular extensions: {}", stats.singularExtension);

  // We expect some singular extension activity at depth 12
  // The exact numbers depend on the position, but there should be some
  EXPECT_GT(stats.singularSearches, 0) << "Expected some singular extension searches";
}

TEST_F(SearchTest, singularExtensionDisabled) {
  // Test that disabling singular extensions works
  Search search{};
  search.isReady();

  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.USE_SINGULAR_EXT = false;);

  // Same complex middlegame position as the enabled test
  const Position p{"r1bq1rk1/pp2ppbp/2np1np1/8/3NP3/2N1BP2/PPPQ2PP/R3KB1R w KQ - 0 9"};

  SearchLimits sl{};
  sl.depth = 16;

  search.startSearch(p, sl);
  search.waitWhileSearching();

  const auto& stats = search.getSearchStats();
  fprintln("Singular searches (disabled): {}", stats.singularSearches);
  fprintln("Singular extensions (disabled): {}", stats.singularExtension);

  // With singular extensions disabled, we should have no activity
  EXPECT_EQ(stats.singularSearches, 0) << "Expected no singular searches when disabled";
  EXPECT_EQ(stats.singularExtension, 0) << "Expected no singular extensions when disabled";
}
#endif // FRANKYCPP_PRODUCTION

TEST_F(SearchTest, 10secondSearchNodesCount) {
  if (isBulkRun()) {
    GTEST_SKIP() << "Skipping debug test in bulk run to save time";
  }

  // Used to experiment with multiple threads
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  // CONFIG_OVERRIDE(s.THREADS = 1;);
  CONFIG_OVERRIDE(s.TT_SIZE_MB = 512;);
  CONFIG_OVERRIDE(e.PAWN_TT_SIZE_MB = 16;);

  const Position p{"5k2/1rn2p2/3pb1p1/7p/p3PP2/PnNBK2P/3N2P1/1R6 w - - 0 1"};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = 10s;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();

  const auto result = s.getLastSearchResult();
  fprintln("Search completed in {:L} ms, nodes: {:L}", result.time.count(), result.nodes);

  fprintln("{}", s.formatDetailedStats());
}

// Verifies that newGame() fully resets Search state so that
// repeated depth-limited searches on the same position produce
// identical, deterministic results. A depth-limited search has
// no timing or random factors, so any difference in node count,
// best move, score, or statistics between runs indicates stale
// state leaking across searches.
//
// NOTE: This test MUST use single-threaded mode (THREADS=1) because
// multi-threaded Lazy SMP search is inherently non-deterministic:
// - Thread scheduling varies between runs
// - TT entries are written by different threads at different times
// - This affects move ordering and pruning decisions differently each run
// This non-determinism is expected and acceptable for SMP - the trade-off
// is speed vs determinism. See SearchSmpTest for multi-threaded tests.
TEST_F(SearchTest, newGameResetsDeterministic) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 1;); // Single-threaded REQUIRED for determinism

  // Use multiple positions to increase coverage
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::vector<std::string> fens = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",         // start position
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", // Kiwi Pete
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                            // endgame
  };

  // ReSharper disable once CppTooWideScope
  constexpr int searchDepth = 8;
  // ReSharper disable once CppTooWideScope
  constexpr int numIterations = 3;

  Search search{};
  search.isReady();

  for (const auto& fen : fens) {
    const Position position(fen);
    SearchLimits sl{};
    sl.depth = searchDepth;

    // Run the first search to establish reference values
    search.newGame();
    search.startSearch(position, sl);
    search.waitWhileSearching();
    ASSERT_TRUE(search.hasResult()) << "First search must produce a result for: " << fen;

    const auto refNodes    = search.getLastSearchResult().nodes;
    const auto refMove     = search.getLastSearchResult().bestMove;
    const auto refValue    = search.getLastSearchResult().bestMoveValue;
    const auto refDepth    = search.getLastSearchResult().depth;
    const auto& refStats   = search.getSearchStats();
    const auto refPvNodes  = refStats.pvNodes;
    const auto refNonPv    = refStats.nonPvNodes;
    const auto refBetaCuts = refStats.betaCuts;
    const auto refQsNodes  = refStats.qsearchNodes;

    fprintln("Position: {}", fen);
    fprintln("  Reference: nodes={}, move={}, value={}, depth={}",
             refNodes, refMove.str(), refValue.str(), refDepth);

    // Run subsequent searches and verify they match exactly
    for (int i = 1; i < numIterations; ++i) {
      search.newGame();
      search.startSearch(position, sl);
      search.waitWhileSearching();
      ASSERT_TRUE(search.hasResult()) << "Search iteration " << i << " must produce a result";

      const auto& result = search.getLastSearchResult();
      const auto& stats  = search.getSearchStats();

      fprintln("  Run {}: nodes={}, move={}, value={}, depth={}",
               i, result.nodes, result.bestMove.str(), result.bestMoveValue.str(), result.depth);

      // Core determinism checks
      EXPECT_EQ(refNodes, result.nodes)
        << "Node count differs on iteration " << i << " for: " << fen;
      EXPECT_EQ(refMove, result.bestMove)
        << "Best move differs on iteration " << i << " for: " << fen;
      EXPECT_EQ(refValue, result.bestMoveValue)
        << "Best move value differs on iteration " << i << " for: " << fen;
      EXPECT_EQ(refDepth, result.depth)
        << "Search depth differs on iteration " << i << " for: " << fen;

      // Statistics determinism checks
      // In PRODUCTION some of these values will always be 0 or not tracked, so
      // checking them isn't meaningful.
      EXPECT_EQ(refPvNodes, stats.pvNodes)
        << "PV node count differs on iteration " << i << " for: " << fen;
      EXPECT_EQ(refNonPv, stats.nonPvNodes)
        << "Non-PV node count differs on iteration " << i << " for: " << fen;
      EXPECT_EQ(refBetaCuts, stats.betaCuts)
        << "Beta cut count differs on iteration " << i << " for: " << fen;
      EXPECT_EQ(refQsNodes, stats.qsearchNodes)
        << "QSearch node count differs on iteration " << i << " for: " << fen;
    }
  }
}

// =============================================================================
// Movetime Compliance Test
// Verifies that search time never significantly exceeds the allocated movetime.
// This test was created to diagnose time management issues observed during
// self-play data generation for Texel tuning (100% time losses at fast TCs).
// =============================================================================
TEST_F(SearchTest, moveTimeCompliance) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 1;);

  constexpr int NUM_POSITIONS = 100;
  constexpr auto MOVETIME     = 500ms;
  // Allow some tolerance for thread scheduling, timer granularity, etc.
  // But flag anything more than 50ms over budget as a failure.
  constexpr auto TOLERANCE = 50ms;
  constexpr auto MAX_TIME  = MOVETIME + TOLERANCE;

  const auto fens           = Test_Fens::getFENs();
  const int positionsToTest = std::min(NUM_POSITIONS, static_cast<int>(fens.size()));

  Search search{};
  search.isReady();

  int violations      = 0;
  int maxDepthReached = 0;
  milliseconds maxTime{0};
  std::string worstFen;

  for (int i = 0; i < positionsToTest; ++i) {
    const Position pos{fens[i]};
    SearchLimits sl{};
    sl.timeControl = true;
    sl.moveTime    = MOVETIME;

    search.startSearch(pos, sl);
    search.waitWhileSearching();

    const auto& result    = search.getLastSearchResult();
    const auto searchTime = MILLISECONDS(result.time);

    if (result.depth > maxDepthReached) {
      maxDepthReached = result.depth;
    }

    if (searchTime > maxTime) {
      maxTime  = searchTime;
      worstFen = fens[i];
    }

    if (searchTime > MAX_TIME) {
      violations++;
      fprintln("VIOLATION [{}/{}]: {} - time={} depth={} move={}",
               i + 1, positionsToTest, fens[i],
               str(searchTime), result.depth, result.bestMove.str());
    }
  }

  fprintln("\nMovetime Compliance Summary:");
  fprintln("  Positions tested: {}", positionsToTest);
  fprintln("  Movetime budget:  {}", str(MOVETIME));
  fprintln("  Tolerance:        {}", str(TOLERANCE));
  fprintln("  Max time used:    {}", str(maxTime));
  fprintln("  Max depth:        {}", maxDepthReached);
  fprintln("  Violations:       {}/{}", violations, positionsToTest);
  if (!worstFen.empty()) {
    fprintln("  Worst position:   {}", worstFen);
  }

  EXPECT_EQ(violations, 0)
    << violations << " out of " << positionsToTest
    << " positions exceeded movetime budget of " << str(MAX_TIME)
    << ". Worst: " << str(maxTime) << " on: " << worstFen;
}

// In production, CONFIG_CONST search/eval configs cannot be overridden at runtime.
// These debug/diagnostic tests are development-only.
#ifndef FRANKYCPP_PRODUCTION
TEST_F(SearchTest, debug) {
  if (isBulkRun()) {
    GTEST_SKIP() << "Skipping debug test in bulk run to save time";
  }
  // CONFIG_OVERRIDE(s.TT_SIZE_MB = 64;);
  // CONFIG_OVERRIDE(s.USE_BOOK = false;);
  // CONFIG_OVERRIDE(s.USE_ALPHABETA = false;);
  // CONFIG_OVERRIDE(s.USE_PVS = false;);
  // CONFIG_OVERRIDE(s.USE_ASP = false;);
  // CONFIG_OVERRIDE(s.USE_TT = false;);
  // CONFIG_OVERRIDE(s.USE_TT_VALUE = false;);
  // CONFIG_OVERRIDE(s.USE_EVAL_TT = false;);
  // CONFIG_OVERRIDE(s.USE_MDP = false;);
  // CONFIG_OVERRIDE(s.USE_HISTORY_COUNTER = false;);
  // CONFIG_OVERRIDE(s.USE_HISTORY_MOVES = false;);
  // CONFIG_OVERRIDE(s.USE_QUIESCENCE = true;);
  // CONFIG_OVERRIDE(s.USE_QS_STANDPAT_CUT = false;);
  // CONFIG_OVERRIDE(s.USE_QS_SEE = false;);
  // CONFIG_OVERRIDE(s.USE_QS_TT = false;);
  //
  // ConfigManager::instance().applyOverrides([&](auto&, EvalConfigData& e) {
  //   e.USE_MATERIAL   = true;
  //   e.USE_POSITIONAL = true;
  //   e.TEMPO          = 34;
  // });

  const Position p{"4k3/4b3/8/4R3/2K5/8/8/3q4 b - - 15 164"};
  SearchLimits sl{};
  Search s{};
  CONFIG_OVERRIDE(s.TB_PATH = "D:/SYZYGY/");
  sl.timeControl = true;
  sl.moveTime    = 500ms;
  // sl.depth = 6;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();
}

#endif // FRANKYCPP_PRODUCTION
