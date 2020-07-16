/*
 * MIT License
 *
 * Copyright (c) 2020 Frank Kopp
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

#include "init.h"
#include "types/types.h"
#include "common/Logging.h"
#include "chesscore/Position.h"
#include "common/misc.h"
#include "engine/See.h"

#include <chesscore/MoveGenerator.h>
#include <gtest/gtest.h>
using testing::Eq;

class SeeTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(SeeTest, attacksTo) {
  Position position("2brr1k1/1pq1b1p1/p1np1p1p/P1p1p2n/1PNPPP2/2P1BNP1/4Q1BP/R2R2K1 w - -");
  Bitboard attacksTo = See::attacksTo(position, SQ_E5, WHITE);
  fprint("{}", strBoard(attacksTo));
  fprintln("{}", strGrouped(attacksTo));
  EXPECT_EQ(740294656ULL, attacksTo);

  attacksTo = See::attacksTo(position, SQ_E5, BLACK);
  fprint("{}", strBoard(attacksTo));
  fprintln("{}", strGrouped(attacksTo));
  EXPECT_EQ(48378511622144ULL, attacksTo);

  attacksTo = See::attacksTo(position, SQ_D4, WHITE);
  fprint("{}", strBoard(attacksTo));
  fprintln("{}", strGrouped(attacksTo));
  EXPECT_EQ(3407880ULL, attacksTo);

  attacksTo = See::attacksTo(position, SQ_D4, BLACK);
  fprint("{}", strBoard(attacksTo));
  fprintln("{}", strGrouped(attacksTo));
  EXPECT_EQ(4483945857024ULL, attacksTo);

  position  = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/6R1/pbp2PPP/1R4K1 b kq e3");
  attacksTo = See::attacksTo(position, SQ_E5, BLACK);
  fprint("{}", strBoard(attacksTo));
  fprintln("{}", strGrouped(attacksTo));
  EXPECT_EQ(2339760743907840ULL, attacksTo);

  attacksTo = See::attacksTo(position, SQ_A3, BLACK);
  fprint("{}", strBoard(attacksTo));
  fprintln("{}", strGrouped(attacksTo));
  EXPECT_EQ(72057594037928448ULL, attacksTo);
}

TEST_F(SeeTest, revealedAttacks) {
  // 1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - ; Nxe5?;
  Position position("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -");
  Bitboard occupiedBitboard = position.getOccupiedBb();

  const Square square = SQ_E5;

  Bitboard attacksTo = See::attacksTo(position, square, BLACK) | See::attacksTo(position, square, WHITE);
  fprintln("Direkt:");
  fprint("{}", strBoard(attacksTo));
  fprintln("{}", strGrouped(attacksTo));
  EXPECT_EQ(2286984186302464ULL, attacksTo);

  // take away bishop on f6
  attacksTo ^= Bitboards::sqBb[SQ_F6];       // reset bit in set to traverse
  occupiedBitboard ^= Bitboards::sqBb[SQ_F6];// reset bit in temporary occupancy (for x-Rays)

  attacksTo |= See::revealedAttacks(position, square, occupiedBitboard, BLACK) | See::revealedAttacks(position, square, occupiedBitboard, WHITE);

  fprintln("Revealed after removing bishop on f6:");
  fprint("{}", strBoard(attacksTo));
  fprintln("{}", strGrouped(attacksTo));
  EXPECT_EQ(9225623836668989440ULL, attacksTo);

  // take away rook on e2
  attacksTo ^= Bitboards::sqBb[SQ_E2];        // reset bit in set to traverse
  occupiedBitboard ^= Bitboards::sqBb[SQ_E2]; // reset bit in temporary occupancy (for x-Rays)

  attacksTo |= See::revealedAttacks(position, square, occupiedBitboard, BLACK)
               | See::revealedAttacks(position, square, occupiedBitboard, WHITE);

  fprintln("Revealed after removing rook on e2:");
  fprint("{}", strBoard(attacksTo));
  fprintln("{}", strGrouped(attacksTo));
  EXPECT_EQ(9225623836668985360ULL, attacksTo);
}

TEST_F(SeeTest, leastValuablePiece) {
  Position position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/6R1/pbp2PPP/1R4K1 b kq e3");
  Bitboard attacksTo = See::attacksTo(position, SQ_E5, BLACK);

  fprintln("All attackers");
  fprint("{}", strBoard(attacksTo));
  fprintln("{}", strGrouped(attacksTo));
  fprintln("{}", position.strBoard());

  Square lva = See::getLeastValuablePiece(position, attacksTo, BLACK);
  fprintln("Least valuable attacker: {}", str(lva));
  EXPECT_EQ(SQ_G6, lva);

  // remove the attacker
  attacksTo ^= lva;

  lva = See::getLeastValuablePiece(position, attacksTo, BLACK);
  fprintln("Least valuable attacker: {}", str(lva));
  EXPECT_EQ(SQ_D7, lva);

  // remove the attacker
  attacksTo ^= lva;

  lva = See::getLeastValuablePiece(position, attacksTo, BLACK);
  fprintln("Least valuable attacker: {}", str(lva));
  EXPECT_EQ(SQ_B2, lva);

  // remove the attacker
  attacksTo ^= lva;

  lva = See::getLeastValuablePiece(position, attacksTo, BLACK);
  fprintln("Least valuable attacker: {}", str(lva));
  EXPECT_EQ(SQ_E6, lva);

  // remove the attacker
  attacksTo ^= lva;

  lva = See::getLeastValuablePiece(position, attacksTo, BLACK);
  fprintln("Least valuable attacker: {}", str(lva));
  EXPECT_EQ(SQ_NONE, lva);
}

TEST_F(SeeTest, seeTest) {
  MoveGenerator mg;
  
  // 1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - ; Nxe5?
  Position position("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -");
  Move     move     = mg.getMoveFromUci(position, "d3e5");
  Value    seeScore = See::see(position, move);
  LOG__DEBUG(Logger::get().TEST_LOG, "See score = {}", seeScore);
  EXPECT_EQ(-220, seeScore);

  // 1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - - ; Rxe5?
  position = Position("1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -");
  move     = mg.getMoveFromUci(position, "e1e5");
  seeScore = See::see(position, move);
  LOG__DEBUG(Logger::get().TEST_LOG, "See score = {}", seeScore);
  EXPECT_EQ(100, seeScore);

  // 5q1k/8/8/8/RRQ2nrr/8/8/K7 w - - 0 1
  position = Position("5q1k/8/8/8/RRQ2nrr/8/8/K7 w - -");
  move     = mg.getMoveFromUci(position, "c4f4");
  seeScore = See::see(position, move);
  LOG__DEBUG(Logger::get().TEST_LOG, "See score = {}", seeScore);
  EXPECT_EQ(-580, seeScore);

  // k6q/3n1n2/3b4/4p3/3P1P2/3N1N2/8/K7 w - -
  position = Position("k6q/3n1n2/3b4/4p3/3P1P2/3N1N2/8/K7 w - -");
  move     = mg.getMoveFromUci(position, "d3e5");
  seeScore = See::see(position, move);
  LOG__DEBUG(Logger::get().TEST_LOG, "See score = {}", seeScore);
  EXPECT_EQ(100, seeScore);

  // r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/6R1/pbp2PPP/1R2R1K1 b kq e3 0 1
  position = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/6R1/pbp2PPP/1R2R1K1 b kq e3 0 1 ");
  move     = mg.getMoveFromUci(position, "a2b1Q");
  seeScore = See::see(position, move);
  LOG__DEBUG(Logger::get().TEST_LOG, "See score = {}", seeScore);
  EXPECT_EQ(500, seeScore);

}
