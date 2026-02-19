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

#ifndef FRANKYCPP_SEARCHTREESIZETEST_H
#define FRANKYCPP_SEARCHTREESIZETEST_H

//=============================================================================
// SearchTreeSizeTest.h - Search Feature Benchmarking
//=============================================================================
//
// Measures the impact of individual search features on tree size, NPS,
// and search depth. Useful for tuning and verifying pruning/reduction
// effectiveness.
// Depends on: Search.h, types.h
//
// Usage:
//   std::vector<std::string> fens = {"startpos FEN", "position 2 FEN", ...};
//   SearchTreeSizeTest test(12, milliseconds{5000}, fens);
//   test.start();  // Runs tests and prints comparison tables
//
// The test runs each position multiple times with different feature
// configurations, comparing nodes, depth, and NPS to measure the impact
// of each search feature.
//
//=============================================================================

#include <string>
#include <utility>
#include <vector>

#include <engine/Search.h>

/// Namespace for search tree size test data structures.
namespace SearchTreeSize {

  /// Results from a single test run (one feature configuration).
  struct SingleTest {
    std::string name;         ///< Feature/configuration name
    uint64_t nodes    = 0;    ///< Nodes searched
    uint64_t nps      = 0;    ///< Nodes per second
    uint64_t depth    = 0;    ///< Search depth reached
    uint64_t extra    = 0;    ///< Extra/selective depth
    uint64_t time     = 0;    ///< Search time in ms
    uint64_t special1 = 0;    ///< Custom statistic 1
    uint64_t special2 = 0;    ///< Custom statistic 2
    Move move         = MOVE_NONE;  ///< Best move found
    Value value       = VALUE_NONE; ///< Search score
    std::string pv;           ///< Principal variation
  };

  /// Results for one position across all feature configurations.
  struct Result {
    std::string fen;                   ///< Position FEN
    std::vector<SingleTest> tests{};   ///< Results for each configuration
    explicit Result(std::string _fen) : fen(std::move(_fen)){};
  };

  /// Aggregated sums across all positions.
  struct TestSums {
    uint64_t sumCounter{};  ///< Number of tests
    uint64_t sumNodes{};    ///< Total nodes
    uint64_t sumNps{};      ///< Total NPS (for averaging)
    uint64_t sumDepth{};    ///< Total depth (for averaging)
    uint64_t sumExtra{};    ///< Total extra depth
    uint64_t sumTime{};     ///< Total time in ms
    uint64_t special1{};    ///< Total special stat 1
    uint64_t special2{};    ///< Total special stat 2
  };
}// namespace SearchTreeSize

/// Runs search tree size measurements to evaluate feature effectiveness.
class SearchTreeSizeTest {

  int depth;
  milliseconds movetime;
  std::vector<std::string> fens;
  std::vector<SearchTreeSize::Result> results{};

  /// Pointer to custom statistic for tracking specific counters.
  const uint64_t* ptrToSpecial1 = nullptr;
  const uint64_t* ptrToSpecial2 = nullptr;

public:
  /// Creates a tree size test with given parameters.
  /// @param depth      Maximum search depth
  /// @param movetime   Time limit per position
  /// @param fenVector  Positions to test
  SearchTreeSizeTest(const int depth, const milliseconds& movetime, std::vector<std::string> fenVector)
      : depth(depth), movetime(movetime), fens(std::move(fenVector)) {}

  /// Runs all tests and prints results.
  void start();

private:
  /// Runs all feature measurements for one position.
  SearchTreeSize::Result featureMeasurements(int d, milliseconds mt, const std::string& fen);

  /// Measures tree size for a single configuration.
  SearchTreeSize::SingleTest measureTreeSize(Search& search, const Position& position, const SearchLimits& searchLimits, const std::string& featureName) const;
};


#endif//FRANKYCPP_SEARCHTREESIZETEST_H
