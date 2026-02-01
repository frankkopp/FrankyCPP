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

#ifndef FRANKYCPP_ENGINE_ARENA_RESULTWRITER_H
#define FRANKYCPP_ENGINE_ARENA_RESULTWRITER_H

#include "ArenaResults.h"

#include <string>
#include <vector>

namespace arena {

/// Writes arena results to JSON files
class ResultWriter {
public:
  /// Creates a ResultWriter for the specified output directory
  /// @param resultsDir Root directory for results (e.g., "./results")
  explicit ResultWriter(const std::string& resultsDir);

  /// Write test suite results to JSON file
  /// @param result Test suite result data
  /// @return Path to the created file
  std::string writeTestSuiteResult(const TestSuiteResult& result);

  /// Write match results to JSON file
  /// @param result Match result data
  /// @return Path to the created file
  std::string writeMatchResult(const MatchResult& result);

  /// Write comparison report between two versions
  /// @param v1Results Results from version 1
  /// @param v2Results Results from version 2
  /// @return Path to the created file
  std::string writeComparison(const std::vector<TestSuiteResult>& v1Results,
                               const std::vector<TestSuiteResult>& v2Results);

private:
  std::string resultsDir;

  /// Generate filename with timestamp
  /// @param prefix Prefix for filename (e.g., "testsuite", "match")
  /// @param name Test/match name
  /// @param version Version string
  /// @return Filename in format: {version}_{name}_{timestamp}.json
  std::string generateFilename(const std::string& prefix,
                                const std::string& name,
                                const std::string& version) const;

  /// Get current timestamp in YYYYMMDD_HHMMSS format
  std::string getTimestamp() const;

  /// Ensure directory exists, create if needed
  void ensureDirectoryExists(const std::string& path);
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_RESULTWRITER_H
