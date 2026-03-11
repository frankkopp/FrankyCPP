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

#include "BenchmarkRunner.h"
#include "UCIEngine.h"
#include "common/TimeUtils.h"
#include "engine/Benchmark.h"
#include "engine/BenchmarkPositions.h"
#include "types/globals.h"

#include <chrono>
#include <format>
#include <iostream>

namespace arena {

  using namespace engine;
  using namespace chess;

  BenchmarkRunner::BenchmarkRunner(const BenchmarkConfig& config, const std::string& arenaVersion)
      : config_(config), arenaVersion_(arenaVersion) {}

  BenchmarkResult BenchmarkRunner::run() const {
    // Check if we should use internal or external engine
    if (config_.enginePath.empty()) {
      return runInternal();
    }
    return runExternal();
  }

  BenchmarkResult BenchmarkRunner::runInternal() const {
    std::cerr << "Using internal engine\n";

    // Configure the benchmark
    engine::BenchConfig benchConfig;
    benchConfig.depth      = config_.depth;
    benchConfig.hashSizeMB = config_.hashSizeMB;
    benchConfig.threads    = config_.threads;

    // Run the benchmark
    const auto engineResult = engine::Benchmark::run(benchConfig);

    // Convert to arena result format
    BenchmarkResult result;
    result.arenaVersion  = arenaVersion_;
    result.timestamp     = common::isoTimestamp();
    result.engineName    = "FrankyCPP";
    result.engineVersion = config_.engineVersion;
    result.enginePath    = ""; // Internal
    result.depth         = config_.depth;
    result.hashSizeMB    = config_.hashSizeMB;
    result.threads       = config_.threads;
    result.positions     = engineResult.positionsRun;
    result.totalNodes    = engineResult.totalNodes;
    result.totalTimeMs   = engineResult.totalTime.count();
    result.nps           = static_cast<uint64_t>(engineResult.nps);
    result.tag           = config_.tag;

    return result;
  }

  BenchmarkResult BenchmarkRunner::runExternal() const {
    std::cerr << "Using external engine: " << config_.enginePath << "\n";

    BenchmarkResult result;
    result.arenaVersion  = arenaVersion_;
    result.timestamp     = common::isoTimestamp();
    result.engineVersion = config_.engineVersion;
    result.enginePath    = config_.enginePath;
    result.depth         = config_.depth;
    result.hashSizeMB    = config_.hashSizeMB;
    result.threads       = config_.threads;
    result.tag           = config_.tag;

    try {
      // Start external engine with Hash and Threads options
      const std::string uciOptions = "Hash=" + std::to_string(config_.hashSizeMB) + "; Threads=" + std::to_string(config_.threads);

      UCIEngine engine(config_.enginePath, config_.commandLineArgs, false, uciOptions);
      result.engineName = "External"; // Could extract from UCI "id name" response

      uint64_t totalNodes       = 0;
      int64_t totalSearchTimeMs = 0; // Sum of actual search times only
      int positionsRun          = 0;

      // Run through all benchmark positions
      for (std::size_t i = 0; i < benchmark::BENCH_POSITION_COUNT; ++i) {
        const auto& fen = benchmark::BENCH_FENS[i];

        // Progress indicator
        std::cerr << "\rPosition " << (i + 1) << "/" << benchmark::BENCH_POSITION_COUNT << std::flush;

        // Clear engine state before each position for fair, independent measurement
        // This is done BEFORE timing starts so TT clearing doesn't affect NPS
        engine.newGame();

        // Set position (also before timing - this is UCI overhead, not search)
        if (!engine.setPosition(std::string{fen})) {
          std::cerr << "\nWarning: Failed to set position " << (i + 1) << ": " << fen << "\n";
          continue;
        }

        // Measure only the search time, not UCI overhead or TT clearing
        const auto searchStart = steady_clock::now();

        // Search with depth limit (no time limit for benchmark consistency)
        // Use a generous time limit that should never be hit
        UCISearchResult searchResult = engine.search(milliseconds{300000}, static_cast<Depth>(config_.depth));

        const auto searchEnd  = steady_clock::now();
        const auto searchTime = std::chrono::duration_cast<milliseconds>(searchEnd - searchStart);

        if (searchResult.bestMove.empty()) {
          std::cerr << "\nWarning: No best move for position " << (i + 1) << "\n";
          continue;
        }

        totalNodes += searchResult.nodes;
        totalSearchTimeMs += searchTime.count();
        positionsRun++;
      }

      std::cerr << "\n"; // New line after progress

      result.positions   = positionsRun;
      result.totalNodes  = totalNodes;
      result.totalTimeMs = totalSearchTimeMs; // Only search time, no UCI overhead

      // Calculate NPS
      if (result.totalTimeMs > 0) {
        result.nps = static_cast<uint64_t>(
          static_cast<double>(result.totalNodes) * 1000.0 / static_cast<double>(result.totalTimeMs));
      }

    } catch (const std::exception& e) {
      std::cerr << "Error running external benchmark: " << e.what() << "\n";
      result.tag = "ERROR: " + std::string(e.what());
    }

    return result;
  }

  void BenchmarkRunner::printResultsTable(const std::vector<BenchmarkResult>& results) {
    if (results.empty()) {
      std::cout << "No benchmark results to display.\n";
      return;
    }

    // Print header
    std::cout << "\n";
    std::cout << "=================================================================================================\n";
    std::cout << "                               BENCHMARK RESULTS HISTORY                                         \n";
    std::cout << "=================================================================================================\n";
    std::cout << std::format("{:<20} {:>8} {:>6} {:>6} {:>15} {:>15} {:>10}\n",
                             "Timestamp", "Version", "Depth", "Hash", "Nodes", "NPS", "Time [s]");
    std::cout << "-------------------------------------------------------------------------------------------------\n";

    // Print each result
    for (const auto& r : results) {
      // Format timestamp to be more compact (remove T, show only date + time)
      std::string ts = r.timestamp;
      if (ts.length() >= 19) {
        ts = ts.substr(0, 10) + " " + ts.substr(11, 8); // "YYYY-MM-DD HH:MM:SS"
      }

      const double timeSec = static_cast<double>(r.totalTimeMs) / 1000.0;

      // Mark external engines
      std::string version = r.engineVersion;
      if (!r.enginePath.empty()) {
        version += "*"; // Asterisk indicates external engine
      }

      std::cout << std::format(projectLocale, "{:<20} {:>8} {:>6} {:>4}MB {:>15L} {:>15L} {:>10.2f}\n",
                               ts,
                               version,
                               r.depth,
                               r.hashSizeMB,
                               r.totalNodes,
                               r.nps,
                               timeSec);
    }

    std::cout << "-------------------------------------------------------------------------------------------------\n";

    // Check if any external engines
    bool hasExternal = false;
    for (const auto& r : results) {
      if (!r.enginePath.empty()) {
        hasExternal = true;
        break;
      }
    }
    if (hasExternal) {
      std::cout << "  * = external engine\n";
    }

    // Print tags if any exist
    bool hasTags = false;
    for (const auto& r : results) {
      if (!r.tag.empty()) {
        hasTags = true;
        break;
      }
    }

    if (hasTags) {
      std::cout << "\nTags:\n";
      for (const auto& r : results) {
        if (!r.tag.empty()) {
          std::string ts = r.timestamp;
          if (ts.length() >= 10) {
            ts = ts.substr(0, 10); // Just date
          }
          std::cout << "  [" << ts << " " << r.engineVersion << "] " << r.tag << "\n";
        }
      }
    }

    std::cout << "=================================================================================================\n";
  }

} // namespace arena
