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
#include "TT.h"
#include "chesscore/Position.h"
#include "common/Logging.h"
#include "config/ConfigManager.h"
#include "types/globals.h"
#include "types/timeunits.h"
#include "version.h"

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

    // Reset R6 instrumentation counters for this bench session
    if constexpr (TT::TT_INSTRUMENTATION) {
      search.resetTTInstrumentation();
    }

    // Set up search limits for depth-limited search
    SearchLimits limits;
    limits.depth = config.depth;
    if (config.timeLimit.count() > 0) {
      limits.timeControl = false; // We use moveTime, not time control
    }

    // Track cumulative search time (excludes TT clearing and position setup)
    nanoseconds totalSearchTime{0};

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
      const auto searchStart = currentTime();

      // Start search
      search.startSearch(position, limits);
      search.waitWhileSearching();

      totalSearchTime += elapsedSince(searchStart);

      // Collect results
      const auto& searchResult = search.getLastSearchResult();
      result.totalNodes += searchResult.nodes;
      result.positionsRun++;
    }

    // Set total time (only search time, no TT clearing overhead)
    result.totalTime = duration_cast<milliseconds>(totalSearchTime);

    // Deterministic bench signature for CI regression gate
    result.signature = result.totalNodes;

    // Calculate NPS using timeunits helper (handles zero-duration safely)
    result.nps = static_cast<double>(nps(result.totalNodes, totalSearchTime));

    // Collect R6 TT instrumentation report (cumulative across all positions)
    if constexpr (TT::TT_INSTRUMENTATION) {
      result.ttInstrumentationReport = search.ttInstrumentationStr();
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
    // Use std::format (without L) to guarantee no thousands separators even if
    // a locale has been imbued on std::cout by other code / tests.
    std::cout << std::format("\nBench: {}\n", result.signature) << std::flush;

    // Print R6 TT instrumentation data if collected
    if (!result.ttInstrumentationReport.empty()) {
      std::cout << "\n" << result.ttInstrumentationReport << "\n" << std::flush;
    }
  }

} // namespace engine
