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

//TEST_F(MoveGenTest, castlingMoves) {
//  string        fen;
//  MoveGenerator mg;
//  MoveList      moves;
//
//  fen = "r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1 b kq e3";
//  Position position(fen);
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}", position.printBoard());
//
//  moves.clear();
//  mg.generateCastling<MoveGenerator::GENCAP>(position, &moves);
//  LOG__DEBUG(Logger::get().TEST_LOG, "Capture moves = {}", moves.size());
//  EXPECT_EQ(0, moves.size());
//
//  moves.clear();
//  mg.generateCastling<MoveGenerator::GENNONCAP>(position, &moves);
//  LOG__DEBUG(Logger::get().TEST_LOG, "Non capture moves = {}", moves.size());
//  EXPECT_EQ(2, moves.size());
//}
//
//TEST_F(MoveGenTest, pseudoLegalMoves) {
//  string        fen;
//  MoveGenerator mg;
//  MoveList      moves;
//  Position      position;
//
//  // Start pos
//  fen      = START_POSITION_FEN;
//  position = Position(fen);
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}", position.str());
//  moves.clear();
//  moves = *mg.generatePseudoLegalMoves<MoveGenerator::GENALL>(position);
//  LOG__DEBUG(Logger::get().TEST_LOG, "Moves = {}", moves.size());
//  for (Move m : moves) {
//    LOG__DEBUG(Logger::get().TEST_LOG, "{}", printMoveVerbose(m));
//  }
//  ASSERT_EQ(20, moves.size());
//
//  // 86 pseudo legal moves (incl. castling over attacked square)
//  fen      = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
//  position = Position(fen);
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}", position.str());
//  moves.clear();
//  moves = *mg.generatePseudoLegalMoves<MoveGenerator::GENALL>(position);
//  LOG__DEBUG(Logger::get().TEST_LOG, "Moves = {}", moves.size());
//  for (Move m : moves) {
//    LOG__DEBUG(Logger::get().TEST_LOG, "{}", printMoveVerbose(m));
//  }
//  ASSERT_EQ(86, moves.size());
//
//  // bug fixed positions
//  fen      = "rnbqkbnr/1ppppppp/8/p7/7P/8/PPPPPPP1/RNBQKBNR w KQkq a6";
//  position = Position(fen);
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}", position.str());
//  moves.clear();
//  moves = *mg.generatePseudoLegalMoves<MoveGenerator::GENALL>(position);
//  LOG__DEBUG(Logger::get().TEST_LOG, "Moves = {}", moves.size());
//  for (Move m : moves) {
//    LOG__DEBUG(Logger::get().TEST_LOG, "{}", printMoveVerbose(m));
//  }
//  ASSERT_EQ(21, moves.size());
//
//  fen      = "rnbqkbnr/p2ppppp/8/1Pp5/8/8/1PPPPPPP/RNBQKBNR w KQkq c6";
//  position = Position(fen);
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}", position.str());
//  moves.clear();
//  moves = *mg.generatePseudoLegalMoves<MoveGenerator::GENALL>(position);
//  LOG__DEBUG(Logger::get().TEST_LOG, "Moves = {}", moves.size());
//  for (Move m : moves) {
//    LOG__DEBUG(Logger::get().TEST_LOG, "{}", printMoveVerbose(m));
//  }
//  ASSERT_EQ(26, moves.size());
//}
//
//TEST_F(MoveGenTest, legalMoves) {
//  string        fen;
//  MoveGenerator mg;
//  MoveList      moves;
//  Position      position;
//
//  // Startpos
//  position = Position(START_POSITION_FEN);
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}", position.str());
//  moves.clear();
//  moves = *mg.generateLegalMoves<MoveGenerator::GENALL>(position);
//  LOG__DEBUG(Logger::get().TEST_LOG, "Moves = {}", moves.size());
//  for (Move m : moves) {
//    LOG__DEBUG(Logger::get().TEST_LOG, "{}", printMoveVerbose(m));
//  }
//  ASSERT_EQ(20, moves.size());
//
//  // 86 pseudo legal moves - 83 legal (incl. castling over attacked square)
//  fen      = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
//  position = Position(fen);
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}", position.str());
//  moves.clear();
//  moves = *mg.generateLegalMoves<MoveGenerator::GENALL>(position);
//  LOG__DEBUG(Logger::get().TEST_LOG, "Moves = {}", moves.size());
//  for (Move m : moves) {
//    LOG__DEBUG(Logger::get().TEST_LOG, "{}", printMoveVerbose(m));
//  }
//  ASSERT_EQ(83, moves.size());
//  ASSERT_FALSE(position.isLegalMove(createMove<CASTLING>(SQ_E8, SQ_G8)));
//}
//
//
//TEST_F(MoveGenTest, hasLegalMoves) {
//  Position position;
//  MoveGenerator mg;
//  MoveList      moves;
//
//  // check mate position
//  position = Position("rn2kbnr/pbpp1ppp/8/1p2p1q1/4K3/3P4/PPP1PPPP/RNBQ1BNR w kq -");
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}\n", position.str());
//  moves               = *mg.generateLegalMoves<MoveGenerator::GENALL>(position);
//  ASSERT_EQ(0, moves.size());
//  ASSERT_FALSE(mg.hasLegalMove(position));
//  ASSERT_TRUE(position.hasCheck());
//
//  // stale mate position
//  position = Position("7k/5K2/6Q1/8/8/8/8/8 b - -");
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}\n", position.str());
//  moves               = *mg.generateLegalMoves<MoveGenerator::GENALL>(position);
//  ASSERT_EQ(0, moves.size());
//  ASSERT_FALSE(mg.hasLegalMove(position));
//  ASSERT_FALSE(position.hasCheck());
//
//  // only en passant
//  position = Position("8/8/8/8/5Pp1/6P1/7k/K3BQ2 b - f3");
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}\n", position.str());
//  moves               = *mg.generateLegalMoves<MoveGenerator::GENALL>(position);
//  ASSERT_EQ(1, moves.size());
//  ASSERT_TRUE(mg.hasLegalMove(position));
//  ASSERT_FALSE(position.hasCheck());
//}
//
//TEST_F(MoveGenTest, validateMove) {
//  string        fen;
//  MoveGenerator mg;
//  MoveList      moves;
//
//  // 86 pseudo legal moves (incl. castling over attacked square)
//  fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
//  Position position(fen);
//
//  ASSERT_TRUE(mg.validateMove(position, createMove("b2e5")));
//  ASSERT_TRUE(mg.validateMove(position, createMove("e6e5")));
//  ASSERT_TRUE(mg.validateMove(position, createMove("c4e4")));
//  ASSERT_TRUE(mg.validateMove(position, createMove("c6e4")));
//  ASSERT_TRUE(mg.validateMove(position, createMove<PROMOTION>("a2a1q")));
//  ASSERT_TRUE(mg.validateMove(position, createMove<PROMOTION>("c2c1q")));
//  ASSERT_TRUE(mg.validateMove(position, createMove<PROMOTION>("a2a1n")));
//  ASSERT_TRUE(mg.validateMove(position, createMove<PROMOTION>("c2c1n")));
//  ASSERT_FALSE(mg.validateMove(position, createMove("e2e4")));
//  ASSERT_FALSE(mg.validateMove(position, createMove("b8c8")));
//  ASSERT_FALSE(mg.validateMove(position, createMove("a2b3")));
//  ASSERT_FALSE(mg.validateMove(position, createMove("b1c3")));
//  ASSERT_FALSE(mg.validateMove(position, MOVE_NONE));
//}
//
//TEST_F(MoveGenTest, onDemandGen) {
//  string        fen;
//  MoveGenerator mg;
//  MoveList      moves;
//
//  // 86 pseudo legal moves (incl. castling over attacked square)
//  fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
//  Position position(fen);
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}\n", position.str());
//
//  Move move;
//  int  counter = 0;
//  while ((move = mg.getNextPseudoLegalMove<MoveGenerator::GENALL>(position)) != MOVE_NONE) {
//    LOG__DEBUG(Logger::get().TEST_LOG, "{}", printMoveVerbose(move));
//    counter++;
//  }
//  LOG__DEBUG(Logger::get().TEST_LOG, "Moves = {}", counter);
//  ASSERT_EQ(86, counter);
//
//  // 218 moves
//  fen      = "R6R/3Q4/1Q4Q1/4Q3/2Q4Q/Q4Q2/pp1Q4/kBNN1KB1 w - - 0 1";
//  position = Position(fen);
//  LOG__DEBUG(Logger::get().TEST_LOG, "\n{}\n", position.str());
//  counter = 0;
//  while ((move = mg.getNextPseudoLegalMove<MoveGenerator::GENALL>(position)) != MOVE_NONE) {
//    LOG__DEBUG(Logger::get().TEST_LOG, "{}", printMoveVerbose(move));
//    counter++;
//  }
//  LOG__DEBUG(Logger::get().TEST_LOG, "Moves = {}", counter);
//  ASSERT_EQ(218, counter);
//}
//
//TEST_F(MoveGenTest, storeKiller) {
//  string        fen;
//  MoveGenerator mg;
//  MoveList      moves;
//
//  // 86 pseudo legal moves (incl. castling over attacked square)
//  fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
//  Position position(fen);
//
//  const MoveList* allMoves = mg.generatePseudoLegalMoves<MoveGenerator::GENNONCAP>(position);
//
//  // add first two killers
//  mg.storeKiller(allMoves->at(11), 2);
//  mg.storeKiller(allMoves->at(21), 2);
//  ASSERT_EQ(mg.maxNumberOfKiller, mg.killerMoves.size());
//  ASSERT_EQ(allMoves->at(11), mg.killerMoves.at(1));
//  ASSERT_EQ(allMoves->at(21), mg.killerMoves.at(0));
//
//  // add a killer already in the list - should not change
//  mg.storeKiller(allMoves->at(21), 2);
//  ASSERT_EQ(mg.maxNumberOfKiller, mg.killerMoves.size());
//  ASSERT_EQ(allMoves->at(21), mg.killerMoves.at(0));
//  ASSERT_EQ(allMoves->at(11), mg.killerMoves.at(1));
//
//  // add a killer NOT already in the list - should change
//  mg.storeKiller(allMoves->at(31), 2);
//  ASSERT_EQ(mg.maxNumberOfKiller, mg.killerMoves.size());
//  ASSERT_EQ(allMoves->at(31), mg.killerMoves.at(0));
//  ASSERT_EQ(allMoves->at(21), mg.killerMoves.at(1));
//
//  mg.reset();
//  ASSERT_EQ(0, mg.killerMoves.size());
//
//  // need to regenerate moves as reset has reset list
//  allMoves = mg.generatePseudoLegalMoves<MoveGenerator::GENNONCAP>(position);
//
//  // add a killer NOT already in the list - should change
//  mg.storeKiller(allMoves->at(31), 2);
//  ASSERT_EQ(1, mg.killerMoves.size());
//  ASSERT_EQ(allMoves->at(31), mg.killerMoves.at(0));
//}
//
//TEST_F(MoveGenTest, pushKiller) {
//  string        fen;
//  MoveGenerator mg;
//  MoveList      moves;
//
//  // 86 pseudo legal moves (incl. castling over attacked square)
//  fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3";
//  Position position(fen);
//
//  const MoveList* allMoves = mg.generatePseudoLegalMoves<MoveGenerator::GENALL>(position);
//  int             i        = 0;
//  LOG__DEBUG(Logger::get().TEST_LOG, "Moves w/o pushed killer:");
//  for (auto m : *allMoves) {
//    LOG__DEBUG(Logger::get().TEST_LOG, "{} {}", std::to_string(++i), printMoveVerbose(m));
//  }
//  ASSERT_EQ(86, i);
//  mg.storeKiller(allMoves->at(21), 2);
//  mg.storeKiller(allMoves->at(81), 2);
//  LOG__DEBUG(Logger::get().TEST_LOG, "Killer: {} {}",
//             printMove(allMoves->at(21)), printMove(allMoves->at(81)));
//
//  LOG__DEBUG(Logger::get().TEST_LOG, "Moves with pushed killer:");
//  Move move;
//  int  counter = 0;
//  while ((move = mg.getNextPseudoLegalMove<MoveGenerator::GENALL>(position)) != MOVE_NONE) {
//    if (counter == 18) {
//      EXPECT_EQ(moveOf(allMoves->at(21)), moveOf(move));
//      LOG__DEBUG(Logger::get().TEST_LOG, "Killer");
//    }
//    else if (counter == 33) {
//      EXPECT_EQ(moveOf(allMoves->at(81)), moveOf(move));
//      LOG__DEBUG(Logger::get().TEST_LOG, "Killer");
//    }
//    counter++;
//    LOG__DEBUG(Logger::get().TEST_LOG, "{} {}", std::to_string(counter), printMoveVerbose(move));
//  }
//  LOG__DEBUG(Logger::get().TEST_LOG, "Moves: {}", counter);
//  ASSERT_EQ(86, counter);
//}
//
//TEST_F(MoveGenTest, pvMove) {
//  string        fen;
//  MoveGenerator mg;
//  MoveList      moves;
//
//  // 86 pseudo legal moves (incl. castling over attacked square)
//  fen = "r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 w kq e3";
//  Position position(fen);
//
//  // Test #1: best move is capturing and generating all moves
//  Move pvMove = createMove("b1b2");
//  mg.setPV(pvMove);
//  Move move;
//  int  counter = 0;
//  // generate all moves
//  while ((move = mg.getNextPseudoLegalMove<MoveGenerator::GENALL>(position)) != MOVE_NONE) {
//    if (counter == 0) { // first move must be pv move
//      EXPECT_EQ(pvMove, move);
//    }
//    else { // no more pv move after first move
//      EXPECT_NE(pvMove, move);
//    }
//    counter++;
//  }
//  ASSERT_EQ(27, counter);
//  mg.resetOnDemand();
//
//  // Test #2: best move is capturing and generating capturing moves
//  pvMove = createMove("b1b2");
//  mg.setPV(pvMove);
//  counter = 0;
//  // generate all moves
//  while ((move = mg.getNextPseudoLegalMove<MoveGenerator::GENCAP>(position)) != MOVE_NONE) {
//    if (counter == 0) { // first move must be pv move
//      EXPECT_EQ(pvMove, move);
//    }
//    else { // no more pv move after first move
//      EXPECT_NE(pvMove, move);
//    }
//    counter++;
//  }
//  ASSERT_EQ(4, counter);
//  mg.resetOnDemand();
//
//  // Test #3: best move is non-capturing and generating all moves
//  pvMove = createMove("h2h3");
//  mg.setPV(pvMove);
//  counter = 0;
//  // generate all moves
//  while ((move = mg.getNextPseudoLegalMove<MoveGenerator::GENALL>(position)) != MOVE_NONE) {
//    if (counter == 0) { // first move must be pv move
//      EXPECT_EQ(pvMove, move);
//    }
//    else { // no more pv move after first move
//      EXPECT_NE(pvMove, move);
//    }
//    counter++;
//  }
//  ASSERT_EQ(27, counter);
//  mg.resetOnDemand();
//
//  // Test #4: best move is non-capturing and generating capturing moves
//  pvMove = createMove("h2h3");
//  mg.setPV(pvMove);
//  counter = 0;
//  // generate all moves
//  while ((move = mg.getNextPseudoLegalMove<MoveGenerator::GENCAP>(position)) != MOVE_NONE) {
//    if (counter == 0) { // first move can't be non capturing pv move
//      EXPECT_NE(pvMove, move);
//    }
//    else { // no more pv move after first move
//      EXPECT_NE(pvMove, move);
//    }
//    counter++;
//  }
//  ASSERT_EQ(4, counter);
//  mg.resetOnDemand();
//
//  // Test #4: best move is non-capturing and generating non-capturing moves
//  // not very relevant for searching
//  pvMove = createMove("h2h3");
//  mg.setPV(pvMove);
//  counter = 0;
//  // generate all moves
//  while ((move = mg.getNextPseudoLegalMove<MoveGenerator::GENNONCAP>(position)) != MOVE_NONE) {
//    if (counter == 0) { // first move must be pv move
//      EXPECT_EQ(pvMove, move);
//    }
//    else { // no more pv move after first move
//      EXPECT_NE(pvMove, move);
//    }
//    counter++;
//  }
//  ASSERT_EQ(23, counter);
//  mg.resetOnDemand();
//}

