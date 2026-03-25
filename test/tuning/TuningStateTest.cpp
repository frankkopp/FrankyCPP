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

#include "tuning/optimizer/TuningState.h"

#include "Test_Utils.h"
#include "config/ConfigManager.h"
#include "init.h"
#include "tuning/optimizer/TexelTuner.h"
#include "tuning/optimizer/TuningDataset.h"
#include "tuning/optimizer/TuningParameter.h"
#include "types/macros.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <version.h>

using namespace tuning;
using namespace config;

class TuningStateTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }

protected:
  void SetUp() override {
    ConfigManager::instance().resetToDefaults();
  }

  void TearDown() override {
    // Clean up temp files
    for (const auto& f : tempFiles_) {
      std::filesystem::remove(f);
    }
  }

  /// Returns a temp file path and registers it for cleanup.
  std::string tempFile(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    tempFiles_.push_back(path.string());
    return path.string();
  }

private:
  std::vector<std::string> tempFiles_;
};

// =========================================================================
// captureFromParams / restoreToParams
// =========================================================================

TEST_F(TuningStateTest, captureFromParams_CapturesAllValues) {
  std::vector<TuningParameter> params(3);
  params[0].name = "PARAM_A";
  params[0].currentValue = 10;
  params[1].name = "PARAM_B";
  params[1].currentValue = -5;
  params[2].name = "PARAM_C[0]";
  params[2].currentValue = 42;

  TuningState state;
  state.captureFromParams(params);

  ASSERT_EQ(state.paramValues.size(), 3u);
  EXPECT_EQ(state.paramValues[0].first, "PARAM_A");
  EXPECT_EQ(state.paramValues[0].second, 10);
  EXPECT_EQ(state.paramValues[1].first, "PARAM_B");
  EXPECT_EQ(state.paramValues[1].second, -5);
  EXPECT_EQ(state.paramValues[2].first, "PARAM_C[0]");
  EXPECT_EQ(state.paramValues[2].second, 42);
  // Timestamp should be set
  EXPECT_FALSE(state.timestamp.empty());
}

TEST_F(TuningStateTest, restoreToParams_RestoresAllValues) {
  TuningState state;
  state.paramValues = {{"PARAM_A", 10}, {"PARAM_B", -5}, {"PARAM_C[0]", 42}};

  std::vector<TuningParameter> params(3);
  params[0].name = "PARAM_A";
  params[0].currentValue = 0;
  params[1].name = "PARAM_B";
  params[1].currentValue = 0;
  params[2].name = "PARAM_C[0]";
  params[2].currentValue = 0;

  const int restored = state.restoreToParams(params);

  EXPECT_EQ(restored, 3);
  EXPECT_EQ(params[0].currentValue, 10);
  EXPECT_EQ(params[1].currentValue, -5);
  EXPECT_EQ(params[2].currentValue, 42);
}

TEST_F(TuningStateTest, restoreToParams_MissingParamKeepsOriginal) {
  TuningState state;
  state.paramValues = {{"PARAM_A", 10}}; // only A, missing B

  std::vector<TuningParameter> params(2);
  params[0].name = "PARAM_A";
  params[0].currentValue = 0;
  params[1].name = "PARAM_B";
  params[1].currentValue = 99;

  const int restored = state.restoreToParams(params);

  EXPECT_EQ(restored, 1);
  EXPECT_EQ(params[0].currentValue, 10);
  EXPECT_EQ(params[1].currentValue, 99) << "Missing param should keep original value";
}

TEST_F(TuningStateTest, restoreToParams_ExtraParamInCheckpoint) {
  // Checkpoint has params that don't exist in the current vector (e.g. removed param)
  TuningState state;
  state.paramValues = {{"PARAM_A", 10}, {"PARAM_REMOVED", 50}};

  std::vector<TuningParameter> params(1);
  params[0].name = "PARAM_A";
  params[0].currentValue = 0;

  const int restored = state.restoreToParams(params);

  EXPECT_EQ(restored, 1);
  EXPECT_EQ(params[0].currentValue, 10);
}

// =========================================================================
// YAML save / load round-trip
// =========================================================================

