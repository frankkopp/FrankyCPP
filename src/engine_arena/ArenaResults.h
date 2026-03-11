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

#ifndef FRANKYCPP_ENGINE_ARENA_RESULTS_H
#define FRANKYCPP_ENGINE_ARENA_RESULTS_H

//=============================================================================
// ArenaResults.h - Engine Arena Result Data Structures
//=============================================================================
//
// Defines data structures for capturing and storing test suite and match
// results from the Engine Arena framework.
// Depends on: types.h
//
// TestSuiteResult:
//   Aggregates results from running an EPD test suite (e.g., WAC, STS).
//   Contains overall statistics and per-test details for later analysis.
//
//   Fields:
//   - version: Engine version tested (e.g., "v1.1")
//   - suiteName: Test suite identifier (e.g., "WAC")
//   - timestamp: ISO 8601 timestamp of test execution
//   - totalTests: Number of positions tested
//   - passed/failed/skipped: Breakdown by result type
//   - totalNodes: Sum of nodes searched across all tests
//   - totalTimeMs: Sum of time spent (milliseconds)
//   - details: Vector of per-test case information
//
// TestCaseDetail:
//   Per-position breakdown within a test suite.
//
//   Fields:
//   - testId: Position identifier (e.g., "WAC.001")
//   - fen: Position FEN string
//   - expected: Expected move(s) or result
//   - actual: Engine's chosen move
//   - passed: Whether test succeeded
//   - nodes: Nodes searched for this position
//   - timeMs: Time spent on this position
//
// MatchResult:
//   Aggregates results from an engine-vs-engine match via cutechess-cli.
//   Contains game outcomes and calculated ELO difference.
//
//   Fields:
//   - version: Engine version being tested
//   - matchName: Match identifier (e.g., "v1.1 vs v1.0 rapid")
//   - timestamp: ISO 8601 timestamp of match start
//   - engine1Name/engine2Name: Engine identifiers
//   - engine1Wins/engine2Wins/draws: Game outcomes
//   - engine1Score/engine2Score: Total points (win=1, draw=0.5, loss=0)
//   - eloDifference: Calculated ELO rating difference
//   - pgnPath: Path to saved game records
//   - durationMs: Total match duration
//
// Serialization:
//   These structures are serialized to JSON by ResultStore for:
//   - Long-term storage and historical tracking
//   - Automated comparison between versions
//   - Analysis and visualization tools
//
// ReportData:
//   Pure data bag holding all results organized for reporting.
//   Query methods (findEngine, getResult, getMatch, etc.) are in ResultStore.
//
// Usage:
//   TestSuiteResult result;
//   result.engineName = "FrankyCPP";
//   result.engineVersion = "v1.1";
//   result.testSuiteName = "WAC";
//   result.totalTests = 300;
//   result.passed = 285;
//   // ... populate details ...
//   resultStore.writeTestSuiteResult(result);
//
//=============================================================================

#include <map>
#include <set>
#include <string>
#include <vector>

namespace arena {

  //=============================================================================
  // EngineId - Engine identifier (name + version)
  //=============================================================================

  /// Uniquely identifies an engine by name and version
  struct EngineId {
    std::string name;    ///< Engine name (e.g., "FrankyCPP", "FrankyGo")
    std::string version; ///< Engine version (e.g., "v1.1", "v1.0.3")

    /// Returns string representation: "EngineName-Version" (e.g., "FrankyCPP-v1.1")
    [[nodiscard]] std::string toString() const {
      if (version.empty()) return name;
      return name + "-" + version;
    }

    /// Returns display string: "EngineName Version" (e.g., "FrankyCPP v1.1")
    /// Avoids duplicating version if name already contains it
    [[nodiscard]] std::string toDisplayString() const {
      if (version.empty()) return name;
      // Check if name already ends with the version
      if (name.ends_with(version)) {
        return name;
      }
      // Check if name already contains the version somewhere
      if (name.find(version) != std::string::npos) {
        return name;
      }
      return name + " " + version;
    }

    /// Comparison operator for use in std::map/set (sorts by name, then version)
    [[nodiscard]] bool operator<(const EngineId& other) const {
      if (name != other.name) return name < other.name;
      return version < other.version;
    }

    /// Equality operator
    [[nodiscard]] bool operator==(const EngineId& other) const {
      return name == other.name && version == other.version;
    }

