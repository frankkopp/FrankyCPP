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

#ifndef FRANKYCPP_ENGINE_ARENA_RESULTSTORE_H
#define FRANKYCPP_ENGINE_ARENA_RESULTSTORE_H

//=============================================================================
// ResultStore.h - Centralized Result I/O for Engine Arena
//=============================================================================
//
// ResultStore handles all reading and writing of arena result data. It
// consolidates functionality previously split between ResultWriter (writing)
// and ArenaRunner (reading) into a single cohesive class.
//
// Also provides static query methods for searching ReportData, centralizing
// the flexible engine-matching logic in one place.
//
// Responsibilities:
//   - Write test suite, match, and benchmark results to JSON files
//   - Read/load results from JSON files into ReportData structures
//   - Query ReportData (findEngine, getResult, getMatch, etc.)
//   - Manage output directory structure
//   - Generate timestamped filenames
//
// Writing:
//   ResultStore store("./results");
//   std::string path = store.writeTestSuiteResult(result);
//   std::string path = store.writeMatchResult(result);
//   std::string path = store.writeBenchmarkResult(result);
//
// Loading:
//   ReportData data = store.loadAllResults();           // everything
//   ReportData data = store.loadTestSuiteResults();     // test suites only
//   store.loadMatchResults(data);                       // add matches
//   store.loadBenchmarkResults(data);                   // add benchmarks
//   auto benchmarks = store.readBenchmarkResults();     // raw benchmark list
//
// Querying (static methods — operate on ReportData):
//   auto engine = ResultStore::findEngine(data, searchId);
//   auto* result = ResultStore::getResult(data, "WAC", engineId);
//   auto* match = ResultStore::getMatch(data, engine1, engine2);
//   auto matches = ResultStore::getMatchesForEngine(data, engineId);
//   auto benchmarks = ResultStore::getBenchmarksForEngine(data, engineId);
//
// Engine Name Consistency (A6):
//   - loadTestSuiteResults() populates data.engines from test suite results
//   - loadMatchResults() does NOT add match engine IDs to data.engines
//     (match names differ from test suite names, causing phantom duplicates)
//   - loadBenchmarkResults() does NOT add benchmark engine IDs to data.engines
//     (benchmark names like "FrankyCPP" differ from "FrankyCPP v1.3")
//   - Cross-domain matching uses flexible queries (getMatchesForEngine, etc.)
//
// Output Directory Structure:
//   results/
//     testsuites/     - EPD test suite results (JSON)
//     matches/        - Engine match results (JSON + PGN)
//     benchmarks/     - Benchmark results (consolidated JSON)
//
// Thread Safety:
//   - Not thread-safe (intended for single-threaded arena execution)
//   - Each write operation is atomic at filesystem level
//
//=============================================================================

#include "ArenaResults.h"

#include <optional>
#include <string>
#include <vector>

namespace arena {

  /// Centralized result I/O — reads and writes all arena result data.
  /// Also provides static query methods for searching ReportData.
  class ResultStore {
    std::string resultsDir; ///< Root directory for results

  public:
    /// Creates a ResultStore for the specified output directory
    /// Creates subdirectories (testsuites/, matches/, etc.) if needed
    /// @param resultsDir Root directory for results (e.g., "./results")
    explicit ResultStore(const std::string& resultsDir);

    //=========================================================================
    // Writing
    //=========================================================================

    /// Write test suite results to a JSON file
    /// @param result Test suite result data
    /// @return Path to the created file
    [[nodiscard]] std::string writeTestSuiteResult(const TestSuiteResult& result) const;

    /// Write match results to a JSON file
    /// @param result Match result data
    /// @return Path to the created file
    [[nodiscard]] std::string writeMatchResult(const MatchResult& result) const;

    /// Write benchmark result to the consolidated JSON file
    /// Appends to existing results file (benchmarks.json)
    /// @param result Benchmark result data
    /// @return Path to the created/updated file
    [[nodiscard]] std::string writeBenchmarkResult(const BenchmarkResult& result) const;


    //=========================================================================
    // Loading
    //=========================================================================

    /// Load all results (test suites + matches + benchmarks) into ReportData
    /// Convenience method that calls loadTestSuiteResults(), then adds matches
    /// and benchmarks to the same ReportData.
    /// @return ReportData with all results organized for reporting
    [[nodiscard]] ReportData loadAllResults() const;

