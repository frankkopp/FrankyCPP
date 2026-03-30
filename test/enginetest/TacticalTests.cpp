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
// TacticalTests.cpp - Eigenmann Rapid Engine Test (ERET) Suite
//=============================================================================
//
// Individual tactical test positions from the Eigenmann Rapid Engine Test suite.
// Each position is a separate Google Test so failures are pinpointed to a single
// puzzle, and individual positions can be run in isolation for debugging.
//
// Usage:
//   # Run ALL ERET tests (takes ~55 min at 30s/position × 111 positions)
//   .\cmake-build-win-release\test\FrankyCPP_v1.6_Test.exe --gtest_filter=ERET.*
//
//   # Run a single test
//   .\cmake-build-win-release\test\FrankyCPP_v1.6_Test.exe --gtest_filter=ERET.ERET_001_Relief
//
//   # Run only endgame tests
//   .\cmake-build-win-release\test\FrankyCPP_v1.6_Test.exe --gtest_filter=ERET.*Endgame*
//
// Configuration knobs at the top of the file allow easy experimentation with:
//   - Search time, depth, thread count
//   - Search parameters (LMR, NMP, extensions, etc.)
//   - Eval parameters (mobility weights, pawn structure, king safety, etc.)
//
// Each test is skipped when running in bulk mode (isBulkRun()).
//=============================================================================

#include "Test_Utils.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"
#include "common/CrashHandler.h"
#include "common/Logging.h"
#include "config/ConfigManager.h"
#include "engine/Search.h"
#include "engine/SearchLimits.h"
#include "enginetest/TestSuite.h"
#include "init.h"
#include "version.h"

#include <gtest/gtest.h>

using namespace engine;
using namespace chess;
using namespace config;
using namespace common;

// =============================================================================
// Tunable test parameters — change these to experiment
// =============================================================================

// Search limits
static constexpr auto ERET_MOVE_TIME = seconds{30}; // time per position
static constexpr int ERET_MAX_DEPTH  = 0;           // 0 = no depth limit (time-only)
static constexpr int ERET_THREADS    = 4;           // number of search threads
static constexpr int ERET_TT_SIZE_MB = 256;         // transposition table size

