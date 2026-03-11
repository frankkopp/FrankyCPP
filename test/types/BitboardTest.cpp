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

#include <gtest/gtest.h>

using testing::Eq;
using namespace chess;
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
  EXPECT_EQ("1111111100000000000000000000000000000000000000000000000000000000", Bitboards::rankBb[RANK_8].str());
  EXPECT_EQ("0000000011111111000000000000000000000000000000000000000000000000", Bitboards::rankBb[RANK_7].str());
  EXPECT_EQ("1000000010000000100000001000000010000000100000001000000010000000", Bitboards::fileBb[FILE_H].str());
  EXPECT_EQ("0100000001000000010000000100000001000000010000000100000001000000", Bitboards::fileBb[FILE_G].str());
  EXPECT_EQ("0000000000000000000000000000000000000000000000000000000000000001", Bitboards::sqBb[SQ_A1].str());
  EXPECT_EQ("1000000000000000000000000000000000000000000000000000000000000000", Bitboards::sqBb[SQ_H8].str());
  EXPECT_EQ("0000000000000000000000000000000000010000000000000000000000000000", Bitboards::sqBb[SQ_E4].str());
  EXPECT_EQ(Bitboards::fileBb[FILE_H], Bitboards::sqToFileBb[SQ_H8]);
  EXPECT_EQ(Bitboards::rankBb[RANK_8], Bitboards::sqToRankBb[SQ_H8]);
  EXPECT_EQ("0000000000000000000000000000000000000000010000000010000000000000", Bitboards::nonSliderAttacks[KNIGHT][SQ_H1].str());
  EXPECT_EQ("0111111101111111011111110111111101111111011111110111111101111111", Bitboards::filesWestMask[SQ_H1].str());
  // EXPECT_EQ("1000000010000000100000001000000010000000100000001000000001111111", Bitboards::rookMagics[SQ_H1].attacks[Bitboards::rookMagics[SQ_H1].index(BbZero)].str());
  EXPECT_EQ("0000000000000000000000000000000011111110000000000000000000000000", Bitboards::rays[E][SQ_A4].str());
  EXPECT_EQ("0000000000000000000000000000000000000000000000000000000000111110", Bitboards::intermediateBb[SQ_A1][SQ_G1].str());
  EXPECT_EQ("0011100000111000001110000011100000000000000000000000000000000000", Bitboards::passedPawnMask[WHITE][SQ_E4].str());
  EXPECT_EQ("0000000000000000000000000000000000000000000000000000000011100000", Bitboards::kingSideCastleMask[WHITE].str());
  EXPECT_EQ("0101010110101010010101011010101001010101101010100101010110101010", Bitboards::colorBb[WHITE].str());
}

TEST_F(BitboardsTest, str) {
  EXPECT_EQ("1111111100000000000000000000000000000000000000000000000000000000", Bitboards::rankBb[RANK_8].str());
  EXPECT_EQ("00000000.00000000.00000000.00000000.00000000.00000000.00000000.11111111 (18374686479671623680)", Bitboards::rankBb[RANK_8].strGrouped());
  EXPECT_EQ(
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
    "+---+---+---+---+---+---+---+---+\n"
    "|   |   |   |   |   |   |   |   |\n"
    "+---+---+---+---+---+---+---+---+\n"
    "|   |   |   |   |   |   |   |   |\n"
    "+---+---+---+---+---+---+---+---+\n"
    "|   |   |   |   |   |   |   |   |\n"
    "+---+---+---+---+---+---+---+---+\n",
    Bitboards::rankBb[RANK_8].strBoard());
}

TEST_F(BitboardsTest, popcount) {
  Bitboard b = 0b0010000000010000000000000010000000000000000000000000000000000000ULL;
  ASSERT_EQ(3, b.popcount());
  b = BbZero;
  ASSERT_EQ(0, b.popcount());
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
  ASSERT_EQ(6, FILE_A.distance(FILE_G));
  ASSERT_EQ(7, RANK_1.distance(RANK_8));
  ASSERT_EQ(7, SQ_A1.distanceTo(SQ_H1));
  ASSERT_EQ(7, SQ_A1.distanceTo(SQ_H8));
  ASSERT_EQ(2, SQ_A1.distanceTo(SQ_A3));
  ASSERT_EQ(4, SQ_A1.distanceTo(SQ_E1));
  ASSERT_EQ(7, SQ_A1.distanceTo(SQ_G8));
}

TEST_F(BitboardsTest, shiftTest) {
  Bitboard shifted = FileABB.shifted(EAST);
  ASSERT_EQ(FileBBB, shifted);

  shifted = FileABB.shifted(WEST);
  ASSERT_EQ(BbZero, shifted);

  shifted = Rank1BB.shifted(NORTH);
  ASSERT_EQ(Rank2BB, shifted);

  shifted = Rank8BB.shifted(SOUTH);
  ASSERT_EQ(Rank7BB, shifted);

  shifted = Rank8BB.shifted(NORTH);
  ASSERT_EQ(BbZero, shifted);

  shifted = Bitboards::sqBb[SQ_E4].shifted(NORTH_EAST);
  ASSERT_EQ(Bitboards::sqBb[SQ_F5], shifted);

  shifted = Bitboards::sqBb[SQ_E4].shifted(SOUTH_EAST);
  ASSERT_EQ(Bitboards::sqBb[SQ_F3], shifted);

  shifted = Bitboards::sqBb[SQ_E4].shifted(SOUTH_WEST);
  ASSERT_EQ(Bitboards::sqBb[SQ_D3], shifted);

  shifted = Bitboards::sqBb[SQ_E4].shifted(NORTH_WEST);
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
  fprintln("{}", b.str());
  Square sql = b.lsb();
  Square sqm = b.msb();
  ASSERT_EQ(SQ_A1, sql);
  ASSERT_EQ(SQ_A1, sqm);

  b = (BbOne << 63);
  fprintln("{}", b.str());
  sql = b.lsb();
  sqm = b.msb();
  ASSERT_EQ(SQ_H8, sql);
  ASSERT_EQ(SQ_H8, sqm);

  b = BbZero | SQ_H1 | SQ_G8;
  fprintln("{}", b.str());
  sql = b.lsb();
  sqm = b.msb();
  ASSERT_EQ(SQ_H1, sql);
  ASSERT_EQ(SQ_G8, sqm);

  NEWLINE;

  b = BbZero | SQ_A1 | SQ_H8;
  fprintln("{}", b.str());
  Square sq = b.popLSB();
  fprintln("{}", b.str());
  ASSERT_EQ(SQ_A1, sq);
  sq = b.popLSB();
  fprintln("{}", b.str());
  ASSERT_EQ(SQ_H8, sq);
}

