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

#include "Benchmark.h"
#include "BenchmarkPositions.h"
#include "Search.h"
#include "SearchLimits.h"
#include "chesscore/Position.h"
#include "config/ConfigManager.h"
#include "types/globals.h"
#include "version.h"

#include <chrono>
#include <format>
#include <iostream>

namespace engine {

BenchResult Benchmark::run(const BenchConfig& config) {
  // Convert default positions to vector
  std::vector<std::string> fens;
  fens.reserve(benchmark::BENCH_POSITION_COUNT);
  for (const auto& fen : benchmark::BENCH_FENS) {
    fens.emplace_back(fen);
  }
  return run(fens, config);
}

BenchResult Benchmark::run(const std::vector<std::string>& fens, const BenchConfig& config) {
  BenchResult result;
  result.version = "FrankyCPP v" + std::to_string(FrankyCPP_VERSION_MAJOR) + "." +
                   std::to_string(FrankyCPP_VERSION_MINOR);

  // Configure hash size via config override
  config::ConfigManager::instance().applyOverrides(
    [&](config::SearchConfigData& s, config::EvalConfigData&) {
      s.TT_SIZE_MB = config.hashSizeMB;
    });

  // Create a search instance (without UCI handler - we don't want UCI output during bench)
  Search search;
  search.newGame();  // Clear TT and history

  // Set up search limits for depth-limited search
  SearchLimits limits;
  limits.depth = config.depth;
  if (config.timeLimit.count() > 0) {
    limits.timeControl = false;  // We use moveTime, not time control
  }

  // Track timing
  const auto benchStartTime = steady_clock::now();

  // Process each position
  int positionNum = 0;
  for (const auto& fen : fens) {
    positionNum++;

    // Parse position
    Position position;
    try {
      position = Position(fen);
    }
    catch (const std::exception& e) {
      std::cerr << "Skipping invalid FEN [" << positionNum << "]: " << fen << " (" << e.what() << ")\n";
      continue;
    }

    // Print progress to stderr
    std::cerr << "\rPosition " << positionNum << "/" << fens.size() << std::flush;

    // Clear state between positions (like ucinewgame but keeps TT for efficiency)
    // We don't clear TT to better simulate real game behavior where TT persists
    // This is consistent with Stockfish's bench behavior

    // Start search
    search.startSearch(position, limits);
    search.waitWhileSearching();

    // Collect results
    const auto& searchResult = search.getLastSearchResult();
    result.totalNodes += searchResult.nodes;
    result.positionsRun++;
  }

  // Calculate total time
  const auto benchEndTime = steady_clock::now();
  result.totalTime = std::chrono::duration_cast<milliseconds>(benchEndTime - benchStartTime);

  // Calculate NPS (avoid division by zero)
  if (result.totalTime.count() > 0) {
    result.nps = static_cast<double>(result.totalNodes) * 1000.0 /
                 static_cast<double>(result.totalTime.count());
  }

  std::cerr << "\n";  // New line after progress indicator

  return result;
}

void Benchmark::printResults(const BenchResult& result) {
  const double totalTimeSec = static_cast<double>(result.totalTime.count()) / 1000.0;
  const auto npsInt = static_cast<uint64_t>(result.nps);

  std::cerr << std::format(deLocale,
    "\n"
    "===================================\n"
    "FrankyCPP Benchmark Results    \n"
    "===================================\n"
    "Version        :  {}\n"
    "Positions      :  {}\n"
    "-----------------------------------\n"
    "Total nodes    :  {:>15L}\n"
    "Total time [s] :  {:>15.2f}\n"
    "Nodes/second   :  {:>15L}\n"
    "===================================\n",
    result.version,
    result.positionsRun,
    result.totalNodes,
    totalTimeSec,
    npsInt);
}

} // namespace engine
