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

//=============================================================================
// TunerMain.cpp - Texel Tuner Main Entry Point
//=============================================================================
//
// Standalone executable for Texel tuning of evaluation parameters.
// Reads a dataset of labeled positions (FEN + game result), optimizes eval
// weights via coordinate descent to minimize mean squared error between
// predicted and actual game outcomes.
//
// Pipeline:
//   1. Load dataset (FEN+result format or EPD c9 format)
//   2. Split into train/test sets (configurable split ratio)
//   3. Apply eval overrides (disable lazy eval, pawn TT)
//   4. Create evaluator pool (one per worker thread)
//   5. Build tunable parameter vector from ConfigRegistry
//   6. Resume from checkpoint if --resume is specified
//   7. Tune scaling constant K via ternary search (unless resuming)
//   8. Run coordinate descent optimization (with checkpointing)
//   9. Print final summary with top parameter changes
//
// Usage:
//   FrankyCPP_Tuner --dataset <file> [options]
//   FrankyCPP_Tuner --dataset <file> --resume checkpoint.yaml
//
// See docs/specs/PLAN_Texel_Tuning.md for details.
//
//=============================================================================

#include "tuning/optimizer/TexelTuner.h"
#include "tuning/optimizer/TuningDataset.h"
#include "tuning/optimizer/TuningParameter.h"
#include "tuning/optimizer/TuningState.h"

#include "common/Logging.h"
#include "config/ConfigManager.h"
#include "init.h"
#include "version.h"

