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

#include "engine/PVTable.h"

#include <gtest/gtest.h>

using namespace engine;
using namespace chess;

class PVTableTest : public ::testing::Test {
protected:
  PVTable pv;
};

TEST_F(PVTableTest, clearAll) {
  // Fill some entries
  pv(DEPTH_NONE, 0) = Move::normal(SQ_E2, SQ_E4);
  pv(DEPTH_NONE, 1) = Move::normal(SQ_E7, SQ_E5);
  pv(DEPTH_ONE, 1)  = Move::normal(SQ_G1, SQ_F3);

  pv.clearAll();

  EXPECT_EQ(pv(DEPTH_NONE, 0), MOVE_NONE);
  EXPECT_EQ(pv(DEPTH_NONE, 1), MOVE_NONE);
  EXPECT_EQ(pv(DEPTH_ONE, 1), MOVE_NONE);
  EXPECT_TRUE(pv.empty(DEPTH_NONE));
  EXPECT_TRUE(pv.empty(DEPTH_ONE));
}

TEST_F(PVTableTest, clearPly) {
  pv(DEPTH_NONE, 0) = Move::normal(SQ_E2, SQ_E4);
  pv(DEPTH_NONE, 1) = Move::normal(SQ_E7, SQ_E5);
  pv(DEPTH_ONE, 1)  = Move::normal(SQ_G1, SQ_F3);

  pv.clear(DEPTH_NONE);

  EXPECT_EQ(pv(DEPTH_NONE, 0), MOVE_NONE);
  // Note: clear(ply) only sets pv(ply, ply) to MOVE_NONE
  // It doesn't clear the entire row, just marks it as empty
  EXPECT_TRUE(pv.empty(DEPTH_NONE));
  // Ply 1 should still have its entry
  EXPECT_EQ(pv(DEPTH_ONE, 1), Move::normal(SQ_G1, SQ_F3));
}

TEST_F(PVTableTest, updateSimple) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);

  pv.clearAll();
  pv.update(move1, DEPTH_NONE);

  EXPECT_EQ(pv(DEPTH_NONE, 0), move1);
  EXPECT_EQ(pv(DEPTH_NONE, 1), MOVE_NONE);
  EXPECT_FALSE(pv.empty(DEPTH_NONE));
  EXPECT_EQ(pv.first(DEPTH_NONE), move1);
  EXPECT_EQ(pv.length(DEPTH_NONE), 1);
}

TEST_F(PVTableTest, updateCopiesChildPV) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);
  constexpr Move move3 = Move::normal(SQ_G1, SQ_F3);

  pv.clearAll();

  // Simulate building PV from leaves up
  // Ply 2: leaf, just one move
  pv.update(move3, DEPTH_TWO);
  EXPECT_EQ(pv.length(DEPTH_TWO), 1);

  // Ply 1: update copies child PV from ply 2
  pv.update(move2, DEPTH_ONE);
  EXPECT_EQ(pv(DEPTH_ONE, 1), move2);
  EXPECT_EQ(pv(DEPTH_ONE, 2), move3);
  EXPECT_EQ(pv(DEPTH_ONE, 3), MOVE_NONE);
  EXPECT_EQ(pv.length(DEPTH_ONE), 2);

  // Ply 0: update copies child PV from ply 1
  pv.update(move1, DEPTH_NONE);
  EXPECT_EQ(pv(DEPTH_NONE, 0), move1);
  EXPECT_EQ(pv(DEPTH_NONE, 1), move2);
  EXPECT_EQ(pv(DEPTH_NONE, 2), move3);
  EXPECT_EQ(pv(DEPTH_NONE, 3), MOVE_NONE);
  EXPECT_EQ(pv.length(DEPTH_NONE), 3);
}

TEST_F(PVTableTest, first) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  pv.clearAll();
  EXPECT_EQ(pv.first(), MOVE_NONE);
  EXPECT_EQ(pv.first(DEPTH_NONE), MOVE_NONE);
  EXPECT_EQ(pv.first(Depth{5}), MOVE_NONE);

  pv(DEPTH_NONE, 0) = move1;
  pv(Depth{5}, 5)   = move2;

  EXPECT_EQ(pv.first(), move1);
  EXPECT_EQ(pv.first(DEPTH_NONE), move1);
  EXPECT_EQ(pv.first(Depth{5}), move2);
}

