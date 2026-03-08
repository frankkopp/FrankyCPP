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
//   These structures are serialized to JSON by ResultWriter for:
//   - Long-term storage and historical tracking
//   - Automated comparison between versions
//   - Analysis and visualization tools
//
// Usage:
//   TestSuiteResult result;
//   result.version = "v1.1";
//   result.suiteName = "WAC";
//   result.totalTests = 300;
//   result.passed = 285;
//   // ... populate details ...
//   resultWriter.writeTestSuiteResult(result);
//
//=============================================================================

#include <algorithm>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <vector>

namespace arena {

  //=============================================================================
  // EngineId - Engine identifier (name + version)
  //=============================================================================

  /// Uniquely identifies an engine by name and version
  struct EngineId {
    std::string name;   ///< Engine name (e.g., "FrankyCPP", "FrankyGo")
    std::string version;///< Engine version (e.g., "v1.1", "v1.0.3")

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
      if (name.length() >= version.length() && name.substr(name.length() - version.length()) == version) {
        return name;
      }
      // Check if name already contains the version somewhere
      if (name.find(version) != std::string::npos) {
        return name;
      }
      return name + " " + version;
    }

    /// Comparison operator for use in std::map/set (sorts by name, then version)
    bool operator<(const EngineId& other) const {
      if (name != other.name) return name < other.name;
      return version < other.version;
    }

    /// Equality operator
    bool operator==(const EngineId& other) const {
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
    std::string testId;  ///< Test identifier
    std::string fen;     ///< Position FEN
    std::string expected;///< Expected move(s)
    std::string actual;  ///< Actual move played
    bool passed;         ///< Test passed
    uint64_t nodes;      ///< Nodes searched
    int64_t timeMs;      ///< Time taken in milliseconds
  };

  /// Result from running a test suite
  struct TestSuiteResult {
    // Arena metadata
    std::string arenaVersion;///< Arena version that ran this test (e.g., "v1.1")
    std::string timestamp;   ///< ISO 8601 timestamp

    // Test suite identification
    std::string testSuiteName;///< Clean test suite name (e.g., "WAC", "Crafty")
    std::string epdPath;      ///< Path to EPD file

    // Engine identification
    std::string engineName;   ///< Engine name (e.g., "FrankyCPP", "FrankyGo")
    std::string engineVersion;///< Engine version (e.g., "v0.5", "v1.1")
    std::string enginePath;   ///< Path to engine executable (empty if internal)

    // Feature tracking
    std::string tag;          ///< Feature tag (e.g., "QuietSee", "TTbuckets")

    // Results summary
    int totalTests;     ///< Total number of tests
    int passed;         ///< Number of passed tests
    int failed;         ///< Number of failed tests
    int skipped;        ///< Number of skipped tests
    uint64_t totalNodes;///< Total nodes searched
    int64_t totalTimeMs;///< Total time in milliseconds

    // Per-test details (optional, can be pruned)
    std::vector<TestCaseDetail> details;///< Per-test details

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
    std::string arenaVersion;///< Arena version that ran this match (e.g., "v1.1")
    std::string timestamp;   ///< ISO 8601 timestamp

    // Match identification
    std::string matchName;///< Match identifier (e.g., "Rapid 60+0.6")

    // Feature tracking
    std::string tag;      ///< Feature tag (e.g., "QuietSee", "TTbuckets")

    // Engine 1 identification
    std::string engine1Name;   ///< Engine 1 name (e.g., "FrankyCPP")
    std::string engine1Version;///< Engine 1 version (e.g., "v1.1")
    std::string engine1Path;   ///< Path to engine 1 executable

    // Engine 2 identification
    std::string engine2Name;   ///< Engine 2 name (e.g., "FrankyGo")
    std::string engine2Version;///< Engine 2 version (e.g., "v1.0.3")
    std::string engine2Path;   ///< Path to engine 2 executable

    // Match settings
    std::string timeControl;///< Time control (e.g., "60+0.6")
    int rounds = 0;         ///< Number of games

    // Results
    int engine1Wins      = 0;  ///< Wins by engine 1
    int engine2Wins      = 0;  ///< Wins by engine 2
    int draws            = 0;  ///< Number of draws
    double engine1Score  = 0.0;///< Score for engine 1 (win=1, draw=0.5)
    double engine2Score  = 0.0;///< Score for engine 2
    double eloDifference = 0.0;///< Calculated ELO difference (engine1 - engine2)

    // Additional data
    std::string pgnPath;   ///< Path to PGN file
    int64_t durationMs = 0;///< Match duration in milliseconds

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
    std::string arenaVersion;///< Arena version that ran this benchmark (e.g., "v1.2")
    std::string timestamp;   ///< ISO 8601 timestamp

    // Engine identification
    std::string engineName;   ///< Engine name (e.g., "FrankyCPP")
    std::string engineVersion;///< Engine version (e.g., "v1.2")
    std::string enginePath;   ///< Path to engine executable (empty if internal)

    // Benchmark configuration
    int depth      = 0;///< Search depth used
    int hashSizeMB = 0;///< Hash table size in MB
    int threads    = 1;///< Number of threads used
    int positions  = 0;///< Number of positions benchmarked

    // Results
    uint64_t totalNodes = 0;///< Total nodes searched
    int64_t totalTimeMs = 0;///< Total time in milliseconds
    uint64_t nps        = 0;///< Nodes per second

    // Feature tracking
    std::string tag;        ///< Feature tag (e.g., "QuietSee") - renamed from notes

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
  // ReportData - Results organized for reporting
  //=============================================================================

  /// Holds all results organized by test suite and engine for report generation
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

