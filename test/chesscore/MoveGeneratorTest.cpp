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
#include <set>
#include <string>

#include "Test_Utils.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"
#include "init.h"
#include "types/types.h"

using namespace std;
using namespace chess;
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
  // Verify all expected moves are present (order may vary with non-stable sort)
  const std::set<string> expectedStart = {"d2d4", "e2e4", "b1c3", "g1f3", "a2a3", "h2h3", "a2a4", "b2b4", "c2c4", "f2f4", "g2g4", "h2h4", "d2d3", "e2e3", "b2b3", "g2g3", "c2c3", "f2f3", "b1a3", "g1h3"};
  std::set<string> actualStart;
  for (const auto& m : moves) actualStart.insert(m.str());
  EXPECT_EQ(expectedStart, actualStart);
  //  for (Move m : moves) {
  //    fprintln("{}", m.strVerbose());
  //  }
  NEWLINE;

  fen      = "r3k2r/pbpNqppp/1pn2n2/1B2p3/1b2P3/2PP1N2/PP1nQPPP/R3K2R w KQkq -";
  position = Position(fen);
  moves.clear();
  moves = *mg.generatePseudoLegalMoves(position, GenAll, false);
  EXPECT_EQ(40, moves.size());
  // Verify all expected moves are present (order may vary with non-stable sort)
  const std::set<string> expected40 = {"d7f6", "f3d2", "b5c6", "f3e5", "d7e5", "d7b6", "e2d2", "e1d2", "c3b4", "e1c1", "e1g1", "d3d4", "f3d4", "d7c5", "a1c1", "a1d1", "h1f1", "b5c4", "a2a3", "f3g5", "h2h3", "e2e3", "g2g4", "h2h4", "c3c4", "e1f1", "a2a4", "b2b3", "g2g3", "e2d1", "b5a4", "b5a6", "a1b1", "h1g1", "e2f1", "e1d1", "f3h4", "d7f8", "d7b8", "f3g1"};
  std::set<string> actual40;
  for (const auto& m : moves) actual40.insert(m.str());
  EXPECT_EQ(expected40, actual40);
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
  // Verify all expected moves are present (order may vary with non-stable sort)
  const std::set<string> expected86 = {"c2b1Q", "a2b1Q", "a2a1Q", "c2c1Q", "c2b1N", "a2b1N", "b2a3", "a8a3", "g6e5", "d7e5", "b2e5", "c2c1N", "a2a1N", "e6e5", "c4e4", "c6e4", "f4g3", "f4e3", "e8g8", "e8c8", "d7c5", "a8c8", "h8f8", "a8d8", "d7f6", "b2d4", "g6e7", "e6f6", "e6d6", "e6f5", "c6d6", "c6d5", "c6c5", "e6d5", "c4d5", "f4f3", "c4c5", "h7h6", "b2c3", "d7b6", "e6e7", "e6f7", "c4d4", "e8f8", "a8a4", "a8a5", "a8a6", "a8a7", "c4e2", "c4b3", "c4c3", "c4d3", "c4b4", "c4b5", "c6b5", "c6b6", "e6g4", "b7b5", "h7h5", "c4a4", "b7b6", "c6a4", "c4f1", "c6a6", "b2c1", "e6h3", "a8b8", "h8g8", "e6g8", "c4a6", "b2a1", "e8e7", "e8f7", "e8d8", "g6h4", "g6f8", "d7f8", "d7b8", "a2b1R", "c2b1R", "a2b1B", "c2b1B", "c2c1R", "a2a1R", "c2c1B", "a2a1B"};
  std::set<string> actual86;
  for (const auto& m : moves) actual86.insert(m.str());
  EXPECT_EQ(expected86, actual86);
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
  MoveGenerator mg;
  MoveList moves;

  // Startpos
  auto position = Position(START_POSITION_FEN);
  moves.clear();
  moves = *mg.generateLegalMoves(position, GenAll);
  EXPECT_EQ(20, moves.size());

  // 86 pseudo legal moves - 83 legal (incl. castling over attacked square)
  const string fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
  position = Position(fen);
  moves.clear();
  moves = *mg.generateLegalMoves(position, GenAll);
  EXPECT_EQ(83, moves.size());
  EXPECT_FALSE(position.isLegalMove(Move::castling(SQ_E8, SQ_G8)));
}

