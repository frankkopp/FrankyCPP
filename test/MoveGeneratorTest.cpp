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

#include <gtest/gtest.h>
#include <ostream>
#include <string>

#include "MoveGenerator.h"
#include "Position.h"
#include "init.h"
#include "types/types.h"

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
  Position pos;
  MoveList moves;

  pos = Position("1kr3nr/pp1pP1P1/2p1p3/3P1p2/1n1bP3/2P5/PP3PPP/RNBQKBNR w KQ -");

  mg.generatePawnMoves<GenNonQuiet>(pos, &moves, false, BbZero);
  EXPECT_EQ(11, moves.size());

  moves.clear();
  mg.generatePawnMoves<GenQuiet>(pos, &moves, false, BbZero);
  EXPECT_EQ(14, moves.size());

  moves.clear();
  mg.generatePawnMoves<GenAll>(pos, &moves, false, BbZero);
  EXPECT_EQ(25, moves.size());

  // sort moves
  sort(moves.begin(), moves.end(), [](const Move lhs, const Move rhs) {
    return valueOf(lhs) > valueOf(rhs);
  });
  for (Move m : moves) {
    fprintln(strVerbose(m));
  }
}

TEST_F(MoveGenTest, kingMoves) {
  MoveGenerator mg;
  Position pos;
  MoveList moves;

  pos = Position("r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R w KQkq -");
  mg.generateKingMoves<GenAll>(pos, &moves, false);
  EXPECT_EQ(3, moves.size());
  EXPECT_EQ("e1d2 e1d1 e1f1", str(moves));
  moves.clear();

  pos = Position("r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R b KQkq -");
  mg.generateKingMoves<GenAll>(pos, &moves, false);
  EXPECT_EQ(3, moves.size());
  EXPECT_EQ("e8d7 e8d8 e8f8", str(moves));

  // sort moves
  sort(moves.begin(), moves.end(), [](const Move lhs, const Move rhs) {
    return valueOf(lhs) > valueOf(rhs);
  });
  for (Move m : moves) {
    fprintln(strVerbose(m));
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
  mg.generateMoves<GenNonQuiet>(pos, &moves, false, BbZero);
  EXPECT_EQ(7, moves.size());
  EXPECT_EQ("f3d2 f3e5 d7e5 d7b6 d7f6 b5c6 e2d2", str(moves));
  moves.clear();

  pos = Position("r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R b KQkq -");
  mg.generateMoves<GenQuiet>(pos, &moves, false, BbZero);
  EXPECT_EQ(28, moves.size());
  EXPECT_EQ("d2b1 d2f1 d2b3 d2c4 c6d4 c6a5 c6b8 c6d8 f6g4 f6d5 f6h5 f6g8 b4a3 b4a5 b4c5 b4d6 b7a6 b7c8 a8b8 a8c8 a8d8 h8f8 h8g8 e7c5 e7d6 e7e6 e7d8 e7f8", str(moves));
  moves.clear();

  pos = Position("r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R b KQkq -");
  mg.generateMoves<GenAll>(pos, &moves, false, BbZero);
  EXPECT_EQ(34, moves.size());
  EXPECT_EQ("d2f3 d2e4 d2b1 d2f1 d2b3 d2c4 c6d4 c6a5 c6b8 c6d8 f6e4 f6d7 f6g4 f6d5 f6h5 f6g8 b4c3 b4a3 b4a5 b4c5 b4d6 b7a6 b7c8 a8b8 a8c8 a8d8 h8f8 h8g8 e7d7 e7c5 e7d6 e7e6 e7d8 e7f8", str(moves));

  // sort moves
  sort(moves.begin(), moves.end(), [](const Move lhs, const Move rhs) {
    return valueOf(lhs) > valueOf(rhs);
  });
  for (Move m : moves) {
    fprintln(strVerbose(m));
  }
}

TEST_F(MoveGenTest, castlingMoves) {
  MoveGenerator mg;
  Position pos;
  MoveList moves;

  pos = Position("r3k2r/pbppqppp/1pn2n2/1B2p3/1b2P3/N1PP1N2/PP1BQPPP/R3K2R w KQkq -");
  mg.generateCastling<GenAll>(pos, &moves);
  EXPECT_EQ(2, moves.size());
  EXPECT_EQ("e1g1 e1c1", str(moves));
  moves.clear();

  pos = Position("r3k2r/pbppqppp/1pn2n2/1B2p3/1b2P3/N1PP1N2/PP1BQPPP/R3K2R b KQkq -");
  mg.generateCastling<GenAll>(pos, &moves);
  EXPECT_EQ(2, moves.size());
  EXPECT_EQ("e8g8 e8c8", str(moves));

  // sort moves
  sort(moves.begin(), moves.end(), [](const Move lhs, const Move rhs) {
    return valueOf(lhs) > valueOf(rhs);
  });
  for (Move m : moves) {
    fprintln(strVerbose(m));
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
  moves = *mg.generatePseudoLegalMoves<GenAll>(position, false);
  EXPECT_EQ(20, moves.size());
  EXPECT_EQ("d2d4 e2e4 b1c3 g1f3 a2a3 h2h3 a2a4 b2b4 c2c4 f2f4 g2g4 h2h4 d2d3 e2e3 b2b3 g2g3 c2c3 f2f3 b1a3 g1h3", str(moves));
  for (Move m : moves) {
    fprintln("{}", strVerbose(m));
  }
  NEWLINE;

  fen      = "r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R w KQkq -";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves<GenAll>(position, false);
  EXPECT_EQ(40, moves.size());
  EXPECT_EQ("c3b4 d7f6 f3d2 b5c6 f3e5 d7e5 d7b6 e2d2 e1d2 e1g1 e1c1 d3d4 f3d4 h1f1 a1d1 a1c1 d7c5 b5c4 e2e3 a2a3 h2h3 f3g5 e1f1 c3c4 h2h4 g2g4 a2a4 g2g3 b2b3 e2d1 b5a4 b5a6 a1b1 h1g1 e2f1 e1d1 f3h4 d7f8 d7b8 f3g1", str(moves));
  for (Move m : moves) {
    fprintln("{}", strVerbose(m));
  }
  NEWLINE;

  // 86 pseudo legal moves (incl. castling over attacked square)
  fen      = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves<GenAll>(position, false);
  EXPECT_EQ(86, moves.size());
  EXPECT_EQ("c2b1Q a2b1Q a2a1Q c2c1Q c2b1N a2b1N f4g3 a2a1N c2c1N f4e3 b2a3 a8a3 g6e5 d7e5 b2e5 e6e5 c4e4 c6e4 c2b1R a2b1R c2b1B a2b1B e8c8 e8g8 h8f8 a8d8 a8c8 d7c5 b2d4 d7f6 h7h6 e6f7 f4f3 e6e7 g6e7 e6f6 e6d6 d7b6 e6f5 e6d5 c6d6 c6d5 c6c5 b2c3 c4d5 c4c5 c4d4 a8a6 a8a7 c4d3 h7h5 b7b5 e6g4 c6b6 c6b5 c4e2 c4b3 c4c3 e8f8 a8a4 c4b4 a8a5 c4b5 c4a4 c6a4 b7b6 c4a6 c4f1 b2c1 c6a6 h8g8 e6h3 a8b8 e6g8 b2a1 d7f8 g6f8 e8e7 e8f7 e8d8 g6h4 d7b8 a2a1R c2c1R c2c1B a2a1B", str(moves));
  for (Move m : moves) {
    fprintln("{}", strVerbose(m));
  }
  NEWLINE;

  // 218 pseudo legal moves (incl. castling over attacked square)
  fen      = "R6R/3Q4/1Q4Q1/4Q3/2Q4Q/Q4Q2/pp1Q4/kBNN1KB1 w - -";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves<GenAll>(position, false);
  EXPECT_EQ(218, moves.size());
  for (Move m : moves) {
    fprintln("{}", strVerbose(m));
  }
  NEWLINE;

  // bug fixed positions
  fen      = "rnbqkbnr/1ppppppp/8/p7/7P/8/PPPPPPP1/RNBQKBNR w KQkq a6";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves<GenAll>(position);
  EXPECT_EQ(21, moves.size());
  for (Move m : moves) {
    fprintln("{}", strVerbose(m));
  }
  NEWLINE;

  fen      = "rnbqkbnr/p2ppppp/8/1Pp5/8/8/1PPPPPPP/RNBQKBNR w KQkq c6";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves<GenAll>(position);
  EXPECT_EQ(26, moves.size());
  for (Move m : moves) {
    fprintln("{}", strVerbose(m));
  }
  NEWLINE;

  // kiwipete 48
  fen      = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves<GenAll>(position);
  EXPECT_EQ(48, moves.size());
  for (Move m : moves) {
    fprintln("{}", strVerbose(m));
  }
}

TEST_F(MoveGenTest, legalMoves) {
  string fen;
  MoveGenerator mg;
  MoveList moves;
  Position position;

  // Startpos
  position = Position(START_POSITION_FEN);
  moves.clear();
  moves = *mg.generateLegalMoves<GenAll>(position);
  EXPECT_EQ(20, moves.size());

  // 86 pseudo legal moves - 83 legal (incl. castling over attacked square)
  fen      = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
  position = Position(fen);
  moves.clear();
  moves = *mg.generateLegalMoves<GenAll>(position);
  EXPECT_EQ(83, moves.size());
  ASSERT_FALSE(position.isLegalMove(createMove(SQ_E8, SQ_G8, CASTLING)));
}

TEST_F(MoveGenTest, hasLegalMoves) {
  Position position;
  MoveGenerator mg;
  MoveList moves;

  // check mate position
  position = Position("rn2kbnr/pbpp1ppp/8/1p2p1q1/4K3/3P4/PPP1PPPP/RNBQ1BNR w kq -");
  moves    = *mg.generateLegalMoves<GenAll>(position);
  EXPECT_EQ(0, moves.size());
  ASSERT_FALSE(mg.hasLegalMove(position));
  ASSERT_TRUE(position.hasCheck());

  // stale mate position
  position = Position("7k/5K2/6Q1/8/8/8/8/8 b - -");
  moves    = *mg.generateLegalMoves<GenAll>(position);
  EXPECT_EQ(0, moves.size());
  ASSERT_FALSE(mg.hasLegalMove(position));
  ASSERT_FALSE(position.hasCheck());

  // only en passant
  position = Position("8/8/8/8/5Pp1/6P1/7k/K3BQ2 b - f3");
  moves    = *mg.generateLegalMoves<GenAll>(position);
  EXPECT_EQ(1, moves.size());
  ASSERT_TRUE(mg.hasLegalMove(position));
  ASSERT_FALSE(position.hasCheck());
}

TEST_F(MoveGenTest, validateMove) {
  string fen;
  MoveGenerator mg;
  MoveList moves;

  // 86 pseudo legal moves (incl. castling over attacked square)
  fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
  Position position(fen);

  ASSERT_TRUE(mg.validateMove(position, createMove(SQ_B2, SQ_E5)));
  ASSERT_TRUE(mg.validateMove(position, createMove(SQ_E6, SQ_E5)));
  ASSERT_TRUE(mg.validateMove(position, createMove(SQ_C4, SQ_E4)));
  ASSERT_TRUE(mg.validateMove(position, createMove(SQ_C6, SQ_E4)));
  ASSERT_TRUE(mg.validateMove(position, createMove(SQ_A2, SQ_A1, PROMOTION, QUEEN)));
  ASSERT_TRUE(mg.validateMove(position, createMove(SQ_C2, SQ_C1, PROMOTION, QUEEN)));
  ASSERT_TRUE(mg.validateMove(position, createMove(SQ_A2, SQ_A1, PROMOTION, QUEEN)));
  ASSERT_TRUE(mg.validateMove(position, createMove(SQ_C2, SQ_C1, PROMOTION, QUEEN)));
  ASSERT_FALSE(mg.validateMove(position, createMove(SQ_E2, SQ_E4)));
  ASSERT_FALSE(mg.validateMove(position, createMove(SQ_B8, SQ_C8)));
  ASSERT_FALSE(mg.validateMove(position, createMove(SQ_A2, SQ_B3)));
  ASSERT_FALSE(mg.validateMove(position, createMove(SQ_B1, SQ_C3)));
  ASSERT_FALSE(mg.validateMove(position, MOVE_NONE));
}

TEST_F(MoveGenTest, fromUci) {
  MoveGenerator mg;
  Position pos;
  Move move;

  pos = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3");

  // invalid pattern
  move = mg.getMoveFromUci(pos, "8888");
  ;
  EXPECT_EQ(MOVE_NONE, move);
  ;

  // valid move
  move = mg.getMoveFromUci(pos, "b7b5");
  EXPECT_EQ(createMove(SQ_B7, SQ_B5, NORMAL), move);

  // invalid move
  move = mg.getMoveFromUci(pos, "a7a5");
  EXPECT_EQ(MOVE_NONE, move);

  // valid promotion
  move = mg.getMoveFromUci(pos, "a2a1Q");
  EXPECT_EQ(createMove(SQ_A2, SQ_A1, PROMOTION, QUEEN), move);

  // valid promotion (we allow lower case promotions);
  move = mg.getMoveFromUci(pos, "a2a1q");
  EXPECT_EQ(createMove(SQ_A2, SQ_A1, PROMOTION, QUEEN), move);

  // valid castling
  move = mg.getMoveFromUci(pos, "e8c8");
  EXPECT_EQ(createMove(SQ_E8, SQ_C8, CASTLING), move);

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
  EXPECT_EQ(createMove(SQ_B7, SQ_B5, NORMAL), move);

  // invalid move
  move = mg.getMoveFromSan(pos, "a5");
  EXPECT_EQ(MOVE_NONE, move);

  // valid promotion
  move = mg.getMoveFromSan(pos, "a1Q");
  EXPECT_EQ(createMove(SQ_A2, SQ_A1, PROMOTION, QUEEN), move);

  // valid promotion (we allow lower case promotions);
  move = mg.getMoveFromSan(pos, "a1q");
  EXPECT_EQ(MOVE_NONE, move);

  // valid castling
  move = mg.getMoveFromSan(pos, "O-O-O");
  EXPECT_EQ(createMove(SQ_E8, SQ_C8, CASTLING), move);

  // invalid castling
  move = mg.getMoveFromSan(pos, "O-O");
  EXPECT_EQ(MOVE_NONE, move);

  // ambiguous
  move = mg.getMoveFromSan(pos, "Ne5");
  EXPECT_EQ(MOVE_NONE, move);
  move = mg.getMoveFromSan(pos, "Nde5");
  EXPECT_EQ(createMove(SQ_D7, SQ_E5, NORMAL), move);
  move = mg.getMoveFromSan(pos, "Nge5");
  EXPECT_EQ(createMove(SQ_G6, SQ_E5, NORMAL), move);
  move = mg.getMoveFromSan(pos, "N7e5");
  EXPECT_EQ(createMove(SQ_D7, SQ_E5, NORMAL), move);
  move = mg.getMoveFromSan(pos, "N6e5");
  EXPECT_EQ(createMove(SQ_G6, SQ_E5, NORMAL), move);
  move = mg.getMoveFromSan(pos, "ab1Q");
  EXPECT_EQ(createMove(SQ_A2, SQ_B1, PROMOTION, QUEEN), move);
  move = mg.getMoveFromSan(pos, "cb1Q");
  EXPECT_EQ(createMove(SQ_C2, SQ_B1, PROMOTION, QUEEN), move);
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
  while ((move = mg.getNextPseudoLegalMove<GenAll>(position)) != MOVE_NONE) {
    counter++;
  }
  EXPECT_EQ(86, counter);

  // 218 moves
  fen      = "R6R/3Q4/1Q4Q1/4Q3/2Q4Q/Q4Q2/pp1Q4/kBNN1KB1 w - - 0 1";
  position = Position(fen);
  counter  = 0;
  while ((move = mg.getNextPseudoLegalMove<GenAll>(position)) != MOVE_NONE) {
    counter++;
  }
  EXPECT_EQ(218, counter);

  // 48 kiwipete moves
  fen      = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
  position = Position(fen);
  counter  = 0;
  while ((move = mg.getNextPseudoLegalMove<GenAll>(position)) != MOVE_NONE) {
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

  const MoveList* allMoves = mg.generatePseudoLegalMoves<GenQuiet>(position);

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
  allMoves = mg.generatePseudoLegalMoves<GenQuiet>(position);

  // add a killer NOT already in the list - should change
  mg.storeKiller(allMoves->at(31));
  EXPECT_EQ(allMoves->at(31), mg.killerMoves[0]);
}

TEST_F(MoveGenTest, onDemandKiller) {
  string fen;
  MoveGenerator mg;
  MoveList moves;
  Position pos;
  Move move;

  // 86
  pos              = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3");
  Move moveFromUci = mg.getMoveFromUci(pos, "g6h4");
  mg.storeKiller(moveFromUci);
  moveFromUci = mg.getMoveFromUci(pos, "b7b6");
  mg.storeKiller(moveFromUci);
  moveFromUci = mg.getMoveFromUci(pos, "a2b1Q");
  mg.setPV(moveFromUci);
  while ((move = mg.getNextPseudoLegalMove<GenAll>(pos)) != MOVE_NONE) {
    moves.push_back(move);
  }
  EXPECT_EQ(86, moves.size());
  EXPECT_EQ("a2b1Q c2b1Q a2a1Q c2c1Q c2b1N a2b1N f4g3 a2a1N c2c1N f4e3 c2b1R a2b1R c2b1B a2b1B b2a3 a8a3 g6e5 d7e5 b2e5 e6e5 c4e4 c6e4 b7b6 f4f3 h7h6 b7b5 h7h5 a2a1R c2c1R a2a1B c2c1B e8g8 e8c8 g6h4 d7c5 a8c8 a8d8 h8f8 d7f6 b2d4 e6f6 e6d6 e6f5 b2c3 e6d5 c6d6 c6d5 c6c5 c4d5 c4c5 g6e7 e6f7 d7b6 e6e7 c4d4 c4e2 c4b3 c4c3 c4d3 c4b4 c4b5 c6b5 a8a7 a8a6 c6b6 a8a5 e6g4 a8a4 c4a4 c6a4 h8g8 a8b8 c4f1 c4a6 c6a6 e6h3 b2c1 e6g8 b2a1 d7f8 g6f8 d7b8 e8f8 e8e7 e8f7 e8d8", str(moves));
  //         c2b1Q a2b1Q a2a1Q c2c1Q c2b1N a2b1N f4g3 a2a1N c2c1N f4e3 c2b1R a2b1R c2b1B a2b1B b2a3 a8a3 g6e5 d7e5 b2e5 e6e5 c4e4 c6e4 f4f3 h7h6 b7b5 h7h5 b7b6 a2a1R c2c1R a2a1B c2c1B e8g8 e8c8 a8d8 h8f8 a8c8 d7c5 b2d4 d7f6 e6e7 e6f6 e6d6 e6f5 b2c3 d7b6 e6d5 e6f7 c6c5 g6e7 c6d6 c4c5 c4d5 c6d5 c4d4 a8a4 a8a5 a8a6 a8a7 e6g4 c4e2 c4b3 c6b6 c4d3 c4b4 c4b5 c6b5 c4c3 c4a4 c6a4 c4a6 c6a6 e6g8 c4f1 e6h3 h8g8 a8b8 b2c1 b2a1 d7f8 g6f8 g6h4 d7b8 e8f8 e8e7 e8f7 e8d8
  moves.clear();

  mg.reset();

  // 48 kiwipete
  pos         = Position("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
  moveFromUci = mg.getMoveFromUci(pos, "d2g5");
  mg.storeKiller(moveFromUci);
  moveFromUci = mg.getMoveFromUci(pos, "b2b3");
  mg.storeKiller(moveFromUci);
  moveFromUci = mg.getMoveFromUci(pos, "e2a6");
  mg.setPV(moveFromUci);
  while ((move = mg.getNextPseudoLegalMove<GenAll>(pos)) != MOVE_NONE) {
    moves.push_back(move);
  }
  EXPECT_EQ(48, moves.size());
  EXPECT_EQ("e2a6 g2h3 d5e6 e5g6 e5d7 e5f7 f3f6 f3h3 b2b3 a2a3 d5d6 a2a4 g2g4 g2g3 e1g1 e1c1 d2g5 e5d3 e5c4 a1c1 a1d1 h1f1 e5c6 d2e3 d2f4 e2d3 e2c4 c3b5 e2b5 f3d3 f3e3 f3f4 f3f5 e5g4 f3g3 f3g4 f3h5 d2h6 e2d1 a1b1 h1g1 c3d1 c3a4 c3b1 d2c1 e2f1 e1f1 e1d1", str(moves));
  //         g2h3 d5e6 e2a6 e5g6 e5d7 e5f7 f3f6 f3h3 a2a3 d5d6 a2a4 g2g4 b2b3 g2g3 e1g1 e1c1 e5d3 e5c4 a1c1 a1d1 h1f1 e5c6 d2e3 d2f4 e2d3 e2c4 c3b5 d2g5 e2b5 f3d3 f3e3 f3f4 f3f5 e5g4 f3g3 f3g4 f3h5 d2h6 e2d1 a1b1 h1g1 c3d1 c3a4 c3b1 d2c1 e2f1 e1f1 e1d1
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
  Move pvMove = createMove(SQ_B1, SQ_B2);
  mg.setPV(pvMove);
  Move move;
  int counter = 0;
  // generate all moves
  while ((move = mg.getNextPseudoLegalMove<GenAll>(position)) != MOVE_NONE) {
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
  pvMove = createMove(SQ_B1, SQ_B2);
  mg.setPV(pvMove);
  counter = 0;
  // generate all moves
  while ((move = mg.getNextPseudoLegalMove<GenNonQuiet>(position)) != MOVE_NONE) {
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
  pvMove = createMove(SQ_H2, SQ_H3);
  mg.setPV(pvMove);
  counter = 0;
  // generate all moves
  while ((move = mg.getNextPseudoLegalMove<GenAll>(position)) != MOVE_NONE) {
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
  pvMove = createMove(SQ_H2, SQ_H3);
  mg.setPV(pvMove);
  counter = 0;
  // generate all moves
  while ((move = mg.getNextPseudoLegalMove<GenNonQuiet>(position)) != MOVE_NONE) {
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
  pvMove = createMove(SQ_H2, SQ_H3);
  mg.setPV(pvMove);
  counter = 0;
  // generate all moves
  while ((move = mg.getNextPseudoLegalMove<GenQuiet>(position)) != MOVE_NONE) {
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
  pseudoLegalMoves = mg.generatePseudoLegalMoves<GenAll>(p, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), str(*pseudoLegalMoves));
  evasionMoves     = mg.generatePseudoLegalMoves<GenAll>(p, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), str(*evasionMoves));
  legalMoves       = mg.generateLegalMoves<GenAll>(p);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), str(*legalMoves));
  fprintln("");

  p                = Position("5k2/8/8/8/8/8/6p1/3K1R2 b - -");
  pseudoLegalMoves = mg.generatePseudoLegalMoves<GenAll>(p, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), str(*pseudoLegalMoves));
  evasionMoves     = mg.generatePseudoLegalMoves<GenAll>(p, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), str(*evasionMoves));
  legalMoves       = mg.generateLegalMoves<GenAll>(p);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), str(*legalMoves));
  fprintln("");

  p                = Position("5k2/8/8/8/8/6p1/5R2/3K4 b - -");
  pseudoLegalMoves = mg.generatePseudoLegalMoves<GenAll>(p, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), str(*pseudoLegalMoves));
  evasionMoves     = mg.generatePseudoLegalMoves<GenAll>(p, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), str(*evasionMoves));
  legalMoves       = mg.generateLegalMoves<GenAll>(p);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), str(*legalMoves));
  fprintln("");

  p                = Position("8/8/8/3k4/4Pp2/8/8/3K4 b - e3");
  pseudoLegalMoves = mg.generatePseudoLegalMoves<GenAll>(p, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), str(*pseudoLegalMoves));
  evasionMoves     = mg.generatePseudoLegalMoves<GenAll>(p, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), str(*evasionMoves));
  legalMoves       = mg.generateLegalMoves<GenAll>(p);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), str(*legalMoves));
  fprintln("");

  p                = Position("8/8/8/3k2n1/8/8/6B1/3K4 b - -");
  pseudoLegalMoves = mg.generatePseudoLegalMoves<GenAll>(p, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), str(*pseudoLegalMoves));
  evasionMoves     = mg.generatePseudoLegalMoves<GenAll>(p, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), str(*evasionMoves));
  legalMoves       = mg.generateLegalMoves<GenAll>(p);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), str(*legalMoves));
  fprintln("");

  p                = Position("5k2/3N4/8/8/8/8/6p1/3K1R2 b - - 1 1");
  pseudoLegalMoves = mg.generatePseudoLegalMoves<GenAll>(p, false);
  fprintln("PseudoLegal: {:3d} {:s}", pseudoLegalMoves->size(), str(*pseudoLegalMoves));
  evasionMoves     = mg.generatePseudoLegalMoves<GenAll>(p, true);
  fprintln("Evasion    : {:3d} {:s}", evasionMoves->size(), str(*evasionMoves));
  legalMoves       = mg.generateLegalMoves<GenAll>(p);
  fprintln("Legal      : {:3d} {:s}", legalMoves->size(), str(*legalMoves));
  fprintln("");
}

#include <chrono>
using namespace std::chrono;

// 8.6.: Loaner Mac:
// 480.000.000 moves generated: 84.774.069 mps
TEST_F(MoveGenTest, DISABLED_PseudoMoveGen) {
  MoveGenerator mg;

  const int rounds     = 5;
  const int iterations = 10'000'000;

  Position position = Position("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
  const MoveList *moves = mg.generatePseudoLegalMoves<GenAll>(position);

  for (int r = 1; r <= rounds; r++) {
    fprintln("Round {}", r);
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
      mg.reset();
      moves = mg.generatePseudoLegalMoves<GenAll>(position);
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
  MoveGenerator mg;
  Position p("rnbq1bnr/ppp1pppp/4k3/3pP3/3P2Q1/8/PPP2PPP/RNB1KBNR b KQ - 2 4");
  fprintln("{}", mg.generatePseudoLegalMoves<GenAll>(p)->size());
  fprintln("{}", mg.generateLegalMoves<GenAll>(p)->size());
  fprintln("{}", str(mg.generateLegalMoves<GenAll>(p)->at(0)));
  fprintln("{}", mg.hasLegalMove(p));

}