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

#include "tablebase/TablebaseDownloader.h"
#include "Test_Utils.h"
#include "common/Logging.h"
#include "init.h"

#include <gtest/gtest.h>

using namespace tablebase;

class TablebaseDownloaderTest : public testing::Test {
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
// getRequiredFiles Tests (requires network)
//=============================================================================

TEST_F(TablebaseDownloaderTest, getRequiredFiles_3piece) {
  const auto files = TablebaseDownloader::getRequiredFiles({3});
  if (files.empty()) {
    GTEST_SKIP() << "Could not fetch file list from server (network unavailable?)";
  }
  // 3-piece has 5 unique base names (KQvK, KRvK, KBvK, KNvK, KPvK) - KvK is just 2 kings
  EXPECT_GE(files.size(), 5u);
  EXPECT_LE(files.size(), 10u);
}

TEST_F(TablebaseDownloaderTest, getRequiredFiles_empty) {
  const auto files = TablebaseDownloader::getRequiredFiles({});
  EXPECT_TRUE(files.empty());
}

//=============================================================================
// estimateDownloadSize Tests
//=============================================================================

TEST_F(TablebaseDownloaderTest, estimateDownloadSize_3piece) {
  const size_t size = TablebaseDownloader::estimateDownloadSize({3});
  EXPECT_GT(size, 0u);
  EXPECT_LT(size, 100 * 1024 * 1024); // Less than 100MB
}

TEST_F(TablebaseDownloaderTest, estimateDownloadSize_345piece) {
  const size_t size = TablebaseDownloader::estimateDownloadSize({3, 4, 5});
  EXPECT_GT(size, 1024 * 1024 * 1024); // More than 1GB (5-piece is ~1GB)
}

TEST_F(TablebaseDownloaderTest, estimateDownloadSize_empty) {
  const size_t size = TablebaseDownloader::estimateDownloadSize({});
  EXPECT_EQ(size, 0u);
}

//=============================================================================
// formatSize Tests
//=============================================================================

TEST_F(TablebaseDownloaderTest, formatSize_bytes) {
  EXPECT_EQ(TablebaseDownloader::formatSize(500), "500 bytes");
}

TEST_F(TablebaseDownloaderTest, formatSize_KB) {
  const std::string result = TablebaseDownloader::formatSize(5 * 1024);
  EXPECT_NE(result.find("KB"), std::string::npos);
}

TEST_F(TablebaseDownloaderTest, formatSize_MB) {
  const std::string result = TablebaseDownloader::formatSize(50 * 1024 * 1024);
  EXPECT_NE(result.find("MB"), std::string::npos);
}

TEST_F(TablebaseDownloaderTest, formatSize_GB) {
  const std::string result = TablebaseDownloader::formatSize(5ULL * 1024 * 1024 * 1024);
  EXPECT_NE(result.find("GB"), std::string::npos);
}

//=============================================================================
// DownloadProgress Tests
//=============================================================================

TEST_F(TablebaseDownloaderTest, downloadProgress_percentComplete) {
  DownloadProgress progress;
  progress.totalFiles = 100;
  progress.filesCompleted = 50;
  EXPECT_EQ(progress.percentComplete(), 50);
}

TEST_F(TablebaseDownloaderTest, downloadProgress_percentComplete_zero) {
  DownloadProgress progress;
  progress.totalFiles = 0;
  progress.filesCompleted = 0;
  EXPECT_EQ(progress.percentComplete(), 0);
}

//=============================================================================
// download Tests (validation only, no actual download)
//=============================================================================

TEST_F(TablebaseDownloaderTest, download_emptyConfig) {
  TablebaseDownloader downloader;
  const DownloadConfig config;
  // Empty pieceCounts should fail
  const auto result = downloader.download(config);
  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.errors.empty());
}

TEST_F(TablebaseDownloaderTest, download_noPieceCounts) {
  TablebaseDownloader downloader;
  DownloadConfig config;
  config.targetPath = ".";
  // No pieceCounts should fail
  const auto result = downloader.download(config);
  EXPECT_FALSE(result.success);
}

TEST_F(TablebaseDownloaderTest, download_noTargetPath) {
  TablebaseDownloader downloader;
  DownloadConfig config;
  config.pieceCounts = {3};
  // No targetPath should fail
  const auto result = downloader.download(config);
  EXPECT_FALSE(result.success);
}
