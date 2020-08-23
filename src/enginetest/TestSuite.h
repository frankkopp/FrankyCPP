/*
 * MIT License
 *
 * Copyright (c) 2018 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef FRANKYCPP_TESTSUITE_H
#define FRANKYCPP_TESTSUITE_H

#include <cstdint>
#include <engine/Search.h>

#include "types/types.h"

#include "gtest/gtest_prod.h"

// supported test types
enum TestType {
  NOOP,
  DM,
  BM,
  AM
};


// resultType define possible results for a tests as a type and constants.
enum ResultType {
  NOT_TESTED,
  SKIPPED,
  FAILED,
  SUCCESS
};

namespace {
  inline const std::string testTypeStr[] = {"noop", "dm", "bm", "am"};
  inline const std::string resultTypeStr[] = {"Not tested", "Skipped", "Failed", "Success"};
}

// SuiteResult data structure to collect sum of the results of tests.
struct TestSuiteResult {
  int counter          = 0;
  int successCounter   = 0;
  int failedCounter    = 0;
  int skippedCounter   = 0;
  int notTestedCounter = 0;
  uint64_t nodes       = 0;
  nanoseconds time         = 0s;
};

struct Test {
  std::string id{};
  std::string fen{};
  TestType type{NOOP};
  MoveList targetMoves{};
  Depth mateDepth{DEPTH_NONE};
  Move expected{MOVE_NONE};
  Move actualMove{MOVE_NONE};
  Value actualValue{VALUE_NONE};
  ResultType result{NOT_TESTED};
  std::string line{};
  uint64_t nodes{};
  nanoseconds time{};
  uint64_t nps{};
};

class TestSuite {

  std::vector<Test> testCases;
  milliseconds searchTime;
  Depth searchDepth;
  std::string filePath;
  TestSuiteResult lastResult{};

public:
  TestSuite(const milliseconds& time, Depth searchDepth, const std::string& filePath);

  // runs the tests
  void runTestSuite();

private:
  // reads all tests from the given file into the given list
  static void readTestCases(const std::string& filePath, std::vector<Test>& tests) ;

  // reads on EPD file and creates a Test
  static bool readOneEPD(std::string& line, Test& test) ;

  // removes leading and trailing whitespace and comments
  static std::string& cleanUpLine(std::string& line);

  static void directMateTest(Search& search, SearchLimits& limits, Position& position, Test& test);
  static void bestMoveTest(Search& search, SearchLimits& limits, Position& position, Test& test);
  static void avoidMoveTest(Search& search, SearchLimits& limits, Position& position, Test& test);

  void runAllTests(Search& search, SearchLimits& searchLimits);

  // determines which test type the test is and call the appropriate
  // test function.
  static void runSingleTest(Search& search, SearchLimits& limits, Test& test);

  TestSuiteResult sumUpTests() const;

  FRIEND_TEST(TestSuite_Test, readFile);
};


#endif//FRANKYCPP_TESTSUITE_H