// =============================================================================
// Experimental config overrides
// Uncomment / tweak individual lines to test different search or eval settings.
// After modifying, rebuild and run the suite to measure impact on solve rate.
// =============================================================================
static void applyExperimentalConfig() {
#ifndef FRANKYCPP_PRODUCTION
  // --- Search tuning knobs ---
  CONFIG_OVERRIDE_START()
  s.USE_BOOK   = false; // no book moves for tactical tests
  s.THREADS    = ERET_THREADS;
  s.TT_SIZE_MB = ERET_TT_SIZE_MB;

  // Core search algorithms
  s.USE_ALPHABETA = true;
  s.USE_PVS       = true;
  s.USE_ASP       = true;

  // Quiescence search
  s.USE_QUIESCENCE      = true;
  s.USE_QS_STANDPAT_CUT = true;
  s.USE_QS_SEE          = true;

  // Transposition table
  s.USE_TT              = true;
  s.TT_SIZE_MB          = 256;
  s.USE_TT_VALUE        = true;
  s.USE_TT_PV_MOVE_SORT = true;
  s.USE_QS_TT           = true;
  s.USE_EVAL_TT         = true;

  // Syzygy tablebase probing
  s.USE_TB_PROBE_ROOT   = true;
  s.TB_ROOT_IMMEDIATE   = true;
  s.USE_TB_PROBE_SEARCH = true;
  s.USE_TB_PROBE_PV     = true;

  // Move sorting
  s.USE_KILLER_MOVES    = true;
  s.USE_HISTORY_COUNTER = true;
  s.USE_HISTORY_MOVES   = true;

  s.USE_IIR = true;

  // Pruning techniques
  s.USE_IMPROVING = true; // Track if position is improving vs 2 plies ago

  s.USE_MDP      = true;
  s.USE_RAZORING = true;

  s.USE_RFP           = true;
  s.USE_RFP_IMPROVING = true;

  s.USE_NMP           = true;
  s.USE_NMP_VERIFY    = true;
  s.USE_NMP_IMPROVING = true;

  // Futility pruning
  s.USE_FP           = true;
  s.USE_QFP          = true;
  s.USE_FP_IMPROVING = true;

  // Late move reductions
  s.USE_LMR               = true;
  s.LMR_MIN_DEPTH         = 1;
  s.LMR_MIN_MOVES         = 3;
  s.LMR_USE_LOG_FORMULA   = true;
  s.LMR_LOG_BASE_DIV      = 2.00;
  s.USE_LMR_IMPROVING     = true;
  s.USE_LMR_CUTNODE       = true;
  s.LMR_CUTNODE_REDUCTION = 2;
  s.USE_LMR_HISTORY       = true;
  s.LMR_HISTORY_DIVISOR   = 8192;

  // Late move pruning
  s.USE_LMP           = true;
  s.USE_LMP_IMPROVING = true;
  s.LMP_MOVES = {0, 7, 9, 11, 13, 15, 17, 19, 22, 24, 27, 29, 32, 35, 38, 41};

  // Extensions
  s.USE_EXTENSIONS        = true;
  s.USE_CHECK_EXT         = true;
  s.USE_CHECK_EXT_SEE     = true;
  s.USE_THREAT_EXT        = true;
  s.USE_EXT_ADD_DEPTH     = true;
  s.USE_SINGULAR_EXT      = true;
  s.USE_SINGULAR_TT_BOUND = true;

  // Best-move instability time management (disable for fixed-depth tests)
  s.USE_BESTMOVE_INSTABILITY = false;
  s.USE_EVAL_VOLATILITY      = false;

  // ***************
  // Eval tuning knobs
  // ***************

  // master toggles
  e.USE_MATERIAL   = true;
  e.USE_POSITIONAL = true;

  // tempo
  e.USE_TEMPO = true;
  e.TEMPO     = 34;

  // lazy eval
  e.USE_LAZY_EVAL  = true;
  e.LAZY_THRESHOLD = 700;

  // pawn eval
  e.USE_PAWN_EVAL   = true;
  e.USE_PAWN_TT     = true;
  e.PAWN_TT_SIZE_MB = 16;

  // pawn structure weights
  e.ISOLATED_PAWN_MID_WEIGHT = -10;
  e.ISOLATED_PAWN_END_WEIGHT = -20;
  e.DOUBLED_PAWN_END_WEIGHT  = -30;
  e.PASSED_PAWN_MID_WEIGHT   = 20;
  e.PASSED_PAWN_END_WEIGHT   = 40;

  // Rank-based passed pawn bonus (indexed by relative rank 2..7, so array index 0..5).
  // Relative rank: for White = actual rank, for Black = 9 - actual rank.
  // When enabled, the rank bonus is added ON TOP of the flat PASSED_PAWN_*_WEIGHT.
  e.USE_PASSED_PAWN_RANK_BONUS = true;
  // Mid/End bonus per relative rank: {rank2, rank3, rank4, rank5, rank6, rank7}
  // Quadratic-ish scaling: low ranks add little, high ranks add substantially.
  e.PASSED_PAWN_RANK_MID_BONUS = {0, 0, 5, 15, 35, 70};
  e.PASSED_PAWN_RANK_END_BONUS = {0, 5, 15, 35, 70, 120};
  e.BLOCKED_PAWN_MID_WEIGHT    = -2;
  e.BLOCKED_PAWN_END_WEIGHT    = -20;
  e.PHALANX_PAWN_MID_WEIGHT    = 4;
  e.PHALANX_PAWN_END_WEIGHT    = 4;
  e.SUPPORTED_PAWN_MID_WEIGHT  = 10;
  e.SUPPORTED_PAWN_END_WEIGHT  = 15;

  // piece eval
  e.USE_PIECE_EVAL = true;

  // bishop pair
  e.USE_BISHOP_PAIR_BONUS = true;
  e.BISHOP_PAIR_MID_BONUS = 30;
  e.BISHOP_PAIR_END_BONUS = 45;

  // knight mobility
  e.USE_KNIGHT_MOBILITY          = true;
  e.KNIGHT_MOBILITY_MID_PER_MOVE = 3;
  e.KNIGHT_MOBILITY_END_PER_MOVE = 2;
  e.KNIGHT_LOW_MOBILITY_LEQ1_MID = -6;
  e.KNIGHT_LOW_MOBILITY_LEQ1_END = -6;

  // bishop mobility
  e.USE_BISHOP_MOBILITY          = true;
  e.BISHOP_MOBILITY_MID_PER_MOVE = 2;
  e.BISHOP_MOBILITY_END_PER_MOVE = 3;
  e.BISHOP_LOW_MOBILITY_LEQ3_END = -2;

  // rook mobility and files
  e.USE_ROOK_MOBILITY            = true;
  e.ROOK_MOBILITY_MID_PER_MOVE   = 2;
  e.ROOK_MOBILITY_END_PER_MOVE   = 2;
  e.USE_ROOK_OPEN_FILE_BONUS     = true;
  e.ROOK_OPEN_FILE_MID_BONUS     = 10;
  e.ROOK_OPEN_FILE_END_BONUS     = 8;
  e.ROOK_SEMIOPEN_FILE_MID_BONUS = 5;
  e.ROOK_SEMIOPEN_FILE_END_BONUS = 4;

  // rook on 7th rank (relative to its color)
  e.USE_ROOK_7TH_RANK_BONUS = true;
  e.ROOK_7TH_RANK_MID_BONUS = 15;
  e.ROOK_7TH_RANK_END_BONUS = 25;

  // queen
  e.USE_QUEEN_MOBILITY          = true;
  e.QUEEN_MOBILITY_MID_PER_MOVE = 1;
  e.QUEEN_MOBILITY_END_PER_MOVE = 1;
  e.USE_QUEEN_TROPISM           = true;
  e.QUEEN_TROPISM_MID_PER_STEP  = 0;
  e.QUEEN_TROPISM_END_PER_STEP  = 1;

  // king
  e.USE_KING_EVAL            = true;
  e.USE_KING_SAFETY_SHIELD   = true;
  e.KING_SHIELD_MID_PER_PAWN = 5;
  e.KING_SHIELD_END_PER_PAWN = 0;

  // king-pawn proximity in endgame
  // Bonus for king close to own passed pawns, bonus for king close to enemy passed pawns (defending).
  e.USE_KING_PAWN_PROXIMITY       = true;
  e.KING_OWN_PASSED_PROXIMITY_END = 5; // bonus per step of closeness to own passers
  e.KING_OPP_PASSED_PROXIMITY_END = 3; // bonus per step of closeness to enemy passers

  // king safety: attack evaluation (midgame only)
  // Counts attacker pieces on the enemy king zone and applies a non-linear penalty.
  e.USE_KING_SAFETY_ATTACK    = true;
  e.KING_ATTACK_WEIGHT_KNIGHT = 2;
  e.KING_ATTACK_WEIGHT_BISHOP = 2;
  e.KING_ATTACK_WEIGHT_ROOK   = 3;
  e.KING_ATTACK_WEIGHT_QUEEN  = 4;
  // Non-linear penalty table indexed by total attack weight (clamped to 0..15)
  e.KING_SAFETY_TABLE = {0, 0, 5, 15, 30, 50, 75, 105, 140, 180, 220, 260, 300, 340, 380, 400};

  e.USE_GAMEPHASE_VALUE = true;

  CONFIG_OVERRIDE_END();
#else
  LOG__WARN( Logger::get().TEST_LOG, "Running ERET tests with PRODUCTION config - no experimental overrides applied" );
#endif
}

// =============================================================================
// Test fixture
// =============================================================================

