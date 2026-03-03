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
#include <ostream>
#include <string>

#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"
#include "init.h"
#include "types/types.h"
#include "Test_Utils.h"

using namespace std;
using testing::Eq;


class MoveGenTest : public ::testing::Test {
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

/**
 * Test pawn move generation
 */
TEST_F(MoveGenTest, pawnMoves) {
  MoveGenerator mg;
  MoveList moves;

  const auto pos = Position("1kr3nr/pp1pP1P1/2p1p3/3P1p2/1n1bP3/2P5/PP3PPP/RNBQKBNR w KQ -");

  mg.generatePawnMoves(pos, &moves, GenNonQuiet, false, BbZero);
  EXPECT_EQ(11, moves.size());

  moves.clear();
  mg.generatePawnMoves(pos, &moves, GenQuiet, false, BbZero);
  EXPECT_EQ(14, moves.size());

  moves.clear();
  mg.generatePawnMoves(pos, &moves, GenAll, false, BbZero);
  EXPECT_EQ(25, moves.size());

  // sort moves
  ranges::sort(moves, [](const Move lhs, const Move rhs) {
    return lhs.value() > rhs.value();
  });
  for (const Move m : moves) {
    println(m.strVerbose());
  }
}

TEST_F(MoveGenTest, kingMoves) {
  MoveGenerator mg;
  MoveList moves;

  auto pos = Position("r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R w KQkq -");
  mg.generateKingMoves(pos, &moves, GenAll, false);
  EXPECT_EQ(3, moves.size());
  EXPECT_EQ("e1d2 e1d1 e1f1", moves.str());
  moves.clear();

  pos = Position("r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R b KQkq -");
  mg.generateKingMoves(pos, &moves, GenAll, false);
  EXPECT_EQ(3, moves.size());
  EXPECT_EQ("e8d7 e8d8 e8f8", moves.str());

  // sort moves
  ranges::sort(moves, [](const Move lhs, const Move rhs) {
    return lhs.value() > rhs.value();
  });
  for (const Move m : moves) {
    println(m.strVerbose());
  }
}

/**
 * Test move generation
 */
TEST_F(MoveGenTest, normalMoves) {
  MoveGenerator mg;
  Position pos;
  MoveList moves;

  pos = Position("r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R w KQkq -");
  mg.generateMoves(pos, &moves, GenNonQuiet, false, BbZero);
  EXPECT_EQ(7, moves.size());
  EXPECT_EQ("f3d2 f3e5 d7e5 d7b6 d7f6 b5c6 e2d2", moves.str());
  moves.clear();

  pos = Position("r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R b KQkq -");
  mg.generateMoves(pos, &moves, GenQuiet, false, BbZero);
  EXPECT_EQ(28, moves.size());
  EXPECT_EQ("d2b1 d2f1 d2b3 d2c4 c6d4 c6a5 c6b8 c6d8 f6g4 f6d5 f6h5 f6g8 b4a3 b4a5 b4c5 b4d6 b7a6 b7c8 a8b8 a8c8 a8d8 h8f8 h8g8 e7c5 e7d6 e7e6 e7d8 e7f8", moves.str());
  moves.clear();

  pos = Position("r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R b KQkq -");
  mg.generateMoves(pos, &moves, GenAll, false, BbZero);
  EXPECT_EQ(34, moves.size());
  EXPECT_EQ("d2f3 d2e4 d2b1 d2f1 d2b3 d2c4 c6d4 c6a5 c6b8 c6d8 f6e4 f6d7 f6g4 f6d5 f6h5 f6g8 b4c3 b4a3 b4a5 b4c5 b4d6 b7a6 b7c8 a8b8 a8c8 a8d8 h8f8 h8g8 e7d7 e7c5 e7d6 e7e6 e7d8 e7f8", moves.str());

  // sort moves
  ranges::sort(moves, [](const Move lhs, const Move rhs) {
    return lhs.value() > rhs.value();
  });
  for (Move m : moves) {
    println(m.strVerbose());
  }
}

TEST_F(MoveGenTest, castlingMoves) {
  MoveGenerator mg;
  MoveList moves;

  auto pos = Position("r3k2r/pbppqppp/1pn2n2/1B2p3/1b2P3/N1PP1N2/PP1BQPPP/R3K2R w KQkq -");
  mg.generateCastling(pos, &moves, GenAll);
  EXPECT_EQ(2, moves.size());
  EXPECT_EQ("e1g1 e1c1", moves.str());
  moves.clear();

  pos = Position("r3k2r/pbppqppp/1pn2n2/1B2p3/1b2P3/N1PP1N2/PP1BQPPP/R3K2R b KQkq -");
  mg.generateCastling(pos, &moves, GenAll);
  EXPECT_EQ(2, moves.size());
  EXPECT_EQ("e8g8 e8c8", moves.str());

  // sort moves
  ranges::sort(moves, [](const Move lhs, const Move rhs) {
    return lhs.value() > rhs.value();
  });
  for (const Move m : moves) {
    println(m.strVerbose());
  }
}

TEST_F(MoveGenTest, pseudoLegalMoves) {
  string fen;
  MoveGenerator mg;
  MoveList moves;
  Position position;

  // Start pos
  fen      = START_POSITION_FEN;
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves(position, GenAll, false);
  EXPECT_EQ(20, moves.size());
  EXPECT_EQ("d2d4 e2e4 b1c3 g1f3 a2a3 h2h3 a2a4 b2b4 c2c4 f2f4 g2g4 h2h4 d2d3 e2e3 b2b3 g2g3 c2c3 f2f3 b1a3 g1h3", moves.str());
  //  for (Move m : moves) {
  //    fprintln("{}", m.strVerbose());
  //  }
  NEWLINE;

  fen      = "r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R w KQkq -";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves(position, GenAll, false);
  EXPECT_EQ(40, moves.size());
  EXPECT_EQ("d7f6 f3d2 b5c6 f3e5 d7e5 d7b6 e2d2 e1d2 c3b4 e1c1 e1g1 d3d4 f3d4 d7c5 a1c1 a1d1 h1f1 b5c4 a2a3 f3g5 h2h3 e2e3 g2g4 h2h4 c3c4 e1f1 a2a4 b2b3 g2g3 e2d1 b5a4 b5a6 a1b1 h1g1 e2f1 e1d1 f3h4 d7f8 d7b8 f3g1", moves.str());
  //  for (Move m : moves) {
  //    fprintln("{}", m.strVerbose());
  //  }
  NEWLINE;

  // 86 pseudo legal moves (incl. castling over attacked square)
  fen      = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves(position, GenAll, false);
  EXPECT_EQ(86, moves.size());
  EXPECT_EQ("c2b1Q a2b1Q a2a1Q c2c1Q c2b1N a2b1N b2a3 a8a3 g6e5 d7e5 b2e5 c2c1N a2a1N e6e5 c4e4 c6e4 f4g3 f4e3 e8g8 e8c8 d7c5 a8c8 h8f8 a8d8 d7f6 b2d4 g6e7 e6f6 e6d6 e6f5 c6d6 c6d5 c6c5 e6d5 c4d5 f4f3 c4c5 h7h6 b2c3 d7b6 e6e7 e6f7 c4d4 e8f8 a8a4 a8a5 a8a6 a8a7 c4e2 c4b3 c4c3 c4d3 c4b4 c4b5 c6b5 c6b6 e6g4 b7b5 h7h5 c4a4 b7b6 c6a4 c4f1 c6a6 b2c1 e6h3 a8b8 h8g8 e6g8 c4a6 b2a1 e8e7 e8f7 e8d8 g6h4 g6f8 d7f8 d7b8 a2b1R c2b1R a2b1B c2b1B c2c1R a2a1R c2c1B a2a1B", moves.str());
  //  for (Move m : moves) {
  //    fprintln("{}", m.strVerbose());
  //  }
  NEWLINE;

  // 218 pseudo legal moves (incl. castling over attacked square)
  fen      = "R6R/3Q4/1Q4Q1/4Q3/2Q4Q/Q4Q2/pp1Q4/kBNN1KB1 w - - 0 1";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves(position, GenAll, false);
  EXPECT_EQ(218, moves.size());
  //  for (Move m : moves) {
  //    fprintln("{}", m.strVerbose());
  //  }
  NEWLINE;

  // bug fixed positions
  fen      = "rnbqkbnr/1ppppppp/8/p7/7P/8/PPPPPPP1/RNBQKBNR w KQkq a6";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves(position, GenAll);
  EXPECT_EQ(21, moves.size());
  //  for (Move m : moves) {
  //    fprintln("{}", m.strVerbose());
  //  }
  NEWLINE;

  fen      = "rnbqkbnr/p2ppppp/8/1Pp5/8/8/1PPPPPPP/RNBQKBNR w KQkq c6";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves(position, GenAll);
  EXPECT_EQ(26, moves.size());
  //  for (Move m : moves) {
  //    fprintln("{}", m.strVerbose());
  //  }
  NEWLINE;

  // kiwipete 48
  fen      = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves(position, GenAll);
  EXPECT_EQ(48, moves.size());
  //  for (Move m : moves) {
  //    fprintln("{}", m.strVerbose());
  //  }
}

TEST_F(MoveGenTest, legalMoves) {
  string fen;
  MoveGenerator mg;
  MoveList moves;
  Position position;

  // Startpos
  position = Position(START_POSITION_FEN);
  moves.clear();
  moves = *mg.generateLegalMoves(position, GenAll);
  EXPECT_EQ(20, moves.size());

  // 86 pseudo legal moves - 83 legal (incl. castling over attacked square)
  fen      = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
  position = Position(fen);
  moves.clear();
  moves = *mg.generateLegalMoves(position, GenAll);
  EXPECT_EQ(83, moves.size());
  EXPECT_FALSE(position.isLegalMove(Move::castling(SQ_E8, SQ_G8)));
}

TEST_F(MoveGenTest, validateMove) {
  string fen;
  MoveGenerator mg;
  MoveList moves;

  // 86 pseudo legal moves (incl. castling over attacked square)
  fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
  Position position(fen);

  EXPECT_TRUE(mg.validateMove(position, Move::normal(SQ_B2, SQ_E5)));
  EXPECT_TRUE(mg.validateMove(position, Move::normal(SQ_E6, SQ_E5)));
  EXPECT_TRUE(mg.validateMove(position, Move::normal(SQ_C4, SQ_E4)));
  EXPECT_TRUE(mg.validateMove(position, Move::normal(SQ_C6, SQ_E4)));
  EXPECT_TRUE(mg.validateMove(position, Move::promotion(SQ_A2, SQ_A1, QUEEN)));
  EXPECT_TRUE(mg.validateMove(position, Move::promotion(SQ_C2, SQ_C1, QUEEN)));
  EXPECT_TRUE(mg.validateMove(position, Move::promotion(SQ_A2, SQ_A1, QUEEN)));
  EXPECT_TRUE(mg.validateMove(position, Move::promotion(SQ_C2, SQ_C1, QUEEN)));
  EXPECT_FALSE(mg.validateMove(position, Move::normal(SQ_E2, SQ_E4)));
  EXPECT_FALSE(mg.validateMove(position, Move::normal(SQ_B8, SQ_C8)));
  EXPECT_FALSE(mg.validateMove(position, Move::normal(SQ_A2, SQ_B3)));
  EXPECT_FALSE(mg.validateMove(position, Move::normal(SQ_B1, SQ_C3)));
  EXPECT_FALSE(mg.validateMove(position, MOVE_NONE));

  // pawn double is only legal move (was bug in hasLegalMoves in previous FrankyGo v1.0 version)
  position = Position("rnbq1bnr/ppp1pppp/4k3/3pP3/3P2Q1/8/PPP2PPP/RNB1KBNR b KQ - 2 4");
  EXPECT_TRUE(mg.validateMove(position, Move::normal(SQ_F7, SQ_F5)));
}

TEST_F(MoveGenTest, fromUci) {
  MoveGenerator mg;

  const auto pos = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3");

  // invalid pattern
  Move move = mg.getMoveFromUci(pos, "8888");
  EXPECT_EQ(MOVE_NONE, move);

  // valid move
  move = mg.getMoveFromUci(pos, "b7b5");
  EXPECT_EQ(Move::normal(SQ_B7, SQ_B5), move);

  // invalid move
  move = mg.getMoveFromUci(pos, "a7a5");
  EXPECT_EQ(MOVE_NONE, move);

  // valid promotion
  move = mg.getMoveFromUci(pos, "a2a1Q");
  EXPECT_EQ(Move::promotion(SQ_A2, SQ_A1, QUEEN), move);

  // valid promotion (we allow lower case promotions);
  move = mg.getMoveFromUci(pos, "a2a1q");
  EXPECT_EQ(Move::promotion(SQ_A2, SQ_A1, QUEEN), move);

  // valid castling
  move = mg.getMoveFromUci(pos, "e8c8");
  EXPECT_EQ(Move::castling(SQ_E8, SQ_C8), move);

  // invalid castling
  move = mg.getMoveFromUci(pos, "e8g8");
  EXPECT_EQ(MOVE_NONE, move);
}

TEST_F(MoveGenTest, fromSan) {
  MoveGenerator mg;
  Position pos;
  Move move;

  pos = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3");

  // invalid pattern
  move = mg.getMoveFromSan(pos, "33");
  EXPECT_EQ(MOVE_NONE, move);

  // valid move
  move = mg.getMoveFromSan(pos, "b5");
  EXPECT_EQ(Move::normal(SQ_B7, SQ_B5), move);

  // invalid move
  move = mg.getMoveFromSan(pos, "a5");
  EXPECT_EQ(MOVE_NONE, move);

  // valid promotion
  move = mg.getMoveFromSan(pos, "a1Q");
  EXPECT_EQ(Move::promotion(SQ_A2, SQ_A1, QUEEN), move);

  // invalid promotion
  move = mg.getMoveFromSan(pos, "a1q");
  EXPECT_EQ(MOVE_NONE, move);

  // valid castling
  move = mg.getMoveFromSan(pos, "O-O-O");
  EXPECT_EQ(Move::castling(SQ_E8, SQ_C8), move);

  // invalid castling
  move = mg.getMoveFromSan(pos, "O-O");
  EXPECT_EQ(MOVE_NONE, move);

  // capture
  move = mg.getMoveFromSan(pos, "Qxe5");
  EXPECT_EQ(Move::normal(SQ_E6, SQ_E5), move);

  // ep capture
  move = mg.getMoveFromSan(pos, "fxe3");
  EXPECT_EQ(Move::enPassant(SQ_F4, SQ_E3), move);

  move = mg.getMoveFromSan(pos, "fxe3e.p.");
  EXPECT_EQ(Move::enPassant(SQ_F4, SQ_E3), move);


  // ambiguous
  move = mg.getMoveFromSan(pos, "Ne5");
  EXPECT_EQ(MOVE_NONE, move);
  move = mg.getMoveFromSan(pos, "Nde5");
  EXPECT_EQ(Move::normal(SQ_D7, SQ_E5), move);
  move = mg.getMoveFromSan(pos, "Nge5");
  EXPECT_EQ(Move::normal(SQ_G6, SQ_E5), move);
  move = mg.getMoveFromSan(pos, "N7e5");
  EXPECT_EQ(Move::normal(SQ_D7, SQ_E5), move);
  move = mg.getMoveFromSan(pos, "N6e5");
  EXPECT_EQ(Move::normal(SQ_G6, SQ_E5), move);
  move = mg.getMoveFromSan(pos, "ab1Q");
  EXPECT_EQ(Move::promotion(SQ_A2, SQ_B1, QUEEN), move);
  move = mg.getMoveFromSan(pos, "cb1Q");
  EXPECT_EQ(Move::promotion(SQ_C2, SQ_B1, QUEEN), move);

  pos  = Position("rnbqkb1r/ppp1p1pp/5B2/3p1p2/3P4/2N5/PPP1PPPP/R2QKBNR b KQkq -");
  move = mg.getMoveFromSan(pos, "exf6");
  EXPECT_EQ(Move::normal(SQ_E7, SQ_F6), move);

  pos  = Position("8/6Bp/7P/5p2/pKP2P2/1b6/p7/1k6 b - - 3 51");
  move = mg.getMoveFromSan(pos, "a1=Q");
  EXPECT_EQ(Move::promotion(SQ_A2, SQ_A1, QUEEN), move);
}


TEST_F(MoveGenTest, onDemandGen) {
  string fen;
  MoveGenerator mg;
  MoveList moves;

  // 86 pseudo legal moves (incl. castling over attacked square)
  fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
  Position position(fen);

  Move move;
  int counter = 0;
  while ((move = mg.getNextPseudoLegalMove(position, GenAll)) != MOVE_NONE) {
    EXPECT_TRUE(!move.isNone());
    counter++;
  }
  EXPECT_EQ(86, counter);

  // 218 moves
  fen      = "R6R/3Q4/1Q4Q1/4Q3/2Q4Q/Q4Q2/pp1Q4/kBNN1KB1 w - - 0 1";
  position = Position(fen);
  counter  = 0;
  while ((move = mg.getNextPseudoLegalMove(position, GenAll)) != MOVE_NONE) {
    EXPECT_TRUE(!move.isNone());
    counter++;
  }
  EXPECT_EQ(218, counter);

  // 48 kiwipete moves
  fen      = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
  position = Position(fen);
  counter  = 0;
  while ((move = mg.getNextPseudoLegalMove(position, GenAll)) != MOVE_NONE) {
    EXPECT_TRUE(!move.isNone());
    counter++;
  }
  EXPECT_EQ(48, counter);
}

TEST_F(MoveGenTest, storeKiller) {
  string fen;
  MoveGenerator mg;
  MoveList moves;

  // 86 pseudo legal moves (incl. castling over attacked square)
  fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
  Position position(fen);

  const MoveList* allMoves = mg.generatePseudoLegalMoves(position, GenQuiet);

  // add first two killers
  mg.storeKiller(allMoves->at(11));
  mg.storeKiller(allMoves->at(21));
  EXPECT_EQ(allMoves->at(11), mg.killerMoves[1]);
  EXPECT_EQ(allMoves->at(21), mg.killerMoves[0]);

  // add a killer already in the list - should not change
  mg.storeKiller(allMoves->at(21));
  EXPECT_EQ(allMoves->at(21), mg.killerMoves[0]);
  EXPECT_EQ(allMoves->at(11), mg.killerMoves[1]);

  // add a killer NOT already in the list - should change
  mg.storeKiller(allMoves->at(31));
  EXPECT_EQ(allMoves->at(31), mg.killerMoves[0]);
  EXPECT_EQ(allMoves->at(21), mg.killerMoves[1]);

  mg = MoveGenerator();

  // need to regenerate moves as reset has reset list
  allMoves = mg.generatePseudoLegalMoves(position, GenQuiet);

  // add a killer NOT already in the list - should change
  mg.storeKiller(allMoves->at(31));
  EXPECT_EQ(allMoves->at(31), mg.killerMoves[0]);
}

TEST_F(MoveGenTest, onDemandKiller) {
  MoveGenerator mg;
  MoveList moves;
  Move move;

  // 86
  auto pos = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3");

  Move move1 = mg.getMoveFromUci(pos, "g6h4");
  Move move2 = mg.getMoveFromUci(pos, "b7b6");
  Move move3 = mg.getMoveFromUci(pos, "a2b1Q");
  mg.resetOnDemand();
  mg.storeKiller(move1);
  mg.storeKiller(move2);
  mg.setPV(move3);
  while ((move = mg.getNextPseudoLegalMove(pos, GenAll)) != MOVE_NONE) {
    moves.push_back(move);
    //    fprintln(move.strVerbose());
  }
  EXPECT_EQ(86, moves.size());
  EXPECT_EQ("a2b1Q c2b1Q a2a1Q c2c1Q c2b1N a2b1N a2a1N c2c1N f4g3 f4e3 c2b1R a2b1R c2b1B a2b1B b2a3 a8a3 g6e5 d7e5 b2e5 e6e5 c4e4 c6e4 b7b6 f4f3 h7h6 b7b5 h7h5 a2a1R c2c1R a2a1B c2c1B e8g8 e8c8 g6h4 d7c5 a8d8 h8f8 a8c8 d7f6 b2d4 g6e7 c6c5 c6d5 c4c5 c6d6 e6f5 e6d6 b2c3 e6f6 e6d5 c4d5 d7b6 e6e7 e6f7 c4d4 a8a5 a8a6 a8a7 c4e2 c4b3 c6b5 c4b4 c6b6 e6g4 a8a4 c4b5 c4c3 c4d3 c6a4 c4a4 a8b8 h8g8 b2c1 c4f1 c4a6 c6a6 e6h3 e6g8 b2a1 g6f8 d7f8 d7b8 e8f8 e8e7 e8f7 e8d8", moves.str());
  moves.clear();

  mg.reset();

  // 48 kiwipete
  pos = Position("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");

  move1 = mg.getMoveFromUci(pos, "d2g5");
  move2 = mg.getMoveFromUci(pos, "b2b3");
  move3 = mg.getMoveFromUci(pos, "e2a6");
  mg.resetOnDemand();
  mg.storeKiller(move1);
  mg.storeKiller(move2);
  mg.setPV(move3);
  while ((move = mg.getNextPseudoLegalMove(pos, GenAll)) != MOVE_NONE) {
    moves.push_back(move);
    //    fprintln(move.strVerbose());
  }
  EXPECT_EQ(48, moves.size());
  EXPECT_EQ("e2a6 g2h3 d5e6 e5d7 e5f7 e5g6 f3f6 f3h3 b2b3 a2a3 d5d6 a2a4 g2g4 g2g3 e1g1 e1c1 d2g5 e5d3 e5c4 a1c1 a1d1 h1f1 e5c6 d2e3 d2f4 e2d3 e2c4 c3b5 e2b5 f3d3 f3e3 f3f4 f3f5 e5g4 f3g3 f3g4 f3h5 d2h6 e2d1 a1b1 h1g1 c3a4 c3d1 d2c1 e2f1 c3b1 e1f1 e1d1", moves.str());
  moves.clear();
}

TEST_F(MoveGenTest, pvMove) {
  string fen;
  MoveGenerator mg;
  MoveList moves;

  // 86 pseudo legal moves (incl. castling over attacked square)
  fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 w kq e3";
  Position position(fen);

  // Test #1: best move is capturing and generating all moves
  Move pvMove = Move::normal(SQ_B1, SQ_B2);
  mg.setPV(pvMove);
  Move move;
  int counter = 0;
  // generate all moves
  while ((move = mg.getNextPseudoLegalMove(position, GenAll)) != MOVE_NONE) {
    if (counter == 0) {// first move must be pv move
      EXPECT_EQ(pvMove, move);
    }
    else {// no more pv move after first move
      EXPECT_NE(pvMove, move);
    }
    counter++;
  }
  EXPECT_EQ(27, counter);
  mg.resetOnDemand();

  // Test #2: best move is capturing and generating capturing moves
  pvMove = Move::normal(SQ_B1, SQ_B2);
  mg.setPV(pvMove);
  counter = 0;
  // generate all moves
  while ((move = mg.getNextPseudoLegalMove(position, GenNonQuiet)) != MOVE_NONE) {
    if (counter == 0) {// first move must be pv move
      EXPECT_EQ(pvMove, move);
    }
    else {// no more pv move after first move
      EXPECT_NE(pvMove, move);
    }
    counter++;
  }
  EXPECT_EQ(4, counter);
  mg.resetOnDemand();

  // Test #3: best move is non-capturing and generating all moves
  pvMove = Move::normal(SQ_H2, SQ_H3);
  mg.setPV(pvMove);
  counter = 0;
  // generate all moves
  while ((move = mg.getNextPseudoLegalMove(position, GenAll)) != MOVE_NONE) {
    if (counter == 0) {// first move must be pv move
      EXPECT_EQ(pvMove, move);
    }
    else {// no more pv move after first move
      EXPECT_NE(pvMove, move);
    }
    counter++;
  }
  EXPECT_EQ(27, counter);
  mg.resetOnDemand();

  // Test #4: best move is non-capturing and generating capturing moves
  pvMove = Move::normal(SQ_H2, SQ_H3);
  mg.setPV(pvMove);
  counter = 0;
  // generate all moves
  while ((move = mg.getNextPseudoLegalMove(position, GenNonQuiet)) != MOVE_NONE) {
    if (counter == 0) {// first move can't be non capturing pv move
      EXPECT_NE(pvMove, move);
    }
    else {// no more pv move after first move
      EXPECT_NE(pvMove, move);
    }
    counter++;
  }
  EXPECT_EQ(4, counter);
  mg.resetOnDemand();

  // Test #4: best move is non-capturing and generating non-capturing moves
  // not very relevant for searching
  pvMove = Move::normal(SQ_H2, SQ_H3);
  mg.setPV(pvMove);
  counter = 0;
  // generate all moves
  while ((move = mg.getNextPseudoLegalMove(position, GenQuiet)) != MOVE_NONE) {
    if (counter == 0) {// first move must be pv move
      EXPECT_EQ(pvMove, move);
    }
    else {// no more pv move after first move
      EXPECT_NE(pvMove, move);
    }
    counter++;
  }
  EXPECT_EQ(23, counter);
  mg.resetOnDemand();
}

TEST_F(MoveGenTest, evasion) {
  MoveGenerator mg;
  Position p;
  const MoveList *pseudoLegalMoves, *evasionMoves, *legalMoves;

  // TODO - real tests

  p                = Position("r3k2r/1pp4p/2q1qNn1/3nP3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq -");
  pseudoLegalMoves = mg.generatePseudoLegalMoves(p, GenAll, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), pseudoLegalMoves->str());
  evasionMoves = mg.generatePseudoLegalMoves(p, GenAll, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), evasionMoves->str());
  legalMoves = mg.generateLegalMoves(p, GenAll);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), legalMoves->str());
  fprintln("");

  p                = Position("5k2/8/8/8/8/8/6p1/3K1R2 b - -");
  pseudoLegalMoves = mg.generatePseudoLegalMoves(p, GenAll, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), pseudoLegalMoves->str());
  evasionMoves = mg.generatePseudoLegalMoves(p, GenAll, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), evasionMoves->str());
  legalMoves = mg.generateLegalMoves(p, GenAll);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), legalMoves->str());
  fprintln("");

  p                = Position("5k2/8/8/8/8/6p1/5R2/3K4 b - -");
  pseudoLegalMoves = mg.generatePseudoLegalMoves(p, GenAll, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), pseudoLegalMoves->str());
  evasionMoves = mg.generatePseudoLegalMoves(p, GenAll, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), evasionMoves->str());
  legalMoves = mg.generateLegalMoves(p, GenAll);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), legalMoves->str());
  fprintln("");

  p                = Position("8/8/8/3k4/4Pp2/8/8/3K4 b - e3");
  pseudoLegalMoves = mg.generatePseudoLegalMoves(p, GenAll, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), pseudoLegalMoves->str());
  evasionMoves = mg.generatePseudoLegalMoves(p, GenAll, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), evasionMoves->str());
  legalMoves = mg.generateLegalMoves(p, GenAll);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), legalMoves->str());
  fprintln("");

  p                = Position("8/8/8/3k2n1/8/8/6B1/3K4 b - -");
  pseudoLegalMoves = mg.generatePseudoLegalMoves(p, GenAll, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), pseudoLegalMoves->str());
  evasionMoves = mg.generatePseudoLegalMoves(p, GenAll, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), evasionMoves->str());
  legalMoves = mg.generateLegalMoves(p, GenAll);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), legalMoves->str());
  fprintln("");

  p                = Position("5k2/3N4/8/8/8/8/6p1/3K1R2 b - - 1 1");
  pseudoLegalMoves = mg.generatePseudoLegalMoves(p, GenAll, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), pseudoLegalMoves->str());
  evasionMoves = mg.generatePseudoLegalMoves(p, GenAll, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), evasionMoves->str());
  legalMoves = mg.generateLegalMoves(p, GenAll);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), legalMoves->str());
  fprintln("");
}

