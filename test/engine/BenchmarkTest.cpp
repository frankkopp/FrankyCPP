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
#include "version.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace engine;
using namespace chess;

/// Test fixture for Benchmark tests
class BenchmarkTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init(); // Initialize attack tables/magics
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
  constexpr BenchConfig config;
  EXPECT_EQ(config.hashSizeMB, 128);
  EXPECT_EQ(config.threads, 1);
  EXPECT_EQ(config.depth, 12);
  EXPECT_EQ(config.timeLimit.count(), 0);
}

/// Test that benchmark result initialization
TEST_F(BenchmarkTest, benchResultDefaults) {
  const BenchResult result;
  EXPECT_EQ(result.totalNodes, 0);
  EXPECT_EQ(result.totalTime.count(), 0);
  EXPECT_DOUBLE_EQ(result.nps, 0.0);
  EXPECT_EQ(result.positionsRun, 0);
  EXPECT_TRUE(result.version.empty());
}

/// Test benchmark with single position at shallow depth
TEST_F(BenchmarkTest, benchRunsSinglePosition) {
  BenchConfig config;
  config.depth = 1; // Very shallow for quick test

  // Run with just one position for quick test
  const std::vector<std::string> fens = {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
  const auto result                   = Benchmark::run(fens, config);

  EXPECT_GT(result.totalNodes, 0);
  EXPECT_GT(result.totalTime.count(), 0);
  EXPECT_GT(result.nps, 0.0);
  EXPECT_EQ(result.positionsRun, 1);
  EXPECT_FALSE(result.version.empty());
}

/// Test that benchmark runs without crashing at shallow depth
TEST_F(BenchmarkTest, benchRunsAtShallowDepth) {
  BenchConfig config;
  config.depth      = 3;  // Shallow depth for quick test
  config.hashSizeMB = 16; // Small hash for test

  // Run with a few positions
  const std::vector<std::string> fens = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11"};

  const auto result = Benchmark::run(fens, config);

  EXPECT_EQ(result.positionsRun, 3);
  EXPECT_GT(result.totalNodes, 0);
  EXPECT_GT(result.nps, 0.0);
}

/// Test print function doesn't crash
TEST_F(BenchmarkTest, printResultsDoesNotCrash) {
  BenchResult result;
  result.totalNodes   = 12345678;
  result.totalTime    = milliseconds{5000};
  result.nps          = 2469135.6;
  result.positionsRun = 50;
  result.version      = "FrankyCPP v1.2";
  result.signature    = result.totalNodes;

  // Should not throw or crash
  EXPECT_NO_THROW(engine::Benchmark::printResults(result));
}

/// Test that signature equals totalNodes
TEST_F(BenchmarkTest, benchSignatureMatchesTotalNodes) {
  BenchConfig config;
  config.depth      = 3;
  config.hashSizeMB = 16;
  config.threads    = 1;

  const std::vector<std::string> fens = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11"};

  const auto result = Benchmark::run(fens, config);

  EXPECT_GT(result.signature, 0U);
  EXPECT_EQ(result.signature, result.totalNodes);
}

/// Test that bench signature is deterministic across two identical runs
TEST_F(BenchmarkTest, benchSignatureIsDeterministic) {
  BenchConfig config;
  config.depth      = 5;
  config.hashSizeMB = 16;
  config.threads    = 1;

  const std::vector<std::string> fens = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11"};

  const auto result1 = Benchmark::run(fens, config);
  const auto result2 = Benchmark::run(fens, config);

  EXPECT_GT(result1.signature, 0U);
  EXPECT_EQ(result1.signature, result2.signature);
}

/// Test that bench signature changes with different depth
TEST_F(BenchmarkTest, benchSignatureChangesWithDepth) {
  const std::vector<std::string> fens = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10"};

  BenchConfig config3;
  config3.depth      = 3;
  config3.hashSizeMB = 16;
  config3.threads    = 1;

  BenchConfig config5;
  config5.depth      = 5;
  config5.hashSizeMB = 16;
  config5.threads    = 1;

  const auto result3 = Benchmark::run(fens, config3);
  const auto result5 = Benchmark::run(fens, config5);

  EXPECT_NE(result3.signature, result5.signature);
}

/// Test that printResults outputs a parseable Bench: line
TEST_F(BenchmarkTest, printResultsOutputsBenchLine) {
  BenchResult result;
  result.totalNodes   = 12345678;
  result.totalTime    = milliseconds{5000};
  result.nps          = 2469135.6;
  result.positionsRun = 50;
  result.version      = "FrankyCPP v1.2";
  result.signature    = result.totalNodes;

  // Capture stdout
  const auto oldBuf = std::cout.rdbuf();
  const std::ostringstream capture;
  std::cout.rdbuf(capture.rdbuf());

  Benchmark::printResults(result);

  std::cout.rdbuf(oldBuf);

  const std::string output = capture.str();
  EXPECT_NE(output.find("Bench: 12345678"), std::string::npos)
    << "Expected 'Bench: 12345678' in output, got:\n"
    << output;
}

/// Verify bench signature matches the committed bench_signature.txt file.
/// Catches search regressions locally before CI — uses the same config as CI:
/// depth 12, hash 128 MB, threads 1.
/// File format: one "compiler value" pair per line (msvc, gcc, clang).
TEST_F(BenchmarkTest, benchSignatureMatchesCommitted) {
  // Determine which compiler label to look for
#if defined(_MSC_VER)
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string compilerLabel = "msvc";
#elif defined(__clang__)
  const std::string compilerLabel = "clang";
#elif defined(__GNUC__)
  const std::string compilerLabel = "gcc";
#else
  GTEST_SKIP() << "Unknown compiler — cannot match bench signature";
#endif

  // Read expected signature from bench_signature.txt at project root
  const std::filesystem::path sigPath =
    std::filesystem::path(FrankyCPP_PROJECT_ROOT) / "bench_signature.txt";
  ASSERT_TRUE(std::filesystem::exists(sigPath))
    << "bench_signature.txt not found at: " << sigPath;

  std::ifstream sigFile(sigPath);
  ASSERT_TRUE(sigFile.is_open()) << "Could not open " << sigPath;

  uint64_t expectedSignature = 0;
  std::string label;
  uint64_t value = 0;
  while (sigFile >> label >> value) {
    if (label == compilerLabel) {
      expectedSignature = value;
      break;
    }
  }
  ASSERT_GT(expectedSignature, 0U)
    << "No signature found for compiler '" << compilerLabel << "' in " << sigPath;

  // Run bench with CI-identical config: depth 12, hash 128, threads 1
  BenchConfig config;
  config.depth      = 12;
  config.hashSizeMB = 128;
  config.threads    = 1;

  const auto result = Benchmark::run(config);

  EXPECT_EQ(result.signature, expectedSignature)
    << "Bench signature mismatch! Search behavior has changed.\n"
    << "  Compiler:                            " << compilerLabel << "\n"
    << "  Expected (from bench_signature.txt): " << expectedSignature << "\n"
    << "  Actual:                              " << result.signature << "\n"
    << "If intentional, update the '" << compilerLabel << "' line in bench_signature.txt.";
}
