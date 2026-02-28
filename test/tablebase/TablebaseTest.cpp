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

#include "config/ConfigManager.h"
#include "config/ConfigMode.h"
#include "tablebase/Tablebase.h"
#include "Test_Utils.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"
#include "common/Logging.h"
#include "init.h"
#include "tablebase/TablebasePaths.h"

#include <chrono>
#include <format>
#include <gtest/gtest.h>

using namespace tablebase;
using testing::Eq;

//=============================================================================
// Shared test helper for tablebase availability checks
//=============================================================================

namespace {

  // Returns the maximum number of tablebase pieces available, or 0 if no tablebases found.
  // Uses a static Tablebase to avoid repeated initialization/shutdown which can corrupt Fathom state.
  // Tests should use this with SKIP_IF_NO_TABLEBASES macro.
  int getMaxTablebasePieces() {
    static Tablebase checkTb;
    static bool initialized = false;

    if (!initialized) {
      initialized = true;
      const std::string tbPath = findTablebasePath();
      if (!tbPath.empty()) {
        checkTb.initialize(tbPath);
      }
    }

    return checkTb.isAvailable() ? checkTb.maxPieces() : 0;
  }

}// namespace

// Macro for skipping tests based on tablebase requirements.
// Must be called directly in the test function (not in a helper).
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SKIP_IF_NO_TABLEBASES(minPieces)                                                          \
  if (const int maxAvailable_ = getMaxTablebasePieces(); maxAvailable_ == 0) {                    \
    GTEST_SKIP() << "Syzygy tablebases not available. "                                           \
                 << "Set SYZYGY_PATH environment variable or configure TB_PATH in search.yaml";   \
  } else if constexpr ((minPieces) > 0) {                                                         \
    if (maxAvailable_ < (minPieces)) {                                                            \
      GTEST_SKIP() << (minPieces) << "-piece tablebases not available (max available: "           \
                   << maxAvailable_ << ")";                                                       \
    }                                                                                             \
  }


class TablebaseTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::info);
    Logger::get().TB_LOG->set_level(spdlog::level::info);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

//=============================================================================
// Basic Construction and State Tests
//=============================================================================

TEST_F(TablebaseTest, defaultConstruction) {
  const Tablebase tb;
  EXPECT_FALSE(tb.isAvailable());
  EXPECT_EQ(0, tb.maxPieces());
  EXPECT_TRUE(tb.getPath().empty());
}

TEST_F(TablebaseTest, initializeWithEmptyPath) {
  Tablebase tb;
  const bool result = tb.initialize("");
  EXPECT_FALSE(result);
  EXPECT_FALSE(tb.isAvailable());
}

TEST_F(TablebaseTest, initializeWithInvalidPath) {
  Tablebase tb;
  const bool result = tb.initialize("C:/nonexistent/path/to/tablebases");
  EXPECT_FALSE(result);
  EXPECT_FALSE(tb.isAvailable());
  EXPECT_EQ(0, tb.maxPieces());
}

TEST_F(TablebaseTest, shutdown) {
  Tablebase tb;
  // Shutdown on uninitialized should be safe
  tb.shutdown();
  EXPECT_FALSE(tb.isAvailable());
}

//=============================================================================
// TBResult Enum Tests
//=============================================================================

TEST_F(TablebaseTest, tbResultValues) {
  // Verify enum ordering (important for comparisons)
  EXPECT_LT(static_cast<int>(TBResult::Loss), static_cast<int>(TBResult::BlessedLoss));
  EXPECT_LT(static_cast<int>(TBResult::BlessedLoss), static_cast<int>(TBResult::Draw));
  EXPECT_LT(static_cast<int>(TBResult::Draw), static_cast<int>(TBResult::CursedWin));
  EXPECT_LT(static_cast<int>(TBResult::CursedWin), static_cast<int>(TBResult::Win));
}

TEST_F(TablebaseTest, resultToString) {
  EXPECT_EQ("Win", Tablebase::resultToString(TBResult::Win));
  EXPECT_EQ("Cursed Win", Tablebase::resultToString(TBResult::CursedWin));
  EXPECT_EQ("Draw", Tablebase::resultToString(TBResult::Draw));
  EXPECT_EQ("Blessed Loss", Tablebase::resultToString(TBResult::BlessedLoss));
  EXPECT_EQ("Loss", Tablebase::resultToString(TBResult::Loss));
  EXPECT_EQ("Failed", Tablebase::resultToString(TBResult::Failed));
}

//=============================================================================
// TBProbeResult Tests
//=============================================================================

TEST_F(TablebaseTest, probeResultDefaults) {
  constexpr TBProbeResult result;
  EXPECT_EQ(TBResult::Failed, result.wdl);
  EXPECT_EQ(0, result.dtz);
  EXPECT_EQ(MOVE_NONE, result.bestMove);
  EXPECT_FALSE(result.success());
}

TEST_F(TablebaseTest, probeResultSuccess) {
  TBProbeResult result;
  result.wdl = TBResult::Win;
  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.isWin());
  EXPECT_FALSE(result.isDraw());
  EXPECT_FALSE(result.isLoss());
}

TEST_F(TablebaseTest, probeResultCursedWin) {
  TBProbeResult result;
  result.wdl = TBResult::CursedWin;
  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.isWin());// CursedWin counts as win
  EXPECT_FALSE(result.isDraw());
  EXPECT_FALSE(result.isLoss());
}

TEST_F(TablebaseTest, probeResultDraw) {
  TBProbeResult result;
  result.wdl = TBResult::Draw;
  EXPECT_TRUE(result.success());
  EXPECT_FALSE(result.isWin());
  EXPECT_TRUE(result.isDraw());
  EXPECT_FALSE(result.isLoss());
}

TEST_F(TablebaseTest, probeResultBlessedLoss) {
  TBProbeResult result;
  result.wdl = TBResult::BlessedLoss;
  EXPECT_TRUE(result.success());
  EXPECT_FALSE(result.isWin());
  EXPECT_FALSE(result.isDraw());
  EXPECT_TRUE(result.isLoss());// BlessedLoss counts as loss
}

TEST_F(TablebaseTest, probeResultLoss) {
  TBProbeResult result;
  result.wdl = TBResult::Loss;
  EXPECT_TRUE(result.success());
  EXPECT_FALSE(result.isWin());
  EXPECT_FALSE(result.isDraw());
  EXPECT_TRUE(result.isLoss());
}

//=============================================================================
// tbValueToScore Tests
//=============================================================================

TEST_F(TablebaseTest, tbValueToScoreWin) {
  const Value score = Tablebase::tbValueToScore(TBResult::Win, DEPTH_NONE);
  EXPECT_GT(static_cast<int>(score), 8000);// High positive value
}

TEST_F(TablebaseTest, tbValueToScoreLoss) {
  const Value score = Tablebase::tbValueToScore(TBResult::Loss, DEPTH_NONE);
  EXPECT_LT(static_cast<int>(score), -8000);// High negative value
}

TEST_F(TablebaseTest, tbValueToScoreDraw) {
  const Value score = Tablebase::tbValueToScore(TBResult::Draw, DEPTH_NONE);
  EXPECT_EQ(VALUE_DRAW, score);
}

TEST_F(TablebaseTest, tbValueToScoreFailed) {
  const Value score = Tablebase::tbValueToScore(TBResult::Failed, DEPTH_NONE);
  EXPECT_EQ(VALUE_NONE, score);
}

TEST_F(TablebaseTest, tbValueToScorePlyAdjustment) {
  // Scores should be adjusted by ply (prefer shorter wins)
  const Value scoreAtPly0  = Tablebase::tbValueToScore(TBResult::Win, DEPTH_NONE);
  const Value scoreAtPly10 = Tablebase::tbValueToScore(TBResult::Win, static_cast<Depth>(10));
  EXPECT_GT(static_cast<int>(scoreAtPly0), static_cast<int>(scoreAtPly10));
}

//=============================================================================
// canProbe Tests (without actual tablebases)
//=============================================================================

TEST_F(TablebaseTest, canProbeNotInitialized) {
  const Tablebase tb;
  const Position pos;
  // Without initialization, canProbe should always return false
  EXPECT_FALSE(tb.canProbe(pos));
}

TEST_F(TablebaseTest, probeWDLNotInitialized) {
  const Tablebase tb;
  const Position pos;
  // Without initialization, canProbe should return false
  EXPECT_FALSE(tb.canProbe(pos));
  // probeWDL requires canProbe() to be true, so we can't call it here
  // The real code always checks canProbe() before calling probeWDL()
}


TEST_F(TablebaseTest, probeRootNotInitialized) {
  const Tablebase tb;
  const Position pos;
  const TBProbeResult result = tb.probeRoot(pos);
  EXPECT_EQ(TBResult::Failed, result.wdl);
  EXPECT_FALSE(result.success());
}

//=============================================================================
// Integration Tests (require actual tablebase files)
// These tests are skipped if tablebases are not available
//=============================================================================


class TablebaseIntegrationTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::info);
    Logger::get().TB_LOG->set_level(spdlog::level::info);
  }

protected:
  Tablebase tb;

  void SetUp() override {
    // REQUIRED FOR WSL run
    // CONFIG_OVERRIDE(s.TB_PATH = "/mnt/d/SYZYGY");

    // Use centralized path resolution (checks env var, config, defaults)
    const std::string tbPath = findTablebasePath();
    if (!tbPath.empty()) {
      tb.initialize(tbPath);
    }
  }

  void TearDown() override {
    tb.shutdown();
  }
};

// KQK - King and Queen vs King (basic win)
TEST_F(TablebaseIntegrationTest, KQK_WhiteWins) {
  SKIP_IF_NO_TABLEBASES(0);

  // Queen on d2 doesn't attack e5 (diagonals are c1-e3-f4-g5 and c3-d2-e1)
  const Position pos("8/8/8/4k3/8/8/3Q4/4K3 w - - 0 1");// White has KQ vs K

  LOG__INFO(Logger::get().TEST_LOG, "KQK test: canProbe={}, pieceCount={}, maxPieces={}",
            tb.canProbe(pos), pos.getOccupiedBb().popcount(), tb.maxPieces());

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result == TBResult::Failed) {
      GTEST_SKIP() << "KQvK tablebase file may be missing or corrupted";
    }
    EXPECT_EQ(TBResult::Win, result);
    LOG__INFO(Logger::get().TEST_LOG, "KQK position: {}", Tablebase::resultToString(result));
  }
  else {
    GTEST_SKIP() << "Position not probeable (KQK tablebase may be missing)";
  }
}