    /// Parse from string format "EngineName-Version"
    static EngineId fromString(const std::string& str) {
      const auto pos = str.rfind('-');
      if (pos == std::string::npos) {
        return {str, ""};
      }
      return {str.substr(0, pos), str.substr(pos + 1)};
    }
  };

  /// Detail for a single test case within a test suite
  struct TestCaseDetail {
    std::string testId;   ///< Test identifier
    std::string fen;      ///< Position FEN
    std::string expected; ///< Expected move(s)
    std::string actual;   ///< Actual move played
    bool passed;          ///< Test passed
    uint64_t nodes;       ///< Nodes searched
    int64_t timeMs;       ///< Time taken in milliseconds
  };

  /// Result from running a test suite
  struct TestSuiteResult {
    // Arena metadata
    std::string arenaVersion; ///< Arena version that ran this test (e.g., "v1.1")
    std::string timestamp;    ///< ISO 8601 timestamp

    // Test suite identification
    std::string testSuiteName; ///< Clean test suite name (e.g., "WAC", "Crafty")
    std::string epdPath;       ///< Path to EPD file

    // Engine identification
    std::string engineName;    ///< Engine name (e.g., "FrankyCPP", "FrankyGo")
    std::string engineVersion; ///< Engine version (e.g., "v0.5", "v1.1")
    std::string enginePath;    ///< Path to engine executable (empty if internal)

    // Feature tracking
    std::string tag; ///< Feature tag (e.g., "QuietSee", "TTbuckets")

    // Results summary
    int totalTests;      ///< Total number of tests
    int passed;          ///< Number of passed tests
    int failed;          ///< Number of failed tests
    int skipped;         ///< Number of skipped tests
    uint64_t totalNodes; ///< Total nodes searched
    int64_t totalTimeMs; ///< Total time in milliseconds

    // Per-test details (optional, can be pruned)
    std::vector<TestCaseDetail> details; ///< Per-test details

    /// Returns EngineId from this result
    [[nodiscard]] EngineId getEngineId() const {
      return {engineName, engineVersion};
    }

    /// Returns pass rate as percentage (0.0 - 100.0)
    [[nodiscard]] double getPassRate() const {
      return totalTests > 0 ? (passed * 100.0 / totalTests) : 0.0;
    }

    /// Returns average time per test in milliseconds
    [[nodiscard]] double getAvgTimeMs() const {
      return totalTests > 0 ? (static_cast<double>(totalTimeMs) / totalTests) : 0.0;
    }

    /// Returns average nodes per test
    [[nodiscard]] double getAvgNodes() const {
      return totalTests > 0 ? (static_cast<double>(totalNodes) / totalTests) : 0.0;
    }
  };

  /// Result from running an engine match
  struct MatchResult {
    // Arena metadata
    std::string arenaVersion; ///< Arena version that ran this match (e.g., "v1.1")
    std::string timestamp;    ///< ISO 8601 timestamp

    // Match identification
    std::string matchName; ///< Match identifier (e.g., "Rapid 60+0.6")

    // Feature tracking
    std::string tag; ///< Feature tag (e.g., "QuietSee", "TTbuckets")

    // Engine 1 identification
    std::string engine1Name;    ///< Engine 1 name (e.g., "FrankyCPP")
    std::string engine1Version; ///< Engine 1 version (e.g., "v1.1")
    std::string engine1Path;    ///< Path to engine 1 executable

    // Engine 2 identification
    std::string engine2Name;    ///< Engine 2 name (e.g., "FrankyGo")
    std::string engine2Version; ///< Engine 2 version (e.g., "v1.0.3")
    std::string engine2Path;    ///< Path to engine 2 executable

    // Match settings
    std::string timeControl; ///< Time control (e.g., "60+0.6")
    int rounds = 0;          ///< Number of games

    // Results
    int engine1Wins      = 0;   ///< Wins by engine 1
    int engine2Wins      = 0;   ///< Wins by engine 2
    int draws            = 0;   ///< Number of draws
    double engine1Score  = 0.0; ///< Score for engine 1 (win=1, draw=0.5)
    double engine2Score  = 0.0; ///< Score for engine 2
    double eloDifference = 0.0; ///< Calculated ELO difference (engine1 - engine2)

    // Additional data
    std::string pgnPath;    ///< Path to PGN file
    int64_t durationMs = 0; ///< Match duration in milliseconds

