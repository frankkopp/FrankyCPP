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

#include "tuning/optimizer/TexelTuner.h"

#include "Test_Utils.h"
#include "config/ConfigManager.h"
#include "init.h"
#include "tuning/optimizer/TuningDataset.h"
#include "tuning/optimizer/TuningOutput.h"
#include "tuning/optimizer/TuningParameter.h"
#include "tuning/optimizer/TuningState.h"
#include "types/macros.h"
#include "version.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace tuning;
using namespace config;

class TexelTunerTest : public testing::Test {
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
};

// =========================================================================
// Sigmoid tests
// =========================================================================

TEST_F(TexelTunerTest, sigmoid_ZeroEval_ReturnsHalf) {
  // eval = 0 → equal position → expected outcome = 0.5
  const double result = TexelTuner::sigmoid(1.0, 0.0);
  EXPECT_DOUBLE_EQ(result, 0.5);
}

TEST_F(TexelTunerTest, sigmoid_LargePositive_NearOne) {
  // eval = +1000 cp → strongly favors White → near 1.0
  const double result = TexelTuner::sigmoid(1.0, 1000.0);
  EXPECT_GT(result, 0.99);
  EXPECT_LE(result, 1.0);
}

TEST_F(TexelTunerTest, sigmoid_LargeNegative_NearZero) {
  // eval = -1000 cp → strongly favors Black → near 0.0
  const double result = TexelTuner::sigmoid(1.0, -1000.0);
  EXPECT_LT(result, 0.01);
  EXPECT_GE(result, 0.0);
}

TEST_F(TexelTunerTest, sigmoid_Symmetric) {
  // sigmoid(K, +e) + sigmoid(K, -e) should equal 1.0
  const double pos = TexelTuner::sigmoid(1.0, 200.0);
  const double neg = TexelTuner::sigmoid(1.0, -200.0);
  EXPECT_NEAR(pos + neg, 1.0, 1e-12);
}

TEST_F(TexelTunerTest, sigmoid_Plus100cp) {
  // eval = +100 cp (one pawn advantage) with K=1.0
  // σ = 1 / (1 + 10^(-100/400)) = 1 / (1 + 10^(-0.25)) ≈ 0.640
  const double result = TexelTuner::sigmoid(1.0, 100.0);
  EXPECT_NEAR(result, 0.640, 0.001);
}

TEST_F(TexelTunerTest, sigmoid_DifferentK) {
  // Larger K → steeper sigmoid → more extreme predictions
  const double k1 = TexelTuner::sigmoid(0.5, 100.0);
  const double k2 = TexelTuner::sigmoid(1.0, 100.0);
  const double k3 = TexelTuner::sigmoid(2.0, 100.0);
  // All should be > 0.5 (positive eval), but higher K → further from 0.5
  EXPECT_GT(k1, 0.5);
  EXPECT_GT(k2, k1);
  EXPECT_GT(k3, k2);
}

// =========================================================================
// MSE computation tests
// =========================================================================

TEST_F(TexelTunerTest, computeMSE_EmptyDataset_ReturnsZero) {
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  const TuningDataset empty;
  const double mse = tuner.computeMSE(empty, 1.0);
  EXPECT_DOUBLE_EQ(mse, 0.0);
}

TEST_F(TexelTunerTest, computeMSE_ThrowsWithoutEvaluator) {
  const TexelTuner tuner;
  TuningDataset dataset;
  // Add one entry so it's not empty
  dataset.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 0.5F);

  EXPECT_THROW((void)tuner.computeMSE(dataset, 1.0), std::logic_error);
}

TEST_F(TexelTunerTest, computeMSE_StartPosition_Draw) {
  // Starting position with draw result → MSE should be small
  // because eval ≈ 0 and sigmoid(K, 0) = 0.5 ≈ draw
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  dataset.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);

  const double mse = tuner.computeMSE(dataset, 1.0);
  // Eval should be near 0 for start position (maybe small tempo bonus)
  // so predicted ≈ 0.5, error ≈ 0, MSE should be small
  EXPECT_LT(mse, 0.01);
  std::cout << "  Start position MSE (draw label): " << mse << "\n";
}

