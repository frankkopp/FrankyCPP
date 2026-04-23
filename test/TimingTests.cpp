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

#include "Test_Utils.h"
#include "common/stringutil.h"
#include "init.h"
#include "types/types.h"
#include <chesscore/Position.h>

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <random>
#include <regex>

// Add near the top (after includes), once:
namespace {
  // Prevent constant folding in micro-benchmarks while keeping the work identical
  volatile int g_zero = 0;
#if defined(_MSC_VER)
  __declspec(noinline)
#else
  __attribute__((noinline))
#endif
  int opaque(const int x) {
    return x + g_zero;
  }
} // namespace

using namespace std::chrono;
using namespace chess;
using namespace common;


class TimingTests : public testing::Test {
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

void TimingTests::testTiming(std::ostringstream& os, const int rounds, const int iterations,
                             const int repetitions, const std::vector<std::function<void()>>& tests) {
  std::cout.imbue(projectLocale);
  os.imbue(projectLocale);
  os << std::setprecision(9);

  os << std::endl;
  os << "Starting timing test: rounds=" << rounds
     << " iterations=" << iterations << " repetitions=" << repetitions << std::endl;
  os << "======================================================================"
     << std::endl;

  nanoseconds lastRound(0);

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
        auto startTime = currentTime();
        for (int j = 0; j < repetitions; ++j)
          f();
        accDuration += duration_cast<nanoseconds>(currentTime() - startTime);
      }

      const nanoseconds cpuTime      = accDuration;
      const nanoseconds avgCpu       = cpuTime / iterations;
      const uint64_t percentFromLast = lastRound.count() ? (avgCpu * 10'000) / lastRound : 10'000;

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

      lastRound   = avgCpu;
      accDuration = nanoseconds(0);
    }
    os << std::endl;
    lastRound = nanoseconds(0);
  }
}

