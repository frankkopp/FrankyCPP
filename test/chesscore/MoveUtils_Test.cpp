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
// MoveUtils_Test.cpp - Unit Tests for Move Utility Functions
//=============================================================================

#include "chesscore/MoveUtils.h"
#include "chesscore/Position.h"
#include "init.h"
#include "types/globals.h"

#include <gtest/gtest.h>

using namespace chess;
//=============================================================================
// Test Fixture
//=============================================================================

class MoveUtilsTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }

protected:
  void SetUp() override {
    // Standard starting position
    startPos = Position(START_POSITION_FEN);

    // Tactical position with multiple piece types
    // 1r1qk2r/2pn1ppp/p3p3/1pbpP3/1n1P4/1B3N2/PPP2PPP/RNBQR1K1 w k - 0 13
    tacticalPos = Position("1r1qk2r/2pn1ppp/p3p3/1pbpP3/1n1P4/1B3N2/PPP2PPP/RNBQR1K1 w k - 0 13");
  }

  Position startPos;    // NOLINT(*-non-private-member-variables-in-classes)
  Position tacticalPos; // NOLINT(*-non-private-member-variables-in-classes)
};

//=============================================================================
// normalizeMove() Tests
//=============================================================================

TEST_F(MoveUtilsTest, NormalizeMoveBasic) {
  EXPECT_EQ(normalizeMove("e2e4"), "e2e4");
  EXPECT_EQ(normalizeMove("E2E4"), "e2e4");
  EXPECT_EQ(normalizeMove("e7e8q"), "e7e8q");
  EXPECT_EQ(normalizeMove("e7e8Q"), "e7e8q");
}

TEST_F(MoveUtilsTest, NormalizeMoveRemovesDecoration) {
  EXPECT_EQ(normalizeMove("e2e4+"), "e2e4");
  EXPECT_EQ(normalizeMove("e2e4#"), "e2e4");
  EXPECT_EQ(normalizeMove("e2e4!"), "e2e4");
  EXPECT_EQ(normalizeMove("e2e4?"), "e2e4");
  EXPECT_EQ(normalizeMove("e2e4!!"), "e2e4");
}

TEST_F(MoveUtilsTest, NormalizeMoveHandlesPromotion) {
  EXPECT_EQ(normalizeMove("e7e8=Q"), "e7e8q");
  EXPECT_EQ(normalizeMove("e7e8=q"), "e7e8q");
  EXPECT_EQ(normalizeMove("a2a1=N"), "a2a1n");
}

//=============================================================================
// matchesExpectedMove() - Long Algebraic Tests
//=============================================================================

