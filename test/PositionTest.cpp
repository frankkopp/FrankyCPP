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
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
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

#include <boost/timer/timer.hpp>
#include <gtest/gtest.h>
#include <ostream>
#include <string>

#include "Position.h"
#include "init.h"

using namespace std;
using testing::Eq;

class PositionTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};


TEST_F(PositionTest, initialization) {
  Position position;
  EXPECT_TRUE(Position::initialized);
}

TEST_F(PositionTest, HistoryStruct) {
  Position position;
  EXPECT_EQ(48, sizeof(HistoryState));
  EXPECT_EQ(MAX_MOVES, position.historyState.size());
}

TEST_F(PositionTest, ZobristTest) {
  Position position;

  Key z = 0ULL;

  z ^= Zobrist::pieces[WHITE_KING][SQ_E1];
  z ^= Zobrist::pieces[BLACK_KING][SQ_E8];
  z ^= Zobrist::castlingRights[ANY_CASTLING];
  z ^= Zobrist::enPassantFile[FILE_NONE];
  Key expected = z;
  // cout << "Zobrist= " << z << std::endl;
  EXPECT_EQ(8408280106960045251, z);

  z ^= Zobrist::pieces[WHITE_KING][SQ_E1];
  z ^= Zobrist::pieces[WHITE_KING][SQ_E2];
  // cout << "Zobrist= " << z << std::endl;

  z ^= Zobrist::pieces[WHITE_KING][SQ_E2];
  z ^= Zobrist::pieces[WHITE_KING][SQ_E1];
  // cout << "Zobrist= " << z << std::endl;
  EXPECT_EQ(expected, z);

  z ^= Zobrist::castlingRights[WHITE_CASTLING];
  // cout << "Zobrist= " << z << std::endl;

  z ^= Zobrist::castlingRights[WHITE_CASTLING];
  // cout << "Zobrist= " << z << std::endl;
  EXPECT_EQ(expected, z);

  z ^= Zobrist::castlingRights[WHITE_OO];
  // cout << "Zobrist= " << z << std::endl;

  z ^= Zobrist::castlingRights[WHITE_OO];
  // cout << "Zobrist= " << z << std::endl;
  EXPECT_EQ(expected, z);

  z ^= Zobrist::enPassantFile[fileOf(SQ_D3)];
  // cout << "Zobrist= " << z << std::endl;

  z ^= Zobrist::enPassantFile[fileOf(SQ_D3)];
  // cout << "Zobrist= " << z << std::endl;
  EXPECT_EQ(expected, z);

  z ^= Zobrist::nextPlayer;
  // cout << "Zobrist= " << z << std::endl;

  z ^= Zobrist::nextPlayer;
  // cout << "Zobrist= " << z << std::endl;
  EXPECT_EQ(expected, z);
}