class ERET : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;

    ConfigManager::instance().resetToDefaults();

    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::info);
    Logger::get().TT_LOG->set_level(spdlog::level::info);
    Logger::get().EVAL_LOG->set_level(spdlog::level::info);
    Logger::get().UCI_LOG->set_level(spdlog::level::warn);
    Logger::get().TSUITE_LOG->set_level(spdlog::level::info);

    crashhandler::install("./crash_dumps");
  }

  static void TearDownTestSuite() {
    crashhandler::uninstall();
  }

protected:
  void SetUp() override {
    if (isBulkRun()) {
      GTEST_SKIP() << "Skipping ERET tactical test in bulk run";
    }
    ConfigManager::instance().resetToDefaults();
    applyExperimentalConfig();
  }

  void TearDown() override {}

  // -----------------------------------------------------------------------
  // Best-move test: engine must find one of the expected best moves
  // -----------------------------------------------------------------------
  static void runBestMove(const std::string& fen,
                          const std::vector<std::string>& expectedSan,
                          const std::string& id) {
    // Parse expected moves in the context of the position
    const Position pos{fen};
    MoveGenerator mg{};
    MoveList expectedMoves;
    for (const auto& san : expectedSan) {
      const Move m = mg.getMoveFromSan(pos, san);
      ASSERT_TRUE(m.isValid()) << "Could not parse SAN move '" << san << "' for " << id;
      expectedMoves.push_back(m);
    }

    // Set up search
    Search search{};
    SearchLimits sl{};
    sl.timeControl = true;
    sl.moveTime    = ERET_MOVE_TIME;
    if (ERET_MAX_DEPTH > 0) {
      sl.depth = ERET_MAX_DEPTH;
    }

    search.newGame();
    search.startSearch(pos, sl);
    search.waitWhileSearching();

    ASSERT_TRUE(search.hasResult()) << "No result for " << id;
    const auto& result = search.getLastSearchResult();
    const Move actual  = result.bestMove.stripped();

    // Check if the engine's move matches any expected move
    bool found = false;
    for (const Move m : expectedMoves) {
      if (m == actual) {
        found = true;
        break;
      }
    }

    EXPECT_TRUE(found)
      << id << ": expected one of { " << expectedMoves.str()
      << " } but got " << result.bestMove.str()
      << " (score: " << result.bestMoveValue.str()
      << ", depth: " << result.depth << "/" << result.extraDepth << ")";
  }

  // -----------------------------------------------------------------------
  // Avoid-move test: engine must NOT play any of the listed moves
  // -----------------------------------------------------------------------
  static void runAvoidMove(const std::string& fen,
                           const std::vector<std::string>& avoidSan,
                           const std::string& id) {
    const Position pos{fen};
    MoveGenerator mg{};
    MoveList avoidMoves;
    for (const auto& san : avoidSan) {
      const Move m = mg.getMoveFromSan(pos, san);
      ASSERT_TRUE(m.isValid()) << "Could not parse SAN move '" << san << "' for " << id;
      avoidMoves.push_back(m);
    }

    Search search{};
    SearchLimits sl{};
    sl.timeControl = true;
    sl.moveTime    = ERET_MOVE_TIME;
    if (ERET_MAX_DEPTH > 0) {
      sl.depth = ERET_MAX_DEPTH;
    }

    search.newGame();
    search.startSearch(pos, sl);
    search.waitWhileSearching();

    ASSERT_TRUE(search.hasResult()) << "No result for " << id;
    const auto& result = search.getLastSearchResult();
    const Move actual  = result.bestMove.stripped();

    bool forbidden = false;
    for (const Move m : avoidMoves) {
      if (m == actual) {
        forbidden = true;
        break;
      }
    }

    EXPECT_FALSE(forbidden)
      << id << ": must avoid { " << avoidMoves.str()
      << " } but played " << result.bestMove.str()
      << " (score: " << result.bestMoveValue.str()
      << ", depth: " << result.depth << "/" << result.extraDepth << ")";
  }
};

// =============================================================================
// ERET positions — one test per puzzle
// =============================================================================

TEST_F(ERET, eigenmann_rapid_engine) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  constexpr milliseconds moveTime{15s};
  constexpr Depth depth{0};
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += +"/test/testsets/eigenmann-rapid-engine.epd";
  enginetest::TestSuite ts{moveTime, depth, filePath};
  ts.runTestSuite();
}

TEST_F(ERET, ERET_001_Relief) {
  runBestMove("r1bqk1r1/1p1p1n2/p1n2pN1/2p1b2Q/2P1Pp2/1PN5/PB4PP/R4RK1 w q -",
              {"Rxf4"}, "ERET 001 - Relief");
}

TEST_F(ERET, ERET_002_Zugzwang) {
  runBestMove("r1n2N1k/2n2K1p/3pp3/5Pp1/b5R1/8/1PPP4/8 w - -",
              {"Ng6"}, "ERET 002 - Zugzwang");
}

TEST_F(ERET, ERET_003_OpenLine) {
  runBestMove("r1b1r1k1/1pqn1pbp/p2pp1p1/P7/1n1NPP1Q/2NBBR2/1PP3PP/R6K w - -",
              {"f5"}, "ERET 003 - Open Line");
}

TEST_F(ERET, ERET_004_Endgame) {
  runBestMove("5b2/p2k1p2/P3pP1p/n2pP1p1/1p1P2P1/1P1KBN2/7P/8 w - -",
              {"Nxg5"}, "ERET 004 - Endgame");
}

TEST_F(ERET, ERET_005_BishopSacrificeF7) {
  runBestMove("r3kbnr/1b3ppp/pqn5/1pp1P3/3p4/1BN2N2/PP2QPPP/R1BR2K1 w kq -",
              {"Bxf7"}, "ERET 005 - Bishop Sacrifice f7");
}

