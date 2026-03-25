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
// ExtractorMain.cpp - Position Extractor Main Entry Point
//=============================================================================
//
// Standalone executable for extracting labeled positions from PGN files.
// Produces FEN+result text files suitable for Texel tuning.
//
// This tool is part of the tuning infrastructure and is completely separate
// from the main FrankyCPP engine executable.
//
// Usage:
//   FrankyCPP_Extractor --input <pgn> --output <txt> [options]
//
// See docs/specs/PLAN_Texel_Tuning.md for details.
//
//=============================================================================

#include "PositionExtractor.h"
#include "init.h"
#include "version.h"

#include <boost/program_options.hpp>
#include <chrono>
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
      "FrankyCPP Position Extractor v{}.{}.{}",
      FrankyCPP_VERSION_MAJOR, FrankyCPP_VERSION_MINOR, FrankyCPP_VERSION_PATCH);

    // =========================================================================
    // Command-line options
    // =========================================================================
    po::options_description desc(appName + "\n\nExtracts labeled positions from PGN files for Texel tuning.\n\nOptions");
    // clang-format off
    desc.add_options()
      ("help,h",              "Show this help message")
      ("input,i",             po::value<std::string>()->required(),
                              "Input PGN file path")
      ("output,o",            po::value<std::string>()->required(),
                              "Output file path (FEN + result per line)")
      ("min-move",            po::value<int>()->default_value(16),
                              "Skip first N half-moves (default: 16 = 8 full moves)")
      ("min-pieces",          po::value<int>()->default_value(6),
                              "Skip positions with fewer than N pieces (default: 6)")
      ("skip-captures",       po::bool_switch()->default_value(true),
                              "Skip positions right after captures (default: on)")
      ("skip-promotions",     po::bool_switch()->default_value(true),
                              "Skip positions right after promotions (default: on)")
      ("qsearch-filter",      po::bool_switch()->default_value(false),
                              "Enable qsearch stability filter (default: off)")
      ("qsearch-threshold",   po::value<int>()->default_value(150),
                              "Threshold in centipawns for qsearch filter (default: 150)")
      ("score-filter",        po::bool_switch()->default_value(false),
                              "Skip positions where search score contradicts game result (default: off)")
      ("score-threshold",     po::value<int>()->default_value(200),
                              "Threshold in centipawns for score contradiction filter (default: 200)")
      ("skip-termination",    po::bool_switch()->default_value(true),
                              "Skip games with [Termination] header (default: on)")
      ("verbose,v",           po::bool_switch()->default_value(false),
                              "Enable verbose output");
    // clang-format on

    // Allow positional arguments: input output
    po::positional_options_description positional;
    positional.add("input", 1);
    positional.add("output", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(positional).run(), vm);

    // Show help before notify() so --help works without required args
    if (vm.contains("help")) {
      std::cout << desc << "\n";
      std::cout << "Examples:\n";
      std::cout << "  FrankyCPP_Extractor --input games.pgn --output positions.txt\n";
      std::cout << "  FrankyCPP_Extractor games.pgn positions.txt --min-move 20\n";
      std::cout << "  FrankyCPP_Extractor -i selfplay.pgn -o data.txt --qsearch-filter\n";
      std::cout << "\n";
      std::cout << "Output Format:\n";
      std::cout << "  One position per line: <FEN> [<result>]\n";
      std::cout << "  Result: 1.0 (white win), 0.5 (draw), 0.0 (black win)\n";
      std::cout << "\n";
      std::cout << "Filters Applied:\n";
      std::cout << "  1. Skip early moves (--min-move)\n";
      std::cout << "  2. Skip positions in check\n";
      std::cout << "  3. Skip positions after captures/promotions (--skip-captures/--skip-promotions)\n";
      std::cout << "  4. Skip trivial endgames (--min-pieces)\n";
      std::cout << "  5. Qsearch stability filter (--qsearch-filter, optional)\n";
      std::cout << "  6. Score contradiction filter (--score-filter, optional)\n";
      return 0;
    }

    // Validate required arguments
    po::notify(vm);

    // =========================================================================
    // Display configuration
    // =========================================================================
    std::cout << appName << "\n";
    std::cout << std::string(appName.size(), '=') << "\n\n";

    const auto& inputPath  = vm["input"].as<std::string>();
    const auto& outputPath = vm["output"].as<std::string>();
    const int minMove      = vm["min-move"].as<int>();
    const int minPieces    = vm["min-pieces"].as<int>();
    const bool skipCapt    = vm["skip-captures"].as<bool>();
    const bool skipPromo   = vm["skip-promotions"].as<bool>();
    const bool skipTerm    = vm["skip-termination"].as<bool>();
    const bool qsFilter    = vm["qsearch-filter"].as<bool>();
    const int qsThreshold  = vm["qsearch-threshold"].as<int>();
    const bool scFilter    = vm["score-filter"].as<bool>();
    const int scThreshold  = vm["score-threshold"].as<int>();
    const bool verbose     = vm["verbose"].as<bool>();

    std::cout << "Configuration:\n";
    std::cout << "  Input PGN:         " << inputPath << "\n";
    std::cout << "  Output file:       " << outputPath << "\n";
    std::cout << "  Min half-moves:    " << minMove << "\n";
    std::cout << "  Min pieces:        " << minPieces << "\n";
    std::cout << "  Skip captures:     " << (skipCapt ? "yes" : "no") << "\n";
    std::cout << "  Skip promotions:   " << (skipPromo ? "yes" : "no") << "\n";
    std::cout << "  Skip termination:  " << (skipTerm ? "yes" : "no") << "\n";
    std::cout << "  Qsearch filter:    " << (qsFilter ? "yes" : "no") << "\n";
    if (qsFilter) {
      std::cout << "  Qsearch threshold: " << qsThreshold << " cp\n";
    }
    std::cout << "  Score filter:      " << (scFilter ? "yes" : "no") << "\n";
    if (scFilter) {
      std::cout << "  Score threshold:   " << scThreshold << " cp\n";
    }
    std::cout << "  Verbose:           " << (verbose ? "yes" : "no") << "\n";
    std::cout << "\n";

    // =========================================================================
    // Build extraction config and run
    // =========================================================================
    tuning::ExtractionConfig config;
    config.minHalfMove      = minMove;
    config.minPieces        = minPieces;
    config.skipCaptures     = skipCapt;
    config.skipPromotions   = skipPromo;
    config.skipTermination  = skipTerm;
    config.qsearchFilter    = qsFilter;
    config.qsearchThreshold = qsThreshold;
    config.scoreFilter      = scFilter;
    config.scoreThreshold   = scThreshold;
    config.verbose          = verbose;

    tuning::PositionExtractor extractor;

    std::cout << "Extracting positions...\n" << std::flush;
    const auto startTime = steady_clock::now();
    extractor.extract(inputPath, outputPath, config);
    const auto endTime = steady_clock::now();
    std::cout << "Extraction complete.\n\n";

    const auto elapsedMs = std::chrono::duration_cast<milliseconds>(endTime - startTime).count();
    extractor.getStats().printSummary(inputPath, outputPath, elapsedMs);

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