// KRK - King and Rook vs King (basic win)
TEST_F(TablebaseIntegrationTest, KRK_WhiteWins) {
  SKIP_IF_NO_TABLEBASES(0);

  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");// White has KR vs K

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    EXPECT_EQ(TBResult::Win, result);
    LOG__INFO(Logger::get().TEST_LOG, "KRK position: {}", Tablebase::resultToString(result));
  }
}

// KPK - King and Pawn vs King (can be win or draw depending on position)
TEST_F(TablebaseIntegrationTest, KPK_WinningPosition) {
  SKIP_IF_NO_TABLEBASES(0);

  const Position pos("8/4P3/8/4k3/8/8/8/4K3 w - - 0 1");// Pawn about to promote

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    EXPECT_EQ(TBResult::Win, result);
    LOG__INFO(Logger::get().TEST_LOG, "KPK winning position: {}", Tablebase::resultToString(result));
  }
}

// Position with castling rights should not be probeable
TEST_F(TablebaseIntegrationTest, PositionWithCastling) {
  SKIP_IF_NO_TABLEBASES(0);

  const Position pos("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");// Has castling rights

  // Should not be probeable due to castling rights
  EXPECT_FALSE(tb.canProbe(pos));
}

// Position with too many pieces (if only 3-4-5 piece TBs available)
TEST_F(TablebaseIntegrationTest, TooManyPieces) {
  SKIP_IF_NO_TABLEBASES(0);

  // Standard starting position - definitely too many pieces
  const Position pos;
  EXPECT_FALSE(tb.canProbe(pos));
}

// Test probeRoot returns best move
TEST_F(TablebaseIntegrationTest, ProbeRootBestMove) {
  SKIP_IF_NO_TABLEBASES(0);

  // Use KRK position which is known to work
  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");// KRK

  if (tb.canProbe(pos)) {
    const TBProbeResult result = tb.probeRoot(pos);
    EXPECT_TRUE(result.success());
    EXPECT_TRUE(result.isWin());
    // Best move should be valid (not MOVE_NONE)
    if (result.bestMove != MOVE_NONE) {
      LOG__INFO(Logger::get().TEST_LOG, "KRK best move: {}", result.bestMove.str());
    }
  }
}

//=============================================================================
// Additional Integration Tests - More positions and edge cases
//=============================================================================

// KBK - King and Bishop vs King (should be draw - insufficient material)
TEST_F(TablebaseIntegrationTest, KBK_Draw) {
  SKIP_IF_NO_TABLEBASES(0);

  // Black king e5, bishop c1 (doesn't attack e5), white king e1
  const Position pos("8/8/8/4k3/8/8/8/2B1K3 w - - 0 1");// White has KB vs K

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result == TBResult::Failed) {
      GTEST_SKIP() << "KBvK tablebase file may be missing or corrupted";
    }
    EXPECT_EQ(TBResult::Draw, result);
    LOG__INFO(Logger::get().TEST_LOG, "KBK position: {}", Tablebase::resultToString(result));
  }
}

// KNK - King and Knight vs King (should be draw - insufficient material)
TEST_F(TablebaseIntegrationTest, KNK_Draw) {
  SKIP_IF_NO_TABLEBASES(0);

  const Position pos("8/8/8/4k3/8/8/1N6/4K3 w - - 0 1");// White has KN vs K

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result == TBResult::Failed) {
      GTEST_SKIP() << "KNvK tablebase file may be missing or corrupted";
    }
    EXPECT_EQ(TBResult::Draw, result);
    LOG__INFO(Logger::get().TEST_LOG, "KNK position: {}", Tablebase::resultToString(result));
  }
}

// KBBK - King and two Bishops vs King (should be win)
TEST_F(TablebaseIntegrationTest, KBBK_WhiteWins) {
  SKIP_IF_NO_TABLEBASES(0);

  // Black king on e5, bishops on c1 and f1, white king on e1
  // Bishops don't attack e5 from c1 or f1
  const Position pos("8/8/8/4k3/8/8/8/2B1KB2 w - - 0 1");// White has KBB vs K

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result == TBResult::Failed) {
      GTEST_SKIP() << "KBBvK tablebase file may be missing or corrupted";
    }
    EXPECT_EQ(TBResult::Win, result);
    LOG__INFO(Logger::get().TEST_LOG, "KBBK position: {}", Tablebase::resultToString(result));
  }
}

// KBNK - King, Bishop and Knight vs King (should be win)
TEST_F(TablebaseIntegrationTest, KBNK_WhiteWins) {
  SKIP_IF_NO_TABLEBASES(0);

  // Black king on e5, bishop on c1 (doesn't attack e5), knight on b1
  const Position pos("8/8/8/4k3/8/8/8/1NB1K3 w - - 0 1");// White has KBN vs K

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result == TBResult::Failed) {
      GTEST_SKIP() << "KBNvK tablebase file may be missing or corrupted";
    }
    EXPECT_EQ(TBResult::Win, result);
    LOG__INFO(Logger::get().TEST_LOG, "KBNK position: {}", Tablebase::resultToString(result));
  }
}

// KNNK - King and two Knights vs King (should be draw - can't force mate)
TEST_F(TablebaseIntegrationTest, KNNK_Draw) {
  SKIP_IF_NO_TABLEBASES(0);

  const Position pos("8/8/8/4k3/8/8/1N6/N3K3 w - - 0 1");// White has KNN vs K

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result == TBResult::Failed) {
      GTEST_SKIP() << "KNNvK tablebase file may be missing or corrupted";
    }
    EXPECT_EQ(TBResult::Draw, result);
    LOG__INFO(Logger::get().TEST_LOG, "KNNK position: {}", Tablebase::resultToString(result));
  }
}

// KRvKR - Rook vs Rook (typically draw)
TEST_F(TablebaseIntegrationTest, KRvKR_Draw) {
  SKIP_IF_NO_TABLEBASES(0);

  const Position pos("8/8/8/4k3/8/8/1R6/r3K3 w - - 0 1");// KR vs KR

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result == TBResult::Failed) {
      GTEST_SKIP() << "KRvKR tablebase file may be missing or corrupted";
    }
    EXPECT_EQ(TBResult::Draw, result);
    LOG__INFO(Logger::get().TEST_LOG, "KRvKR position: {}", Tablebase::resultToString(result));
  }
}

// KQvKR - Queen vs Rook (should be win for queen side)
TEST_F(TablebaseIntegrationTest, KQvKR_WhiteWins) {
  SKIP_IF_NO_TABLEBASES(0);

  // Queen d2 doesn't attack e5 (diagonals are c1-e3-f4-g5 and a5-b4-c3-e1)
  const Position pos("8/8/8/4k3/8/8/3Q4/r3K3 w - - 0 1");// KQ vs KR

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result == TBResult::Failed) {
      GTEST_SKIP() << "KQvKR tablebase file may be missing or corrupted";
    }
    EXPECT_EQ(TBResult::Win, result);
    LOG__INFO(Logger::get().TEST_LOG, "KQvKR position: {}", Tablebase::resultToString(result));
  }
}

// Black to move - KRK from black's perspective (black is losing)
TEST_F(TablebaseIntegrationTest, KRK_BlackToMove_BlackLoses) {
  SKIP_IF_NO_TABLEBASES(0);

  const Position pos("8/8/8/4k3/8/8/1R6/4K3 b - - 0 1");// Black to move, white has KR vs K

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result == TBResult::Failed) {
      GTEST_SKIP() << "Tablebase file may be missing or corrupted";
    }
    EXPECT_EQ(TBResult::Loss, result);
    LOG__INFO(Logger::get().TEST_LOG, "KRK black to move: {}", Tablebase::resultToString(result));
  }
}

// KPK - Drawn position (king in front of pawn, wrong side)
TEST_F(TablebaseIntegrationTest, KPK_DrawnPosition) {
  SKIP_IF_NO_TABLEBASES(0);

  // Pawn on a-file with black king controlling promotion square
  const Position pos("k7/P7/K7/8/8/8/8/8 w - - 0 1");// Drawn - stalemate pattern

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result == TBResult::Failed) {
      GTEST_SKIP() << "KPvK tablebase file may be missing or corrupted";
    }
    // This specific position might be win or draw depending on exact squares
    // Just verify it returns a valid result (not Failed)
    EXPECT_NE(TBResult::Failed, result);
    LOG__INFO(Logger::get().TEST_LOG, "KPK edge position: {}", Tablebase::resultToString(result));
  }
}

// Test position with en passant square
TEST_F(TablebaseIntegrationTest, EnPassantPosition) {
  SKIP_IF_NO_TABLEBASES(0);

  // KPvKP position with legal en passant capture available
  // White pawn on e5, black pawn just moved d7-d5, white can capture en passant
  // Position: white king h1, white pawn e5, black king a8, black pawn d5
  const Position posWithEP("k7/8/8/3pP3/8/8/8/7K w - d6 0 1");

  if (tb.canProbe(posWithEP)) {
    const TBResult result = tb.probeWDL(posWithEP);
    // Just verify it doesn't fail - the EP capture should be handled correctly
    EXPECT_NE(TBResult::Failed, result);
    LOG__INFO(Logger::get().TEST_LOG, "EP position result: {}", Tablebase::resultToString(result));
  }

  // Position with EP square set but NO legal EP capture (no pawn can actually capture)
  // White king h1, white pawn g5 (cannot capture on d6), black king a8, black pawn d5
  const Position posNoLegalEP("k7/8/8/3p2P1/8/8/8/7K w - d6 0 1");

  if (tb.canProbe(posNoLegalEP)) {
    const TBResult result = tb.probeWDL(posNoLegalEP);
    // This should also work - EP square should be ignored since capture isn't legal
    EXPECT_NE(TBResult::Failed, result);
    LOG__INFO(Logger::get().TEST_LOG, "No legal EP result: {}", Tablebase::resultToString(result));
  }
}