TEST_F(ERET, ERET_006_KnightSacrifice) {
  runBestMove("r2r2k1/1p1n1pp1/4pnp1/8/PpBRqP2/1Q2B1P1/1P5P/R5K1 b - -",
              {"Nc5"}, "ERET 006 - Knight Sacrifice");
}

TEST_F(ERET, ERET_007_BishopPair) {
  runBestMove("2rq1rk1/pb1n1ppN/4p3/1pb5/3P1Pn1/P1N5/1PQ1B1PP/R1B2RK1 b - -",
              {"Nde5"}, "ERET 007 - Bishop Pair");
}

TEST_F(ERET, ERET_008_Center) {
  runBestMove("r2qk2r/ppp1bppp/2n5/3p1b2/3P1Bn1/1QN1P3/PP3P1P/R3KBNR w KQkq -",
              {"Qxd5"}, "ERET 008 - Center");
}

TEST_F(ERET, ERET_009_KnightSacrifice) {
  runBestMove("rnb1kb1r/p4p2/1qp1pn2/1p2N2p/2p1P1p1/2N3B1/PPQ1BPPP/3RK2R w Kkq -",
              {"Ng6"}, "ERET 009 - Knight Sacrifice");
}

TEST_F(ERET, ERET_010_PassedPawn) {
  runBestMove("5rk1/pp1b4/4pqp1/2Ppb2p/1P2p3/4Q2P/P3BPP1/1R3R1K b - -",
              {"d4"}, "ERET 010 - Passed Pawn");
}

TEST_F(ERET, ERET_011_AttackingCastle) {
  runBestMove("r1b2r1k/ppp2ppp/8/4p3/2BPQ3/P3P1K1/1B3PPP/n3q1NR w - -",
              {"dxe5", "Nf3"}, "ERET 011 - Attacking Castle");
}

TEST_F(ERET, ERET_012_Relief) {
  runBestMove("1nkr1b1r/5p2/1q2p2p/1ppbP1p1/2pP4/2N3B1/1P1QBPPP/R4RK1 w - -",
              {"Nxd5"}, "ERET 012 - Relief");
}

TEST_F(ERET, ERET_013_Center) {
  runBestMove("1nrq1rk1/p4pp1/bp2pn1p/3p4/2PP1B2/P1PB2N1/4QPPP/1R2R1K1 w - -",
              {"Qd2", "Bc2"}, "ERET 013 - Center");
}

TEST_F(ERET, ERET_014_Endgame) {
  runBestMove("5k2/1rn2p2/3pb1p1/7p/p3PP2/PnNBK2P/3N2P1/1R6 w - -",
              {"Nf3"}, "ERET 014 - Endgame");
}

TEST_F(ERET, ERET_015_Endgame) {
  runBestMove("8/p2p4/r7/1k6/8/pK5Q/P7/b7 w - -",
              {"Qd3"}, "ERET 015 - Endgame");
}

TEST_F(ERET, ERET_016_PosSacrifice) {
  runBestMove("1b1rr1k1/pp1q1pp1/8/NP1p1b1p/1B1Pp1n1/PQR1P1P1/4BP1P/5RK1 w - -",
              {"Nc6"}, "ERET 016 - Pos. Sacrifice");
}

TEST_F(ERET, ERET_017_KingAttack) {
  runBestMove("1r3rk1/6p1/p1pb1qPp/3p4/4nPR1/2N4Q/PPP4P/2K1BR2 b - -",
              {"Rxb2"}, "ERET 017 - King Attack");
}

TEST_F(ERET, ERET_018_Development) {
  runBestMove("r1b1kb1r/1p1n1p2/p3pP1p/q7/3N3p/2N5/P1PQB1PP/1R3R1K b kq -",
              {"Qg5"}, "ERET 018 - Development");
}

TEST_F(ERET, ERET_019_Endgame) {
  runBestMove("3kB3/5K2/7p/3p4/3pn3/4NN2/8/1b4B1 w - -",
              {"Nf5"}, "ERET 019 - Endgame");
}

TEST_F(ERET, ERET_020_BishopSacrificeH7) {
  runBestMove("1nrrb1k1/1qn1bppp/pp2p3/3pP3/N2P3P/1P1B1NP1/PBR1QPK1/2R5 w - -",
              {"Bxh7"}, "ERET 020 - Bishop Sacrifice h7");
}

TEST_F(ERET, ERET_021_Prophylaxis) {
  runBestMove("3rr1k1/1pq2b1p/2pp2p1/4bp2/pPPN4/4P1PP/P1QR1PB1/1R4K1 b - -",
              {"Rc8"}, "ERET 021 - Prophylaxis");
}

TEST_F(ERET, ERET_022_PassedPawn) {
  runBestMove("r4rk1/p2nbpp1/2p2np1/q7/Np1PPB2/8/PPQ1N1PP/1K1R3R w - -",
              {"h4"}, "ERET 022 - Passed Pawn");
}

TEST_F(ERET, ERET_023_AttackingCastle) {
  runBestMove("r3r2k/1bq1nppp/p2b4/1pn1p2P/2p1P1QN/2P1N1P1/PPBB1P1R/2KR4 w - -",
              {"Ng6"}, "ERET 023 - Attacking Castle");
}

TEST_F(ERET, ERET_024_Development) {
  runAvoidMove("r2q1r1k/3bppbp/pp1p4/2pPn1Bp/P1P1P2P/2N2P2/1P1Q2P1/R3KB1R w KQ -",
               {"b3"}, "ERET 024 - Development");
}

TEST_F(ERET, ERET_025_Endgame) {
  runBestMove("2kb4/p7/r1p3p1/p1P2pBp/R2P3P/2K3P1/5P2/8 w - -",
              {"Bxd8"}, "ERET 025 - Endgame");
}

TEST_F(ERET, ERET_026_KnightSacrifice) {
  runBestMove("rqn2rk1/pp2b2p/2n2pp1/1N2p3/5P1N/1PP1B3/4Q1PP/R4RK1 w - -",
              {"Nxg6"}, "ERET 026 - Knight Sacrifice");
}