TEST_F(BitboardsTest, bitScans) {
  ASSERT_EQ(1, Bitboards::sqBb[SQ_D3].popcount());
  ASSERT_EQ(2, (Bitboards::sqBb[SQ_D3] | Bitboards::sqBb[SQ_H2]).popcount());
  ASSERT_EQ(8, DiagUpA1.popcount());

  ASSERT_EQ(19, Bitboards::sqBb[SQ_D3].lsb());
  ASSERT_EQ(19, Bitboards::sqBb[SQ_D3].msb());

  Bitboard tmp = DiagUpA1;
  int i        = 0;
  while (tmp) {
    i++;
    tmp.popLSB();
  }
  ASSERT_EQ(8, i);
}

TEST_F(BitboardsTest, pawnAttacksMoves) {
  std::string expected = "+---+---+---+---+---+---+---+---+\n"
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
  std::string actual   = Bitboards::pawnAttacks[WHITE][SQ_A2].strBoard();
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
  actual   = Bitboards::pawnAttacks[BLACK][SQ_H7].strBoard();
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
  actual   = Bitboards::pawnAttacks[BLACK][SQ_D5].strBoard();
  ASSERT_EQ(expected, actual);
}

TEST_F(BitboardsTest, knightAttacks) {
  std::string expected = "+---+---+---+---+---+---+---+---+\n"
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
  std::string actual   = Attacks::attacks(KNIGHT, SQ_E4, BbZero).strBoard();
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
  actual   = Attacks::attacks(KNIGHT, SQ_H2, BbZero).strBoard();
  ASSERT_EQ(expected, actual);
}

TEST_F(BitboardsTest, kingAttacks) {
  std::string expected = "+---+---+---+---+---+---+---+---+\n"
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
  std::string actual   = Attacks::attacks(KING, SQ_E4, BbZero).strBoard();
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
  actual   = Attacks::attacks(KING, SQ_H2, BbZero).strBoard();
  ASSERT_EQ(expected, actual);
}

TEST_F(BitboardsTest, slidingAttacks) {
  std::string expected = "+---+---+---+---+---+---+---+---+\n"
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
  std::string actual   = Attacks::attacks(BISHOP, SQ_E4, BbZero).strBoard();
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
  actual   = Attacks::attacks(ROOK, SQ_E4, BbZero).strBoard();
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
  actual   = Attacks::attacks(QUEEN, SQ_E4, BbZero).strBoard();
  ASSERT_EQ(expected, actual);
  ASSERT_EQ(Attacks::attacks(QUEEN, SQ_E4, BbZero), (Attacks::attacks(BISHOP, SQ_E4, BbZero) | Attacks::attacks(ROOK, SQ_E4, BbZero)));
}

TEST_F(BitboardsTest, masks) {
  std::string expected = "+---+---+---+---+---+---+---+---+\n"
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
  std::string actual   = Bitboards::filesWestMask[SQ_E4].strBoard();
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
  actual   = Bitboards::filesEastMask[SQ_E4].strBoard();
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
  actual   = Bitboards::ranksNorthMask[SQ_E4].strBoard();
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
  actual   = Bitboards::ranksSouthMask[SQ_E4].strBoard();
  ASSERT_EQ(expected, actual);
}

TEST_F(BitboardsTest, rays) {
  std::string expected = "+---+---+---+---+---+---+---+---+\n"
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
  std::string actual   = Bitboards::rays[N][SQ_E4].strBoard();
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
  actual   = Bitboards::rays[SE][SQ_E4].strBoard();
  ASSERT_EQ(expected, actual);
}

TEST_F(BitboardsTest, intermediates) {

  std::string expected = "+---+---+---+---+---+---+---+---+\n"
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
  std::string actual   = Bitboards::intermediateBb[SQ_C3][SQ_G7].strBoard();
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
  actual   = Bitboards::intermediateBb[SQ_A7][SQ_F2].strBoard();
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
  actual   = Bitboards::intermediateBb[SQ_A7][SQ_A2].strBoard();
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
  actual   = Bitboards::intermediateBb[SQ_H7][SQ_D7].strBoard();
  ASSERT_EQ(expected, actual);
}

TEST_F(BitboardsTest, centerDistance) {
  ASSERT_EQ(2, Squares::centerDistance[SQ_C2]);
  ASSERT_EQ(3, Squares::centerDistance[SQ_B8]);
  ASSERT_EQ(3, Squares::centerDistance[SQ_H1]);
  ASSERT_EQ(3, Squares::centerDistance[SQ_H7]);
}