TEST_F(TuningStateTest, saveAndLoad_RoundTrip_AllFields) {
  const auto path = tempFile("checkpoint_roundtrip.yaml");

  TuningState original;
  original.completedPasses = 7;
  original.bestTrainMSE    = 0.078912345678;
  original.bestTestMSE     = 0.079234567890;
  original.K               = 0.523796;
  original.datasetPath     = "D:/_DEV/FrankyCPP/test/testsets/tuning/data.txt";
  original.timestamp       = "2026-03-25 14:30:00";
  original.paramValues = {
    {"ISOLATED_PAWN_MID", -12},
    {"KNIGHT_OUTPOST_SUPPORTED_MID", 25},
    {"KING_SAFETY_TABLE[0]", 0},
    {"KING_SAFETY_TABLE[15]", 500},
    {"TEMPO", 30},
  };

  original.saveToYaml(path);

  // Verify file exists and is non-empty
  ASSERT_TRUE(std::filesystem::exists(path));
  EXPECT_GT(std::filesystem::file_size(path), 100u);

  // Load and compare
  const auto loaded = TuningState::loadFromYaml(path);

  EXPECT_EQ(loaded.completedPasses, original.completedPasses);
  EXPECT_DOUBLE_EQ(loaded.K, original.K);
  EXPECT_NEAR(loaded.bestTrainMSE, original.bestTrainMSE, 1e-12);
  EXPECT_NEAR(loaded.bestTestMSE, original.bestTestMSE, 1e-12);
  EXPECT_EQ(loaded.datasetPath, original.datasetPath);
  EXPECT_EQ(loaded.timestamp, original.timestamp);

  ASSERT_EQ(loaded.paramValues.size(), original.paramValues.size());
  for (std::size_t i = 0; i < original.paramValues.size(); ++i) {
    EXPECT_EQ(loaded.paramValues[i].first, original.paramValues[i].first) << "Param " << i;
    EXPECT_EQ(loaded.paramValues[i].second, original.paramValues[i].second) << "Param " << i;
  }
}

TEST_F(TuningStateTest, saveAndLoad_EmptyParams) {
  const auto path = tempFile("checkpoint_empty.yaml");

  TuningState original;
  original.completedPasses = 0;
  original.K = 1.0;
  // No params

  original.saveToYaml(path);
  const auto loaded = TuningState::loadFromYaml(path);

  EXPECT_EQ(loaded.completedPasses, 0);
  EXPECT_DOUBLE_EQ(loaded.K, 1.0);
  EXPECT_TRUE(loaded.paramValues.empty());
}

TEST_F(TuningStateTest, saveAndLoad_NegativeValues) {
  const auto path = tempFile("checkpoint_negative.yaml");

  TuningState original;
  original.completedPasses = 1;
  original.K = 0.5;
  original.paramValues = {
    {"PENALTY_A", -100},
    {"PENALTY_B", -999},
    {"BONUS_C", 0},
  };

  original.saveToYaml(path);
  const auto loaded = TuningState::loadFromYaml(path);

  ASSERT_EQ(loaded.paramValues.size(), 3u);
  EXPECT_EQ(loaded.paramValues[0].second, -100);
  EXPECT_EQ(loaded.paramValues[1].second, -999);
  EXPECT_EQ(loaded.paramValues[2].second, 0);
}

TEST_F(TuningStateTest, loadFromYaml_MissingFile_Throws) {
  EXPECT_THROW((void)TuningState::loadFromYaml("nonexistent_file_xyz.yaml"), std::runtime_error);
}

TEST_F(TuningStateTest, loadFromYaml_InvalidYaml_Throws) {
  const auto path = tempFile("checkpoint_invalid.yaml");

  std::ofstream file(path);
  file << "{{{{not valid yaml\n";
  file.close();

  EXPECT_THROW((void)TuningState::loadFromYaml(path), std::runtime_error);
}

TEST_F(TuningStateTest, loadFromYaml_WrongFormat_Throws) {
  const auto path = tempFile("checkpoint_wrongformat.yaml");

  std::ofstream file(path);
  file << "format: SomeOtherFormat_v99\n";
  file << "completed_passes: 5\n";
  file.close();

  EXPECT_THROW((void)TuningState::loadFromYaml(path), std::runtime_error);
}

TEST_F(TuningStateTest, saveToYaml_InvalidPath_Throws) {
  const TuningState state;
  // Path to a directory that doesn't exist
  EXPECT_THROW(state.saveToYaml("/nonexistent_dir_xyz/checkpoint.yaml"), std::runtime_error);
}