TEST_F(PositionTest, Setup) {
  string fen;

  // Constructor (default)
  Position position;
  EXPECT_EQ(WHITE, position.getNextPlayer());
  EXPECT_EQ(BLACK, ~position.getNextPlayer());
  EXPECT_EQ(position.getMaterial(WHITE), position.getMaterial(BLACK));
  EXPECT_EQ(24, position.getGamePhase());
  EXPECT_FLOAT_EQ(1.0, position.getGamePhaseFactor());
  EXPECT_EQ(position.getMidPosValue(WHITE), position.getMidPosValue(BLACK));
  EXPECT_EQ(-225, position.getMidPosValue(WHITE));
  EXPECT_EQ(-225, position.getMidPosValue(BLACK));
  EXPECT_EQ(WHITE_KING, position.getPiece(SQ_E1));
  EXPECT_EQ(BLACK_KING, position.getPiece(SQ_E8));
  EXPECT_EQ(WHITE_KNIGHT, position.getPiece(SQ_B1));
  EXPECT_EQ(BLACK_KNIGHT, position.getPiece(SQ_B8));

  // Copy constructor
  Position position2 = Position(position);
  EXPECT_EQ(WHITE, position2.getNextPlayer());
  EXPECT_EQ(BLACK, ~position2.getNextPlayer());
  EXPECT_EQ(position2.getMaterial(WHITE), position2.getMaterial(BLACK));
  EXPECT_EQ(24, position2.getGamePhase());
  EXPECT_FLOAT_EQ(1.0, position2.getGamePhaseFactor());
  EXPECT_EQ(position2.getMidPosValue(WHITE), position2.getMidPosValue(BLACK));
  EXPECT_EQ(-225, position2.getMidPosValue(WHITE));
  EXPECT_EQ(-225, position2.getMidPosValue(BLACK));
  EXPECT_EQ(WHITE_KING, position2.getPiece(SQ_E1));
  EXPECT_EQ(BLACK_KING, position2.getPiece(SQ_E8));
  EXPECT_EQ(WHITE_KNIGHT, position2.getPiece(SQ_B1));
  EXPECT_EQ(BLACK_KNIGHT, position2.getPiece(SQ_B8));

  // Copy constructor
  Position position3 = position;
  EXPECT_EQ(WHITE, position3.getNextPlayer());
  EXPECT_EQ(BLACK, ~position3.getNextPlayer());
  EXPECT_EQ(position3.getMaterial(WHITE), position3.getMaterial(BLACK));
  EXPECT_EQ(24, position3.getGamePhase());
  EXPECT_FLOAT_EQ(1.0, position3.getGamePhaseFactor());
  EXPECT_EQ(position3.getMidPosValue(WHITE), position3.getMidPosValue(BLACK));
  EXPECT_EQ(-225, position3.getMidPosValue(WHITE));
  EXPECT_EQ(-225, position3.getMidPosValue(BLACK));
  EXPECT_EQ(WHITE_KING, position3.getPiece(SQ_E1));
  EXPECT_EQ(BLACK_KING, position3.getPiece(SQ_E8));
  EXPECT_EQ(WHITE_KNIGHT, position3.getPiece(SQ_B1));
  EXPECT_EQ(BLACK_KNIGHT, position3.getPiece(SQ_B8));

  // Copy assignment constructor
  Position position4;
  position4 = position3;
  EXPECT_EQ(WHITE, position4.getNextPlayer());
  EXPECT_EQ(BLACK, ~position4.getNextPlayer());
  EXPECT_EQ(position4.getMaterial(WHITE), position4.getMaterial(BLACK));
  EXPECT_EQ(24, position4.getGamePhase());
  EXPECT_FLOAT_EQ(1.0, position4.getGamePhaseFactor());
  EXPECT_EQ(position4.getMidPosValue(WHITE), position4.getMidPosValue(BLACK));
  EXPECT_EQ(-225, position4.getMidPosValue(WHITE));
  EXPECT_EQ(-225, position4.getMidPosValue(BLACK));
  EXPECT_EQ(WHITE_KING, position4.getPiece(SQ_E1));
  EXPECT_EQ(BLACK_KING, position4.getPiece(SQ_E8));
  EXPECT_EQ(WHITE_KNIGHT, position4.getPiece(SQ_B1));
  EXPECT_EQ(BLACK_KNIGHT, position4.getPiece(SQ_B8));

  // Constructor  (with FEN)
  fen      = string("r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1 b kq e3 10 113");
  position = Position(fen.c_str());
  EXPECT_EQ(fen, position.strFen());
  EXPECT_EQ(SQ_E3, position.getEnPassantSquare());
  EXPECT_EQ(BLACK, position.getNextPlayer());
  EXPECT_EQ(3400, position.getMaterial(WHITE));
  EXPECT_EQ(6940, position.getMaterial(BLACK));
  EXPECT_EQ(22, position.getGamePhase());
  EXPECT_FLOAT_EQ((22.0 / 24), position.getGamePhaseFactor());
  EXPECT_EQ(90, position.getMidPosValue(WHITE));
  EXPECT_EQ(7, position.getMidPosValue(BLACK));
  EXPECT_EQ(WHITE_KING, position.getPiece(SQ_G1));
  EXPECT_EQ(BLACK_KING, position.getPiece(SQ_E8));
  EXPECT_EQ(WHITE_ROOK, position.getPiece(SQ_G3));
  EXPECT_EQ(BLACK_QUEEN, position.getPiece(SQ_C6));

  // Further constructor tests with FEN
  fen      = string("r1bqkb1r/pppp1ppp/2n2n2/3Pp3/8/8/PPP1PPPP/RNBQKBNR w - e6 0 1");
  position = Position(fen.c_str());
  EXPECT_EQ(fen, position.strFen());
  EXPECT_EQ(SQ_E6, position.getEnPassantSquare());
  EXPECT_EQ(WHITE, position.getNextPlayer());

  fen      = string("R6R/3Q4/1Q4Q1/4Q3/2Q4Q/Q4Q2/pp1Q4/kBNN1KB1 w - - 0 1");
  position = Position(fen.c_str());
  EXPECT_EQ(fen, position.strFen());
  EXPECT_EQ(WHITE, position.getNextPlayer());
  EXPECT_EQ(24, position.getGamePhase());
}

TEST_F(PositionTest, Output) {
  ostringstream expected;
  ostringstream actual;

  // start pos
  Position position;
  EXPECT_EQ(START_POSITION_FEN, position.strFen());
  expected << "  +---+---+---+---+---+---+---+---+\n"
              "8 | r | n | b | q | k | b | n | r |\n"
              "  +---+---+---+---+---+---+---+---+\n"
              "7 | * | * | * | * | * | * | * | * |\n"
              "  +---+---+---+---+---+---+---+---+\n"
              "6 |   |   |   |   |   |   |   |   |\n"
              "  +---+---+---+---+---+---+---+---+\n"
              "5 |   |   |   |   |   |   |   |   |\n"
              "  +---+---+---+---+---+---+---+---+\n"
              "4 |   |   |   |   |   |   |   |   |\n"
              "  +---+---+---+---+---+---+---+---+\n"
              "3 |   |   |   |   |   |   |   |   |\n"
              "  +---+---+---+---+---+---+---+---+\n"
              "2 | O | O | O | O | O | O | O | O |\n"
              "  +---+---+---+---+---+---+---+---+\n"
              "1 | R | N | B | Q | K | B | N | R |\n"
              "  +---+---+---+---+---+---+---+---+\n"
              "    A   B   C   D   E   F   G   H  \n\n";
  actual << position.strBoard();
  //  cout << expected.str() << endl;
  NEWLINE;
  //  cout << actual.str() << endl;
  EXPECT_EQ(expected.str(), actual.str());

  string fen("r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1 b kq e3 10 113");
  position = Position(fen.c_str());
  EXPECT_EQ(fen, position.strFen());

  fen = string(
    "r1b1k2r/pp2ppbp/2n3p1/q7/3pP3/2P1BN2/P2Q1PPP/2R1KB1R w Kkq - 0 11");
  position = Position(fen.c_str());
  EXPECT_EQ(fen, position.strFen());

  fen      = string("rnbqkbnr/1ppppppp/8/p7/Q1P5/8/PP1PPPPP/RNB1KBNR b KQkq - 1 2");
  position = Position(fen.c_str());
  EXPECT_EQ(fen, position.strFen());
}

