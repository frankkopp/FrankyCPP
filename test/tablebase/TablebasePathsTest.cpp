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

#include "tablebase/TablebasePaths.h"
#include "Test_Utils.h"
#include "common/Logging.h"
#include "config/ConfigManager.h"
#include "init.h"

#include <gtest/gtest.h>

using namespace tablebase;
using namespace chess;
using namespace config;
using namespace common;

class TablebasePathsTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().TB_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

//=============================================================================
// getDefaultTablebasePath Tests
//=============================================================================

TEST_F(TablebasePathsTest, getDefaultTablebasePath_returnsNonEmpty) {
  const std::string path = getDefaultTablebasePath();
  // Should return a platform-appropriate path (may not exist)
#ifdef _WIN32
  // Windows: should contain FrankyCPP\syzygy
  if (!path.empty()) {
    EXPECT_NE(path.find("FrankyCPP"), std::string::npos);
    EXPECT_NE(path.find("syzygy"), std::string::npos);
  }
#else
  // Linux: should contain frankycpp/syzygy
  if (!path.empty()) {
    EXPECT_NE(path.find("frankycpp"), std::string::npos);
    EXPECT_NE(path.find("syzygy"), std::string::npos);
  }
#endif
  LOG__INFO(Logger::get().TEST_LOG, "Default TB path: {}", path.empty() ? "(empty)" : path);
}

//=============================================================================
// getEnvironmentPath Tests
//=============================================================================

TEST_F(TablebasePathsTest, getEnvironmentPath_returnsEnvValue) {
  const std::string path = getEnvironmentPath();
  // Just verify it doesn't crash - actual value depends on environment
  LOG__INFO(Logger::get().TEST_LOG, "Environment TB_PATH: {}", path.empty() ? "(not set)" : path);
}

//=============================================================================
// getConfiguredPath Tests
//=============================================================================

TEST_F(TablebasePathsTest, getConfiguredPath_returnsConfigValue) {
  const std::string path = getConfiguredPath();
  // Should return the value from ConfigManager
  const std::string expected = ConfigManager::instance().search().TB_PATH;
  EXPECT_EQ(path, expected);
  LOG__INFO(Logger::get().TEST_LOG, "Configured TB_PATH: {}", path.empty() ? "(empty)" : path);
}

//=============================================================================
// validateTablebasePath Tests
//=============================================================================

TEST_F(TablebasePathsTest, validateTablebasePath_emptyPath) {
  EXPECT_FALSE(validateTablebasePath(""));
}

TEST_F(TablebasePathsTest, validateTablebasePath_nonexistentPath) {
  EXPECT_FALSE(validateTablebasePath("C:/nonexistent/path/to/tablebases"));
  EXPECT_FALSE(validateTablebasePath("/nonexistent/path/to/tablebases"));
}

TEST_F(TablebasePathsTest, validateTablebasePath_directoryWithoutTBFiles) {
  // Current directory exists but likely has no .rtbw/.rtbz files
  // This test may pass or fail depending on where tests are run
  const bool result = validateTablebasePath(".");
  LOG__INFO(Logger::get().TEST_LOG, "Current directory has TB files: {}", result);
}

TEST_F(TablebasePathsTest, validateTablebasePath_hasTBFiles) {
  // Current directory exists but likely has no .rtbw/.rtbz files
  // This test may pass or fail depending on where tests are run
  const bool result = validateTablebasePath(getConfiguredPath());
  LOG__INFO(Logger::get().TEST_LOG, "Directory {} has TB files: {}", getConfiguredPath(), result);
}
//=============================================================================
// countTablebaseFiles Tests
//=============================================================================

TEST_F(TablebasePathsTest, countTablebaseFiles_emptyPath) {
  const auto [wdl, dtz] = countTablebaseFiles("");
  EXPECT_EQ(wdl, 0);
  EXPECT_EQ(dtz, 0);
}

TEST_F(TablebasePathsTest, countTablebaseFiles_nonexistentPath) {
  const auto [wdl, dtz] = countTablebaseFiles("C:/nonexistent/path");
  EXPECT_EQ(wdl, 0);
  EXPECT_EQ(dtz, 0);
}

//=============================================================================
// findTablebasePath Tests
//=============================================================================

TEST_F(TablebasePathsTest, findTablebasePath_explicitOverride) {
  // If we pass a valid path explicitly, it should be returned
  const std::string configPath = getConfiguredPath();
  if (!configPath.empty() && validateTablebasePath(configPath)) {
    const std::string found = findTablebasePath(configPath);
    EXPECT_EQ(found, configPath);
  }
}

TEST_F(TablebasePathsTest, findTablebasePath_invalidExplicitFallsBack) {
  // Invalid explicit path should fall back to other sources
  const std::string found = findTablebasePath("/invalid/nonexistent/path");
  // Result depends on whether other sources have valid paths
  LOG__INFO(Logger::get().TEST_LOG, "findTablebasePath with invalid explicit: {}",
            found.empty() ? "(none found)" : found);
}

//=============================================================================
// getTablebaseStatus Tests
//=============================================================================

TEST_F(TablebasePathsTest, getTablebaseStatus_noPath) {
  const std::string status = getTablebaseStatus("/nonexistent/path");
  EXPECT_FALSE(status.empty());
  LOG__INFO(Logger::get().TEST_LOG, "TB status (invalid path): {}", status);
}

TEST_F(TablebasePathsTest, getTablebaseStatus_autoDetect) {
  const std::string status = getTablebaseStatus();
  EXPECT_FALSE(status.empty());
  LOG__INFO(Logger::get().TEST_LOG, "TB status (auto-detect): {}", status);
}

//=============================================================================
// Integration Tests (require actual tablebases)
//=============================================================================

// Macro for skipping tests based on tablebase requirements.
// Must be called directly in the test function (not in a helper).
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SKIP_IF_TABLEBASES_UNAVAIL                \
  {                                               \
    const std::string path = findTablebasePath(); \
    if (path.empty()) {                           \
      GTEST_SKIP() << "Tablebases not available"; \
    }                                             \
  }


class TablebasePathsIntegrationTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().TB_LOG->set_level(spdlog::level::debug);
  }
};

TEST_F(TablebasePathsIntegrationTest, findTablebasePath_findsValidPath) {
  SKIP_IF_TABLEBASES_UNAVAIL

  const std::string path = findTablebasePath();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(validateTablebasePath(path));

  LOG__INFO(Logger::get().TEST_LOG, "Found valid TB path: {}", path);
}

TEST_F(TablebasePathsIntegrationTest, countTablebaseFiles_countsCorrectly) {
  SKIP_IF_TABLEBASES_UNAVAIL;

  const std::string path = findTablebasePath();
  const auto [wdl, dtz]  = countTablebaseFiles(path);

  EXPECT_GT(wdl, 0) << "Expected at least one WDL file";
  // DTZ files are optional
  LOG__INFO(Logger::get().TEST_LOG, "TB file count: {} WDL, {} DTZ", wdl, dtz);
}

TEST_F(TablebasePathsIntegrationTest, getTablebaseStatus_showsDetails) {
  SKIP_IF_TABLEBASES_UNAVAIL;

  const std::string status = getTablebaseStatus();
  EXPECT_NE(status.find("available"), std::string::npos);
  LOG__INFO(Logger::get().TEST_LOG, "TB status: {}", status);
}