TEST_F(MoveGenTest, validateMove) {
  string fen;
  MoveGenerator mg;

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
  MoveGenerator mg;

  // 86 pseudo legal moves (incl. castling over attacked square)
  const string fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
  const Position position(fen);

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
  MoveList movesWithKillers;
  Move move;

  // Helper to find index of a move by UCI string in a move list
  const auto findMoveIndex = [](const MoveList& list, const string& uci) -> int {
    for (size_t i = 0; i < list.size(); i++) {
      if (list[i].str() == uci) return static_cast<int>(i);
    }
    return -1;
  };

  // 86 moves
  auto pos = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3");

  // First pass: generate without PV/killers to get baseline move count
  while ((move = mg.getNextPseudoLegalMove(pos, GenAll)) != MOVE_NONE) {
    moves.push_back(move);
  }
  EXPECT_EQ(86, moves.size());

  // Second pass: generate with PV and killers set
  mg.reset();
  const Move killer1 = mg.getMoveFromUci(pos, "g6h4");
  const Move killer2 = mg.getMoveFromUci(pos, "b7b6");
  const Move pv1     = mg.getMoveFromUci(pos, "a2b1Q");
  mg.resetOnDemand();
  mg.storeKiller(killer1);
  mg.storeKiller(killer2);
  mg.setPV(pv1);
  while ((move = mg.getNextPseudoLegalMove(pos, GenAll)) != MOVE_NONE) {
    movesWithKillers.push_back(move);
  }
  EXPECT_EQ(86, movesWithKillers.size());

  // PV move must be first
  EXPECT_EQ("a2b1Q", movesWithKillers[0].str());

  // Killer moves must be somewhere in the middle (not first, not last)
  const int idx_k1 = findMoveIndex(movesWithKillers, "g6h4");
  const int idx_k2 = findMoveIndex(movesWithKillers, "b7b6");
  EXPECT_GT(idx_k1, 0);
  EXPECT_LT(idx_k1, static_cast<int>(movesWithKillers.size()) - 1);
  EXPECT_GT(idx_k2, 0);
  EXPECT_LT(idx_k2, static_cast<int>(movesWithKillers.size()) - 1);

  moves.clear();
  movesWithKillers.clear();
  mg.reset();

  // 48 kiwipete
  pos = Position("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");

  // First pass: generate without PV/killers
  while ((move = mg.getNextPseudoLegalMove(pos, GenAll)) != MOVE_NONE) {
    moves.push_back(move);
  }
  EXPECT_EQ(48, moves.size());

  // Second pass: generate with PV and killers set
  mg.reset();
  const Move killer3 = mg.getMoveFromUci(pos, "d2g5");
  const Move killer4 = mg.getMoveFromUci(pos, "b2b3");
  const Move pv2     = mg.getMoveFromUci(pos, "e2a6");
  mg.resetOnDemand();
  mg.storeKiller(killer3);
  mg.storeKiller(killer4);
  mg.setPV(pv2);
  while ((move = mg.getNextPseudoLegalMove(pos, GenAll)) != MOVE_NONE) {
    movesWithKillers.push_back(move);
  }
  EXPECT_EQ(48, movesWithKillers.size());

  // PV move must be first
  EXPECT_EQ("e2a6", movesWithKillers[0].str());

  // Killer moves must be somewhere in the middle (not first, not last)
  const int idx_k3 = findMoveIndex(movesWithKillers, "d2g5");
  const int idx_k4 = findMoveIndex(movesWithKillers, "b2b3");
  EXPECT_GT(idx_k3, 0);
  EXPECT_LT(idx_k3, static_cast<int>(movesWithKillers.size()) - 1);
  EXPECT_GT(idx_k4, 0);
  EXPECT_LT(idx_k4, static_cast<int>(movesWithKillers.size()) - 1);
}