TEST_F(PositionTest, Copy) {
  string fen("r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1 b kq e3 10 113");
  Position position(fen.c_str());
  Position copy(position);
  EXPECT_EQ(position.getZobristKey(), copy.getZobristKey());
  EXPECT_EQ(position.strFen(), copy.strFen());
  EXPECT_EQ(position.strBoard(), copy.strBoard());
  EXPECT_EQ(position.getOccupiedBB(WHITE), copy.getOccupiedBB(WHITE));
  EXPECT_EQ(position.getOccupiedBB(BLACK), copy.getOccupiedBB(BLACK));
  EXPECT_EQ(SQ_E3, copy.getEnPassantSquare());
  EXPECT_EQ(BLACK, copy.getNextPlayer());
}

TEST_F(PositionTest, PosValue) {
  Position position("8/8/8/8/8/8/8/8 w - - 0 1");
  //  cout << position.str() << endl;

  position.putPiece(WHITE_KING, SQ_E1);
  position.putPiece(BLACK_KING, SQ_E8);
  position.putPiece(WHITE_KNIGHT, SQ_E4);
  position.putPiece(BLACK_KNIGHT, SQ_D5);
  // cout << position.str() << endl;
  EXPECT_EQ(2, position.getGamePhase());
  EXPECT_EQ(2320, position.getMaterial(WHITE));
  EXPECT_EQ(2320, position.getMaterial(BLACK));
  EXPECT_EQ(0, position.getMidPosValue(WHITE));
  EXPECT_EQ(0, position.getMidPosValue(BLACK));
}

TEST_F(PositionTest, Bitboards) {
  string fen("r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1 b kq e3 10 113");
  Position position(fen.c_str());

  Bitboard bb, expected;

  bb = position.getPieceBb(WHITE, KING);
  //  cout << Bitboards::str(bb);
  EXPECT_EQ(Bitboards::sqBb[SQ_G1], bb);

  bb = position.getPieceBb(BLACK, KING);
  //  cout << Bitboards::str(bb);
  EXPECT_EQ(Bitboards::sqBb[SQ_E8], bb);

  bb = position.getPieceBb(WHITE, ROOK);
  //  cout << Bitboards::str(bb);
  expected = Bitboards::sqBb[SQ_B1] | Bitboards::sqBb[SQ_G3];
  EXPECT_EQ(expected, bb);

  bb = position.getPieceBb(BLACK, ROOK);
  //  cout << Bitboards::str(bb);
  expected = Bitboards::sqBb[SQ_A8] | Bitboards::sqBb[SQ_H8];
  EXPECT_EQ(expected, bb);

  bb = position.getPieceBb(WHITE, PAWN);
  //  cout << Bitboards::str(bb);
  expected = Bitboards::sqBb[SQ_E4] | Bitboards::sqBb[SQ_F2] | Bitboards::sqBb[SQ_G2] | Bitboards::sqBb[SQ_H2];
  EXPECT_EQ(expected, bb);

  bb = position.getPieceBb(BLACK, KNIGHT);
  //  cout << Bitboards::str(bb);
  expected = Bitboards::sqBb[SQ_D7] | Bitboards::sqBb[SQ_G6];
  EXPECT_EQ(expected, bb);
}

TEST_F(PositionTest, doUndoMoveNormal) {
  Position position;
  //  cout << position.str() << endl;

  // do move tests
  position.doMove(createMove(SQ_E2, SQ_E4, NORMAL));
  //  cout << position.str() << endl;
  EXPECT_EQ(SQ_E3, position.getEnPassantSquare());
  EXPECT_EQ("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", position.strFen());

  position.doMove(createMove(SQ_D7, SQ_D5, NORMAL));
  //  cout << position.str() << endl;
  EXPECT_EQ(SQ_D6, position.getEnPassantSquare());
  EXPECT_EQ("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2", position.strFen());

  position.doMove(createMove(SQ_E4, SQ_D5, NORMAL));
  //  cout << position.str() << endl;
  EXPECT_EQ(SQ_NONE, position.getEnPassantSquare());
  EXPECT_EQ(BLACK, position.getNextPlayer());
  EXPECT_EQ(5900, position.getMaterial(BLACK));
  EXPECT_EQ("rnbqkbnr/ppp1pppp/8/3P4/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2", position.strFen());

  // undo move tests

  position.undoMove();
  EXPECT_EQ("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2", position.strFen());
  EXPECT_EQ(SQ_D6, position.getEnPassantSquare());
  EXPECT_EQ(WHITE, position.getNextPlayer());
  EXPECT_EQ(6000, position.getMaterial(BLACK));

  position.undoMove();
  EXPECT_EQ(SQ_E3, position.getEnPassantSquare());
  EXPECT_EQ("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", position.strFen());

  position.undoMove();
  EXPECT_EQ(Position().strFen(), position.strFen());
}

TEST_F(PositionTest, doUndoMovePromotion) {
  Position position("6k1/P7/8/8/8/8/8/3K4 w - - 0 1");
  //  cout << position.str() << endl;

  // do move

  position.doMove(createMove(SQ_A7, SQ_A8, PROMOTION, QUEEN));
  //  cout << position.str() << endl;
  EXPECT_EQ(BLACK, position.getNextPlayer());
  EXPECT_EQ(2900, position.getMaterial(WHITE));
  EXPECT_EQ("Q5k1/8/8/8/8/8/8/3K4 b - - 0 1", position.strFen());

  // undo move

  position.undoMove();
  EXPECT_EQ(WHITE, position.getNextPlayer());
  EXPECT_EQ(2100, position.getMaterial(WHITE));
  EXPECT_EQ("6k1/P7/8/8/8/8/8/3K4 w - - 0 1", position.strFen());
}

