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

#ifndef FRANKYCPP_TESTSUITE_H
#define FRANKYCPP_TESTSUITE_H

/**
 * @file TestSuite.h
 * @brief Test harness for running EPD-like chess test suites against the engine.
 *
 * This component loads an EPD-like test file, runs the engine on each position with
 * configurable limits, evaluates the outcome against the expected directive, and
 * prints a detailed per-test and aggregate report.
 *
 * Supported test types (see TestType):
 * - DM (direct mate): expect the search to report a mate in N.
 * - BM (best move): expect the engine’s best move to be within a set.
 * - AM (avoid move): expect the engine’s best move not to be within a set.
 *
 * Input format (EPD-like lines)
 * Each non-empty, non-comment line is expected to match this structure:
 *   <FEN> <type> <result> ; [ ... id "<ID>" ; ]
 * where:
 * - <type> is one of: bm | am | dm
 * - For bm/am, <result> is a space-separated list of SAN moves (annotations !/? are ignored).
 * - For dm, <result> is an integer mate depth N (mate in N).
 * - The id opcode is optional; if absent an implicit "no ID" is used.
 *
 * Example lines:
 *   r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 2 3 bm Ng5 Qh5+ ; id "Fool’s mate defense" ;
 *   8/8/8/8/8/8/6K1/6Qr b - - 0 1 dm 1 ; id "Mate in 1" ;
 *   rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 am a3 h3 ; id "Don’t play flank pawn" ;
 *
 * Lines are preprocessed by cleanUpLine(): leading/trailing whitespace is removed; a whole-line
 * comment starting with '#' is dropped; trailing comments introduced by '#' are removed up to the end
 * and replaced with ';' to keep opcode termination consistent.
 *
 * How it works (high level):
 * 1) Construction parses the file into a vector<Test> (readTestCases(), readOneEPD()). Invalid or
 *    unparsable lines are skipped; only valid tests are stored.
 * 2) runTestSuite() builds a Search and SearchLimits from the configured per-test time and/or
 *    depth and iterates all tests (runAllTests()).
 * 3) For each test, runSingleTest() dispatches to one of:
 *      - directMateTest(): sets limits.mate = test.mateDepth and checks reported score "mate N".
 *      - bestMoveTest(): engine’s best move must be in test.targetMoves.
 *      - avoidMoveTest(): engine’s best move must not be in test.targetMoves.
 *    The best move, score, nodes, time, and NPS are recorded into the Test struct.
 * 4) sumUpTests() aggregates counters and runtime totals into TestSuiteResult; a human-readable
 *    report is printed to stdout/log.
 *
 * Pass/Fail criteria:
 * - DM: SUCCESS if last best-move value string equals "mate <mateDepth>"; otherwise FAILED.
 * - BM: SUCCESS if best move (stripped of decorations like promotions/ambiguities by .stripped())
 *       is among test.targetMoves; otherwise FAILED.
 * - AM: SUCCESS if best move is not in test.targetMoves; otherwise FAILED.
 * - Tests that cannot be constructed from the input line are not added. Executed tests start as
 *   NOT_TESTED and are updated to SUCCESS/FAILED. SKIPPED is possible for lines deliberately
 *   bypassed during reading, though typical invalid lines are simply omitted.
 *
 * Usage:
 *   // Configure a suite with per-position limits and the path to an EPD file
 *   TestSuite suite(std::chrono::milliseconds{2000}, /max depth/ 30, "path/to/suite.epd");
 *   suite.runTestSuite();
 *   // Results are printed; an aggregate is stored internally (lastResult).
 *
 * Notes and constraints:
 * - SearchConfig::USE_BOOK is disabled for reproducibility.
 * - Execution is single-threaded per suite; no synchronization guarantees are provided.
 * - The parser accepts SAN for bm/am results and validates moves against the position.
 * - On Windows builds, output goes to standard logging/console as configured by Logging.
 */

#include "engine/Search.h"
#include "types/types.h"

#include "common/gtest_friends.h"

// supported test types
// DM = direct mate
// BM = best move
// AM = avoid move
enum TestType {
  NOOP,
  DM,
  BM,
  AM
};

// resultType define possible results for a test as a type and constants.
enum ResultType {
  NOT_TESTED,
  SKIPPED,
  FAILED,
  SUCCESS
};

inline const char* testTypeStr[]   = {"noop", "dm", "bm", "am"};
inline const char* resultTypeStr[] = {"Not tested", "Skipped", "Failed", "Success"};

// SuiteResult data structure to collect sum of the results of tests.
struct TestSuiteResult {
  int counter          = 0;
  int successCounter   = 0;
  int failedCounter    = 0;
  int skippedCounter   = 0;
  int notTestedCounter = 0;
  uint64_t nodes       = 0;
  nanoseconds time     = 0s;
};

// A Test struct holds all information to run a test and
// has fields to store the test's result
// Tests are created when reading a test file.
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

// A TestSuite provides the ability to run chess test positions
class TestSuite {

  std::vector<Test> testCases;
  milliseconds searchTime;
  Depth searchDepth;
  std::string filePath;
  TestSuiteResult lastResult{};

public:
  // Creates a TestSuite instance for a given test file
  // with given search time and max search depth
  // Reads all tests from the file. To run the tests call runTestSuite()
  TestSuite(const milliseconds& time, Depth searchDepth, const std::string& filePath);
  void printReportHeader();

  // runs the tests
  void runTestSuite();

  // returns the last test result summary
  const TestSuiteResult& getLastResult() const { return lastResult; }

private:
  // reads all tests from the given file into the given list
  static void readTestCases(const std::string& filePathStr, std::vector<Test>& tests);

  // reads on EPD file and creates a Test
  static bool readOneEPD(std::string& line, Test& test);

  // removes leading and trailing whitespace and comments
  static std::string& cleanUpLine(std::string& line);

  static void directMateTest(Search& search, SearchLimits& limits, const Position& position, Test& test);
  static void bestMoveTest(Search& search, const SearchLimits& limits, const Position& position, Test& test);
  static void avoidMoveTest(Search& search, const SearchLimits& limits, const Position& position, Test& test);

  void runAllTests(Search& search, SearchLimits& searchLimits);

  // determines which test type the test is and call the appropriate
  // test function.
  static void runSingleTest(Search& search, SearchLimits& limits, Test& test);

  // goes through all results and sums up the result type for each test
  TestSuiteResult sumUpTests() const;

  // prints a report of the test results
  void printReport(nanoseconds elapsed);

  FRIEND_TEST(TestSuite_Test, readFile);
};


#endif// FRANKYCPP_TESTSUITE_H