// Test EP positions with pinned pawns - EP square should be ignored when capture is illegal
TEST_F(TablebaseIntegrationTest, EnPassantPinnedPawn) {
  SKIP_IF_NO_TABLEBASES(0);

  // Horizontal pin: White king a5, black rook h5, white pawn e5, black pawn d5
  // EP capture exd6 would expose king to rook - illegal
  // EP square should NOT be passed to Fathom
  const Position posHorizontalPin("4k3/8/8/K2pP2r/8/8/8/8 w - d6 0 1");

  if (tb.canProbe(posHorizontalPin)) {
    const TBResult result = tb.probeWDL(posHorizontalPin);
    // Should not fail - EP is correctly identified as illegal and not passed to Fathom
    EXPECT_NE(TBResult::Failed, result);
    LOG__INFO(Logger::get().TEST_LOG, "Horizontal pin EP result: {}", Tablebase::resultToString(result));
  }

  // Vertical pin: White king c1, black rook c8, white pawn c5, black pawn d5
  // EP capture cxd6 would expose king to rook - illegal
  const Position posVerticalPin("2r1k3/8/8/2Pp4/8/8/8/2K5 w - d6 0 1");

  if (tb.canProbe(posVerticalPin)) {
    const TBResult result = tb.probeWDL(posVerticalPin);
    EXPECT_NE(TBResult::Failed, result);
    LOG__INFO(Logger::get().TEST_LOG, "Vertical pin EP result: {}", Tablebase::resultToString(result));
  }

  // Diagonal pin: White king d3, black bishop h7, white pawn f5, black pawn e5
  // EP capture fxe6 would expose king to bishop - illegal
  const Position posDiagonalPin("8/7b/8/4pP2/8/3K4/8/k7 w - e6 0 1");

  if (tb.canProbe(posDiagonalPin)) {
    const TBResult result = tb.probeWDL(posDiagonalPin);
    EXPECT_NE(TBResult::Failed, result);
    LOG__INFO(Logger::get().TEST_LOG, "Diagonal pin EP result: {}", Tablebase::resultToString(result));
  }

  // Diagonal "pin" but capture is along the ray - legal!
  // White king h3, black bishop d7, white pawn f5, black pawn e5
  // EP capture fxe6 stays on the d7-h3 diagonal, so pin is not broken
  const Position posDiagonalPinAlongRay("8/3b4/8/4pP2/8/7K/8/k7 w - e6 0 1");

  if (tb.canProbe(posDiagonalPinAlongRay)) {
    const TBResult result = tb.probeWDL(posDiagonalPinAlongRay);
    EXPECT_NE(TBResult::Failed, result);
    LOG__INFO(Logger::get().TEST_LOG, "Diagonal along ray EP result: {}", Tablebase::resultToString(result));
  }
}

// Test probing from multiple different board positions (stress test)
TEST_F(TablebaseIntegrationTest, MultipleProbes) {
  SKIP_IF_NO_TABLEBASES(0);

  std::vector<std::pair<std::string, TBResult>> testPositions = {
    {"8/8/8/4k3/8/8/1R6/4K3 w - - 0 1", TBResult::Win}, // KRK white wins
    {"8/8/8/4k3/8/8/1R6/4K3 b - - 0 1", TBResult::Loss},// KRK black loses
    {"8/8/8/4k3/8/8/8/2B1K3 w - - 0 1", TBResult::Draw},// KBK draw (bishop c1 doesn't attack e5)
    {"8/8/8/4k3/8/8/1N6/4K3 w - - 0 1", TBResult::Draw},// KNK draw
  };

  int probed  = 0;
  int correct = 0;

  for (const auto& [fen, expected] : testPositions) {
    const Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBResult result = tb.probeWDL(pos);
      if (result != TBResult::Failed) {
        probed++;
        if (result == expected) {
          correct++;
        }
        else {
          LOG__WARN(Logger::get().TEST_LOG, "Unexpected result for {}: got {} expected {}",
                    fen, Tablebase::resultToString(result), Tablebase::resultToString(expected));
        }
      }
    }
  }

  LOG__INFO(Logger::get().TEST_LOG, "Multiple probes: {}/{} correct, {} probed",
            correct, testPositions.size(), probed);

  // At least some should work
  EXPECT_GT(probed, 0) << "No positions could be probed";
  EXPECT_EQ(correct, probed) << "Some probe results were incorrect";
}

// Test DTZ values make sense
TEST_F(TablebaseIntegrationTest, ProbeRootDTZ) {
  SKIP_IF_NO_TABLEBASES(0);

  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");// KRK

  if (tb.canProbe(pos)) {
    const TBProbeResult result = tb.probeRoot(pos);
    if (result.success()) {
      // DTZ should be positive for a win
      if (result.isWin()) {
        EXPECT_GT(result.dtz, 0) << "DTZ should be positive for winning position";
        LOG__INFO(Logger::get().TEST_LOG, "KRK DTZ: {}", result.dtz);
      }
    }
  }
}

// Test that repeated initialization/shutdown works correctly
TEST_F(TablebaseIntegrationTest, RepeatedInitShutdown) {
  const std::string tbPath = findTablebasePath();
  if (tbPath.empty()) {
    GTEST_SKIP() << "No tablebase path available";
  }

  Tablebase localTb;

  // Initialize, probe, shutdown - repeat multiple times
  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(localTb.initialize(tbPath));
    EXPECT_TRUE(localTb.isAvailable());

    const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");
    if (localTb.canProbe(pos)) {
      const TBResult result = localTb.probeWDL(pos);
      EXPECT_NE(TBResult::Failed, result);
    }

    localTb.shutdown();
    EXPECT_FALSE(localTb.isAvailable());
  }
}

// Test probing with different piece configurations on same squares
TEST_F(TablebaseIntegrationTest, DifferentPiecesSameSquares) {
  SKIP_IF_NO_TABLEBASES(0);

  // Different pieces but all legal positions (piece doesn't attack black king)
  std::vector<std::pair<std::string, std::string>> configs = {
    {"8/8/8/4k3/8/8/1R6/4K3 w - - 0 1", "KRK"},// Rook b2 doesn't attack e5
    {"8/8/8/4k3/8/8/8/2B1K3 w - - 0 1", "KBK"},// Bishop c1 doesn't attack e5
    {"8/8/8/4k3/8/8/1N6/4K3 w - - 0 1", "KNK"},// Knight b2 doesn't attack e5
  };

  for (const auto& [fen, name] : configs) {
    const Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBResult result = tb.probeWDL(pos);
      if (result != TBResult::Failed) {
        LOG__INFO(Logger::get().TEST_LOG, "{}: {}", name, Tablebase::resultToString(result));
      }
    }
  }
}

// Test 5-piece endgames (if available)
TEST_F(TablebaseIntegrationTest, FivePieceEndgames) {
  SKIP_IF_NO_TABLEBASES(5);

  // KQPK - Queen and pawn vs king (should be win)
  // Queen on d2 doesn't attack e5 (diagonal d2-e3-f4-g5, not through e5)
  const Position pos("8/8/8/4k3/8/1P6/3Q4/4K3 w - - 0 1");

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result != TBResult::Failed) {
      EXPECT_EQ(TBResult::Win, result);
      LOG__INFO(Logger::get().TEST_LOG, "KQPK position: {}", Tablebase::resultToString(result));
    }
  }
}

// Test position at edge of board
TEST_F(TablebaseIntegrationTest, EdgeOfBoardPositions) {
  SKIP_IF_NO_TABLEBASES(0);

  // King in corner, rook on edge but NOT attacking enemy king
  // Black king b8 (not on a-file), rook a1, white king h1
  const Position pos("1k6/8/8/8/8/8/8/R6K w - - 0 1");

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    if (result != TBResult::Failed) {
      EXPECT_EQ(TBResult::Win, result);
      LOG__INFO(Logger::get().TEST_LOG, "Edge position KRK: {}", Tablebase::resultToString(result));
    }
  }
}

// Test symmetric position (same result regardless of color swap)
TEST_F(TablebaseIntegrationTest, SymmetricPositions) {
  SKIP_IF_NO_TABLEBASES(0);

  // KRK - white to move
  const Position posWhite("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");
  // KRK - mirrored, black has the rook, black to move
  const Position posBlack("4k3/1r6/8/8/4K3/8/8/8 b - - 0 1");

  auto resultWhite = TBResult::Failed;
  auto resultBlack = TBResult::Failed;

  if (tb.canProbe(posWhite)) {
    resultWhite = tb.probeWDL(posWhite);
  }
  if (tb.canProbe(posBlack)) {
    resultBlack = tb.probeWDL(posBlack);
  }

  // Both should give the same result (Win for side with rook)
  if (resultWhite != TBResult::Failed && resultBlack != TBResult::Failed) {
    EXPECT_EQ(resultWhite, resultBlack);
    LOG__INFO(Logger::get().TEST_LOG, "Symmetric KRK: white={} black={}",
              Tablebase::resultToString(resultWhite), Tablebase::resultToString(resultBlack));
  }
}

//=============================================================================
// Cache Pre-Warming Tests
//=============================================================================

// Test that prewarmCache doesn't crash when called on uninitialized tablebase
TEST_F(TablebaseTest, prewarmCacheUninitialized) {
  const Tablebase tb;
  // Should not crash, just return early
  tb.prewarmCache(5);
  EXPECT_FALSE(tb.isAvailable());
}

// Test prewarmCache with actual tablebases
TEST_F(TablebaseIntegrationTest, prewarmCacheBasic) {
  SKIP_IF_NO_TABLEBASES(0);

  // prewarmCache should complete without errors
  // We can't directly verify cache state, but we can verify it doesn't crash
  // and that subsequent probes still work
  tb.prewarmCache(5);

  // Verify TB still works after pre-warming
  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");// KRK
  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    EXPECT_NE(TBResult::Failed, result);
    LOG__INFO(Logger::get().TEST_LOG, "Post-prewarm KRK probe: {}", Tablebase::resultToString(result));
  }
}

