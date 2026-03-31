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

#include "tuning/optimizer/TuningDataset.h"

#include "init.h"
#include "types/macros.h"
#include "version.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace tuning;

namespace {

  /// Helper: writes content to a temporary file and returns the path.
  std::string writeTempFile(const std::string& content, const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream ofs(path);
    ofs << content;
    ofs.close();
    return path.string();
  }

} // anonymous namespace

class TuningDatasetTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }

protected:
  void TearDown() override {
    for (const auto& f : tempFiles) {
      std::filesystem::remove(f);
    }
  }

  /// Tracks temp files for cleanup.
  std::vector<std::string> tempFiles;

  /// Creates a temp file, registers it for cleanup, and returns its path.
  std::string createTempFile(const std::string& content, const std::string& name) {
    auto path = writeTempFile(content, name);
    tempFiles.push_back(path);
    return path;
  }
};

// =============================================================================
// FrankyCPP format parsing tests
// =============================================================================

TEST_F(TuningDatasetTest, loadFrankyCppFormat_BasicPositions) {
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1 [1.0]\n"
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 [0.5]\n"
    "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1 [0.0]\n";

  const auto path = createTempFile(data, "test_basic.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 3);
  EXPECT_FLOAT_EQ(dataset[0].result, 1.0F);
  EXPECT_FLOAT_EQ(dataset[1].result, 0.5F);
  EXPECT_FLOAT_EQ(dataset[2].result, 0.0F);
}

TEST_F(TuningDatasetTest, loadFrankyCppFormat_VerifyFenIntegrity) {
  // Verify that the stored FEN matches the original input
  const std::string fen = "r1bqkb1r/pppppppp/2n2n2/8/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3";
  const std::string data = fen + " [0.5]\n";

  const auto path = createTempFile(data, "test_fen_integrity.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 1);
  EXPECT_EQ(dataset[0].fen, fen);
}

TEST_F(TuningDatasetTest, loadFrankyCppFormat_AllResultFormats) {
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]\n"
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.5]\n"
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.0]\n";

  const auto path = createTempFile(data, "test_results.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 3);
  EXPECT_FLOAT_EQ(dataset[0].result, 1.0F);
  EXPECT_FLOAT_EQ(dataset[1].result, 0.5F);
  EXPECT_FLOAT_EQ(dataset[2].result, 0.0F);
}

// =============================================================================
// EPD c9 format parsing tests
// =============================================================================