TEST_F(MoveGenTest, pvMove) {
  string fen;
  MoveGenerator mg;

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

  const auto position   = Position("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
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

// =================================================================================================
// hasLegalEpCapture Tests
// =================================================================================================

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
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",     // starting
    "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3",// checkmate
    "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1",                               // stalemate
    "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3",   // complex
    "4k3/8/8/K2pP2r/8/8/8/8 w - d6 0 1",                            // EP pinned
    "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",                            // EP legal
    "8/8/8/8/k1PpK3/8/8/8 b - c3 0 1",                              // EP only move
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

// =================================================================================================
// isPseudoLegal tests - validates TT move structural and attack pattern correctness
// =================================================================================================

TEST_F(MoveGenTest, isPseudoLegal_basicChecks) {
  const Position p;// start position

  // MOVE_NONE should fail
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, MOVE_NONE));

  // Move to same square should fail
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E2, SQ_E2)));

  // No piece on from square should fail
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E4, SQ_E5)));

  // Moving opponent's piece should fail
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E7, SQ_E6)));

  // Capturing own piece should fail
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E2, SQ_D1)));
}

TEST_F(MoveGenTest, isPseudoLegal_pawnMoves) {
  const Position p;// start position

  // Valid pawn moves
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E2, SQ_E4)));// double push
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E2, SQ_E3)));// single push
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_A2, SQ_A3)));// edge pawn
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_H2, SQ_H4)));// edge pawn double

  // Invalid pawn moves
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E2, SQ_E5)));// triple push
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E2, SQ_D3)));// diagonal without capture
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E2, SQ_E1)));// backward

  // Pawn capture
  const Position p2("rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_E4, SQ_D5)));// capture
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_E4, SQ_F5)));// no piece to capture

  // Double push only from start rank
  const Position p3("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p3, Move::normal(SQ_E4, SQ_E6)));// not from start rank

  // Pawn push blocked by piece (position after 1.d4 Nf6 2.d5 Ne4)
  const Position p4("rnbqkb1r/pppppppp/8/3P4/4n3/8/PPP1PPPP/RNBQKBNR w KQkq - 1 3");
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p4, Move::normal(SQ_E2, SQ_E3)));// single push ok
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p4, Move::normal(SQ_E2, SQ_E4)));// blocked by knight on e4

  // Pawn single push blocked (position after 1.e4 e5 2.Nf3 Ne7 3.Ng1 Ng6 4.Nf3 Ne5)
  const Position p5("rnbqkb1r/pppp1ppp/8/4n3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 2 5");
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p5, Move::normal(SQ_E4, SQ_E5)));// blocked by knight on e5
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p5, Move::normal(SQ_D2, SQ_D4)));// other pawn ok

  // Pawn double push - target square blocked (contrived but valid for testing)
  const Position p6("rnbqkbnr/pppp1ppp/8/4p3/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");// after 1...e5
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p6, Move::normal(SQ_E2, SQ_E3)));// single push ok
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p6, Move::normal(SQ_E2, SQ_E4)));// double push ok (e5 doesn't block)
}

TEST_F(MoveGenTest, isPseudoLegal_knightMoves) {
  const Position p;// start position

  // Valid knight moves (L-shape)
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_G1, SQ_F3)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_G1, SQ_H3)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_B1, SQ_C3)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_B1, SQ_A3)));

  // Invalid knight moves (not L-shape)
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_G1, SQ_G3)));// straight
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_G1, SQ_E2)));// diagonal
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_G1, SQ_G2)));// one step
}