TEST_F(ERET, ERET_027_Zugzwang) {
  runBestMove("8/3Pk1p1/1p2P1K1/1P1Bb3/7p/7P/6P1/8 w - -",
              {"g4"}, "ERET 027 - Zugzwang");
}

TEST_F(ERET, ERET_028_PoisonedPawn) {
  runAvoidMove("4rrk1/Rpp3pp/6q1/2PPn3/4p3/2N5/1P2QPPP/5RK1 w - -",
               {"Rxb7"}, "ERET 028 - Poisoned Pawn");
}

TEST_F(ERET, ERET_029_ExchangeSacrifice) {
  runBestMove("2q2rk1/2p2pb1/PpP1p1pp/2n5/5B1P/3Q2P1/4PPN1/2R3K1 w - -",
              {"Rxc5"}, "ERET 029 - Exchange Sacrifice");
}

TEST_F(ERET, ERET_030_Initiative) {
  runBestMove("rnbq1r1k/4p1bP/p3p3/1pn5/8/2Np1N2/PPQ2PP1/R1B1KB1R w KQ -",
              {"Nh4"}, "ERET 030 - Initiative");
}

TEST_F(ERET, ERET_031_Endgame) {
  runBestMove("4b1k1/1p3p2/4pPp1/p2pP1P1/P2P4/1P1B4/8/2K5 w - -",
              {"b4"}, "ERET 031 - Endgame");
}

TEST_F(ERET, ERET_032_Zugzwang) {
  runBestMove("8/7p/5P1k/1p5P/5p2/2p1p3/P1P1P1P1/1K3Nb1 w - -",
              {"Ng3"}, "ERET 032 - Zugzwang");
}

TEST_F(ERET, ERET_033_Initiative) {
  runBestMove("r3kb1r/ppnq2pp/2n5/4pp2/1P1PN3/P4N2/4QPPP/R1B1K2R w KQkq -",
              {"Nxe5"}, "ERET 033 - Initiative");
}

TEST_F(ERET, ERET_034_BishopPair) {
  runBestMove("b4r1k/6bp/3q1ppN/1p2p3/3nP1Q1/3BB2P/1P3PP1/2R3K1 w - -",
              {"Rc8"}, "ERET 034 - Bishop Pair");
}

TEST_F(ERET, ERET_035_ExchangeSacrifice) {
  runBestMove("r3k2r/5ppp/3pbb2/qp1Np3/2BnP3/N7/PP1Q1PPP/R3K2R w KQkq -",
              {"Nxb5"}, "ERET 035 - Exchange Sacrifice");
}

TEST_F(ERET, ERET_036_Endgame) {
  runBestMove("r1k1n2n/8/pP6/5R2/8/1b1B4/4N3/1K5N w - -",
              {"b7"}, "ERET 036 - Endgame");
}

TEST_F(ERET, ERET_037_Zugzwang) {
  runBestMove("1k6/bPN2pp1/Pp2p3/p1p5/2pn4/3P4/PPR5/1K6 w - -",
              {"Na8"}, "ERET 037 - Zugzwang");
}

TEST_F(ERET, ERET_038_Endgame) {
  runBestMove("8/6N1/3kNKp1/3p4/4P3/p7/P6b/8 w - -",
              {"exd5"}, "ERET 038 - Endgame");
}

TEST_F(ERET, ERET_039_Development) {
  runBestMove("r1b1k2r/pp3ppp/1qn1p3/2bn4/8/6P1/PPN1PPBP/RNBQ1RK1 w kq -",
              {"a3"}, "ERET 039 - Development");
}

TEST_F(ERET, ERET_040_KingSafety) {
  runBestMove("r3kb1r/3n1ppp/p3p3/1p1pP2P/P3PBP1/4P3/1q2B3/R2Q1K1R b kq -",
              {"Bc5"}, "ERET 040 - King Safety");
}

TEST_F(ERET, ERET_041_KnightSacrifice) {
  runBestMove("3q1rk1/2nbppb1/pr1p1n1p/2pP1Pp1/2P1P2Q/2N2N2/1P2B1PP/R1B2RK1 w - -",
              {"Nxg5"}, "ERET 041 - Knight Sacrifice");
}

TEST_F(ERET, ERET_042_Endgame) {
  runBestMove("8/2k5/N3p1p1/2KpP1P1/b2P4/8/8/8 b - -",
              {"Kb7"}, "ERET 042 - Endgame");
}

TEST_F(ERET, ERET_043_KnightSacrifice) {
  runBestMove("2r1rbk1/1pqb1p1p/p2p1np1/P4p2/3NP1P1/2NP1R1Q/1P5P/R5BK w - -",
              {"Nxf5"}, "ERET 043 - Knight Sacrifice");
}

TEST_F(ERET, ERET_044_OpenLine) {
  runBestMove("rnb2rk1/pp2q2p/3p4/2pP2p1/2P1Pp2/2N5/PP1QBRPP/R5K1 w - -",
              {"h4"}, "ERET 044 - Open Line");
}

TEST_F(ERET, ERET_045_Initiative) {
  runBestMove("5rk1/p1p1rpb1/q1Pp2p1/3Pp2p/4Pn2/1R4N1/P1BQ1PPP/R5K1 w - -",
              {"Rb4"}, "ERET 045 - Initiative");
}

TEST_F(ERET, ERET_046_Endgame) {
  runBestMove("8/4nk2/1p3p2/1r1p2pp/1P1R1N1P/6P1/3KPP2/8 w - -",
              {"Nd3"}, "ERET 046 - Endgame");
}

TEST_F(ERET, ERET_047_Relief) {
  runBestMove("4kbr1/1b1nqp2/2p1p3/2N4p/1p1PP1pP/1PpQ2B1/4BPP1/r4RK1 w - -",
              {"Nxb7"}, "ERET 047 - Relief");
}

