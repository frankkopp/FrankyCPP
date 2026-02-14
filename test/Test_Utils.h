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

#ifndef FRANKYCPP_TEST_UTILS_H
#define FRANKYCPP_TEST_UTILS_H

#include "common/misc.h"

#include <iostream>
#include <gtest/gtest.h>

/**
 * @brief Cross-platform safe environment variable exists check
 * @param varName Environment variable name
 * @return true if the variable exists and is non-empty, false otherwise
 */
inline bool getEnvVarExists(const char* varName) {
  return !getEnv(varName).empty();
}

/**
 * @brief Check if tests are running in bulk mode (CI or multiple tests locally)
 *
 * This function is used to skip or shorten long-running tests when running in:
 * - CI/CD environments (GitHub Actions, etc.)
 * - Local bulk test runs (multiple tests executed together)
 *
 * @return true if running in bulk/CI mode, false for individual test runs
 */
inline bool isBulkRun() {
  // Check if running in CI environment
  if (getEnvVarExists("CI") || getEnvVarExists("GITHUB_ACTIONS")) {
    std::cout << "CI environment detected - skipping long-running tests" << std::endl;
    return true;
  }

  // Check if running multiple tests locally
  const auto* ut  = testing::UnitTest::GetInstance();
  const bool cond = ut && ut->test_to_run_count() > 1;
  if (cond) {
    std::cout << "Bulk run detected - limiting depth to shorten test time" << std::endl;
  }
  return cond;
}

#endif // FRANKYCPP_TEST_UTILS_H
