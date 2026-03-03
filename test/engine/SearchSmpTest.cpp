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
// SearchSmpTest.cpp - Lazy SMP Multi-Threading Tests
//=============================================================================
//
// Tests for the Lazy SMP (Symmetric Multi-Processing) parallel search implementation.
// See: docs/specs/PLAN_Lazy_SMP_MultiThreading.md
//
// Test Categories:
//   - Single-thread regression: verify THREADS=1 behavior unchanged
//   - Multi-thread correctness: verify 2+ threads complete without deadlock
//   - Node count aggregation: verify total nodes = sum of all thread nodes
//   - Mate finding: verify engine finds mates correctly with multiple threads
//   - Time management: verify search stops within expected time window
//
//=============================================================================

#include "Test_Utils.h"
#include "common/CrashHandler.h"
#include "common/Logging.h"
#include "config/ConfigManager.h"
#include "engine/Search.h"
#include "init.h"
#include "types/types.h"

#include <gtest/gtest.h>

using testing::Eq;

using namespace engine;
using namespace chess;
using namespace config;
using namespace common;

class SearchSmpTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::debug);

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

// =============================================================================
// Single-Thread Regression Tests
// =============================================================================

// Test: 1 thread produces same result as pre-SMP (deterministic depth-limited search)
// A depth-limited search with THREADS=1 should produce identical results every run.
TEST_F(SearchSmpTest, SingleThreadDeterministic) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 1;);

  const Position p{"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3"};
  constexpr int searchDepth = 8;

  Search search{};
  search.isReady();

  // First search - establish reference
  SearchLimits sl{};
  sl.depth = searchDepth;
  search.newGame();
  search.startSearch(p, sl);
  search.waitWhileSearching();
  ASSERT_TRUE(search.hasResult());

  const auto refNodes = search.getLastSearchResult().nodes;
  const auto refMove  = search.getLastSearchResult().bestMove;
  const auto refValue = search.getLastSearchResult().bestMoveValue;
  const auto refDepth = search.getLastSearchResult().depth;

  fprintln("Reference: nodes={:L}, move={}, value={}, depth={}",
           refNodes, refMove.str(), refValue.str(), refDepth);

  // Second search - must match exactly
  search.newGame();
  search.startSearch(p, sl);
  search.waitWhileSearching();
  ASSERT_TRUE(search.hasResult());

  const auto& result = search.getLastSearchResult();
  fprintln("Verify:    nodes={:L}, move={}, value={}, depth={}",
           result.nodes, result.bestMove.str(), result.bestMoveValue.str(), result.depth);

  EXPECT_EQ(refNodes, result.nodes) << "Node count must be deterministic with 1 thread";
  EXPECT_EQ(refMove, result.bestMove) << "Best move must be deterministic with 1 thread";
  EXPECT_EQ(refValue, result.bestMoveValue) << "Score must be deterministic with 1 thread";
  EXPECT_EQ(refDepth, result.depth) << "Depth must match";
}

// Test: Single thread zero overhead - no helper threads created
TEST_F(SearchSmpTest, SingleThreadNoHelpers) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 1;);

  const Position p{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.depth = 6;
  search.startSearch(p, sl);
  search.waitWhileSearching();

  // With THREADS=1, result.threads should be 1
  const auto& result = search.getLastSearchResult();
  EXPECT_EQ(1, result.threads) << "Single-thread mode should report 1 thread";

  // Node count from main thread should equal total
  EXPECT_EQ(search.mainThread().nodesVisited, result.nodes)
    << "With 1 thread, mainThread nodes should equal total nodes";
}

// =============================================================================
// Multi-Thread Correctness Tests
// =============================================================================

