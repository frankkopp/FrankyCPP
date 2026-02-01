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
// engine_arena_main.cpp - Engine Arena Main Entry Point
//=============================================================================
//
// Main executable for the Engine Arena testing framework. Provides a command-line
// interface for running test suites, engine matches, and version comparisons.
//
// Command-Line Modes:
//   --help              Show usage information
//   --testsuites        Run EPD test suites only
//   --matches           Run engine matches only
//   --compare v1 v2     Compare results between two versions
//   (no args)           Run all configured tests and matches
//
// Requirements:
//   - Must be run from project root directory
//   - Config file at config/arena.yaml (or specify with --config)
//   - Paths in config are relative to project root
//
//=============================================================================

#include "engine_arena/ArenaConfig.h"
#include "engine_arena/ResultWriter.h"
#include "init.h"

#include <boost/program_options.hpp>
#include <iostream>
#include <exception>
#include <filesystem>

int main(int argc, char* argv[]) {
  try {
    // Initialize FrankyCPP
    init::init();

    namespace po = boost::program_options;

    // Define command-line options
    po::options_description desc("FrankyCPP Arena - Engine Strength Testing\n\nOptions");
    desc.add_options()
      ("help,h", "Show this help message")
      ("config,c", po::value<std::string>()->default_value("config/arena.yaml"),
       "Configuration file path")
      ("testsuites,t", "Run test suites only")
      ("matches,m", "Run matches only")
      ("compare", po::value<std::vector<std::string>>()->multitoken(),
       "Compare two versions: --compare v1.1 v1.0");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // Show help
    if (vm.contains("help")) {
      std::cout << desc << std::endl;
      std::cout << "\nExamples:\n";
      std::cout << "  Run all tests:       FrankyCPP_Arena\n";
      std::cout << "  Test suites only:    FrankyCPP_Arena --testsuites\n";
      std::cout << "  Matches only:        FrankyCPP_Arena --matches\n";
      std::cout << "  Compare versions:    FrankyCPP_Arena --compare v1.1 v1.0\n";
      return 0;
    }

    // Load configuration
    auto configPath = vm["config"].as<std::string>();
    std::cout << "Current working directory: " << std::filesystem::current_path() << std::endl;
    std::cout << "Loading configuration from: " << configPath << std::endl;

    arena::ArenaConfig config = arena::ArenaConfig::loadFromYaml(configPath);

    std::cout << "Configuration loaded successfully!" << std::endl;
    std::cout << "  Version: " << config.version << std::endl;
    std::cout << "  Results Directory: " << config.resultsDir << std::endl;
    std::cout << "  Test Suites: " << config.testSuites.size() << std::endl;
    std::cout << "  Matches: " << config.matches.size() << std::endl;

    // Validate configuration
    if (!config.validate()) {
      std::cerr << "\nERROR: Configuration validation failed!" << std::endl;
      std::cerr << "Please check that all paths exist and values are correct." << std::endl;
      std::cerr << "\nIMPORTANT: Make sure you are running from the project root directory." << std::endl;
      std::cerr << R"(Example: cd D:\_DEV\FrankyCPP && .\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe)" << std::endl;
      return 1;
    }

    std::cout << "Configuration validated successfully!" << std::endl;

    // Create ResultWriter to ensure directories exist
    arena::ResultWriter resultWriter(config.resultsDir);
    std::cout << "Result directories created/verified." << std::endl;

    // Process commands
    if (vm.contains("compare")) {
      auto versions = vm["compare"].as<std::vector<std::string>>();
      if (versions.size() != 2) {
        std::cerr << "ERROR: --compare requires exactly 2 version arguments" << std::endl;
        return 1;
      }
      std::cout << "\n[COMPARE MODE]" << std::endl;
      std::cout << "Comparing versions: " << versions[0] << " vs " << versions[1] << std::endl;
      std::cout << "NOTE: Comparison functionality will be implemented in Phase 4" << std::endl;

    } else if (vm.contains("testsuites")) {
      std::cout << "\n[TEST SUITES MODE]" << std::endl;
      std::cout << "Will run " << config.testSuites.size() << " test suite(s)" << std::endl;
      for (const auto& suite : config.testSuites) {
        std::cout << "  - " << suite.name << " (" << suite.epdPath << ")" << std::endl;
      }
      std::cout << "NOTE: Test suite execution will be implemented in Phase 2" << std::endl;

    } else if (vm.contains("matches")) {
      std::cout << "\n[MATCHES MODE]" << std::endl;
      std::cout << "Will run " << config.matches.size() << " match(es)" << std::endl;
      for (const auto& match : config.matches) {
        std::cout << "  - " << match.name << " (" << match.rounds << " rounds)" << std::endl;
      }
      std::cout << "NOTE: Match execution will be implemented in Phase 3" << std::endl;

    } else {
      std::cout << "\n[FULL RUN MODE]" << std::endl;
      std::cout << "Will run all test suites and matches" << std::endl;
      std::cout << "NOTE: Full execution will be implemented in later phases" << std::endl;
    }

    std::cout << "\n=== Phase 1 Complete ===" << std::endl;
    std::cout << "Arena framework initialized successfully!" << std::endl;

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
}
