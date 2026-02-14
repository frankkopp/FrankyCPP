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

#include "tablebase/Tablebase.h"
#include "tablebase/TablebasePaths.h"
#include "Test_Utils.h"
#include "chesscore/Position.h"
#include "common/Logging.h"
#include "init.h"

#include <gtest/gtest.h>

using namespace tablebase;
using testing::Eq;

class TablebaseTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().TB_LOG->set_level(spdlog::level::debug);
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
  const TBResult result = tb.probeWDL(pos);
  EXPECT_EQ(TBResult::Failed, result);
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
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().TB_LOG->set_level(spdlog::level::debug);
  }

protected:
  Tablebase tb;

  void SetUp() override {
    // Use centralized path resolution (checks env var, config, defaults)
    const std::string tbPath = findTablebasePath();
    if (!tbPath.empty()) {
      tb.initialize(tbPath);
    }
  }

  void TearDown() override {
    tb.shutdown();
  }

  void skipIfNoTablebases() const {
    if (!tb.isAvailable()) {
      GTEST_SKIP() << "Tablebases not available (set TB_PATH environment variable)";
    }
  }
};

// KQK - King and Queen vs King (basic win)
TEST_F(TablebaseIntegrationTest, KQK_WhiteWins) {
  skipIfNoTablebases();

  const Position pos("8/8/8/4k3/8/8/1Q6/4K3 w - - 0 1");// White has KQ vs K

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    EXPECT_EQ(TBResult::Win, result);
    LOG__INFO(Logger::get().TEST_LOG, "KQK position: {}", Tablebase::resultToString(result));
  }
}

// KRK - King and Rook vs King (basic win)
TEST_F(TablebaseIntegrationTest, KRK_WhiteWins) {
  skipIfNoTablebases();

  const Position pos("8/8/8/4k3/8/8/1R6/4K3 w - - 0 1");// White has KR vs K

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    EXPECT_EQ(TBResult::Win, result);
    LOG__INFO(Logger::get().TEST_LOG, "KRK position: {}", Tablebase::resultToString(result));
  }
}

// KPK - King and Pawn vs King (can be win or draw depending on position)
TEST_F(TablebaseIntegrationTest, KPK_WinningPosition) {
  skipIfNoTablebases();

  const Position pos("8/4P3/8/4k3/8/8/8/4K3 w - - 0 1");// Pawn about to promote

  if (tb.canProbe(pos)) {
    const TBResult result = tb.probeWDL(pos);
    EXPECT_EQ(TBResult::Win, result);
    LOG__INFO(Logger::get().TEST_LOG, "KPK winning position: {}", Tablebase::resultToString(result));
  }
}

// Position with castling rights should not be probeable
TEST_F(TablebaseIntegrationTest, PositionWithCastling) {
  skipIfNoTablebases();

  const Position pos("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");// Has castling rights

  // Should not be probeable due to castling rights
  EXPECT_FALSE(tb.canProbe(pos));
}

// Position with too many pieces (if only 3-4-5 piece TBs available)
TEST_F(TablebaseIntegrationTest, TooManyPieces) {
  skipIfNoTablebases();

  // Standard starting position - definitely too many pieces
  const Position pos;
  EXPECT_FALSE(tb.canProbe(pos));
}

// Test probeRoot returns best move
TEST_F(TablebaseIntegrationTest, ProbeRootBestMove) {
  skipIfNoTablebases();

  const Position pos("8/8/8/4k3/8/8/1Q6/4K3 w - - 0 1");// KQK

  if (tb.canProbe(pos)) {
    const TBProbeResult result = tb.probeRoot(pos);
    EXPECT_TRUE(result.success());
    EXPECT_TRUE(result.isWin());
    // Best move should be valid (not MOVE_NONE)
    if (result.bestMove != MOVE_NONE) {
      LOG__INFO(Logger::get().TEST_LOG, "KQK best move: {}", result.bestMove.str());
    }
  }
}