    /// Find an engine by flexible matching
    /// First tries exact match, then tries to match by version and base name
    /// This handles cases where stored engine names include version (e.g., "FrankyCPP v1.1")
    /// but user provides just base name (e.g., "FrankyCPP" with version "v1.1")
    /// Prioritizes engines with test suite results over benchmark-only engines
    [[nodiscard]] std::optional<EngineId> findEngine(const EngineId& search) const {
      // First try exact match
      if (engines.contains(search)) {
        return search;
      }

      // Collect all matching engines
      std::vector<EngineId> matches;

      // Try flexible matching: find engines where version matches and name contains the search name
      for (const auto& engine : engines) {
        if (engine.version == search.version) {
          // Check if stored name starts with search name (e.g., "FrankyCPP v1.1" starts with "FrankyCPP")
          if (engine.name.find(search.name) == 0) {
            matches.push_back(engine);
            continue;
          }
          // Also check if search name contains stored base name (for underscore variants)
          // e.g., "FrankyCPP_v1.1" should match "FrankyCPP v1.1"
          std::string searchNoUnderscore = search.name;
          std::ranges::replace(searchNoUnderscore, '_', ' ');
          if (engine.name.find(searchNoUnderscore) == 0 || searchNoUnderscore.find(engine.name) == 0) {
            matches.push_back(engine);
          }
        }
      }

      if (matches.empty()) {
        return std::nullopt;
      }

      // Prioritize engines that have test suite results
      for (const auto& engine : matches) {
        for (const auto& [suiteName, engineResults] : suiteResults) {
          if (engineResults.contains(engine)) {
            return engine;
          }
        }
      }

      // Fall back to first match (e.g., benchmark-only engine)
      return matches.front();
    }

    /// Returns result for given suite and engine, or nullptr if not found
    /// Uses flexible matching to handle name variations (e.g., "FrankyCPP" vs "FrankyCPP v1.5")
    [[nodiscard]] const TestSuiteResult* getResult(const std::string& suite, const EngineId& engine) const {
      auto suiteIt = suiteResults.find(suite);
      if (suiteIt == suiteResults.end()) return nullptr;

      // First try exact match
      auto engineIt = suiteIt->second.find(engine);
      if (engineIt != suiteIt->second.end()) return &engineIt->second;

      // Fall back to flexible matching
      for (const auto& [engineId, result] : suiteIt->second) {
        if (enginesMatchFlexibly(engine, engineId)) {
          return &result;
        }
      }

      return nullptr;
    }

    /// Returns match result for given engine pair, or nullptr if not found
    /// Order doesn't matter: checks both "e1 vs e2" and "e2 vs e1"
    /// Uses flexible matching to handle underscore vs space differences in engine names
    [[nodiscard]] const MatchResult* getMatch(const EngineId& engine1, const EngineId& engine2) const {
      // First try exact key lookup for efficiency
      const std::string key1 = engine1.toString() + " vs " + engine2.toString();
      const std::string key2 = engine2.toString() + " vs " + engine1.toString();

      auto it = matchResults.find(key1);
      if (it != matchResults.end()) return &it->second;

      it = matchResults.find(key2);
      if (it != matchResults.end()) return &it->second;

      // Fall back to flexible matching: iterate through all matches
      for (const auto& match : matchResults | std::views::values) {
        const EngineId e1 = match.getEngine1Id();
        const EngineId e2 = match.getEngine2Id();

        // Check if (engine1, engine2) matches (e1, e2) in either order
        if ((enginesMatchFlexibly(engine1, e1) && enginesMatchFlexibly(engine2, e2)) || (enginesMatchFlexibly(engine1, e2) && enginesMatchFlexibly(engine2, e1))) {
          return &match;
        }
      }

      return nullptr;
    }

    /// Returns all matches involving the given engine (using flexible matching)
    [[nodiscard]] std::vector<const MatchResult*> getMatchesForEngine(const EngineId& engine) const {
      std::vector<const MatchResult*> matches;
      for (const auto& [key, match] : matchResults) {
        if (enginesMatchFlexibly(engine, match.getEngine1Id()) || enginesMatchFlexibly(engine, match.getEngine2Id())) {
          matches.push_back(&match);
        }
      }
      return matches;
    }

    /// Returns all benchmarks for the given engine (using flexible matching)
    /// Returns only the latest benchmark for each unique (tag, date) combination
    [[nodiscard]] std::vector<const BenchmarkResult*> getBenchmarksForEngine(const EngineId& engine) const {
      std::vector<const BenchmarkResult*> benchmarks;
      for (const auto& benchmark : benchmarkResults) {
        if (enginesMatchFlexibly(engine, benchmark.getEngineId())) {
          benchmarks.push_back(&benchmark);
        }
      }
      return benchmarks;
    }

  private:
    /// Helper: Check if two engine IDs refer to the same engine
    /// Handles differences like underscore vs space (FrankyCPP_v1.1 vs FrankyCPP v1.1)
    [[nodiscard]] static bool enginesMatchFlexibly(const EngineId& a, const EngineId& b) {
      // Exact match
      if (a == b) return true;

      // Version must match
      if (a.version != b.version) return false;

      // Normalize names by replacing underscores with spaces
      std::string nameA = a.name;
      std::string nameB = b.name;
      std::replace(nameA.begin(), nameA.end(), '_', ' ');
      std::replace(nameB.begin(), nameB.end(), '_', ' ');

      if (nameA == nameB) return true;

      // Check if one name starts with the other (for variations like "FrankyCPP" vs "FrankyCPP v1.1")
      if (nameA.find(nameB) == 0 || nameB.find(nameA) == 0) return true;

      return false;
    }
  };

}// namespace arena

#endif// FRANKYCPP_ENGINE_ARENA_RESULTS_H
