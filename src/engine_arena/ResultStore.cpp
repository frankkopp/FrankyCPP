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

//=============================================================================
// ResultStore.cpp - Centralized Result I/O Implementation
//=============================================================================

#include "ResultStore.h"
#include "common/TimeUtils.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>

// Suppress false positive Clangd warning about nlohmann/json template instantiation
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wc++20-extensions"
#endif

#include <nlohmann/json.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace arena {

  using common::fileTimestamp;
  using json = nlohmann::json;

  //=========================================================================
  // Construction
  //=========================================================================

  ResultStore::ResultStore(const std::string& resultsDir)
      : resultsDir(resultsDir) {
    ensureDirectoryExists(resultsDir);
    ensureDirectoryExists(resultsDir + "/testsuites");
    ensureDirectoryExists(resultsDir + "/matches");
    ensureDirectoryExists(resultsDir + "/benchmarks");
  }

  //=========================================================================
  // Writing — Test Suite Results
  //=========================================================================

  std::string ResultStore::writeTestSuiteResult(const TestSuiteResult& result) const {
    const std::string engineId = result.engineName + "-" + result.engineVersion;
    std::string filename       = generateFilename("testsuite", result.testSuiteName, engineId);

    // Calculate derived metrics
    const double successRate = result.totalTests > 0
                                 ? result.passed * 100.0 / result.totalTests
                                 : 0.0;
    const double avgTimeMs   = result.totalTests > 0
                                 ? static_cast<double>(result.totalTimeMs) / result.totalTests
                                 : 0.0;
    const double avgNodes    = result.totalTests > 0
                                 ? static_cast<double>(result.totalNodes) / result.totalTests
                                 : 0.0;

    // Build JSON structure
    json j;
    j["arenaVersion"] = result.arenaVersion;
    j["timestamp"]    = result.timestamp;
    j["tag"]          = result.tag;

    j["testSuite"] = {
      {"name", result.testSuiteName},
      {"epdPath", result.epdPath}};

    j["engine"] = {
      {"name", result.engineName},
      {"version", result.engineVersion},
      {"path", result.enginePath}};

    j["summary"] = {
      {"totalTests", result.totalTests},
      {"passed", result.passed},
      {"failed", result.failed},
      {"skipped", result.skipped},
      {"successRate", std::round(successRate * 100.0) / 100.0},
      {"totalNodes", result.totalNodes},
      {"totalTimeMs", result.totalTimeMs},
      {"avgTimeMs", std::round(avgTimeMs * 100.0) / 100.0},
      {"avgNodes", std::round(avgNodes * 100.0) / 100.0}};

    // Build details array
    json details = json::array();
    for (const auto& detail : result.details) {
      details.push_back({{"testId", detail.testId},
                         {"fen", detail.fen},
                         {"expected", detail.expected},
                         {"actual", detail.actual},
                         {"passed", detail.passed},
                         {"nodes", detail.nodes},
                         {"timeMs", detail.timeMs}});
    }
    j["details"] = std::move(details);

    // Write to file
    std::ofstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file for writing: " + filename);
    }
    file << j.dump(2) << "\n";
    file.close();

    return filename;
  }

  //=========================================================================
  // Writing — Match Results
  //=========================================================================

  std::string ResultStore::writeMatchResult(const MatchResult& result) const {
    const std::string engine1Id = result.engine1Name + "-" + result.engine1Version;
    const std::string engine2Id = result.engine2Name + "-" + result.engine2Version;
    std::string sanitizedTC     = result.timeControl;
    std::ranges::replace(sanitizedTC, '+', '_');
    std::ranges::replace(sanitizedTC, '/', '_');

    const std::string matchId = engine1Id + "_vs_" + engine2Id + "_" + sanitizedTC;
    std::string filename      = generateFilename("match", matchId, "");

    // Build JSON structure
    json j;
    j["arenaVersion"] = result.arenaVersion;
    j["timestamp"]    = result.timestamp;
    j["tag"]          = result.tag;

    j["match"] = {
      {"name", result.matchName},
      {"timeControl", result.timeControl},
      {"rounds", result.rounds}};

    j["engine1"] = {
      {"name", result.engine1Name},
      {"version", result.engine1Version},
      {"path", result.engine1Path}};

    j["engine2"] = {
      {"name", result.engine2Name},
      {"version", result.engine2Version},
      {"path", result.engine2Path}};

    j["results"] = {
      {"engine1Wins", result.engine1Wins},
      {"engine2Wins", result.engine2Wins},
      {"draws", result.draws},
      {"engine1Score", result.engine1Score},
      {"engine2Score", result.engine2Score},
      {"eloDifference", result.eloDifference}};

    j["pgnPath"]    = result.pgnPath;
    j["durationMs"] = result.durationMs;

    // Write to file
    std::ofstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file for writing: " + filename);
    }
    file << j.dump(2) << "\n";
    file.close();

    return filename;
  }

  //=========================================================================
  // Writing — Benchmark Results
  //=========================================================================

  std::string ResultStore::writeBenchmarkResult(const BenchmarkResult& result) const {
    const std::string filename = resultsDir + "/benchmarks/benchmarks.json";

    // Read existing results
    std::vector<BenchmarkResult> allResults = readBenchmarkResults();

    // Check for duplicate tag (same tag, version, engineName on same date)
    if (!result.tag.empty()) {
      const std::string resultDate = result.timestamp.substr(0, 10); // YYYY-MM-DD
      for (const auto& existing : allResults) {
        if (existing.tag == result.tag && existing.engineVersion == result.engineVersion && existing.engineName == result.engineName && existing.timestamp.substr(0, 10) == resultDate) {
          std::cerr << "WARNING: Duplicate benchmark run detected!\n"
                    << "  Tag: \"" << result.tag << "\"\n"
                    << "  Engine: " << result.engineName << " " << result.engineVersion << "\n"
                    << "  Date: " << resultDate << "\n"
                    << "  Consider using a different tag or cleaning up old results.\n"
                    << std::endl;
          break;
        }
      }
    }

    // Add new result
    allResults.push_back(result);

    // Build JSON structure
    json benchmarksArray = json::array();
    for (const auto& r : allResults) {
      benchmarksArray.push_back({{"arenaVersion", r.arenaVersion},
                                 {"timestamp", r.timestamp},
                                 {"engineName", r.engineName},
                                 {"engineVersion", r.engineVersion},
                                 {"enginePath", r.enginePath},
                                 {"depth", r.depth},
                                 {"hashSizeMB", r.hashSizeMB},
                                 {"threads", r.threads},
                                 {"positions", r.positions},
                                 {"totalNodes", r.totalNodes},
                                 {"totalTimeMs", r.totalTimeMs},
                                 {"nps", r.nps},
                                 {"tag", r.tag}});
    }

    json j;
    j["benchmarks"] = std::move(benchmarksArray);

    // Write all results back (overwrite file)
    std::ofstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file for writing: " + filename);
    }
    file << j.dump(2) << "\n";
    file.close();

    return filename;
  }


  //=========================================================================
  // Loading — All Results (convenience)
  //=========================================================================

  ReportData ResultStore::loadAllResults() const {
    ReportData data = loadTestSuiteResults();
    loadMatchResults(data);
    loadBenchmarkResults(data);
    return data;
  }

  //=========================================================================
  // Loading — Test Suite Results
  //=========================================================================

  ReportData ResultStore::loadTestSuiteResults() const {
    ReportData data;

    const std::filesystem::path testsuitesDir = std::filesystem::path(resultsDir) / "testsuites";

    if (!std::filesystem::exists(testsuitesDir)) {
      std::cout << "  Warning: No testsuites directory found at " << testsuitesDir << std::endl;
      return data;
    }

    // Load all JSON files from testsuites directory
    for (const auto& entry : std::filesystem::directory_iterator(testsuitesDir)) {
      if (entry.path().extension() != ".json") continue;

      try {
        std::ifstream file(entry.path());
        const json jsonData = json::parse(file);

        TestSuiteResult result;

        // Parse JSON format
        result.arenaVersion  = jsonData["arenaVersion"];
        result.timestamp     = jsonData["timestamp"];
        result.testSuiteName = jsonData["testSuite"]["name"];
        result.epdPath       = jsonData["testSuite"]["epdPath"];
        result.engineName    = jsonData["engine"]["name"];
        result.engineVersion = jsonData["engine"]["version"];
        result.enginePath    = jsonData["engine"]["path"];

        // Parse summary section
        const auto& summary = jsonData["summary"];
        result.totalTests   = summary["totalTests"];
        result.passed       = summary["passed"];
        result.failed       = summary["failed"];
        result.skipped      = summary["skipped"];
        result.totalNodes   = summary["totalNodes"];
        result.totalTimeMs  = summary["totalTimeMs"];

        // Parse tag (may not exist in old files)
        result.tag = jsonData.value("tag", "");

        // Skip details for reporting (not needed, saves memory)

        // Get engine identifier
        const EngineId engineId      = result.getEngineId();
        const std::string& suiteName = result.testSuiteName;

        // Add to data structures
        data.testSuites.insert(suiteName);
        data.engines.insert(engineId);

        // Store in allSuiteResults for history mode
        data.allSuiteResults[engineId].push_back(result);

        // Check if we already have a result for this suite/engine — keep latest
        auto& suiteMap      = data.suiteResults[suiteName];
        const auto existing = suiteMap.find(engineId);
        if (existing == suiteMap.end() || result.timestamp > existing->second.timestamp) {
          suiteMap[engineId] = result;
        }

      } catch (const std::exception& e) {
        std::cerr << "  Warning: Failed to load " << entry.path().filename().string()
                  << ": " << e.what() << std::endl;
      }
    }

    return data;
  }

  //=========================================================================
  // Loading — Match Results
  //=========================================================================

  void ResultStore::loadMatchResults(ReportData& data) const {
    const std::filesystem::path matchesDir = std::filesystem::path(resultsDir) / "matches";

    if (!std::filesystem::exists(matchesDir)) {
      return; // No matches directory — OK, just means no matches have been run
    }

    // Load all JSON files from matches directory
    for (const auto& entry : std::filesystem::directory_iterator(matchesDir)) {
      if (entry.path().extension() != ".json") continue;

      try {
        std::ifstream file(entry.path());
        const json jsonData = json::parse(file);

        MatchResult result;

        // Parse JSON format
        result.arenaVersion = jsonData["arenaVersion"];
        result.timestamp    = jsonData["timestamp"];

        // Match info
        result.matchName   = jsonData["match"]["name"];
        result.timeControl = jsonData["match"]["timeControl"];
        result.rounds      = jsonData["match"]["rounds"];

        // Engine 1
        result.engine1Name    = jsonData["engine1"]["name"];
        result.engine1Version = jsonData["engine1"]["version"];
        result.engine1Path    = jsonData["engine1"]["path"];

        // Engine 2
        result.engine2Name    = jsonData["engine2"]["name"];
        result.engine2Version = jsonData["engine2"]["version"];
        result.engine2Path    = jsonData["engine2"]["path"];

        // Results
        const auto& results  = jsonData["results"];
        result.engine1Wins   = results["engine1Wins"];
        result.engine2Wins   = results["engine2Wins"];
        result.draws         = results["draws"];
        result.engine1Score  = results["engine1Score"];
        result.engine2Score  = results["engine2Score"];
        result.eloDifference = results["eloDifference"];

        // Additional data
        result.pgnPath    = jsonData["pgnPath"];
        result.durationMs = jsonData["durationMs"];

        // Parse tag (may not exist in old files)
        result.tag = jsonData.value("tag", "");

        // NOTE (A6 fix): Do NOT add match engine IDs to data.engines.
        // Match engine names (e.g., "FrankyGo") differ from test suite names
        // (e.g., "FrankyGo v1.0.3 (4.6.2021)") causing phantom duplicate rows.
        // Match engines are accessed via getMatchesForEngine() flexible matching.

        // Get match key — keep latest result per match pair
        const std::string matchKey = result.getMatchKey();
        const auto existing        = data.matchResults.find(matchKey);
        if (existing == data.matchResults.end() || result.timestamp > existing->second.timestamp) {
          data.matchResults[matchKey] = result;
        }

      } catch (const std::exception& e) {
        std::cerr << "  Warning: Failed to load " << entry.path().filename().string()
                  << ": " << e.what() << std::endl;
      }
    }
  }

  //=========================================================================
  // Loading — Benchmark Results
  //=========================================================================

  void ResultStore::loadBenchmarkResults(ReportData& data) const {
    const auto benchmarks = readBenchmarkResults();

    for (const auto& benchmark : benchmarks) {
      // NOTE (A6): Do NOT add benchmark engines to data.engines.
      // Benchmark engine names (e.g., "FrankyCPP") differ from test suite names
      // (e.g., "FrankyCPP v1.3") causing duplicate display entries.
      // Benchmark engines are accessed via getBenchmarksForEngine() flexible matching.
      data.benchmarkResults.push_back(benchmark);
    }
  }

  //=========================================================================
  // Reading — Benchmark Results (raw)
  //=========================================================================

  std::vector<BenchmarkResult> ResultStore::readBenchmarkResults() const {
    std::vector<BenchmarkResult> results;
    const std::string filename = resultsDir + "/benchmarks/benchmarks.json";

    if (!std::filesystem::exists(filename)) {
      return results;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
      return results;
    }

    try {
      const json j = json::parse(file);
      file.close();

      if (!j.contains("benchmarks") || !j["benchmarks"].is_array()) {
        return results;
      }

      for (const auto& entry : j["benchmarks"]) {
        BenchmarkResult r;
        r.arenaVersion  = entry.value("arenaVersion", "");
        r.timestamp     = entry.value("timestamp", "");
        r.engineName    = entry.value("engineName", "");
        r.engineVersion = entry.value("engineVersion", "");
        r.enginePath    = entry.value("enginePath", "");
        r.depth         = entry.value("depth", 0);
        r.hashSizeMB    = entry.value("hashSizeMB", 0);
        r.threads       = entry.value("threads", 0);
        r.positions     = entry.value("positions", 0);
        r.totalNodes    = entry.value("totalNodes", static_cast<uint64_t>(0));
        r.totalTimeMs   = entry.value("totalTimeMs", static_cast<int64_t>(0));
        r.nps           = entry.value("nps", static_cast<uint64_t>(0));
        // Support both "tag" (new) and "notes" (old) for backwards compatibility
        r.tag = entry.value("tag", "");
        if (r.tag.empty()) {
          r.tag = entry.value("notes", "");
        }
        results.push_back(std::move(r));
      }
    } catch (const std::exception& e) {
      std::cerr << "Warning: Failed to parse benchmark results: " << e.what() << std::endl;
      file.close();
    }

    return results;
  }

  //=========================================================================
  // Query Methods (static — operate on ReportData)
  //=========================================================================

  std::optional<EngineId> ResultStore::findEngine(const ReportData& data, const EngineId& search) {
    // First try exact match
    if (data.engines.contains(search)) {
      return search;
    }

    // Collect all matching engines
    std::vector<EngineId> matches;

    // Try flexible matching: find engines where version matches and name contains the search name
    for (const auto& engine : data.engines) {
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
      for (const auto& [suiteName, engineResults] : data.suiteResults) {
        if (engineResults.contains(engine)) {
          return engine;
        }
      }
    }

    // Fall back to first match (e.g., benchmark-only engine)
    return matches.front();
  }

  const TestSuiteResult* ResultStore::getResult(const ReportData& data, const std::string& suite, const EngineId& engine) {
    const auto suiteIt = data.suiteResults.find(suite);
    if (suiteIt == data.suiteResults.end()) return nullptr;

    // First try exact match
    const auto engineIt = suiteIt->second.find(engine);
    if (engineIt != suiteIt->second.end()) return &engineIt->second;

    // Fall back to flexible matching
    for (const auto& [engineId, result] : suiteIt->second) {
      if (enginesMatchFlexibly(engine, engineId)) {
        return &result;
      }
    }

    return nullptr;
  }

  const MatchResult* ResultStore::getMatch(const ReportData& data, const EngineId& engine1, const EngineId& engine2) {
    // First try exact key lookup for efficiency
    const std::string key1 = engine1.toString() + " vs " + engine2.toString();
    const std::string key2 = engine2.toString() + " vs " + engine1.toString();

    auto it = data.matchResults.find(key1);
    if (it != data.matchResults.end()) return &it->second;

    it = data.matchResults.find(key2);
    if (it != data.matchResults.end()) return &it->second;

    // Fall back to flexible matching: iterate through all matches
    for (const auto& match : data.matchResults | std::views::values) {
      const EngineId e1 = match.getEngine1Id();
      const EngineId e2 = match.getEngine2Id();

      // Check if (engine1, engine2) matches (e1, e2) in either order
      if ((enginesMatchFlexibly(engine1, e1) && enginesMatchFlexibly(engine2, e2)) || (enginesMatchFlexibly(engine1, e2) && enginesMatchFlexibly(engine2, e1))) {
        return &match;
      }
    }

    return nullptr;
  }

  std::vector<const MatchResult*> ResultStore::getMatchesForEngine(const ReportData& data, const EngineId& engine) {
    std::vector<const MatchResult*> matches;
    for (const auto& [key, match] : data.matchResults) {
      if (enginesMatchFlexibly(engine, match.getEngine1Id()) || enginesMatchFlexibly(engine, match.getEngine2Id())) {
        matches.push_back(&match);
      }
    }
    return matches;
  }

  std::vector<const BenchmarkResult*> ResultStore::getBenchmarksForEngine(const ReportData& data, const EngineId& engine) {
    std::vector<const BenchmarkResult*> benchmarks;
    for (const auto& benchmark : data.benchmarkResults) {
      if (enginesMatchFlexibly(engine, benchmark.getEngineId())) {
        benchmarks.push_back(&benchmark);
      }
    }
    return benchmarks;
  }

  bool ResultStore::enginesMatchFlexibly(const EngineId& a, const EngineId& b) {
    // Exact match
    if (a == b) return true;

    // Version must match
    if (a.version != b.version) return false;

    // Normalize names by replacing underscores with spaces
    std::string nameA = a.name;
    std::string nameB = b.name;
    std::ranges::replace(nameA, '_', ' ');
    std::ranges::replace(nameB, '_', ' ');

    if (nameA == nameB) return true;

    // Check if one name starts with the other (for variations like "FrankyCPP" vs "FrankyCPP v1.1")
    if (nameA.find(nameB) == 0 || nameB.find(nameA) == 0) return true;

    return false;
  }

  //=========================================================================
  // Private Helpers
  //=========================================================================

  std::string ResultStore::generateFilename(const std::string& prefix,
                                            const std::string& name,
                                            const std::string& engineId) const {
    std::string sanitizedName     = name;
    std::string sanitizedEngineId = engineId;
    std::ranges::replace(sanitizedName, ' ', '_');
    std::ranges::replace(sanitizedEngineId, ' ', '_');

    const std::string dir = prefix == "testsuite" ? "/testsuites" : "/matches";
    if (sanitizedEngineId.empty()) {
      return resultsDir + dir + "/" + sanitizedName + "_" + fileTimestamp() + ".json";
    }
    return resultsDir + dir + "/" + sanitizedName + "_" + sanitizedEngineId + "_" + fileTimestamp() + ".json";
  }

  void ResultStore::ensureDirectoryExists(const std::string& path) {
    if (!std::filesystem::exists(path)) {
      std::filesystem::create_directories(path);
    }
  }

} // namespace arena
