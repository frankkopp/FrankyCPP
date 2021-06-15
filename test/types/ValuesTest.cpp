// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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

#include "chesscore/Values.h"
#include "init.h"
#include "types/types.h"

using testing::Eq;

class ValuesTest : public ::testing::Test {
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

TEST_F(ValuesTest, basic) {
  EXPECT_EQ(30, Values::posMidValue[WHITE_PAWN][SQ_E4]);
  EXPECT_EQ(-30, Values::posMidValue[WHITE_KNIGHT][SQ_H3]);
  EXPECT_EQ(5, Values::posMidValue[WHITE_BISHOP][SQ_G2]);
  EXPECT_EQ(-15, Values::posMidValue[WHITE_ROOK][SQ_H1]);
  EXPECT_EQ(2, Values::posMidValue[WHITE_QUEEN][SQ_E5]);
  EXPECT_EQ(50, Values::posMidValue[WHITE_KING][SQ_G1]);

  EXPECT_EQ(30, Values::posMidValue[BLACK_PAWN][SQ_E5]);
  EXPECT_EQ(-30, Values::posMidValue[BLACK_KNIGHT][SQ_A6]);
  EXPECT_EQ(5, Values::posMidValue[BLACK_BISHOP][SQ_B7]);
  EXPECT_EQ(-15, Values::posMidValue[BLACK_ROOK][SQ_A8]);
  EXPECT_EQ(2, Values::posMidValue[BLACK_QUEEN][SQ_D4]);
  EXPECT_EQ(50, Values::posMidValue[BLACK_KING][SQ_G8]);

  EXPECT_EQ(90, Values::posEndValue[WHITE_PAWN][SQ_E7]);
  EXPECT_EQ(-30, Values::posEndValue[WHITE_KNIGHT][SQ_H3]);
  EXPECT_EQ(0, Values::posEndValue[WHITE_BISHOP][SQ_G2]);
  EXPECT_EQ(5, Values::posEndValue[WHITE_ROOK][SQ_H8]);
  EXPECT_EQ(5, Values::posEndValue[WHITE_QUEEN][SQ_E5]);
  EXPECT_EQ(-30, Values::posEndValue[WHITE_KING][SQ_G1]);

  EXPECT_EQ(90, Values::posEndValue[BLACK_PAWN][SQ_E2]);
  EXPECT_EQ(-30, Values::posEndValue[BLACK_KNIGHT][SQ_A6]);
  EXPECT_EQ(0, Values::posEndValue[BLACK_BISHOP][SQ_B7]);
  EXPECT_EQ(5, Values::posEndValue[BLACK_ROOK][SQ_A1]);
  EXPECT_EQ(5, Values::posEndValue[BLACK_QUEEN][SQ_D4]);
  EXPECT_EQ(-30, Values::posEndValue[BLACK_KING][SQ_G8]);

  const Value value = Values::posMidValue[WHITE_PAWN][SQ_E2];
  const Value value1 = Values::posValue[WHITE_PAWN][SQ_E2][GAME_PHASE_MAX];
  EXPECT_EQ(value, value1);
  const Value value2 = Values::posEndValue[WHITE_PAWN][SQ_E2];
  const Value value3 = Values::posValue[WHITE_PAWN][SQ_E2][0];
  EXPECT_EQ(value2, value3);
  const Value value5 = Values::posValue[WHITE_PAWN][SQ_E2][12];
  EXPECT_EQ(-10, value5);

}