TEST_F(MoveGenTest, isPseudoLegal_bishopMoves) {
  // Position after 1.e4 e6 - bishop f1 diagonal is open
  const Position p("rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");

  // Valid bishop diagonal moves (path clear)
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_F1, SQ_E2)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_F1, SQ_D3)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_F1, SQ_C4)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_F1, SQ_B5)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_F1, SQ_A6)));// long diagonal

  // Invalid bishop moves (not diagonal)
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_F1, SQ_F2)));// straight up
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_F1, SQ_E1)));// horizontal
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_F1, SQ_G3)));// knight-like

  // Invalid: bishop path blocked (position after 1.e3 e6 - pawn on e3 blocks c1 bishop)
  const Position p2("rnbqkbnr/pppp1ppp/4p3/8/8/4P3/PPPP1PPP/RNBQKBNR w KQkq - 0 2");// pawns on e3/e6
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_C1, SQ_F4)));// blocked by e3 pawn (c1-d2-e3-f4)
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_C1, SQ_G5)));// blocked by e3 pawn
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_F1, SQ_E2)));// not blocked
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_C1, SQ_E3)));// e3 has our pawn - can't capture own piece
}

TEST_F(MoveGenTest, isPseudoLegal_rookMoves) {
  const Position p("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");

  // Valid rook moves (rank/file, path clear)
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_A1, SQ_B1)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_A1, SQ_C1)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_A1, SQ_D1)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_H1, SQ_G1)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_H1, SQ_F1)));

  // Invalid rook moves (diagonal)
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_A1, SQ_B2)));// diagonal
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_H1, SQ_G2)));// diagonal

  // Invalid: rook path blocked
  const Position p2("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/RN2K2R w KQkq - 0 1");// knight on b1
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_A1, SQ_C1)));// blocked by knight
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_A1, SQ_D1)));// blocked by knight
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_H1, SQ_G1)));// not blocked
}

TEST_F(MoveGenTest, isPseudoLegal_queenMoves) {
  // Position with queen in center (after some opening moves)
  const Position p("rnbqkbnr/pppp1ppp/8/4p3/3QP3/8/PPP2PPP/RNB1KBNR w KQkq - 0 3");

  // Valid queen moves (diagonal and rank/file, path clear)
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_D4, SQ_D5)));// vertical
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_D4, SQ_D6)));// vertical
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_D4, SQ_H4)));// horizontal
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_D4, SQ_A4)));// horizontal
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_D4, SQ_E5)));// diagonal capture
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_D4, SQ_C5)));// diagonal

  // Invalid queen moves (knight-like)
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_D4, SQ_E6)));// L-shape
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_D4, SQ_B3)));// L-shape

  // Invalid: queen path blocked (knight blocks diagonal)
  const Position p2("rnbqkb1r/ppp2ppp/4pn2/8/3QP3/8/PPP2PPP/RNB1KBNR w KQkq - 2 3");// black knight on f6
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_D4, SQ_G7)));// blocked by f6 knight
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_D4, SQ_H8)));// blocked by f6 knight
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p2, Move::normal(SQ_D4, SQ_F6)));// capture knight is ok
}

TEST_F(MoveGenTest, isPseudoLegal_kingMoves) {
  // Position with king in center (simplified endgame position)
  const Position p("8/8/8/8/4K3/8/8/7k w - - 0 1");

  // Valid king moves (one square any direction)
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E4, SQ_E5)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E4, SQ_D4)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E4, SQ_F5)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E4, SQ_D3)));

  // Invalid king moves (more than one square)
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E4, SQ_E6)));// two squares
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E4, SQ_G4)));// two squares
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E4, SQ_G6)));// knight-like
}

