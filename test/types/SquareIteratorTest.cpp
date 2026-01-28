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

#include "init.h"


#include <gtest/gtest.h>

class SquareIteratorTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }
};

TEST_F(SquareIteratorTest, ClassicNumericForLoopStillWorks) {
  int count = 0;
  for (Square s = SQ_A1; s <= SQ_H8; ++s) {
    ASSERT_TRUE(s.isValid());
    ++count;
  }
  EXPECT_EQ(count, 64);
}

TEST_F(SquareIteratorTest, RangeAll) {
  int count = 0;
  Square firstSq = SQ_NONE;
  Square lastSq  = SQ_NONE;
  for (const Square s : Square::all()) {
    if (count == 0) firstSq = s;
    lastSq = s;
    ++count;
  }
  EXPECT_EQ(count, 64);
  EXPECT_EQ(firstSq, SQ_A1);
  EXPECT_EQ(lastSq, SQ_H8);
}

TEST_F(SquareIteratorTest, RangeBetweenInclusive) {
  int count = 0;
  for (Square s : Square::between(SQ_A1, SQ_A1)) {
    ASSERT_TRUE(s.isValid());
    ++count;
    EXPECT_EQ(s, SQ_A1);
  }
  EXPECT_EQ(count, 1);

  count = 0;
  Square firstSq = SQ_NONE;
  Square lastSq  = SQ_NONE;
  for (const Square s : Square::between(SQ_B2, SQ_C3)) {
    if (count == 0) firstSq = s;
    lastSq = s;
    ++count;
  }
  // numeric contiguous interval from 9 (b2) to 18 (c3) inclusive
  EXPECT_EQ(count, static_cast<int>(SQ_C3) - static_cast<int>(SQ_B2) + 1);
  EXPECT_EQ(firstSq, SQ_B2);
  EXPECT_EQ(lastSq, SQ_C3);
}

TEST_F(SquareIteratorTest, RangeTo) {
  int count = 0;
  Square start = SQ_A1;
  for (Square s : start.to(SQ_A1)) {
    ++count;
    EXPECT_EQ(s, SQ_A1);
  }
  EXPECT_EQ(count, 1);

  count = 0;
  start = SQ_A1;
  Square lastSq = SQ_NONE;
  for (const Square s : start.to(SQ_H8)) {
    ++count;
    lastSq = s;
  }
  EXPECT_EQ(count, 64);
  EXPECT_EQ(lastSq, SQ_H8);
}

TEST_F(SquareIteratorTest, UseInAlgorithms) {
  // example use: build a string of all square names via range-based loop
  std::string names;
  names.reserve(64 * 2);
  for (Square s : Square::all()) {
    names += s.str();
  }
  const std::string expected =
      "a1b1c1d1e1f1g1h1"
      "a2b2c2d2e2f2g2h2"
      "a3b3c3d3e3f3g3h3"
      "a4b4c4d4e4f4g4h4"
      "a5b5c5d5e5f5g5h5"
      "a6b6c6d6e6f6g6h6"
      "a7b7c7d7e7f7g7h7"
      "a8b8c8d8e8f8g8h8";
  EXPECT_EQ(names, expected);
}
