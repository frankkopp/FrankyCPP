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

//=============================================================================
// ResultWriter.h - Engine Arena Result Persistence
//=============================================================================
//
// ResultWriter handles serialization and storage of arena test results to
// JSON files for long-term tracking and version comparison.
// Depends on: ArenaResults.h
//
// Responsibilities:
//   - Write test suite results to JSON files
//   - Write match results to JSON files
//   - Generate comparison reports between versions
//   - Manage output directory structure
//   - Generate timestamped filenames
//
// Output Directory Structure:
//   results/
//     testsuites/     - EPD test suite results (JSON)
//     matches/        - Engine match results (JSON + PGN)
//     comparisons/    - Version comparison reports (text)
//
// File Naming Convention:
//   {TestSuite}_{EngineId}_{timestamp}.json
//   Example: WAC_FrankyCPP-v0.5_20260201_143022.json
//
// Timestamp Format:
//   YYYYMMDD_HHMMSS (sortable, no spaces)
//
// JSON Output Format (Test Suite):
//   {
//     "arenaVersion": "v1.1",
//     "timestamp": "2026-02-01T14:30:22Z",
//     "testSuite": {
//       "name": "WAC",
//       "epdPath": "test/testsets/wac.epd"
//     },
//     "engine": {
//       "name": "FrankyCPP",
//       "version": "v0.5",
//       "path": "Release/FrankyCPP_V0.5/FrankyCPP_v0.5.exe"
//     },
//     "summary": {
//       "totalTests": 300,
//       "passed": 285,
//       "failed": 15,
//       "successRate": 95.0,
//       "avgTimeMs": 1234.5,
//       "avgNodes": 12345678
//     },
//     "details": [ ... ]
//   }
//
// Directory Creation:
//   - Automatically creates result directories if they don't exist
//   - Called during construction to ensure output paths are valid
//
// Thread Safety:
//   - Not thread-safe (intended for single-threaded arena execution)
//   - Each write operation is atomic at filesystem level
//
// Usage:
//   ResultWriter writer("./results");
//   std::string path = writer.writeTestSuiteResult(result);
//   std::cout << "Results saved to: " << path << std::endl;
//
//=============================================================================

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

  /// Write test suite results to a JSON file
  /// @param result Test suite result data
  /// @return Path to the created file
  std::string writeTestSuiteResult(const TestSuiteResult& result);

  /// Write match results to the JSON file
  /// @param result Match result data
  /// @return Path to the created file
  std::string writeMatchResult(const MatchResult& result);

  /// Write a comparison report between two versions
  /// @param v1Results Results from version 1
  /// @param v2Results Results from version 2
  /// @return Path to the created file
  std::string writeComparison(const std::vector<TestSuiteResult>& v1Results,
                               const std::vector<TestSuiteResult>& v2Results);

private:
  std::string resultsDir;

  /// Generate filename with timestamp
  /// @param prefix Prefix for filename (e.g., "testsuite", "match")
  /// @param name Test suite or match name
  /// @param engineId Engine identifier (e.g., "FrankyCPP-v0.5")
  /// @return Filename in format: {name}_{engineId}_{timestamp}.json
  std::string generateFilename(const std::string& prefix,
                                const std::string& name,
                                const std::string& engineId) const;

  /// Get current timestamp in YYYYMMDD_HHMMSS format
  static std::string getTimestamp() ;

  /// Ensure directory exists, create if needed
  static void ensureDirectoryExists(const std::string& path);
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_RESULTWRITER_H
