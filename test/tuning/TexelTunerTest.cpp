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

#include "tuning/optimizer/TuningDataset.h"
#include "tuning/optimizer/TuningParameter.h"
#include "config/ConfigManager.h"
#include "init.h"
#include "types/macros.h"

#include <gtest/gtest.h>
#include <iostream>
#include <string>

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
