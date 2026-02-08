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

#include "types/square.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <ranges>

class StaticMoveListTest : public ::testing::Test {
protected:
  MoveList moveList;
  VariationStack variationStack;
};

// =============================================================================
// Basic Operations
// =============================================================================

TEST_F(StaticMoveListTest, initialState) {
  EXPECT_TRUE(moveList.empty());
  EXPECT_EQ(moveList.size(), 0);
  EXPECT_EQ(MoveList::MAX_SIZE, 256);
  EXPECT_EQ(VariationStack::MAX_SIZE, 128);
}

TEST_F(StaticMoveListTest, pushBack) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  moveList.push_back(move1);
  EXPECT_FALSE(moveList.empty());
  EXPECT_EQ(moveList.size(), 1);
  EXPECT_EQ(moveList[0], move1);

  moveList.push_back(move2);
  EXPECT_EQ(moveList.size(), 2);
  EXPECT_EQ(moveList[0], move1);
  EXPECT_EQ(moveList[1], move2);
}

TEST_F(StaticMoveListTest, popBack) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  moveList.push_back(move1);
  moveList.push_back(move2);
  EXPECT_EQ(moveList.size(), 2);

  moveList.pop_back();
  EXPECT_EQ(moveList.size(), 1);
  EXPECT_EQ(moveList[0], move1);

  moveList.pop_back();
  EXPECT_TRUE(moveList.empty());
}

TEST_F(StaticMoveListTest, clear) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  moveList.push_back(move1);
  moveList.push_back(move2);
  EXPECT_EQ(moveList.size(), 2);

  moveList.clear();
  EXPECT_TRUE(moveList.empty());
  EXPECT_EQ(moveList.size(), 0);
}

// =============================================================================
// Element Access
// =============================================================================

TEST_F(StaticMoveListTest, indexOperator) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);
  constexpr Move move3 = Move::normal(SQ_G1, SQ_F3);

  moveList.push_back(move1);
  moveList.push_back(move2);
  moveList.push_back(move3);

  EXPECT_EQ(moveList[0], move1);
  EXPECT_EQ(moveList[1], move2);
  EXPECT_EQ(moveList[2], move3);

  // Const access
  const MoveList& constList = moveList;
  EXPECT_EQ(constList[0], move1);
  EXPECT_EQ(constList[1], move2);
  EXPECT_EQ(constList[2], move3);
}

TEST_F(StaticMoveListTest, atMethod) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  moveList.push_back(move1);
  moveList.push_back(move2);

  EXPECT_EQ(moveList.at(0), move1);
  EXPECT_EQ(moveList.at(1), move2);

  // Bounds checking
  Move x = MOVE_NONE;
  EXPECT_THROW(x = moveList.at(2), std::out_of_range);
  EXPECT_THROW(x = moveList.at(100), std::out_of_range);
  (void)x;
}

TEST_F(StaticMoveListTest, frontAndBack) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);
  constexpr Move move3 = Move::normal(SQ_G1, SQ_F3);

  moveList.push_back(move1);
  EXPECT_EQ(moveList.front(), move1);
  EXPECT_EQ(moveList.back(), move1);

  moveList.push_back(move2);
  moveList.push_back(move3);
  EXPECT_EQ(moveList.front(), move1);
  EXPECT_EQ(moveList.back(), move3);

  // Const access
  const MoveList& constList = moveList;
  EXPECT_EQ(constList.front(), move1);
  EXPECT_EQ(constList.back(), move3);
}

// =============================================================================
// Iterator Support
// =============================================================================

TEST_F(StaticMoveListTest, iterators) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);
  constexpr Move move3 = Move::normal(SQ_G1, SQ_F3);

  moveList.push_back(move1);
  moveList.push_back(move2);
  moveList.push_back(move3);

  // Range-based for loop
  int count = 0;
  for (const auto& m : moveList) {
    (void)m;
    count++;
  }
  EXPECT_EQ(count, 3);

  // Manual iteration
  auto it = moveList.begin();
  EXPECT_EQ(*it, move1);
  ++it;
  EXPECT_EQ(*it, move2);
  ++it;
  EXPECT_EQ(*it, move3);
  ++it;
  EXPECT_EQ(it, moveList.end());

  // Const iteration
  const MoveList& constList = moveList;
  auto cit = constList.begin();
  EXPECT_EQ(*cit, move1);
}

