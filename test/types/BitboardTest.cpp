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

#include "types/types.h"
#include <gtest/gtest.h>

using testing::Eq;

class BitboardsTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    Types::init();
    NEWLINE;
  }
  static void TearDownTestSuite() {}

protected:
  void SetUp() override {}
  void TearDown() override {}
};

// test if everything is initialized and precomputed
TEST_F(BitboardsTest, init) {
  EXPECT_EQ(7, Squares::squareDistance[SQ_A1][SQ_H8]);
  EXPECT_EQ("1111111100000000000000000000000000000000000000000000000000000000", str(Bitboards::rankBb[RANK_8]));
  EXPECT_EQ("0000000011111111000000000000000000000000000000000000000000000000", str(Bitboards::rankBb[RANK_7]));
  EXPECT_EQ("1000000010000000100000001000000010000000100000001000000010000000", str(Bitboards::fileBb[FILE_H]));
  EXPECT_EQ("0100000001000000010000000100000001000000010000000100000001000000", str(Bitboards::fileBb[FILE_G]));
  EXPECT_EQ("0000000000000000000000000000000000000000000000000000000000000001", str(Bitboards::sqBb[SQ_A1]));
  EXPECT_EQ("1000000000000000000000000000000000000000000000000000000000000000", str(Bitboards::sqBb[SQ_H8]));
  EXPECT_EQ("0000000000000000000000000000000000010000000000000000000000000000", str(Bitboards::sqBb[SQ_E4]));
  EXPECT_EQ(Bitboards::fileBb[FILE_H], Bitboards::sqToFileBb[SQ_H8]);
  EXPECT_EQ(Bitboards::rankBb[RANK_8], Bitboards::sqToRankBb[SQ_H8]);
  EXPECT_EQ("0000000000000000000000000000000000000000010000000010000000000000", str(Bitboards::nonSliderAttacks[KNIGHT][SQ_H1]));
  EXPECT_EQ("0111111101111111011111110111111101111111011111110111111101111111", str(Bitboards::filesWestMask[SQ_H1]));
  EXPECT_EQ("1000000010000000100000001000000010000000100000001000000001111111", str(Bitboards::rookMagics[SQ_H1].attacks[Bitboards::rookMagics[SQ_H1].index(BbZero)]));
  EXPECT_EQ("0000000000000000000000000000000011111110000000000000000000000000", str(Bitboards::rays[E][SQ_A4]));
  EXPECT_EQ("0000000000000000000000000000000000000000000000000000000000111110", str(Bitboards::intermediateBb[SQ_A1][SQ_G1]));
  EXPECT_EQ("0011100000111000001110000011100000000000000000000000000000000000", str(Bitboards::passedPawnMask[WHITE][SQ_E4]));
  EXPECT_EQ("0000000000000000000000000000000000000000000000000000000011100000", str(Bitboards::kingSideCastleMask[WHITE]));
  EXPECT_EQ("0101010110101010010101011010101001010101101010100101010110101010", str(Bitboards::colorBb[WHITE]));
}

TEST_F(BitboardsTest, str) {
  EXPECT_EQ("1111111100000000000000000000000000000000000000000000000000000000", str(Bitboards::rankBb[RANK_8]));
  EXPECT_EQ("00000000.00000000.00000000.00000000.00000000.00000000.00000000.11111111 (18374686479671623680)", strGrouped(Bitboards::rankBb[RANK_8]));
  EXPECT_EQ("+---+---+---+---+---+---+---+---+\n| X | X | X | X | X | X | X | X |\n+---+---+---+---+---+---+---+---+\n|   |   |   |   |   |   |   |   |\n+---+---+---+---+---+---+---+---+\n|   |   |   |   |   |   |   |   |\n+---+---+---+---+---+---+---+---+\n|   |   |   |   |   |   |   |   |\n+---+---+---+---+---+---+---+---+\n|   |   |   |   |   |   |   |   |\n+---+---+---+---+---+---+---+---+\n|   |   |   |   |   |   |   |   |\n+---+---+---+---+---+---+---+---+\n|   |   |   |   |   |   |   |   |\n+---+---+---+---+---+---+---+---+\n|   |   |   |   |   |   |   |   |\n+---+---+---+---+---+---+---+---+\n", strBoard(Bitboards::rankBb[RANK_8]));
}

TEST_F(BitboardsTest, popcount) {
  uint64_t b = 0b0010000000010000000000000010000000000000000000000000000000000000ULL;
  ASSERT_EQ(3, popcount(b));
  b = BbZero;
  ASSERT_EQ(0, popcount(b));
}

