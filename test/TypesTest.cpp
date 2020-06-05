/*
 * MIT License
 *
 * Copyright (c) 2018-2020 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "types/types.h"
#include "fmt/locale.h"
#include <gtest/gtest.h>
#include <random>

using testing::Eq;

TEST(TypesTest, colors) {
  ASSERT_EQ(WHITE, ~BLACK);
  ASSERT_EQ(BLACK, ~WHITE);
}

TEST(TypesTest, labels) {
  // all squares and label of squares
  std::string actual;
  for (int i = 0; i < SQ_NONE; ++i) {
    ASSERT_TRUE(validSquare(Square(i)));
    actual += str(Square(i));
  }
  std::string expected = "a1b1c1d1e1f1g1h1a2b2c2d2e2f2g2h2a3b3c3d3e3f3g3h3a4b4c4"
                         "d4e4f4g4h4a5b5c5d5e5f5g5h5a6b6c6d6e6f6g6h6a7b7c7d7e7f7"
                         "g7h7a8b8c8d8e8f8g8h8";
  ASSERT_EQ(expected, actual);
}

TEST(TypesTest, filesAndRanks) {
  // all squares and label of squares
  std::string actual;
  for (int i = 0; i < SQ_NONE; ++i) {
    ASSERT_EQ(Square(i), squareOf(File(fileOf(Square(i))), Rank(rankOf(Square(i)))));
  }
}

TEST(TypesTest, ColorLabel) {
  ASSERT_EQ('w', str(WHITE));
  ASSERT_EQ('b', str(BLACK));
}

TEST(TypesTest, MoveDirection) {
  ASSERT_EQ(1, moveDirection(WHITE));
  ASSERT_EQ(-1, moveDirection(BLACK));
}

TEST(TypesTest, castling) {

  CastlingRights cr = ANY_CASTLING;
  ASSERT_EQ(0b1110, cr - WHITE_OO);
  ASSERT_EQ(0b1101, cr - WHITE_OOO);
  ASSERT_EQ(0b1011, cr - BLACK_OO);
  ASSERT_EQ(0b0111, cr - BLACK_OOO);

  cr = NO_CASTLING;
  ASSERT_TRUE(cr == NO_CASTLING);

  cr += WHITE_OO;
  ASSERT_EQ(0b0001, cr);
  ASSERT_TRUE(cr == WHITE_OO);
  ASSERT_TRUE(cr != WHITE_OOO);
  ASSERT_TRUE(cr != NO_CASTLING);
  ASSERT_TRUE(cr != BLACK_OO);
  ASSERT_TRUE(cr != BLACK_OOO);
  ASSERT_TRUE(cr != BLACK_CASTLING);

  cr += WHITE_OOO;
  ASSERT_EQ(0b0011, cr);
  ASSERT_TRUE(cr == WHITE_OO);
  ASSERT_TRUE(cr == WHITE_OOO);
  ASSERT_TRUE(cr == WHITE_CASTLING);
  ASSERT_TRUE(cr != NO_CASTLING);
  ASSERT_TRUE(cr != BLACK_OO);
  ASSERT_TRUE(cr != BLACK_OOO);
  ASSERT_TRUE(cr != BLACK_CASTLING);

  cr += BLACK_OO;
  ASSERT_EQ(0b0111, cr);
  ASSERT_EQ(0b1111, cr + BLACK_OOO);
}

TEST(TypesTest, CastlingStr) {
  ASSERT_EQ("KQkq", str(ANY_CASTLING));
  ASSERT_EQ("KQ", str(WHITE_CASTLING));
  ASSERT_EQ("kq", str(BLACK_CASTLING));
  ASSERT_EQ("k", str(BLACK_OO));
  ASSERT_EQ("Q", str(WHITE_OOO));
}

TEST(TypesTest, pieceTypeLabels) {
  ASSERT_EQ('K', str(KING));
  ASSERT_EQ('Q', str(QUEEN));
  ASSERT_EQ('R', str(ROOK));
  ASSERT_EQ('B', str(BISHOP));
  ASSERT_EQ('N', str(KNIGHT));
  ASSERT_EQ('P', str(PAWN));
}


TEST(TypesTest, GamePhaseValue) {
  ASSERT_EQ(1, gamePhaseValue(KNIGHT));
  ASSERT_EQ(2, gamePhaseValue(ROOK));
  ASSERT_EQ(4, gamePhaseValue(QUEEN));
}

TEST(TypesTest, pieceLabels) {
  ASSERT_EQ('K', str(WHITE_KING));
  ASSERT_EQ('Q', str(WHITE_QUEEN));
  ASSERT_EQ('R', str(WHITE_ROOK));
  ASSERT_EQ('B', str(WHITE_BISHOP));
  ASSERT_EQ('N', str(WHITE_KNIGHT));
  ASSERT_EQ('P', str(WHITE_PAWN));
  ASSERT_EQ('k', str(BLACK_KING));
  ASSERT_EQ('q', str(BLACK_QUEEN));
  ASSERT_EQ('r', str(BLACK_ROOK));
  ASSERT_EQ('b', str(BLACK_BISHOP));
  ASSERT_EQ('n', str(BLACK_KNIGHT));
  ASSERT_EQ('p', str(BLACK_PAWN));
}

TEST(TypesTest, pieces) {

  // make piece
  ASSERT_EQ(WHITE_KING, makePiece(WHITE, KING));
  ASSERT_EQ(BLACK_KING, makePiece(BLACK, KING));
  ASSERT_EQ(WHITE_QUEEN, makePiece(WHITE, QUEEN));
  ASSERT_EQ(BLACK_QUEEN, makePiece(BLACK, QUEEN));

  // colorOf
  ASSERT_EQ(WHITE, colorOf(WHITE_KING));
  ASSERT_EQ(WHITE, colorOf(WHITE_QUEEN));
  ASSERT_EQ(WHITE, colorOf(WHITE_PAWN));
  ASSERT_EQ(WHITE, colorOf(WHITE_ROOK));
  ASSERT_EQ(BLACK, colorOf(BLACK_KING));
  ASSERT_EQ(BLACK, colorOf(BLACK_QUEEN));
  ASSERT_EQ(BLACK, colorOf(BLACK_PAWN));
  ASSERT_EQ(BLACK, colorOf(BLACK_ROOK));

  // typeOf
  ASSERT_EQ(KING, typeOf(WHITE_KING));
  ASSERT_EQ(QUEEN, typeOf(WHITE_QUEEN));
  ASSERT_EQ(PAWN, typeOf(WHITE_PAWN));
  ASSERT_EQ(ROOK, typeOf(WHITE_ROOK));
  ASSERT_EQ(KING, typeOf(BLACK_KING));
  ASSERT_EQ(QUEEN, typeOf(BLACK_QUEEN));
  ASSERT_EQ(PAWN, typeOf(BLACK_PAWN));
  ASSERT_EQ(ROOK, typeOf(BLACK_ROOK));
  ASSERT_EQ(PT_NONE, typeOf(PIECE_NONE));
}

TEST(TypesTest, directionOperators) {
  ASSERT_EQ(SQ_A2, SQ_A1 + NORTH);
  ASSERT_TRUE(SQ_H8 + NORTH > 63);
  ASSERT_TRUE(SQ_H1 + SOUTH < 0);
  ASSERT_EQ(SQ_H8, SQ_A1 + (7 * NORTH_EAST));
  ASSERT_EQ(SQ_A8, SQ_H1 + (7 * NORTH_WEST));
}

TEST(TypesTest, moves) {
  Move move = createMove(SQ_A1, SQ_H1, NORMAL);
  ASSERT_TRUE(validMove(move));
  ASSERT_EQ(SQ_A1, fromSquare(move));
  ASSERT_EQ(SQ_H1, toSquare(move));
  ASSERT_EQ(NORMAL, moveTypeOf(move));
  ASSERT_EQ(KNIGHT, promotionTypeOf(move));// not useful is not type PROMOTION

  move = createMove(SQ_A7, SQ_A8, PROMOTION, QUEEN);
  ASSERT_TRUE(validMove(move));
  ASSERT_EQ(SQ_A7, fromSquare(move));
  ASSERT_EQ(SQ_A8, toSquare(move));
  ASSERT_EQ(PROMOTION, moveTypeOf(move));
  ASSERT_EQ(QUEEN, promotionTypeOf(move));// not useful is not type PROMOTION

  std::stringstream buffer1, buffer2;
  buffer1 << "a7a8Q";
  buffer2 << move;
  ASSERT_EQ(buffer1.str(), buffer2.str());
  ASSERT_EQ("a7a8Q (PROMOTION -15001 31800)", strVerbose(move));
}

TEST(TypesTest, movesValue) {
  NEWLINE;
  Move move = createMove(SQ_A1, SQ_H1, NORMAL);

  ASSERT_EQ(VALUE_NONE, valueOf(move));

  Value v = VALUE_MAX;
  setValueOf(move, v);
  ASSERT_EQ(v, valueOf(move));

  v = VALUE_MIN;
  setValueOf(move, v);
  ASSERT_EQ(v, valueOf(move));

  v = Value(100);
  setValueOf(move, v);
  ASSERT_EQ(v, valueOf(move));

  v = VALUE_CHECKMATE_THRESHOLD;
  setValueOf(move, v);
  ASSERT_EQ(v, valueOf(move));

  move = createMove(SQ_A1, SQ_H1, NORMAL, VALUE_DRAW);
  ASSERT_EQ(VALUE_DRAW, valueOf(move));
  move = createMove(SQ_A1, SQ_H1, NORMAL, Value(-100));
  ASSERT_EQ(Value(-100), valueOf(move));
  move = createMove(SQ_A1, SQ_H1, NORMAL, Value(100));
  ASSERT_EQ(Value(100), valueOf(move));

  move = createMove(SQ_A1, SQ_H1, PROMOTION, QUEEN, Value(-pieceTypeValue[QUEEN]));
  ASSERT_EQ(-pieceTypeValue[QUEEN], valueOf(move));

  // test equality without value / pure move
  move       = createMove(SQ_A1, SQ_H1, NORMAL, Value(100));
  Move move2 = createMove(SQ_A1, SQ_H1, NORMAL, Value(-100));
  ASSERT_NE(move, move2);
  ASSERT_EQ(moveOf(move), moveOf(move2));
}

TEST(TypesTest, moveListPrint) {

  Move move1 = createMove(SQ_A1, SQ_H1, NORMAL);
  Move move2 = createMove(SQ_A7, SQ_A8, PROMOTION, QUEEN);
  Move move3 = createMove(SQ_E1, SQ_G1, CASTLING);
  MoveList moveList;
  moveList.push_back(move1);
  moveList.push_back(move2);
  moveList.push_back(move3);

  std::ostringstream ml;
  ml << moveList;
  std::string expected = "a1h1 a7a8Q e1g1";
  ASSERT_EQ(expected, ml.str());
}

