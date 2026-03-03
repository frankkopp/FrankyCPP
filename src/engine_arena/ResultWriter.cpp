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
// ResultWriter.cpp - Engine Arena Result Persistence Implementation
//=============================================================================

#include "ResultWriter.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace arena {

  namespace {
    // Escapes string values for JSON output.
    std::string escapeJsonString(const std::string& value) {
      std::string escaped;
      escaped.reserve(value.size() + value.size() / 4);

      for (const unsigned char ch : value) {
        switch (ch) {
          case '\"':
            escaped += "\\\"";
            break;
          case '\\':
            escaped += "\\\\";
            break;
          case '\b':
            escaped += "\\b";
            break;
          case '\f':
            escaped += "\\f";
            break;
          case '\n':
            escaped += "\\n";
            break;
          case '\r':
            escaped += "\\r";
            break;
          case '\t':
            escaped += "\\t";
            break;
          default:
            if (ch < 0x20) {
              char buffer[7];// NOLINT(*-avoid-c-arrays)
              std::snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
              escaped += buffer;
            }
            else {
              escaped += static_cast<char>(ch);
            }
        }
      }

      return escaped;
    }
  }// namespace

  ResultWriter::ResultWriter(const std::string& resultsDir)
      : resultsDir(resultsDir) {
    // Ensure base results directory exists
    ensureDirectoryExists(resultsDir);
    ensureDirectoryExists(resultsDir + "/testsuites");
    ensureDirectoryExists(resultsDir + "/matches");
    ensureDirectoryExists(resultsDir + "/comparisons");
    ensureDirectoryExists(resultsDir + "/benchmarks");
  }

  std::string ResultWriter::writeTestSuiteResult(const TestSuiteResult& result) const {
    // New file naming: {TestSuite}_{EngineName}-{EngineVersion}_{Timestamp}.json
    // e.g., WAC_FrankyCPP-v0.5_20260205_143000.json
    const std::string engineId = result.engineName + "-" + result.engineVersion;
    std::string filename       = generateFilename("testsuite", result.testSuiteName, engineId);

    std::ofstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file for writing: " + filename);
    }

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

    // Write JSON with new structure per spec Section 2.3
    file << "{\n";
    file << "  \"arenaVersion\": \"" << escapeJsonString(result.arenaVersion) << "\",\n";
    file << "  \"timestamp\": \"" << escapeJsonString(result.timestamp) << "\",\n";
    file << "\n";
    file << "  \"testSuite\": {\n";
    file << "    \"name\": \"" << escapeJsonString(result.testSuiteName) << "\",\n";
    file << "    \"epdPath\": \"" << escapeJsonString(result.epdPath) << "\"\n";
    file << "  },\n";
    file << "\n";
    file << "  \"engine\": {\n";
    file << "    \"name\": \"" << escapeJsonString(result.engineName) << "\",\n";
    file << "    \"version\": \"" << escapeJsonString(result.engineVersion) << "\",\n";
    file << "    \"path\": \"" << escapeJsonString(result.enginePath) << "\"\n";
    file << "  },\n";
    file << "\n";
    file << "  \"summary\": {\n";
    file << "    \"totalTests\": " << result.totalTests << ",\n";
    file << "    \"passed\": " << result.passed << ",\n";
    file << "    \"failed\": " << result.failed << ",\n";
    file << "    \"skipped\": " << result.skipped << ",\n";

    file << std::fixed << std::setprecision(2);
    file << "    \"successRate\": " << successRate << ",\n";
    file << "    \"totalNodes\": " << result.totalNodes << ",\n";
    file << "    \"totalTimeMs\": " << result.totalTimeMs << ",\n";
    file << "    \"avgTimeMs\": " << avgTimeMs << ",\n";
    file << "    \"avgNodes\": " << avgNodes << "\n";
    file << std::defaultfloat;

    file << "  },\n";
    file << "\n";
    file << "  \"details\": [\n";

    // Write per-test details
    for (size_t i = 0; i < result.details.size(); ++i) {
      const auto& detail = result.details[i];
      file << "    {\n";
      file << "      \"testId\": \"" << escapeJsonString(detail.testId) << "\",\n";
      file << "      \"fen\": \"" << escapeJsonString(detail.fen) << "\",\n";
      file << "      \"expected\": \"" << escapeJsonString(detail.expected) << "\",\n";
      file << "      \"actual\": \"" << escapeJsonString(detail.actual) << "\",\n";
      file << "      \"passed\": " << (detail.passed ? "true" : "false") << ",\n";
      file << "      \"nodes\": " << detail.nodes << ",\n";
      file << "      \"timeMs\": " << detail.timeMs << "\n";
      file << "    }" << (i < result.details.size() - 1 ? ",\n" : "\n");
    }

    file << "  ]\n";
    file << "}\n";

    file.close();
    return filename;
  }

  std::string ResultWriter::writeMatchResult(const MatchResult& result) const {
    // New file naming: {Engine1Id}_vs_{Engine2Id}_{TimeControl}_{Timestamp}.json
    // e.g., FrankyCPP-v1.1_vs_FrankyGo-v1.0.3_60+0.6_20260205_143000.json
    const std::string engine1Id = result.engine1Name + "-" + result.engine1Version;
    const std::string engine2Id = result.engine2Name + "-" + result.engine2Version;
    std::string sanitizedTC     = result.timeControl;
    std::ranges::replace(sanitizedTC, '+', '_');
    std::ranges::replace(sanitizedTC, '/', '_');

    const std::string matchId = engine1Id + "_vs_" + engine2Id + "_" + sanitizedTC;
    std::string filename      = generateFilename("match", matchId, "");

    std::ofstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    // Write JSON with new structure per spec
    file << "{\n";
    file << "  \"arenaVersion\": \"" << escapeJsonString(result.arenaVersion) << "\",\n";
    file << "  \"timestamp\": \"" << escapeJsonString(result.timestamp) << "\",\n";
    file << "\n";
    file << "  \"match\": {\n";
    file << "    \"name\": \"" << escapeJsonString(result.matchName) << "\",\n";
    file << "    \"timeControl\": \"" << escapeJsonString(result.timeControl) << "\",\n";
    file << "    \"rounds\": " << result.rounds << "\n";
    file << "  },\n";
    file << "\n";
    file << "  \"engine1\": {\n";
    file << "    \"name\": \"" << escapeJsonString(result.engine1Name) << "\",\n";
    file << "    \"version\": \"" << escapeJsonString(result.engine1Version) << "\",\n";
    file << "    \"path\": \"" << escapeJsonString(result.engine1Path) << "\"\n";
    file << "  },\n";
    file << "\n";
    file << "  \"engine2\": {\n";
    file << "    \"name\": \"" << escapeJsonString(result.engine2Name) << "\",\n";
    file << "    \"version\": \"" << escapeJsonString(result.engine2Version) << "\",\n";
    file << "    \"path\": \"" << escapeJsonString(result.engine2Path) << "\"\n";
    file << "  },\n";
    file << "\n";
    file << "  \"results\": {\n";
    file << "    \"engine1Wins\": " << result.engine1Wins << ",\n";
    file << "    \"engine2Wins\": " << result.engine2Wins << ",\n";
    file << "    \"draws\": " << result.draws << ",\n";

    file << std::fixed << std::setprecision(1);
    file << "    \"engine1Score\": " << result.engine1Score << ",\n";
    file << "    \"engine2Score\": " << result.engine2Score << ",\n";
    file << "    \"eloDifference\": " << result.eloDifference << "\n";
    file << std::defaultfloat;

    file << "  },\n";
    file << "\n";
    file << "  \"pgnPath\": \"" << escapeJsonString(result.pgnPath) << "\",\n";
    file << "  \"durationMs\": " << result.durationMs << "\n";
    file << "}\n";

    file.close();
    return filename;
  }

  std::string ResultWriter::writeBenchmarkResult(const BenchmarkResult& result) const {
    const std::string filename = resultsDir + "/benchmarks/benchmarks.json";

    // Read existing results
    std::vector<BenchmarkResult> allResults = readBenchmarkResults();

    // Add new result
    allResults.push_back(result);

    // Write all results back (overwrite file)
    std::ofstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    file << "{\n";
    file << "  \"benchmarks\": [\n";

    for (size_t i = 0; i < allResults.size(); ++i) {
      const auto& r = allResults[i];
      file << "    {\n";
      file << "      \"arenaVersion\": \"" << escapeJsonString(r.arenaVersion) << "\",\n";
      file << "      \"timestamp\": \"" << escapeJsonString(r.timestamp) << "\",\n";
      file << "      \"engineName\": \"" << escapeJsonString(r.engineName) << "\",\n";
      file << "      \"engineVersion\": \"" << escapeJsonString(r.engineVersion) << "\",\n";
      file << "      \"enginePath\": \"" << escapeJsonString(r.enginePath) << "\",\n";
      file << "      \"depth\": " << r.depth << ",\n";
      file << "      \"hashSizeMB\": " << r.hashSizeMB << ",\n";
      file << "      \"threads\": " << r.threads << ",\n";
      file << "      \"positions\": " << r.positions << ",\n";
      file << "      \"totalNodes\": " << r.totalNodes << ",\n";
      file << "      \"totalTimeMs\": " << r.totalTimeMs << ",\n";
      file << "      \"nps\": " << r.nps << ",\n";
      file << "      \"notes\": \"" << escapeJsonString(r.notes) << "\"\n";
      file << "    }" << (i < allResults.size() - 1 ? ",\n" : "\n");
    }

    file << "  ]\n";
    file << "}\n";

    file.close();
    return filename;
  }

  std::vector<BenchmarkResult> ResultWriter::readBenchmarkResults() const {
    std::vector<BenchmarkResult> results;
    const std::string filename = resultsDir + "/benchmarks/benchmarks.json";

    if (!std::filesystem::exists(filename)) {
      return results;// Empty vector if file doesn't exist
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
      return results;
    }

    // Simple JSON parsing for our specific format
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Parse each benchmark entry
    // This is a simple parser that assumes well-formed JSON from our writer
    size_t pos = 0;
    while ((pos = content.find("\"arenaVersion\"", pos)) != std::string::npos) {
      BenchmarkResult r;

      // Helper lambda to extract string value
      auto extractString = [&content](size_t startPos, const std::string& key) -> std::string {
        const size_t keyPos = content.find("\"" + key + "\"", startPos);
        if (keyPos == std::string::npos) return "";
        const size_t colonPos = content.find(':', keyPos);
        if (colonPos == std::string::npos) return "";
        const size_t quoteStart = content.find('"', colonPos + 1);
        if (quoteStart == std::string::npos) return "";
        const size_t quoteEnd = content.find('"', quoteStart + 1);
        if (quoteEnd == std::string::npos) return "";
        return content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
      };

      // Helper lambda to extract numeric value
      auto extractNumber = [&content](size_t startPos, const std::string& key) -> int64_t {
        const size_t keyPos = content.find("\"" + key + "\"", startPos);
        if (keyPos == std::string::npos) return 0;
        const size_t colonPos = content.find(':', keyPos);
        if (colonPos == std::string::npos) return 0;
        size_t numStart = colonPos + 1;
        while (numStart < content.size() && (content[numStart] == ' ' || content[numStart] == '\n')) {
          numStart++;
        }
        size_t numEnd = numStart;
        while (numEnd < content.size() && (std::isdigit(content[numEnd]) || content[numEnd] == '-')) {
          numEnd++;
        }
        if (numStart == numEnd) return 0;
        return std::stoll(content.substr(numStart, numEnd - numStart));
      };

      // Find the closing brace for this entry
      const size_t entryEnd = content.find('}', pos);
      if (entryEnd == std::string::npos) break;

      r.arenaVersion  = extractString(pos, "arenaVersion");
      r.timestamp     = extractString(pos, "timestamp");
      r.engineName    = extractString(pos, "engineName");
      r.engineVersion = extractString(pos, "engineVersion");
      r.enginePath    = extractString(pos, "enginePath");
      r.depth         = static_cast<int>(extractNumber(pos, "depth"));
      r.hashSizeMB    = static_cast<int>(extractNumber(pos, "hashSizeMB"));
      r.threads       = static_cast<int>(extractNumber(pos, "threads"));
      r.positions     = static_cast<int>(extractNumber(pos, "positions"));
      r.totalNodes    = static_cast<uint64_t>(extractNumber(pos, "totalNodes"));
      r.totalTimeMs   = extractNumber(pos, "totalTimeMs");
      r.nps           = static_cast<uint64_t>(extractNumber(pos, "nps"));
      r.notes         = extractString(pos, "notes");

      results.push_back(r);
      pos = entryEnd + 1;
    }

    return results;
  }

  std::string ResultWriter::writeComparison(const std::vector<TestSuiteResult>& v1Results,
                                            const std::vector<TestSuiteResult>& v2Results) const {
    // Placeholder implementation - will be completed in Phase 4
    // For now, just return expected filename
    if (v1Results.empty() || v2Results.empty()) {
      return "";
    }
    // Build engine identifiers for filename
    const std::string engine1Id = v1Results[0].engineName + "-" + v1Results[0].engineVersion;
    const std::string engine2Id = v2Results[0].engineName + "-" + v2Results[0].engineVersion;
    std::string filename        = resultsDir + "/comparisons/" + engine1Id + "_vs_" + engine2Id + "_" + getTimestamp() + ".txt";
    return filename;
  }

  std::string ResultWriter::generateFilename(const std::string& prefix,
                                             const std::string& name,
                                             const std::string& engineId) const {
    std::string sanitizedName     = name;
    std::string sanitizedEngineId = engineId;
    // Replace spaces with underscores
    std::ranges::replace(sanitizedName, ' ', '_');
    std::ranges::replace(sanitizedEngineId, ' ', '_');

    const std::string dir = prefix == "testsuite" ? "/testsuites" : "/matches";
    // New naming: {TestSuite}_{EngineId}_{Timestamp}.json
    // e.g., WAC_FrankyCPP-v0.5_20260205_143000.json
    return resultsDir + dir + "/" + sanitizedName + "_" + sanitizedEngineId + "_" + getTimestamp() + ".json";
  }

  std::string ResultWriter::getTimestamp() {
    const auto now  = system_clock::now();
    const auto time = system_clock::to_time_t(now);

    std::stringstream ss;
    std::tm timeInfo{};
#ifdef _WIN32
    localtime_s(&timeInfo, &time);
#else
    localtime_r(&time, &timeInfo);
#endif
    ss << std::put_time(&timeInfo, "%Y%m%d_%H%M%S");
    return ss.str();
  }

  void ResultWriter::ensureDirectoryExists(const std::string& path) {
    if (!std::filesystem::exists(path)) {
      std::filesystem::create_directories(path);
    }
  }

}// namespace arena
