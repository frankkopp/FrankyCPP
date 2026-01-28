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

// FrankyCPP - Color iteration tests
#include "gtest/gtest.h"
#include <array>
#include <vector>

TEST(ColorTest, IterateAllProvidesExactlyWhiteThenBlack) {
  std::vector<Color> seen;
  for (const Color c : Color::all()) {
    seen.push_back(c);
  }
  ASSERT_EQ(seen.size(), COLOR_LENGTH);
  EXPECT_EQ(seen[0], WHITE);
  EXPECT_EQ(seen[1], BLACK);
}

TEST(ColorTest, NoNoColorInIteration) {
  for (const Color c : Color::all()) {
    EXPECT_NE(c, NOCOLOR);
    EXPECT_TRUE(Color{c}.isValid());
  }
}

TEST(ColorTest, RangeBasedForCompilesAndWorks) {
  int sum = 0;
  for (const Color c : Color::all()) {
    sum += static_cast<int>(c);
  }
  EXPECT_EQ(sum, 1); // 0 + 1
}

TEST(ColorTest, OppositeAndBitNotAreEquivalent) {
  EXPECT_EQ(~WHITE, BLACK);
  EXPECT_EQ(~BLACK, WHITE);
  EXPECT_EQ(WHITE.opposite(), BLACK);
  EXPECT_EQ(BLACK.opposite(), WHITE);
}

TEST(ColorTest, SignIsPlusMinusOne) {
  EXPECT_EQ(WHITE.sign(), 1);
  EXPECT_EQ(BLACK.sign(), -1);
}

TEST(ColorTest, ArrayIndexingWithColor) {
  int sum = 0;
  for (const Color c : Color::all()) {
    constexpr std::array a{10, 20};
    sum += a[static_cast<int>(c)];
  }
  EXPECT_EQ(sum, 30);
}