TEST_F(PositionTest, doUndoMoveEnPassantCapture) {
  // do move
  Position position("rnbqkbnr/ppp1pppp/8/8/3pP3/2N2N2/PPPP1PPP/R1BQKB1R b KQkq e3 0 3");
  //  cout << position.str() << endl;
  position.doMove(createMove(SQ_D4, SQ_E3, ENPASSANT));
  //  cout << position.str() << endl;
  EXPECT_EQ(WHITE, position.getNextPlayer());
  EXPECT_EQ(5900, position.getMaterial(WHITE));
  EXPECT_EQ("rnbqkbnr/ppp1pppp/8/8/8/2N1pN2/PPPP1PPP/R1BQKB1R w KQkq - 0 4", position.strFen());

  // undo move
  position.undoMove();
  EXPECT_EQ(BLACK, position.getNextPlayer());
  EXPECT_EQ(6000, position.getMaterial(WHITE));
  EXPECT_EQ("rnbqkbnr/ppp1pppp/8/8/3pP3/2N2N2/PPPP1PPP/R1BQKB1R b KQkq e3 0 3",
            position.strFen());

  // do move
  position = Position(
    "r1bqkb1r/pppp1ppp/2n2n2/3Pp3/8/8/PPP1PPPP/RNBQKBNR w KQkq e6 0 1");
  //  cout << position.str() << endl;
  position.doMove(createMove(SQ_D5, SQ_E6, ENPASSANT));
  //  cout << position.str() << endl;
  EXPECT_EQ(BLACK, position.getNextPlayer());
  EXPECT_EQ(5900, position.getMaterial(BLACK));
  EXPECT_EQ("r1bqkb1r/pppp1ppp/2n1Pn2/8/8/8/PPP1PPPP/RNBQKBNR b KQkq - 0 1",
            position.strFen());

  // undo move
  position.undoMove();
  EXPECT_EQ(WHITE, position.getNextPlayer());
  EXPECT_EQ(6000, position.getMaterial(WHITE));
  EXPECT_EQ("r1bqkb1r/pppp1ppp/2n2n2/3Pp3/8/8/PPP1PPPP/RNBQKBNR w KQkq e6 0 1",
            position.strFen());
}

TEST_F(PositionTest, doMoveCASTLING) {
  // do move
  Position position("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R w KQkq -");
  ;
  ;
  // cout << position.str() << endl;
  position.doMove(createMove(SQ_E1, SQ_G1, CASTLING));
  // cout << position.str() << endl;
  EXPECT_EQ("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R4RK1 b kq - 1 1", position.strFen());

  // undo move
  position.undoMove();
  EXPECT_EQ("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R w KQkq - 0 1", position.strFen());

  // do move
  position = Position("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R w KQkq -");
  ;
  ;
  // cout << position.str() << endl;
  position.doMove(createMove(SQ_E1, SQ_C1, CASTLING));
  // cout << position.str() << endl;
  EXPECT_EQ("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/2KR3R b kq - 1 1", position.strFen());

  // undo move
  position.undoMove();
  EXPECT_EQ("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R w KQkq - 0 1", position.strFen());

  // do move
  position = Position("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R b KQkq -");
  ;
  ;
  // cout << position.str() << endl;
  position.doMove(createMove(SQ_E8, SQ_G8, CASTLING));
  // cout << position.str() << endl;
  EXPECT_EQ("r4rk1/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R w KQ - 1 2", position.strFen());

  // undo move
  position.undoMove();
  EXPECT_EQ("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R b KQkq - 0 1", position.strFen());

  // do move
  position = Position("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R b KQkq -");
  ;
  ;
  // cout << position.str() << endl;
  position.doMove(createMove(SQ_E8, SQ_C8, CASTLING));
  // cout << position.str() << endl;
  EXPECT_EQ("2kr3r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R w KQ - 1 2", position.strFen());

  // undo move
  position.undoMove();
  EXPECT_EQ("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R b KQkq - 0 1", position.strFen());

  // do move
  position = Position("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R w KQkq -");
  ;
  ;
  // cout << position.str() << endl;
  position.doMove(createMove(SQ_E1, SQ_F1, NORMAL));
  // cout << position.str() << endl;
  EXPECT_EQ("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R4K1R b kq - 1 1", position.strFen());

  // undo move
  position.undoMove();
  EXPECT_EQ("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R w KQkq - 0 1", position.strFen());

  // do move
  position = Position("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R w KQkq -");
  ;
  ;
  // cout << position.str() << endl;
  position.doMove(createMove(SQ_H1, SQ_F1, NORMAL));
  // cout << position.str() << endl;
  EXPECT_EQ("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3KR2 b Qkq - 1 1", position.strFen());

  // undo move
  position.undoMove();
  EXPECT_EQ("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R w KQkq - 0 1", position.strFen());

  // do move
  position = Position("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R b KQkq -");
  ;
  ;
  // cout << position.str() << endl;
  position.doMove(createMove(SQ_A8, SQ_C8, NORMAL));
  // cout << position.str() << endl;
  EXPECT_EQ("2r1k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R w KQk - 1 2", position.strFen());

  // undo move
  position.undoMove();
  EXPECT_EQ("r3k2r/pppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/PPPQ1PPP/R3K2R b KQkq - 0 1", position.strFen());

  // do move
  position = Position("r3k2r/1ppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/1PPQ1PPP/R3K2R b KQkq - 0 1");
  // cout << position.str() << endl;
  position.doMove(createMove(SQ_A8, SQ_A1, NORMAL));
  // cout << position.str() << endl;
  EXPECT_EQ("4k2r/1ppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/1PPQ1PPP/r3K2R w Kk - 0 2", position.strFen());

  // undo move
  position.undoMove();
  EXPECT_EQ("r3k2r/1ppqbppp/2np1n2/1B2p1B1/4P1b1/2NP1N2/1PPQ1PPP/R3K2R b KQkq - 0 1", position.strFen());
}