TEST_F(ERET, ERET_048_StrongSquares) {
  runBestMove("r1b2rk1/p2nqppp/1ppbpn2/3p4/2P5/1PN1PN2/PBQPBPPP/R4RK1 w - -",
              {"cxd5"}, "ERET 048 - Strong Squares");
}

TEST_F(ERET, ERET_049_Development) {
  runBestMove("r1b1kq1r/1p1n2bp/p2p2p1/3PppB1/Q1P1N3/8/PP2BPPP/R4RK1 w kq -",
              {"f4"}, "ERET 049 - Development");
}

TEST_F(ERET, ERET_050_KingAttack) {
  runBestMove("r4r1k/p1p3bp/2pp2p1/4nb2/N1P4q/1P5P/PBNQ1PP1/R4RK1 b - -",
              {"Nf3"}, "ERET 050 - King Attack");
}

TEST_F(ERET, ERET_051_Defence) {
  runBestMove("6k1/pb1r1qbp/3p1p2/2p2p2/2P1rN2/1P1R3P/PB3QP1/3R2K1 b - -",
              {"Bh6"}, "ERET 051 - Defence");
}

TEST_F(ERET, ERET_052_StrongSquares) {
  runBestMove("2r2r2/1p1qbkpp/p2ppn2/P1n1p3/4P3/2N1BB2/QPP2PPP/R4RK1 w - -",
              {"b4"}, "ERET 052 - Strong Squares");
}

TEST_F(ERET, ERET_053_PosSacrifice) {
  runBestMove("r1bq1rk1/p4ppp/3p2n1/1PpPp2n/4P2P/P1PB1PP1/2Q1N3/R1B1K2R b KQ -",
              {"c4"}, "ERET 053 - Pos. Sacrifice");
}

TEST_F(ERET, ERET_054_Endgame) {
  runBestMove("2b1r3/5pkp/6p1/4P3/QppqPP2/5RPP/6BK/8 b - -",
              {"c3"}, "ERET 054 - Endgame");
}

TEST_F(ERET, ERET_055_BishopSacrificeH6) {
  runBestMove("r2q1rk1/1p2bpp1/p1b2n1p/8/5B2/2NB4/PP1Q1PPP/3R1RK1 w - -",
              {"Bxh6"}, "ERET 055 - Bishop Sacrifice h6");
}

TEST_F(ERET, ERET_056_Zwischenzug) {
  runBestMove("r2qr1k1/pp2bpp1/2pp3p/4nbN1/2P4P/4BP2/PPPQ2P1/1K1R1B1R w - -",
              {"Be2"}, "ERET 056 - Zwischenzug");
}

TEST_F(ERET, ERET_057_Exchange) {
  runBestMove("r2qr1k1/pp1bbp2/n5p1/2pPp2p/8/P2PP1PP/1P2N1BK/R1BQ1R2 w - -",
              {"d6"}, "ERET 057 - Exchange");
}

TEST_F(ERET, ERET_058_Endgame) {
  runBestMove("8/8/R7/1b4k1/5p2/1B3r2/7P/7K w - -",
              {"h4"}, "ERET 058 - Endgame");
}

TEST_F(ERET, ERET_059_Endgame) {
  runBestMove("rq6/5k2/p3pP1p/3p2p1/6PP/1PB1Q3/2P5/1K6 w - -",
              {"Qd3"}, "ERET 059 - Endgame");
}

TEST_F(ERET, ERET_060_KingAttack) {
  runBestMove("q2B2k1/pb4bp/4p1p1/2p1N3/2PnpP2/PP3B2/6PP/2RQ2K1 b - -",
              {"Qxd8"}, "ERET 060 - King Attack");
}

TEST_F(ERET, ERET_061_KingAttack) {
  runBestMove("4rrk1/pp4pp/3p4/3P3b/2PpPp1q/1Q5P/PB4B1/R4RK1 b - -",
              {"Rf6"}, "ERET 061 - King Attack");
}

TEST_F(ERET, ERET_062_StrongSquares) {
  runBestMove("rr1nb1k1/2q1b1pp/pn1p1p2/1p1PpNPP/4P3/1PP1BN2/2B2P2/R2QR1K1 w - -",
              {"g6"}, "ERET 062 - Strong Squares");
}

TEST_F(ERET, ERET_063_Defence) {
  runBestMove("r3k2r/4qn2/p1p1b2p/6pB/P1p5/2P5/5PPP/RQ2R1K1 b kq -",
              {"Kf8"}, "ERET 063 - Defence");
}

TEST_F(ERET, ERET_064_Endgame) {
  runAvoidMove("8/1pp5/p3k1pp/8/P1p2PPP/2P2K2/1P3R2/5r2 b - -",
               {"Rxf2"}, "ERET 064 - Endgame");
}

TEST_F(ERET, ERET_065_Zwischenzug) {
  runBestMove("1r3rk1/2qbppbp/3p1np1/nP1P2B1/2p2P2/2N1P2P/1P1NB1P1/R2Q1RK1 b - -",
              {"Qb6"}, "ERET 065 - Zwischenzug");
}

TEST_F(ERET, ERET_066_Endgame) {
  runBestMove("8/2pN1k2/p4p1p/Pn1R4/3b4/6Pp/1P3K1P/8 w - -",
              {"Ke1"}, "ERET 066 - Endgame");
}

TEST_F(ERET, ERET_067_Clearance) {
  runBestMove("5r1k/1p4bp/3p1q2/1NpP1b2/1pP2p2/1Q5P/1P1KBP2/r2RN2R b - -",
              {"f3"}, "ERET 067 - Clearance");
}

TEST_F(ERET, ERET_068_OpenLine) {
  runBestMove("r3kb1r/pbq2ppp/1pn1p3/2p1P3/1nP5/1P3NP1/PB1N1PBP/R2Q1RK1 w kq -",
              {"a3"}, "ERET 068 - Open Line");
}