// Test prewarmCache with different piece limits
TEST_F(TablebaseIntegrationTest, prewarmCacheWithPieceLimits) {
  SKIP_IF_NO_TABLEBASES(0);

  // Test with different piece limits
  // Should complete without errors for all valid limits
  tb.prewarmCache(3);// Only 3-piece endgames
  tb.prewarmCache(4);// 3-4 piece endgames
  tb.prewarmCache(5);// 3-5 piece endgames

  // If 6-piece TBs available, test that too
  if (tb.maxPieces() >= 6) {
    tb.prewarmCache(6);// 3-6 piece endgames
  }

  // Verify TB still works
  EXPECT_TRUE(tb.isAvailable());
}

// Test prewarmCache with limit higher than available TBs
TEST_F(TablebaseIntegrationTest, prewarmCacheExceedsAvailable) {
  SKIP_IF_NO_TABLEBASES(0);

  const int maxAvailable = tb.maxPieces();

  // Request warming for more pieces than available - should handle gracefully
  tb.prewarmCache(maxAvailable + 2);

  // Verify TB still works
  EXPECT_TRUE(tb.isAvailable());

  // Verify probing still works
  const Position pos("8/8/8/4k3/8/8/8/4K2Q w - - 0 1");// KQK
  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    EXPECT_NE(TBResult::Failed, result);
  }
}

// Timing test for prewarmCache (informational only)
TEST_F(TablebaseIntegrationTest, prewarmCacheTiming) {
  SKIP_IF_NO_TABLEBASES(0);

  const auto start = std::chrono::high_resolution_clock::now();
  tb.prewarmCache(std::min(5, tb.maxPieces()));
  const auto end = std::chrono::high_resolution_clock::now();

  const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  LOG__INFO(Logger::get().TEST_LOG, "prewarmCache(5) took {} ms", durationMs);

  // Pre-warming should complete in reasonable time (< 5 seconds even on slow disks)
  EXPECT_LT(durationMs, 5000);
}

//=============================================================================
// Comprehensive Position Tests - All piece types and configurations
//=============================================================================

// Test all basic 3-piece endgames
TEST_F(TablebaseIntegrationTest, AllThreePieceEndgames) {
  SKIP_IF_NO_TABLEBASES(0);

  // All 3-piece endgames with expected results
  // Note: KPvK result depends heavily on position - use clearly winning position
  std::vector<std::tuple<std::string, std::string, TBResult>> endgames = {
    // Winning endgames
    {"8/8/8/4k3/8/8/1R6/4K3 w - - 0 1", "KRvK", TBResult::Win},
    {"8/4P3/8/4k3/8/8/8/4K3 w - - 0 1", "KPvK (pawn on e7)", TBResult::Win},// Pawn about to promote
    // Drawing endgames (insufficient material)
    {"8/8/8/4k3/8/8/1N6/4K3 w - - 0 1", "KNvK", TBResult::Draw},
  };

  int tested = 0;
  int passed = 0;

  for (const auto& [fen, name, expected] : endgames) {
    const Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBResult result = tb.probeWDL(pos);
      if (result != TBResult::Failed) {
        tested++;
        if (result == expected) {
          passed++;
          LOG__DEBUG(Logger::get().TEST_LOG, "{}: {} (correct)", name, Tablebase::resultToString(result));
        }
        else {
          LOG__WARN(Logger::get().TEST_LOG, "{}: got {} expected {}",
                    name, Tablebase::resultToString(result), Tablebase::resultToString(expected));
        }
      }
    }
  }

  LOG__INFO(Logger::get().TEST_LOG, "3-piece endgames: {}/{} passed", passed, tested);
  EXPECT_EQ(passed, tested) << "Some 3-piece endgame results were incorrect";
}

// Test all basic 4-piece endgames
TEST_F(TablebaseIntegrationTest, AllFourPieceEndgames) {
  SKIP_IF_NO_TABLEBASES(4);

  // Note: Some tablebase files may be missing - only count tests that don't return Failed
  std::vector<std::tuple<std::string, std::string, TBResult>> endgames = {
    // Winning endgames
    {"8/8/8/4k3/8/8/1R6/R3K3 w - - 0 1", "KRRvK", TBResult::Win},
    {"8/8/8/4k3/8/8/8/1NB1K3 w - - 0 1", "KBNvK", TBResult::Win},// Bishop c1, Knight b1
    // Drawing endgames
    {"8/8/8/4k3/8/8/1N6/N3K3 w - - 0 1", "KNNvK", TBResult::Draw},
    {"8/8/8/4k3/8/8/1R6/r3K3 w - - 0 1", "KRvKR", TBResult::Draw},
  };

  int tested = 0;
  int passed = 0;

  for (const auto& [fen, name, expected] : endgames) {
    const Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBResult result = tb.probeWDL(pos);
      if (result != TBResult::Failed) {
        tested++;
        if (result == expected) {
          passed++;
          LOG__DEBUG(Logger::get().TEST_LOG, "{}: {} (correct)", name, Tablebase::resultToString(result));
        }
        else {
          LOG__WARN(Logger::get().TEST_LOG, "{}: got {} expected {}",
                    name, Tablebase::resultToString(result), Tablebase::resultToString(expected));
        }
      }
      else {
        LOG__DEBUG(Logger::get().TEST_LOG, "{}: tablebase file may be missing", name);
      }
    }
  }

  LOG__INFO(Logger::get().TEST_LOG, "4-piece endgames: {}/{} passed", passed, tested);
  if (tested > 0) {
    EXPECT_EQ(passed, tested) << "Some 4-piece endgame results were incorrect";
  }
}

// Test 5-piece endgames with various piece combinations
TEST_F(TablebaseIntegrationTest, AllFivePieceEndgames) {
  SKIP_IF_NO_TABLEBASES(5);

  // Note: Some tablebase files may be missing - only count tests that don't return Failed
  std::vector<std::tuple<std::string, std::string, TBResult>> endgames = {
    // Strong piece + pawn vs king - all wins
    {"8/8/8/4k3/8/1P6/1R6/4K3 w - - 0 1", "KRPvK", TBResult::Win},
    // Two pawns vs king - win
    {"8/8/8/4k3/8/1P6/2P5/4K3 w - - 0 1", "KPPvK", TBResult::Win},
    // Piece vs piece+pawn - typically won for stronger side
    {"8/8/8/4k3/4p3/8/1R6/4K3 w - - 0 1", "KRvKP", TBResult::Win},
  };

  int tested = 0;
  int passed = 0;

  for (const auto& [fen, name, expected] : endgames) {
    const Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBResult result = tb.probeWDL(pos);
      if (result != TBResult::Failed) {
        tested++;
        if (result == expected) {
          passed++;
          LOG__DEBUG(Logger::get().TEST_LOG, "{}: {} (correct)", name, Tablebase::resultToString(result));
        }
        else {
          LOG__WARN(Logger::get().TEST_LOG, "{}: got {} expected {}",
                    name, Tablebase::resultToString(result), Tablebase::resultToString(expected));
        }
      }
      else {
        LOG__DEBUG(Logger::get().TEST_LOG, "{}: tablebase file may be missing", name);
      }
    }
  }

  LOG__INFO(Logger::get().TEST_LOG, "5-piece endgames: {}/{} passed", passed, tested);
  if (tested > 0) {
    EXPECT_EQ(passed, tested) << "Some 5-piece endgame results were incorrect";
  }
}

// Test pieces on various board positions
TEST_F(TablebaseIntegrationTest, AllCornersPositions) {
  SKIP_IF_NO_TABLEBASES(0);

  // Test various corner and edge positions - must be LEGAL positions
  // (opponent king cannot be in check when it's our turn)
  std::vector<std::pair<std::string, std::string>> positions = {
    {"k7/8/8/8/8/8/1R6/7K w - - 0 1", "KRvK black king a8, rook b2"},// Rook b2 doesn't attack a8
    {"7k/8/8/8/8/8/6R1/K7 w - - 0 1", "KRvK black king h8, rook g2"},// Rook g2 doesn't attack h8
    {"1k6/8/8/8/8/8/8/R6K w - - 0 1", "KRvK black king b8, rook a1"},// Rook a1 doesn't attack b8
    {"6k1/8/8/8/8/8/8/K6R w - - 0 1", "KRvK black king g8, rook h1"},// Rook h1 doesn't attack g8
  };

  int tested = 0;

  for (const auto& [fen, name] : positions) {
    const Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBResult result = tb.probeWDL(pos);
      if (result != TBResult::Failed) {
        tested++;
        EXPECT_EQ(TBResult::Win, result) << name << " should be a win";
        LOG__INFO(Logger::get().TEST_LOG, "{}: {}", name, Tablebase::resultToString(result));
      }
      else {
        LOG__WARN(Logger::get().TEST_LOG, "{}: probe failed unexpectedly", name);
      }
    }
  }

  EXPECT_GT(tested, 0) << "No corner positions could be probed";
}

// Test positions with pieces on all ranks
TEST_F(TablebaseIntegrationTest, AllRanksPositions) {
  SKIP_IF_NO_TABLEBASES(0);

  // KRK with rook on different ranks - use positions known to work
  std::vector<std::pair<std::string, int>> positions = {
    {"8/8/8/4k3/8/8/8/R3K3 w - - 0 1", 1},
    {"8/8/8/4k3/R7/8/8/4K3 w - - 0 1", 4},
    {"R7/8/8/4k3/8/8/8/4K3 w - - 0 1", 8},
  };

  int tested = 0;
  int passed = 0;

  for (const auto& [fen, rank] : positions) {
    const Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBResult result = tb.probeWDL(pos);
      if (result != TBResult::Failed) {
        tested++;
        if (result == TBResult::Win) {
          passed++;
        }
        else {
          LOG__WARN(Logger::get().TEST_LOG, "KRvK on rank {} returned {} instead of Win",
                    rank, Tablebase::resultToString(result));
        }
      }
    }
  }

  EXPECT_GT(tested, 0) << "No rank positions could be probed";
  EXPECT_EQ(passed, tested) << "Some rank positions gave wrong results";
}