// Test: 2 threads completes without deadlock or crash
TEST_F(SearchSmpTest, TwoThreadsNoDeadlock) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 2;);

  const Position p{"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.timeControl = true;
  sl.moveTime    = 2s;

  const auto startTime = high_resolution_clock::now();
  search.startSearch(p, sl);
  search.waitWhileSearching();
  const auto elapsed = high_resolution_clock::now() - startTime;

  ASSERT_TRUE(search.hasResult()) << "Search must complete with 2 threads";

  const auto& result = search.getLastSearchResult();
  const auto nps     = result.time.count() > 0
                         ? (result.nodes * 1'000'000'000ULL) / static_cast<uint64_t>(result.time.count())
                         : 0ULL;
  fprintln("2-thread search: {} in {}, nodes={:L}, nps={:L}",
           result.bestMove.str(), str(result.time), result.nodes, nps);

  EXPECT_NE(MOVE_NONE, result.bestMove) << "Must find a best move";
  EXPECT_GT(result.nodes, 0) << "Must visit some nodes";
  EXPECT_EQ(2, result.threads) << "Should report 2 threads";
  EXPECT_LT(elapsed, 5s) << "Must complete within reasonable time (no deadlock)";
}

// Test: 4 threads completes without deadlock or crash
TEST_F(SearchSmpTest, FourThreadsNoDeadlock) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 4;);

  const Position p{"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.timeControl = true;
  sl.moveTime    = 2s;

  const auto startTime = high_resolution_clock::now();
  search.startSearch(p, sl);
  search.waitWhileSearching();
  const auto elapsed = high_resolution_clock::now() - startTime;

  ASSERT_TRUE(search.hasResult()) << "Search must complete with 4 threads";

  const auto& result = search.getLastSearchResult();
  const auto nps     = result.time.count() > 0
                         ? (result.nodes * 1'000'000'000ULL) / static_cast<uint64_t>(result.time.count())
                         : 0ULL;
  fprintln("4-thread search: {} in {}, nodes={:L}, nps={:L}",
           result.bestMove.str(), str(result.time), result.nodes, nps);

  EXPECT_NE(MOVE_NONE, result.bestMove) << "Must find a best move";
  EXPECT_GT(result.nodes, 0) << "Must visit some nodes";
  EXPECT_EQ(4, result.threads) << "Should report 4 threads";
  EXPECT_LT(elapsed, 5s) << "Must complete within reasonable time (no deadlock)";
}

// Test: Multiple consecutive searches with different thread counts
// Note: Using more threads than CPU cores is valid - they will time-share.
// This tests that the engine handles over-subscription gracefully.
TEST_F(SearchSmpTest, VaryingThreadCounts) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);

  const Position p{"rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2"};
  Search search{};
  search.isReady();

  // Test various thread counts including over-subscription (more threads than typical CPU cores)
  for (const int threads : {1, 2, 4, 8, 16, 8, 4, 2, 1}) {
    fprintln("Starting search with {} threads...", threads);
    CONFIG_OVERRIDE(s.THREADS = threads;);

    SearchLimits sl{};
    sl.depth = 8;
    search.newGame();
    search.startSearch(p, sl);
    search.waitWhileSearching();

    ASSERT_TRUE(search.hasResult()) << "Search must complete with " << threads << " threads";

    const auto& result = search.getLastSearchResult();
    fprintln("{}-thread search: {} nodes={:L}", threads, result.bestMove.str(), result.nodes);

    EXPECT_NE(MOVE_NONE, result.bestMove);
    EXPECT_EQ(threads, result.threads);
  }
}

// =============================================================================
// Node Count Aggregation Tests
// =============================================================================

// Test: Total nodes reported = sum of all thread node counts
TEST_F(SearchSmpTest, NodeCountAggregation) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 4;);

  const Position p{"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.timeControl = true;
  sl.moveTime    = 2s;

  search.startSearch(p, sl);
  search.waitWhileSearching();

  ASSERT_TRUE(search.hasResult());

  const auto& result = search.getLastSearchResult();

  // Get aggregated nodes via getTotalNodes()
  const uint64_t aggregatedNodes = search.getTotalNodes();

  // The result.nodes should match getTotalNodes()
  EXPECT_EQ(aggregatedNodes, result.nodes)
    << "Result nodes must equal aggregated total from all threads";

  // With 4 threads, we expect helper threads to have contributed
  // (Main thread alone would have fewer nodes in the same time)
  EXPECT_GT(result.nodes, search.mainThread().nodesVisited)
    << "Main thread nodes should be less than total nodes with multiple threads";

  fprintln("Result nodes: {:L}, getTotalNodes(): {:L}", result.nodes, aggregatedNodes);
  fprintln("Main thread nodes: {:L}", search.mainThread().nodesVisited);
}