TEST_F(PositionTest, doNullMove) {
  // do move
  Position position("rnbqkbnr/ppp1pppp/8/8/3pP3/2N2N2/PPPP1PPP/R1BQKB1R b KQkq e3");
  //  cout << position.str() << endl;

  position.doNullMove();
  //  cout << position.str() << endl;
  EXPECT_EQ("rnbqkbnr/ppp1pppp/8/8/3pP3/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 0 2", position.strFen());

  position.undoNullMove();
  //  cout << position.str() << endl;
  EXPECT_EQ("rnbqkbnr/ppp1pppp/8/8/3pP3/2N2N2/PPPP1PPP/R1BQKB1R b KQkq e3 0 1", position.strFen());
}


TEST_F(PositionTest, repetitionSimple) {
  Position position;

  position.doMove(createMove(SQ_E2, SQ_E4, NORMAL));
  position.doMove(createMove(SQ_E7, SQ_E5, NORMAL));

  // cout << "Repetitions: " << position.countRepetitions() << endl;
  EXPECT_EQ(0, position.countRepetitions());

  // Simple repetition
  // takes 3 loops to get to repetition
  for (int i = 0; i <= 2; i++) {
    position.doMove(createMove(SQ_G1, SQ_F3, NORMAL));
    position.doMove(createMove(SQ_B8, SQ_C6, NORMAL));
    position.doMove(createMove(SQ_F3, SQ_G1, NORMAL));
    position.doMove(createMove(SQ_C6, SQ_B8, NORMAL));
    // cout << "Repetitions: " << position.countRepetitions() << endl;
  }

  // cout << "3-Repetitions: " << position.countRepetitions() << endl;
  EXPECT_EQ(2, position.countRepetitions());
  EXPECT_TRUE(position.checkRepetitions(2));
}

TEST_F(PositionTest, repetitionAdvanced) {
  Position position("6k1/p3q2p/1n1Q2pB/8/5P2/6P1/PP5P/3R2K1 b - -");
  ;
  ;

  position.doMove(createMove(SQ_E7, SQ_E3, NORMAL));
  position.doMove(createMove(SQ_G1, SQ_G2, NORMAL));

  //  cout << "Repetitions: " << position.countRepetitions() << endl;
  EXPECT_EQ(0, position.countRepetitions());

  // takes 2 loops to get to repetition
  for (int i = 0; i < 2; i++) {
    position.doMove(createMove(SQ_E3, SQ_E2, NORMAL));
    position.doMove(createMove(SQ_G2, SQ_G1, NORMAL));
    position.doMove(createMove(SQ_E2, SQ_E3, NORMAL));
    position.doMove(createMove(SQ_G1, SQ_G2, NORMAL));
    //    cout << "Repetitions: " << position.countRepetitions() << endl;
  }

  //  cout << "3-Repetitions: " << position.countRepetitions() << endl;
  EXPECT_EQ(2, position.countRepetitions());
  EXPECT_TRUE(position.checkRepetitions(2));
}

TEST_F(PositionTest, insufficientMaterial) {
  Position position;

  // 	both sides have a bare king
  position = Position("8/3k4/8/8/8/8/4K3/8 w - -");
  ;
  ;
  EXPECT_TRUE(position.checkInsufficientMaterial());

  // 	one side has a king and a minor piece against a bare king
  // 	both sides have a king and a minor piece each
  position = Position("8/3k4/8/8/8/2B5/4K3/8 w - -");
  ;
  EXPECT_TRUE(position.checkInsufficientMaterial());
  position = Position("8/8/4K3/8/8/2b5/4k3/8 b - -");
  ;
  EXPECT_TRUE(position.checkInsufficientMaterial());

  // 	both sides have a king and a bishop, the bishops being the same color
  position = Position("8/8/3BK3/8/8/2b5/4k3/8 b - -");
  ;
  EXPECT_TRUE(position.checkInsufficientMaterial());
  position = Position("8/8/2B1K3/8/8/8/2b1k3/8 b - -");
  ;
  EXPECT_TRUE(position.checkInsufficientMaterial());
  position = Position("8/8/4K3/2B5/8/8/2b1k3/8 b - -");
  ;
  EXPECT_TRUE(position.checkInsufficientMaterial());

  // one side has two bishops a mate can be forced
  position = Position("8/8/2B1K3/2B5/8/8/2n1k3/8 b - -");
  ;
  EXPECT_FALSE(position.checkInsufficientMaterial());

  // 	two knights against the bare king
  position = Position("8/8/2NNK3/8/8/8/4k3/8 w - -");
  ;
  EXPECT_TRUE(position.checkInsufficientMaterial());
  position = Position("8/8/2nnk3/8/8/8/4K3/8 w - -");
  ;
  EXPECT_TRUE(position.checkInsufficientMaterial());

  // 	the weaker side has a minor piece against two knights
  position = Position("8/8/2n1kn2/8/8/8/4K3/4B3 w - -");
  ;
  EXPECT_TRUE(position.checkInsufficientMaterial());

  // 	two bishops draw against a bishop
  position = Position("8/8/3bk1b1/8/8/8/4K3/4B3 w - -");
  ;
  EXPECT_TRUE(position.checkInsufficientMaterial());

  // 	two minor pieces against one draw, except when the stronger side has a bishop pair
  position = Position("8/8/3bk1b1/8/8/8/4K3/4N3 w - -");
  ;
  EXPECT_FALSE(position.checkInsufficientMaterial());
  position = Position("8/8/3bk1n1/8/8/8/4K3/4N3 w - -");
  ;
  EXPECT_TRUE(position.checkInsufficientMaterial());

  // bugs
  position = Position("8/8/8/6k1/8/4K3/8/r7 b - -");
  ;
  EXPECT_FALSE(position.checkInsufficientMaterial());
}

