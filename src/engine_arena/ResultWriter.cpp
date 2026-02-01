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

#include <fstream>
#include <filesystem>
#include <chrono>
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
  // Placeholder implementation - will be completed in Phase 2
  // For now, just return expected filename
  return generateFilename("testsuite", result.suiteName, result.version);
}

std::string ResultWriter::writeMatchResult(const MatchResult& result) {
  // Placeholder implementation - will be completed in Phase 3
  // For now, just return expected filename
  return generateFilename("match", result.matchName, result.version);
}

std::string ResultWriter::writeComparison(const std::vector<TestSuiteResult>& v1Results,
                                          const std::vector<TestSuiteResult>& v2Results) {
  // Placeholder implementation - will be completed in Phase 4
  // For now, just return expected filename
  if (v1Results.empty() || v2Results.empty()) {
    return "";
  }
  std::string filename = resultsDir + "/comparisons/" +
                         v1Results[0].version + "_vs_" + v2Results[0].version +
                         "_" + getTimestamp() + ".txt";
  return filename;
}

std::string ResultWriter::generateFilename(const std::string& prefix,
                                            const std::string& name,
                                            const std::string& version) const {
  std::string sanitizedName = name;
  // Replace spaces with underscores
  std::replace(sanitizedName.begin(), sanitizedName.end(), ' ', '_');

  std::string dir = prefix == "testsuite" ? "/testsuites" : "/matches";
  return resultsDir + dir + "/" + version + "_" + sanitizedName + "_" +
         getTimestamp() + ".json";
}

std::string ResultWriter::getTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
  return ss.str();
}

void ResultWriter::ensureDirectoryExists(const std::string& path) {
  if (!std::filesystem::exists(path)) {
    std::filesystem::create_directories(path);
  }
}

} // namespace arena