TEST_F(ERET, ERET_069_KingAttack) {
  runBestMove("5rk1/n2qbpp1/pp2p1p1/3pP1P1/PP1P3P/2rNPN2/R7/1Q3RK1 w - -",
              {"h5"}, "ERET 069 - King Attack");
}

TEST_F(ERET, ERET_070_StrongSquares) {
  runBestMove("r5k1/1bqp1rpp/p1n1p3/1p4p1/1b2PP2/2NBB1P1/PPPQ4/2KR3R w - -",
              {"a3"}, "ERET 070 - Strong Squares");
}

TEST_F(ERET, ERET_071_Deflection) {
  runBestMove("1r4k1/1nq3pp/pp1pp1r1/8/PPP2P2/6P1/5N1P/2RQR1K1 w - -",
              {"f5"}, "ERET 071 - Deflection");
}

TEST_F(ERET, ERET_072_Centralization) {
  runBestMove("q5k1/p2p2bp/1p1p2r1/2p1np2/6p1/1PP2PP1/P2PQ1KP/4R1NR b - -",
              {"Qd5"}, "ERET 072 - Centralization");
}

TEST_F(ERET, ERET_073_Mobility) {
  runBestMove("r4rk1/ppp2ppp/1nnb4/8/1P1P3q/PBN1B2P/4bPP1/R2QR1K1 w - -",
              {"Qxe2"}, "ERET 073 - Mobility");
}

TEST_F(ERET, ERET_074_Endgame) {
  runBestMove("1r3k2/2N2pp1/1pR2n1p/4p3/8/1P1K1P2/P5PP/8 w - -",
              {"Kc4"}, "ERET 074 - Endgame");
}

TEST_F(ERET, ERET_075_Fortress) {
  runBestMove("6r1/6r1/2p1k1pp/p1pbP2q/Pp1p1PpP/1P1P2NR/1KPQ3R/8 b - -",
              {"Qf5"}, "ERET 075 - Fortress");
}

TEST_F(ERET, ERET_076_Development) {
  runBestMove("r1b1kb1r/1p1npppp/p2p1n2/6B1/3NPP2/q1N5/P1PQ2PP/1R2KB1R w Kkq -",
              {"Bxf6"}, "ERET 076 - Development");
}

TEST_F(ERET, ERET_077_AttackingCastle) {
  runBestMove("r3r1k1/1bq2ppp/p1p2n2/3ppPP1/4P3/1PbB4/PBP1Q2P/R4R1K w - -",
              {"gxf6"}, "ERET 077 - Attacking Castle");
}

TEST_F(ERET, ERET_078_PassedPawn) {
  runBestMove("r4rk1/ppq3pp/2p1Pn2/4p1Q1/8/2N5/PP4PP/2KR1R2 w - -",
              {"Rxf6"}, "ERET 078 - Passed Pawn");
}

TEST_F(ERET, ERET_079_QueenSacrifice) {
  runBestMove("r1bqr1k1/3n1ppp/p2p1b2/3N1PP1/1p1B1P2/1P6/1PP1Q2P/2KR2R1 w - -",
              {"Qxe8"}, "ERET 079 - Queen Sacrifice");
}

TEST_F(ERET, ERET_080_Clearance) {
  runBestMove("5rk1/1ppbq1pp/3p3r/pP1PppbB/2P5/P1BP4/5PPP/3QRRK1 b - -",
              {"Bc1"}, "ERET 080 - Clearance");
}

TEST_F(ERET, ERET_081_KingAttack) {
  runBestMove("r3r1kb/p2bp2p/1q1p1npB/5NQ1/2p1P1P1/2N2P2/PPP5/2KR3R w - -",
              {"Bg7"}, "ERET 081 - King Attack");
}

TEST_F(ERET, ERET_082_Endgame) {
  runBestMove("8/3P4/1p3b1p/p7/P7/1P3NPP/4p1K1/3k4 w - -",
              {"g4"}, "ERET 082 - Endgame");
}

TEST_F(ERET, ERET_083_Exchange) {
  runBestMove("3q1rk1/7p/rp1n4/p1pPbp2/P1P2pb1/1QN4P/1B2B1P1/1R3RK1 w - -",
              {"Nb5"}, "ERET 083 - Exchange");
}

TEST_F(ERET, ERET_084_KingAttack) {
  runBestMove("4r1k1/1r1np3/1pqp1ppB/p7/2b1P1PQ/2P2P2/P3B2R/3R2K1 w - -",
              {"Bg7", "Bg5"}, "ERET 084 - King Attack");
}

TEST_F(ERET, ERET_085_Exchange) {
  runBestMove("r4rk1/q4bb1/p1R4p/3pN1p1/8/2N3P1/P4PP1/3QR1K1 w - -",
              {"Ng4"}, "ERET 085 - Exchange");
}

TEST_F(ERET, ERET_086_ExchangeSacrifice) {
  runBestMove("r3k2r/pp2pp1p/8/q2Pb3/2P5/4p3/B1Q2PPP/2R2RK1 w kq -",
              {"c5"}, "ERET 086 - Exchange Sacrifice");
}

TEST_F(ERET, ERET_087_Clearance) {
  runBestMove("r3r1k1/1bnq1pbn/p2p2p1/1p1P3p/2p1PP1B/P1N2B1P/1PQN2P1/3RR1K1 w - -",
              {"e5"}, "ERET 087 - Clearance");
}

TEST_F(ERET, ERET_088_Endgame) {
  runBestMove("8/4k3/p2p2p1/P1pPn2p/1pP1P2P/1P1NK1P1/8/8 w - -",
              {"g4"}, "ERET 088 - Endgame");
}

TEST_F(ERET, ERET_089_Underpromotion) {
  runBestMove("8/2P1P3/b1B2p2/1pPRp3/2k3P1/P4pK1/nP3p1p/N7 w - -",
              {"e8N"}, "ERET 089 - Underpromotion");
}

