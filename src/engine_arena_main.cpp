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
// interface for running test suites, engine matches, and generating reports.
//
// Command-Line Modes:
//   --help              Show usage information
//   --testsuites        Run EPD test suites only
//   --matches           Run engine matches only
//   --report            Show baseline report (all engines, all test suites)
//   --engines           List available engines from results
//   --cmp <engine>      Compare engine against baselines
//   (no args)           Run all configured tests and matches
//
// Requirements:
//   - Must be run from project root directory
//   - Config file at config/arena.yaml (or specify with --config)
//   - Paths in config are relative to project root
//
//=============================================================================

#include "config/ConfigManager.h"
#include "engine/Benchmark.h"
#include "engine_arena/ArenaRunner.h"
#include "engine_arena/BenchmarkRunner.h"
#include "engine_arena/ResultWriter.h"
#include "init.h"

#include <boost/program_options.hpp>
#include <exception>
#include <filesystem>
#include <iostream>

using namespace engine;
using namespace chess;
using namespace config;

int main(int argc, char* argv[]) {
  try {
    // Initialize FrankyCPP
    init::init();

    namespace po = boost::program_options;

    // Define command-line options
    po::options_description desc("FrankyCPP Arena - Engine Strength Testing\n\nOptions");
    desc.add_options()("help,h", "Show this help message")("config,c", po::value<std::string>()->default_value("config/arena.yaml"),
                                                           "Configuration file path")("testsuites,t", "Run test suites only")("matches,m", "Run matches only")
      // Benchmark options
      ("bench,b", "Run benchmarks (NPS measurement)")("bench-report", "Show benchmark results history")
      // Reporting options
      ("report,r", "Show baseline report (all engines, all test suites)")("baselines", "Alias for --report")("engines", "List all available engines from results")("cmp", po::value<std::string>(),
                                                                                                                                                                   "Compare engine against baselines: --cmp FrankyCPP-v1.2-dev")("baseline", po::value<std::vector<std::string>>()->multitoken(),
                                                                                                                                                                                                                                 "Specify baseline(s) for comparison (can repeat)")
      // Filtering options
      ("testsuites-only", "Show only test suite results (filter out matches)")("matches-only", "Show only match results (filter out test suites)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // Show help
    if (vm.contains("help")) {
      std::cout << desc << std::endl;
      std::cout << "\nExecution Commands:\n";
      std::cout << "  (no args)                Run all tests, matches, and benchmarks\n";
      std::cout << "  --testsuites, -t         Run test suites only\n";
      std::cout << "  --matches, -m            Run matches only\n";
      std::cout << "  --bench, -b              Run benchmarks only\n";
      std::cout << "  --bench-report           Show benchmark results history\n";
      std::cout << "\nReporting Commands:\n";
      std::cout << "  --report, --baselines    Show baseline report (all engines)\n";
      std::cout << "  --engines                List available engines from results\n";
      std::cout << "  --cmp <engine>           Compare engine against baselines\n";
      std::cout << "  --baseline <engine>      Specify baseline(s) for --cmp\n";
      std::cout << "  --testsuites-only        Show only test suite results\n";
      std::cout << "  --matches-only           Show only match results\n";
      std::cout << "\nExamples:\n";
      std::cout << "  FrankyCPP_Arena --bench\n";
      std::cout << "  FrankyCPP_Arena --bench-report\n";
      std::cout << "  FrankyCPP_Arena --report\n";
      std::cout << "  FrankyCPP_Arena --report --testsuites-only\n";
      std::cout << "  FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev\n";
      std::cout << "  FrankyCPP_Arena --testsuites\n";
      std::cout << "\nBenchmark Configuration (in arena.yaml):\n";
      std::cout << "  benchmarks:\n";
      std::cout << "    - name: \"v1.2 Release\"\n";
      std::cout << "      engineVersion: \"v1.2\"\n";
      std::cout << "      depth: 10\n";
      std::cout << "      hashSizeMB: 128\n";
      std::cout << "      notes: \"After StaticMoveList optimization\"\n";
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
    std::cout << "  Benchmarks: " << config.benchmarks.size() << std::endl;

    // Handle bench-report before validation (doesn't need full config)
    if (vm.contains("bench-report")) {
      arena::ResultWriter writer(config.resultsDir);
      auto results = writer.readBenchmarkResults();
      arena::BenchmarkRunner::printResultsTable(results);
      return 0;
    }

    // Validate configuration
    if (!config.validate()) {
      std::cerr << "\nERROR: Configuration validation failed!" << std::endl;
      std::cerr << "Please check that all paths exist and values are correct." << std::endl;
      std::cerr << "\nIMPORTANT: Make sure you are running from the project root directory." << std::endl;
      std::cerr << R"(Example: cd D:\_DEV\FrankyCPP && .\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe)" << std::endl;
      return 1;
    }

    std::cout << "Configuration validated successfully!" << std::endl;

    // Handle benchmark command
    if (vm.contains("bench")) {
      if (config.benchmarks.empty()) {
        std::cerr << "ERROR: No benchmarks configured in " << configPath << std::endl;
        std::cerr << "Add a 'benchmarks' section to your arena.yaml file." << std::endl;
        return 1;
      }

      std::cout << "\n=== Running Benchmarks ===" << std::endl;
      std::cout << "Configured benchmarks: " << config.benchmarks.size() << std::endl;

      // Print current engine configuration
      std::cout << "\n--- Current Engine Configuration ---" << std::endl;
      std::cout << ConfigManager::instance().strCurrent() << std::endl;

      arena::ResultWriter writer(config.resultsDir);

      for (const auto& benchConfig : config.benchmarks) {
        std::cout << "\n--- Benchmark: " << benchConfig.name << " ---" << std::endl;
        std::cout << "  Version: " << benchConfig.engineVersion << std::endl;
        std::cout << "  Depth: " << benchConfig.depth << std::endl;
        std::cout << "  Hash: " << benchConfig.hashSizeMB << " MB" << std::endl;
        std::cout << "  Threads: " << benchConfig.threads << std::endl;
        if (!benchConfig.notes.empty()) {
          std::cout << "  Notes: " << benchConfig.notes << std::endl;
        }
        std::cout << std::endl;

        // Run benchmark
        arena::BenchmarkRunner runner(benchConfig, config.version);
        auto result = runner.run();

        // Print results
        engine::Benchmark::printResults({result.totalNodes,
                                         milliseconds{result.totalTimeMs},
                                         static_cast<double>(result.nps),
                                         result.positions,
                                         result.engineName + " " + result.engineVersion});

        // Save results only if notes are provided (non-empty)
        // This allows ad-hoc runs without polluting the results file
        if (!result.notes.empty()) {
          const std::string path = writer.writeBenchmarkResult(result);
          std::cout << "\nResults saved to: " << path << std::endl;
        }
        else {
          std::cout << "\n[Ad-hoc run] Results not saved (no notes provided)" << std::endl;
        }
      }

      // Show history
      std::cout << "\n";
      auto allResults = writer.readBenchmarkResults();
      arena::BenchmarkRunner::printResultsTable(allResults);

      return 0;
    }

    // Create ArenaRunner - main orchestrator
    arena::ArenaRunner arenaRunner(config);

    // Process commands - check reporting commands first
    if (vm.contains("engines")) {
      // List available engines
      std::cout << "\nAvailable engines from results:\n";
      std::cout << "--------------------------------------------------------------------------------\n";
      auto engines = arenaRunner.listAvailableEngines();
      if (engines.empty()) {
        std::cout << "  No results found. Run test suites first.\n";
      }
      else {
        for (const auto& engine : engines) {
          std::cout << "  " << engine.toString() << "\n";
        }
      }
      std::cout << "--------------------------------------------------------------------------------\n";
    }
    else if (vm.contains("report") || vm.contains("baselines")) {
      // Show baseline report
      auto data = arenaRunner.loadAllResults();

      bool testSuitesOnly = vm.contains("testsuites-only");
      bool matchesOnly    = vm.contains("matches-only");

      // Can't have both filters
      if (testSuitesOnly && matchesOnly) {
        std::cerr << "ERROR: Cannot use both --testsuites-only and --matches-only\n";
        return 1;
      }

      if (matchesOnly) {
        // Show only match results
        std::cout << arena::ArenaRunner::generateMatchBaselineReport(data);
      }
      else if (testSuitesOnly) {
        // Show only test suite results
        std::cout << arena::ArenaRunner::generateBaselineReport(data);
      }
      else {
        // Show both (default)
        std::cout << arena::ArenaRunner::generateBaselineReport(data);
        std::cout << arena::ArenaRunner::generateMatchBaselineReport(data);
      }
    }
    else if (vm.contains("cmp")) {
      // Comparison report
      auto targetStr               = vm["cmp"].as<std::string>();
      arena::EngineId targetEngine = arena::EngineId::fromString(targetStr);

      std::vector<arena::EngineId> baselines;
      if (vm.contains("baseline")) {
        auto baselineStrs = vm["baseline"].as<std::vector<std::string>>();
        for (const auto& str : baselineStrs) {
          baselines.push_back(arena::EngineId::fromString(str));
        }
      }

      auto data = arenaRunner.loadAllResults();

      bool testSuitesOnly = vm.contains("testsuites-only");
      bool matchesOnly    = vm.contains("matches-only");

      // Can't have both filters
      if (testSuitesOnly && matchesOnly) {
        std::cerr << "ERROR: Cannot use both --testsuites-only and --matches-only\n";
        return 1;
      }

      if (matchesOnly) {
        // Show only match comparison
        std::cout << arena::ArenaRunner::generateMatchComparisonReport(data, targetEngine, baselines);
      }
      else if (testSuitesOnly) {
        // Show only test suite comparison
        std::cout << arena::ArenaRunner::generateComparisonReport(data, targetEngine, baselines);
      }
      else {
        // Show both (default)
        std::cout << arena::ArenaRunner::generateComparisonReport(data, targetEngine, baselines);
        std::cout << arena::ArenaRunner::generateMatchComparisonReport(data, targetEngine, baselines);
      }
    }
    else if (vm.contains("testsuites")) {
      arenaRunner.runTestSuitesOnly();
    }
    else if (vm.contains("matches")) {
      arenaRunner.runMatchesOnly();
    }
    else {
      arenaRunner.runAll();
    }

    std::cout << "\n=== Arena Complete ===" << std::endl;
    std::cout << "All operations finished successfully!" << std::endl;

    return 0;

  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
}