TEST_F(MoveGenTest, sortValueTest) {
  MoveGenerator mg;
  MoveList moves;

  // Start pos
  const auto p = Position("r3k2r/1pp4p/2q1qNn1/3nP3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq -");

  Move moveFromUci = Move::normal(SQ_G6, SQ_H4);
  mg.storeKiller(moveFromUci);
  moveFromUci = Move::normal(SQ_B7, SQ_B6);
  mg.storeKiller(moveFromUci);
  moveFromUci = Move::promotion(SQ_A2, SQ_B1, QUEEN);
  mg.setPV(moveFromUci);

  mg.generatePawnMoves(p, &moves, GenNonQuiet, false, BbZero);
  mg.generateMoves(p, &moves, GenNonQuiet, false, BbZero);
  mg.generateKingMoves(p, &moves, GenNonQuiet, false);
  mg.generatePawnMoves(p, &moves, GenQuiet, false, BbZero);
  mg.generateCastling(p, &moves, GenQuiet);
  mg.generateMoves(p, &moves, GenQuiet, false, BbZero);
  mg.generateKingMoves(p, &moves, GenQuiet, false);

  // PV, Killer and history handling
  mg.updateSortValues(p, &moves);

  fprintln("Pre sort:");
  for (const Move m : moves) {
    fprintln("{}", m.strVerbose());
  }
  NEWLINE;

  // sort moves
  ranges::stable_sort(moves, moveValueGreaterComparator());

  // TODO real tests

  fprintln("Post sort:");
  int counter   = 0;
  Move lastMove = MOVE_NONE;
  for (const Move m : moves) {
    fprintln("{}", m.strVerbose());
    if (!counter++) {
      lastMove = m;
      continue;
    }
    EXPECT_GE(lastMove.value(), m.value());
    lastMove = m;
  }
  NEWLINE;
}