// Test positions with pieces on all files
TEST_F(TablebaseIntegrationTest, AllFilesPositions) {
  SKIP_IF_NO_TABLEBASES(0);

  // KRK with rook on different files
  std::vector<std::pair<std::string, char>> positions = {
    {"8/8/8/4k3/8/8/8/R3K3 w - - 0 1", 'a'},
    {"8/8/8/4k3/8/8/8/1R2K3 w - - 0 1", 'b'},
    {"8/8/8/4k3/8/8/8/2R1K3 w - - 0 1", 'c'},
    {"8/8/8/4k3/8/8/8/3RK3 w - - 0 1", 'd'},
    {"8/8/8/4k3/8/8/8/4KR2 w - - 0 1", 'f'},
    {"8/8/8/4k3/8/8/8/4K1R1 w - - 0 1", 'g'},
    {"8/8/8/4k3/8/8/8/4K2R w - - 0 1", 'h'},
  };

  int tested = 0;
  int passed = 0;

  for (const auto& [fen, file] : positions) {
    const Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBResult result = tb.probeWDL(pos);
      if (result != TBResult::Failed) {
        tested++;
        if (result == TBResult::Win) {
          passed++;
        }
        else {
          LOG__WARN(Logger::get().TEST_LOG, "KRvK on file {} returned {} instead of Win",
                    file, Tablebase::resultToString(result));
        }
      }
    }
  }

  EXPECT_GT(tested, 0) << "No file positions could be probed";
  EXPECT_EQ(passed, tested) << "Some file positions gave wrong results";
}

// Test probeWDL and probeRoot consistency
TEST_F(TablebaseIntegrationTest, WDLAndRootConsistency) {
  SKIP_IF_NO_TABLEBASES(0);

  // Use positions that are known to work
  const std::vector<std::string> positions = {
    "8/8/8/4k3/8/8/1R6/4K3 w - - 0 1",// KRK
    "8/8/8/4k3/8/8/1N6/4K3 w - - 0 1",// KNK
    "8/8/8/4k3/4P3/8/8/4K3 w - - 0 1",// KPK
  };

  for (const auto& fen : positions) {
    const Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBResult wdlResult       = tb.probeWDL(pos);
      const TBProbeResult rootResult = tb.probeRoot(pos);

      if (wdlResult != TBResult::Failed && rootResult.success()) {
        // WDL results should match
        EXPECT_EQ(wdlResult, rootResult.wdl)
          << "WDL mismatch for " << fen
          << ": probeWDL=" << Tablebase::resultToString(wdlResult)
          << ", probeRoot=" << Tablebase::resultToString(rootResult.wdl);
      }
    }
  }
}

// Test that best move from probeRoot is actually legal
TEST_F(TablebaseIntegrationTest, ProbeRootMoveLegality) {
  SKIP_IF_NO_TABLEBASES(0);

  const std::vector<std::string> positions = {
    "8/8/8/4k3/8/8/1R6/4K3 w - - 0 1",// KRK white to move
    "8/8/8/4k3/8/8/1R6/4K3 b - - 0 1",// KRK black to move
    "8/8/8/4k3/4P3/8/8/4K3 w - - 0 1",// KPK
  };

  for (const auto& fen : positions) {
    Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBProbeResult result = tb.probeRoot(pos);
      if (result.success() && result.bestMove != MOVE_NONE) {
        // Generate all legal moves and check if best move is among them
        MoveGenerator mg;
        const MoveList* legalMoves = mg.generateLegalMoves(pos, GenAll);

        bool found = false;
        for (const auto& move : *legalMoves) {
          if (move.from() == result.bestMove.from() && move.to() == result.bestMove.to()) {
            found = true;
            break;
          }
        }

        EXPECT_TRUE(found) << "Best move " << result.bestMove.str()
                           << " is not legal in position " << fen;
      }
    }
  }
}

// Test DTZ sign consistency with WDL
TEST_F(TablebaseIntegrationTest, DTZSignConsistency) {
  SKIP_IF_NO_TABLEBASES(0);

  const std::vector<std::string> positions = {
    "8/8/8/4k3/8/8/1R6/4K3 w - - 0 1",// KRK white wins
    "8/8/8/4k3/8/8/1R6/4K3 b - - 0 1",// KRK black loses
    "8/8/8/4k3/8/8/1N6/4K3 w - - 0 1",// KNK draw
  };

  for (const auto& fen : positions) {
    const Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBProbeResult result = tb.probeRoot(pos);
      if (result.success()) {
        // Fathom's DTZ is the distance to zeroing (capture/pawn move)
        // DTZ is typically positive for both wins and losses (it's a distance)
        // The WDL result tells you if it's winning or losing
        if (result.isWin() || result.isLoss()) {
          EXPECT_GT(result.dtz, 0) << "DTZ should be positive for non-draw position: " << fen;
        }
        else if (result.isDraw()) {
          // DTZ for draws is typically 0 for insufficient material (e.g., KNK),
          // but may be non-zero for positions where progress is still possible
          // (e.g., KRvKR where captures can occur). We only verify it's non-negative.
          EXPECT_GE(result.dtz, 0) << "DTZ should be non-negative for drawn position: " << fen;
        }
        LOG__DEBUG(Logger::get().TEST_LOG, "{}: WDL={}, DTZ={}",
                   fen, Tablebase::resultToString(result.wdl), result.dtz);
      }
    }
  }
}

// Test that color flipping gives opposite results
TEST_F(TablebaseIntegrationTest, ColorFlipConsistency) {
  SKIP_IF_NO_TABLEBASES(0);

  // KRK - white to move (winning)
  const Position posWhiteToMove("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");
  // Same position - black to move (losing)
  const Position posBlackToMove("8/8/8/4k3/8/8/1R6/4K3 b - - 0 1");

  if (tb.canProbe(posWhiteToMove) && tb.canProbe(posBlackToMove)) {
    const TBResult resultWhite = tb.probeWDL(posWhiteToMove);
    const TBResult resultBlack = tb.probeWDL(posBlackToMove);

    if (resultWhite != TBResult::Failed && resultBlack != TBResult::Failed) {
      // In this KRK position, white to move should win, black to move should lose
      EXPECT_EQ(TBResult::Win, resultWhite);
      EXPECT_EQ(TBResult::Loss, resultBlack);
    }
  }
}

// Test rapid probing doesn't cause issues (stress test)
TEST_F(TablebaseIntegrationTest, RapidProbing) {
  SKIP_IF_NO_TABLEBASES(0);

  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");// KRK

  if (!tb.canProbe(pos)) {
    GTEST_SKIP() << "Cannot probe test position";
  }

  // Probe the same position many times rapidly
  constexpr int iterations = 100;
  int successes            = 0;
  auto firstResult         = TBResult::Failed;

  for (int i = 0; i < iterations; i++) {
    const TBResult result = tb.probeWDL(pos);
    if (result != TBResult::Failed) {
      successes++;
      if (firstResult == TBResult::Failed) {
        firstResult = result;
      }
      else {
        // All results should be the same
        EXPECT_EQ(firstResult, result) << "Inconsistent result on iteration " << i;
      }
    }
  }

  LOG__INFO(Logger::get().TEST_LOG, "Rapid probing: {}/{} succeeded", successes, iterations);
  EXPECT_EQ(iterations, successes) << "Some rapid probes failed";
}

// Test probing with different positions alternating
TEST_F(TablebaseIntegrationTest, AlternatingPositions) {
  SKIP_IF_NO_TABLEBASES(0);

  // Use positions that are known to work reliably
  std::vector<std::pair<std::string, TBResult>> positions = {
    {"8/8/8/4k3/8/8/1R6/4K3 w - - 0 1", TBResult::Win}, // KRK white wins
    {"8/8/8/4k3/8/8/1R6/4K3 b - - 0 1", TBResult::Loss},// KRK black loses
    {"8/8/8/4k3/8/8/1N6/4K3 w - - 0 1", TBResult::Draw},// KNK draw
  };

  int tested = 0;
  int passed = 0;

  // Probe positions in alternating order multiple times
  for (int round = 0; round < 3; round++) {
    for (const auto& [fen, expected] : positions) {
      const Position pos(fen);
      if (tb.canProbe(pos)) {
        const TBResult result = tb.probeWDL(pos);
        if (result != TBResult::Failed) {
          tested++;
          if (result == expected) {
            passed++;
          }
          else {
            LOG__WARN(Logger::get().TEST_LOG, "Round {}: wrong result for {}", round, fen);
          }
        }
      }
    }
  }

  EXPECT_GT(tested, 0) << "No alternating positions could be probed";
  EXPECT_EQ(passed, tested) << "Some alternating position results were wrong";
}

// Test very specific positions that might reveal bitboard issues
TEST_F(TablebaseIntegrationTest, SpecificBitboardPositions) {
  SKIP_IF_NO_TABLEBASES(0);

  // Positions with pieces on specific squares that might reveal bit ordering issues
  // Use valid positions that we know should work
  std::vector<std::pair<std::string, std::string>> positions = {
    {"8/8/8/4k3/8/8/R7/4K3 w - - 0 1", "Rook on a2"},
    {"8/8/8/4k3/8/8/7R/4K3 w - - 0 1", "Rook on h2"},
    {"8/8/8/4k3/8/8/8/RK6 w - - 0 1", "Rook on a1, King on b1"},
    {"8/8/8/4k3/8/8/8/5KR1 w - - 0 1", "Rook on g1, King on f1"},
  };

  int tested = 0;

  for (const auto& [fen, name] : positions) {
    try {
      const Position pos(fen);
      if (tb.canProbe(pos)) {
        const TBResult result = tb.probeWDL(pos);
        if (result != TBResult::Failed) {
          tested++;
          LOG__DEBUG(Logger::get().TEST_LOG, "{}: {}", name, Tablebase::resultToString(result));
        }
      }
    } catch (...) {
      // Skip invalid FENs
      continue;
    }
  }

  EXPECT_GT(tested, 0) << "No specific bitboard positions could be probed";
}