TEST_F(MoveGenTest, isPseudoLegal_promotions) {
  // Position with pawn ready to promote
  const Position p("8/P7/8/8/8/8/8/K6k w - - 0 1");

  // Valid promotions
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::promotion(SQ_A7, SQ_A8, QUEEN)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::promotion(SQ_A7, SQ_A8, ROOK)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::promotion(SQ_A7, SQ_A8, BISHOP)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::promotion(SQ_A7, SQ_A8, KNIGHT)));

  // Invalid: promotion to wrong rank
  const Position p2("8/8/P7/8/8/8/8/K6k w - - 0 1");
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p2, Move::promotion(SQ_A6, SQ_A7, QUEEN)));

  // Invalid: non-pawn promotion
  const Position p3("K7/8/8/8/8/8/8/7k w - - 0 1");
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p3, Move::promotion(SQ_A8, SQ_B8, QUEEN)));

  // Promotion with capture
  const Position p4("1n6/P7/8/8/8/8/8/K6k w - - 0 1");
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p4, Move::promotion(SQ_A7, SQ_B8, QUEEN)));
  // Note: Can't test invalid promo piece types (PAWN, KING) because Move constructor
  // clamps them to KNIGHT: (promType < KNIGHT ? KNIGHT : promType)
}

TEST_F(MoveGenTest, isPseudoLegal_enPassant) {
  // Position with en passant possible
  const Position p("8/8/8/pP6/8/8/8/K6k w - a6 0 1");

  // Valid en passant
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::enPassant(SQ_B5, SQ_A6)));

  // Invalid: en passant to wrong square
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::enPassant(SQ_B5, SQ_B6)));

  // Invalid: en passant with non-pawn
  const Position p2("8/8/8/pK6/8/8/8/7k w - a6 0 1");
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p2, Move::enPassant(SQ_B5, SQ_A6)));

  // Invalid: en passant when no ep square set
  const Position p3("8/8/8/pP6/8/8/8/K6k w - - 0 1");
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p3, Move::enPassant(SQ_B5, SQ_A6)));

  // Invalid: en passant when captured pawn doesn't exist (corrupted position/move)
  // EP square is set but no pawn to capture - this catches corrupted TT data
  const Position p4("8/8/8/1P6/8/8/8/K6k w - a6 0 1");// EP square a6 but no pawn on a5
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p4, Move::enPassant(SQ_B5, SQ_A6)));

  // Invalid: captured piece is not a pawn
  const Position p5("8/8/8/nP6/8/8/8/K6k w - a6 0 1");// Knight on a5 instead of pawn
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p5, Move::enPassant(SQ_B5, SQ_A6)));

  // Invalid: captured pawn is our own pawn (corrupted)
  const Position p6("8/8/8/PP6/8/8/8/K6k w - a6 0 1");// Our pawn on a5
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p6, Move::enPassant(SQ_B5, SQ_A6)));
}

