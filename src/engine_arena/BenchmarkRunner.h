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

#ifndef FRANKYCPP_ENGINE_ARENA_BENCHMARKRUNNER_H
#define FRANKYCPP_ENGINE_ARENA_BENCHMARKRUNNER_H

//=============================================================================
// BenchmarkRunner.h - Engine Arena Benchmark Runner
//=============================================================================
//
// BenchmarkRunner executes performance benchmarks for the Arena framework.
// It runs the internal benchmark and captures results for persistence.
//
// Features:
//   - Runs the standard 50-position benchmark
//   - Captures NPS, nodes, and timing data
//   - Formats results for Arena's result persistence
//   - Supports configuration via BenchmarkConfig
//
// Usage:
//   BenchmarkRunner runner(config, "v1.2");
//   BenchmarkResult result = runner.run();
//   resultWriter.writeBenchmarkResult(result);
//
//=============================================================================

#include "ArenaConfig.h"
#include "ArenaResults.h"

#include <string>

namespace arena {

/// Runs benchmarks and captures results for Arena persistence
class BenchmarkRunner {
public:
  /// Creates a benchmark runner with the given configuration
  /// @param config Benchmark configuration
  /// @param arenaVersion Arena version string for result metadata
  explicit BenchmarkRunner(const BenchmarkConfig& config, const std::string& arenaVersion);

  /// Runs the benchmark and returns results
  /// Uses internal engine if enginePath is empty, otherwise uses external UCI engine
  /// @return Benchmark results including NPS, nodes, and timing
  [[nodiscard]] BenchmarkResult run() const;

  /// Prints a formatted table of benchmark results
  /// @param results Vector of benchmark results to display
  static void printResultsTable(const std::vector<BenchmarkResult>& results);

private:
  /// Run benchmark using internal engine (engine::Benchmark)
  [[nodiscard]] BenchmarkResult runInternal() const;

  /// Run benchmark using external UCI engine
  [[nodiscard]] BenchmarkResult runExternal() const;

  BenchmarkConfig config_;
  std::string arenaVersion_;
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_BENCHMARKRUNNER_H
