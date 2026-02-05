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
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace arena {

  ResultWriter::ResultWriter(const std::string& resultsDir)
      : resultsDir(resultsDir) {
    // Ensure base results directory exists
    ensureDirectoryExists(resultsDir);
    ensureDirectoryExists(resultsDir + "/testsuites");
    ensureDirectoryExists(resultsDir + "/matches");
    ensureDirectoryExists(resultsDir + "/comparisons");
  }

  std::string ResultWriter::writeTestSuiteResult(const TestSuiteResult& result) {
    // New file naming: {TestSuite}_{EngineName}-{EngineVersion}_{Timestamp}.json
    // e.g., WAC_FrankyCPP-v0.5_20260205_143000.json
    std::string engineId = result.engineName + "-" + result.engineVersion;
    std::string filename = generateFilename("testsuite", result.testSuiteName, engineId);

    std::ofstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    // Calculate derived metrics
    double successRate = result.totalTests > 0
        ? (result.passed * 100.0 / result.totalTests)
        : 0.0;
    double avgTimeMs = result.totalTests > 0
        ? (static_cast<double>(result.totalTimeMs) / result.totalTests)
        : 0.0;
    double avgNodes = result.totalTests > 0
        ? (static_cast<double>(result.totalNodes) / result.totalTests)
        : 0.0;

    // Write JSON with new structure per spec Section 2.3
    file << "{\n";
    file << "  \"arenaVersion\": \"" << result.arenaVersion << "\",\n";
    file << "  \"timestamp\": \"" << result.timestamp << "\",\n";
    file << "\n";
    file << "  \"testSuite\": {\n";
    file << "    \"name\": \"" << result.testSuiteName << "\",\n";
    file << "    \"epdPath\": \"" << result.epdPath << "\"\n";
    file << "  },\n";
    file << "\n";
    file << "  \"engine\": {\n";
    file << "    \"name\": \"" << result.engineName << "\",\n";
    file << "    \"version\": \"" << result.engineVersion << "\",\n";
    file << "    \"path\": \"" << result.enginePath << "\"\n";
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
      file << "      \"testId\": \"" << detail.testId << "\",\n";
      file << "      \"fen\": \"" << detail.fen << "\",\n";
      file << "      \"expected\": \"" << detail.expected << "\",\n";
      file << "      \"actual\": \"" << detail.actual << "\",\n";
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

  std::string ResultWriter::writeMatchResult(const MatchResult& result) {
    std::string filename = generateFilename("match", result.matchName, result.version);

    std::ofstream file(filename);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    // Write JSON
    file << "{\n";
    file << "  \"version\": \"" << result.version << "\",\n";
    file << "  \"matchName\": \"" << result.matchName << "\",\n";
    file << "  \"timestamp\": \"" << result.timestamp << "\",\n";
    file << "  \"engines\": {\n";
    file << "    \"engine1\": \"" << result.engine1Name << "\",\n";
    file << "    \"engine2\": \"" << result.engine2Name << "\"\n";
    file << "  },\n";
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
    file << "  \"pgnPath\": \"" << result.pgnPath << "\",\n";
    file << "  \"durationMs\": " << result.durationMs << "\n";
    file << "}\n";

    file.close();
    return filename;
  }

  std::string ResultWriter::writeComparison(const std::vector<TestSuiteResult>& v1Results,
                                            const std::vector<TestSuiteResult>& v2Results) {
    // Placeholder implementation - will be completed in Phase 4
    // For now, just return expected filename
    if (v1Results.empty() || v2Results.empty()) {
      return "";
    }
    // Build engine identifiers for filename
    std::string engine1Id = v1Results[0].engineName + "-" + v1Results[0].engineVersion;
    std::string engine2Id = v2Results[0].engineName + "-" + v2Results[0].engineVersion;
    std::string filename = resultsDir + "/comparisons/" + engine1Id + "_vs_" + engine2Id + "_" + getTimestamp() + ".txt";
    return filename;
  }

  std::string ResultWriter::generateFilename(const std::string& prefix,
                                             const std::string& name,
                                             const std::string& engineId) const {
    std::string sanitizedName = name;
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
