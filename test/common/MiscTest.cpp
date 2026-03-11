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

#include "common/misc.h"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace common {
  //=============================================================================
  // getEnv Tests
  //=============================================================================

  TEST(MiscTest, getEnv_nullptrReturnsEmpty) {
    EXPECT_EQ(getEnv(nullptr), "");
  }

  TEST(MiscTest, getEnv_nonexistentVarReturnsEmpty) {
    // Use a variable name that definitely doesn't exist
    EXPECT_EQ(getEnv("FRANKYCPP_NONEXISTENT_VAR_12345"), "");
  }

  TEST(MiscTest, getEnv_existingVarReturnsValue) {
    // PATH should exist on all platforms
    const std::string path = getEnv("PATH");
    EXPECT_FALSE(path.empty()) << "PATH environment variable should exist";
  }

  TEST(MiscTest, getEnv_windowsSpecificVars) {
#ifdef _WIN32
    // These should exist on Windows
    EXPECT_FALSE(getEnv("SYSTEMROOT").empty());
    EXPECT_FALSE(getEnv("USERPROFILE").empty());
#else
    GTEST_SKIP() << "Windows-specific test";
#endif
  }

  TEST(MiscTest, getEnv_posixSpecificVars) {
#ifndef _WIN32
    // These should exist on POSIX systems
    EXPECT_FALSE(getEnv("HOME").empty());
    EXPECT_FALSE(getEnv("USER").empty());
#else
    GTEST_SKIP() << "POSIX-specific test";
#endif
  }

  //=============================================================================
  // getEnvOpt Tests
  //=============================================================================

  TEST(MiscTest, getEnvOpt_nullptrReturnsNullopt) {
    EXPECT_FALSE(getEnvOpt(nullptr).has_value());
  }

  TEST(MiscTest, getEnvOpt_nonexistentVarReturnsNullopt) {
    EXPECT_FALSE(getEnvOpt("FRANKYCPP_NONEXISTENT_VAR_12345").has_value());
  }

  TEST(MiscTest, getEnvOpt_existingVarReturnsValue) {
    const auto path = getEnvOpt("PATH");
    ASSERT_TRUE(path.has_value()) << "PATH environment variable should exist";
    EXPECT_FALSE(path->empty());
  }

  TEST(MiscTest, getEnvOpt_valueMatchesGetEnv) {
    // Verify both functions return the same value
    const auto optValue = getEnvOpt("PATH");
    const auto strValue = getEnv("PATH");

    ASSERT_TRUE(optValue.has_value());
    EXPECT_EQ(*optValue, strValue);
  }

  //=============================================================================
  // Thread Safety Tests
  //=============================================================================

  TEST(MiscTest, getEnv_threadSafety) {
    // Call getEnv concurrently from multiple threads
    constexpr int numThreads = 10;
    constexpr int iterations = 100;

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
      threads.emplace_back([&successCount]() {
        for (int j = 0; j < iterations; ++j) {
          const std::string path = getEnv("PATH");
          if (!path.empty()) {
            ++successCount;
          }
        }
      });
    }

    for (auto& t : threads) {
      t.join();
    }

    // All reads should have succeeded
    EXPECT_EQ(successCount.load(), numThreads * iterations);
  }
} // namespace common