    /// Returns EngineId for engine 1
    [[nodiscard]] EngineId getEngine1Id() const {
      return {engine1Name, engine1Version};
    }

    /// Returns EngineId for engine 2
    [[nodiscard]] EngineId getEngine2Id() const {
      return {engine2Name, engine2Version};
    }

    /// Returns match key for indexing: "Engine1-v1 vs Engine2-v2"
    [[nodiscard]] std::string getMatchKey() const {
      return getEngine1Id().toString() + " vs " + getEngine2Id().toString();
    }

    /// Returns total number of games played
    [[nodiscard]] int getTotalGames() const {
      return engine1Wins + engine2Wins + draws;
    }

    /// Returns win rate for engine 1 as percentage (0.0 - 100.0)
    [[nodiscard]] double getEngine1WinRate() const {
      const int total = getTotalGames();
      return total > 0 ? (engine1Score * 100.0 / total) : 0.0;
    }
  };

  //=============================================================================
  // BenchmarkResult - Performance benchmark results
  //=============================================================================

  /// Result from running a benchmark (NPS measurement)
  struct BenchmarkResult {
    // Arena metadata
    std::string arenaVersion; ///< Arena version that ran this benchmark (e.g., "v1.2")
    std::string timestamp;    ///< ISO 8601 timestamp

    // Engine identification
    std::string engineName;    ///< Engine name (e.g., "FrankyCPP")
    std::string engineVersion; ///< Engine version (e.g., "v1.2")
    std::string enginePath;    ///< Path to engine executable (empty if internal)

    // Benchmark configuration
    int depth      = 0; ///< Search depth used
    int hashSizeMB = 0; ///< Hash table size in MB
    int threads    = 1; ///< Number of threads used
    int positions  = 0; ///< Number of positions benchmarked

    // Results
    uint64_t totalNodes = 0; ///< Total nodes searched
    int64_t totalTimeMs = 0; ///< Total time in milliseconds
    uint64_t nps        = 0; ///< Nodes per second

    // Feature tracking
    std::string tag; ///< Feature tag (e.g., "QuietSee")

    /// Returns EngineId from this result
    [[nodiscard]] EngineId getEngineId() const {
      return {engineName, engineVersion};
    }

    /// Returns benchmark key for indexing: "EngineName-vX.Y_depth_hash"
    [[nodiscard]] std::string getBenchmarkKey() const {
      return getEngineId().toString() + "_d" + std::to_string(depth) + "_h" + std::to_string(hashSizeMB);
    }
  };

  //=============================================================================
  // ReportData - Results organized for reporting (pure data bag)
  //=============================================================================

  /// Holds all results organized by test suite and engine for report generation.
  /// This is a pure data struct — query methods are in ResultStore.
  struct ReportData {
    /// All test suites found (e.g., "WAC", "Crafty", "STS")
    std::set<std::string> testSuites;

    /// All engines found (e.g., FrankyCPP v1.1, FrankyGo v1.0.3)
    std::set<EngineId> engines;

    /// Results indexed by: testSuite -> engineId -> result
    /// Uses latest result if multiple exist for same suite/engine
    std::map<std::string, std::map<EngineId, TestSuiteResult>> suiteResults;

    /// All historical test suite results (for --history mode)
    /// Indexed by: engineId -> vector of all results for that engine
    std::map<EngineId, std::vector<TestSuiteResult>> allSuiteResults;

    /// Match results indexed by match key: "Engine1-v1 vs Engine2-v2"
    std::map<std::string, MatchResult> matchResults;

    /// Benchmark results indexed by timestamp (keeps all runs for history)
    std::vector<BenchmarkResult> benchmarkResults;

    /// Returns true if any results are loaded
    [[nodiscard]] bool hasResults() const {
      return !suiteResults.empty() || !matchResults.empty() || !benchmarkResults.empty();
    }

    /// Returns true if test suite results exist
    [[nodiscard]] bool hasTestSuiteResults() const {
      return !suiteResults.empty();
    }

    /// Returns true if match results exist
    [[nodiscard]] bool hasMatchResults() const {
      return !matchResults.empty();
    }

    /// Returns true if benchmark results exist
    [[nodiscard]] bool hasBenchmarkResults() const {
      return !benchmarkResults.empty();
    }

    /// Returns true if results exist for given engine
    [[nodiscard]] bool hasEngine(const EngineId& engine) const {
      return engines.contains(engine);
    }
  };

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_RESULTS_H