TEST_F(TuningStateTest, yamlFile_IsHumanReadable) {
  const auto path = tempFile("checkpoint_readable.yaml");

  TuningState original;
  original.completedPasses = 3;
  original.K = 0.52;
  original.bestTrainMSE = 0.0789;
  original.timestamp = "2026-03-25 14:00:00";
  original.paramValues = {{"TEMPO", 30}, {"BISHOP_PAIR_MID_BONUS", 45}};

  original.saveToYaml(path);

  // Read file content and verify key fields are present as readable text
  std::ifstream file(path);
  const std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
  file.close();

  EXPECT_NE(content.find("FrankyCPP_TuningCheckpoint_v1"), std::string::npos);
  EXPECT_NE(content.find("completed_passes"), std::string::npos);
  EXPECT_NE(content.find("TEMPO"), std::string::npos);
  EXPECT_NE(content.find("BISHOP_PAIR_MID_BONUS"), std::string::npos);
  EXPECT_NE(content.find("2026-03-25"), std::string::npos);

  std::cout << "  YAML checkpoint content (" << content.size() << " bytes):\n";
  std::cout << content << "\n";
}

// =========================================================================
// Full round-trip with real registry params
// =========================================================================

TEST_F(TuningStateTest, roundTrip_RealRegistryParams) {
  const auto path = tempFile("checkpoint_registry.yaml");

  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto params = TuningParameter::buildFromRegistry(searchConfig, evalConfig);
  ASSERT_FALSE(params.empty());

  // Modify some params
  params[0].currentValue += 5;
  params[1].currentValue -= 3;

  // Save
  TuningState state;
  state.completedPasses = 2;
  state.K = 0.52;
  state.bestTrainMSE = 0.078;
  state.captureFromParams(params);
  state.saveToYaml(path);

  // Reset params to original values
  for (auto& p : params) {
    p.currentValue = p.originalValue;
  }

  // Load and restore
  const auto loaded = TuningState::loadFromYaml(path);
  const int restored = loaded.restoreToParams(params);

  EXPECT_EQ(restored, static_cast<int>(params.size()));
  EXPECT_EQ(params[0].currentValue, params[0].originalValue + 5);
  EXPECT_EQ(params[1].currentValue, params[1].originalValue - 3);

  std::cout << "  Round-tripped " << params.size() << " params through YAML checkpoint\n";
}

// =========================================================================
// Timestamp
// =========================================================================

TEST_F(TuningStateTest, currentTimestamp_ReturnsValidFormat) {
  const auto ts = TuningState::currentTimestamp();

  // Should be "YYYY-MM-DD HH:MM:SS" format (19 chars)
  EXPECT_EQ(ts.size(), 19u) << "Timestamp: " << ts;
  EXPECT_EQ(ts[4], '-');
  EXPECT_EQ(ts[7], '-');
  EXPECT_EQ(ts[10], ' ');
  EXPECT_EQ(ts[13], ':');
  EXPECT_EQ(ts[16], ':');

  std::cout << "  Current timestamp: " << ts << "\n";
}

// =========================================================================
// Integration: checkpoint during coordinate descent
// =========================================================================

TEST_F(TuningStateTest, coordinateDescent_SavesCheckpoint) {
  const std::string devPath =
    std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/v1.6_vs_v1.5_score.txt";
  const auto checkpointPath = tempFile("checkpoint_cd.yaml");

  TuningDataset dataset;
  try {
    dataset.loadFromFile(devPath, 2000);
  }
  catch (...) {
    GTEST_SKIP() << "Dev dataset not available at " << devPath;
  }

  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluators(4);

  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto params = TuningParameter::buildFromRegistry(searchConfig, evalConfig);

  const double K = tuner.tuneK(dataset);

  // Run 1 pass with checkpoint saving
  tuner.tuneParameters(dataset, nullptr, params, 1, checkpointPath, devPath);

  // Verify checkpoint file was created
  ASSERT_TRUE(std::filesystem::exists(checkpointPath)) << "Checkpoint file should exist after tuning";

  // Load and verify
  const auto loaded = TuningState::loadFromYaml(checkpointPath);
  EXPECT_EQ(loaded.completedPasses, 1);
  EXPECT_GT(loaded.bestTrainMSE, 0.0);
  EXPECT_NEAR(loaded.K, K, 1e-10);
  EXPECT_EQ(loaded.datasetPath, devPath);
  EXPECT_EQ(loaded.paramValues.size(), params.size());
  EXPECT_FALSE(loaded.timestamp.empty());

  std::cout << "  Checkpoint: pass " << loaded.completedPasses
            << ", MSE " << loaded.bestTrainMSE
            << ", " << loaded.paramValues.size() << " params\n";
}