TEST_F(TimingTests, distancevsdiff) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  std::ostringstream os;

  volatile bool t = false;

  const std::function f1 = [&] {
    t = SQ_E2.distanceTo(SQ_E4) == 2;
  };
  const std::function f2 = [&] {
    t = std::abs(static_cast<int>(SQ_E2) - static_cast<int>(SQ_E4)) == 16;
    (void) t;
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
TEST_F(TimingTests, doMoveUndoMove) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  std::ostringstream os;

  //// TESTS START
  // position for each move type
  // fxe3 enpassant
  // fxe3 normal capture
  // o-o castling
  // Rc1 normal non capturing
  // c1Q promotion
  Position position("r3k2r/1ppn3p/4q1n1/8/4Pp2/3R4/p1p2PPP/R5K1 b kq e3 0 1");
  constexpr auto move1 = Move::enPassant(SQ_F4, SQ_E3);
  constexpr auto move2 = Move::normal(SQ_F2, SQ_E3);
  constexpr auto move3 = Move::castling(SQ_E8, SQ_G8);
  constexpr auto move4 = Move::normal(SQ_D3, SQ_C3);
  constexpr auto move5 = Move::promotion(SQ_C2, SQ_C1, QUEEN);

  const std::function f1 = [&] {
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
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  std::ostringstream os;

  //// TESTS START

  // ReSharper disable once CppVariableCanBeMadeConstexpr
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
  const std::function f5 = [&] {
    trimmedLineWhile = trimFast(line);
    counter++;
  };

  // while
  std::string_view trimmedLineViewWhile{};
  const std::function f6 = [&] {
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
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  std::ostringstream os;

  //// TESTS START

  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string fen = "r3k2r/1ppn3p/2q1q1n1/8/2q1Pp2/6R1/p1p2PPP/1R4K1";
  static const std::regex illegalInFenPosition(R"([^1-8pPnNbBrRqQkK/]+)");
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  static const std::string allowedChars{"12345678pPnNbBrRqQkK/"};

  NEWLINE;

  int counter1 = 0;
  int counter2 = 0;

  // regex
  std::string trimmedLineRegex{};
  const std::function f1 = [&] {
    if (!std::regex_search(fen, illegalInFenPosition)) {
      counter1++;
    }
  };

  std::string trimmedLineViewRegex{};
  const std::function f2 = [&] {
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
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  std::ostringstream os;

  //// TESTS START

  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "1. e4 e6 2. d4 d5 3. Nd2 Nc6 4. Ngf3 Nf6 5. e5 Nd7 6. g3 Be7 7. Bh3 b6 8. O-O "
                           "Bb7 9. c3 h5 10. Qe2 Nf8 11. b4 a5 12. b5 Na7 13. a4 c3 14. Ba3 cxb5 15. Bxe7 "
                           "Qxe7 16. axb5 g5 17. Bg2 Ng6 18. Rab1 h4 19. Qe3 g4 20. Ne1 Rc8 21. c4 Qf8 22. "
                           "Nd3 Rc7 23. c5 Nc8 24. c6 Ba8 25. f3 hxg3 26. hxg3 gxf3 27. Rxf3 Nce7 28. Rbf1 "
                           "Rh7 29. Rf6 Qh8 30. Qg5 Bxc6 31. bxc6 Nxc6 32. Nf4 Nf4 33. Qxf4 Ng6 34. Qg4 "
                           "Ne7 35. Qe2 Qg8 36. Qd3 Rg7 37. R1f3 Qh8 38. Qf1 Qg8 39. Qd3 Qh8 40. Qf1 Qg8 "
                           "41. Qf2 Ng6 42. Nf1 Ke7 43. Qd2 Nf8 44. Rc3 Rxc3 45. Qxc3 Nd7 46. Qa3+ Kd8 47. "
                           "Rf3 Qf8 48. Qa4 Qb4 49. Qxb4 axb4 50. Bh3 Nb8 51. Nd2 Nc6 52. Nb3 Ke7 53. Kf2 "
                           "Rg8 54. Bf1 Rc8 55. Ke1 Na5 56. Kd2 Nc4+ 57. Ke1 Ra8 58. Nb3 Na5 59. Kd1 Rg8 "
                           "60. Bb5 Rh2 61. Be2 Rh1+ 63. Bf1 Rh2 64. Nb3 Rc8 65. Be2 Nc4 66. Nd2 Na5 67. Kd1 "
                           "Rh2 68. Rf3 Rh1+ 73. Nf1 Rh8 74. Ke1 Rc8 75. Nd2 Rh8 76. Bf3 Rh2 77. Kf1 Rh8 "
                           "78. Kg1 Rc8 79. Bh5 Rc2 80. Kf1 Nc4 81. Nb3 Rb2 82. Kg1 Na4 83. Bd1 Rb1 84. Nxa5 "
                           "Kf7 86. Ke2 a4 87. Kd2 b3 88. Kc3 Rc4 89. Kb2 Rc4 90. Rd2 Rc4 91. Be2 Rh8 "
                           "92. Bb5 Rg8 93. Rd3 Ra8 94. Rd2 Rg8 95. Bxa4 Rxg3 96. Bxb3 f6 97. exf6+ Kxf6 "
                           "98. Rf2+ Ke7 99. Kc2 Re3 100. Rh2 e5 101. d5 Rxe5 102. Kd3 103. Bc2 Rg5 "
                           "104. Rh8 Rg3+ 105. Kd4 Rg4+ 106. Kc3 Rc4+ 107. Kb3 Rg4 108. Rd8+ Kc5 109. Rc8+ "
                           "Kd6 110. Bh7 Rc4 111. Rd8+ Kc5 112. Bg8 Rb4+ 113. Ka3 Rd4 114. Kb3 Rb4+ 115. "
                           "Kc3 Rc4+ 116. Kd3 Rd4+ 117. Ke3 Re4+ 118. Kf3 Re5 119. Kf4 Rh5 120. Bf7 Rh4+ "
                           "121. Ke5 Re4+ 122. Kf5 Rd4 123. Be6 Rd1 124. Ke5 Re1+ 125. Kf6 Rd1 126. Ke7 Rd2 "
                           "127. Rc8+ Kd4 128. Bxd5 Rh2 129. Kd6 Rh6+ 130. Be6 Rg6 131. Rc4+ Kd4 132. Rh4 "
                           "Kc3 133. Re4 Kd3 134. Rh4 Kc3 135. Ke5 Rg5+ 136. Bf5 Kb3 137. Rf4 Kc3 138. Kd5 "
                           "Rg7 139. Rc4+ Kd2 140. Kd4 Ke2 141. Ke4 Re2+ 142. Kf4 Rf2+ 143. Rc2+ Kd1 144. "
                           "Ra2 Kc1 145. Ke4 Rf8 146. Rf2 Rd8 147. Ke3 Re8+ 148. Be4 Rc8 149. Bd3 Re8+ 150. "
                           "Be4 Rc8 151. Rf1+ Kb2 152. Rb1+ Ka3 153. Bd3 Re8+ 154. Be4 Rc8 155. Rf1 Kb2 "
                           "156. Rb1+ Ka3 157. Kd4 Ka4 158. Bf5 Rh8 159. Be6 Rh6 160. Kc4 Rh4+ 161. Kc5 "
                           "Rh5+ 162. Bd5 Ka3 163. Rb3+ Ka4 164. Rg3 Re5 165. Rg2 Re3 166. Kc4 Ka5 167. Rg6 "
                           "Re1 168. Kc5 Rb1 169. Bc4 Rb5+ 170. Kd4 1/2-1/2";
  const std::string_view lineView{line};

  NEWLINE;

  std::vector<std::string> splitStringParts{};
  const std::function f1 = [&] {
    splitStringParts.clear();
    splitFast(line, splitStringParts, " ");
  };

  //  std::vector<std::string> splitStringParts2{};
  //  std::function<void()> f2 = [&]() {
  //    splitStringParts2.clear();
  //    split(line, splitStringParts2, ' ');
  //  };

  std::vector<std::string_view> splitViewParts{};
  const std::function f3 = [&] {
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

TEST_F(TimingTests, ColorIteration) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }

  std::ostringstream os;

  // Compare three variants of iterating over colors
  int sinkRange   = 0;
  int sinkClassic = 0;
  int sinkInt     = 0;

  // range-based for loop
  const std::function fRange = [&] {
    int sum = 0;
    for (const Color c : Color::all()) {
      sum += c.sign();
    }
    sinkRange += sum;
  };

  // classic for loop
  const std::function fClassic = [&] {
    int sum = 0;
    for (Color c = WHITE; c <= BLACK; ++c) {
      sum += c.sign();
    }
    sinkClassic += sum;
  };

  // integer loop
  const std::function fInt = [&] {
    int sum = 0;
    for (int ci = 0; ci < static_cast<int>(COLOR_LENGTH); ++ci) {
      sum += Color{ci}.sign();
    }
    sinkInt += sum;
  };

  std::vector<std::function<void()>> tests;
  tests.push_back(fRange);
  tests.push_back(fClassic);
  tests.push_back(fInt);

  // Keep durations modest; adjust repetitions if needed
  testTiming(os, 5, 100, 10'000'000, tests);

  NEWLINE;
  fprintln("ColorIteration sinks: range={} classic={} int={}", sinkRange, sinkClassic, sinkInt);
  std::cout << os.str();
}


TEST_F(TimingTests, SquareIteration) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }

  std::ostringstream os;

  // Compare three variants of iterating 64 squares
  int sinkRange   = 0;
  int sinkClassic = 0;
  int sinkInt     = 0;

  // range-based loop over all squares
  const std::function fRange = [&] {
    int sum = 0;
    for (Square s : Square::all()) {
      sum += opaque(s);
    }
    sinkRange += sum;
  };

  // classic numeric Square loop
  const std::function fClassic = [&] {
    int sum = 0;
    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
      sum += opaque(s);
    }
    sinkClassic += sum;
  };

  // plain integer loop
  const std::function fInt = [&] {
    int sum = 0;
    for (int i = 0; i < 64; ++i) {
      sum += opaque(i);
    }
    sinkInt += sum;
  };

  std::vector<std::function<void()>> tests;
  tests.push_back(fRange);
  tests.push_back(fClassic);
  tests.push_back(fInt);

  // Keep durations reasonable since each repetition walks 64 items
  testTiming(os, 5, 50, 2'000'000, tests);

  NEWLINE;
  fprintln("SquareIteration sinks: range={} classic={} int={}", sinkRange, sinkClassic, sinkInt);
  std::cout << os.str();
}

