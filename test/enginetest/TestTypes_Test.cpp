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

#include "enginetest/TestTypes.h"
#include "init.h"

#include <gtest/gtest.h>

class TestTypes_Test : public testing::Test {
public:
  static void SetUpTestSuite() {
    init::init();
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

// ============================================================================
// TestType Enum Tests
// ============================================================================

TEST_F(TestTypes_Test, TestType_EnumValues) {
  // Verify enum class is scoped
  constexpr auto noop = TestType::NOOP;
  constexpr auto dm = TestType::DM;
  constexpr auto bm = TestType::BM;
  constexpr auto am = TestType::AM;

  EXPECT_NE(noop, dm);
  EXPECT_NE(dm, bm);
  EXPECT_NE(bm, am);
}

TEST_F(TestTypes_Test, TestType_UnderlyingType) {
  // Verify underlying type is uint8_t
  static_assert(sizeof(TestType) == sizeof(uint8_t), "TestType should be uint8_t");
}

// ============================================================================
// testTypeToString Tests
// ============================================================================

TEST_F(TestTypes_Test, testTypeToString_NOOP) {
  EXPECT_EQ(testTypeToString(TestType::NOOP), "noop");
}

TEST_F(TestTypes_Test, testTypeToString_DM) {
  EXPECT_EQ(testTypeToString(TestType::DM), "dm");
}

TEST_F(TestTypes_Test, testTypeToString_BM) {
  EXPECT_EQ(testTypeToString(TestType::BM), "bm");
}

TEST_F(TestTypes_Test, testTypeToString_AM) {
  EXPECT_EQ(testTypeToString(TestType::AM), "am");
}

TEST_F(TestTypes_Test, testTypeToString_Constexpr) {
  // Verify function is constexpr (compile-time)
  constexpr auto str = testTypeToString(TestType::BM);
  EXPECT_EQ(str, "bm");
}

// ============================================================================
// ResultType Enum Tests
// ============================================================================

TEST_F(TestTypes_Test, ResultType_EnumValues) {
  // Verify enum class is scoped
  constexpr auto notTested = ResultType::NOT_TESTED;
  constexpr auto skipped = ResultType::SKIPPED;
  constexpr auto failed = ResultType::FAILED;
  constexpr auto success = ResultType::SUCCESS;

  EXPECT_NE(notTested, skipped);
  EXPECT_NE(skipped, failed);
  EXPECT_NE(failed, success);
}

TEST_F(TestTypes_Test, ResultType_UnderlyingType) {
  // Verify underlying type is uint8_t
  static_assert(sizeof(ResultType) == sizeof(uint8_t), "ResultType should be uint8_t");
}

// ============================================================================
// resultTypeToString Tests
// ============================================================================

TEST_F(TestTypes_Test, resultTypeToString_NOT_TESTED) {
  EXPECT_EQ(resultTypeToString(ResultType::NOT_TESTED), "Not tested");
}

TEST_F(TestTypes_Test, resultTypeToString_SKIPPED) {
  EXPECT_EQ(resultTypeToString(ResultType::SKIPPED), "Skipped");
}

TEST_F(TestTypes_Test, resultTypeToString_FAILED) {
  EXPECT_EQ(resultTypeToString(ResultType::FAILED), "Failed");
}

TEST_F(TestTypes_Test, resultTypeToString_SUCCESS) {
  EXPECT_EQ(resultTypeToString(ResultType::SUCCESS), "Success");
}

TEST_F(TestTypes_Test, resultTypeToString_Constexpr) {
  // Verify function is constexpr (compile-time)
  constexpr auto str = resultTypeToString(ResultType::SUCCESS);
  EXPECT_EQ(str, "Success");
}

// ============================================================================
// TestSuiteResult Struct Tests
// ============================================================================

TEST_F(TestTypes_Test, TestSuiteResult_DefaultValues) {
  constexpr TestSuiteResult result{};

  EXPECT_EQ(result.counter, 0);
  EXPECT_EQ(result.successCounter, 0);
  EXPECT_EQ(result.failedCounter, 0);
  EXPECT_EQ(result.skippedCounter, 0);
  EXPECT_EQ(result.notTestedCounter, 0);
  EXPECT_EQ(result.nodes, 0);
  EXPECT_EQ(result.time, nanoseconds{0});
}

TEST_F(TestTypes_Test, TestSuiteResult_Aggregate) {
  TestSuiteResult result{};

  result.counter = 100;
  result.successCounter = 80;
  result.failedCounter = 15;
  result.skippedCounter = 3;
  result.notTestedCounter = 2;
  result.nodes = 1000000;
  result.time = seconds(60);

  // Verify assignment works
  EXPECT_EQ(result.counter, 100);
  EXPECT_EQ(result.successCounter, 80);
  EXPECT_EQ(result.failedCounter, 15);
  EXPECT_EQ(result.skippedCounter, 3);
  EXPECT_EQ(result.notTestedCounter, 2);
  EXPECT_EQ(result.nodes, 1000000);
  EXPECT_EQ(result.time, std::chrono::seconds(60));
}

TEST_F(TestTypes_Test, TestSuiteResult_POD) {
  // Verify it's a POD-like struct (aggregate initialization)
  constexpr TestSuiteResult result{10, 8, 1, 1, 0, 50000, milliseconds(100)};

  EXPECT_EQ(result.counter, 10);
  EXPECT_EQ(result.successCounter, 8);
  EXPECT_EQ(result.failedCounter, 1);
  EXPECT_EQ(result.skippedCounter, 1);
  EXPECT_EQ(result.notTestedCounter, 0);
  EXPECT_EQ(result.nodes, 50000);
  EXPECT_EQ(result.time, std::chrono::milliseconds(100));
}

// ============================================================================
// Type Safety Tests (Enum Class Benefits)
// ============================================================================

TEST_F(TestTypes_Test, TypeSafety_CannotMixEnums) {
  // This should NOT compile (and doesn't):
  // TestType t = ResultType::SUCCESS;  // ERROR - type mismatch

  // But this works:
  auto testType = TestType::BM;
  auto resultType = ResultType::SUCCESS;

  // They're distinct types
  EXPECT_NE(static_cast<int>(testType), static_cast<int>(resultType));
}

TEST_F(TestTypes_Test, TypeSafety_ExplicitComparison) {
  // Enum classes require explicit scoping
  constexpr auto type1 = TestType::BM;
  constexpr auto type2 = TestType::BM;
  constexpr auto type3 = TestType::DM;

  EXPECT_EQ(type1, type2);
  EXPECT_NE(type1, type3);
}