TEST_F(MoveUtilsTest, DirectMatchLongAlgebraic) {
  const std::string actualMove                 = "e2e4";
  const std::vector<std::string> expectedMoves = {"e2e4"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

TEST_F(MoveUtilsTest, DirectMatchCaseInsensitive) {
  const std::string actualMove                 = "e2e4";
  const std::vector<std::string> expectedMoves = {"E2E4"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

TEST_F(MoveUtilsTest, DirectMatchWithDecoration) {
  const std::string actualMove                 = "e2e4";
  const std::vector<std::string> expectedMoves = {"e2e4+"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

TEST_F(MoveUtilsTest, NoMatchLongAlgebraic) {
  const std::string actualMove                 = "e2e4";
  const std::vector<std::string> expectedMoves = {"d2d4"};

  EXPECT_FALSE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

TEST_F(MoveUtilsTest, MatchInMultipleExpected) {
  const std::string actualMove                 = "e2e4";
  const std::vector<std::string> expectedMoves = {"d2d4", "e2e4", "g1f3"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

//=============================================================================
// matchesExpectedMove() - SAN Conversion Tests
//=============================================================================

TEST_F(MoveUtilsTest, SANPawnMove) {
  const std::string actualMove                 = "e2e4";
  const std::vector<std::string> expectedMoves = {"e4"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

TEST_F(MoveUtilsTest, SANKnightMove) {
  const std::string actualMove                 = "g1f3";
  const std::vector<std::string> expectedMoves = {"Nf3"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

TEST_F(MoveUtilsTest, SANBishopMove) {
  const Position pos("rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");
  const std::string actualMove                 = "f8c5";
  const std::vector<std::string> expectedMoves = {"Bc5"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, pos));
}

TEST_F(MoveUtilsTest, SANCastlingKingside) {
  // Italian Game position - f1, g1 clearly empty for white
  const Position pos("r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");
  const std::string actualMove                 = "e1g1";
  const std::vector<std::string> expectedMoves = {"O-O"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, pos));
}

TEST_F(MoveUtilsTest, SANCastlingQueenside) {
  // Queen's Gambit Declined - b1, c1, d1 clear for white
  const Position pos("r1bq1rk1/ppp1bppp/2np1n2/4p3/2B1P3/2NPB3/PPPQ1PPP/R3K1NR w KQ - 5 4");
  const std::string actualMove                 = "e1c1";
  const std::vector<std::string> expectedMoves = {"O-O-O"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, pos));
}

TEST_F(MoveUtilsTest, SANPawnDoubleMove) {
  const Position pos("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 1");
  const std::string actualMove = "d2d4";

  // Both notations should work
  EXPECT_TRUE(matchesExpectedMove(actualMove, {"d4"}, pos));
}

TEST_F(MoveUtilsTest, SANPawnCapture) {
  const Position pos("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 1");
  const std::string actualMove                 = "e4d5";
  const std::vector<std::string> expectedMoves = {"exd5"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, pos));
}

TEST_F(MoveUtilsTest, SANPromotion) {
  const Position pos("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
  const std::string actualMove = "a7a8q";

  // Multiple promotion notations
  EXPECT_TRUE(matchesExpectedMove(actualMove, {"a8=Q"}, pos));
  EXPECT_TRUE(matchesExpectedMove(actualMove, {"a8Q"}, pos));
  EXPECT_TRUE(matchesExpectedMove(actualMove, {"a7a8q"}, pos));
  EXPECT_TRUE(matchesExpectedMove(actualMove, {"a7a8Q"}, pos));
}

TEST_F(MoveUtilsTest, SANAmbiguousKnight) {
  const Position pos("4k3/8/8/8/3N1N2/8/8/4K3 w - - 0 1");
  const std::string actualMove                 = "d4e6";
  const std::vector<std::string> expectedMoves = {"Nde6"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, pos));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(MoveUtilsTest, EmptyActualMove) {
  const std::string actualMove;
  const std::vector<std::string> expectedMoves = {"e2e4"};

  EXPECT_FALSE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

TEST_F(MoveUtilsTest, EmptyExpectedMoves) {
  const std::string actualMove = "e2e4";
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::vector<std::string> expectedMoves = {};

  EXPECT_FALSE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

TEST_F(MoveUtilsTest, InvalidSANNotation) {
  const std::string actualMove                 = "e2e4";
  const std::vector<std::string> expectedMoves = {"Zz9"}; // Invalid SAN

  EXPECT_FALSE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

TEST_F(MoveUtilsTest, MultipleValidFormats) {
  const std::string actualMove                 = "g1f3";
  const std::vector<std::string> expectedMoves = {"Nf3", "g1f3", "N1f3"};

  // Should match any valid format
  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

//=============================================================================
// Real-World Test Cases (from EPD files)
//=============================================================================

TEST_F(MoveUtilsTest, RealWorldWAC001) {
  // WAC.001: 2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - - bm Qg6
  const Position pos("2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - - 0 1");
  const std::string actualMove                 = "g3g6";
  const std::vector<std::string> expectedMoves = {"Qg6"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, pos));
}

TEST_F(MoveUtilsTest, RealWorldMultipleBestMoves) {
  // Starting position - multiple good first moves
  const std::string actualMove                 = "e2e4";
  const std::vector<std::string> expectedMoves = {"e4", "d4", "Nf3", "c4"};

  EXPECT_TRUE(matchesExpectedMove(actualMove, expectedMoves, startPos));
}

TEST_F(MoveUtilsTest, RealWorldAvoidMove) {
  // AM (avoid move) test
  const Position pos("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 1");
  const std::string actualMove              = "e4d5";
  const std::vector<std::string> avoidMoves = {"Nf3", "d4"}; // Avoid these

  // Should NOT match avoid moves
  EXPECT_FALSE(matchesExpectedMove(actualMove, avoidMoves, pos));
}
