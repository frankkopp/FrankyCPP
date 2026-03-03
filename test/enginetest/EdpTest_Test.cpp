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

#include "chesscore/MoveGenerator.h"
#include "enginetest/EdpTest.h"
#include "enginetest/TestTypes.h"
#include "init.h"
#include "types/types.h"

#include <gtest/gtest.h>

using namespace enginetest;
using namespace chess;


class EdpTest_Test : public testing::Test {
public:
  static void SetUpTestSuite() {
    init::init();
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

// ============================================================================
// Builder Pattern Tests
// ============================================================================

TEST_F(EdpTest_Test, Builder_BasicConstruction) {
  EpdTest::Builder builder;

  EpdTest test = builder
                   .setId("TestID")
                   .setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
                   .setLine("original line")
                   .setType(TestType::BM)
                   .build();

  EXPECT_EQ(test.getId(), "TestID");
  EXPECT_EQ(test.getFen(), "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  EXPECT_EQ(test.getLine(), "original line");
  EXPECT_EQ(test.getType(), TestType::BM);
}

TEST_F(EdpTest_Test, Builder_BestMoveTest) {
  MoveList moves;
  moves.push_back(Move(SQ_E2, SQ_E4));
  moves.push_back(Move(SQ_D2, SQ_D4));

  EpdTest::Builder builder;
  EpdTest test = builder
                   .setId("BM Test")
                   .setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
                   .setType(TestType::BM)
                   .setTargetMoves(moves)
                   .setExpectedMove(moves[0])
                   .build();

  EXPECT_EQ(test.getType(), TestType::BM);
  EXPECT_EQ(test.getTargetMoves().size(), 2);
  EXPECT_TRUE(test.getExpectedMove().isValid());
}

TEST_F(EdpTest_Test, Builder_DirectMateTest) {
  EpdTest::Builder builder;
  EpdTest test = builder
                   .setId("DM Test")
                   .setFen("8/8/8/8/8/8/6K1/6Qr b - - 0 1")
                   .setType(TestType::DM)
                   .setMateDepth(DEPTH_ONE)
                   .build();

  EXPECT_EQ(test.getType(), TestType::DM);
  EXPECT_EQ(test.getMateDepth(), DEPTH_ONE);
}

TEST_F(EdpTest_Test, Builder_AvoidMoveTest) {
  MoveList moves;
  moves.push_back(Move(SQ_A2, SQ_A3));
  moves.push_back(Move(SQ_H2, SQ_H3));

  EpdTest::Builder builder;
  EpdTest test = builder
                   .setId("AM Test")
                   .setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
                   .setType(TestType::AM)
                   .setTargetMoves(moves)
                   .build();

  EXPECT_EQ(test.getType(), TestType::AM);
  EXPECT_EQ(test.getTargetMoves().size(), 2);
}

// ============================================================================
// Default Values Tests
// ============================================================================

TEST_F(EdpTest_Test, DefaultValues) {
  EpdTest::Builder builder;
  EpdTest test = builder.build();

  EXPECT_TRUE(test.getId().empty());
  EXPECT_TRUE(test.getFen().empty());
  EXPECT_TRUE(test.getLine().empty());
  EXPECT_EQ(test.getType(), TestType::NOOP);
  EXPECT_TRUE(test.getTargetMoves().empty());
  EXPECT_EQ(test.getMateDepth(), DEPTH_NONE);
  EXPECT_EQ(test.getExpectedMove(), MOVE_NONE);

  // Results should be initialized
  EXPECT_EQ(test.getActualMove(), MOVE_NONE);
  EXPECT_EQ(test.getActualValue(), VALUE_NONE);
  EXPECT_EQ(test.getResult(), ResultType::NOT_TESTED);
  EXPECT_EQ(test.getNodes(), 0);
  EXPECT_EQ(test.getTime(), nanoseconds{0});
  EXPECT_EQ(test.getNps(), 0);
}

// ============================================================================
// Mutator Tests (for TestSuite to set results)
// ============================================================================

TEST_F(EdpTest_Test, Mutators_SetResults) {
  EpdTest::Builder builder;
  EpdTest test = builder
                   .setId("Test")
                   .setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
                   .build();

  // Initially NOT_TESTED
  EXPECT_EQ(test.getResult(), ResultType::NOT_TESTED);

  // Set results as TestSuite would do
  test.setActualMove(Move(SQ_E2, SQ_E4));
  test.setActualValue(Value(25));
  test.setResult(ResultType::SUCCESS);
  test.setNodes(100000);
  test.setTime(milliseconds(1000));
  test.setNps(100000);

  // Verify results
  EXPECT_TRUE(test.getActualMove().isValid());
  EXPECT_EQ(test.getActualValue(), Value(25));
  EXPECT_EQ(test.getResult(), ResultType::SUCCESS);
  EXPECT_EQ(test.getNodes(), 100000);
  EXPECT_EQ(test.getTime(), std::chrono::milliseconds(1000));
  EXPECT_EQ(test.getNps(), 100000);
}

TEST_F(EdpTest_Test, Mutators_AllResultTypes) {
  EpdTest::Builder builder;
  EpdTest test = builder.build();

  test.setResult(ResultType::SUCCESS);
  EXPECT_EQ(test.getResult(), ResultType::SUCCESS);

  test.setResult(ResultType::FAILED);
  EXPECT_EQ(test.getResult(), ResultType::FAILED);

  test.setResult(ResultType::SKIPPED);
  EXPECT_EQ(test.getResult(), ResultType::SKIPPED);

  test.setResult(ResultType::NOT_TESTED);
  EXPECT_EQ(test.getResult(), ResultType::NOT_TESTED);
}

// ============================================================================
// Immutability Tests (Test definition should not change after construction)
// ============================================================================

TEST_F(EdpTest_Test, Immutability_TestDefinition) {
  EpdTest::Builder builder;
  EpdTest test = builder
                   .setId("Immutable")
                   .setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
                   .setType(TestType::BM)
                   .build();

  // Test definition accessors are const
  const EpdTest& constTest = test;

  EXPECT_EQ(constTest.getId(), "Immutable");
  EXPECT_EQ(constTest.getType(), TestType::BM);
  // These should all compile (const correctness) - void cast to silence nodiscard warnings
  (void) constTest.getFen();
  (void) constTest.getLine();
  (void) constTest.getTargetMoves();
  (void) constTest.getMateDepth();
  (void) constTest.getExpectedMove();
}

// ============================================================================
// Builder Method Chaining Tests
// ============================================================================

TEST_F(EdpTest_Test, Builder_MethodChaining) {
  // All builder methods should return reference to builder for chaining
  MoveList moves;
  moves.push_back(Move(SQ_E2, SQ_E4));

  EpdTest::Builder builder;
  EpdTest test = builder
                   .setId("Chain1")
                   .setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
                   .setLine("line1")
                   .setType(TestType::BM)
                   .setTargetMoves(moves)
                   .setMateDepth(DEPTH_NONE)
                   .setExpectedMove(moves[0])
                   .build();

  EXPECT_EQ(test.getId(), "Chain1");
  EXPECT_EQ(test.getType(), TestType::BM);
}
