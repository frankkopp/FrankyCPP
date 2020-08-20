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

#include <gtest/gtest.h>
#include <random>
#include <thread>

#include "init.h"
#include "types/types.h"
#include "common/stringutil.h"
#include <chesscore/Position.h>

#include <chrono>
using namespace std::chrono;

class TimingTests : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }

protected:
  void SetUp() override {}
  void TearDown() override {}

  // Necessary because of function pointer use below.
  static void testTiming(std::ostringstream& os, int rounds, int iterations,
                  int repetitions, const std::vector<std::function<void(void)>>& tests);
};

TEST_F(TimingTests, DISABLED_popcount) {
  std::ostringstream os;

  //// TESTS START
  std::function<void()> f1 = []() { popcount(0b0010000000010000000000000010000000000000000000000000000000000000ULL); };
  std::vector<std::function<void()>> tests;
  tests.push_back(f1);
  //// TESTS END

  testTiming(os, 5, 50, 10'000'000, tests);

  std::cout << os.str();
}

TEST_F(TimingTests, DISABLED_distancevsdiff) {
  std::ostringstream os;

  //// TESTS START
  Position position("r3k2r/1ppqbppp/2n2n2/1B2p1B1/3p2b1/2NP1N2/1PPQPPPP/R3K2R w KQkq - 0 1");

  volatile bool t = false;

  std::function<void()> f1 = [&]() {
    t = distance(SQ_E2, SQ_E4) == 2;
  };
  std::function<void()> f2 = [&]() {
    t = std::abs(static_cast<int>(SQ_E2) - static_cast<int>(SQ_E4)) == 16;
  };
  std::vector<std::function<void()>> tests;
  tests.push_back(f1);
  tests.push_back(f2);
  //// TESTS END

  testTiming(os, 5, 10, 10'000'000, tests);

  std::cout << os.str();
}

/**
 * Test the absolute speed of doMove, undoMove
 */
TEST_F(TimingTests, DISABLED_doMoveUndoMove) {
  std::ostringstream os;

  //// TESTS START
  // position for each move type
  // fxe3 enpassant
  // fxe3 normal capture
  // o-o castling
  // Rc1 normal non capturing
  // c1Q promotion
  Position position("r3k2r/1ppn3p/4q1n1/8/4Pp2/3R4/p1p2PPP/R5K1 b kq e3 0 1");
  const Move move1 = createMove(SQ_F4, SQ_E3, ENPASSANT);
  const Move move2 = createMove(SQ_F2, SQ_E3);
  const Move move3 = createMove(SQ_E8, SQ_G8, CASTLING);
  const Move move4 = createMove(SQ_D3, SQ_C3);
  const Move move5 = createMove(SQ_C2, SQ_C1, PROMOTION, QUEEN);

  std::function<void()> f1 = [&]() {
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
  };

  std::vector<std::function<void()>> tests;
  tests.push_back(f1);
  //// TESTS END

  testTiming(os, 5, 1, 20'000'000, tests);

  std::cout << os.str();
}