TEST_F(MoveGenTest, isPseudoLegal_castling) {
  // Position with castling possible (rights and path clear)
  const Position p("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");

  // Valid castling moves
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::castling(SQ_E1, SQ_G1)));// white kingside
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p, Move::castling(SQ_E1, SQ_C1)));// white queenside

  // Invalid: wrong from square
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::castling(SQ_D1, SQ_G1)));

  // Invalid: wrong to square
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::castling(SQ_E1, SQ_H1)));
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::castling(SQ_E1, SQ_B1)));

  // Invalid: castling with non-king
  const Position p2("8/8/8/8/8/8/8/R3K2R w KQ - 0 1");
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p2, Move::castling(SQ_A1, SQ_C1)));

  // Black castling
  const Position p3("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p3, Move::castling(SQ_E8, SQ_G8)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p3, Move::castling(SQ_E8, SQ_C8)));
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p3, Move::castling(SQ_E8, SQ_H8)));

  // Invalid: no castling rights
  const Position p4("r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1");// no rights
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p4, Move::castling(SQ_E1, SQ_G1)));
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p4, Move::castling(SQ_E1, SQ_C1)));

  // Invalid: only one side has rights
  const Position p5("r3k2r/8/8/8/8/8/8/R3K2R w K - 0 1");// only kingside
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p5, Move::castling(SQ_E1, SQ_G1)));
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p5, Move::castling(SQ_E1, SQ_C1)));

  const Position p6("r3k2r/8/8/8/8/8/8/R3K2R w Q - 0 1");// only queenside
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p6, Move::castling(SQ_E1, SQ_G1)));
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p6, Move::castling(SQ_E1, SQ_C1)));

  // Invalid: path blocked for kingside
  const Position p7("r3k2r/8/8/8/8/8/8/R3K1NR w KQ - 0 1");// knight on g1
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p7, Move::castling(SQ_E1, SQ_G1)));// blocked
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p7, Move::castling(SQ_E1, SQ_C1)));// ok

  // Invalid: path blocked for queenside
  const Position p8("r3k2r/8/8/8/8/8/8/R1N1K2R w KQ - 0 1");// knight on c1
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p8, Move::castling(SQ_E1, SQ_G1)));// ok
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p8, Move::castling(SQ_E1, SQ_C1)));// blocked

  // Path blocked between king and rook (b1 for queenside)
  const Position p9("r3k2r/8/8/8/8/8/8/RN2K2R w KQ - 0 1");// knight on b1
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(p9, Move::castling(SQ_E1, SQ_G1)));// ok
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p9, Move::castling(SQ_E1, SQ_C1)));// blocked by b1
}

TEST_F(MoveGenTest, isPseudoLegal_corruptedMoves) {
  const Position p;// start position

  // Simulate corrupted TT move - piece exists but move type is wrong
  // Non-pawn with PROMOTION type
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::promotion(SQ_G1, SQ_F3, QUEEN)));

  // Non-pawn with ENPASSANT type (and no en passant square)
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::enPassant(SQ_G1, SQ_F3)));

  // Non-king with CASTLING type
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::castling(SQ_G1, SQ_G3)));

  // Knight trying to move like bishop
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_G1, SQ_F2)));

  // Pawn trying to move like knight
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(p, Move::normal(SQ_E2, SQ_D4)));
}

