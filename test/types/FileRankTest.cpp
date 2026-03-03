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

// FrankyCPP - File/Rank unit tests
#include <gtest/gtest.h>

#include "types/types.h"

using namespace chess;
TEST(FileTest, ConstantsAndValidity) {
  EXPECT_TRUE(FILE_A.isValid());
  EXPECT_TRUE(FILE_H.isValid());
  EXPECT_FALSE(FILE_NONE.isValid());
  EXPECT_EQ(static_cast<int>(FILE_A), 0);
  EXPECT_EQ(static_cast<int>(FILE_H), 7);
}

TEST(FileTest, FromCharAndStr) {
  EXPECT_EQ(File::fromChar('a'), FILE_A);
  EXPECT_EQ(File::fromChar('h'), FILE_H);
  EXPECT_EQ(File::fromChar('x'), FILE_NONE);
  EXPECT_EQ(File::fromChar('A'), FILE_NONE);// uppercase not accepted

  EXPECT_EQ(FILE_A.str(), 'a');
  EXPECT_EQ(FILE_B.str(), 'b');
}

TEST(FileTest, DistanceAndComparisons) {
  EXPECT_EQ(FILE_A.distance(FILE_H), 7);
  EXPECT_EQ(FILE_B.distance(FILE_D), 2);

  EXPECT_TRUE(FILE_A == 0);
  EXPECT_TRUE(7 == FILE_H);
  EXPECT_TRUE(FILE_C < 5);
  EXPECT_TRUE(3 <= FILE_D);
}

TEST(FileTest, Iteration) {
  int count = 0;
  for (File f = FILE_A; f <= FILE_H; ++f) {
    EXPECT_TRUE(f.isValid());
    ++count;
  }
  EXPECT_EQ(count, 8);
}

TEST(RankTest, ConstantsAndValidity) {
  EXPECT_TRUE(RANK_1.isValid());
  EXPECT_TRUE(RANK_8.isValid());
  EXPECT_FALSE(RANK_NONE.isValid());
  EXPECT_EQ(static_cast<int>(RANK_1), 0);
  EXPECT_EQ(static_cast<int>(RANK_8), 7);
}

TEST(RankTest, FromCharAndStr) {
  EXPECT_EQ(Rank::fromChar('1'), RANK_1);
  EXPECT_EQ(Rank::fromChar('8'), RANK_8);
  EXPECT_EQ(Rank::fromChar('9'), RANK_NONE);
  EXPECT_EQ(Rank::fromChar('x'), RANK_NONE);

  EXPECT_EQ(RANK_1.str(), '1');
  EXPECT_EQ(RANK_2.str(), '2');
}

TEST(RankTest, DistancePromotionAndDouble) {
  EXPECT_EQ(RANK_1.distance(RANK_8), 7);
  EXPECT_EQ(RANK_3.distance(RANK_6), 3);

  EXPECT_EQ(Rank::promotionFor(WHITE), RANK_8);
  EXPECT_EQ(Rank::promotionFor(BLACK), RANK_1);
  EXPECT_EQ(Rank::pawnDoubleFor(WHITE), RANK_3);
  EXPECT_EQ(Rank::pawnDoubleFor(BLACK), RANK_6);
}

TEST(RankTest, Iteration) {
  int count = 0;
  for (Rank r = RANK_1; r <= RANK_8; ++r) {
    EXPECT_TRUE(r.isValid());
    ++count;
  }
  EXPECT_EQ(count, 8);
}