TEST_F(StaticMoveListTest, rangesAlgorithms) {
  constexpr Move move1 = Move::normal(SQ_C2, SQ_C4, static_cast<Value>(-100));
  constexpr Move move2 = Move::normal(SQ_D2, SQ_D4, static_cast<Value>(0));
  constexpr Move move3 = Move::normal(SQ_E2, SQ_E4, static_cast<Value>(100));

  moveList.push_back(move1);
  moveList.push_back(move2);
  moveList.push_back(move3);

  // Sort using std::ranges::stable_sort (same as in Search.cpp)
  std::ranges::stable_sort(moveList, moveValueGreaterComparator());

  // After sorting by value descending, order should be: move3 (100), move2 (0), move1 (-100)
  EXPECT_EQ(moveList.at(0), move3);
  EXPECT_EQ(moveList.at(1), move2);
  EXPECT_EQ(moveList.at(2), move1);
}

TEST_F(StaticMoveListTest, rangesForEach) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4, static_cast<Value>(100));
  constexpr Move move2 = Move::normal(SQ_D2, SQ_D4, static_cast<Value>(200));

  moveList.push_back(move1);
  moveList.push_back(move2);

  // Use std::ranges::for_each (same pattern as MoveGenerator.cpp)
  std::ranges::for_each(moveList, [](Move& m) { m = m.stripped(); });

  // After stripping, sort values should be gone
  EXPECT_EQ(moveList[0].value(), VALUE_NONE);
  EXPECT_EQ(moveList[1].value(), VALUE_NONE);
}

// =============================================================================
// String Output
// =============================================================================

TEST_F(StaticMoveListTest, str) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::promotion(SQ_A7, SQ_A8, QUEEN);
  constexpr Move move3 = Move::castling(SQ_E1, SQ_G1);

  moveList.push_back(move1);
  moveList.push_back(move2);
  moveList.push_back(move3);

  const std::string expected = "e2e4 a7a8Q e1g1";
  EXPECT_EQ(moveList.str(), expected);
}

TEST_F(StaticMoveListTest, strVerbose) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_D2, SQ_D4);

  moveList.push_back(move1);
  moveList.push_back(move2);

  const std::string result = moveList.strVerbose();
  EXPECT_TRUE(result.find("MoveList:") != std::string::npos);
  EXPECT_TRUE(result.find("size=2") != std::string::npos);
}

TEST_F(StaticMoveListTest, streamOperator) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_D2, SQ_D4);

  moveList.push_back(move1);
  moveList.push_back(move2);

  std::ostringstream oss;
  oss << moveList;
  EXPECT_EQ(oss.str(), "e2e4 d2d4");
}

// =============================================================================
// Capacity
// =============================================================================

TEST_F(StaticMoveListTest, capacityMethods) {
  EXPECT_EQ(moveList.capacity(), 256);
  EXPECT_EQ(moveList.max_size(), 256);
  EXPECT_EQ(variationStack.capacity(), 128);
  EXPECT_EQ(variationStack.max_size(), 128);
}


TEST_F(StaticMoveListTest, maxCapacity) {
  // Fill to capacity (for VariationStack which is smaller)
  for (size_t i = 0; i < VariationStack::MAX_SIZE; ++i) {
    variationStack.push_back(Move::normal(SQ_E2, SQ_E4));
  }
  EXPECT_EQ(variationStack.size(), 128);

  // Clear and verify
  variationStack.clear();
  EXPECT_TRUE(variationStack.empty());
}

// =============================================================================
// Type Aliases
// =============================================================================

TEST_F(StaticMoveListTest, typeAliasesAreDifferentTypes) {
  // MoveList and VariationStack should be different types (different capacities)
  static_assert(!std::is_same_v<MoveList, VariationStack>,
                "MoveList and VariationStack should be different types");

  // But both should be StaticMoveList instantiations
  static_assert(std::is_same_v<MoveList, StaticMoveList<256>>,
                "MoveList should be StaticMoveList<256>");
  static_assert(std::is_same_v<VariationStack, StaticMoveList<128>>,
                "VariationStack should be StaticMoveList<128>");
}

// =============================================================================
// Data Access
// =============================================================================

TEST_F(StaticMoveListTest, dataPointer) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  moveList.push_back(move1);

  Move* ptr       = moveList.data();
  const Move* cptr = static_cast<const MoveList&>(moveList).data();

  EXPECT_EQ(*ptr, move1);
  EXPECT_EQ(*cptr, move1);
}