TEST_F(BitboardsTest, BitboardSquareTest) {
  // Tests if the & operator is overloaded for Bitboards & Square
  EXPECT_EQ(Bitboards::sqBb[SQ_E4], BbFull & SQ_E4);
  EXPECT_EQ(Bitboards::sqBb[SQ_A1], BbFull & SQ_A1);
  EXPECT_EQ(Bitboards::sqBb[SQ_H8], BbFull & SQ_H8);
  EXPECT_EQ(Bitboards::sqBb[SQ_A8], BbFull & SQ_A8);
  EXPECT_NE(Bitboards::sqBb[SQ_A8], BbFull & SQ_A1);
}

TEST_F(BitboardsTest, SquareDistanceTest) {
  ASSERT_EQ(6, distance(FILE_A, FILE_G));
  ASSERT_EQ(7, distance(RANK_1, RANK_8));
  ASSERT_EQ(7, distance(SQ_A1, SQ_H1));
  ASSERT_EQ(7, distance(SQ_A1, SQ_H8));
  ASSERT_EQ(2, distance(SQ_A1, SQ_A3));
  ASSERT_EQ(4, distance(SQ_A1, SQ_E1));
  ASSERT_EQ(7, distance(SQ_A1, SQ_G8));
}

TEST_F(BitboardsTest, shiftTest) {
  Bitboard shifted = shiftBb(EAST, FileABB);
  ASSERT_EQ(FileBBB, shifted);

  shifted = shiftBb(WEST, FileABB);
  ASSERT_EQ(BbZero, shifted);

  shifted = shiftBb(NORTH, Rank1BB);
  ASSERT_EQ(Rank2BB, shifted);

  shifted = shiftBb(SOUTH, Rank8BB);
  ASSERT_EQ(Rank7BB, shifted);

  shifted = shiftBb(NORTH, Rank8BB);
  ASSERT_EQ(BbZero, shifted);

  shifted = shiftBb(NORTH_EAST, Bitboards::sqBb[SQ_E4]);
  ASSERT_EQ(Bitboards::sqBb[SQ_F5], shifted);

  shifted = shiftBb(SOUTH_EAST, Bitboards::sqBb[SQ_E4]);
  ASSERT_EQ(Bitboards::sqBb[SQ_F3], shifted);

  shifted = shiftBb(SOUTH_WEST, Bitboards::sqBb[SQ_E4]);
  ASSERT_EQ(Bitboards::sqBb[SQ_D3], shifted);

  shifted = shiftBb(NORTH_WEST, Bitboards::sqBb[SQ_E4]);
  ASSERT_EQ(Bitboards::sqBb[SQ_D5], shifted);
}

TEST_F(BitboardsTest, Diagonals) {
  ASSERT_EQ(DiagUpA1, Bitboards::squareDiagUpBb[SQ_A1]);
  ASSERT_EQ(DiagUpA1, Bitboards::squareDiagUpBb[SQ_C3]);
  ASSERT_EQ(DiagUpA1, Bitboards::squareDiagUpBb[SQ_G7]);
  ASSERT_EQ(DiagUpA1, Bitboards::squareDiagUpBb[SQ_H8]);
  ASSERT_EQ(DiagDownH1, Bitboards::squareDiagDownBb[SQ_A8]);
  ASSERT_EQ(DiagDownH1, Bitboards::squareDiagDownBb[SQ_C6]);
  ASSERT_EQ(DiagDownH1, Bitboards::squareDiagDownBb[SQ_G2]);
  ASSERT_EQ(DiagDownH1, Bitboards::squareDiagDownBb[SQ_H1]);
}

TEST_F(BitboardsTest, lsb_msb) {
  // set least significant bit
  Bitboard b = BbOne;
  fprintln("{}", str(b));
  Square sql = lsb(b);
  Square sqm = msb(b);
  ASSERT_EQ(SQ_A1, sql);
  ASSERT_EQ(SQ_A1, sqm);

  b   = (BbOne << 63);
  fprintln("{}", str(b));
  sql = lsb(b);
  sqm = msb(b);
  ASSERT_EQ(SQ_H8, sql);
  ASSERT_EQ(SQ_H8, sqm);

  b   = BbZero | SQ_H1 | SQ_G8;
  fprintln("{}", str(b));
  sql = lsb(b);
  sqm = msb(b);
  ASSERT_EQ(SQ_H1, sql);
  ASSERT_EQ(SQ_G8, sqm);

  NEWLINE;

  b         =  BbZero | SQ_A1 | SQ_H8;
  fprintln("{}", str(b));
  Square sq = popLSB(b);
  fprintln("{}", str(b));
  ASSERT_EQ(SQ_A1, sq);
  sq = popLSB(b);
  fprintln("{}", str(b));
  ASSERT_EQ(SQ_H8, sq);

}

