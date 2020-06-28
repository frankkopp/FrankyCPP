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

#include "chesscore/MoveGenerator.h"
#include "chesscore/Perft.h"
#include "chesscore/Position.h"
#include "init.h"
#include "types/types.h"

#include <chrono>
#include <gtest/gtest.h>
#include <ostream>
#include <string>

using namespace std::chrono;
using namespace std;
using testing::Eq;

class SpeedTests : public ::testing::Test {
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

// /////////////////////
// TIMING POSITION
// /////////////////////
TEST_F(SpeedTests, TimingDoMoveUndoMove) {

  const int rounds     = 5;
  const int iterations = 50'000'000;

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

// /////////////////////
// TIMING MOVEGEN
// /////////////////////
TEST_F(SpeedTests, onDemandPseudoMoveGen) {
  MoveGenerator mg;

  const int rounds     = 5;
  const int iterations = 1'000'000;

  Position position = Position("r3k2r/1ppn3p/2q1q1n1/4P3/2q1Pp2/B5R1/pbp2PPP/1R4K1 b kq e3");
  auto k1           = mg.getMoveFromUci(position, "g6h4");
  auto k2           = mg.getMoveFromUci(position, "b7b6");
  auto pv           = mg.getMoveFromUci(position, "a2b1Q");

  uint64_t generated = 0;
  Move move;
  for (int r = 1; r <= rounds; r++) {
    fprintln("Round {}", r);
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
      generated = 0;
      mg.resetOnDemand();
      mg.storeKiller(k1);
      mg.storeKiller(k2);
      mg.setPV(pv);
      while ((move = mg.getNextPseudoLegalMove(position, GenAll)) != MOVE_NONE) {
        generated++;
      }
    }
    auto elapsed = duration_cast<nanoseconds>(high_resolution_clock::now() - start);

    std::ostringstream os;
    os.flags(std::cout.flags());
    os.imbue(deLocale);
    os.precision(os.precision());
    os << "Test took " << elapsed.count() << " ns for " << iterations << " iterations" << std::endl;
    os << "Test took " << elapsed.count() / iterations << " ns per test" << std::endl;
    os << "Test per sec " << (iterations * nanoPerSec) / elapsed.count() << " tps" << std::endl;
    os << generated * iterations << " moves generated: " << (generated * iterations * nanoPerSec) / elapsed.count() << " mps" << std::endl;
    std::cout << os.str() << std::endl;
  }
}

// /////////////////////
// TIMING PERFT
// /////////////////////
TEST_F(SpeedTests, stdPerftOD) {
  MoveGenerator mg;
  Position position;
  Perft p;

  cout << "Standard PERFT OnDemand Test" << endl;
  cout << "==============================" << endl;

  // @formatter:off
  const uint64_t results[10][8] = {
    //N                 Nodes            Captures              EP             Checks              Mates           Castles      Promotions
    { 0,                 1ULL,               0ULL,           0ULL,              0ULL,              0ULL,             0ULL ,          0ULL },
    { 1,                20ULL,               0ULL,           0ULL,              0ULL,              0ULL,             0ULL ,          0ULL },
    { 2,               400ULL,               0ULL,           0ULL,              0ULL,              0ULL,             0ULL ,          0ULL },
    { 3,             8'902ULL,              34ULL,           0ULL,             12ULL,              0ULL,             0ULL ,          0ULL },
    { 4,           197'281ULL,           1'576ULL,           0ULL,            469ULL,              8ULL,             0ULL ,          0ULL },
    { 5,         4'865'609ULL,          82'719ULL,         258ULL,         27'351ULL,            347ULL,             0ULL ,          0ULL },
    { 6,       119'060'324ULL,       2'812'008ULL,       5'248ULL,        809'099ULL,         10'828ULL,             0ULL ,          0ULL },
    { 7,     3'195'901'860ULL,     108'329'926ULL,     319'617ULL,     33'103'848ULL,        435'767ULL,       883'453ULL ,          0ULL },
    { 8,    84'998'978'956ULL,   3'523'740'106ULL,   7'187'977ULL,    968'981'593ULL,      9'852'036ULL,    23'605'205ULL ,          0ULL },
    { 9, 2'439'530'234'167ULL, 125'208'536'153ULL, 319'496'827ULL, 36'095'901'903ULL,    400'191'963ULL, 1'784'356'000ULL , 17'334'376ULL }
  };
  // @formatter:on

  const int startDepth = 1;
  const int maxDepth   = 7;

  for (int i = startDepth; i <= maxDepth; i++) {
    p.perft(i);
    NEWLINE;
    EXPECT_EQ(results[i][1], p.getNodes());
    EXPECT_EQ(results[i][2], p.getCaptureCounter());
    EXPECT_EQ(results[i][3], p.getEnpassantCounter());
    EXPECT_EQ(results[i][4], p.getCheckCounter());
    EXPECT_EQ(results[i][5], p.getCheckMateCounter());
    EXPECT_EQ(results[i][6], p.getCastleCounter());
    EXPECT_EQ(results[i][7], p.getPromotionCounter());
  }
  cout << "==============================" << endl;
}