#include <algorithm>
#include <boost/program_options.hpp>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <vector>

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
  try {
    // Initialize FrankyCPP (attack tables, Zobrist keys, etc.)
    init::init();

    // Build version string
    const std::string appName = std::format(
      "FrankyCPP Texel Tuner v{}.{}.{}",
      FrankyCPP_VERSION_MAJOR, FrankyCPP_VERSION_MINOR, FrankyCPP_VERSION_PATCH);

    // =========================================================================
    // Command-line options
    // =========================================================================
    po::options_description desc(appName + "\n\nOptimizes eval parameters via Texel's tuning method.\n\nOptions");
    // clang-format off
    desc.add_options()
      ("help,h",              "Show this help message")
      ("dataset,d",           po::value<std::string>()->required(),
                              "Input dataset file (FEN + result per line)")
      ("output,o",            po::value<std::string>()->default_value("tuned_params.yaml"),
                              "Output YAML file for tuned parameters (default: tuned_params.yaml)")
      ("threads,t",           po::value<int>()->default_value(4),
                              "Worker threads for parallel MSE computation (default: 4)")
      ("test-split",          po::value<double>()->default_value(0.2, "0.2"),
                              "Fraction of data held out for test set (default: 0.2)")
      ("resume,r",            po::value<std::string>(),
                              "Resume from checkpoint YAML file")
      ("max-passes",          po::value<int>()->default_value(100),
                              "Maximum optimization passes (default: 100)")
      ("verbose,v",           po::bool_switch()->default_value(false),
                              "Enable verbose output");
    // clang-format on

    // Allow positional argument: dataset
    po::positional_options_description positional;
    positional.add("dataset", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(positional).run(), vm);

    // Show help before notify() so --help works without required args
    if (vm.contains("help")) {
      std::cout << desc << "\n";
      std::cout << "Examples:\n";
      std::cout << "  FrankyCPP_Tuner --dataset positions.txt\n";
      std::cout << "  FrankyCPP_Tuner positions.txt --threads 8 --output tuned.yaml\n";
      std::cout << "  FrankyCPP_Tuner -d data.txt --test-split 0.1 --max-passes 50\n";
      std::cout << "  FrankyCPP_Tuner -d data.txt --resume checkpoint_pass5.yaml\n";
      std::cout << "\n";
      std::cout << "Dataset Format:\n";
      std::cout << "  One position per line: <FEN> [<result>]\n";
      std::cout << "  Result: 1.0 (white win), 0.5 (draw), 0.0 (black win)\n";
      std::cout << "\n";
      std::cout << "Tuning Process:\n";
      std::cout << "  1. Load dataset, split into train/test sets\n";
      std::cout << "  2. Tune scaling constant K via ternary search\n";
      std::cout << "  3. Optimize parameters via coordinate descent\n";
      std::cout << "  4. Save tuned parameters and comparison report\n";
      std::cout << "\n";
      std::cout << "Output Files:\n";
      std::cout << "  <output>.yaml          Tuned parameters (loadable by ConfigManager)\n";
      std::cout << "  <output>_checkpoint.yaml  Checkpoint after each pass (for resume)\n";
      std::cout << "  <output>_comparison.txt   Side-by-side original vs tuned values\n";
      return 0;
    }

    // Validate required arguments
    po::notify(vm);

    // =========================================================================
    // Display configuration
    // =========================================================================
    std::cout << appName << "\n";
    std::cout << std::string(appName.size(), '=') << "\n\n";

    const auto& datasetPath = vm["dataset"].as<std::string>();
    const auto& outputPath  = vm["output"].as<std::string>();
    const int threads       = vm["threads"].as<int>();
    const double testSplit  = vm["test-split"].as<double>();
    const int maxPasses     = vm["max-passes"].as<int>();
    const bool verbose      = vm["verbose"].as<bool>();
    const bool hasResume    = vm.contains("resume");
    const std::string resumePath = hasResume ? vm["resume"].as<std::string>() : "";

    std::cout << "Configuration:\n";
    std::cout << "  Dataset:           " << datasetPath << "\n";
    std::cout << "  Output:            " << outputPath << "\n";
    std::cout << "  Threads:           " << threads << "\n";
    std::cout << "  Test split:        " << testSplit << "\n";
    std::cout << "  Max passes:        " << maxPasses << "\n";
    std::cout << "  Verbose:           " << (verbose ? "yes" : "no") << "\n";
    if (hasResume) {
      std::cout << "  Resume from:       " << resumePath << "\n";
    }
    std::cout << "\n";

    // =========================================================================
    // Initialize logging
    // =========================================================================
    const auto& tuningLog = common::Logger::get().TUNING_LOG;
    if (verbose) {
      common::Logger::setLoggerLevel(tuningLog, spdlog::level::debug);
    }
    else {
      common::Logger::setLoggerLevel(tuningLog, spdlog::level::info);
    }

    const auto totalStart = steady_clock::now();

    // =========================================================================
    // Step 1: Load dataset
    // =========================================================================
    std::cout << "Loading dataset: " << datasetPath << "\n" << std::flush;
    tuning::TuningDataset fullDataset;
    fullDataset.loadFromFile(datasetPath);
    if (fullDataset.empty()) {
      std::cerr << "Error: Dataset is empty or could not be loaded.\n";
      return 1;
    }
    const auto& stats = fullDataset.getLoadStats();
    std::cout << "  Loaded " << fullDataset.size() << " positions"
              << " (parsed: " << stats.parsedOk
              << ", skipped: " << (stats.skippedEmpty + stats.skippedComment + stats.skippedMalformed) << ")\n\n";

    // =========================================================================
    // Step 2: Split into train/test sets
    // =========================================================================
    const auto trainFraction = static_cast<float>(1.0 - testSplit);
    auto [trainSet, testSet] = fullDataset.split(trainFraction);
    std::cout << "Train/test split:\n";
    std::cout << "  Train set: " << trainSet.size() << " positions ("
              << std::format("{:.0f}", trainFraction * 100.0F) << "%)\n";
    std::cout << "  Test set:  " << testSet.size() << " positions ("
              << std::format("{:.0f}", testSplit * 100.0) << "%)\n\n";

    // =========================================================================
    // Step 3: Apply eval overrides for tuning
    // =========================================================================
    std::cout << "Applying eval overrides for tuning...\n";
    tuning::TexelTuner::setupEvalOverrides();
    std::cout << "  Lazy eval:   disabled\n";
    std::cout << "  Pawn TT:     disabled\n";
    std::cout << "  Space eval:  enabled\n\n";

    // =========================================================================
    // Step 4: Create evaluators (thread pool)
    // =========================================================================
    std::cout << "Creating " << threads << " evaluator(s)...\n\n";
    tuning::TexelTuner tuner;
    tuner.createEvaluators(threads);

    // =========================================================================
    // Step 5: Build tunable parameter vector from registry
    // =========================================================================
    const auto& searchCfg = config::ConfigManager::instance().search();
    const auto& evalCfg   = config::ConfigManager::instance().eval();
    auto params = tuning::TuningParameter::buildFromRegistry(searchCfg, evalCfg);
    std::cout << "Tunable parameters: " << params.size() << "\n\n";

    // =========================================================================
    // Step 6: Handle --resume (restore checkpoint state)
    // =========================================================================
    int startPass = 0;
    if (hasResume) {
      std::cout << "Resuming from checkpoint: " << resumePath << "\n";
      const auto checkpoint = tuning::TuningState::loadFromYaml(resumePath);
      const int restored = checkpoint.restoreToParams(params);
      std::cout << "  Restored " << restored << "/" << params.size() << " parameter values\n";
      std::cout << "  Completed passes: " << checkpoint.completedPasses << "\n";
      std::cout << "  Previous train MSE: " << std::format("{:.10f}", checkpoint.bestTrainMSE) << "\n";
      if (checkpoint.bestTestMSE > 0.0) {
        std::cout << "  Previous test MSE:  " << std::format("{:.10f}", checkpoint.bestTestMSE) << "\n";
      }

      // Apply restored values to the live config
      config::ConfigManager::instance().applyOverrides([&](auto& s, auto& e) {
        for (const auto& p : params) {
          p.applyToConfig(s, e);
        }
      });

      // Use K from checkpoint (skip K tuning)
      const double checkpointK = checkpoint.K;
      std::cout << "  Using K from checkpoint: " << std::format("{:.6f}", checkpointK) << "\n\n";
      tuner.setK(checkpointK);

      startPass = checkpoint.completedPasses;
    }
    else {
      // =========================================================================
      // Step 7: Tune scaling constant K (ternary search)
      // =========================================================================
      std::cout << "Tuning scaling constant K...\n" << std::flush;
      const auto kStart = steady_clock::now();
      const double K = tuner.tuneK(trainSet);
      const auto kElapsed = std::chrono::duration<double>(steady_clock::now() - kStart).count();
      std::cout << "  Optimal K: " << std::format("{:.6f}", K) << " (found in " << std::format("{:.1f}", kElapsed) << "s)\n\n";
    }

    // =========================================================================
    // Step 8: Derive checkpoint path from output path
    // =========================================================================
    const std::filesystem::path outPath(outputPath);
    const std::string checkpointPath = (outPath.parent_path() / (outPath.stem().string() + "_checkpoint.yaml")).string();
    std::cout << "Checkpoint file: " << checkpointPath << "\n\n";

    // =========================================================================
    // Step 9: Run coordinate descent optimization
    // =========================================================================
    std::cout << "Starting coordinate descent optimization...\n";
    std::cout << "  Max passes: " << maxPasses << "\n";
    std::cout << "  Start pass: " << startPass << "\n";
    std::cout << std::string(70, '-') << "\n" << std::flush;

    const auto cdStart = steady_clock::now();

    tuner.tuneParameters(
      trainSet,
      testSet.empty() ? nullptr : &testSet,
      params,
      maxPasses,
      checkpointPath,
      datasetPath,
      startPass
    );

    const auto cdElapsed = std::chrono::duration<double>(steady_clock::now() - cdStart).count();

    // =========================================================================
    // Step 10: Final summary
    // =========================================================================
    const auto totalElapsed = std::chrono::duration<double>(steady_clock::now() - totalStart).count();

    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "TUNING COMPLETE\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << "  K:                 " << std::format("{:.6f}", tuner.getK()) << "\n";
    std::cout << "  Parameters tuned:  " << params.size() << "\n";

    // Count how many params changed from original
    int changedCount = 0;
    for (const auto& p : params) {
      if (p.currentValue != p.originalValue) {
        ++changedCount;
      }
    }
    std::cout << "  Parameters changed:" << changedCount << "/" << params.size() << "\n";
    std::cout << "  Optimization time: " << std::format("{:.1f}", cdElapsed) << "s\n";
    std::cout << "  Total time:        " << std::format("{:.1f}", totalElapsed) << "s\n";
    std::cout << "  Checkpoint:        " << checkpointPath << "\n";
    std::cout << "\n";

    // Print top movers (params with largest absolute change)
    std::vector<std::pair<std::string, int>> movers;
    movers.reserve(params.size());
    for (const auto& p : params) {
      const int delta = p.currentValue - p.originalValue;
      if (delta != 0) {
        movers.emplace_back(p.name, delta);
      }
    }
    std::ranges::sort(movers,
              [](const auto& a, const auto& b) { return std::abs(a.second) > std::abs(b.second); });

    if (!movers.empty()) {
      const int showCount = std::min(static_cast<int>(movers.size()), 20);
      std::cout << "Top " << showCount << " parameter changes:\n";
      std::cout << std::format("  {:<45s} {:>8s} {:>8s} {:>8s}\n", "Parameter", "Original", "Tuned", "Delta");
      std::cout << "  " << std::string(69, '-') << "\n";
      for (int i = 0; i < showCount; ++i) {
        // Find the param to get original value
        for (const auto& p : params) {
          if (p.name == movers[i].first) {
            std::cout << std::format("  {:<45s} {:>8d} {:>8d} {:>+8d}\n",
                                     p.name, p.originalValue, p.currentValue, movers[i].second);
            break;
          }
        }
      }
      std::cout << "\n";
    }

    std::cout << "Output files will be generated by a subsequent step (Sprint 6.9).\n";
    std::cout << "Checkpoint saved at: " << checkpointPath << "\n";

    return 0;

  } catch (const po::error& e) {
    std::cerr << "Error: " << e.what() << "\n";
    std::cerr << "Use --help for usage information.\n";
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << "\n";
    return 1;
  }
}
