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

#ifndef FRANKYCPP_ENGINE_ARENA_ARENACONFIG_H
#define FRANKYCPP_ENGINE_ARENA_ARENACONFIG_H

//=============================================================================
// ArenaConfig.h - Engine Arena Configuration Management
//=============================================================================
//
// ArenaConfig loads and validates configuration for the Engine Arena testing
// framework from YAML files.
// Depends on: types.h, yaml-cpp
//
// Configuration Structure:
//   - Version identifier for the engine being tested
//   - Results directory path (for JSON output)
//   - List of test suite configurations (EPD tactical tests)
//   - List of match configurations (cutechess-cli engine matches)
//
// Test Suite Configuration:
//   - Name: identifier for the test suite (e.g., "WAC", "STS")
//   - EPD path: location of test position file
//   - Time per move: milliseconds to search each position
//   - Max depth: maximum search depth limit
//
// Match Configuration:
//   - Name: identifier for the match (e.g., "v1.1 vs v1.0")
//   - Engine paths: executables for both engines
//   - Cutechess path: location of cutechess-cli executable
//   - Opening book: PGN file for game openings
//   - Time control: format "base+increment" (e.g., "10+0.1")
//   - Rounds: number of games to play
//   - Output PGN: where to save game records
//
// YAML Format Example:
//   version: "v1.1"
//   resultsDir: "./results"
//   testSuites:
//     - name: "WAC"
//       epdPath: "test/testsets/wac.epd"
//       timePerMove: 5000
//       maxDepth: 30
//   matches:
//     - name: "v1.1 vs v1.0"
//       engine1Path: "./FrankyCPP_v1.1.exe"
//       engine2Path: "./FrankyCPP_v1.0.exe"
//       cutechessPath: "D:/Games/Cute Chess/cutechess-cli.exe"
//       openingBook: "books/8moves_GM_LB.pgn"
//       timeControl: "10+0.1"
//       rounds: 100
//       outputPgn: "results/matches/v1.1_vs_v1.0.pgn"
//
// Validation:
//   - Checks all required fields are present
//   - Verifies file paths exist (EPD files, executables, books)
//   - Validates numeric values (positive depths, rounds)
//   - Provides detailed error messages for failures
//
// Path Handling:
//   - All paths in config are relative to project root
//   - Arena executable must be run from project root
//   - Validation uses std::filesystem to check existence
//
// Usage:
//   ArenaConfig config = ArenaConfig::loadFromYaml("config/arena.yaml");
//   if (!config.validate()) {
//     std::cerr << "Configuration validation failed!" << std::endl;
//     return 1;
//   }
//   for (const auto& suite : config.testSuites) {
//     runTestSuite(suite);
//   }
//
//=============================================================================