TEST_F(TexelTunerTest, computeMSE_WhiteWinning_WhiteWinLabel) {
  // Position where White is clearly winning + result = 1.0
  // MSE should be small (prediction aligns with result)
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  // White is up a queen (major material advantage)
  dataset.getEntries().emplace_back(
    "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1.0F);

  const double mse = tuner.computeMSE(dataset, 1.0);
  // Eval should be strongly positive for White → sigmoid near 1.0 → small error
  EXPECT_LT(mse, 0.05);
  std::cout << "  White winning + white-win label MSE: " << mse << "\n";
}

TEST_F(TexelTunerTest, computeMSE_WhiteWinning_BlackWinLabel) {
  // Position where White is clearly winning but result = 0.0 (Black win)
  // MSE should be large (prediction contradicts result)
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  // White is up a queen (major material advantage) but labeled as Black win
  dataset.getEntries().emplace_back(
    "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.0F);

  const double mse = tuner.computeMSE(dataset, 1.0);
  // Eval strongly positive → sigmoid near 1.0, but result = 0.0 → large error
  EXPECT_GT(mse, 0.5);
  std::cout << "  White winning + black-win label MSE: " << mse << "\n";
}

TEST_F(TexelTunerTest, computeMSE_HandCraftedDataset) {
  // Small dataset with varied positions and results
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  auto& entries = dataset.getEntries();

  // Start position, draw
  entries.emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  // After 1. e4, draw (slightly favoring White due to tempo)
  entries.emplace_back(
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 0.5F);
  // White up a rook, White wins
  entries.emplace_back(
    "rnbqkbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);

  const double mse = tuner.computeMSE(dataset, 1.0);
  // Should produce a reasonable MSE (not zero, not huge)
  EXPECT_GT(mse, 0.0);
  EXPECT_LT(mse, 0.5);
  std::cout << "  Hand-crafted 3-position dataset MSE: " << mse << "\n";
}

TEST_F(TexelTunerTest, computeMSE_EvalPerspectiveBlackToMove) {
  // Verify eval perspective is handled correctly for Black-to-move positions.
  // Same material advantage but viewed from Black's side.
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  // White is up a queen, White to move, labeled White win
  TuningDataset datasetW;
  datasetW.getEntries().emplace_back(
    "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1.0F);
  const double mseW = tuner.computeMSE(datasetW, 1.0);

  // White is up a queen, Black to move, labeled White win
  TuningDataset datasetB;
  datasetB.getEntries().emplace_back(
    "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1", 1.0F);
  const double mseB = tuner.computeMSE(datasetB, 1.0);

  // Both should have small MSE (White is winning in both cases)
  EXPECT_LT(mseW, 0.05);
  EXPECT_LT(mseB, 0.05);
  // They should be similar (same position, just different side to move)
  EXPECT_NEAR(mseW, mseB, 0.02);
  std::cout << "  Perspective test — White to move MSE: " << mseW
            << ", Black to move MSE: " << mseB << "\n";
}

// =========================================================================
// K-tuning tests
// =========================================================================

TEST_F(TexelTunerTest, tuneK_ThrowsWithoutEvaluator) {
  TexelTuner tuner;
  TuningDataset dataset;
  dataset.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);

  EXPECT_THROW((void)tuner.tuneK(dataset), std::logic_error);
}

TEST_F(TexelTunerTest, tuneK_ConvergesInRange) {
  // K-tuning should converge to a value in [0.5, 2.0]
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  // Small dataset — enough for K-tuning to converge
  TuningDataset dataset;
  auto& entries = dataset.getEntries();
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 0.5F);
  entries.emplace_back("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  entries.emplace_back("rnbqkbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  entries.emplace_back("RNBQKBNR/PPPPPPPP/8/8/8/8/pppppppp/rnb1kbnr b kq - 0 1", 0.0F);

  const double K = tuner.tuneK(dataset);
  EXPECT_GE(K, 0.5);
  EXPECT_LE(K, 2.0);
  EXPECT_EQ(K, tuner.getK()); // Should be stored
  std::cout << "  Tuned K = " << K << "\n";
}

TEST_F(TexelTunerTest, tuneK_MSEDecreasesAtOptimal) {
  // The MSE at the tuned K should be <= MSE at the boundaries
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  auto& entries = dataset.getEntries();
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  entries.emplace_back("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", 0.0F);

  const double K = tuner.tuneK(dataset);
  const double mseOptimal = tuner.computeMSE(dataset, K);
  const double mseLow     = tuner.computeMSE(dataset, 0.5);
  const double mseHigh    = tuner.computeMSE(dataset, 2.0);

  EXPECT_LE(mseOptimal, mseLow + 1e-9);
  EXPECT_LE(mseOptimal, mseHigh + 1e-9);
  std::cout << "  MSE at K=" << K << ": " << mseOptimal
            << " (vs K=0.5: " << mseLow << ", K=2.0: " << mseHigh << ")\n";
}

// =========================================================================
// Dev dataset integration test (requires test data file)
// =========================================================================

TEST_F(TexelTunerTest, computeMSE_DevDataset) {
  const std::string devPath =
    std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/v1.6_vs_v1.5_score.txt";

  // Skip if file not available
  TuningDataset dataset;
  try {
    dataset.loadFromFile(devPath, 1000); // Load first 1000 for speed
  } catch (...) {
    GTEST_SKIP() << "Dev dataset not available at " << devPath;
  }

  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  const double mse = tuner.computeMSE(dataset, 1.0);
  // Should produce a valid MSE (between 0 and 1 for normalized results)
  EXPECT_GT(mse, 0.0);
  EXPECT_LT(mse, 1.0);
  std::cout << "  Dev dataset (first 1000) MSE at K=1.0: " << mse << "\n";
}

TEST_F(TexelTunerTest, tuneK_DevDataset) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string devPath =
    std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/v1.6_vs_v1.5_score.txt";

  TuningDataset dataset;
  try {
    dataset.loadFromFile(devPath, 1000);
  } catch (...) {
    GTEST_SKIP() << "Dev dataset not available at " << devPath;
  }

  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  const double K = tuner.tuneK(dataset);
  EXPECT_GE(K, 0.5);
  EXPECT_LE(K, 2.0);
  std::cout << "  Dev dataset (first 1000) tuned K = " << K << "\n";

  // MSE at optimal K should be reasonable
  const double mse = tuner.computeMSE(dataset, K);
  EXPECT_GT(mse, 0.0);
  EXPECT_LT(mse, 1.0);
  std::cout << "  Dev dataset MSE at optimal K: " << mse << "\n";
}

// =========================================================================
// Parallel MSE tests (Sprint 6.4)
// =========================================================================

TEST_F(TexelTunerTest, parallelMSE_MatchesSingleThread) {
  // Parallel MSE must match single-threaded MSE within tight FP tolerance
  TexelTuner::setupEvalOverrides();

  TexelTuner singleThreadTuner;
  singleThreadTuner.createEvaluator();

  TexelTuner parallelTuner;
  parallelTuner.createEvaluators(4);

  // Use hand-crafted dataset
  TuningDataset dataset;
  auto& entries = dataset.getEntries();
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 0.5F);
  entries.emplace_back("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  entries.emplace_back("rnbqkbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", 0.0F);
  entries.emplace_back("r1bqkbnr/pppppppp/2n5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", 0.5F);
  entries.emplace_back("rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", 0.5F);
  entries.emplace_back("rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2", 0.5F);

  const double mseSingle   = singleThreadTuner.computeMSE(dataset, 1.0);
  const double mseParallel = parallelTuner.computeMSEParallel(dataset, 1.0);

  std::cout << "  Single-threaded MSE: " << mseSingle << "\n";
  std::cout << "  Parallel MSE (4t):   " << mseParallel << "\n";

  // Tolerance: FP rounding from different summation order
  EXPECT_NEAR(mseSingle, mseParallel, 1e-10);
}

TEST_F(TexelTunerTest, parallelMSE_DeterministicAcrossRuns) {
  // Two consecutive calls must produce bit-identical results (sorted reduction)
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluators(4);

  TuningDataset dataset;
  auto& entries = dataset.getEntries();
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  entries.emplace_back("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", 0.0F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 0.5F);
  entries.emplace_back("r1bqkbnr/pppppppp/2n5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", 0.5F);

  const double mse1 = tuner.computeMSEParallel(dataset, 1.0);
  const double mse2 = tuner.computeMSEParallel(dataset, 1.0);

  EXPECT_DOUBLE_EQ(mse1, mse2);
  std::cout << "  Run 1 MSE: " << mse1 << "\n";
  std::cout << "  Run 2 MSE: " << mse2 << "\n";
}

TEST_F(TexelTunerTest, parallelMSE_SingleThread_MatchesSingleThread) {
  // computeMSEParallel with 1 thread should delegate to computeMSE
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator(); // single-threaded mode

  TuningDataset dataset;
  auto& entries = dataset.getEntries();
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  entries.emplace_back("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);

  const double mseSingle   = tuner.computeMSE(dataset, 1.0);
  const double mseParallel = tuner.computeMSEParallel(dataset, 1.0);

  EXPECT_DOUBLE_EQ(mseSingle, mseParallel);
}

TEST_F(TexelTunerTest, parallelMSE_EmptyDataset_ReturnsZero) {
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluators(4);

  const TuningDataset empty;
  EXPECT_DOUBLE_EQ(tuner.computeMSEParallel(empty, 1.0), 0.0);
}

TEST_F(TexelTunerTest, parallelMSE_ThrowsWithoutEvaluator) {
  const TexelTuner tuner;
  TuningDataset dataset;
  dataset.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 0.5F);

  EXPECT_THROW((void)tuner.computeMSEParallel(dataset, 1.0), std::logic_error);
}

TEST_F(TexelTunerTest, parallelMSE_DevDataset) {
  const std::string devPath =
    std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/v1.6_vs_v1.5_score.txt";

  TuningDataset dataset;
  try {
    dataset.loadFromFile(devPath, 2000); // 2000 entries for meaningful parallel test
  } catch (...) {
    GTEST_SKIP() << "Dev dataset not available at " << devPath;
  }

  TexelTuner::setupEvalOverrides();

  TexelTuner singleThreadTuner;
  singleThreadTuner.createEvaluator();

  TexelTuner parallelTuner;
  parallelTuner.createEvaluators(4);

  const double mseSingle   = singleThreadTuner.computeMSE(dataset, 1.0);
  const double mseParallel = parallelTuner.computeMSEParallel(dataset, 1.0);

  // Within tight FP tolerance
  EXPECT_NEAR(mseSingle, mseParallel, 1e-10);
  std::cout << "  Dev dataset (2000): single MSE = " << mseSingle
            << ", parallel MSE = " << mseParallel
            << ", diff = " << std::abs(mseSingle - mseParallel) << "\n";
}

// =========================================================================
// Coordinate descent tests (Sprint 6.4)
// =========================================================================

TEST_F(TexelTunerTest, coordinateDescent_ThrowsWithoutEvaluator) {
  TexelTuner tuner;
  TuningDataset trainSet;
  trainSet.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  std::vector<TuningParameter> params;

  EXPECT_THROW(tuner.tuneParameters(trainSet, nullptr, params), std::logic_error);
}

TEST_F(TexelTunerTest, coordinateDescent_EmptyTrainSet_NoOp) {
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  const TuningDataset empty;
  std::vector<TuningParameter> params;
  // Should return without throwing
  EXPECT_NO_THROW(tuner.tuneParameters(empty, nullptr, params));
}

TEST_F(TexelTunerTest, coordinateDescent_EmptyParams_NoOp) {
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset trainSet;
  trainSet.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);

  std::vector<TuningParameter> params;
  EXPECT_NO_THROW(tuner.tuneParameters(trainSet, nullptr, params));
}

TEST_F(TexelTunerTest, coordinateDescent_TerminatesOnNoImprovement) {
  // With a tiny dataset and few params, coordinate descent should converge quickly
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset trainSet;
  auto& entries = trainSet.getEntries();
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  entries.emplace_back("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", 0.0F);

  // Build full param vector, but limit to just 3 params for speed
  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto allParams = TuningParameter::buildFromRegistry(searchConfig, evalConfig);

  // Take only the first 3 params (enough to test convergence logic)
  std::vector params(
    allParams.begin(),
    allParams.begin() + std::min(static_cast<std::size_t>(3), allParams.size()));

  // Tune K first
  (void)tuner.tuneK(trainSet);

  // Run with high maxPasses — should terminate well before 50
  tuner.tuneParameters(trainSet, nullptr, params, 50);

  // If we got here without hanging, the termination logic works.
  // Params may or may not have changed (tiny dataset = noisy), but no crash.
  SUCCEED();
}

TEST_F(TexelTunerTest, coordinateDescent_ReducesMSE_DevDataset) {
  const std::string devPath =
    std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/v1.6_vs_v1.5_score.txt";

  TuningDataset fullDataset;
  try {
    fullDataset.loadFromFile(devPath, 5000); // 5000 positions for meaningful test
  } catch (...) {
    GTEST_SKIP() << "Dev dataset not available at " << devPath;
  }

  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluators(4);

  // Split into train/test
  auto [trainSet, testSet] = fullDataset.split(0.8F);

  // Build parameters
  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto params = TuningParameter::buildFromRegistry(searchConfig, evalConfig);

  std::cout << "  Parameters: " << params.size() << "\n";
  std::cout << "  Train set:  " << trainSet.size() << " positions\n";
  std::cout << "  Test set:   " << testSet.size() << " positions\n";

  // Tune K
  const double K = tuner.tuneK(trainSet);
  std::cout << "  Tuned K:    " << K << "\n";

  // Baseline MSE before tuning
  const double baselineMSE = tuner.computeMSEParallel(trainSet, K);
  std::cout << "  Baseline train MSE: " << baselineMSE << "\n";

  // Run 1 pass of coordinate descent
  tuner.tuneParameters(trainSet, &testSet, params, 1);

  // Final MSE after 1 pass
  const double finalMSE = tuner.computeMSEParallel(trainSet, K);
  std::cout << "  Final train MSE:    " << finalMSE << "\n";
  std::cout << "  Improvement:        " << (baselineMSE - finalMSE) << "\n";

  // MSE should not increase after optimization
  EXPECT_LE(finalMSE, baselineMSE + 1e-12);

  // Count how many params changed
  int changed = 0;
  for (const auto& p : params) {
    if (p.currentValue != p.originalValue) {
      ++changed;
    }
  }
  std::cout << "  Parameters changed: " << changed << "/" << params.size() << "\n";
}

TEST_F(TexelTunerTest, createEvaluators_ClampsThreadCount) {
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;

  // 0 threads should be clamped to 1
  tuner.createEvaluators(0);
  EXPECT_GE(tuner.numThreads(), 1);
  EXPECT_TRUE(tuner.hasEvaluator());

  // Negative threads should be clamped to 1
  tuner.createEvaluators(-5);
  EXPECT_GE(tuner.numThreads(), 1);

  // Valid thread count
  tuner.createEvaluators(2);
  EXPECT_EQ(tuner.numThreads(), 2);
}

// =========================================================================
// Activation flags tests (Sprint 6.5)
// =========================================================================

TEST_F(TexelTunerTest, activationFlags_StartPosition_AllActive) {
  // Start position has all piece types → almost all groups active
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  dataset.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);

  tuner.computeActivationFlags(dataset);

  const auto& flags = dataset[0].activeParamGroups;
  // All groups should be active for start position (has all piece types, 2 rooks, 2 bishops, etc.)
  for (int g = 0; g <= 12; ++g) {
    EXPECT_TRUE(flags.test(g)) << "Group " << g << " should be active for start position";
  }
}