TEST_F(TimingTests, trimWhiteSpace) {
  std::ostringstream os;

  //// TESTS START

  const std::string line = " \t This is a text. This is a text. This is a text. This is a text.\t  \r\n";
  const std::string_view lineView{line};

  fprintln("Original line:      '{}'", line);
  fprintln("Original line view: '{}'", lineView);

  NEWLINE;

  int counter = 0;

//  // regex
//  std::string trimmedLineRegex{};
//  std::function<void()> f1 = [&]() {
//    trimmedLineRegex = trimRegex(line);
//    counter++;
//  };
//
//  std::string trimmedLineViewRegex{};
//  std::function<void()> f2 = [&]() {
//    trimmedLineViewRegex = trimRegex(lineView);
//    counter++;
//  };
//
//  // find_first_not_of
//  std::string trimmedLineFindNot{};
//  std::function<void()> f3 = [&]() {
//    trimmedLineFindNot = trimFindNot(line);
//    counter++;
//  };
//
//  // find_if
//  std::string trimmedLineFindIf{};
//  std::string lineCopy{line};
//  std::function<void()> f4 = [&]() {
//    trimmedLineFindIf = trimFindIf(lineCopy);
//    counter++;
//  };

  // while
  std::string trimmedLineWhile{};
  std::function<void()> f5 = [&]() {
    trimmedLineWhile = trimFast(line);
    counter++;
  };

  // while
  std::string_view trimmedLineViewWhile{};
  std::function<void()> f6 = [&]() {
    trimmedLineViewWhile = trimFast(lineView);
    counter++;
  };

  std::vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  tests.push_back(f2);
//  tests.push_back(f3);
//  tests.push_back(f4);
  tests.push_back(f5);
  tests.push_back(f6);
  //// TESTS END

  testTiming(os, 5, 10, 10'000, tests);

  NEWLINE;

//  fprintln("trimmedLineRegex:     '{}'", trimmedLineRegex);
//  fprintln("trimmedLineViewRegex: '{}'", trimmedLineViewRegex);
//  fprintln("trimmedLineFindNot:   '{}'", trimmedLineFindNot);
//  fprintln("trimmedLineFindIf:    '{}'", trimmedLineFindIf);
  fprintln("trimmedLineWhile:     '{}'", trimmedLineWhile);
  fprintln("trimmedLineViewWhile: '{}'", trimmedLineViewWhile);
  fprintln("counter: {:L}", counter);

  std::cout << os.str();
}

TEST_F(TimingTests, illegalCharacter) {
  std::ostringstream os;

  //// TESTS START

  const std::string fen = "r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1";
  static const std::regex illegalInFenPosition(R"([^1-8pPnNbBrRqQkK/]+)");
  static const std::string allowedChars{"12345678pPnNbBrRqQkK/"};

  NEWLINE;

  int counter1 = 0;
  int counter2 = 0;

  // regex
  std::string trimmedLineRegex{};
  std::function<void()> f1 = [&]() {
    if (!std::regex_search(fen, illegalInFenPosition)) {
      counter1++;
    }
  };

  std::string trimmedLineViewRegex{};
  std::function<void()> f2 = [&]() {
    bool illegalFound = false;
    const auto l = fen.length();
    for (int i = 0; i < l; i++) {
      if (allowedChars.find(fen[i]) == std::string::npos) {
        illegalFound = true;
        break;
      }
    }
    if (!illegalFound) {
      counter2++;
    }
  };

  std::vector<std::function<void()>> tests;
  tests.push_back(f1);
  tests.push_back(f2);

  //// TESTS END

  testTiming(os, 5, 10, 10'000, tests);

  NEWLINE;

  fprintln("Counter 1: {:L}", counter1);
  fprintln("Counter 2: {:L}", counter2);

  std::cout << os.str();
}

///**
// * Test difference for getMoves with pre rotated bb vs. on-the-fly rotated bb
// * Round  5 Test  1:  451.076.050 ns (  0,45107605 sec)
// * Round  5 Test  2:   17.723.886 ns ( 0,017723886 sec)
// */
//TEST_F(TimingTests, DISABLED_rotation) {
//  ostringstream os;
//
//  //// TESTS START
//  Position position(
//      "r3k2r/1ppqbppp/2n2n2/1B2p1B1/3p2b1/2NP1N2/1PPQPPPP/R3K2R w KQkq - 0 1");
//
//  std::function<void()> f1 = [&]() {
//    Bitboards::getMovesDiagUp(SQ_D2, position.getOccupiedBB());
//  };
//  std::function<void()> f2 = [&]() {
//    Bitboards::getMovesDiagUpR(SQ_D2, position.getOccupiedBBR45());
//  };
//  vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  tests.push_back(f2);
//  //// TESTS END
//
//  testTiming(os, 5, 50, 10'000'000, tests);
//
//  cout << os.str();
//}
//
//std::array<Value, 32> gain1{};
//
//TEST_F(TimingTests, DISABLED_gain_array) {
//  ostringstream os;
//
//  //// TESTS START
//  std::function<void()> f1 = [&]() {
//    gain1.fill(Value{ 0 });
//  };
//  std::function<void()> f2 = [&]() {
//    std::array<Value, 32> gain2{};
//  };
//  vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  tests.push_back(f2);
//  //// TESTS END
//
//  testTiming(os, 10, 100, 10'000'000, tests);
//
//  cout << os.str();
//}
//
//TEST_F(TimingTests, DISABLED_TThash) {
//  ostringstream os;
//
//  uint64_t*                                         data1 = new uint64_t[2'500'000];
//  uint64_t*                                         data2 = new uint64_t[2'500'000];
//  std::mt19937_64                                   eng1(12345);
//  std::mt19937_64                                   eng2(12345);
//  std::uniform_int_distribution<unsigned long long> distr1;
//  std::uniform_int_distribution<unsigned long long> distr2;
//
//  //// TESTS START
//  std::function<void()> f1 = [&]() {
//    data1[distr1(eng1) % 2'000'000] = std::chrono::high_resolution_clock::now().time_since_epoch().count();
//  };
//  std::function<void()> f2 = [&]() {
//    data2[distr2(eng2) & 2'097'151] = std::chrono::high_resolution_clock::now().time_since_epoch().count();
//  };
//  vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  tests.push_back(f2);
//  //// TESTS END
//
//  testTiming(os, 5, 50, 1'000'000, tests);
//
//  cout << os.str();
//
//  delete[] data1;
//  delete[] data2;
//}
//
//unsigned myPopcount16(unsigned u) {
//  u -= (u >> 1U) & 0x5555U;
//  u = ((u >> 2U) & 0x3333U) + (u & 0x3333U);
//  u = ((u >> 4U) + u) & 0x0F0FU;
//  return (u * 0x0101U) >> 8U;
//}
//
//TEST_F(TimingTests, DISABLED_busyWait) {
//  ostringstream os;
//  //// TESTS START
//  std::function<void()> f1 = [&]() {
//    int i = 0;
//    while (i++ < 10'000) {
//      std::this_thread::sleep_for(chrono::milliseconds(1));
//    }
//  };
//  vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  //// TESTS END
//  testTiming(os, 3, 1, 1, tests);
//  cout << os.str();
//}
//
////#pragma clang diagnostic push
////#pragma clang diagnostic ignored "-Wunused-variable"
////TEST_F(TimingTests, DISABLED_moveUnion) {
////  ostringstream os;
////
////  const Square from = SQ_A2;
////  const Square to = SQ_A1;
////  const PieceType promPT = BISHOP;
////  const MoveType type = PROMOTION;
////  const Value value = static_cast<Value>(999);
////
////  union ValuedMove {
////    uint32_t all;
////    struct {
////      union {
////        uint16_t all;
////        struct {
////          uint8_t from : 6;
////          uint8_t to : 6;
////          uint8_t promType : 2;
////          uint8_t moveType : 2;
////        } __attribute__((packed)) data;
////      } __attribute__((packed)) move;
////      int16_t value;
////    } __attribute__((packed)) moveValue;
////  } __attribute__((packed));
////
////  fprintln("Size Move {} \nSize NewMove {}", sizeof(Move), sizeof(ValuedMove));
////  fprintln("{}", sizeof(ValuedMove::moveValue));
////  fprintln("{}", sizeof(ValuedMove::moveValue.move));
////  fprintln("{}", sizeof(ValuedMove::moveValue.value));
////
////  ValuedMove newMove = {};
////  newMove.moveValue.move.data.from = from;
////  newMove.moveValue.move.data.to = to;
////  newMove.moveValue.move.data.promType = promPT - 3;
////  newMove.moveValue.move.data.moveType = type >> MoveShifts::TYPE_SHIFT;
////  newMove.moveValue.value = value;
////  fprintln("{} {}", static_cast<uint32_t>(newMove.all),
////           printBitString(static_cast<uint32_t>(newMove.all)));
////
////  //// TESTS START
////  std::function<void()> f1 = [&]() {
////    Move m = createMove<PROMOTION>(from, to, value, promPT);
////    ASSERT_EQ(from, getFromSquare(m));
////    ASSERT_EQ(to, getToSquare(m));
////    ASSERT_EQ(promPT, promotionType(m));
////    ASSERT_EQ(type, typeOf(m));
////    ASSERT_EQ(value, valueOf(m));
////  };
////  std::function<void()> f2 = [&]() {
////    ValuedMove n = {};
////    n.moveValue.move.data.from = from;
////    n.moveValue.move.data.to = to;
////    n.moveValue.move.data.promType = promPT - 3;
////    n.moveValue.move.data.moveType = type >> MoveShifts::TYPE_SHIFT;
////    n.moveValue.value = value;
////    ASSERT_EQ(from, n.moveValue.move.data.from);
////    ASSERT_EQ(to, n.moveValue.move.data.to);
////    ASSERT_EQ(promPT, n.moveValue.move.data.promType + 3);
////    ASSERT_EQ(type, n.moveValue.move.data.moveType << MoveShifts::TYPE_SHIFT);
////    ASSERT_EQ(value, n.moveValue.value);
////  };
////  vector<std::function<void()>> tests;
////  tests.push_back(f1);
////  tests.push_back(f2);
////  //// TESTS END
////  testTiming(os, 5, 100, 1'000'000, tests);
////  cout << os.str();
////}
////
////#pragma clang diagnostic pop
//
//TEST_F(TimingTests, DISABLED_bitCount) {
//  ostringstream os;
//
//  std::mt19937_64                                   rg(12345);
//  std::uniform_int_distribution<unsigned long long> randomU64;
//
//  int i;
//
//  uint8_t PopCnt16[1 << 16];
//  // pre-computes 16-bit population counter to use in popcount(64-bit)
//  for (i = 0; i < (1 << 16); ++i)
//    PopCnt16[i] = static_cast<uint8_t>(myPopcount16(i));
//
//  //// TESTS START
//  std::function<void()> f1 = [&]() {
//    union {
//      Bitboard bb;
//      uint16_t u[4];
//    } v = { randomU64(rg) };
//    i   = PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]];
//  };
//  std::function<void()>         f2 = [&]() { i = __builtin_popcountll(randomU64(rg)); };
//  vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  tests.push_back(f2);
//  //// TESTS END
//
//  testTiming(os, 5, 50, 10'000'000, tests);
//
//  cout << os.str();
//}
//
//TEST_F(TimingTests, DISABLED_popLSB) {
//  ostringstream os;
//
//  std::mt19937_64                                   rg(12345);
//  std::uniform_int_distribution<unsigned long long> randomU64;
//
//  Square result;
//
//  //// TESTS START
//  std::function<void()> f1 = [&]() {
//    Bitboard b = randomU64(rg);
//    result     = Bitboards::popLSB(b);
//  };
//  std::function<void()> f2 = [&]() {
//    Bitboard b = randomU64(rg);
//    Bitboards::popLSB2(b, result);
//  };
//  vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  tests.push_back(f2);
//  //// TESTS END
//
//  testTiming(os, 5, 50, 50'000'000, tests);
//
//  cout << os.str();
//}
//
//TEST_F(TimingTests, DISABLED_max) {
//  ostringstream os;
//
//  std::mt19937_64                                   rg(12345);
//  std::uniform_int_distribution<unsigned long long> randomU64;
//
//  int alpha     = 1000;
//  int ply       = 5;
//  int globalVal = -10000;
//
//  //// TESTS START
//  std::function<void()> f1 = [&]() {
//    using namespace std;
//    alpha = min(globalVal + ply, alpha);
//  };
//  std::function<void()> f2 = [&]() {
//    if (alpha > globalVal + ply)
//      alpha = globalVal + ply;
//  };
//  vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  tests.push_back(f2);
//  //// TESTS END
//
//  testTiming(os, 5, 50, 30'000'000, tests);
//
//  cout << os.str();
//}
//
//TEST_F(TimingTests, DISABLED_SquareTest) {
//  ostringstream os;
//
//  Square sq = static_cast<Square>(65);
//  bool   result1, result2;
//
//  //// TESTS START
//  std::function<void()> f1 = [&]() {
//    result1 = sq >= SQ_A1 && sq <= SQ_H8;
//  };
//  std::function<void()> f2 = [&]() {
//    result2 = !(sq & ~0b11'1111);
//  };
//  vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  tests.push_back(f2);
//  //// TESTS END
//
//  testTiming(os, 5, 50, 10'000'000, tests);
//
//  ASSERT_EQ(result1, result2);
//
//  cout << os.str();
//}
//
//TEST_F(TimingTests, DequeSortVsArray) {
//  ostringstream os;
//
//  std::mt19937_64                                   rg(12345);
//  std::uniform_int_distribution<unsigned long long> randomU64;
//
//  const int items = 75;
//  std::vector<Move> movesVector;
//  std::deque<Move>  movesDeque;
//
//  // fill array and deque
//  for (int i = 0; i < items; ++i) {
//    Move m        = Move(randomU64(rg));
//    movesVector.push_back(m);
//    movesDeque.push_back(m);
//  }
//
//  bool reverse = false;
//
//  //// TESTS START
//  std::function<void()>         f1 = [&]() {
//    reverse ? std::stable_sort(movesVector.begin(), movesVector.end(), less<Move>())
//            : std::stable_sort(movesVector.begin(), movesVector.end(), greater<Move>());
//    reverse = !reverse;
//  };
//  std::function<void()>         f2 = [&]() {
//    reverse ? std::stable_sort(movesDeque.begin(), movesDeque.end(), less<Move>())
//            : std::stable_sort(movesDeque.begin(), movesDeque.end(), greater<Move>());
//    reverse = !reverse;
//  };
//  vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  tests.push_back(f2);
//  //// TESTS END
//
//  testTiming(os, 5, 100, 10'000, tests);
//
////  Move tmp = movesArray[0];
////  for (int i = 0; i < items; ++i) {
//////    fprintln("{}", movesArray[i]);
////    EXPECT_LE(tmp, movesArray[i]);
////    tmp = movesArray[i];
////  }
////  NEWLINE;
////  tmp = movesList[0];
////  for (int i = 0; i < items; ++i) {
//////    fprintln("{}", movesList[i]);
////    EXPECT_LE(tmp, movesList[i]);
////    tmp = movesList[i];
////  }
//
//  cout << os.str();
//}
//
//TEST_F(TimingTests, StableSortVsSort) {
//  ostringstream os;
//
//  std::mt19937_64                                   rg(12345);
//  std::uniform_int_distribution<unsigned long long> randomU64;
//
//  const int items = 75;
//  std::vector<Move> movesVector;
//  std::deque<Move>  movesDeque;
//
//  // fill array and deque
//  for (int i = 0; i < items; ++i) {
//    Move m        = Move(randomU64(rg));
//    movesVector.push_back(m);
//    movesDeque.push_back(m);
//  }
//
//  bool reverse = false;
//
//  //// TESTS START
//  std::function<void()>         f1 = [&]() {
//    reverse ? std::sort(movesVector.begin(), movesVector.end(), less())
//            : std::sort(movesVector.begin(), movesVector.end(), greater());
//    reverse = !reverse;
//  };
//  std::function<void()>         f2 = [&]() {
//    reverse ? std::stable_sort(movesVector.begin(), movesVector.end(), less())
//            : std::stable_sort(movesVector.begin(), movesVector.end(), greater());
//    reverse = !reverse;
//  };
//  vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  tests.push_back(f2);
//  //// TESTS END
//
//  testTiming(os, 5, 100, 10'000, tests);
//
////  Move tmp = movesArray[0];
////  for (int i = 0; i < items; ++i) {
//////    fprintln("{}", movesArray[i]);
////    EXPECT_LE(tmp, movesArray[i]);
////    tmp = movesArray[i];
////  }
////  NEWLINE;
////  tmp = movesList[0];
////  for (int i = 0; i < items; ++i) {
//////    fprintln("{}", movesList[i]);
////    EXPECT_LE(tmp, movesList[i]);
////    tmp = movesList[i];
////  }
//
//  cout << os.str();
//}
//
//TEST_F(TimingTests, DISABLED_Skeleton) {
//  ostringstream os;
//
//  std::mt19937_64                                   rg(12345);
//  std::uniform_int_distribution<unsigned long long> randomU64;
//
//  //// TESTS START
//  std::function<void()>         f1 = [&]() {};
//  std::function<void()>         f2 = [&]() {};
//  vector<std::function<void()>> tests;
//  tests.push_back(f1);
//  tests.push_back(f2);
//  //// TESTS END
//
//  testTiming(os, 5, 50, 10'000'000, tests);
//
//  cout << os.str();
//}

void TimingTests::testTiming(std::ostringstream& os, int rounds, int iterations,
                             int repetitions, const std::vector<std::function<void()>>& tests) {
  std::cout.imbue(deLocale);
  os.imbue(deLocale);
  os << std::setprecision(9);

  os << std::endl;
  os << "Starting timing test: rounds=" << rounds
     << " iterations=" << iterations << " repetitions=" << repetitions << std::endl;
  os << "======================================================================"
     << std::endl;

  auto startTime = high_resolution_clock::now();
  nanoseconds last(0);

  // rounds
  for (int round = 1; round <= rounds; ++round) {
    std::cout << "Round " << round << " of " << rounds << " timing tests." << std::endl;

    nanoseconds accDuration(0);

    // tests
    int testNr = 1;
    for (auto f : tests) {
      // iterations
      int i = 0;

      while (i++ < iterations) {
        // repetitions
        startTime = high_resolution_clock::now();
        for (int j = 0; j < repetitions; ++j)
          f();
        accDuration += duration_cast<nanoseconds>(high_resolution_clock::now() - startTime);
      }

      const nanoseconds cpuTime = accDuration;
      const nanoseconds avgCpu  = cpuTime / iterations;
      uint64_t percentFromLast  = last.count() ? (avgCpu * 10'000) / last : 10'000;

      os << "Round " << std::setfill(' ') << std::setw(2) << round << " Test "
         << std::setw(2) << testNr++ << ": " << std::setfill(' ') << std::setw(12)
         << avgCpu.count() << " ns"
         << " (" << std::setfill(' ') << std::setw(6) << (percentFromLast / 100)
         << "%)"
         << " (" << std::setfill(' ') << std::setw(12) << (avgCpu.count() / 1e9) << " sec)"
         << " (" << std::setfill(' ') << std::setw(12)
         << static_cast<double>(avgCpu.count()) / (repetitions * iterations)
         << " ns avg per test)"
         << std::endl;

      last = avgCpu;
      accDuration = nanoseconds(0);
    }
    os << std::endl;
    last = nanoseconds(0);
  }
}