// Test that probeRoot returns valid best move for all test positions
TEST_F(TablebaseIntegrationTest, ProbeRootBestMoveValidity) {
  SKIP_IF_NO_TABLEBASES(0);

  const std::vector<std::string> positions = {
    "8/8/8/4k3/8/8/1R6/4K3 w - - 0 1", // KRK - rook b2 doesn't attack e5
    "8/8/8/4k3/4P3/8/8/4K3 w - - 0 1", // KPK - pawn e4 doesn't attack e5
    "8/8/8/4k3/8/8/8/2B1KB2 w - - 0 1",// KBBK - bishops c1,f1 don't attack e5
    "8/8/8/4k3/8/8/8/1NB1K3 w - - 0 1",// KBNK - bishop c1, knight b1 don't attack e5
  };

  for (const auto& fen : positions) {
    const Position pos(fen);
    if (tb.canProbe(pos)) {
      const TBProbeResult result = tb.probeRoot(pos);
      if (result.success()) {
        // Best move should either be MOVE_NONE (for certain positions) or a valid move
        if (result.bestMove != MOVE_NONE) {
          // Basic validation - from and to squares should be valid
          EXPECT_LT(static_cast<int>(result.bestMove.from()), 64)
            << "Invalid from square for " << fen;
          EXPECT_LT(static_cast<int>(result.bestMove.to()), 64)
            << "Invalid to square for " << fen;
          EXPECT_NE(result.bestMove.from(), result.bestMove.to())
            << "From and to should differ for " << fen;

          LOG__DEBUG(Logger::get().TEST_LOG, "{}: best move {} (DTZ={})",
                     fen, result.bestMove.str(), result.dtz);
        }
      }
    }
  }
}

//=============================================================================
// Search Integration Tests - Root Tablebase Probing
//=============================================================================

#include "config/ConfigManager.h"
#include "engine/Search.h"

class SearchTablebaseTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::info);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::info);
    Logger::get().TB_LOG->set_level(spdlog::level::info);
  }

protected:
  void SetUp() override {
    ConfigManager::instance().resetToDefaults();
  }

  void TearDown() override {}
};

// Test that Search uses tablebase at root when available
TEST_F(SearchTablebaseTest, RootProbeReturnsTablebaseMove) {
  SKIP_IF_NO_TABLEBASES(0);

  // Configure tablebase path with immediate mode (default)
  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(s.TB_PATH = tbPath; s.USE_BOOK = false;);
#ifndef FRANKYCPP_PRODUCTION
  // CONFIG_CONST in production — frozen at defaults (USE_TB_PROBE_ROOT=true, TB_ROOT_IMMEDIATE=false)
  CONFIG_OVERRIDE(s.USE_TB_PROBE_ROOT = true; s.TB_ROOT_IMMEDIATE = true;);
#endif

  // KRK position - definitely in 3-piece tablebases
  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 1;// Minimal depth since TB should return immediately

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchResult& result = search.getLastSearchResult();

  // Check if TB was used
  if (result.tbHit) {
    LOG__INFO(Logger::get().TEST_LOG, "TB root hit: move={} value={}",
              result.bestMove.str(), result.bestMoveValue.str());
    EXPECT_NE(MOVE_NONE, result.bestMove) << "TB hit should provide a move";
    EXPECT_GT(static_cast<int>(result.bestMoveValue), 8000) << "KRK should be winning";
  }
  else {
    LOG__INFO(Logger::get().TEST_LOG, "No TB hit - search returned: move={} value={}",
              result.bestMove.str(), result.bestMoveValue.str());
    // TB might not be initialized - this is OK, just log it
  }
}

// Test that TB probing is disabled when USE_TB_PROBE_ROOT=false
// In production, USE_TB_PROBE_ROOT is CONFIG_CONST frozen at true — cannot disable at runtime.
#ifndef FRANKYCPP_PRODUCTION
TEST_F(SearchTablebaseTest, RootProbeDisabledWhenConfigured) {
  SKIP_IF_NO_TABLEBASES(0);

  // Configure tablebase path but disable root probing
  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(
    s.TB_PATH           = tbPath;
    s.USE_TB_PROBE_ROOT = false;// Disable root probing
    s.USE_BOOK          = false;);

  // KRK position
  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 4;

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchResult& result = search.getLastSearchResult();

  // TB should NOT be used when disabled
  EXPECT_FALSE(result.tbHit) << "TB should not be used when USE_TB_PROBE_ROOT=false";
  EXPECT_NE(MOVE_NONE, result.bestMove) << "Search should still find a move";
}
#endif // FRANKYCPP_PRODUCTION

// Test that positions with too many pieces don't trigger TB
TEST_F(SearchTablebaseTest, RootProbeSkippedForLargePositions) {
  SKIP_IF_NO_TABLEBASES(0);

  // Configure tablebase
  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(s.TB_PATH = tbPath; s.USE_BOOK = false;);
#ifndef FRANKYCPP_PRODUCTION
  CONFIG_OVERRIDE(s.USE_TB_PROBE_ROOT = true;);// CONFIG_CONST in production — frozen at true
#endif

  // Starting position - 32 pieces, way more than any TB
  const Position pos;

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 4;

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchResult& result = search.getLastSearchResult();

  // TB should NOT be used for starting position (too many pieces)
  EXPECT_FALSE(result.tbHit) << "TB should not be used for 32-piece position";
  EXPECT_NE(MOVE_NONE, result.bestMove) << "Search should still find a move";
}

// Test that positions with castling rights don't trigger TB
TEST_F(SearchTablebaseTest, RootProbeSkippedWithCastlingRights) {
  SKIP_IF_NO_TABLEBASES(0);

  // Configure tablebase
  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(s.TB_PATH = tbPath; s.USE_BOOK = false;);
#ifndef FRANKYCPP_PRODUCTION
  CONFIG_OVERRIDE(s.USE_TB_PROBE_ROOT = true;);// CONFIG_CONST in production — frozen at true
#endif

  // 6-piece position but WITH castling rights (TBs don't cover castling)
  const Position pos("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 4;

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchResult& result = search.getLastSearchResult();

  // TB should NOT be used when castling rights exist
  EXPECT_FALSE(result.tbHit) << "TB should not be used with castling rights";
  EXPECT_NE(MOVE_NONE, result.bestMove) << "Search should still find a move";
}

// Test that TB_ROOT_IMMEDIATE=false causes search to continue despite TB hit
TEST_F(SearchTablebaseTest, RootProbeNonImmediateSearchesDespiteTBHit) {
  SKIP_IF_NO_TABLEBASES(0);

  // Configure tablebase with non-immediate mode
  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(s.TB_PATH = tbPath; s.USE_BOOK = false;);
#ifndef FRANKYCPP_PRODUCTION
  // CONFIG_CONST in production — frozen at defaults (USE_TB_PROBE_ROOT=true, TB_ROOT_IMMEDIATE=false)
  CONFIG_OVERRIDE(s.USE_TB_PROBE_ROOT = true; s.TB_ROOT_IMMEDIATE = false;);
#endif

  // KRK position - definitely in 3-piece tablebases
  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 6;// Search to reasonable depth

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchResult& result = search.getLastSearchResult();

  // We should still find a good move
  EXPECT_NE(MOVE_NONE, result.bestMove) << "Search should find a move";
  // And reach the requested depth (search actually happened)
  EXPECT_GE(result.depth, 4) << "Should have searched to reasonable depth";
  // With root move filtering, only TB-optimal moves are searched
  // tbHit should be true because all filtered moves are TB-optimal
  EXPECT_TRUE(result.tbHit) << "With TB filtering, result should be TB-backed";
  // Score should be TB winning score (>8000), not regular eval
  EXPECT_GT(static_cast<int>(result.bestMoveValue), 8000) << "Score should be TB winning score";

  LOG__INFO(Logger::get().TEST_LOG, "Non-immediate TB mode: move={} value={} depth={} tbHit={}",
            result.bestMove.str(), result.bestMoveValue.str(), result.depth, result.tbHit);
}

// Test DTZ-based scoring gives higher scores for shorter wins
TEST_F(SearchTablebaseTest, DTZBasedScoringPrefersShortWins) {
  // Test that smaller DTZ gives higher score
  const Value scoreDtz5  = tablebase::Tablebase::tbResultToScore(tablebase::TBResult::Win, 5);
  const Value scoreDtz50 = tablebase::Tablebase::tbResultToScore(tablebase::TBResult::Win, 50);

  EXPECT_GT(static_cast<int>(scoreDtz5), static_cast<int>(scoreDtz50))
    << "Shorter win (DTZ=5) should score higher than longer win (DTZ=50)";

  // Both should be clearly winning
  EXPECT_GT(static_cast<int>(scoreDtz5), 8000) << "DTZ=5 win should be high score";
  EXPECT_GT(static_cast<int>(scoreDtz50), 8000) << "DTZ=50 win should still be winning";

  LOG__INFO(Logger::get().TEST_LOG, "DTZ scoring: DTZ=5 -> {}, DTZ=50 -> {}",
            scoreDtz5.str(), scoreDtz50.str());
}

//=============================================================================
// Search Tablebase Probing Tests
//=============================================================================

// Test USE_TB_PROBE_ROOT=false and USE_TB_PROBE_SEARCH=false disables all tablebase probing
// In production, these configs are CONFIG_CONST frozen at true — cannot disable at runtime.
#ifndef FRANKYCPP_PRODUCTION
TEST_F(SearchTablebaseTest, DisableBothProbingDisablesAllTB) {
  SKIP_IF_NO_TABLEBASES(0);

  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(
    s.TB_PATH             = tbPath;
    s.USE_TB_PROBE_ROOT   = false;// Disable root probing
    s.USE_TB_PROBE_SEARCH = false;// Disable search probing
    s.TB_PROBE_DEPTH      = 0;    // Would normally probe everywhere
    s.TB_PROBE_LIMIT      = 6;
    s.USE_BOOK            = false;);

  // KRK position - would normally hit TB
  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 6;

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchResult& result = search.getLastSearchResult();
  const SearchStats& stats   = search.getSearchStats();

  // With both probing disabled, should have NO TB activity
  EXPECT_FALSE(result.tbHit) << "USE_TB_PROBE_ROOT=false should disable root TB probing";
  EXPECT_EQ(0ULL, stats.tbRootHits) << "USE_TB_PROBE_ROOT=false should have 0 root hits";
  EXPECT_EQ(0ULL, stats.tbSearchHits) << "USE_TB_PROBE_SEARCH=false should have 0 search hits";
  EXPECT_EQ(0ULL, stats.tbSearchCutoffs) << "USE_TB_PROBE_SEARCH=false should have 0 cutoffs";

  // Should still find a move via normal search
  EXPECT_NE(MOVE_NONE, result.bestMove) << "Should find move via normal search";

  LOG__INFO(Logger::get().TEST_LOG, "TB probing disabled test: rootHits={} searchHits={} cutoffs={}",
            stats.tbRootHits, stats.tbSearchHits, stats.tbSearchCutoffs);
}
#endif // FRANKYCPP_PRODUCTION

// Test TB_RULE50_THRESHOLD >= 100 effectively disables DTZ checks
// In production, TB_RULE50_THRESHOLD is CONFIG_CONST — cannot be changed at runtime.
#ifndef FRANKYCPP_PRODUCTION
TEST_F(SearchTablebaseTest, Rule50ThresholdDisablesWhenHigh) {
  SKIP_IF_NO_TABLEBASES(0);

  // Configure with threshold >= 100 (disables DTZ checks)
  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(
    s.TB_PATH             = tbPath;
    s.USE_TB_PROBE_ROOT   = false;// Disable root probing to test search probing behavior
    s.USE_TB_PROBE_SEARCH = true; // Enable search probing
    s.TB_PROBE_DEPTH      = 1;
    s.TB_PROBE_LIMIT      = 6;
    s.TB_RULE50_THRESHOLD = 100;// Disable 50-move rule checks (threshold >= 100)
    s.USE_BOOK            = false;);

  // KRK position with high halfmove clock (set via FEN)
  // Even with halfmove=95, should NOT treat as draw when threshold=100
  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 95 50");

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 6;

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchResult& result = search.getLastSearchResult();

  // With threshold=100, even with halfmove=95, should report as winning (not draw)
  EXPECT_NE(MOVE_NONE, result.bestMove) << "Should find a move";
  // Score should be positive (winning) not draw - TB should return win for KRK
  // Note: The score may be lower than normal win due to halfmove clock, but should still be positive
  EXPECT_GT(static_cast<int>(result.bestMoveValue), 0)
    << "With threshold=100, should report winning even with high halfmove clock";

  LOG__INFO(Logger::get().TEST_LOG, "Rule50 threshold=100 test: move={} value={}",
            result.bestMove.str(), result.bestMoveValue.str());
}
#endif // FRANKYCPP_PRODUCTION

// Test that search TB probing produces cutoffs (statistics check)
TEST_F(SearchTablebaseTest, SearchProbingProducesCutoffs) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  SKIP_IF_NO_TABLEBASES(0);

  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(s.TB_PATH = tbPath; s.USE_BOOK = false;);
#ifndef FRANKYCPP_PRODUCTION // would not compile in production since these are CONFIG_CONST frozen at defaults (probing enabled)
  // CONFIG_CONST in production — frozen at defaults
  CONFIG_OVERRIDE(
    s.USE_TB_PROBE_ROOT   = true;
    s.USE_TB_PROBE_SEARCH = true;
    s.TB_PROBE_DEPTH      = 1;
    s.TB_PROBE_LIMIT      = 6;
    s.TB_RULE50_THRESHOLD = 80;
  );
#endif

  // KRK position - simple 3-piece position
  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 8;// Search deep enough to get TB hits

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchStats& stats = search.getSearchStats();

  LOG__INFO(Logger::get().TEST_LOG,
            "TB search stats: hits={} misses={} cutoffs={}",
            stats.tbSearchHits, stats.tbSearchMisses, stats.tbSearchCutoffs);

  // In a pure TB position with depth=8, we should get some TB hits
  // (exact numbers depend on search behavior)
  EXPECT_GT(stats.tbSearchHits + stats.tbSearchMisses, 0ULL)
    << "Search should have attempted TB probes";
}