TEST_F(TexelTunerTest, activationFlags_KingsOnly_MinimalGroups) {
  // Position with just kings — most groups should be inactive
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  dataset.getEntries().emplace_back(
    "4k3/8/8/8/8/8/8/4K3 w - - 0 1", 0.5F);

  tuner.computeActivationFlags(dataset);

  const auto& flags = dataset[0].activeParamGroups;
  // Active: group 0 (tempo), group 9 (king safety)
  EXPECT_TRUE(flags.test(0))  << "Group 0 (tempo) should be active";
  EXPECT_TRUE(flags.test(9))  << "Group 9 (king safety) should be active";

  // Inactive: pawn, knight, bishop, rook, queen, bishop pair, coordination, space, threats
  EXPECT_FALSE(flags.test(1))  << "Group 1 (pawn structure) should be inactive — no pawns";
  EXPECT_FALSE(flags.test(2))  << "Group 2 (passed pawns) should be inactive — no pawns";
  EXPECT_FALSE(flags.test(3))  << "Group 3 (pawn advance) should be inactive — no pawns";
  EXPECT_FALSE(flags.test(4))  << "Group 4 (bishop pair) should be inactive — no bishops";
  EXPECT_FALSE(flags.test(5))  << "Group 5 (knight) should be inactive — no knights";
  EXPECT_FALSE(flags.test(6))  << "Group 6 (bishop) should be inactive — no bishops";
  EXPECT_FALSE(flags.test(7))  << "Group 7 (rook) should be inactive — no rooks";
  EXPECT_FALSE(flags.test(8))  << "Group 8 (queen) should be inactive — no queens";
  EXPECT_FALSE(flags.test(10)) << "Group 10 (threats) should be inactive — no non-pawn pieces";
  EXPECT_FALSE(flags.test(11)) << "Group 11 (space) should be inactive — no pawns";
  EXPECT_FALSE(flags.test(12)) << "Group 12 (coordination) should be inactive — no rook/minor pairs";
}

TEST_F(TexelTunerTest, activationFlags_KnightsAndPawns_SpecificGroups) {
  // Position with just knights, pawns, and kings
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  dataset.getEntries().emplace_back(
    "4k3/pppppppp/8/8/8/8/PPPPPPPP/1N2K1N1 w - - 0 1", 0.5F);

  tuner.computeActivationFlags(dataset);

  const auto& flags = dataset[0].activeParamGroups;
  // Active: tempo(0), pawn groups(1,2,3), knight(5), king safety(9), threats(10), space(11)
  EXPECT_TRUE(flags.test(0));
  EXPECT_TRUE(flags.test(1));  // pawn structure
  EXPECT_TRUE(flags.test(5));  // knight
  EXPECT_TRUE(flags.test(9));  // king safety
  EXPECT_TRUE(flags.test(10)); // threats (has knights)
  EXPECT_TRUE(flags.test(11)); // space

  // Inactive: bishop pair(4), bishop(6), rook(7), queen(8)
  EXPECT_FALSE(flags.test(4)); // bishop pair
  EXPECT_FALSE(flags.test(6)); // bishop
  EXPECT_FALSE(flags.test(7)); // rook
  EXPECT_FALSE(flags.test(8)); // queen
}