TEST_F(PositionTest, hasCheck) {
  string fen;
  Position position;

  fen      = "r3k2r/1ppn3p/2q1qNn1/8/2q1Pp2/6R1/p1p2PPP/1R4K1 b kq e3";
  position = Position(fen);

  EXPECT_TRUE(position.isAttacked(SQ_E8, WHITE));
  EXPECT_TRUE(position.hasCheck());
}

TEST_F(PositionTest, isAttacked) {
  string fen;
  Position position;

  fen      = "r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1 b kq e3";
  position = Position(fen);

  // pawns
  EXPECT_TRUE(position.isAttacked(SQ_G3, WHITE));
  EXPECT_TRUE(position.isAttacked(SQ_E3, WHITE));
  EXPECT_TRUE(position.isAttacked(SQ_B1, BLACK));
  EXPECT_TRUE(position.isAttacked(SQ_E4, BLACK));
  EXPECT_TRUE(position.isAttacked(SQ_E3, BLACK));

  // knight
  EXPECT_TRUE(position.isAttacked(SQ_E5, BLACK));
  EXPECT_TRUE(position.isAttacked(SQ_F4, BLACK));
  EXPECT_FALSE(position.isAttacked(SQ_G1, BLACK));

  // sliding
  EXPECT_TRUE(position.isAttacked(SQ_G6, WHITE));
  EXPECT_TRUE(position.isAttacked(SQ_A5, BLACK));

  fen      = "rnbqkbnr/1ppppppp/8/p7/Q1P5/8/PP1PPPPP/RNB1KBNR b KQkq - 1 2";
  position = Position(fen);

  // king
  EXPECT_TRUE(position.isAttacked(SQ_D1, WHITE));
  EXPECT_FALSE(position.isAttacked(SQ_E1, BLACK));

  // rook
  EXPECT_TRUE(position.isAttacked(SQ_A5, BLACK));
  EXPECT_FALSE(position.isAttacked(SQ_A4, BLACK));

  // queen
  EXPECT_FALSE(position.isAttacked(SQ_E8, WHITE));
  EXPECT_TRUE(position.isAttacked(SQ_D7, WHITE));
  EXPECT_FALSE(position.isAttacked(SQ_E8, WHITE));

  // en passant
  fen      = "rnbqkbnr/1pp1pppp/p7/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6";
  position = Position(fen);
  EXPECT_TRUE(position.isAttacked(SQ_D5, WHITE));

  fen      = "rnbqkbnr/1pp1pppp/p7/2Pp4/8/8/PP1PPPPP/RNBQKBNR w KQkq d6";
  position = Position(fen);
  EXPECT_TRUE(position.isAttacked(SQ_D5, WHITE));

  fen      = "rnbqkbnr/pppp1ppp/8/8/3Pp3/7P/PPP1PPP1/RNBQKBNR b - d3";
  position = Position(fen);
  EXPECT_TRUE(position.isAttacked(SQ_D4, BLACK));

  fen      = "rnbqkbnr/pppp1ppp/8/8/2pP4/7P/PPP1PPP1/RNBQKBNR b - d3";
  position = Position(fen);
  EXPECT_TRUE(position.isAttacked(SQ_D4, BLACK));

  // bug tests
  fen      = "r1bqk1nr/pppp1ppp/2nb4/1B2B3/3pP3/8/PPP2PPP/RN1QK1NR b KQkq -";
  position = Position(fen);
  EXPECT_FALSE(position.isAttacked(SQ_E8, WHITE));
  EXPECT_FALSE(position.isAttacked(SQ_E1, BLACK));

  fen      = "rnbqkbnr/ppp1pppp/8/1B6/3Pp3/8/PPP2PPP/RNBQK1NR b KQkq -";
  position = Position(fen);
  EXPECT_TRUE(position.isAttacked(SQ_E8, WHITE));
  EXPECT_FALSE(position.isAttacked(SQ_E1, BLACK));

  fen      = "8/1pk2p2/2p5/5p2/8/1pp2Q2/5K2/8 w - -";
  position = Position(fen);
  EXPECT_FALSE(position.isAttacked(SQ_F7, WHITE));
  EXPECT_FALSE(position.isAttacked(SQ_B7, WHITE));
  EXPECT_FALSE(position.isAttacked(SQ_B3, WHITE));
}