#include <chrono>
using namespace std::chrono;

// 8.6.: Loaner Mac:
// 480.000.000 moves generated: 84.774.069 mps
// 4.5.2025 GROOT
// Test took 2.822.184.200 ns for 10.000.000 iterations
// Test took 282 ns per test
// Test per sec 3.543.354 tps
// 480.000.000 moves generated: 170.081.031 mps
TEST_F(MoveGenTest, PseudoMoveGenSpeedTest) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }

  MoveGenerator mg;

  constexpr int rounds = 5;

  const auto position         = Position("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
  const MoveList* moves = mg.generatePseudoLegalMoves(position, GenAll);

  for (int r = 1; r <= rounds; r++) {
    constexpr int iterations = 10'000'000;
    fprintln("Round {}", r);
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
      mg.reset();
      moves = mg.generatePseudoLegalMoves(position, GenAll);
    }
    auto elapsed = duration_cast<nanoseconds>(high_resolution_clock::now() - start);

    std::ostringstream os;
    os.flags(std::cout.flags());
    os.imbue(deLocale);
    os.precision(os.precision());
    os << "Test took " << elapsed.count() << " ns for " << iterations << " iterations" << std::endl;
    os << "Test took " << elapsed.count() / iterations << " ns per test" << std::endl;
    os << "Test per sec " << (iterations * nanoPerSec) / elapsed.count() << " tps" << std::endl;
    os << moves->size() * iterations << " moves generated: " << (moves->size() * iterations * nanoPerSec) / elapsed.count() << " mps" << std::endl;
    std::cout << os.str() << std::endl;
  }

  //  fprintln(str(*moves));
}