TEST_F(TexelTunerTest, activationFlags_BishopPair) {
  // Position where White has 2 bishops → group 4 active
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  // White has 2 bishops
  dataset.getEntries().emplace_back(
    "4k3/8/8/8/8/8/8/2B1KB2 w - - 0 1", 0.5F);
  // White has 1 bishop
  dataset.getEntries().emplace_back(
    "4k3/8/8/8/8/8/8/4KB2 w - - 0 1", 0.5F);

  tuner.computeActivationFlags(dataset);

  EXPECT_TRUE(dataset[0].activeParamGroups.test(4))  << "2 bishops → bishop pair active";
  EXPECT_FALSE(dataset[1].activeParamGroups.test(4)) << "1 bishop → bishop pair inactive";
  // Both should have bishop group active
  EXPECT_TRUE(dataset[0].activeParamGroups.test(6));
  EXPECT_TRUE(dataset[1].activeParamGroups.test(6));
}

TEST_F(TexelTunerTest, activationFlags_Coordination) {
  // Test coordination group (12): needs 2+ rooks or 2+ minors
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  // White has 2 rooks → coordination active
  dataset.getEntries().emplace_back(
    "4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1", 0.5F);
  // White has knight + bishop (2 minors) → coordination active
  dataset.getEntries().emplace_back(
    "4k3/8/8/8/8/8/8/1NB1K3 w - - 0 1", 0.5F);
  // Only 1 rook, 0 minors → coordination inactive
  dataset.getEntries().emplace_back(
    "4k3/8/8/8/8/8/8/R3K3 w Q - 0 1", 0.5F);

  tuner.computeActivationFlags(dataset);

  EXPECT_TRUE(dataset[0].activeParamGroups.test(12)) << "2 rooks → coordination active";
  EXPECT_TRUE(dataset[1].activeParamGroups.test(12)) << "knight + bishop → coordination active";
  EXPECT_FALSE(dataset[2].activeParamGroups.test(12)) << "1 rook only → coordination inactive";
}

// =========================================================================
// Incremental MSE tests (Sprint 6.5)
// =========================================================================