    /// Load only test suite results into a new ReportData
    /// Populates: data.engines, data.testSuites, data.suiteResults, data.allSuiteResults
    /// @return ReportData with test suite results only
    [[nodiscard]] ReportData loadTestSuiteResults() const;

    /// Load match results into an existing ReportData
    /// Populates: data.matchResults (does NOT add to data.engines — see A6)
    /// @param data ReportData to populate (modifies in place)
    void loadMatchResults(ReportData& data) const;

    /// Load benchmark results into an existing ReportData
    /// Populates: data.benchmarkResults (does NOT add to data.engines — see A6)
    /// @param data ReportData to populate (modifies in place)
    void loadBenchmarkResults(ReportData& data) const;

    /// Read all benchmark results from the consolidated JSON file
    /// @return Vector of all stored benchmark results
    [[nodiscard]] std::vector<BenchmarkResult> readBenchmarkResults() const;

    //=========================================================================
    // Query Methods (static — operate on ReportData)
    //=========================================================================

    /// Find an engine by flexible matching.
    /// First tries exact match, then tries to match by version and base name.
    /// Handles cases where stored engine names include version (e.g., "FrankyCPP v1.1")
    /// but user provides just base name (e.g., "FrankyCPP" with version "v1.1").
    /// Prioritizes engines with test suite results over benchmark-only engines.
    /// @param data   ReportData to search
    /// @param search Engine identifier to find
    /// @return       Matching EngineId, or std::nullopt if not found
    [[nodiscard]] static std::optional<EngineId> findEngine(const ReportData& data, const EngineId& search);

    /// Returns result for given suite and engine, or nullptr if not found.
    /// Uses flexible matching to handle name variations (e.g., "FrankyCPP" vs "FrankyCPP v1.6").
    /// @param data   ReportData to search
    /// @param suite  Test suite name (e.g., "WAC")
    /// @param engine Engine identifier
    /// @return       Pointer to result, or nullptr
    [[nodiscard]] static const TestSuiteResult* getResult(const ReportData& data, const std::string& suite, const EngineId& engine);

    /// Returns match result for given engine pair, or nullptr if not found.
    /// Order doesn't matter: checks both "e1 vs e2" and "e2 vs e1".
    /// Uses flexible matching to handle underscore vs space differences.
    /// @param data    ReportData to search
    /// @param engine1 First engine
    /// @param engine2 Second engine
    /// @return        Pointer to match result, or nullptr
    [[nodiscard]] static const MatchResult* getMatch(const ReportData& data, const EngineId& engine1, const EngineId& engine2);

    /// Returns all matches involving the given engine (using flexible matching).
    /// @param data   ReportData to search
    /// @param engine Engine identifier
    /// @return       Vector of pointers to matching MatchResults
    [[nodiscard]] static std::vector<const MatchResult*> getMatchesForEngine(const ReportData& data, const EngineId& engine);

    /// Returns all benchmarks for the given engine (using flexible matching).
    /// @param data   ReportData to search
    /// @param engine Engine identifier
    /// @return       Vector of pointers to matching BenchmarkResults
    [[nodiscard]] static std::vector<const BenchmarkResult*> getBenchmarksForEngine(const ReportData& data, const EngineId& engine);

    /// Check if two engine IDs refer to the same engine.
    /// Handles differences like underscore vs space (FrankyCPP_v1.1 vs FrankyCPP v1.1).
    /// @param a First engine identifier
    /// @param b Second engine identifier
    /// @return  true if the engines match flexibly
    [[nodiscard]] static bool enginesMatchFlexibly(const EngineId& a, const EngineId& b);

  private:
    /// Generate filename with timestamp
    /// @param prefix Prefix for filename (e.g., "testsuite", "match")
    /// @param name Test suite or match name
    /// @param engineId Engine identifier (e.g., "FrankyCPP-v0.5"), may be empty
    /// @return Filename in format: {name}_{engineId}_{timestamp}.json
    std::string generateFilename(const std::string& prefix,
                                 const std::string& name,
                                 const std::string& engineId) const;

    /// Ensure directory exists, create if needed
    static void ensureDirectoryExists(const std::string& path);
  };

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_RESULTSTORE_H