#include "types/types.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace arena {

  /// Per-suite override settings (optional) for TestSuiteRunConfig
  struct SuiteOverride {
    std::string path;                            ///< EPD file path (required)
    std::optional<milliseconds> timePerMove;     ///< Override time limit
    std::optional<chess::Depth> maxDepth;        ///< Override depth limit
  };

  /// Configuration for a grouped test suite run (new unified format)
  struct TestSuiteRunConfig {
    std::string engine;              ///< Display name: "FrankyCPP v1.5"
    std::string engineVersion;       ///< Version for grouping: "v1.5"
    std::string tag;                 ///< Feature tag: "QuietSee"
    std::string enginePath;          ///< Path to executable
    milliseconds timePerMove;        ///< Time limit per move
    chess::Depth maxDepth;           ///< Maximum search depth
    bool isolatePositions = true;    ///< Clear state between positions
    bool debugMode        = false;   ///< Print UCI communication
    std::string commandLineArgs;     ///< Command-line arguments
    std::string uciOptions;          ///< UCI options
    int parallelWorkers = 1;         ///< Parallel workers
    std::vector<std::variant<std::string, SuiteOverride>> suites; ///< EPD paths or overrides
  };

  /// Configuration for a single EPD test suite
  struct TestSuiteConfig {
    std::string name;             ///< Test suite name (e.g., "WAC", "STS")
    std::string epdPath;          ///< Path to EPD file
    milliseconds timePerMove;     ///< Time limit per move
    chess::Depth maxDepth;               ///< Maximum search depth
    std::string enginePath;       ///< Path to external UCI engine (required)
    std::string engineVersion;    ///< Engine version for results (e.g., "v0.5", "v1.1") - explicit, not parsed from UCI
    bool isolatePositions = true; ///< Clear engine state (TT/history) between positions (default: true)
    bool debugMode        = false;///< Print all UCI communication for debugging
    std::string commandLineArgs;  ///< Command-line arguments to pass to engine (e.g., "--nobook -hash 128")
    std::string uciOptions;       ///< UCI options as semicolon-separated pairs (e.g., "Hash=256; Threads=4")
    int parallelWorkers = 1;      ///< Number of parallel workers (1 = sequential, N>1 = parallel)
    std::string tag;              ///< Feature tag: "QuietSee" (propagated from TestSuiteRunConfig)
  };

  /// Configuration for a single engine match
  struct MatchConfig {
    std::string name;          ///< Match name (e.g., "v1.1 vs v1.0")
    std::string tag;           ///< Feature tag: "QuietSee"
    std::string engine1Path;   ///< Path to first engine executable
    std::string engine1Version;///< Engine 1 version (e.g., "v1.1") - explicit, for results
    std::string engine1Options;///< UCI options for engine 1 (e.g., "OwnBook=false")
    std::string engine2Path;   ///< Path to second engine executable
    std::string engine2Version;///< Engine 2 version (e.g., "v1.0") - explicit, for results
    std::string engine2Options;///< UCI options for engine 2 (e.g., "OwnBook=false")
    std::string cutechessPath; ///< Path to cutechess-cli executable
    std::string openingBook;   ///< Path to opening book (PGN format)
    std::string timeControl;   ///< Time control (e.g., "10+0.1")
    int rounds;                ///< Number of rounds to play
    int concurrency = 1;       ///< Number of games to run in parallel by cutechess-cli (default: 1 for deterministic)
    int batchSize   = 0;       ///< Games per batch for resumable matches (0 = auto: max(2, concurrency), must be even)
    std::string outputPgn;     ///< Path to save PGN games
  };

  /// Configuration for a benchmark run
  struct BenchmarkConfig {
    std::string name;           ///< Benchmark name (e.g., "v1.2 release")
    std::string enginePath;     ///< Path to engine executable (empty = use internal engine)
    std::string engineVersion;  ///< Engine version (e.g., "v1.2") - explicit, for results
    int depth      = 10;        ///< Search depth (default: 10)
    int hashSizeMB = 128;       ///< Hash table size in MB (default: 128)
    int threads    = 1;         ///< Number of threads (default: 1, reserved for future SMP)
    std::string commandLineArgs;///< Command-line arguments (e.g., "--nobook")
    std::string tag;            ///< Feature tag: "QuietSee" (renamed from notes)
  };

  /// Main arena configuration
  struct ArenaConfig {
    std::string version;                         ///< Arena version (e.g., "v1.5") - used for result tracking
    std::string resultsDir;                      ///< Root directory for results
    std::string cutechessPath;                   ///< Path to cutechess-cli executable (global)
    bool debugMode = false;                      ///< Enable cutechess-cli debug output (prints engine I/O)
    std::vector<TestSuiteRunConfig> testSuiteRuns; ///< Grouped test suite run configurations
    std::vector<MatchConfig> matches;            ///< Match configurations
    std::vector<BenchmarkConfig> benchmarks;     ///< Benchmark configurations

    /// Load configuration from YAML file
    /// @param configPath Path to arena.yaml configuration file
    /// @throws std::runtime_error if file not found or invalid YAML
    static ArenaConfig loadFromYaml(const std::string& configPath);

    /// Validate configuration (check paths exist, values are sensible)
    /// @return True if valid, false otherwise
    bool validate() const;

    /// Expand testSuiteRuns into flat list of TestSuiteConfig for runners
    /// @return Vector of individual test suite configs with all settings propagated
    [[nodiscard]] std::vector<TestSuiteConfig> expandTestSuiteRuns() const;
  };

}// namespace arena

#endif// FRANKYCPP_ENGINE_ARENA_ARENACONFIG_H