//
//TEST_F(MiscTest, moveFromUCI) {
//  Position position;
//  std::string moveStr;
//  Move expected;
//  Move actual;
//
//  position = Position("r1b1kb1r/1p3ppp/p1nppn2/q7/2BNP3/2N1B3/PPP2PPP/R2QK2R w KQkq -");
//  moveStr = "e1g1";
//  expected = createMove<CASTLING>(moveStr.c_str());
//  actual = Misc::getMoveFromUCI(position, moveStr);
//  LOG__DEBUG(Logger::get().TEST_LOG, "Expected move {} == Actual move {}", printMoveVerbose(expected), printMoveVerbose(actual));
//  EXPECT_EQ(expected, actual);
//
//  // promotion
//  position = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/6R1/pbp2PPP/1R4K1 b kq e3");
//  moveStr = "a2b1q";
//  expected = createMove<PROMOTION>(moveStr.c_str());
//  actual = Misc::getMoveFromUCI(position, moveStr);
//  LOG__DEBUG(Logger::get().TEST_LOG, "Expected move {} == Actual move {}", printMoveVerbose(expected), printMoveVerbose(actual));
//  EXPECT_EQ(expected, actual);
//}
//
//TEST_F(MiscTest, moveFromSAN) {
//  Position position;
//  Move expected;
//  Move actual;
//
//  expected = createMove("e2e4");
//  actual = Misc::getMoveFromSAN(position, "e4");
//  ASSERT_EQ(expected, actual);
//
//  position = Position("r1bqk2r/ppp2ppp/2np1n2/2b1p3/2B1P3/1P1P1N2/P1P2PPP/RNBQK2R w KQkq - 0 6");
//
//  // not a move on this position
//  expected = MOVE_NONE;
//  actual = Misc::getMoveFromSAN(position, "e4");
//  ASSERT_EQ(expected, actual);
//
//  // ambiguous
//  expected = MOVE_NONE;
//  actual = Misc::getMoveFromSAN(position, "d2");
//  ASSERT_EQ(expected, actual);
//
//  expected = createMove("d1d2");
//  actual = Misc::getMoveFromSAN(position, "Qd2");
//  ASSERT_EQ(expected, actual);
//
//  expected = createMove("e1d2");
//  actual = Misc::getMoveFromSAN(position, "Kd2");
//  ASSERT_EQ(expected, actual);
//
//  expected = createMove("c1d2");
//  actual = Misc::getMoveFromSAN(position, "Bd2");
//  ASSERT_EQ(expected, actual);
//
//  position = Position("r1bqk2r/p1p2pp1/1pnp1n1p/2b1p3/2B1P2N/1P1P4/P1PN1PPP/R1BQK2R w KQkq - 0 8");
//
//  // ambiguous
//  expected = createMove("f2f3");
//  actual = Misc::getMoveFromSAN(position, "f3");
//  ASSERT_EQ(expected, actual);
//
//  // ambiguous
//  expected = MOVE_NONE;
//  actual = Misc::getMoveFromSAN(position, "Nf3");
//  ASSERT_EQ(expected, actual);
//
//  // file disambiguation
//  expected = createMove("d2f3");
//  actual = Misc::getMoveFromSAN(position, "Ndf3");
//  ASSERT_EQ(expected, actual);
//
//  // file disambiguation
//  expected = createMove("h4f3");
//  actual = Misc::getMoveFromSAN(position, "Nhf3");
//  ASSERT_EQ(expected, actual);
//
//  position = Position("r3k2r/pbpq1pp1/1pnp1n1p/2b1pN2/2B1P3/1P1P1N2/P1P2PPP/R1BQK2R w KQkq - 4 10");
//
//  // pawn
//  expected = createMove("h2h4");
//  actual = Misc::getMoveFromSAN(position, "h4");
//  ASSERT_EQ(expected, actual);
//
//  // ambiguous
//  expected = MOVE_NONE;
//  actual = Misc::getMoveFromSAN(position, "Nh4");
//  ASSERT_EQ(expected, actual);
//
//  // rank disambiguation
//  expected = createMove("f3h4");
//  actual = Misc::getMoveFromSAN(position, "N3h4");
//  ASSERT_EQ(expected, actual);
//
//  // rank disambiguation
//  expected = createMove("f5h4");
//  actual = Misc::getMoveFromSAN(position, "N5h4");
//  ASSERT_EQ(expected, actual);
//
//  // castling white king side
//  expected = createMove<CASTLING>("e1g1");
//  actual = Misc::getMoveFromSAN(position, "O-O");
//  ASSERT_EQ(expected, actual);
//
//  position = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/6R1/pbp2PPP/1R4K1 b kq e3");
//
//  // promotion ambigous
//  expected = MOVE_NONE;
//  actual = Misc::getMoveFromSAN(position, "Qb1");
//  ASSERT_EQ(expected, actual);
//
//  // promotion
//  expected = createMove<PROMOTION>("a2b1q");
//  actual = Misc::getMoveFromSAN(position, "ab1=Q");
//  ASSERT_EQ(expected, actual);
//
//  // promotion & check
//  expected = createMove<PROMOTION>("a2b1q");
//  actual = Misc::getMoveFromSAN(position, "ab1=Q+");
//  ASSERT_EQ(expected, actual);
//
//  // en passant
//  expected = createMove<ENPASSANT>("f4e3");
//  actual = Misc::getMoveFromSAN(position, "e3");
//  ASSERT_EQ(expected, actual);
//
//  // capture sign
//  position = Position("7k/8/3p4/4N3/8/5p2/P7/1K2N3 w - -");
//  expected = createMove("e5f3");
//  actual = Misc::getMoveFromSAN(position, "N5xf3");
//  ASSERT_EQ(expected, actual);
//
//  // r7/2r1kpp1/1p6/pB1Pp1P1/Pbp1P3/2N2b1P/1PPK1P2/R6R b - - bm Bxh1; id "FRANKY-1 #11";
//  position = Position("r7/2r1kpp1/1p6/pB1Pp1P1/Pbp1P3/2N2b1P/1PPK1P2/R6R b - -");
//  expected = createMove("f3h1");
//  actual = Misc::getMoveFromSAN(position, "Bxh1");
//  ASSERT_EQ(expected, actual);
//
//  position = Position("r2qr1k1/pb2bp1p/1pn1p1pB/8/2BP4/P1P2N2/4QPPP/3R1RK1 w - - 0 1");
//  expected = createMove("d4d5");
//  actual = Misc::getMoveFromSAN(position, "d5");
//  ASSERT_EQ(expected, actual);
//
//  position = Position("rn1k1b1r/ppp2ppp/4bn2/4p1B1/4P3/2N5/PPP2PPP/R3KBNR w KQ -");
//  expected = createMove<CASTLING>("e1c1");
//  actual = Misc::getMoveFromSAN(position, "O-O-O+");
//  ASSERT_EQ(expected, actual);
//}
