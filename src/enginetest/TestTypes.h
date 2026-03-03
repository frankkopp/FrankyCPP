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

#ifndef FRANKYCPP_TESTTYPES_H
#define FRANKYCPP_TESTTYPES_H

//=============================================================================
// TestTypes.h - Common Test Infrastructure Types
//=============================================================================
//
// Defines common types used across the test infrastructure:
// - TestType: Type of test (DM, BM, AM)
// - ResultType: Result of test execution (SUCCESS, FAILED, etc.)
// - TestSuiteResult: Aggregated test suite statistics
//
// All enums are modern enum classes for type safety.
// String conversion functions are constexpr for compile-time evaluation.
//
//=============================================================================

#include "types/timeunits.h"
#include <cstdint>
#include <string_view>

namespace enginetest {

  /// Test types supported by the test suite.
  enum class TestType : uint8_t {
    NOOP,///< Invalid/uninitialized test
    DM,  ///< Direct mate - expect mate in N
    BM,  ///< Best move - engine move must be in target set
    AM   ///< Avoid move - engine move must NOT be in target set
  };

  /// Result of a single test.
  enum class ResultType : uint8_t {
    NOT_TESTED,///< Test has not been run yet
    SKIPPED,   ///< Test was skipped
    FAILED,    ///< Test failed (wrong move or no mate found)
    SUCCESS    ///< Test passed
  };

  /// Converts TestType to string representation.
  /// @param type TestType value
  /// @return String view of test type name
  [[nodiscard]] constexpr std::string_view testTypeToString(const TestType type) noexcept {
    switch (type) {
      case TestType::NOOP:
        return "noop";
      case TestType::DM:
        return "dm";
      case TestType::BM:
        return "bm";
      case TestType::AM:
        return "am";
    }
    return "unknown";
  }

  /// Converts ResultType to string representation.
  /// @param result ResultType value
  /// @return String view of result type name
  [[nodiscard]] constexpr std::string_view resultTypeToString(const ResultType result) noexcept {
    switch (result) {
      case ResultType::NOT_TESTED:
        return "Not tested";
      case ResultType::SKIPPED:
        return "Skipped";
      case ResultType::FAILED:
        return "Failed";
      case ResultType::SUCCESS:
        return "Success";
    }
    return "unknown";
  }

  /// Aggregated results from running a test suite.
  /// Simple POD struct for returning aggregate statistics.
  struct TestSuiteResult {
    int counter          = 0; ///< Total tests run
    int successCounter   = 0; ///< Tests that passed
    int failedCounter    = 0; ///< Tests that failed
    int skippedCounter   = 0; ///< Tests that were skipped
    int notTestedCounter = 0; ///< Tests not run
    uint64_t nodes       = 0; ///< Total nodes searched
    nanoseconds time     = 0s;///< Total search time
  };

}// namespace enginetest

#endif// FRANKYCPP_TESTTYPES_H