TEST_F(MoveGenTest, isPseudoLegal_rawCorruptedMoves) {
  // Test with raw move data that bypasses Move constructor validation
  // This simulates corrupted TT entries with garbage bit patterns
  //
  // Move bit layout (from movetype.h):
  //   Bits 0-5:   TO square (6 bits, 0-63)
  //   Bits 6-11:  FROM square (6 bits, 0-63)
  //   Bits 12-13: PROM_TYPE (2 bits, 0=KNIGHT, 1=BISHOP, 2=ROOK, 3=QUEEN)
  //   Bits 14-15: MOVE_TYPE (2 bits, 0=NORMAL, 1=PROMOTION, 2=ENPASSANT, 3=CASTLING)
  //   Bits 16-31: VALUE (16 bits)

  const Position startPos;// start position

  // Move type raw values (before shifting to bit position 14-15)
  constexpr int MT_NORMAL    = 0;
  constexpr int MT_PROMOTION = 1;
  constexpr int MT_ENPASSANT = 2;
  constexpr int MT_CASTLING  = 3;

  // Helper to build raw move: to | (from << 6) | (promType << 12) | (moveType << 14)
  const auto makeRaw = [](const int to, const int from, const int promType, const int moveType) -> Move::Raw {
    return static_cast<Move::Raw>(to) | (static_cast<Move::Raw>(from) << 6) | (static_cast<Move::Raw>(promType) << 12)
           | (static_cast<Move::Raw>(moveType) << 14);
  };

  // Test 1: Invalid square indices (out of range 0-63)
  // Square 64 would be invalid but fits in 6 bits - the Move would decode it
  // Since from() and to() just mask the bits, we test the effect on isPseudoLegal
  // Actually squares are only 6 bits so max is 63, but let's test edge cases

  // Test 2: from == to (same square)
  constexpr Move::Raw sameSquare = makeRaw(SQ_E2, SQ_E2, 0, MT_NORMAL);
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(startPos, Move(sameSquare)));

  // Test 3: Move from empty square
  constexpr Move::Raw emptyFrom = makeRaw(SQ_E4, SQ_E3, 0, MT_NORMAL);// no piece on e3
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(startPos, Move(emptyFrom)));

  // Test 4: Move opponent's piece (white to move but moving black pawn)
  constexpr Move::Raw opponentPiece = makeRaw(SQ_E6, SQ_E7, 0, MT_NORMAL);
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(startPos, Move(opponentPiece)));

  // Test 5: Capture own piece
  constexpr Move::Raw captureOwn = makeRaw(SQ_D1, SQ_E2, 0, MT_NORMAL);// pawn captures queen
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(startPos, Move(captureOwn)));

  // Test 6: PROMOTION move type but piece is not a pawn
  constexpr Move::Raw knightPromo = makeRaw(SQ_F3, SQ_G1, 3, MT_PROMOTION);// knight "promotes"
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(startPos, Move(knightPromo)));

  // Test 7: ENPASSANT move type but piece is not a pawn
  constexpr Move::Raw knightEp = makeRaw(SQ_F3, SQ_G1, 0, MT_ENPASSANT);// knight "en passant"
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(startPos, Move(knightEp)));

  // Test 8: CASTLING move type but piece is not a king
  constexpr Move::Raw rookCastle = makeRaw(SQ_C1, SQ_A1, 0, MT_CASTLING);// rook "castles"
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(startPos, Move(rookCastle)));

  // Test 9: Valid looking promotion but wrong target rank
  constexpr Move::Raw wrongPromoRank = makeRaw(SQ_A7, SQ_A6, 3, MT_PROMOTION);// a6-a7 promo (rank 7 not 8)
  const Position midPos("8/8/P7/8/8/8/8/K6k w - - 0 1");
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(midPos, Move(wrongPromoRank)));

  // Test 10: En passant to wrong square (not the EP square)
  const Position epPos("8/8/8/pP6/8/8/8/K6k w - a6 0 1");
  constexpr Move::Raw wrongEpSquare = makeRaw(SQ_B6, SQ_B5, 0, MT_ENPASSANT);// b5-b6 not a6
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(epPos, Move(wrongEpSquare)));

  // Test 11: Castling with wrong target square
  const Position castlePos("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  constexpr Move::Raw wrongCastleTo = makeRaw(SQ_F1, SQ_E1, 0, MT_CASTLING);// e1-f1 not g1/c1
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(castlePos, Move(wrongCastleTo)));

  // Test 12: MOVE_NONE (all zeros)
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(startPos, Move(static_cast<Move::Raw>(0))));

  // Test 13: Garbage high bits in value field shouldn't affect validation
  // (isPseudoLegal should ignore value bits)
  constexpr Move::Raw withGarbageValue = makeRaw(SQ_E4, SQ_E2, 0, MT_NORMAL) | (0xFFFFu << 16);
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(startPos, Move(withGarbageValue)));// e2-e4 is valid

  // Test 14: Slider move through blocker (raw move encoding is valid but path blocked)
  constexpr Move::Raw blockedBishop = makeRaw(SQ_C4, SQ_F1, 0, MT_NORMAL);// f1-c4 blocked by e2
  EXPECT_FALSE(MoveGenerator::isPseudoLegal(startPos, Move(blockedBishop)));

  // Test 15: Valid raw move should pass
  constexpr Move::Raw validKnight = makeRaw(SQ_F3, SQ_G1, 0, MT_NORMAL);// Nf3
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(startPos, Move(validKnight)));

  constexpr Move::Raw validPawnDouble = makeRaw(SQ_E4, SQ_E2, 0, MT_NORMAL);// e4
  EXPECT_TRUE(MoveGenerator::isPseudoLegal(startPos, Move(validPawnDouble)));
}