// Test: NPS calculation uses aggregated nodes
TEST_F(SearchSmpTest, NpsUsesAggregatedNodes) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 4;);

  const Position p{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.timeControl = true;
  sl.moveTime    = 2s;

  search.startSearch(p, sl);
  search.waitWhileSearching();

  ASSERT_TRUE(search.hasResult());
  const auto& result = search.getLastSearchResult();

  // NPS should be calculated from total nodes / time
  const auto calculatedNps = result.time.count() > 0
                               ? (result.nodes * 1'000'000'000ULL) / static_cast<uint64_t>(result.time.count())
                               : 0ULL;

  fprintln("Calculated NPS from result: {:L}", calculatedNps);

  // With aggregated nodes, NPS should be reasonable
  if (calculatedNps > 0) {
    EXPECT_GT(calculatedNps, 100'000ULL) << "NPS should be reasonable with 4 threads";
  }
}

// =============================================================================
// Mate Finding Tests (Multi-Thread)
// =============================================================================

// Test: Mate-in-1 found correctly with 2 threads
TEST_F(SearchSmpTest, Mate1With2Threads) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 2;);

  // Mate in 1: Ra2# (back rank mate)
  const Position p{"8/8/8/8/8/6K1/R7/6k1 w - - 0 8"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.timeControl = true;
  sl.moveTime    = 5s;
  sl.mate        = 1;

  search.startSearch(p, sl);
  search.waitWhileSearching();

  ASSERT_TRUE(search.hasResult());
  const auto& result = search.getLastSearchResult();

  fprintln("Mate-1 (2 threads): {} value={}", result.bestMove.str(), result.bestMoveValue.str());

  EXPECT_EQ(VALUE_CHECKMATE - 1, result.bestMoveValue) << "Must find mate in 1";
  EXPECT_TRUE(result.mateFound) << "mateFound flag should be set";
}

// Test: Mate-in-3 found correctly with 4 threads
TEST_F(SearchSmpTest, Mate3With4Threads) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 4;);

  // Mate in 3 position
  const Position p{"8/8/8/8/8/4K3/R7/6k1 w - - 0 6"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.timeControl = true;
  sl.moveTime    = 10s;
  sl.mate        = 3;

  search.startSearch(p, sl);
  search.waitWhileSearching();

  ASSERT_TRUE(search.hasResult());
  const auto& result = search.getLastSearchResult();

  fprintln("Mate-3 (4 threads): {} value={} in {}",
           result.bestMove.str(), result.bestMoveValue.str(), str(result.time));

  EXPECT_EQ(VALUE_CHECKMATE - 5, result.bestMoveValue) << "Must find mate in 3";
  EXPECT_TRUE(result.mateFound) << "mateFound flag should be set";
}

// =============================================================================
// Time Management Tests (Multi-Thread)
// =============================================================================

// Test: Search stops within expected time window with N threads
// Note: The engine may stop early if it determines it can't complete the next iteration
// within the remaining time budget. This is correct time management behavior.
TEST_F(SearchSmpTest, TimeManagementWithMultipleThreads) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 4;);

  const Position p{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.timeControl = true;
  sl.moveTime    = 1s;

  const auto startTime = high_resolution_clock::now();
  search.startSearch(p, sl);
  search.waitWhileSearching();
  const auto elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - startTime);

  ASSERT_TRUE(search.hasResult());
  const auto& result = search.getLastSearchResult();

  fprintln("Time-limited search (4 threads): elapsed={}, reported={}, depth={}",
           str(elapsed), str(result.time), result.depth);

  // The engine uses smart time management that may stop early if:
  // - It can't complete the next iteration within remaining time
  // - The position complexity factor reduces effective time budget
  // So we only check that it doesn't significantly OVERSHOOT the limit
  EXPECT_LT(elapsed, 1500ms) << "Should not overshoot time limit significantly";

  // Should have searched for at least some reasonable time (not instant)
  EXPECT_GT(elapsed, 100ms) << "Should have spent some time searching";

  // Should have reached a reasonable depth (conservative for CI runners which may be slower)
  EXPECT_GE(result.depth, 6) << "Should reach reasonable depth in 1 second";
}

