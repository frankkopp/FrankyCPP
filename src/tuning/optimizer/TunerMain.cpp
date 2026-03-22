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
// This tool is part of the tuning infrastructure and is completely separate
// from the main FrankyCPP engine executable.
//
// Usage:
//   FrankyCPP_Tuner --dataset <file> [options]
//
// See docs/specs/PLAN_Texel_Tuning.md for details.
//
//=============================================================================

#include "init.h"
#include "version.h"

#include <boost/program_options.hpp>
#include <exception>
#include <iostream>
#include <string>

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
    // TODO: Phase 6 — Implement TexelTuner and wire it up here
    // =========================================================================
    std::cout << "Texel tuning not yet implemented (Phase 6).\n";
    std::cout << "See docs/specs/PLAN_Texel_Tuning.md for details.\n";

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