TEST_F(ERET, ERET_090_Endgame) {
  runBestMove("4K1k1/8/1p5p/1Pp3b1/8/1P3P2/P1B2P2/8 w - -",
              {"f4"}, "ERET 090 - Endgame");
}

TEST_F(ERET, ERET_091_Endgame) {
  runBestMove("8/6p1/3k4/3p1p1p/p2K1P1P/4P1P1/P7/8 b - -",
              {"g6", "Kc6"}, "ERET 091 - Endgame");
}

TEST_F(ERET, ERET_092_PoisonedPawn) {
  runAvoidMove("r1b2rk1/ppp3p1/4p2p/4Qpq1/3P4/2PB4/PPK2PPP/R6R b - -",
               {"Qxg2"}, "ERET 092 - Poisoned Pawn");
}

TEST_F(ERET, ERET_093_Endgame) {
  runBestMove("2b1r3/r2ppN2/8/1p1p1k2/pP1P4/2P3R1/PP3PP1/2K5 w - -",
              {"Nd6"}, "ERET 093 - Endgame");
}

TEST_F(ERET, ERET_094_QueenSacrifice) {
  runBestMove("2k2Br1/p6b/Pq1r4/1p2p1b1/1Ppp2p1/Q1P3N1/5RPP/R3N1K1 b - -",
              {"Rf6"}, "ERET 094 - Queen Sacrifice");
}

TEST_F(ERET, ERET_095_QueenSacrifice) {
  runBestMove("r2qk2r/ppp1b1pp/2n1p3/3pP1n1/3P2b1/2PB1NN1/PP4PP/R1BQK2R w KQkq -",
              {"Nxg5"}, "ERET 095 - Queen Sacrifice");
}

TEST_F(ERET, ERET_096_Endgame) {
  runBestMove("8/8/4p1Pk/1rp1K1p1/4P1P1/1nP2Q2/p2b1P2/8 w - -",
              {"Kf6"}, "ERET 096 - Endgame");
}

TEST_F(ERET, ERET_097_Endgame) {
  runBestMove("2k5/p7/Pp1p1b2/1P1P1p2/2P2P1p/3K3P/5B2/8 w - -",
              {"c5"}, "ERET 097 - Endgame");
}

TEST_F(ERET, ERET_098_Endgame) {
  runAvoidMove("8/6pp/5k2/1p1r4/4R3/7P/5PP1/5K2 w - -",
               {"Ke2"}, "ERET 098 - Endgame");
}

TEST_F(ERET, ERET_099_Endgame) {
  runBestMove("3q1r1k/4RPp1/p6p/2pn4/2P5/1P6/P3Q2P/6K1 w - -",
              {"Re8"}, "ERET 099 - Endgame");
}

TEST_F(ERET, ERET_100_Initiative) {
  runBestMove("rn2k2r/3pbppp/p3p3/8/Nq1Nn3/4B1P1/PP3P1P/R2Q1RK1 w k -",
              {"Nf5"}, "ERET 100 - Initiative");
}

TEST_F(ERET, ERET_101_Development) {
  runBestMove("r1b1kb1N/pppnq1pB/8/3p4/3P4/8/PPPK1nPP/RNB1R3 b q -",
              {"Ne5"}, "ERET 101 - Development");
}

TEST_F(ERET, ERET_102_KingAttack) {
  runBestMove("N4rk1/pp1b1ppp/n3p1n1/3pP1Q1/1P1N4/8/1PP2PPP/q1B1KB1R b K -",
              {"Nxb4"}, "ERET 102 - King Attack");
}

TEST_F(ERET, ERET_103_Zugzwang) {
  runBestMove("4k1br/1K1p1n1r/2p2pN1/P2p1N2/2P3pP/5B2/P2P4/8 w - -",
              {"Kc8"}, "ERET 103 - Zugzwang");
}

TEST_F(ERET, ERET_104_Development) {
  runBestMove("r1bqkb1r/ppp3pp/2np4/3N1p2/3pnB2/5N2/PPP1QPPP/2KR1B1R b kq -",
              {"Ne7"}, "ERET 104 - Development");
}

TEST_F(ERET, ERET_105_StrongSquares) {
  runBestMove("r3kb1r/pbqp1pp1/1pn1pn1p/8/3PP3/2PB1N2/3N1PPP/R1BQR1K1 w kq -",
              {"e5"}, "ERET 105 - Strong Squares");
}

TEST_F(ERET, ERET_106_KingSafety) {
  runBestMove("r2r2k1/pq2bppp/1np1bN2/1p2B1P1/5Q2/P4P2/1PP4P/2KR1B1R b - -",
              {"Bxf6"}, "ERET 106 - King Safety");
}

TEST_F(ERET, ERET_107_Defence) {
  runBestMove("1r1r2k1/2pq3p/4p3/2Q1Pp2/1PNn1R2/P5P1/5P1P/4R2K b - -",
              {"Rb5"}, "ERET 107 - Defence");
}

TEST_F(ERET, ERET_108_Endgame) {
  runBestMove("8/5p1p/3P1k2/p1P2n2/3rp3/1B6/P4R2/6K1 w - -",
              {"Ba4"}, "ERET 108 - Endgame");
}

TEST_F(ERET, ERET_109_Relief) {
  runBestMove("2rbrnk1/1b3p2/p2pp3/1p4PQ/1PqBPP2/P1NR4/2P4P/5RK1 b - -",
              {"Qxd4"}, "ERET 109 - Relief");
}

TEST_F(ERET, ERET_110_PassedPawn) {
  runBestMove("4r1k1/1bq2r1p/p2p1np1/3Pppb1/P1P5/1N3P2/1R2B1PP/1Q1R2BK w - -",
              {"c5"}, "ERET 110 - Passed Pawn");
}

TEST_F(ERET, ERET_111_Fortress) {
  runBestMove("8/8/8/8/4kp2/1R6/P2q1PPK/8 w - -",
              {"a3"}, "ERET 111 - Fortress");
}