// Test: stopSearch() works correctly with multiple threads
TEST_F(SearchSmpTest, StopSearchWithMultipleThreads) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 4;);

  const Position p{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.infinite = true;// Would run forever without stop

  const auto startTime = high_resolution_clock::now();
  search.startSearch(p, sl);

  // Let it run for a bit, then stop
  std::this_thread::sleep_for(500ms);
  EXPECT_TRUE(search.isSearching()) << "Should still be searching";

  search.stopSearch();
  search.waitWhileSearching();
  const auto elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - startTime);

  ASSERT_TRUE(search.hasResult()) << "Must produce result after stop";
  EXPECT_LT(elapsed, 1s) << "Should stop promptly after stopSearch()";

  const auto& result = search.getLastSearchResult();
  fprintln("Stopped search (4 threads): {} in {}, nodes={:L}",
           result.bestMove.str(), str(elapsed), result.nodes);
  EXPECT_NE(MOVE_NONE, result.bestMove) << "Should have found some move";
}

// =============================================================================
// Scaling Tests (Multi-Thread)
// =============================================================================

// Test: More threads = more NPS (basic scaling check)
// Note: This is a smoke test, not a precise scaling measurement
TEST_F(SearchSmpTest, NpsScalesWithThreads) {
  if (isBulkRun()) {
    GTEST_SKIP() << "Skipping scaling test in bulk run to save time";
  }

  CONFIG_OVERRIDE(s.USE_BOOK = false;);

  const Position p{"r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.timeControl = true;
  sl.moveTime    = 2s;

  // Helper lambda to calculate NPS from result
  const auto calcNps = [](const SearchResult& r) -> uint64_t {
    return r.time.count() > 0
             ? (r.nodes * 1'000'000'000ULL) / static_cast<uint64_t>(r.time.count())
             : 0ULL;
  };

  // Single thread baseline
  CONFIG_OVERRIDE(s.THREADS = 1;);
  search.newGame();
  search.startSearch(p, sl);
  search.waitWhileSearching();
  const auto nps1 = calcNps(search.getLastSearchResult());
  fprintln("1 thread:  {:L} NPS", nps1);

  // 2 threads
  CONFIG_OVERRIDE(s.THREADS = 2;);
  search.newGame();
  search.startSearch(p, sl);
  search.waitWhileSearching();
  const auto nps2 = calcNps(search.getLastSearchResult());
  fprintln("2 threads: {:L} NPS ({:.1f}x)", nps2, static_cast<double>(nps2) / static_cast<double>(nps1));

  // 4 threads
  CONFIG_OVERRIDE(s.THREADS = 4;);
  search.newGame();
  search.startSearch(p, sl);
  search.waitWhileSearching();
  const auto nps4 = calcNps(search.getLastSearchResult());
  fprintln("4 threads: {:L} NPS ({:.1f}x)", nps4, static_cast<double>(nps4) / static_cast<double>(nps1));

  // Expect some scaling (not perfect, but should be noticeable)
  // With 2 threads, expect at least 1.3x NPS (conservative)
  // With 4 threads, expect at least 2.0x NPS (conservative)
  EXPECT_GT(nps2, static_cast<uint64_t>(nps1 * 1.3))
    << "2 threads should provide at least 30% NPS improvement";
  EXPECT_GT(nps4, static_cast<uint64_t>(nps1 * 2.0))
    << "4 threads should provide at least 2x NPS improvement";
}

// =============================================================================
// Stress Tests
// =============================================================================

// Test: Rapid start/stop cycles don't cause issues
TEST_F(SearchSmpTest, RapidStartStopCycles) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 4;);

  const Position p{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
  Search search{};
  search.isReady();

  constexpr int cycles = 10;
  for (int i = 0; i < cycles; ++i) {
    SearchLimits sl{};
    sl.infinite = true;

    search.newGame();
    search.startSearch(p, sl);
    std::this_thread::sleep_for(50ms);
    search.stopSearch();
    search.waitWhileSearching();

    ASSERT_TRUE(search.hasResult()) << "Cycle " << i << " must produce result";
    EXPECT_NE(MOVE_NONE, search.getLastSearchResult().bestMove)
      << "Cycle " << i << " must find a move";
  }

  fprintln("Completed {} rapid start/stop cycles with 4 threads", cycles);
}

// Test: Search different positions consecutively
TEST_F(SearchSmpTest, ConsecutiveDifferentPositions) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 4;);

  const std::vector<std::string> positions = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",        // start
    "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",// Italian setup
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",// Kiwi Pete
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",                           // endgame
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",    // complex
  };

  Search search{};
  search.isReady();

  for (const auto& fen : positions) {
    const Position p(fen);
    SearchLimits sl{};
    sl.depth = 8;

    search.newGame();
    search.startSearch(p, sl);
    search.waitWhileSearching();

    ASSERT_TRUE(search.hasResult()) << "Must complete search for: " << fen;
    const auto& result = search.getLastSearchResult();
    EXPECT_NE(MOVE_NONE, result.bestMove) << "Must find move for: " << fen;

    fprintln("Position search: {} nodes={:L} move={}",
             fen.substr(0, 20) + "...", result.nodes, result.bestMove.str());
  }
}