TEST_F(BitboardsTest, bitScans) {
  ASSERT_EQ(1, popcount(Bitboards::sqBb[SQ_D3]));
  ASSERT_EQ(2, popcount(Bitboards::sqBb[SQ_D3] | Bitboards::sqBb[SQ_H2]));
  ASSERT_EQ(8, popcount(DiagUpA1));

  ASSERT_EQ(19, lsb(Bitboards::sqBb[SQ_D3]));
  ASSERT_EQ(19, msb(Bitboards::sqBb[SQ_D3]));

  Bitboard tmp = DiagUpA1;
  int i        = 0;
  while (tmp) {
    i++;
    popLSB(tmp);
  }
  ASSERT_EQ(8, i);
}

TEST_F(BitboardsTest, pawnAttacksMoves) {
  std::string expected, actual;

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   | X |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::pawnAttacks[WHITE][SQ_A2]);
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   | X |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::pawnAttacks[BLACK][SQ_H7]);
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   | X |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::pawnAttacks[BLACK][SQ_D5]);
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);
}


TEST_F(BitboardsTest, knightAttacks) {
  std::string expected, actual;

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   | X |   | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   | X |   |   |   | X |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   | X |   |   |   | X |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   | X |   | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(getAttacksBb(KNIGHT, SQ_E4, BbZero));
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   | X |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(getAttacksBb(KNIGHT, SQ_H2, BbZero));
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);
}

TEST_F(BitboardsTest, kingAttacks) {
  std::string expected, actual;

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   | X | X | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   | X |   | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   | X | X | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(getAttacksBb(KING, SQ_E4, BbZero));
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   | X |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(getAttacksBb(KING, SQ_H2, BbZero));
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);
}

TEST_F(BitboardsTest, slidingAttacks) {
  std::string expected, actual;

  expected = "+---+---+---+---+---+---+---+---+\n"
             "| X |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   | X |   |   |   |   |   | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   | X |   |   |   | X |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   | X |   | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   | X |   | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   | X |   |   |   | X |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   | X |   |   |   |   |   | X |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(getAttacksBb(BISHOP, SQ_E4, BbZero));
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X |   | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(getAttacksBb(ROOK, SQ_E4, BbZero));
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "| X |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   | X |   |   | X |   |   | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   | X |   | X |   | X |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   | X | X | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X |   | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   | X | X | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   | X |   | X |   | X |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   | X |   |   | X |   |   | X |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(getAttacksBb(QUEEN, SQ_E4, BbZero));
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);
  ASSERT_EQ(getAttacksBb(QUEEN, SQ_E4, BbZero), (getAttacksBb(BISHOP, SQ_E4, BbZero) | getAttacksBb(ROOK, SQ_E4, BbZero)));
}

TEST_F(BitboardsTest, masks) {
  std::string expected, actual;

  expected = "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::filesWestMask[SQ_E4]);
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::filesEastMask[SQ_E4]);
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X | X | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X | X | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X | X | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X | X | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::ranksNorthMask[SQ_E4]);
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X | X | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X | X | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X | X | X | X | X | X | X | X |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::ranksSouthMask[SQ_E4]);
  //  std::cout << actual;
  ASSERT_EQ(expected, actual);
}

TEST_F(BitboardsTest, rays) {
  std::string expected, actual;

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::rays[N][SQ_E4]);
//  std::cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   | X |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   | X |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::rays[SE][SQ_E4]);
//  std::cout << actual;
  ASSERT_EQ(expected, actual);
}

TEST_F(BitboardsTest, intermediates) {
  std::string expected, actual;

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   | X |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   | X |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::intermediateBb[SQ_C3][SQ_G7]);
//  cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   | X |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   | X |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   | X |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::intermediateBb[SQ_A7][SQ_F2]);
//  cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "| X |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::intermediateBb[SQ_A7][SQ_A2]);
//  cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::intermediateBb[SQ_A7][SQ_H1]);
//  cout << actual;
  ASSERT_EQ(expected, actual);

  expected = "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   | X | X | X |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n"
             "|   |   |   |   |   |   |   |   |\n"
             "+---+---+---+---+---+---+---+---+\n";
  actual = strBoard(Bitboards::intermediateBb[SQ_H7][SQ_D7]);
//  cout << actual;
  ASSERT_EQ(expected, actual);
}

TEST_F(BitboardsTest, centerDistance) {
  ASSERT_EQ(2, Squares::centerDistance[SQ_C2]);
  ASSERT_EQ(3, Squares::centerDistance[SQ_B8]);
  ASSERT_EQ(3, Squares::centerDistance[SQ_H1]);
  ASSERT_EQ(3, Squares::centerDistance[SQ_H7]);
}