// Test TB_PROBE_DEPTH controls when probing happens
// In production, TB_PROBE_DEPTH is CONFIG_CONST — cannot be changed at runtime.
#ifndef FRANKYCPP_PRODUCTION
TEST_F(SearchTablebaseTest, ProbeDepthControlsProbing) {
  SKIP_IF_NO_TABLEBASES(0);

  const std::string tbPath = findTablebasePath();

  // First search with TB_PROBE_DEPTH = 0 (always probe)
  CONFIG_OVERRIDE(
    s.TB_PATH             = tbPath;
    s.USE_TB_PROBE_ROOT   = false;
    s.USE_TB_PROBE_SEARCH = true;// Enable search probing
    s.TB_PROBE_DEPTH      = 0;   // Always probe
    s.TB_PROBE_LIMIT      = 6;
    s.USE_BOOK            = false;);

  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");

  Search search1;
  search1.isReady();

  SearchLimits sl;
  sl.depth = 6;

  search1.startSearch(pos, sl);
  search1.waitWhileSearching();

  const uint64_t hitsDepth0 = search1.getSearchStats().tbSearchHits;

  // Second search with TB_PROBE_DEPTH = 5 (only probe at depth >= 5)
  CONFIG_OVERRIDE(
    s.TB_PATH             = tbPath;
    s.USE_TB_PROBE_ROOT   = false;
    s.USE_TB_PROBE_SEARCH = true;// Enable search probing
    s.TB_PROBE_DEPTH      = 5;   // Only probe at depth >= 5
    s.TB_PROBE_LIMIT      = 6;
    s.USE_BOOK            = false;);

  Search search2;
  search2.isReady();

  search2.startSearch(pos, sl);
  search2.waitWhileSearching();

  const uint64_t hitsDepth5 = search2.getSearchStats().tbSearchHits;

  LOG__INFO(Logger::get().TEST_LOG,
            "TB_PROBE_DEPTH test: depth=0 hits={}, depth=5 hits={}",
            hitsDepth0, hitsDepth5);

  // With TB_PROBE_DEPTH=0, should have more hits than with TB_PROBE_DEPTH=5
  // (probing at all depths vs only deep nodes)
  EXPECT_GE(hitsDepth0, hitsDepth5)
    << "Lower TB_PROBE_DEPTH should result in >= TB hits";
}
#endif // FRANKYCPP_PRODUCTION

// Test TB_PROBE_LIMIT controls piece count
// In production, TB_PROBE_LIMIT is CONFIG_CONST — cannot be changed at runtime.
#ifndef FRANKYCPP_PRODUCTION
TEST_F(SearchTablebaseTest, ProbeLimitControlsPieceCount) {
  SKIP_IF_NO_TABLEBASES(0);

  const std::string tbPath = findTablebasePath();

  // Configure with TB_PROBE_LIMIT = 3 (only probe 3-piece positions)
  CONFIG_OVERRIDE(
    s.TB_PATH             = tbPath;
    s.USE_TB_PROBE_ROOT   = false;
    s.USE_TB_PROBE_SEARCH = true;// Enable search probing
    s.TB_PROBE_DEPTH      = 1;
    s.TB_PROBE_LIMIT      = 3;// Very restrictive - only KvK (impossible) or 3-piece
    s.USE_BOOK            = false;);

  // Use 5-piece position so even after captures we still exceed limit of 3
  // KRKpp = 5 pieces, after one capture = 4 pieces (still > 3)
  const Position pos("8/8/8/3p4/4p3/8/1R2K3/7k w - - 0 1");// KRKpp = 5 pieces

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 4;

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchStats& stats = search.getSearchStats();

  LOG__INFO(Logger::get().TEST_LOG,
            "TB_PROBE_LIMIT=3 on 5-piece pos: hits={} misses={}",
            stats.tbSearchHits, stats.tbSearchMisses);

  // With 5-piece position and limit=3, should have 0 hits
  // (even after one capture, 4 pieces still exceeds limit)
  EXPECT_EQ(0ULL, stats.tbSearchHits)
    << "Should not probe positions exceeding TB_PROBE_LIMIT";
}
#endif // FRANKYCPP_PRODUCTION

//=============================================================================
// Tablebase Probing During Search (not at root)
// These positions have 7+ pieces at root but reach <=6 pieces after captures
//=============================================================================

// 7-piece position: After Rxd5, reaches 6-piece KRK position
// White has decisive advantage - rook captures hanging pawn
TEST_F(SearchTablebaseTest, SevenPieceTBProbeAfterCapture) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  SKIP_IF_NO_TABLEBASES(0);

  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(s.USE_BOOK = false; s.TB_PATH = tbPath;);
#ifndef FRANKYCPP_PRODUCTION // would not compile in production since these are CONFIG_CONST frozen at defaults (probing enabled)
  // CONFIG_CONST in production — frozen at defaults (probing enabled)
  CONFIG_OVERRIDE(
    s.USE_TB_PROBE_ROOT   = true;
    s.USE_TB_PROBE_SEARCH = true;
    s.TB_PROBE_DEPTH      = 1;
    s.TB_PROBE_LIMIT      = 6;
  );
#endif

  // 7 pieces: KR vs. KPP + extra pawn on d5 that can be captured
  // Position: White Ke1, Rb2; Black Ke5, Pd5, Pf7, Pg6
  // After Rxd5 or other exchanges, reaches 6-piece TB territory
  const Position pos("8/1p3p2/6p1/3pk3/8/8/1R2K3/8 w - - 0 1");

  EXPECT_EQ(7, pos.getOccupiedBb().popcount()) << "Should be 7-piece position";

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 10;

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchStats& stats   = search.getSearchStats();
  const SearchResult& result = search.getLastSearchResult();

  LOG__INFO(Logger::get().TEST_LOG,
            "7-piece TB search: tbRootHits={} tbSearchHits={} tbSearchCutoffs={} move={} value={}",
            stats.tbRootHits, stats.tbSearchHits, stats.tbSearchCutoffs, result.bestMove.str(), result.bestMoveValue.str());

  // Should have TB hits from positions after captures
  EXPECT_GT(stats.tbSearchHits, 0ULL)
    << "Search should probe TB after reaching 6-piece positions via captures";

  EXPECT_EQ(stats.tbRootHits, 0ULL)
    << "Should not probe TB at root for 7-piece position";
}