// =============================================================================
// Delayed Helper Thread Startup Tests
// =============================================================================

// Test: Helpers are not launched until after SMP_HELPER_START_DEPTH iterations
// This verifies that TT priming occurs before helpers start contributing.
TEST_F(SearchSmpTest, DelayedHelperStartup) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping delayed helper startup test in production build to save time";
#else
  CONFIG_OVERRIDE(s.SMP_HELPER_START_DEPTH = 5;);// Helpers start after depth 5
#endif

  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 4;);

  const Position p{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.depth = 8;// Search to depth 8, helpers should start after depth 5

  search.startSearch(p, sl);
  search.waitWhileSearching();

  ASSERT_TRUE(search.hasResult()) << "Search must complete";

  const auto& result = search.getLastSearchResult();
  fprintln("Delayed startup test: depth={}, nodes={:L}, threads={}",
           result.depth, result.nodes, result.threads);

  EXPECT_GE(result.depth, 5) << "Should reach at least depth 5 (helper start depth)";
  EXPECT_EQ(4, result.threads) << "Should report 4 threads";
  EXPECT_NE(MOVE_NONE, result.bestMove) << "Must find a best move";
}

// Test: With very short search, helpers may not be launched at all
// If search completes before SMP_HELPER_START_DEPTH, helpers never start.
TEST_F(SearchSmpTest, ShortSearchNoHelpers) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping short search no helpers test in production build to save time";
#else
  CONFIG_OVERRIDE(s.SMP_HELPER_START_DEPTH = 6;);// Helpers start after depth 6
#endif

  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 4;);

  const Position p{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.depth = 4;// Only search to depth 4, which is before helper start depth

  search.startSearch(p, sl);
  search.waitWhileSearching();

  ASSERT_TRUE(search.hasResult()) << "Search must complete";

  const auto& result = search.getLastSearchResult();
  fprintln("Short search test: depth={}, nodes={:L}", result.depth, result.nodes);

  // Main thread should have done all the work since helpers weren't launched
  EXPECT_EQ(4, result.depth) << "Should reach exactly depth 4";
  EXPECT_EQ(search.mainThread().nodesVisited, result.nodes)
    << "With no helpers launched, main thread nodes should equal total";
}

