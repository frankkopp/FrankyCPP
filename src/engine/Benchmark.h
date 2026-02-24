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

#ifndef FRANKYCPP_BENCHMARK_H
#define FRANKYCPP_BENCHMARK_H

//=============================================================================
// Benchmark.h - Engine Benchmark for NPS Measurement
//=============================================================================
//
// Provides a standardized benchmark command (`bench`) for measuring engine
// performance in nodes per second (NPS). Used for:
//   - Comparing performance between engine versions
//   - Detecting performance regressions
//   - Validating optimizations
//
// Usage:
//   UCI command: bench [depth] [hash] [threads]
//   Default: bench 13 128 1
//
// Output format (to stderr):
//   ===========================
//   FrankyCPP Bench Results
//   ===========================
//   Version            : FrankyCPP v1.2
//   Hash size [MB]     : 128
//   Threads            : 1
//   Depth limit        : 13
//   Positions          : 50
//   ---------------------------
//   Total nodes        : 45,123,456
//   Total time [s]     : 28.5
//   Nodes/second       : 1,583,279
//   ===========================
//
// The benchmark uses a fixed set of 50 positions from public domain test
// suites (WAC, Kaufman, Eigenmann) to ensure reproducibility.
//
//=============================================================================

#include <cstdint>
#include <string>
#include <vector>

#include "types/types.h"

namespace engine {

/// Configuration for the benchmark run
struct BenchConfig {
  int hashSizeMB = 128;       ///< Transposition table size in MB
  int threads    = 1;         ///< Number of threads (future SMP support)
  int depth      = 12;        ///< Search depth limit (1-127)
  milliseconds timeLimit{0};  ///< Time limit per position (0 = use depth only)
};

/// Result of a benchmark run
struct BenchResult {
  uint64_t totalNodes   = 0;      ///< Total nodes searched across all positions
  milliseconds totalTime{0};      ///< Total wall-clock time
  double nps            = 0.0;    ///< Nodes per second
  int positionsRun      = 0;      ///< Number of positions benchmarked
  std::string version;            ///< Engine version string
};

/// Benchmark runner for standardized NPS measurement
class Benchmark {
public:
  /// Run the benchmark with default positions
  /// @param config Benchmark configuration (depth, hash, threads)
  /// @return Benchmark results including NPS
  static BenchResult run(const BenchConfig& config = {});

  /// Run the benchmark with custom positions (for testing)
  /// @param fens Vector of FEN strings to benchmark
  /// @param config Benchmark configuration
  /// @return Benchmark results
  static BenchResult run(const std::vector<std::string>& fens, const BenchConfig& config = {});

  /// Print benchmark results to stderr (UCI-compatible format)
  /// @param result The benchmark results to print
  static void printResults(const BenchResult& result);
};

} // namespace engine

#endif // FRANKYCPP_BENCHMARK_H