TEST_F(PVTableTest, empty) {
  pv.clearAll();

  EXPECT_TRUE(pv.empty());
  EXPECT_TRUE(pv.empty(DEPTH_NONE));
  EXPECT_TRUE(pv.empty(Depth{10}));

  pv(DEPTH_NONE, 0) = Move::normal(SQ_E2, SQ_E4);

  EXPECT_FALSE(pv.empty());
  EXPECT_FALSE(pv.empty(DEPTH_NONE));
  EXPECT_TRUE(pv.empty(Depth{10}));
}

TEST_F(PVTableTest, hasLength) {
  pv.clearAll();

  EXPECT_FALSE(pv.hasLength(DEPTH_NONE, 1));
  EXPECT_FALSE(pv.hasLength(DEPTH_NONE, 2));

  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  pv(DEPTH_NONE, 0) = move1;
  pv(DEPTH_NONE, 1) = move2;
  pv(DEPTH_NONE, 2) = MOVE_NONE;

  EXPECT_TRUE(pv.hasLength(DEPTH_NONE, 1));
  EXPECT_TRUE(pv.hasLength(DEPTH_NONE, 2));
  EXPECT_FALSE(pv.hasLength(DEPTH_NONE, 3));
}

TEST_F(PVTableTest, length) {
  pv.clearAll();

  EXPECT_EQ(pv.length(), 0);
  EXPECT_EQ(pv.length(DEPTH_NONE), 0);
  EXPECT_EQ(pv.length(Depth{5}), 0);

  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);
  constexpr Move move3 = Move::normal(SQ_G1, SQ_F3);

  pv(DEPTH_NONE, 0) = move1;
  pv(DEPTH_NONE, 1) = move2;
  pv(DEPTH_NONE, 2) = move3;
  pv(DEPTH_NONE, 3) = MOVE_NONE;

  EXPECT_EQ(pv.length(), 3);
  EXPECT_EQ(pv.length(DEPTH_NONE), 3);

  pv(Depth{5}, 5) = move1;
  pv(Depth{5}, 6) = MOVE_NONE;
  EXPECT_EQ(pv.length(Depth{5}), 1);
}

TEST_F(PVTableTest, extract) {
  pv.clearAll();

  MoveList extracted = pv.extract();
  EXPECT_TRUE(extracted.empty());

  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);
  constexpr Move move3 = Move::normal(SQ_G1, SQ_F3);

  pv(DEPTH_NONE, 0) = move1;
  pv(DEPTH_NONE, 1) = move2;
  pv(DEPTH_NONE, 2) = move3;
  pv(DEPTH_NONE, 3) = MOVE_NONE;

  extracted = pv.extract();
  ASSERT_EQ(extracted.size(), 3);
  EXPECT_EQ(extracted[0], move1);
  EXPECT_EQ(extracted[1], move2);
  EXPECT_EQ(extracted[2], move3);

  // Extract from different ply
  pv(DEPTH_TWO, 2) = move3;
  pv(DEPTH_TWO, 3) = MOVE_NONE;

  const MoveList extracted2 = pv.extract(DEPTH_TWO);
  ASSERT_EQ(extracted2.size(), 1);
  EXPECT_EQ(extracted2[0], move3);
}

TEST_F(PVTableTest, directAccess) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);

  pv(Depth{5}, 7) = move1;

  EXPECT_EQ(pv(Depth{5}, 7), move1);

  // Const access
  const PVTable& constPv = pv;
  EXPECT_EQ(constPv(Depth{5}, 7), move1);
}

TEST_F(PVTableTest, row) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  pv(DEPTH_THREE, 3) = move1;
  pv(DEPTH_THREE, 4) = move2;

  const Move* rowPtr = pv.row(DEPTH_THREE);
  EXPECT_EQ(rowPtr[3], move1);
  EXPECT_EQ(rowPtr[4], move2);
}

TEST_F(PVTableTest, sentinelTermination) {
  pv.clearAll();

  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_E7, SQ_E5);

  // Build a 2-move PV at ply 0 using proper update() API
  // First update at child ply, then parent copies it
  pv.update(move2, DEPTH_ONE); // Child ply gets move2
  pv.update(move1, DEPTH_NONE);// Parent copies from child

  // Verify sentinel is placed correctly
  EXPECT_EQ(pv(DEPTH_NONE, 0), move1);
  EXPECT_EQ(pv(DEPTH_NONE, 1), move2);
  EXPECT_EQ(pv(DEPTH_NONE, 2), MOVE_NONE);// Sentinel

  // Length should stop at sentinel
  EXPECT_EQ(pv.length(DEPTH_NONE), 2);
}

