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

#include "common/stringutil.h"
#include "init.h"
#include "types/types.h"
#include <chesscore/Position.h>

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <random>
#include <regex>
#include <thread>
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

TEST_F(TimingTests, DISABLED_trimWhiteSpace) {
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

TEST_F(TimingTests, DISABLED_illegalCharacter) {
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
    const auto l      = fen.length();
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

TEST_F(TimingTests, split) {
  std::ostringstream os;

  //// TESTS START

  const std::string line = "1. e4 e6 2. d4 d5 3. Nd2 Nc6 4. Ngf3 Nf6 5. e5 Nd7 6. g3 Be7 7. Bh3 b6 8. O-O "
                           "Bb7 9. c3 h5 10. Qe2 Nf8 11. b4 a5 12. b5 Na7 13. a4 c6 14. Ba3 cxb5 15. Bxe7 "
                           "Qxe7 16. axb5 g5 17. Bg2 Ng6 18. Rab1 h4 19. Qe3 g4 20. Ne1 Rc8 21. c4 Qf8 22. "
                           "Nd3 Rc7 23. c5 Nc8 24. c6 Ba8 25. f3 hxg3 26. hxg3 gxf3 27. Rxf3 Nce7 28. Rbf1 "
                           "Rh7 29. Rf6 Qh8 30. Qg5 Bxc6 31. bxc6 Rxc6 32. Nf4 Nxf4 33. Qxf4 Ng6 34. Qg4 "
                           "Ne7 35. Qe2 Qg8 36. Qd3 Rg7 37. R1f3 Qh8 38. Qf1 Qg8 39. Qd3 Qh8 40. Qf1 Qg8 "
                           "41. Qf2 Ng6 42. Nf1 Ke7 43. Qd2 Nf8 44. Rc3 Rxc3 45. Qxc3 Nd7 46. Qa3+ Kd8 47. "
                           "Rf3 Qf8 48. Qa4 Qb4 49. Qxb4 axb4 50. Bh3 Nb8 51. Nd2 Nc6 52. Nb3 Ke7 53. Kf2 "
                           "Rg8 54. Bf1 Rc8 55. Ke1 Na5 56. Kd2 Nc4+ 57. Ke1 Ra8 58. Nc1 Na5 59. Kd1 Rg8 "
                           "60. Bb5 Rh8 61. Be2 Rh2 62. Ke1 Rh1+ 63. Bf1 Rh8 64. Nb3 Rc8 65. Be2 Nc4 66. "
                           "Nd2 Na5 67. Kd1 Rh8 68. Rf2 Rh3 69. Nf1 Rh8 70. Nd2 Rh3 71. Rf3 Rh2 72. Rd3 "
                           "Rh1+ 73. Nf1 Rh8 74. Ke1 Rc8 75. Nd2 Rh8 76. Bf3 Rh2 77. Kf1 Rh8 78. Kg1 Rc8 "
                           "79. Bh5 Rc2 80. Kf1 Nc4 81. Nb3 Rb2 82. Kg1 Na5 83. Bd1 Rb1 84. Nxa5 bxa5 85. "
                           "Kf2 Ra1 86. Ke2 a4 87. Kd2 b3 88. Kc3 Rc1+ 89. Kb2 Rc4 90. Rd2 Rc8 91. Be2 Rh8 "
                           "92. Bb5 Rg8 93. Rd3 Ra8 94. Rd2 Rg8 95. Bxa4 Rxg3 96. Bxb3 f6 97. exf6+ Kxf6 "
                           "98. Rf2+ Ke7 99. Kc2 Re3 100. Rh2 e5 101. dxe5 Rxe5 102. Kd3 Kd6 103. Bc2 Rg5 "
                           "104. Rh8 Rg3+ 105. Kd4 Rg4+ 106. Kc3 Rc4+ 107. Kb3 Rg4 108. Rd8+ Kc5 109. Rc8+ "
                           "Kd6 110. Bh7 Rc4 111. Rd8+ Kc5 112. Bg8 Rb4+ 113. Ka3 Rd4 114. Kb3 Rb4+ 115. "
                           "Kc3 Rc4+ 116. Kd3 Rd4+ 117. Ke3 Re4+ 118. Kf3 Re5 119. Kf4 Rh5 120. Bf7 Rh4+ "
                           "121. Ke5 Re4+ 122. Kf5 Rd4 123. Be6 Rd1 124. Ke5 Re1+ 125. Kf6 Rd1 126. Ke7 Rd2 "
                           "127. Rc8+ Kd4 128. Bxd5 Rh2 129. Kd6 Rh6+ 130. Be6 Rg6 131. Rc4+ Kd3 132. Rh4 "
                           "Kc3 133. Re4 Kd3 134. Rh4 Kc3 135. Ke5 Rg5+ 136. Bf5 Kb3 137. Rf4 Kc3 138. Kd5 "
                           "Rg7 139. Rc4+ Kd2 140. Kd4 Ke2 141. Ke4 Re7+ 142. Kf4 Rf7 143. Rc2+ Kd1 144. "
                           "Ra2 Kc1 145. Ke4 Rf8 146. Rf2 Rd8 147. Ke3 Re8+ 148. Be4 Rc8 149. Bd3 Re8+ 150. "
                           "Be4 Rc8 151. Rf1+ Kb2 152. Rb1+ Ka3 153. Bd3 Re8+ 154. Be4 Rc8 155. Rf1 Kb2 "
                           "156. Rb1+ Ka3 157. Kd4 Ka4 158. Bf5 Rh8 159. Be6 Rh6 160. Kc4 Rh4+ 161. Kc5 "
                           "Rh5+ 162. Bd5 Ka3 163. Rb3+ Ka4 164. Rg3 Re5 165. Rg2 Re3 166. Kc4 Ka5 167. Rg6 "
                           "Re1 168. Kc5 Rb1 169. Bc4 Rb5+ 170. Kd4 1/2-1/2";
  const std::string_view lineView{line};

  NEWLINE;

  std::vector<std::string> splitStringParts{};
  std::function<void()> f1 = [&]() {
    splitStringParts.clear();
    splitFast(line, splitStringParts, " ");
  };

  //  std::vector<std::string> splitStringParts2{};
  //  std::function<void()> f2 = [&]() {
  //    splitStringParts2.clear();
  //    split(line, splitStringParts2, ' ');
  //  };

  std::vector<std::string_view> splitViewParts{};
  std::function<void()> f3 = [&]() {
    splitViewParts.clear();
    splitFast(lineView, splitViewParts, " ");
  };

  std::vector<std::function<void()>> tests;
  tests.push_back(f1);
  //    tests.push_back(f2);
  tests.push_back(f3);
  //// TESTS END

  testTiming(os, 5, 10, 10'000, tests);

  NEWLINE;

  fprintln("Elements: {:L}", splitStringParts.size());
  //  fprintln("Elements: {:L}", splitStringParts2.size());
  fprintln("Elements: {:L}", splitViewParts.size());

  std::cout << os.str();
}

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

//unsigned myPopcount16(unsigned u) {
//  u -= (u >> 1U) & 0x5555U;
//  u = ((u >> 2U) & 0x3333U) + (u & 0x3333U);
//  u = ((u >> 4U) + u) & 0x0F0FU;
//  return (u * 0x0101U) >> 8U;
//}

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

  auto startTime = currentTime();
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
        startTime = currentTime();
        for (int j = 0; j < repetitions; ++j)
          f();
        accDuration += duration_cast<nanoseconds>(currentTime() - startTime);
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

      last        = avgCpu;
      accDuration = nanoseconds(0);
    }
    os << std::endl;
    last = nanoseconds(0);
  }
}
