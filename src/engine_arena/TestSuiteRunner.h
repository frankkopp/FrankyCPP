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

#ifndef FRANKYCPP_ENGINE_ARENA_TESTSUITERUNNER_H
#define FRANKYCPP_ENGINE_ARENA_TESTSUITERUNNER_H

//=============================================================================
// TestSuiteRunner.h - Engine Arena Test Suite Execution
//=============================================================================
//
// TestSuiteRunner executes EPD test suites against external UCI chess engines
// and captures results with metadata for version comparison.
//
// Responsibilities:
//   - Execute EPD test suites via external UCI engines
//   - Parse EPD files and evaluate each position
//   - Support BM (best move), AM (avoid move), and DM (direct mate) tests
//   - Capture detailed per-test results with statistics
//   - Add metadata (version, timestamp, engine info)
//   - Provide simple interface for running all configured suites
//
// Architecture:
//   - External-only: All engines tested via UCI protocol
//   - Engine reuse: One UCIEngine instance per test suite (not per position)
//   - Position isolation: Optional state clearing between positions (via isolatePositions flag)
//   - Error resilience: Continue suite on position failures
//
// UCI Engine Lifecycle:
//   1. Create UCIEngine instance once at suite start
//   2. For each position:
//      a. Call newGame() to clear state (if isolatePositions=true)
//      b. Set position via setPosition(fen)
//      c. Execute search with time/depth limits
//      d. Evaluate result against expected moves
//   3. Destroy UCIEngine once at suite end (sends "quit")
//
// Metadata Captured:
//   - Engine name (from UCI "id name")
//   - Engine path (executable location)
//   - Test execution timestamp (ISO 8601 format)
//   - Per-position statistics (nodes, depth, time, score)
//
// Usage:
//   TestSuiteRunner runner(config);
//   TestSuiteResult result = runner.runTestSuite(config.testSuites[0]);
//   std::cout << "Passed: " << result.passed << "/" << result.totalTests << std::endl;
//
// Multiple Suites:
//   auto allResults = runner.runAllTestSuites();
//   for (const auto& result : allResults) {
//     std::cout << result.suiteName << ": " << result.passed << " passed" << std::endl;
//   }
//
// Error Handling:
//   - Throws std::runtime_error if EPD file not found
//   - Throws std::runtime_error if engine fails to start
//   - Logs and continues if individual positions fail
//   - Caller should catch and log suite-level errors
//
// Thread Safety:
//   - Not thread-safe (UCI communication is sequential)
//   - Run test suites sequentially from single thread
//
//=============================================================================

#include "ArenaConfig.h"
#include "ArenaResults.h"

#include <vector>

namespace arena {

/// Executes EPD test suites against external UCI engines
class TestSuiteRunner {
public:
  /// Creates a TestSuiteRunner with the given configuration
  /// @param config Arena configuration containing test suite definitions
  explicit TestSuiteRunner(const ArenaConfig& config);

  /// Runs a single test suite and returns detailed results
  /// @param suiteConfig Test suite configuration
  /// @return TestSuiteResult with full metadata and per-test details
  /// @throws std::runtime_error if EPD file not found or execution fails
  TestSuiteResult runTestSuite(const TestSuiteConfig& suiteConfig) const;

  /// Runs all configured test suites sequentially
  /// @return Vector of TestSuiteResult, one per configured suite
  /// @throws std::runtime_error if any suite fails
  std::vector<TestSuiteResult> runAllTestSuites() const;

private:
  const ArenaConfig& arenaConfig; ///< Reference to arena configuration

  /// Generates ISO 8601 timestamp for current time
  /// @return Timestamp string (e.g., "2026-02-01T14:30:22Z")
  static std::string getCurrentTimestamp();
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_TESTSUITERUNNER_H