TEST_F(PVTableTest, updateReplacesExistingPV) {
  constexpr Move move1 = Move::normal(SQ_E2, SQ_E4);
  constexpr Move move2 = Move::normal(SQ_D2, SQ_D4);
  constexpr Move move3 = Move::normal(SQ_E7, SQ_E5);

  pv.clearAll();

  // Initial PV: move1, move3 (built properly via update)
  pv.update(move3, DEPTH_ONE); // Child first
  pv.update(move1, DEPTH_NONE);// Parent copies from child
  EXPECT_EQ(pv.length(DEPTH_NONE), 2);
  EXPECT_EQ(pv(DEPTH_NONE, 0), move1);

  // Now update with move2 (simulating alpha improvement)
  // Child PV is empty (cleared, no update called)
  pv.clear(DEPTH_ONE);
  pv.update(move2, DEPTH_NONE);

  EXPECT_EQ(pv(DEPTH_NONE, 0), move2);
  EXPECT_EQ(pv(DEPTH_NONE, 1), MOVE_NONE);// Old data should be terminated
  EXPECT_EQ(pv.length(DEPTH_NONE), 1);
}

// Test that generation counter prevents stale data from being copied
// This tests the bug scenario described in PVTable.h header documentation
TEST_F(PVTableTest, generationCounterPreventsStaleData) {
  constexpr Move moveA1 = Move::normal(SQ_E2, SQ_E4);// Branch A, ply 1
  constexpr Move moveA2 = Move::normal(SQ_E7, SQ_E5);// Branch A, ply 2
  constexpr Move moveA3 = Move::normal(SQ_G1, SQ_F3);// Branch A, ply 3
  constexpr Move moveB1 = Move::normal(SQ_D2, SQ_D4);// Branch B, ply 1
  constexpr Move moveB2 = Move::normal(SQ_D7, SQ_D5);// Branch B, ply 2

  pv.clearAll();

  // === Branch A: Build a full PV at ply 1-3 ===
  // Simulate: search finds PV e4 e5 Nf3
  pv.update(moveA3, DEPTH_THREE);// Leaf
  pv.update(moveA2, DEPTH_TWO);  // Copies from ply 3
  pv.update(moveA1, DEPTH_ONE);  // Copies from ply 2

  EXPECT_EQ(pv.length(DEPTH_ONE), 3);
  EXPECT_EQ(pv(DEPTH_ONE, 3), moveA3);// This is the "stale" data for later

  // === Branch B: Different branch, beta cutoff at ply 3 ===
  // Simulate: search tries d4 d5, then at ply 3 gets beta cutoff (no update)
  pv.clear(DEPTH_TWO);  // Enter ply 2
  pv.clear(DEPTH_THREE);// Enter ply 3
  // Beta cutoff at ply 3 - NO pv.update() called!

  // Now ply 2 finds best move and tries to update
  // WITHOUT generation counter, this would copy stale moveA3 from ply 3
  pv.update(moveB2, DEPTH_TWO);

  // Key assertion: PV at ply 2 should NOT have moveA3 (stale data)
  EXPECT_EQ(pv.length(DEPTH_TWO), 1);// Only moveB2, no stale continuation
  EXPECT_EQ(pv(DEPTH_TWO, 2), moveB2);
  EXPECT_EQ(pv(DEPTH_TWO, 3), MOVE_NONE);// Stale moveA3 NOT copied

  // Continue up: ply 1 update
  pv.clear(DEPTH_ONE);
  pv.update(moveB1, DEPTH_ONE);

  // Final PV should be [d4, d5] not [d4, d5, Nf3(stale)]
  EXPECT_EQ(pv.length(DEPTH_ONE), 2);
  EXPECT_EQ(pv(DEPTH_ONE, 1), moveB1);
  EXPECT_EQ(pv(DEPTH_ONE, 2), moveB2);
  EXPECT_EQ(pv(DEPTH_ONE, 3), MOVE_NONE);
}
