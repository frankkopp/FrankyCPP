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
// TestSuiteRunner wraps the existing TestSuite class to execute EPD test
// suites and capture results with metadata for version comparison.
// Depends on: ArenaConfig.h, ArenaResults.h, enginetest/TestSuite.h
//
// Responsibilities:
//   - Execute EPD test suites using existing TestSuite class
//   - Capture detailed per-test results
//   - Add metadata (version, timestamp, system info)
//   - Convert TestSuite results to ArenaResults format
//   - Provide simple interface for running all configured suites
//
// Metadata Added:
//   - Engine version (from ArenaConfig)
//   - Test execution timestamp (ISO 8601 format)
//   - System info (OS, compiler, build type) - future enhancement
//
// Result Conversion:
//   Converts from enginetest::TestSuiteResult + Test vector to
//   arena::TestSuiteResult with TestCaseDetail breakdown.
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
//   - Throws std::runtime_error if TestSuite execution fails
//   - Caller should catch and log errors appropriately
//
// Thread Safety:
//   - Not thread-safe (TestSuite is not thread-safe)
//   - Run test suites sequentially from single thread
//
//=============================================================================

#include "ArenaConfig.h"
#include "ArenaResults.h"

#include <vector>

// Forward declarations from global namespace (enginetest/TestSuite.h and Test.h)
struct TestSuiteResult;  // from TestTypes.h
class EpdTest;           // from Test.h

namespace arena {

/// Executes EPD test suites and captures results with metadata
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

  /// Converts TestSuite internal result to arena result format
  /// @param suiteConfig Test suite configuration
  /// @param internalResult Result from TestSuite::getLastResult()
  /// @param testCases Vector of EpdTest from TestSuite
  /// @return Populated TestSuiteResult with metadata
  TestSuiteResult convertToArenaResult(
      const TestSuiteConfig& suiteConfig,
      const ::TestSuiteResult& internalResult,
      const std::vector<EpdTest>& testCases) const;
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_TESTSUITERUNNER_H
