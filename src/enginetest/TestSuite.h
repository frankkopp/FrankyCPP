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

#ifndef FRANKYCPP_TESTSUITE_H
#define FRANKYCPP_TESTSUITE_H

//=============================================================================
// TestSuite.h - EPD Test Suite Runner
//=============================================================================
//
// Test harness for running EPD-like chess test suites against the engine.
// Loads test positions, runs search on each, and evaluates results against
// expected moves or mate depths.
// Depends on: Search.h, types.h
//
// Supported Test Types:
//   DM (direct mate)  - Expect search to find mate in N moves
//   BM (best move)    - Expect engine's move to be in target set
//   AM (avoid move)   - Expect engine's move NOT to be in target set
//
// EPD Input Format:
//   <FEN> <type> <result> ; [ id "<ID>" ; ]
//
// Example:
//   r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 2 3 bm Ng5 ; id "Test1" ;
//   8/8/8/8/8/8/6K1/6Qr b - - 0 1 dm 1 ; id "Mate in 1" ;
//
// Usage:
//   TestSuite suite(milliseconds{2000}, 30, "path/to/suite.epd");
//   suite.runTestSuite();
//   TestSuiteResult result = suite.getLastResult();
//
// See detailed documentation in the @file block below.
//
//=============================================================================

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

#include "EdpTest.h"
#include "TestTypes.h"
#include "engine/Search.h"

#include <string>
#include <string_view>
#include <vector>

namespace enginetest {
  using namespace chess;

  /// Runs EPD test suites against the chess engine.
  /// Uses EpdParser to load tests, executes searches, and reports results.
  class TestSuite {

    std::vector<EpdTest> testCases_;
    milliseconds searchTime_;
    Depth searchDepth_;
    std::string filePath_;
    TestSuiteResult lastResult_{};

  public:
    /// Creates a TestSuite for the given test file.
    /// Reads all tests from the file using EpdParser. Call runTestSuite() to execute.
    /// @param time        Search time limit per test
    /// @param searchDepth Maximum search depth per test
    /// @param filePath    Path to EPD test file
    TestSuite(milliseconds time, Depth searchDepth, std::string_view filePath);

    /// Runs all tests in the suite and prints results.
    void runTestSuite();

    /// Returns the aggregated results from the last run.
    /// @return Reference to TestSuiteResult
    [[nodiscard]] const TestSuiteResult& getLastResult() const { return lastResult_; }

    /// Returns the vector of test cases with detailed per-test results.
    /// @return Reference to test cases vector
    [[nodiscard]] const std::vector<EpdTest>& getTestCases() const { return testCases_; }

  private:
    /// Runs all tests in testCases list.
    void runAllTests();

    /// Dispatches a single test to the appropriate test function.
    static void runSingleTest(engine::Search& search, engine::SearchLimits& limits, EpdTest& test);

    /// Runs a direct mate test.
    static void directMateTest(engine::Search& search, engine::SearchLimits& limits, const Position& position, EpdTest& test);

    /// Runs a best-move test.
    static void bestMoveTest(engine::Search& search, const engine::SearchLimits& limits, const Position& position, EpdTest& test);

    /// Runs an avoid-move test.
    static void avoidMoveTest(engine::Search& search, const engine::SearchLimits& limits, const Position& position, EpdTest& test);

    /// Aggregates results from all tests.
    [[nodiscard]] TestSuiteResult sumUpTests() const;

    /// Prints the report header.
    void printReportHeader() const;

    /// Prints the final report with elapsed time.
    void printReport(nanoseconds elapsed) const;
  };

} // namespace enginetest

#endif // FRANKYCPP_TESTSUITE_H