/**
 * Compare do/undo moves on same Position vs. copying Position before moves.
 * Tests the performance trade-off between Position copy construction and move undo operations.
 */
TEST_F(TimingTests, PositionCopyVsUndo) {
  if (isBulkRun()) {
    GTEST_SKIP();
  }
  std::ostringstream os;

  //// PREP: Create position with 15 moves of history
  Position basePosition;

  // Execute 15 moves to build up history
  constexpr std::array historyMoves = {
    Move::normal(SQ_E2, SQ_E4), // 1. e4
    Move::normal(SQ_E7, SQ_E5), // 1... e5
    Move::normal(SQ_G1, SQ_F3), // 2. Nf3
    Move::normal(SQ_B8, SQ_C6), // 2... Nc6
    Move::normal(SQ_F1, SQ_C4), // 3. Bc4
    Move::normal(SQ_G8, SQ_F6), // 3... Nf6
    Move::normal(SQ_D2, SQ_D3), // 4. d3
    Move::normal(SQ_F8, SQ_C5), // 4... Bc5
    Move::normal(SQ_C2, SQ_C3), // 5. c3
    Move::normal(SQ_D7, SQ_D6), // 5... d6
    Move::normal(SQ_B1, SQ_D2), // 6. Nbd2
    Move::normal(SQ_C8, SQ_E6), // 6... Be6
    Move::normal(SQ_C4, SQ_B5), // 7. Bb5
    Move::normal(SQ_E8, SQ_G8), // 7... O-O (kingside castling)
    Move::normal(SQ_E1, SQ_G1)  // 8. O-O (kingside castling)
  };

  for (const auto& move : historyMoves) {
    basePosition.doMove(move);
  }

  //// TEST MOVES: 5 moves to execute during the test
  constexpr std::array testMoves = {
    Move::normal(SQ_H2, SQ_H3), // h3
    Move::normal(SQ_A7, SQ_A6), // a6
    Move::normal(SQ_B5, SQ_A4), // Ba4
    Move::normal(SQ_B7, SQ_B5), // b5
    Move::normal(SQ_A4, SQ_B3)  // Bb3
  };

  //// TEST 1: do/undo on same position
  Position position1 = basePosition;
  int sink1          = 0;

  const std::function f1 = [&] {
    for (const auto& move : testMoves) {
      position1.doMove(move);
      sink1 += position1.getHalfMoveClock(); // Use position to prevent optimization
    }
    // Restore position by undoing all moves
    for (int i = 0; i < 5; ++i) {
      position1.undoMove();
    }
  };

  //// TEST 2: copy position before moves (no undo needed)
  int sink2 = 0;

  const std::function f2 = [&] {
    Position position2 = basePosition; // Copy the position
    for (const auto& move : testMoves) {
      position2.doMove(move);
      sink2 += position2.getHalfMoveClock(); // Use position to prevent optimization
    }
    // No undo needed - we have the original basePosition
  };

  std::vector<std::function<void()>> tests;
  tests.push_back(f1);
  tests.push_back(f2);
  //// TESTS END

  testTiming(os, 5, 50, 500'000, tests);

  NEWLINE;
  fprintln("PositionCopyVsUndo sinks: do/undo={} copy={}", sink1, sink2);
  std::cout << os.str();
}