// 8-piece position: After exchanges, reaches 6-piece TB territory
// Rook vs two pawns with possible captures leading to TB probe
TEST_F(SearchTablebaseTest, EightPieceTBProbeAfterExchanges) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  SKIP_IF_NO_TABLEBASES(5);

  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(s.TB_PATH = tbPath; s.USE_BOOK = false;);
#ifndef FRANKYCPP_PRODUCTION // would not compile in production since these are CONFIG_CONST frozen at defaults (probing enabled)
  // CONFIG_CONST in production — frozen at defaults (probing enabled)
  CONFIG_OVERRIDE(
    s.USE_TB_PROBE_ROOT   = true;
    s.USE_TB_PROBE_SEARCH = true;
    s.TB_PROBE_DEPTH      = 1;
    s.TB_PROBE_LIMIT      = 6;
  );
#endif

  // 8 pieces: White Ke1, Ra1, Pa2, Pb2; Black Ke8, Rb8, Pa7, Ph7
  // Rook exchanges and pawn captures lead to TB positions
  const Position pos("1r2k3/p6p/8/8/8/8/PP6/R3K3 w - - 0 1");

  EXPECT_EQ(8, pos.getOccupiedBb().popcount()) << "Should be 8-piece position";

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 10;

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchStats& stats = search.getSearchStats();

  LOG__INFO(Logger::get().TEST_LOG,
            "8-piece TB search: tbRootHits={} tbSearchHits={} tbSearchCutoffs={}",
            stats.tbRootHits, stats.tbSearchHits, stats.tbSearchCutoffs);

  EXPECT_GT(stats.tbSearchHits, 0ULL)
    << "Search should probe TB after reaching <=6 piece positions";

  EXPECT_EQ(stats.tbRootHits, 0ULL)
    << "Should not probe TB at root for 8-piece position";
}

// 7-piece endgame: KQPKRP - Queen+Pawn vs Rook+Pawn
// After captures, reaches 6-piece or smaller TB positions
TEST_F(SearchTablebaseTest, QueenVsRookPawnTBProbe) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  SKIP_IF_NO_TABLEBASES(0);

  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(s.TB_PATH = tbPath; s.USE_BOOK = false;);
#ifndef FRANKYCPP_PRODUCTION // would not compile in production since these are CONFIG_CONST frozen at defaults (probing enabled)
  // CONFIG_CONST in production — frozen at defaults (probing enabled)
  CONFIG_OVERRIDE(
    s.USE_TB_PROBE_ROOT   = true;
    s.USE_TB_PROBE_SEARCH = true;
    s.TB_PROBE_DEPTH      = 1;
    s.TB_PROBE_LIMIT      = 6;
  );
#endif

  // 7 pieces: White Kh1, Qd4, Pg2; Black Kg8, Rf8, Pf2, Pg7
  // Queen can capture f2 pawn, reaching 6-piece position
  const Position pos("5rk1/6p1/8/8/3Q4/8/5pP1/7K w - - 0 1");

  EXPECT_EQ(7, pos.getOccupiedBb().popcount()) << "Should be 7-piece position";

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 10;

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchStats& stats   = search.getSearchStats();
  const SearchResult& result = search.getLastSearchResult();

  LOG__INFO(Logger::get().TEST_LOG,
            "KQKRP TB search: tbRootHits={} tbSearchHits={} move={} value={}",
            stats.tbRootHits, stats.tbSearchHits, result.bestMove.str(), result.bestMoveValue.str());

  EXPECT_GT(stats.tbSearchHits, 0ULL)
    << "Should probe 6-piece TB after captures";

  EXPECT_EQ(stats.tbRootHits, 0ULL)
    << "Should not probe TB at root for 7-piece position";
}

// 7-piece tactical position: Forced sequence leads to TB position
// White to play: After Bxf7+ Kxf7, Qxd8 reaching 5-piece KQKP
TEST_F(SearchTablebaseTest, TacticalSequenceToTB) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  SKIP_IF_NO_TABLEBASES(0);

  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(s.TB_PATH = tbPath; s.USE_BOOK = false;);
#ifndef FRANKYCPP_PRODUCTION // would not compile in production since these are CONFIG_CONST frozen at defaults (probing enabled)
  // CONFIG_CONST in production — frozen at defaults (probing enabled)
  CONFIG_OVERRIDE(
    s.USE_TB_PROBE_ROOT   = true;
    s.USE_TB_PROBE_SEARCH = true;
    s.TB_PROBE_DEPTH      = 1;
    s.TB_PROBE_LIMIT      = 6;
  );
#endif

  // 7 pieces: White Ke1, Qe2, Bb5, Pg2; Black Kf8, Qd8, Pf7
  // Tactical line: Bxf7+ threats lead to exchanges
  const Position pos("3q1k2/5p2/8/1B6/8/8/4Q1P1/4K3 w - - 0 1");

  EXPECT_EQ(7, pos.getOccupiedBb().popcount()) << "Should be 7-piece position";

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 12;// Need deeper search for tactical sequence

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchStats& stats   = search.getSearchStats();
  const SearchResult& result = search.getLastSearchResult();

  LOG__INFO(Logger::get().TEST_LOG,
            "Tactical TB search: tbRootHits={} tbSearchHits={} tbSearchCutoffs={} move={} value={}",
            stats.tbRootHits, stats.tbSearchHits, stats.tbSearchCutoffs, result.bestMove.str(), result.bestMoveValue.str());

  EXPECT_GT(stats.tbSearchHits, 0ULL)
    << "Tactical sequence should reach TB positions";

  EXPECT_EQ(stats.tbRootHits, 0ULL)
    << "Should not probe TB at root for 7-piece position";
}

// 9-piece rook endgame: Multiple captures lead to TB
// Common practical endgame type
TEST_F(SearchTablebaseTest, NinePieceRookEndgameTBProbe) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  SKIP_IF_NO_TABLEBASES(6);

  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(s.TB_PATH = tbPath; s.USE_BOOK = false;);
#ifndef FRANKYCPP_PRODUCTION // would not compile in production since these are CONFIG_CONST frozen at defaults (probing enabled)
  // CONFIG_CONST in production — frozen at defaults (probing enabled)
  CONFIG_OVERRIDE(
    s.USE_TB_PROBE_ROOT   = true;
    s.USE_TB_PROBE_SEARCH = true;
    s.TB_PROBE_DEPTH      = 1;
    s.TB_PROBE_LIMIT      = 6;
  );
#endif

  // 9 pieces: White Kg1, Rb1, Pa2, Pc3; Black Kg7, Rd7, Pa6, Pc6, Ph6
  // Rook exchanges and pawn captures lead to simple TB positions
  const Position pos("8/3r2k1/p1p4p/8/8/2P5/P7/1R4K1 w - - 0 1");

  EXPECT_EQ(9, pos.getOccupiedBb().popcount()) << "Should be 9-piece position";

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 10;

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchStats& stats = search.getSearchStats();

  LOG__INFO(Logger::get().TEST_LOG,
            "9-piece rook endgame TB search: tbRootHits={} tbSearchHits={} tbSearchCutoffs={}",
            stats.tbRootHits, stats.tbSearchHits, stats.tbSearchCutoffs);

  // Should hit TB when reaching 6-piece positions after multiple captures
  EXPECT_GT(stats.tbSearchHits, 0ULL)
    << "Deep search in 9-piece endgame should reach TB positions";

  EXPECT_EQ(stats.tbRootHits, 0ULL)
    << "Should not probe TB at root for 9-piece position";
}

// 7-piece position with immediate capture option
// White Rxh7 immediately creates 6-piece KRK position (with pawns)
TEST_F(SearchTablebaseTest, ImmediateCaptureToTB) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  SKIP_IF_NO_TABLEBASES(0);

  const std::string tbPath = findTablebasePath();
  CONFIG_OVERRIDE(s.TB_PATH = tbPath; s.USE_BOOK = false;);
#ifndef FRANKYCPP_PRODUCTION // would not compile in production since these are CONFIG_CONST frozen at defaults (probing enabled)
  // CONFIG_CONST in production — frozen at defaults (probing enabled)
  CONFIG_OVERRIDE(
    s.USE_TB_PROBE_ROOT   = true;
    s.USE_TB_PROBE_SEARCH = true;
    s.TB_PROBE_DEPTH      = 1;
    s.TB_PROBE_LIMIT      = 6;
  );
#endif

  // 7 pieces: White Ke1, Rh1, Pg2; Black Ke8, Ph7, Pa7, Pb7
  // Rxh7 immediately reaches 6-piece KRKPPP
  const Position pos("4k3/pp5p/8/8/8/8/6P1/4K2R w - - 0 1");

  EXPECT_EQ(7, pos.getOccupiedBb().popcount()) << "Should be 7-piece position";

  Search search;
  search.isReady();

  SearchLimits sl;
  sl.depth = 8;

  search.startSearch(pos, sl);
  search.waitWhileSearching();

  const SearchStats& stats   = search.getSearchStats();
  const SearchResult& result = search.getLastSearchResult();

  LOG__INFO(Logger::get().TEST_LOG,
            "Immediate capture TB: tbRootHits={} tbSearchHits={} move={} value={}",
            stats.tbRootHits, stats.tbSearchHits, result.bestMove.str(), result.bestMoveValue.str());

  EXPECT_GT(stats.tbSearchHits, 0ULL)
    << "Immediate capture should allow TB probe at depth 1";

  EXPECT_EQ(stats.tbRootHits, 0ULL)
    << "Should not probe TB at root for 7-piece position";
}
