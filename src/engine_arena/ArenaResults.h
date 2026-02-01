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

#ifndef FRANKYCPP_ENGINE_ARENA_RESULTS_H
#define FRANKYCPP_ENGINE_ARENA_RESULTS_H

#include "types/types.h"
#include "types/timeunits.h"

#include <string>
#include <vector>

namespace arena {

/// Detail for a single test case within a test suite
struct TestCaseDetail {
  std::string testId;      ///< Test identifier
  std::string fen;         ///< Position FEN
  std::string expected;    ///< Expected move(s)
  std::string actual;      ///< Actual move played
  bool passed;             ///< Test passed
  uint64_t nodes;          ///< Nodes searched
  int64_t timeMs;          ///< Time taken in milliseconds
};

/// Result from running a test suite
struct TestSuiteResult {
  std::string version;        ///< Engine version
  std::string suiteName;      ///< Test suite name
  std::string timestamp;      ///< ISO 8601 timestamp
  int totalTests;             ///< Total number of tests
  int passed;                 ///< Number of passed tests
  int failed;                 ///< Number of failed tests
  int skipped;                ///< Number of skipped tests
  uint64_t totalNodes;        ///< Total nodes searched
  int64_t totalTimeMs;        ///< Total time in milliseconds
  std::vector<TestCaseDetail> details; ///< Per-test details
};

/// Result from running an engine match
struct MatchResult {
  std::string version;        ///< Engine version
  std::string matchName;      ///< Match name
  std::string timestamp;      ///< ISO 8601 timestamp
  std::string engine1Name;    ///< First engine name
  std::string engine2Name;    ///< Second engine name
  int engine1Wins;            ///< Wins by engine 1
  int engine2Wins;            ///< Wins by engine 2
  int draws;                  ///< Number of draws
  double engine1Score;        ///< Score for engine 1 (win=1, draw=0.5)
  double engine2Score;        ///< Score for engine 2
  double eloDifference;       ///< Calculated ELO difference
  std::string pgnPath;        ///< Path to PGN file
  int64_t durationMs;         ///< Match duration in milliseconds
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_RESULTS_H