TEST_F(TexelTunerTest, computeAndCacheErrors_PopulatesCache) {
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  dataset.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  dataset.getEntries().emplace_back(
    "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);

  const double mse = tuner.computeAndCacheErrors(dataset, 1.0);
  EXPECT_GT(mse, 0.0);

  // Each entry should have a nonzero cached error (we don't expect exact 0.5 match)
  for (std::size_t i = 0; i < dataset.size(); ++i) {
    EXPECT_GE(dataset[i].cachedSquaredError, 0.0) << "Entry " << i << " cached error should be >= 0";
  }

  // Verify MSE matches computeMSE
  const double mseFull = tuner.computeMSE(dataset, 1.0);
  EXPECT_NEAR(mse, mseFull, 1e-10);
}

TEST_F(TexelTunerTest, incrementalMSE_MatchesFullMSE_NoChange) {
  // When no parameter has changed, incremental MSE should match the cached value exactly
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  dataset.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  dataset.getEntries().emplace_back(
    "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  dataset.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 0.5F);

  tuner.computeActivationFlags(dataset);
  const double cachedMSE = tuner.computeAndCacheErrors(dataset, 1.0);

  // No param changed → incremental MSE for any group should match cached MSE
  for (int g = 0; g < 13; ++g) {
    const double incMSE = tuner.computeMSEIncremental(dataset, 1.0, g);
    EXPECT_NEAR(incMSE, cachedMSE, 1e-10) << "Group " << g << " incremental MSE should match cached";
  }
}

TEST_F(TexelTunerTest, incrementalMSE_MatchesFullMSE_AfterParamChange) {
  // After changing a parameter, incremental MSE should match full MSE
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluators(4);

  const std::string devPath =
    std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/v1.6_vs_v1.5_score.txt";

  TuningDataset dataset;
  try {
    dataset.loadFromFile(devPath, 1000);
  } catch (...) {
    GTEST_SKIP() << "Dev dataset not available at " << devPath;
  }

  tuner.computeActivationFlags(dataset);
  tuner.computeAndCacheErrors(dataset, 1.0);

  // Build params and modify one
  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto params = TuningParameter::buildFromRegistry(searchConfig, evalConfig);
  ASSERT_FALSE(params.empty());

  // Get mutable config
  SearchConfigData* pSearch = nullptr;
  EvalConfigData* pEval     = nullptr;
  ConfigManager::instance().applyOverrides([&](auto& s, auto& e) {
    pSearch = &s;
    pEval   = &e;
  });

  // Modify a parameter and compare incremental vs full MSE
  auto& param = params[0];
  const int origValue = param.currentValue;
  param.currentValue = origValue + 5;
  param.applyToConfig(*pSearch, *pEval);

  const double incMSE  = tuner.computeMSEIncremental(dataset, 1.0, param.paramGroup);
  const double fullMSE = tuner.computeMSEParallel(dataset, 1.0);

  std::cout << "  Incremental MSE: " << incMSE << "\n";
  std::cout << "  Full MSE:        " << fullMSE << "\n";
  std::cout << "  Diff:            " << std::abs(incMSE - fullMSE) << "\n";

  EXPECT_NEAR(incMSE, fullMSE, 1e-10);

  // Restore
  param.currentValue = origValue;
  param.applyToConfig(*pSearch, *pEval);
}

TEST_F(TexelTunerTest, updateCacheForGroup_KeepsCacheConsistent) {
  // After updateCacheForGroup, the cached MSE should match a fresh full eval
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  dataset.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  dataset.getEntries().emplace_back(
    "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  dataset.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 0.5F);

  tuner.computeActivationFlags(dataset);
  tuner.computeAndCacheErrors(dataset, 1.0);

  // Build params
  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto params = TuningParameter::buildFromRegistry(searchConfig, evalConfig);
  ASSERT_FALSE(params.empty());

  SearchConfigData* pSearch = nullptr;
  EvalConfigData* pEval     = nullptr;
  ConfigManager::instance().applyOverrides([&](auto& s, auto& e) {
    pSearch = &s;
    pEval   = &e;
  });

  // Modify param and update cache
  auto& param = params[0];
  param.currentValue += 3;
  param.applyToConfig(*pSearch, *pEval);
  tuner.updateCacheForGroup(dataset, 1.0, param.paramGroup);

  // Now the cached MSE should match a fresh full eval
  const double freshMSE = tuner.computeMSE(dataset, 1.0);
  const double cachedMSE = tuner.computeAndCacheErrors(dataset, 1.0);

  EXPECT_NEAR(freshMSE, cachedMSE, 1e-10);
}

TEST_F(TexelTunerTest, incrementalMSE_InactiveGroup_NoChange) {
  // For a group that's inactive for all entries, incremental MSE should
  // be identical to cached MSE regardless of parameter changes
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  // Kings-only position — most groups inactive
  TuningDataset dataset;
  dataset.getEntries().emplace_back("4k3/8/8/8/8/8/8/4K3 w - - 0 1", 0.5F);
  dataset.getEntries().emplace_back("4k3/8/8/8/8/8/8/4K3 b - - 0 1", 0.5F);

  tuner.computeActivationFlags(dataset);
  const double cachedMSE = tuner.computeAndCacheErrors(dataset, 1.0);

  // Knight group (5) is inactive — incremental MSE must equal cached even if we "change" something
  const double incMSE = tuner.computeMSEIncremental(dataset, 1.0, 5);
  EXPECT_DOUBLE_EQ(incMSE, cachedMSE);
}

TEST_F(TexelTunerTest, coordinateDescent_WithIncremental_ReducesMSE_DevDataset) {
  // Full integration test: coordinate descent with incremental MSE
  // should produce the same kind of improvement as the Sprint 6.4 test
  const std::string devPath =
    std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/v1.6_vs_v1.5_score.txt";

  TuningDataset fullDataset;
  try {
    fullDataset.loadFromFile(devPath, 5000);
  } catch (...) {
    GTEST_SKIP() << "Dev dataset not available at " << devPath;
  }

  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluators(4);

  auto [trainSet, testSet] = fullDataset.split(0.8F);

  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto params = TuningParameter::buildFromRegistry(searchConfig, evalConfig);

  const double K = tuner.tuneK(trainSet);
  std::cout << "  Tuned K:    " << K << "\n";

  // Baseline MSE
  const double baselineMSE = tuner.computeMSEParallel(trainSet, K);
  std::cout << "  Baseline train MSE: " << baselineMSE << "\n";

  // Run 1 pass (uses incremental MSE internally now)
  tuner.tuneParameters(trainSet, &testSet, params, 1);

  // Final MSE
  const double finalMSE = tuner.computeMSEParallel(trainSet, K);
  std::cout << "  Final train MSE:    " << finalMSE << "\n";
  std::cout << "  Improvement:        " << (baselineMSE - finalMSE) << "\n";

  EXPECT_LE(finalMSE, baselineMSE + 1e-12);

  int changed = 0;
  for (const auto& p : params) {
    if (p.currentValue != p.originalValue) ++changed;
  }
  std::cout << "  Parameters changed: " << changed << "/" << params.size() << "\n";
}

// =========================================================================
// Incremental MSE speedup measurement (Sprint 6.5)
// =========================================================================

TEST_F(TexelTunerTest, SpeedTests_incrementalMSE_Speedup_QuietLabeled) {
  if (isBulkRun()) {
    GTEST_SKIP() << "Skipping incremental MSE speed test in bulk runs";
  }
  // Measures the speedup of incremental MSE vs full MSE on the quiet-labeled
  // dataset (1.4M positions). This is a timing test — disabled by default.
  const std::string qlPath =
    std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/quiet-labeled.epd";

  TuningDataset dataset;
  try {
    dataset.loadFromFile(qlPath, 100000); // 100K positions — enough to see speedup
  } catch (...) {
    GTEST_SKIP() << "quiet-labeled.epd not available at " << qlPath;
  }

  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluators(4);

  // Build params
  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto params = TuningParameter::buildFromRegistry(searchConfig, evalConfig);
  ASSERT_FALSE(params.empty());

  // Get mutable config
  SearchConfigData* pSearch = nullptr;
  EvalConfigData* pEval     = nullptr;
  ConfigManager::instance().applyOverrides([&](auto& s, auto& e) {
    pSearch = &s;
    pEval   = &e;
  });

  // Compute activation flags and cache
  tuner.computeActivationFlags(dataset);

  // Count active entries per group
  std::cout << "\n  Activation flag statistics (" << dataset.size() << " entries):\n";
  for (int g = 0; g <= 12; ++g) {
    int count = 0;
    for (auto & i : dataset) {
      if (i.activeParamGroups.test(g)) ++count;
    }
    std::cout << "    Group " << std::setw(2) << g << ": " << std::setw(6) << count
              << "/" << dataset.size() << " ("
              << std::fixed << std::setprecision(1)
              << (100.0 * count / static_cast<double>(dataset.size())) << "%)\n";
  }

  tuner.computeAndCacheErrors(dataset, 1.0);

  // Pick a param from a group that's NOT always active (e.g., knight group 5)
  TuningParameter* knightParam = nullptr;
  for (auto& p : params) {
    if (p.paramGroup == 5) { knightParam = &p; break; }
  }
  ASSERT_NE(knightParam, nullptr) << "No knight param found";

  if (!knightParam) {
    GTEST_SKIP() << "No knight param found in registry";
  }

  // Measure full MSE speed
  constexpr int trials = 5;

  const auto startFull = steady_clock::now();
  for (int i = 0; i < trials; ++i) {
    knightParam->currentValue = knightParam->originalValue + i + 1;
    knightParam->applyToConfig(*pSearch, *pEval);
    (void)tuner.computeMSEParallel(dataset, 1.0);
  }
  const auto elapsedFull = steady_clock::now() - startFull;
  const double fullMs = std::chrono::duration<double, std::milli>(elapsedFull).count() / trials;

  // Restore
  knightParam->currentValue = knightParam->originalValue;
  knightParam->applyToConfig(*pSearch, *pEval);
  tuner.computeAndCacheErrors(dataset, 1.0); // reset cache

  // Measure incremental MSE speed
  const auto startInc = steady_clock::now();
  for (int i = 0; i < trials; ++i) {
    knightParam->currentValue = knightParam->originalValue + i + 1;
    knightParam->applyToConfig(*pSearch, *pEval);
    (void)tuner.computeMSEIncremental(dataset, 1.0, knightParam->paramGroup);
  }
  const auto elapsedInc = steady_clock::now() - startInc;
  const double incMs = std::chrono::duration<double, std::milli>(elapsedInc).count() / trials;

  // Restore
  knightParam->currentValue = knightParam->originalValue;
  knightParam->applyToConfig(*pSearch, *pEval);

  const double speedup = fullMs / incMs;

  std::cout << "\n  Speedup measurement (group 5 = knight, " << dataset.size() << " entries):\n";
  std::cout << "    Full MSE:        " << std::fixed << std::setprecision(1) << fullMs << " ms/eval\n";
  std::cout << "    Incremental MSE: " << std::fixed << std::setprecision(1) << incMs << " ms/eval\n";
  std::cout << "    Speedup:         " << std::fixed << std::setprecision(2) << speedup << "x\n";

  // Verify correctness while we're here
  knightParam->currentValue = knightParam->originalValue + 3;
  knightParam->applyToConfig(*pSearch, *pEval);
  const double mseInc  = tuner.computeMSEIncremental(dataset, 1.0, knightParam->paramGroup);
  const double mseFull = tuner.computeMSEParallel(dataset, 1.0);
  EXPECT_NEAR(mseInc, mseFull, 1e-9);
  std::cout << "    Correctness:     diff = " << std::scientific << std::abs(mseInc - mseFull) << "\n";

  // We expect at least some speedup for a non-universal group
  EXPECT_GT(speedup, 1.0) << "Incremental MSE should be faster than full for a non-universal group";
}

// =========================================================================
// Monotonicity constraint tests (Sprint 6.6)
// =========================================================================

TEST_F(TexelTunerTest, enforceMonotonicity_NonDecreasing_ClampsToFloor) {
  // Setting array[1] below array[0] should clamp to array[0]'s value
  std::vector<TuningParameter> params(3);
  for (int i = 0; i < 3; ++i) {
    params[i].name = "TEST_ARRAY[" + std::to_string(i) + "]";
    params[i].configName = "TEST_ARRAY";
    params[i].arrayIndex = i;
    params[i].arraySize = 3;
    params[i].monotonicity = MonotonicityConstraint::NON_DECREASING;
  }
  params[0].currentValue = 10;
  params[1].currentValue = 5; // violates: 5 < 10
  params[2].currentValue = 20;

  TexelTuner::enforceMonotonicity(params[1], params);

  EXPECT_EQ(params[1].currentValue, 10) << "Should be clamped up to predecessor's value";
  EXPECT_EQ(params[0].currentValue, 10) << "Predecessor should not change";
  EXPECT_EQ(params[2].currentValue, 20) << "Successor should not change";
}

TEST_F(TexelTunerTest, enforceMonotonicity_NonDecreasing_ClampsToCeiling) {
  // Setting array[1] above array[2] should clamp to array[2]'s value
  std::vector<TuningParameter> params(3);
  for (int i = 0; i < 3; ++i) {
    params[i].name = "TEST_ARRAY[" + std::to_string(i) + "]";
    params[i].configName = "TEST_ARRAY";
    params[i].arrayIndex = i;
    params[i].arraySize = 3;
    params[i].monotonicity = MonotonicityConstraint::NON_DECREASING;
  }
  params[0].currentValue = 10;
  params[1].currentValue = 25; // violates: 25 > 20 (successor)
  params[2].currentValue = 20;

  TexelTuner::enforceMonotonicity(params[1], params);

  EXPECT_EQ(params[1].currentValue, 20) << "Should be clamped down to successor's value";
}

TEST_F(TexelTunerTest, enforceMonotonicity_NonDecreasing_ValidValue_NoChange) {
  // A valid value (between predecessor and successor) should not be modified
  std::vector<TuningParameter> params(3);
  for (int i = 0; i < 3; ++i) {
    params[i].name = "TEST_ARRAY[" + std::to_string(i) + "]";
    params[i].configName = "TEST_ARRAY";
    params[i].arrayIndex = i;
    params[i].arraySize = 3;
    params[i].monotonicity = MonotonicityConstraint::NON_DECREASING;
  }
  params[0].currentValue = 10;
  params[1].currentValue = 15; // valid: 10 <= 15 <= 20
  params[2].currentValue = 20;

  TexelTuner::enforceMonotonicity(params[1], params);

  EXPECT_EQ(params[1].currentValue, 15) << "Valid value should not change";
}

TEST_F(TexelTunerTest, enforceMonotonicity_NonIncreasing_ClampsToFloor) {
  // Non-increasing: array[i] <= array[i-1], array[i] >= array[i+1]
  // Setting array[1] above array[0] should clamp down
  std::vector<TuningParameter> params(3);
  for (int i = 0; i < 3; ++i) {
    params[i].name = "TEST_ARRAY[" + std::to_string(i) + "]";
    params[i].configName = "TEST_ARRAY";
    params[i].arrayIndex = i;
    params[i].arraySize = 3;
    params[i].monotonicity = MonotonicityConstraint::NON_INCREASING;
  }
  params[0].currentValue = 20;
  params[1].currentValue = 25; // violates: 25 > 20 (predecessor)
  params[2].currentValue = 5;

  TexelTuner::enforceMonotonicity(params[1], params);

  EXPECT_EQ(params[1].currentValue, 20) << "Should be clamped down to predecessor's value";
}

TEST_F(TexelTunerTest, enforceMonotonicity_NonIncreasing_ClampsUpToSuccessor) {
  // Non-increasing: setting array[1] below array[2] should clamp up
  std::vector<TuningParameter> params(3);
  for (int i = 0; i < 3; ++i) {
    params[i].name = "TEST_ARRAY[" + std::to_string(i) + "]";
    params[i].configName = "TEST_ARRAY";
    params[i].arrayIndex = i;
    params[i].arraySize = 3;
    params[i].monotonicity = MonotonicityConstraint::NON_INCREASING;
  }
  params[0].currentValue = 20;
  params[1].currentValue = 3; // violates: 3 < 5 (successor)
  params[2].currentValue = 5;

  TexelTuner::enforceMonotonicity(params[1], params);

  EXPECT_EQ(params[1].currentValue, 5) << "Should be clamped up to successor's value";
}

TEST_F(TexelTunerTest, enforceMonotonicity_Scalar_NoOp) {
  // Scalar parameters (arrayIndex < 0) should not be modified
  std::vector<TuningParameter> params(1);
  params[0].name = "SOME_SCALAR";
  params[0].configName = "SOME_SCALAR";
  params[0].arrayIndex = -1;
  params[0].currentValue = 42;
  params[0].monotonicity = MonotonicityConstraint::NON_DECREASING; // even if set, should be no-op

  TexelTuner::enforceMonotonicity(params[0], params);

  EXPECT_EQ(params[0].currentValue, 42) << "Scalar should not be modified";
}

TEST_F(TexelTunerTest, enforceMonotonicity_NoConstraint_NoOp) {
  // Array element with NONE constraint should not be modified
  std::vector<TuningParameter> params(3);
  for (int i = 0; i < 3; ++i) {
    params[i].name = "TEST_ARRAY[" + std::to_string(i) + "]";
    params[i].configName = "TEST_ARRAY";
    params[i].arrayIndex = i;
    params[i].arraySize = 3;
    params[i].monotonicity = MonotonicityConstraint::NONE;
  }
  params[0].currentValue = 10;
  params[1].currentValue = 5; // would violate NON_DECREASING, but constraint is NONE
  params[2].currentValue = 20;

  TexelTuner::enforceMonotonicity(params[1], params);

  EXPECT_EQ(params[1].currentValue, 5) << "No constraint → no modification";
}

TEST_F(TexelTunerTest, enforceMonotonicity_FirstElement_NoPredecessor) {
  // First array element has no predecessor — only successor bound applies
  std::vector<TuningParameter> params(3);
  for (int i = 0; i < 3; ++i) {
    params[i].name = "TEST_ARRAY[" + std::to_string(i) + "]";
    params[i].configName = "TEST_ARRAY";
    params[i].arrayIndex = i;
    params[i].arraySize = 3;
    params[i].monotonicity = MonotonicityConstraint::NON_DECREASING;
  }
  params[0].currentValue = 50; // exceeds successor
  params[1].currentValue = 20;
  params[2].currentValue = 30;

  TexelTuner::enforceMonotonicity(params[0], params);

  EXPECT_EQ(params[0].currentValue, 20) << "First element clamped to successor's value";
}

TEST_F(TexelTunerTest, enforceMonotonicity_LastElement_NoSuccessor) {
  // Last array element has no successor — only predecessor bound applies
  std::vector<TuningParameter> params(3);
  for (int i = 0; i < 3; ++i) {
    params[i].name = "TEST_ARRAY[" + std::to_string(i) + "]";
    params[i].configName = "TEST_ARRAY";
    params[i].arrayIndex = i;
    params[i].arraySize = 3;
    params[i].monotonicity = MonotonicityConstraint::NON_DECREASING;
  }
  params[0].currentValue = 10;
  params[1].currentValue = 20;
  params[2].currentValue = 5; // violates: 5 < 20 (predecessor)

  TexelTuner::enforceMonotonicity(params[2], params);

  EXPECT_EQ(params[2].currentValue, 20) << "Last element clamped to predecessor's value";
}

TEST_F(TexelTunerTest, enforceMonotonicity_RealArrayParams_FromRegistry) {
  // Verify that array params from the real registry have monotonicity set,
  // and that enforceMonotonicity works on them
  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto params = TuningParameter::buildFromRegistry(searchConfig, evalConfig);

  // Find KING_SAFETY_TABLE elements
  std::vector<TuningParameter*> kst;
  for (auto& p : params) {
    if (p.configName == "KING_SAFETY_TABLE") {
      kst.push_back(&p);
    }
  }
  ASSERT_GE(kst.size(), 3u) << "Expected at least 3 KING_SAFETY_TABLE elements";

  // All should be NON_DECREASING
  for (const auto* p : kst) {
    EXPECT_EQ(p->monotonicity, MonotonicityConstraint::NON_DECREASING)
      << "KING_SAFETY_TABLE[" << p->arrayIndex << "] should be non-decreasing";
  }

  // Verify the original values are already monotonic
  for (std::size_t i = 1; i < kst.size(); ++i) {
    EXPECT_GE(kst[i]->currentValue, kst[i - 1]->currentValue)
      << "KING_SAFETY_TABLE[" << kst[i]->arrayIndex << "] = " << kst[i]->currentValue
      << " should be >= [" << kst[i - 1]->arrayIndex << "] = " << kst[i - 1]->currentValue;
  }

  // Set middle element below its predecessor and enforce
  const int origValue = kst[1]->currentValue;
  kst[1]->currentValue = kst[0]->currentValue - 10;

  TexelTuner::enforceMonotonicity(*kst[1], params);

  EXPECT_GE(kst[1]->currentValue, kst[0]->currentValue)
    << "After enforcement, [1] should be >= [0]";

  // Restore
  kst[1]->currentValue = origValue;
}

// =========================================================================
// Phase 6.10 — Additional edge case tests
// =========================================================================

TEST_F(TexelTunerTest, setupEvalOverrides_DisablesLazyEvalAndPawnTT) {
  // Verify that setupEvalOverrides actually changes the config state
  ConfigManager::instance().resetToDefaults();

  // Before: defaults may or may not have these set
  const bool lazyBefore  = ConfigManager::instance().eval().USE_LAZY_EVAL;
  const bool pawnTTBefore = ConfigManager::instance().eval().USE_PAWN_TT;

  // The defaults should have these enabled
  EXPECT_TRUE(lazyBefore) << "Default USE_LAZY_EVAL should be true";
  EXPECT_TRUE(pawnTTBefore) << "Default USE_PAWN_TT should be true";

  TexelTuner::setupEvalOverrides();

  // After: lazy eval and pawn TT must be disabled
  EXPECT_FALSE(ConfigManager::instance().eval().USE_LAZY_EVAL)
    << "setupEvalOverrides must disable USE_LAZY_EVAL";
  EXPECT_FALSE(ConfigManager::instance().eval().USE_PAWN_TT)
    << "setupEvalOverrides must disable USE_PAWN_TT";

  // Space eval was fully removed in Phase 9 — no USE_SPACE_EVAL field exists.

  // Connected rooks and minor connectivity remain enabled (defaults true)
  EXPECT_TRUE(ConfigManager::instance().eval().USE_CONNECTED_ROOKS)
    << "USE_CONNECTED_ROOKS must be true (default)";
  EXPECT_TRUE(ConfigManager::instance().eval().USE_MINOR_CONNECTIVITY)
    << "USE_MINOR_CONNECTIVITY must be true (default)";
}

TEST_F(TexelTunerTest, setK_GetK_RoundTrip) {
  TexelTuner tuner;
  EXPECT_DOUBLE_EQ(tuner.getK(), 1.0); // default

  tuner.setK(0.523);
  EXPECT_DOUBLE_EQ(tuner.getK(), 0.523);

  tuner.setK(1.987);
  EXPECT_DOUBLE_EQ(tuner.getK(), 1.987);
}

TEST_F(TexelTunerTest, computeMSE_PerfectPredictions_ZeroMSE) {
  // If every position's eval exactly produces the labeled result via sigmoid,
  // MSE should be (nearly) zero. We approximate this by using positions where
  // the eval strongly agrees with the label and a large K to make sigmoid extreme.
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  // Start position is ~0cp eval, draw label → sigmoid(K, 0) = 0.5 = draw
  dataset.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);

  // With any K, eval=0 → sigmoid = 0.5 exactly, result = 0.5 → error = 0
  const double mse = tuner.computeMSE(dataset, 1.0);
  // Eval may not be exactly 0 (tempo bonus), so allow small tolerance
  EXPECT_LT(mse, 0.005) << "Start position draw should produce near-zero MSE";
}

TEST_F(TexelTunerTest, computeMSE_AllDraws_ReasonableMSE) {
  // Dataset where every position is labeled as draw (0.5)
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  auto& entries = dataset.getEntries();
  // Mix of various positions, all labeled as draws
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 0.5F);
  entries.emplace_back("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 0.5F);

  const double mse = tuner.computeMSE(dataset, 1.0);
  EXPECT_GT(mse, 0.0);
  EXPECT_LT(mse, 1.0);
  std::cout << "  All-draws dataset MSE: " << mse << "\n";
}

TEST_F(TexelTunerTest, computeMSE_AllWhiteWins_ReasonableMSE) {
  // Dataset where every position is labeled as white win (1.0)
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset dataset;
  auto& entries = dataset.getEntries();
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1.0F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 1.0F);
  entries.emplace_back("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);

  const double mse = tuner.computeMSE(dataset, 1.0);
  EXPECT_GT(mse, 0.0);
  EXPECT_LT(mse, 1.0);
  std::cout << "  All-white-wins dataset MSE: " << mse << "\n";
}

TEST_F(TexelTunerTest, computeMSE_EvalPerspective_KnownMaterialAdvantage) {
  // Test eval perspective with a known material advantage:
  // White has an extra knight. Regardless of side to move, eval should
  // favor White (positive from White's perspective), and with a white-win
  // label the MSE should be small.
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  // White has an extra knight, White to move
  TuningDataset datasetWtm;
  datasetWtm.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 0 1", 1.0F);

  // White has an extra knight, Black to move (same position)
  TuningDataset datasetBtm;
  datasetBtm.getEntries().emplace_back(
    "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 1", 1.0F);

  const double mseW = tuner.computeMSE(datasetWtm, 1.0);
  const double mseB = tuner.computeMSE(datasetBtm, 1.0);

  // Both should be relatively small (eval agrees with label)
  EXPECT_LT(mseW, 0.25);
  EXPECT_LT(mseB, 0.25);
  // And should be similar to each other (same position, different side to move)
  EXPECT_NEAR(mseW, mseB, 0.05);

  std::cout << "  Extra knight perspective: WTM MSE=" << mseW << ", BTM MSE=" << mseB << "\n";
}

TEST_F(TexelTunerTest, coordinateDescent_SingleParameter_Converges) {
  // Coordinate descent with only 1 parameter should converge without issues
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluator();

  TuningDataset trainSet;
  auto& entries = trainSet.getEntries();
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  entries.emplace_back("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", 0.0F);
  entries.emplace_back("r1bqkbnr/pppppppp/2n5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", 0.5F);

  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto allParams = TuningParameter::buildFromRegistry(searchConfig, evalConfig);

  // Take exactly 1 parameter
  std::vector<TuningParameter> singleParam = {allParams[0]};

  (void)tuner.tuneK(trainSet);

  const double baselineMSE = tuner.computeMSE(trainSet, tuner.getK());

  // Run coordinate descent with just 1 param, max 10 passes
  tuner.tuneParameters(trainSet, nullptr, singleParam, 10);

  const double finalMSE = tuner.computeMSE(trainSet, tuner.getK());

  // MSE should not increase
  EXPECT_LE(finalMSE, baselineMSE + 1e-12);
  std::cout << "  Single-param: baseline MSE=" << baselineMSE
            << ", final MSE=" << finalMSE << "\n";
}

TEST_F(TexelTunerTest, tuneK_ParallelMatchesSingleThread) {
  // tuneK should use parallel MSE when available, producing the same K
  TexelTuner::setupEvalOverrides();

  TexelTuner singleTuner;
  singleTuner.createEvaluator();

  TexelTuner parallelTuner;
  parallelTuner.createEvaluators(4);

  TuningDataset dataset;
  auto& entries = dataset.getEntries();
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  entries.emplace_back("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", 0.0F);
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 0.5F);
  entries.emplace_back("r1bqkbnr/pppppppp/2n5/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2", 0.5F);

  const double kSingle   = singleTuner.tuneK(dataset);
  const double kParallel = parallelTuner.tuneK(dataset);

  EXPECT_NEAR(kSingle, kParallel, 1e-6)
    << "Parallel and single-threaded K-tuning should converge to same value";
  std::cout << "  K single=" << kSingle << ", K parallel=" << kParallel << "\n";
}

TEST_F(TexelTunerTest, hasEvaluator_BeforeAndAfterCreate) {
  TexelTuner tuner;
  EXPECT_FALSE(tuner.hasEvaluator());
  EXPECT_EQ(tuner.numThreads(), 0);

  TexelTuner::setupEvalOverrides();
  tuner.createEvaluator();
  EXPECT_TRUE(tuner.hasEvaluator());
  EXPECT_EQ(tuner.numThreads(), 1);
}

TEST_F(TexelTunerTest, coordinateDescent_WithMonotonicity_MaintainsOrdering) {
  // After coordinate descent, all array params with monotonicity constraints
  // should still be in valid order
  const std::string devPath =
    std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/v1.6_vs_v1.5_score.txt";

  TuningDataset dataset;
  try {
    dataset.loadFromFile(devPath, 2000);
  } catch (...) {
    GTEST_SKIP() << "Dev dataset not available at " << devPath;
  }

  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluators(4);

  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto params = TuningParameter::buildFromRegistry(searchConfig, evalConfig);

  (void)tuner.tuneK(dataset);
  tuner.tuneParameters(dataset, nullptr, params, 2); // 2 passes

  // Check all NON_DECREASING arrays maintain ordering
  // Group elements by configName
  std::unordered_map<std::string, std::vector<const TuningParameter*>> arrays;
  for (const auto& p : params) {
    if (p.arrayIndex >= 0 && p.monotonicity == MonotonicityConstraint::NON_DECREASING) {
      arrays[p.configName].push_back(&p);
    }
  }

  for (const auto& [arrayName, elements] : arrays) {
    // Sort by arrayIndex (should already be sorted, but be defensive)
    auto sorted = elements;
    std::ranges::sort(sorted, [](const auto* a, const auto* b) { return a->arrayIndex < b->arrayIndex; });

    for (std::size_t i = 1; i < sorted.size(); ++i) {
      EXPECT_GE(sorted[i]->currentValue, sorted[i - 1]->currentValue)
        << arrayName << "[" << sorted[i]->arrayIndex << "] = " << sorted[i]->currentValue
        << " should be >= [" << sorted[i - 1]->arrayIndex << "] = " << sorted[i - 1]->currentValue;
    }
  }

  std::cout << "  Verified monotonicity on " << arrays.size() << " array parameter groups\n";
}

// =========================================================================
// Phase 6.10 — End-to-end integration test
// =========================================================================

TEST_F(TexelTunerTest, endToEnd_SmallDataset_TuneAndGenerateOutput) {
  // Full pipeline: dataset → tuneK → coordinate descent → checkpoint → output files
  // Verifies that MSE decreases and output files are valid.
  TexelTuner::setupEvalOverrides();
  TexelTuner tuner;
  tuner.createEvaluators(4);

  // Build a small but meaningful dataset (mixed positions and results)
  TuningDataset fullDataset;
  auto& entries = fullDataset.getEntries();

  // Start position (draw)
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0.5F);
  // After 1. e4 (draw)
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", 0.5F);
  // Italian game (white win)
  entries.emplace_back("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4", 1.0F);
  // Sicilian (draw)
  entries.emplace_back("rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2", 0.5F);
  // White up a rook (white win)
  entries.emplace_back("rnbqkbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1", 1.0F);
  // Black up a queen (black win)
  entries.emplace_back("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1", 0.0F);
  // Endgame (draw)
  entries.emplace_back("4k3/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1", 0.5F);
  // Sicilian Najdorf position (black win)
  entries.emplace_back("r1bqkb1r/1p1n1ppp/p2ppn2/8/3NP3/2N1BP2/PPP3PP/R2QKB1R w KQkq - 0 8", 0.0F);
  // French Defense (draw)
  entries.emplace_back("rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", 0.5F);
  // White up a piece (white win)
  entries.emplace_back("r1bqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1.0F);

  // Also load from dev dataset if available
  const std::string devPath =
    std::string(FrankyCPP_PROJECT_ROOT) + "/test/testsets/tuning/v1.6_vs_v1.5_score.txt";
  try {
    TuningDataset devData;
    devData.loadFromFile(devPath, 90); // Small subset — 90 more entries
    for (const auto& e : devData) {
      entries.push_back(e);
    }
  } catch (...) {
    // OK, continue with just the hand-crafted positions
  }

  ASSERT_GE(fullDataset.size(), 10u) << "Need at least the hand-crafted entries";

  // Split into train/test
  auto [trainSet, testSet] = fullDataset.split(0.8F);
  std::cout << "  Dataset: " << fullDataset.size() << " total"
            << " (train=" << trainSet.size() << ", test=" << testSet.size() << ")\n";

  // Build parameters — use only first 5 for speed
  const auto& searchConfig = ConfigManager::instance().search();
  const auto& evalConfig   = ConfigManager::instance().eval();
  auto allParams = TuningParameter::buildFromRegistry(searchConfig, evalConfig);
  std::vector<TuningParameter> params(
    allParams.begin(),
    allParams.begin() + std::min(static_cast<std::size_t>(5), allParams.size()));

  // Step 1: Tune K
  const double K = tuner.tuneK(trainSet);
  EXPECT_GE(K, 0.5);
  EXPECT_LE(K, 2.0);
  std::cout << "  Tuned K: " << K << "\n";

  // Step 2: Baseline MSE
  const double baselineMSE = tuner.computeMSEParallel(trainSet, K);
  std::cout << "  Baseline train MSE: " << baselineMSE << "\n";
  EXPECT_GT(baselineMSE, 0.0);
  EXPECT_LT(baselineMSE, 1.0);

  // Step 3: Coordinate descent (2 passes)
  const std::string checkpointPath =
    (std::filesystem::temp_directory_path() / "e2e_checkpoint.yaml").string();
  tuner.tuneParameters(trainSet, &testSet, params, 2, checkpointPath, "e2e_test");

  // Step 4: Verify MSE did not increase
  const double finalMSE = tuner.computeMSEParallel(trainSet, K);
  std::cout << "  Final train MSE: " << finalMSE << "\n";
  EXPECT_LE(finalMSE, baselineMSE + 1e-12)
    << "MSE should not increase after tuning";

  // Step 5: Verify checkpoint was created
  EXPECT_TRUE(std::filesystem::exists(checkpointPath))
    << "Checkpoint file should exist";
  const auto loaded = TuningState::loadFromYaml(checkpointPath);
  EXPECT_GE(loaded.completedPasses, 1);
  EXPECT_EQ(loaded.paramValues.size(), params.size());

  // Step 6: Generate output files
  const std::string yamlPath =
    (std::filesystem::temp_directory_path() / "e2e_tuned.yaml").string();
  const std::string reportPath =
    (std::filesystem::temp_directory_path() / "e2e_comparison.txt").string();

  TuningOutput::writeParamsYaml(yamlPath, params, K);
  TuningOutput::writeComparisonReport(reportPath, params);

  EXPECT_TRUE(std::filesystem::exists(yamlPath))
    << "Tuned params YAML should exist";
  EXPECT_TRUE(std::filesystem::exists(reportPath))
    << "Comparison report should exist";

  // Step 7: Verify output files are non-empty and valid
  {
    std::ifstream yamlFile(yamlPath);
    const std::string yamlContent(
      (std::istreambuf_iterator<char>(yamlFile)),
      std::istreambuf_iterator<char>());
    EXPECT_GT(yamlContent.size(), 50u) << "YAML should have content";
    EXPECT_NE(yamlContent.find("FrankyCPP Tuned"), std::string::npos);
  }

  {
    std::ifstream reportFile(reportPath);
    const std::string reportContent(
      (std::istreambuf_iterator<char>(reportFile)),
      std::istreambuf_iterator<char>());
    EXPECT_GT(reportContent.size(), 50u) << "Report should have content";
    EXPECT_NE(reportContent.find("Parameter Comparison"), std::string::npos);
  }

  // Cleanup
  std::filesystem::remove(checkpointPath);
  std::filesystem::remove(yamlPath);
  std::filesystem::remove(reportPath);

  std::cout << "  ✅ End-to-end integration test passed\n";
}