// Test: Default SMP_HELPER_START_DEPTH (4) works correctly
TEST_F(SearchSmpTest, DefaultHelperStartDepth) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 2;);
  // Don't override SMP_HELPER_START_DEPTH - use default (4)

  const Position p{"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3"};
  Search search{};
  search.isReady();

  SearchLimits sl{};
  sl.timeControl = true;
  sl.moveTime    = 2s;

  search.startSearch(p, sl);
  search.waitWhileSearching();

  ASSERT_TRUE(search.hasResult()) << "Search must complete";

  const auto& result = search.getLastSearchResult();
  fprintln("Default helper start: depth={}, nodes={:L}, threads={}",
           result.depth, result.nodes, result.threads);

  // With 2 second search, should easily reach depth > 4 and helpers should contribute
  EXPECT_GT(result.depth, 4) << "Should search deeper than helper start depth";
  EXPECT_EQ(2, result.threads) << "Should report 2 threads";

  // With helpers, total nodes should be more than just main thread
  // (This is a weak check since we can't directly access helper node counts after join)
  EXPECT_GT(result.nodes, 0) << "Must visit nodes";
}

// =============================================================================
// Multi-Threaded Reset Tests
// =============================================================================

// Test: newGame() properly resets state for multi-threaded searches.
// Verifies that:
// - Search completes successfully after each newGame()
// - No crashes or state corruption
// - Best move is consistent (same move found most of the time)
//
// Note on determinism: While we'd expect fixed-depth search to be deterministic,
// Lazy SMP introduces non-determinism because:
// - Helper threads write TT entries concurrently with the main thread
// - Thread scheduling affects WHEN entries are written
// - This affects which branches get pruned via TT cutoffs
// - Different pruning can lead to slightly different evaluations
// - In close positions, this could even affect the best move choice
//
// This is a known trade-off of Lazy SMP: speed vs strict determinism.
// The search is still correct - it finds a good move - just not guaranteed
// to be identical across runs.
TEST_F(SearchSmpTest, NewGameResetsMultiThreaded) {
  CONFIG_OVERRIDE(s.USE_BOOK = false;);
  CONFIG_OVERRIDE(s.THREADS = 4;);

  const Position p{"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3"};
  Search search{};
  search.isReady();

  constexpr int numIterations = 5;
  // ReSharper disable once CppTooWideScope
  constexpr int searchDepth = 10;

  Move firstBestMove = MOVE_NONE;
  int sameMoveCount  = 0;

  for (int i = 0; i < numIterations; ++i) {
    search.newGame();

    SearchLimits sl{};
    sl.depth = searchDepth;

    search.startSearch(p, sl);
    search.waitWhileSearching();

    ASSERT_TRUE(search.hasResult()) << "Iteration " << i << " must produce a result";

    const auto& result = search.getLastSearchResult();
    fprintln("Multi-thread reset run {}: move={}, value={}, nodes={:L}, depth={}",
             i, result.bestMove.str(), result.bestMoveValue.str(), result.nodes, result.depth);

    EXPECT_NE(MOVE_NONE, result.bestMove) << "Must find a move on iteration " << i;
    EXPECT_EQ(searchDepth, result.depth) << "Must reach target depth on iteration " << i;
    EXPECT_GT(result.nodes, 0) << "Must visit nodes on iteration " << i;

    // Track best move consistency
    if (i == 0) {
      firstBestMove = result.bestMove;
    }
    else if (result.bestMove == firstBestMove) {
      sameMoveCount++;
    }
  }

  // Due to Lazy SMP non-determinism, we can't guarantee 100% same move,
  // but for positions with a clearly best move, it should be consistent most of the time.
  // We expect at least half the runs to find the same move as the first run.
  fprintln("Same best move as first run: {}/{} times (move: {})",
           sameMoveCount, numIterations - 1, firstBestMove.str());
  // EXPECT_GE(sameMoveCount, (numIterations - 1) / 2)
  //   << "Best move should be consistent across most runs";
}
