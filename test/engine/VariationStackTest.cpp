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

#include "types/staticmovelist.h"

#include <gtest/gtest.h>

using namespace chess;

class VariationStackTest : public ::testing::Test {
protected:
  VariationStack stack;
};

TEST_F(VariationStackTest, initialState) {
  EXPECT_TRUE(stack.empty());
  EXPECT_EQ(stack.size(), 0);
}

TEST_F(VariationStackTest, pushBack) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  stack.push_back(move1);
  EXPECT_FALSE(stack.empty());
  EXPECT_EQ(stack.size(), 1);
  EXPECT_EQ(stack[0], move1);

  stack.push_back(move2);
  EXPECT_EQ(stack.size(), 2);
  EXPECT_EQ(stack[0], move1);
  EXPECT_EQ(stack[1], move2);
}

TEST_F(VariationStackTest, popBack) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  stack.push_back(move1);
  stack.push_back(move2);
  EXPECT_EQ(stack.size(), 2);

  stack.pop_back();
  EXPECT_EQ(stack.size(), 1);
  EXPECT_EQ(stack[0], move1);

  stack.pop_back();
  EXPECT_TRUE(stack.empty());
}

TEST_F(VariationStackTest, clear) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  stack.push_back(move1);
  stack.push_back(move2);
  EXPECT_EQ(stack.size(), 2);

  stack.clear();
  EXPECT_TRUE(stack.empty());
  EXPECT_EQ(stack.size(), 0);
}

TEST_F(VariationStackTest, indexOperator) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);
  constexpr Move move3 = Move::normal(SQ_G1, SQ_F3);

  stack.push_back(move1);
  stack.push_back(move2);
  stack.push_back(move3);

  EXPECT_EQ(stack[0], move1);
  EXPECT_EQ(stack[1], move2);
  EXPECT_EQ(stack[2], move3);

  // Const access
  const VariationStack& constStack = stack;
  EXPECT_EQ(constStack[0], move1);
  EXPECT_EQ(constStack[1], move2);
  EXPECT_EQ(constStack[2], move3);
}

TEST_F(VariationStackTest, back) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  stack.push_back(move1);
  EXPECT_EQ(stack.back(), move1);

  stack.push_back(move2);
  EXPECT_EQ(stack.back(), move2);

  // Const access
  const VariationStack& constStack = stack;
  EXPECT_EQ(constStack.back(), move2);
}

TEST_F(VariationStackTest, iterators) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);
  constexpr Move move3 = Move::normal(SQ_G1, SQ_F3);

  stack.push_back(move1);
  stack.push_back(move2);
  stack.push_back(move3);

  // Test begin/end
  EXPECT_EQ(*stack.begin(), move1);
  EXPECT_EQ(stack.end() - stack.begin(), 3);

  // Test range-based for loop
  std::vector<Move> collected;
  for (const auto& m : stack) {
    collected.push_back(m);
  }
  ASSERT_EQ(collected.size(), 3);
  EXPECT_EQ(collected[0], move1);
  EXPECT_EQ(collected[1], move2);
  EXPECT_EQ(collected[2], move3);

  // Const iteration
  const VariationStack& constStack = stack;
  collected.clear();
  for (const auto& m : constStack) {
    collected.push_back(m);
  }
  ASSERT_EQ(collected.size(), 3);
}

TEST_F(VariationStackTest, str) {
  EXPECT_EQ(stack.str(), "");

  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);
  constexpr Move move3 = Move::normal(SQ_G1, SQ_F3);

  stack.push_back(move1);
  EXPECT_EQ(stack.str(), "e2e4");

  stack.push_back(move2);
  EXPECT_EQ(stack.str(), "e2e4 e7e5");

  stack.push_back(move3);
  EXPECT_EQ(stack.str(), "e2e4 e7e5 g1f3");
}

TEST_F(VariationStackTest, pushPopMany) {
  // Simulate deep search with many push/pop operations
  for (int i = 0; i < 50; ++i) {
    stack.push_back(Move::normal(static_cast<Square>(i % 64), static_cast<Square>((i + 8) % 64)));
  }
  EXPECT_EQ(stack.size(), 50);

  for (int i = 0; i < 50; ++i) {
    stack.pop_back();
  }
  EXPECT_TRUE(stack.empty());
}

TEST_F(VariationStackTest, maxDepthCapacity) {
  // Should be able to handle MAX_SIZE (128) entries
  for (size_t i = 0; i < VariationStack::MAX_SIZE; ++i) {
    stack.push_back(Move::normal(SQ_E2, SQ_E4));
  }
  EXPECT_EQ(stack.size(), VariationStack::MAX_SIZE);

  // Clear and verify
  stack.clear();
  EXPECT_TRUE(stack.empty());
}