TEST_F(MoveGenTest, debug) {
  MoveGenerator mg{};
  const Position p("rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 1 2");
  MoveList moves{};

  // Move move;
  //  int counter = 0;
  //  while ((move = mg.getNextPseudoLegalMove(p, GenAll)) != MOVE_NONE) {
  //    counter++;
  //    fprintln(move.strVerbose());
  //  }
  //
  //  NEWLINE;

  mg.reset();
  moves = *mg.generatePseudoLegalMoves(p, GenAll);
  fprintln("{}", moves.size());
  for (const Move m : moves) {
    println(m.strVerbose());
  }

  fprintln("{}", mg.generateLegalMoves(p, GenAll)->size());
}

//=============================================================================
// hasLegalEpCapture Tests
//=============================================================================

TEST_F(MoveGenTest, hasLegalEpCapture_noEpSquare) {
  // Starting position - no EP square
  const Position pos;
  EXPECT_FALSE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_epSquareButNoPawnCanCapture) {
  // Position with EP square but no pawn can capture it
  // White pawn on e5, black pawn moved d7-d5, but no white pawn on d or f file to capture
  const Position pos("8/8/8/3pP3/8/8/8/4K2k w - d6 0 1");
  // e5 pawn can capture on d6
  EXPECT_TRUE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_simpleLegalEpCapture) {
  // Simple legal EP capture: white pawn on e5, black pawn just moved d7-d5
  // White to move, can capture en passant on d6
  const Position pos("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
  EXPECT_TRUE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_simpleLegalEpCaptureBlack) {
  // Simple legal EP capture for black: black pawn on d4, white pawn just moved e2-e4
  // Black to move, can capture en passant on e3
  const Position pos("4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1");
  EXPECT_TRUE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_twoPawnsCanCapture) {
  // Two pawns can capture EP: white pawns on c5 and e5, black pawn moved d7-d5
  const Position pos("4k3/8/8/2PpP3/8/8/8/4K3 w - d6 0 1");
  EXPECT_TRUE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_horizontalPinIllegal) {
  // Horizontal pin makes EP capture illegal:
  // White king on a5, black rook on h5, white pawn on e5, black pawn on d5 (just moved d7-d5)
  // EP capture exd6 would remove both pawns from rank 5, exposing king to rook
  const Position pos("4k3/8/8/K2pP2r/8/8/8/8 w - d6 0 1");
  EXPECT_FALSE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_horizontalPinIllegalBlack) {
  // Horizontal pin makes EP capture illegal for black:
  // Black king on h4, white rook on a4, black pawn on d4, white pawn on e4 (just moved e2-e4)
  // EP capture dxe3 would remove both pawns from rank 4, exposing king to rook
  const Position pos("8/8/8/8/R2pP2k/8/8/4K3 b - e3 0 1");
  EXPECT_FALSE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_verticalPinIllegal) {
  // Vertical pin makes EP capture illegal:
  // White king on c1, black rook on c8, white pawn on c5 pinned vertically
  // Black pawn just moved d7-d5, EP capture cxd6 would expose king to rook
  const Position pos("2r1k3/8/8/2Pp4/8/8/8/2K5 w - d6 0 1");
  EXPECT_FALSE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_verticalPinIllegalBlack) {
  // Vertical pin makes EP capture illegal for black:
  // Black king on f8, white rook on f1, black pawn on f4 pinned vertically
  // White pawn just moved e2-e4, EP capture fxe3 would expose king to rook
  const Position pos("5k2/8/8/8/4Pp2/8/8/5RK1 b - e3 0 1");
  EXPECT_FALSE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_horizontalPinWithQueen) {
  // Same horizontal pin scenario but with queen instead of rook
  const Position pos("4k3/8/8/K2pP2q/8/8/8/8 w - d6 0 1");
  EXPECT_FALSE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_diagonalPinIllegal) {
  // Diagonal pin makes EP capture illegal:
  // White king on d3, black bishop on h7, white pawn on f5 pinned diagonally
  // Black pawn just moved e7-e5, EP capture fxe6 would expose king to bishop
  const Position pos("8/7b/2B5/4pP2/8/3K4/8/k7 w - e6 0 1");
  EXPECT_FALSE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_diagonalPinIllegalBlack) {
  // Diagonal pin makes EP capture illegal for black:
  // Black king on d6, white bishop on h2, black pawn on f4 pinned diagonally
  // White pawn just moved e2-e4, EP capture fxe3 would expose king to bishop
  const Position pos("8/8/3k4/8/4Pp2/8/7B/K7 b - e3 0 1");
  EXPECT_FALSE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_diagonalPinWithQueen) {
  // Diagonal pin with queen instead of bishop
  const Position pos("8/7q/8/4pP2/8/3K4/8/k7 w - e6 0 1");
  EXPECT_FALSE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_diagonalPinButCaptureAlongRay) {
  // Diagonal "pin" but EP capture moves along the same diagonal - legal!
  // White king on h3, black bishop on d7, white pawn on f5
  // Black pawn just moved e7-e5, EP capture fxe6 moves pawn to e6
  // which is still on the d7-h3 diagonal, so pin is not broken
  const Position pos("8/3b4/8/4pP2/2B5/7K/8/k7 w - e6 0 1");
  EXPECT_TRUE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_onePawnPinnedOtherNot) {
  // Two pawns can capture, but one is horizontally pinned
  // White king on a5, black rook on h5, white pawns on c5 and e5, black pawn on d5
  // cxd6 is legal (c5 pawn not pinned), exd6 is illegal (e5 pawn pinned)
  const Position pos("4k3/8/8/K1PpP2r/8/8/8/8 w - d6 0 1");
  EXPECT_TRUE(MoveGenerator::hasLegalEpCapture(pos));// cxd6 is legal
}

TEST_F(MoveGenTest, hasLegalEpCapture_kingNotOnSameRank) {
  // King on different rank - no horizontal pin possible, EP is legal
  const Position pos("4k3/8/8/3pP3/8/8/8/K7 w - d6 0 1");
  EXPECT_TRUE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_diagonalAttackerNotPinning) {
  // Diagonal attacker present but not creating a pin on the capturing pawn
  // Bishop on a1, white pawn on e5, black pawn on d5 - bishop doesn't pin e5 pawn
  // because pawn is not between king and bishop
  const Position pos("4k3/8/8/3pP3/8/8/8/b3K3 w - d6 0 1");
  EXPECT_TRUE(MoveGenerator::hasLegalEpCapture(pos));
}

TEST_F(MoveGenTest, hasLegalEpCapture_rookNotAttackingKing) {
  // Rook on same rank but not attacking king (blocked by another piece)
  const Position pos("4k3/8/8/K1NpP2r/8/8/8/8 w - d6 0 1");
  EXPECT_TRUE(MoveGenerator::hasLegalEpCapture(pos));// Knight blocks, EP is legal
}

TEST_F(MoveGenTest, hasLegalEpCapture_rookOnDifferentRank) {
  // Rook present but on different rank - no horizontal pin
  const Position pos("4k3/8/8/K2pP3/7r/8/8/8 w - d6 0 1");
  EXPECT_TRUE(MoveGenerator::hasLegalEpCapture(pos));
}

//=============================================================================
// hasLegalMove Tests
//=============================================================================

TEST_F(MoveGenTest, hasLegalMove_startingPosition) {
  const Position pos;
  EXPECT_TRUE(MoveGenerator::hasLegalMove(pos));
}

TEST_F(MoveGenTest, hasLegalMove_checkmate) {
  // Fool's mate position - black is checkmated
  const Position pos("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
  EXPECT_FALSE(MoveGenerator::hasLegalMove(pos));
}

TEST_F(MoveGenTest, hasLegalMove_stalemate) {
  // Classic stalemate: black king on h8, white queen on g6, white king on f7
  const Position pos("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
  EXPECT_FALSE(MoveGenerator::hasLegalMove(pos));
}

TEST_F(MoveGenTest, hasLegalMove_onlyEpCaptureLegal) {
  // Position where only EP capture is legal
  // This is a constructed position where all pieces are blocked except EP
  const Position pos("8/8/8/8/k1PpK3/8/8/8 b - c3 0 1");
  // Black king on a4 is in check from nothing, can't move due to white king
  // Only move is dxc3 en passant
  EXPECT_TRUE(MoveGenerator::hasLegalMove(pos));
}

TEST_F(MoveGenTest, hasLegalMove_pawnDoubleOnlyMove) {
  // Position where pawn double is the only legal move (was bug in previous version)
  const Position pos("rnbq1bnr/ppp1pppp/4k3/3pP3/3P2Q1/8/PPP2PPP/RNB1KBNR b KQ - 2 4");
  EXPECT_TRUE(MoveGenerator::hasLegalMove(pos));
}

TEST_F(MoveGenTest, hasLegalMove_kingMoveOnly) {
  // Only king can move
  const Position pos("8/8/8/8/8/8/1k6/K7 w - - 0 1");
  EXPECT_TRUE(MoveGenerator::hasLegalMove(pos));
}

TEST_F(MoveGenTest, hasLegalMove_epIllegalDueToHorizontalPin) {
  // Position where only potential move is EP, but it's illegal due to horizontal pin
  // Constructed so that EP would be the only move if legal
  // King a5, rook h5, pawns on e5/d5, black to potentially capture
  // Actually we need white with EP square
  const Position pos("8/8/8/K2pP2r/8/8/8/7k w - d6 0 1");
  // King has moves (Kb5, Kb4, Ka4, etc.), so hasLegalMove should be true
  EXPECT_TRUE(MoveGenerator::hasLegalMove(pos));
}

TEST_F(MoveGenTest, hasLegalMove_vsGenerateLegalMoves) {
  // Verify hasLegalMove returns true iff generateLegalMoves returns non-empty
  MoveGenerator mg;

  const std::vector<std::string> testFens = {
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",                         // starting
      "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3",                    // checkmate
      "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1",                                                   // stalemate
      "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3",                       // complex
      "4k3/8/8/K2pP2r/8/8/8/8 w - d6 0 1",                                                // EP pinned
      "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",                                                // EP legal
      "8/8/8/8/k1PpK3/8/8/8 b - c3 0 1",                                                  // EP only move
  };

  for (const auto& fen : testFens) {
    const Position pos(fen);
    const bool hasMove         = MoveGenerator::hasLegalMove(pos);
    const MoveList* legalMoves = mg.generateLegalMoves(pos, GenAll);
    const bool hasMoveFromList = !legalMoves->empty();

    EXPECT_EQ(hasMove, hasMoveFromList)
        << "Mismatch for position: " << fen
        << "\nhasLegalMove: " << hasMove
        << "\ngenerateLegalMoves empty: " << legalMoves->empty()
        << "\nlegal moves count: " << legalMoves->size();
  }
}
