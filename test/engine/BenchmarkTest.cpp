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

#include "engine/Benchmark.h"
#include "chesscore/Position.h"
#include "engine/BenchmarkPositions.h"
#include "init.h"

#include <gtest/gtest.h>

using namespace engine;
using namespace chess;

/// Test fixture for Benchmark tests
class BenchmarkTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();// Initialize attack tables/magics
    NEWLINE;
  }
};

/// Test that benchmark positions are valid FENs
TEST_F(BenchmarkTest, benchmarkPositionsAreValid) {
  EXPECT_EQ(benchmark::BENCH_POSITION_COUNT, 50);

  for (std::size_t i = 0; i < benchmark::BENCH_POSITION_COUNT; ++i) {
    const auto& fen = benchmark::BENCH_FENS[i];
    EXPECT_FALSE(fen.empty()) << "Position " << i << " is empty";
    // Position constructor will throw if FEN is invalid
    EXPECT_NO_THROW({
      [[maybe_unused]] Position pos{std::string{fen}};
    }) << "Position "
       << i << " has invalid FEN: " << fen;
  }
}

/// Test benchmark configuration defaults
TEST_F(BenchmarkTest, benchConfigDefaults) {
  constexpr engine::BenchConfig config;
  EXPECT_EQ(config.hashSizeMB, 128);
  EXPECT_EQ(config.threads, 1);
  EXPECT_EQ(config.depth, 12);
  EXPECT_EQ(config.timeLimit.count(), 0);
}

/// Test that benchmark result initialization
TEST_F(BenchmarkTest, benchResultDefaults) {
  const engine::BenchResult result;
  EXPECT_EQ(result.totalNodes, 0);
  EXPECT_EQ(result.totalTime.count(), 0);
  EXPECT_DOUBLE_EQ(result.nps, 0.0);
  EXPECT_EQ(result.positionsRun, 0);
  EXPECT_TRUE(result.version.empty());
}

/// Test benchmark with single position at shallow depth
TEST_F(BenchmarkTest, benchRunsSinglePosition) {
  engine::BenchConfig config;
  config.depth = 1;// Very shallow for quick test

  // Run with just one position for quick test
  const std::vector<std::string> fens = {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
  const auto result                   = engine::Benchmark::run(fens, config);

  EXPECT_GT(result.totalNodes, 0);
  EXPECT_GT(result.totalTime.count(), 0);
  EXPECT_GT(result.nps, 0.0);
  EXPECT_EQ(result.positionsRun, 1);
  EXPECT_FALSE(result.version.empty());
}

/// Test that benchmark runs without crashing at shallow depth
TEST_F(BenchmarkTest, benchRunsAtShallowDepth) {
  engine::BenchConfig config;
  config.depth      = 3; // Shallow depth for quick test
  config.hashSizeMB = 16;// Small hash for test

  // Run with a few positions
  const std::vector<std::string> fens = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11"};

  const auto result = engine::Benchmark::run(fens, config);

  EXPECT_EQ(result.positionsRun, 3);
  EXPECT_GT(result.totalNodes, 0);
  EXPECT_GT(result.nps, 0.0);
}

/// Test print function doesn't crash
TEST_F(BenchmarkTest, printResultsDoesNotCrash) {
  engine::BenchResult result;
  result.totalNodes   = 12345678;
  result.totalTime    = milliseconds{5000};
  result.nps          = 2469135.6;
  result.positionsRun = 50;
  result.version      = "FrankyCPP v1.2";

  // Should not throw or crash
  EXPECT_NO_THROW(engine::Benchmark::printResults(result));
}