TEST_F(PositionTest, giveCheck) {
  // DIRECT CHECKS
  Position p;
  Move move;

  // Pawns
  p    = Position("4r3/1pn3k1/4p1b1/p1Pp1P1r/3P2NR/1P3B2/3K2P1/4R3 w - -");
  move = createMove(SQ_F5, SQ_F6, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("5k2/4pp2/1N2n1p1/r3P2p/P5PP/2rR1K2/P7/3R4 b - -");
  move = createMove(SQ_H5, SQ_G4, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  // promotion
  p    = Position("1k3r2/1p1bP3/2p2p1Q/Ppb5/4Rp1P/2q2N1P/5PB1/6K1 w - -");
  move = createMove(SQ_E7, SQ_F8, PROMOTION, QUEEN);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("1r3r2/1p1bP2k/2p2n2/p1Pp4/P2N1PpP/1R2p3/1P2P1BP/3R2K1 w - -");
  move = createMove(SQ_E7, SQ_F8, PROMOTION, KNIGHT);
  EXPECT_TRUE(p.givesCheck(move));

  // Knights
  p    = Position("5k2/4pp2/1N2n1p1/r3P2p/P5PP/2rR1K2/P7/3R4 w - -");
  move = createMove(SQ_B6, SQ_D7, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("5k2/4pp2/1N2n1p1/r3P2p/P5PP/2rR1K2/P7/3R4 b - -");
  move = createMove(SQ_E6, SQ_D4, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  // Rooks
  p    = Position("5k2/4pp2/1N2n1pp/r3P3/P5PP/2rR4/P3K3/3R4 w - -");
  move = createMove(SQ_D3, SQ_D8, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("5k2/4pp2/1N2n1pp/r3P3/P5PP/2rR4/P3K3/3R4 b - -");
  move = createMove(SQ_C3, SQ_C2, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  // blocked opponent piece - no check
  p    = Position("5k2/4pp2/1N2n1pp/r3P3/P5PP/2rR4/P2RK3/8 b - -");
  move = createMove(SQ_C3, SQ_C2, NORMAL, PT_NONE);
  EXPECT_FALSE(p.givesCheck(move));

  // blocked own piece - no check
  p    = Position("5k2/4pp2/1N2n1pp/r3P3/P5PP/2rR4/P2nK3/3R4 b - -");
  move = createMove(SQ_C3, SQ_C2, NORMAL, PT_NONE);
  EXPECT_FALSE(p.givesCheck(move));

  // Bishop
  p    = Position("6k1/3q2b1/p1rrnpp1/P3p3/2B1P3/1p1R3Q/1P4PP/1B1R3K w - -");
  move = createMove(SQ_C4, SQ_E6, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  // Queen
  p    = Position("5k2/4pp2/1N2n1pp/r3P3/P5PP/2qR4/P3K3/3R4 b - -");
  move = createMove(SQ_C3, SQ_C2, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("6k1/3q2b1/p1rrnpp1/P3p3/2B1P3/1p1R3Q/1P4PP/1B1R3K w - -");
  move = createMove(SQ_H3, SQ_E6, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("6k1/p3q2p/1n1Q2pB/8/5P2/6P1/PP5P/3R2K1 b - -");
  move = createMove(SQ_E7, SQ_E3, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  // no check
  p    = Position("6k1/p3q2p/1n1Q2pB/8/5P2/6P1/PP5P/3R2K1 b - -");
  move = createMove(SQ_E7, SQ_E4, NORMAL, PT_NONE);
  EXPECT_FALSE(p.givesCheck(move));

  // CASTLING checks
  p    = Position("r4k1r/8/8/8/8/8/8/R3K2R w KQ -");
  move = createMove(SQ_E1, SQ_G1, CASTLING, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("r2k3r/8/8/8/8/8/8/R3K2R w KQ -");
  move = createMove(SQ_E1, SQ_C1, CASTLING, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("r3k2r/8/8/8/8/8/8/R4K1R b kq -");
  move = createMove(SQ_E8, SQ_G8, CASTLING, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("r3k2r/8/8/8/8/8/8/R2K3R b kq -");
  move = createMove(SQ_E8, SQ_C8, CASTLING, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("r6r/8/8/8/8/8/8/2k1K2R w K -");
  move = createMove(SQ_E1, SQ_G1, CASTLING, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  // en passant checks
  p    = Position("8/3r1pk1/p1R2p2/1p5p/r2Pp3/PRP3P1/4KP1P/8 b - d3");
  move = createMove(SQ_E4, SQ_D3, ENPASSANT, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  // REVEALED CHECKS
  p    = Position("6k1/8/3P1bp1/2BNp3/8/1Q3P1q/7r/1K2R3 w - -");
  move = createMove(SQ_D5, SQ_E7, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("6k1/8/3P1bp1/2BNp3/8/1Q3P1q/7r/1K2R3 w - -");
  move = createMove(SQ_D5, SQ_C7, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("1Q1N2k1/8/3P1bp1/2B1p3/8/5P1q/7r/1K2R3 w - -");
  move = createMove(SQ_D8, SQ_E6, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("1R1N2k1/8/3P1bp1/2B1p3/8/5P1q/7r/1K2R3 w - -");
  move = createMove(SQ_D8, SQ_E6, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  // revealed by en passant capture
  p    = Position("8/b2r1pk1/p1R2p2/1p5p/r2Pp3/PRP3P1/5K1P/8 b - d3");
  move = createMove(SQ_E4, SQ_D3, ENPASSANT, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  // Misc
  p    = Position("2r1r3/pb1n1kpn/1p1qp3/6p1/2PP4/8/P2Q1PPP/3R1RK1 w - -");
  move = createMove(SQ_F2, SQ_F4, NORMAL, PT_NONE);
  EXPECT_FALSE(p.givesCheck(move));

  p    = Position("2r1r1k1/pb3pp1/1p1qpn2/4n1p1/2PP4/6KP/P2Q1PP1/3RR3 b - -");
  move = createMove(SQ_E5, SQ_D3, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("R6R/3Q4/1Q4Q1/4Q3/2Q4Q/Q1NNQQ2/1p6/qk3KB1 b - -");
  move = createMove(SQ_B1, SQ_C2, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("8/8/8/8/8/5K2/R7/7k w - -");
  move = createMove(SQ_A2, SQ_H2, NORMAL, PT_NONE);
  EXPECT_TRUE(p.givesCheck(move));

  p    = Position("r1bqkb1r/ppp1pppp/2n2n2/1B1P4/8/8/PPPP1PPP/RNBQK1NR w KQkq -");
  move = createMove(SQ_D5, SQ_C6, NORMAL, PT_NONE);
  EXPECT_FALSE(p.givesCheck(move));

  p    = Position("rnbq1bnr/pppkpppp/8/3p4/3P4/3Q4/PPP1PPPP/RNB1KBNR w KQ -");
  move = createMove(SQ_D3, SQ_H7, NORMAL, PT_NONE);
  EXPECT_FALSE(p.givesCheck(move));
}

TEST_F(PositionTest, isCapturingMove) {
  string fen;
  Position position;

  fen      = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/6R1/pbp2PPP/1R4K1 b kq e3";
  position = Position(fen);
  EXPECT_TRUE(position.isCapturingMove(createMove(SQ_A2, SQ_B1, PROMOTION)));
  EXPECT_FALSE(position.isCapturingMove(createMove(SQ_A2, SQ_A1, PROMOTION)));
  EXPECT_TRUE(position.isCapturingMove(createMove(SQ_C6, SQ_E4, NORMAL)));
  EXPECT_FALSE(position.isCapturingMove(createMove(SQ_C4, SQ_F1, NORMAL)));
}

TEST_F(PositionTest, isLegalMove) {
  string fen;
  Position position;

  // no o-o castling
  fen      = "r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/B5R1/p1p2PPP/1R4K1 b kq e3";
  position = Position(fen);
  EXPECT_FALSE(position.isLegalMove(createMove(SQ_E8, SQ_G8, CASTLING)));
  EXPECT_TRUE(position.isLegalMove(createMove(SQ_E8, SQ_C8, CASTLING)));

  // in check - no castling at all
  fen      = "r3k2r/1ppn3p/2q1qNn1/8/2q1Pp2/B5R1/p1p2PPP/1R4K1 b kq e3";
  position = Position(fen);
  EXPECT_FALSE(position.isLegalMove(createMove(SQ_E8, SQ_G8, CASTLING)));
  EXPECT_FALSE(position.isLegalMove(createMove(SQ_E8, SQ_C8, CASTLING)));
}

TEST_F(PositionTest, wasLegalMove) {
  string fen;
  Position position;

  // no o-o castling
  fen      = "r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/B5R1/p1p2PPP/1R4K1 b kq e3";
  position = Position(fen);

  position.doMove(createMove(SQ_E8, SQ_G8, CASTLING));
  EXPECT_FALSE(position.wasLegalMove());
  position.undoMove();

  position.doMove(createMove(SQ_E8, SQ_C8, CASTLING));
  EXPECT_TRUE(position.wasLegalMove());
  position.undoMove();

  // in check - no castling at all
  fen      = "r3k2r/1ppn3p/2q1qNn1/8/2q1Pp2/B5R1/p1p2PPP/1R4K1 b kq e3";
  position = Position(fen);

  position.doMove(createMove(SQ_E8, SQ_G8, CASTLING));
  EXPECT_FALSE(position.wasLegalMove());
  position.undoMove();

  position.doMove(createMove(SQ_E8, SQ_C8, CASTLING));
  EXPECT_FALSE(position.wasLegalMove());
  position.undoMove();
}

#include <chrono>
using namespace std::chrono;

TEST_F(PositionTest, TimingDoMoveUndoMove) {
  const int rounds     = 5;
  const int iterations = 20'000'000;

  // position for each move type
  // fxe3 enpassant
  // fxe3 normal capture
  // o-o castling
  // Rc1 normal non capturing
  // c1Q promotion
  Position position = Position("r3k2r/1ppn3p/4q1n1/8/4Pp2/3R4/p1p2PPP/R5K1 b kq e3 0 1");
  const Move move1  = createMove(SQ_F4, SQ_E3, ENPASSANT);
  const Move move2  = createMove(SQ_F2, SQ_E3);
  const Move move3  = createMove(SQ_E8, SQ_G8, CASTLING);
  const Move move4  = createMove(SQ_D3, SQ_C3);
  const Move move5  = createMove(SQ_C2, SQ_C1, PROMOTION, QUEEN);

  for (int r = 1; r <= rounds; r++) {
    fprintln("Round {}", r);
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
      position.doMove(move1);
      position.doMove(move2);
      position.doMove(move3);
      position.doMove(move4);
      position.doMove(move5);
      position.undoMove();
      position.undoMove();
      position.undoMove();
      position.undoMove();
      position.undoMove();
    }
    auto elapsed = duration_cast<nanoseconds>(high_resolution_clock::now() - start);

    std::ostringstream os;
    os.flags(std::cout.flags());
    os.imbue(deLocale);
    os.precision(os.precision());
    os << "DoMove/UndoMove took " << elapsed.count() << " ns for " << iterations << " iterations with 5 do/undo pairs" << std::endl;
    os << "DoMove/UndoMove took " << elapsed.count() / (iterations * 5) << " ns per do/undo pair" << std::endl;
    os << "Positions per sec " << (iterations * 5 * nanoPerSec) / elapsed.count() << " pps" << std::endl;
    std::cout << os.str() << std::endl;
  }
}
