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
#include "common/Logging.h"
#include "config/ConfigManager.h"
#include "types/globals.h"
#include "version.h"

#include <chrono>
#include <format>
#include <iostream>

using namespace common;
using namespace chess;
namespace engine {

  using namespace config;

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
    result.version = "FrankyCPP v" + std::to_string(FrankyCPP_VERSION_MAJOR)
                     + "."
                     + std::to_string(FrankyCPP_VERSION_MINOR)
                     + "."
                     + std::to_string(FrankyCPP_VERSION_PATCH);

    // Configure hash size and threads via config override
    ConfigManager::instance().applyOverrides(
      [&](SearchConfigData& s, EvalConfigData&) {
        s.TT_SIZE_MB = config.hashSizeMB;
        s.THREADS    = config.threads;
      });

    // Create a search instance (without UCI handler - we don't want UCI output during bench)
    Search search;
    search.newGame(); // Clear TT and history

    // Set up search limits for depth-limited search
    SearchLimits limits;
    limits.depth = config.depth;
    if (config.timeLimit.count() > 0) {
      limits.timeControl = false; // We use moveTime, not time control
    }

    // Track cumulative search time (excludes TT clearing and position setup)
    int64_t totalSearchTimeMs = 0;

    // Process each position
    int positionNum = 0;
    for (const auto& fen : fens) {
      positionNum++;

      // Parse position
      Position position;
      try {
        position = Position(fen);
      } catch (const std::exception& e) {
        LOG__ERROR(Logger::get().APP_LOG, "Skipping invalid FEN [{}]: {} ({})", positionNum, fen, e.what());
        continue;
      }

      // Print progress to stderr
      std::cout << "\rPosition " << positionNum << "/" << fens.size() << std::flush;

      // Clear TT before each position for fair, independent measurement
      // Done BEFORE timing so TT clearing doesn't affect NPS
      search.newGame();

      // Measure only search time
      const auto searchStart = steady_clock::now();

      // Start search
      search.startSearch(position, limits);
      search.waitWhileSearching();

      const auto searchEnd = steady_clock::now();
      totalSearchTimeMs += duration_cast<milliseconds>(searchEnd - searchStart).count();

      // Collect results
      const auto& searchResult = search.getLastSearchResult();
      result.totalNodes += searchResult.nodes;
      result.positionsRun++;
    }

    // Set total time (only search time, no TT clearing overhead)
    result.totalTime = milliseconds{totalSearchTimeMs};

    // Deterministic bench signature for CI regression gate
    result.signature = result.totalNodes;

    // Calculate NPS (avoid division by zero)
    if (result.totalTime.count() > 0) {
      result.nps = static_cast<double>(result.totalNodes) * 1000.0 / static_cast<double>(result.totalTime.count());
    }

    std::cout << "\n"; // New line after progress indicator

    return result;
  }

  void Benchmark::printResults(const BenchResult& result) {
    const double totalTimeSec = static_cast<double>(result.totalTime.count()) / 1000.0;
    const auto npsInt         = static_cast<uint64_t>(result.nps);

    std::cout << std::format(projectLocale,
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

    // Unformatted signature line for CI/script parsing (no locale separators)
    std::cout << "\nBench: " << result.signature << std::endl;
  }

} // namespace engine