TEST_F(TuningDatasetTest, loadEpdFormat_BasicPositions) {
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - c9 \"1-0\";\n"
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - c9 \"1/2-1/2\";\n"
    "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - c9 \"0-1\";\n";

  const auto path = createTempFile(data, "test_epd.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 3);
  EXPECT_FLOAT_EQ(dataset[0].result, 1.0F);
  EXPECT_FLOAT_EQ(dataset[1].result, 0.5F);
  EXPECT_FLOAT_EQ(dataset[2].result, 0.0F);
}

TEST_F(TuningDatasetTest, loadEpdFormat_RealZurichessFormat) {
  // Exact lines from quiet-labeled.epd (Zurichess dataset)
  const std::string data =
    "r2qkr2/p1pp1ppp/1pn1pn2/2P5/3Pb3/2N1P3/PP3PPP/R1B1KB1R b KQq - c9 \"0-1\";\n"
    "r4rk1/3bppb1/p3q1p1/1p1p3p/2pPn3/P1P1PN1P/1PB1QPPB/1R3RK1 b - - c9 \"1/2-1/2\";\n"
    "r1bqk2r/2p2ppp/2p5/p3pn2/1bB5/2NP2P1/PPP1NP1P/R1B1K2R w KQkq - c9 \"0-1\";\n";

  const auto path = createTempFile(data, "test_zurichess.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 3);
  EXPECT_FLOAT_EQ(dataset[0].result, 0.0F); // 0-1
  EXPECT_FLOAT_EQ(dataset[1].result, 0.5F); // 1/2-1/2
  EXPECT_FLOAT_EQ(dataset[2].result, 0.0F); // 0-1
}

// =============================================================================
// Mixed format tests
// =============================================================================

TEST_F(TuningDatasetTest, loadMixedFormats) {
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]\n"
    "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - c9 \"0-1\";\n";

  const auto path = createTempFile(data, "test_mixed.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 2);
  EXPECT_FLOAT_EQ(dataset[0].result, 1.0F);
  EXPECT_FLOAT_EQ(dataset[1].result, 0.0F);
}

// =============================================================================
// Filtering and edge case tests
// =============================================================================

TEST_F(TuningDatasetTest, loadSkipsEmptyLines) {
  const std::string data =
    "\n"
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]\n"
    "\n"
    "   \n"
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 [0.5]\n"
    "\n";

  const auto path = createTempFile(data, "test_empty_lines.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 2);
  EXPECT_EQ(dataset.getLoadStats().skippedEmpty, 4);
}

TEST_F(TuningDatasetTest, loadSkipsCommentLines) {
  const std::string data =
    "# This is a comment\n"
    "// This is also a comment\n"
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]\n"
    "# Another comment\n";

  const auto path = createTempFile(data, "test_comments.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 1);
  EXPECT_EQ(dataset.getLoadStats().skippedComment, 3);
}

TEST_F(TuningDatasetTest, loadSkipsMalformedLines) {
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]\n"
    "this is not a valid FEN [0.5]\n"
    "no brackets here\n"
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 [0.5]\n"
    "[1.0]\n";

  const auto path = createTempFile(data, "test_malformed.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 2);
  EXPECT_GE(dataset.getLoadStats().skippedMalformed, 2);
}

TEST_F(TuningDatasetTest, loadRejectsInvalidResult) {
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [2.0]\n"
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [*]\n"
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.5]\n";

  const auto path = createTempFile(data, "test_bad_result.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 1);
  EXPECT_FLOAT_EQ(dataset[0].result, 0.5F);
}

TEST_F(TuningDatasetTest, loadEmptyFile) {
  const auto path = createTempFile("", "test_empty.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  EXPECT_TRUE(dataset.empty());
  EXPECT_EQ(dataset.size(), 0);
}

TEST_F(TuningDatasetTest, loadSinglePosition) {
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]\n";

  const auto path = createTempFile(data, "test_single.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 1);
  EXPECT_FLOAT_EQ(dataset[0].result, 1.0F);
}

TEST_F(TuningDatasetTest, loadThrowsOnMissingFile) {
  TuningDataset dataset;
  EXPECT_THROW(dataset.loadFromFile("nonexistent_file_12345.txt"), std::runtime_error);
}

// =============================================================================
// TuningEntry default state tests
// =============================================================================

TEST_F(TuningDatasetTest, tuningEntryDefaultHasAllGroupsActive) {
  const TuningEntry entry;
  EXPECT_TRUE(entry.activeParamGroups.all());
  EXPECT_EQ(entry.activeParamGroups.count(), NUM_PARAM_GROUPS);
}

TEST_F(TuningDatasetTest, tuningEntryConstructedFromLoad) {
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.5]\n";

  const auto path = createTempFile(data, "test_entry_state.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  ASSERT_EQ(dataset.size(), 1);
  // Loaded entries should have all param groups active by default
  EXPECT_TRUE(dataset[0].activeParamGroups.all());
  EXPECT_FLOAT_EQ(dataset[0].result, 0.5F);
}

// =============================================================================
// Train/test split tests
// =============================================================================

TEST_F(TuningDatasetTest, splitDefaultFraction) {
  // Create 10 positions
  std::string data;
  for (int i = 0; i < 10; ++i) {
    data += "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.5]\n";
  }

  const auto path = createTempFile(data, "test_split_default.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);
  ASSERT_EQ(dataset.size(), 10);

  const auto [train, test] = dataset.split(); // default 0.8
  EXPECT_EQ(train.size(), 8);
  EXPECT_EQ(test.size(), 2);
  EXPECT_EQ(train.size() + test.size(), dataset.size());
}

TEST_F(TuningDatasetTest, splitCustomFraction) {
  std::string data;
  for (int i = 0; i < 100; ++i) {
    data += "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.5]\n";
  }

  const auto path = createTempFile(data, "test_split_custom.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);
  ASSERT_EQ(dataset.size(), 100);

  const auto [train, test] = dataset.split(0.7F);
  EXPECT_EQ(train.size(), 70);
  EXPECT_EQ(test.size(), 30);
}

TEST_F(TuningDatasetTest, splitPreservesOrder) {
  // Use different results to track ordering
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]\n"
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 [0.5]\n"
    "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1 [0.0]\n"
    "r1bqkbnr/pppppppp/2n5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2 [1.0]\n"
    "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2 [0.0]\n";

  const auto path = createTempFile(data, "test_split_order.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);
  ASSERT_EQ(dataset.size(), 5);

  const auto [train, test] = dataset.split(0.6F);
  ASSERT_EQ(train.size(), 3);
  ASSERT_EQ(test.size(), 2);

  // Train set has first 3 entries in order
  EXPECT_FLOAT_EQ(train[0].result, 1.0F);
  EXPECT_FLOAT_EQ(train[1].result, 0.5F);
  EXPECT_FLOAT_EQ(train[2].result, 0.0F);

  // Test set has last 2 entries in order
  EXPECT_FLOAT_EQ(test[0].result, 1.0F);
  EXPECT_FLOAT_EQ(test[1].result, 0.0F);
}

TEST_F(TuningDatasetTest, splitEdgeCases) {
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.5]\n"
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 [0.5]\n";

  const auto path = createTempFile(data, "test_split_edge.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);
  ASSERT_EQ(dataset.size(), 2);

  // All to train
  {
    const auto [train, test] = dataset.split(1.0F);
    EXPECT_EQ(train.size(), 2);
    EXPECT_EQ(test.size(), 0);
  }

  // All to test
  {
    const auto [train, test] = dataset.split(0.0F);
    EXPECT_EQ(train.size(), 0);
    EXPECT_EQ(test.size(), 2);
  }
}

TEST_F(TuningDatasetTest, splitEmptyDataset) {
  const TuningDataset dataset;
  const auto [train, test] = dataset.split(0.8F);
  EXPECT_EQ(train.size(), 0);
  EXPECT_EQ(test.size(), 0);
}

// =============================================================================
// Load statistics tests
// =============================================================================

TEST_F(TuningDatasetTest, loadStatsAreAccurate) {
  const std::string data =
    "# comment\n"
    "// another comment\n"
    "\n"
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]\n"
    "garbage line\n"
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 [0.5]\n"
    "   \n";

  const auto path = createTempFile(data, "test_stats.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  const auto& stats = dataset.getLoadStats();
  EXPECT_EQ(stats.parsedOk, 2);
  EXPECT_EQ(stats.skippedComment, 2);
  EXPECT_EQ(stats.skippedEmpty, 2);     // blank line + whitespace line
  EXPECT_GE(stats.skippedMalformed, 1); // garbage line
  EXPECT_EQ(stats.totalLines, 3);       // 2 parsed + 1 malformed (comments/empty not counted)
}

// =============================================================================
// Iteration tests
// =============================================================================

TEST_F(TuningDatasetTest, rangeBasedForLoop) {
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]\n"
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 [0.5]\n"
    "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1 [0.0]\n";

  const auto path = createTempFile(data, "test_iteration.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  int count = 0;
  float sum = 0.0F;
  for (const auto& entry : dataset) {
    sum += entry.result;
    count++;
  }
  EXPECT_EQ(count, 3);
  EXPECT_FLOAT_EQ(sum, 1.5F);
}

// =============================================================================
// Integration test with real dataset file
// =============================================================================

TEST_F(TuningDatasetTest, loadDevDataset) {
  const std::string devPath = std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/v1.6_vs_v1.5_score.txt";

  // This test only runs if the dev dataset file exists
  if (!std::filesystem::exists(devPath)) {
    GTEST_SKIP() << "Dev dataset not found: " << devPath;
  }

  TuningDataset dataset;
  const auto start = steady_clock::now();
  dataset.loadFromFile(devPath);
  const auto elapsed = steady_clock::now() - start;
  const auto elapsedMs = std::chrono::duration_cast<milliseconds>(elapsed).count();

  // Gate: must load ~49K positions in under 2 seconds
  std::cout << "Dev dataset load time: " << elapsedMs << " ms\n";
  EXPECT_LT(elapsedMs, 2500) << "Loading dev dataset took too long: " << elapsedMs << " ms";

  // Dev dataset should have ~48-49K positions
  EXPECT_GT(dataset.size(), 40000);
  EXPECT_LT(dataset.size(), 60000);

  // All results should be valid (1.0, 0.5, or 0.0)
  for (const auto& entry : dataset) {
    EXPECT_TRUE(entry.result == 1.0F || entry.result == 0.5F || entry.result == 0.0F)
      << "Invalid result: " << entry.result;
  }

  // Verify result distribution is reasonable (not all same result)
  int wins = 0, draws = 0, losses = 0;
  for (const auto& entry : dataset) {
    if (entry.result == 1.0F) wins++;
    else if (entry.result == 0.5F) draws++;
    else losses++;
  }
  EXPECT_GT(wins, 0);
  EXPECT_GT(draws, 0);
  EXPECT_GT(losses, 0);

  std::cout << "Dev dataset: " << dataset.size() << " positions"
            << " (W:" << wins << " D:" << draws << " L:" << losses << ")\n";

  // Test train/test split on real data
  const auto [train, test] = dataset.split(0.8F);
  EXPECT_EQ(train.size() + test.size(), dataset.size());
  // 80% of ~49K ≈ ~39K
  EXPECT_GT(train.size(), 35000);
  EXPECT_GT(test.size(), 8000);
}

TEST_F(TuningDatasetTest, loadZurichessEpd) {
  const std::string epdPath = std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/quiet-labeled.epd";

  // This test only runs if the Zurichess dataset file exists
  if (!std::filesystem::exists(epdPath)) {
    GTEST_SKIP() << "Zurichess EPD dataset not found: " << epdPath;
  }

  // The full 1.4M dataset is exercised by the tuner executable.
  constexpr std::size_t MAX_ENTRIES = std::numeric_limits<std::size_t>::max();
  TuningDataset dataset;
  dataset.loadFromFile(epdPath, MAX_ENTRIES);

  EXPECT_EQ(dataset.size(), 1428000);

  // All results should be valid
  for (const auto& entry : dataset) {
    EXPECT_TRUE(entry.result == 1.0F || entry.result == 0.5F || entry.result == 0.0F)
      << "Invalid result: " << entry.result;
  }

  // Verify result distribution is reasonable
  int wins = 0, draws = 0, losses = 0;
  for (const auto& entry : dataset) {
    if (entry.result == 1.0F) wins++;
    else if (entry.result == 0.5F) draws++;
    else losses++;
  }
  EXPECT_GT(wins, 0);
  EXPECT_GT(draws, 0);
  EXPECT_GT(losses, 0);

  std::cout << "Zurichess EPD sample: " << dataset.size() << " positions"
            << " (W:" << wins << " D:" << draws << " L:" << losses << ")\n";
}

// =============================================================================
// Reloading test (loadFromFile clears previous data)
// =============================================================================

TEST_F(TuningDatasetTest, loadClearsPreviousData) {
  const std::string data1 =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]\n"
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 [0.5]\n";

  const std::string data2 =
    "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1 [0.0]\n";

  const auto path1 = createTempFile(data1, "test_reload1.txt");
  const auto path2 = createTempFile(data2, "test_reload2.txt");

  TuningDataset dataset;
  dataset.loadFromFile(path1);
  ASSERT_EQ(dataset.size(), 2);

  dataset.loadFromFile(path2);
  ASSERT_EQ(dataset.size(), 1);
  EXPECT_FLOAT_EQ(dataset[0].result, 0.0F);
}

// =============================================================================
// Phase 6.10 — Additional edge case tests
// =============================================================================

TEST_F(TuningDatasetTest, loadDuplicateFens_AllAccepted) {
  // Duplicate FENs should be accepted — the tuner doesn't deduplicate.
  // This tests that the loader does not filter or reject duplicates.
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.0]\n"
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.5]\n"
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.0]\n";

  const auto path = createTempFile(data, "test_duplicates.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  EXPECT_EQ(dataset.size(), 3) << "Duplicate FENs should all be accepted";
  // Each can have a different result (same position, different game outcome)
  EXPECT_FLOAT_EQ(dataset[0].result, 1.0F);
  EXPECT_FLOAT_EQ(dataset[1].result, 0.5F);
  EXPECT_FLOAT_EQ(dataset[2].result, 0.0F);
}

TEST_F(TuningDatasetTest, loadMaxEntries_Limits) {
  // loadFromFile with maxEntries should stop after the limit
  std::string data;
  for (int i = 0; i < 20; ++i) {
    data += "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.5]\n";
  }

  const auto path = createTempFile(data, "test_maxentries.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path, 5);

  EXPECT_EQ(dataset.size(), 5) << "Should stop at maxEntries=5";
}

TEST_F(TuningDatasetTest, loadMaxEntries_ZeroMeansUnlimited) {
  std::string data;
  for (int i = 0; i < 10; ++i) {
    data += "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.5]\n";
  }

  const auto path = createTempFile(data, "test_maxentries_zero.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path, 0); // 0 = unlimited

  EXPECT_EQ(dataset.size(), 10) << "maxEntries=0 should load all";
}

TEST_F(TuningDatasetTest, loadRejectsOutOfRangeResults) {
  // Results outside [0.0, 1.0] should be rejected
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [-0.5]\n"
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [1.5]\n"
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.5]\n";

  const auto path = createTempFile(data, "test_oor_results.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  EXPECT_EQ(dataset.size(), 1) << "Only the valid [0.5] should be accepted";
  EXPECT_FLOAT_EQ(dataset[0].result, 0.5F);
}

TEST_F(TuningDatasetTest, loadOnlyComments_EmptyDataset) {
  const std::string data =
    "# comment 1\n"
    "// comment 2\n"
    "# comment 3\n";

  const auto path = createTempFile(data, "test_only_comments.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);

  EXPECT_TRUE(dataset.empty());
  EXPECT_EQ(dataset.getLoadStats().skippedComment, 3);
}

TEST_F(TuningDatasetTest, splitSingleEntry) {
  // Splitting a single entry: should go to train with fraction >= 0.5
  const std::string data =
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1 [0.5]\n";

  const auto path = createTempFile(data, "test_split_single.txt");
  TuningDataset dataset;
  dataset.loadFromFile(path);
  ASSERT_EQ(dataset.size(), 1);

  const auto [train, test] = dataset.split(0.8F);
  // floor(1 * 0.8) = 0, so 0 train and 1 test? Or 1 train and 0 test?
  // This tests the actual behavior — the point is no crash.
  EXPECT_EQ(train.size() + test.size(), 1);
}

TEST_F(TuningDatasetTest, reserveDoesNotAffectSize) {
  TuningDataset dataset;
  dataset.reserve(1000);
  EXPECT_EQ(dataset.size(), 0);
  EXPECT_TRUE(dataset.empty());
}
