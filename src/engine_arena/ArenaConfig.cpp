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
// ArenaConfig.cpp - Engine Arena Configuration Management Implementation
//=============================================================================

#include "ArenaConfig.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace arena {

  namespace {
    /// Derives suite name from EPD file path
    /// e.g., "test/testsets/wac.epd" -> "wac"
    std::string deriveSuiteNameFromPath(const std::string& epdPath) {
      const std::filesystem::path path(epdPath);
      std::string stem = path.stem().string();
      // Convert to lowercase for consistency
      std::ranges::transform(stem, stem.begin(), ::tolower);
      return stem;
    }
  }// namespace

  ArenaConfig ArenaConfig::loadFromYaml(const std::string& configPath) {
    // Check if file exists
    if (!std::filesystem::exists(configPath)) {
      throw std::runtime_error("Configuration file not found: " + configPath);
    }

    ArenaConfig config;

    try {
      YAML::Node root = YAML::LoadFile(configPath);

      // Load version
      if (root["version"]) {
        config.version = root["version"].as<std::string>();
      }
      else {
        throw std::runtime_error("Missing required field: version");
      }

      // Load results directory
      if (root["resultsDir"]) {
        config.resultsDir = root["resultsDir"].as<std::string>();
      }
      else {
        config.resultsDir = "./results";
      }

      // Load cutechess-cli path (global configuration)
      if (root["cutechessPath"]) {
        config.cutechessPath = root["cutechessPath"].as<std::string>();
      }

      // Load debug mode (optional, defaults to false)
      if (root["debugMode"]) {
        config.debugMode = root["debugMode"].as<bool>();
      }

      // Load test suite runs (new unified format)
      if (root["testSuiteRuns"]) {
        for (const auto& runNode : root["testSuiteRuns"]) {
          TestSuiteRunConfig run;

          // Required fields
          run.engine        = runNode["engine"].as<std::string>();
          run.engineVersion = runNode["engineVersion"].as<std::string>();
          run.enginePath    = runNode["enginePath"].as<std::string>();

          // Optional: tag (feature tag for tracking)
          if (runNode["tag"]) {
            run.tag = runNode["tag"].as<std::string>();
          }

          // Time per move in milliseconds
          const int timeMs = runNode["timePerMove"].as<int>();
          run.timePerMove  = milliseconds{timeMs};

          run.maxDepth = static_cast<chess::Depth>(runNode["maxDepth"].as<int>());

          // Optional fields with defaults
          if (runNode["isolatePositions"]) {
            run.isolatePositions = runNode["isolatePositions"].as<bool>();
          }
          if (runNode["debugMode"]) {
            run.debugMode = runNode["debugMode"].as<bool>();
          }
          if (runNode["commandLineArgs"]) {
            run.commandLineArgs = runNode["commandLineArgs"].as<std::string>();
          }
          if (runNode["uciOptions"]) {
            run.uciOptions = runNode["uciOptions"].as<std::string>();
          }
          if (runNode["parallelWorkers"]) {
            run.parallelWorkers = runNode["parallelWorkers"].as<int>();
            if (run.parallelWorkers < 1) {
              run.parallelWorkers = 1;
            }
          }

          // Parse suites list (can be strings or override objects)
          if (runNode["suites"]) {
            for (const auto& suiteNode : runNode["suites"]) {
              if (suiteNode.IsScalar()) {
                // Simple string path
                run.suites.emplace_back(suiteNode.as<std::string>());
              }
              else if (suiteNode.IsMap()) {
                // Override object with path and optional overrides
                SuiteOverride override;
                override.path = suiteNode["path"].as<std::string>();
                if (suiteNode["timePerMove"]) {
                  override.timePerMove = milliseconds{suiteNode["timePerMove"].as<int>()};
                }
                if (suiteNode["maxDepth"]) {
                  override.maxDepth = static_cast<chess::Depth>(suiteNode["maxDepth"].as<int>());
                }
                run.suites.emplace_back(std::move(override));
              }
            }
          }

          config.testSuiteRuns.push_back(std::move(run));
        }
      }

      // Load matches
      if (root["matches"]) {
        for (const auto& matchNode : root["matches"]) {
          MatchConfig match;
          match.name        = matchNode["name"].as<std::string>();
          match.engine1Path = matchNode["engine1Path"].as<std::string>();
          match.engine2Path = matchNode["engine2Path"].as<std::string>();

          // Optional: tag (feature tag for tracking)
          if (matchNode["tag"]) {
            match.tag = matchNode["tag"].as<std::string>();
          }

          // Engine versions (optional - if not specified, extract from path)
          if (matchNode["engine1Version"]) {
            match.engine1Version = matchNode["engine1Version"].as<std::string>();
          }
          else {
            // Try to extract version from path (e.g., "FrankyCPP_v1.1.exe" -> "v1.1")
            match.engine1Version = "";// Will be extracted from engine name later
          }

          if (matchNode["engine2Version"]) {
            match.engine2Version = matchNode["engine2Version"].as<std::string>();
          }
          else {
            match.engine2Version = "";// Will be extracted from engine name later
          }

          // Engine UCI options (optional - sent before isready during validation)
          if (matchNode["engine1Options"]) {
            match.engine1Options = matchNode["engine1Options"].as<std::string>();
          }
          if (matchNode["engine2Options"]) {
            match.engine2Options = matchNode["engine2Options"].as<std::string>();
          }

          // cutechessPath can be specified per-match or use global default
          if (matchNode["cutechessPath"]) {
            match.cutechessPath = matchNode["cutechessPath"].as<std::string>();
          }
          else if (!config.cutechessPath.empty()) {
            match.cutechessPath = config.cutechessPath;
          }
          else {
            throw std::runtime_error("cutechessPath not specified for match: " + match.name);
          }

          match.openingBook = matchNode["openingBook"].as<std::string>();
          match.timeControl = matchNode["timeControl"].as<std::string>();
          match.rounds      = matchNode["rounds"].as<int>();

          // Concurrency (optional, defaults to 1 for deterministic results)
          if (matchNode["concurrency"]) {
            match.concurrency = matchNode["concurrency"].as<int>();
          }

          // Batch size for resumable matches (optional, defaults to 0 = auto)
          // Auto means: max(2, concurrency) rounded up to even number
          if (matchNode["batchSize"]) {
            match.batchSize = matchNode["batchSize"].as<int>();
          }

          match.outputPgn = matchNode["outputPgn"].as<std::string>();

          config.matches.push_back(match);
        }
      }

      // Load benchmarks
      if (root["benchmarks"]) {
        for (const auto& benchNode : root["benchmarks"]) {
          BenchmarkConfig bench;

          // Required: name
          bench.name = benchNode["name"].as<std::string>();

          // Optional: enginePath (empty = use internal engine)
          if (benchNode["enginePath"]) {
            bench.enginePath = benchNode["enginePath"].as<std::string>();
          }

          // Optional: engineVersion (defaults to config.version)
          if (benchNode["engineVersion"]) {
            bench.engineVersion = benchNode["engineVersion"].as<std::string>();
          }
          else {
            bench.engineVersion = config.version;
          }

          // Optional: depth (default: 10)
          if (benchNode["depth"]) {
            bench.depth = benchNode["depth"].as<int>();
          }

          // Optional: hashSizeMB (default: 128)
          if (benchNode["hashSizeMB"]) {
            bench.hashSizeMB = benchNode["hashSizeMB"].as<int>();
          }

          // Optional: threads (default: 1)
          if (benchNode["threads"]) {
            bench.threads = benchNode["threads"].as<int>();
          }

          // Optional: commandLineArgs
          if (benchNode["commandLineArgs"]) {
            bench.commandLineArgs = benchNode["commandLineArgs"].as<std::string>();
          }

          // Optional: tag (feature tag, renamed from notes)
          if (benchNode["tag"]) {
            bench.tag = benchNode["tag"].as<std::string>();
          }

          config.benchmarks.push_back(bench);
        }
      }

    } catch (const YAML::Exception& e) {
      throw std::runtime_error("YAML parsing error: " + std::string(e.what()));
    }

    return config;
  }

  bool ArenaConfig::validate() const {
    // Check version is not empty
    if (version.empty()) {
      std::cerr << "Validation error: version is empty" << std::endl;
      return false;
    }

    // Check results directory path is valid and writable
    if (resultsDir.empty()) {
      std::cerr << "Validation error: resultsDir is empty" << std::endl;
      return false;
    }

    // Check resultsDir is writable (create if needed, then verify)
    try {
      std::filesystem::create_directories(resultsDir);
      // Test writability by creating a temp file
      const auto testFile = std::filesystem::path(resultsDir) / ".write_test";
      std::ofstream ofs(testFile);
      if (!ofs) {
        std::cerr << "Validation error: resultsDir is not writable: " << resultsDir << std::endl;
        return false;
      }
      ofs.close();
      std::filesystem::remove(testFile);
    } catch (const std::filesystem::filesystem_error& e) {
      std::cerr << "Validation error: cannot create/access resultsDir: " << resultsDir
                << " (" << e.what() << ")" << std::endl;
      return false;
    }

    // Validate test suite runs
    for (const auto& run : testSuiteRuns) {
      if (run.engine.empty() || run.enginePath.empty()) {
        std::cerr << "Validation error: test suite run has empty engine name or path" << std::endl;
        return false;
      }
      if (run.maxDepth <= 0) {
        std::cerr << "Validation error: test suite run for '" << run.engine
                  << "' has invalid maxDepth: " << run.maxDepth << std::endl;
        return false;
      }
      // Validate timePerMove > 0
      if (run.timePerMove.count() <= 0) {
        std::cerr << "Validation error: test suite run for '" << run.engine
                  << "' has invalid timePerMove: " << run.timePerMove.count() << "ms (must be > 0)" << std::endl;
        return false;
      }
      // Validate parallelWorkers > 0
      if (run.parallelWorkers <= 0) {
        std::cerr << "Validation error: test suite run for '" << run.engine
                  << "' has invalid parallelWorkers: " << run.parallelWorkers << " (must be > 0)" << std::endl;
        return false;
      }
      // Check engine executable exists
      if (!std::filesystem::exists(run.enginePath)) {
        std::cerr << "Validation error: engine not found for test suite run '" << run.engine
                  << "': " << run.enginePath << std::endl;
        std::cerr << "  Current working directory: "
                  << std::filesystem::current_path() << std::endl;
        std::cerr << "  Tip: Make sure to run from project root or use absolute paths" << std::endl;
        return false;
      }
      // Check suites list is not empty
      if (run.suites.empty()) {
        std::cerr << "Validation error: test suite run for '" << run.engine
                  << "' has no suites defined" << std::endl;
        return false;
      }
      // Check each suite EPD file exists
      for (const auto& suite : run.suites) {
        const std::string* epdPath = nullptr;
        if (std::holds_alternative<std::string>(suite)) {
          epdPath = &std::get<std::string>(suite);
        }
        else {
          epdPath = &std::get<SuiteOverride>(suite).path;
        }
        if (!std::filesystem::exists(*epdPath)) {
          std::cerr << "Validation error: EPD file not found for run '" << run.engine
                    << "': " << *epdPath << std::endl;
          std::cerr << "  Current working directory: "
                    << std::filesystem::current_path() << std::endl;
          std::cerr << "  Tip: Make sure to run from project root or use absolute paths" << std::endl;
          return false;
        }
      }
      // Warn if tag is empty
      if (run.tag.empty()) {
        std::cerr << "WARNING: Test suite run for '" << run.engine
                  << "' has empty tag - results won't be grouped by feature" << std::endl;
      }
    }

    // Validate matches
    for (const auto& match : matches) {
      if (match.name.empty() || match.engine1Path.empty() || match.engine2Path.empty() || match.cutechessPath.empty()) {
        std::cerr << "Validation error: match '" << match.name
                  << "' has empty required fields" << std::endl;
        return false;
      }
      if (match.rounds <= 0) {
        std::cerr << "Validation error: match '" << match.name
                  << "' has invalid rounds: " << match.rounds << std::endl;
        return false;
      }
      // Validate concurrency > 0
      if (match.concurrency <= 0) {
        std::cerr << "Validation error: match '" << match.name
                  << "' has invalid concurrency: " << match.concurrency << " (must be > 0)" << std::endl;
        return false;
      }
      // Validate batchSize if specified (must be even and >= 2)
      if (match.batchSize != 0) {
        if (match.batchSize < 2) {
          std::cerr << "Validation error: match '" << match.name
                    << "' has invalid batchSize: " << match.batchSize << " (must be >= 2)" << std::endl;
          return false;
        }
        if (match.batchSize % 2 != 0) {
          std::cerr << "Validation error: match '" << match.name
                    << "' has invalid batchSize: " << match.batchSize << " (must be even for color fairness)" << std::endl;
          return false;
        }
      }
      // Check cutechess-cli exists
      if (!std::filesystem::exists(match.cutechessPath)) {
        std::cerr << "Validation error: cutechess-cli not found for match '" << match.name
                  << "': " << match.cutechessPath << std::endl;
        return false;
      }
      // Check opening book exists if specified
      if (!match.openingBook.empty() && !std::filesystem::exists(match.openingBook)) {
        std::cerr << "Validation error: opening book not found for match '" << match.name
                  << "': " << match.openingBook << std::endl;
        return false;
      }
      // Warn if tag is empty
      if (match.tag.empty()) {
        std::cerr << "WARNING: Match '" << match.name
                  << "' has empty tag - results won't be grouped by feature" << std::endl;
      }
    }

    // Validate benchmarks
    for (const auto& bench : benchmarks) {
      if (bench.name.empty()) {
        std::cerr << "Validation error: benchmark has empty name" << std::endl;
        return false;
      }
      if (bench.depth <= 0 || bench.depth > 127) {
        std::cerr << "Validation error: benchmark '" << bench.name
                  << "' has invalid depth: " << bench.depth << " (must be 1-127)" << std::endl;
        return false;
      }
      if (bench.hashSizeMB <= 0 || bench.hashSizeMB > 65536) {
        std::cerr << "Validation error: benchmark '" << bench.name
                  << "' has invalid hashSizeMB: " << bench.hashSizeMB << " (must be 1-65536)" << std::endl;
        return false;
      }
      if (bench.threads <= 0 || bench.threads > 256) {
        std::cerr << "Validation error: benchmark '" << bench.name
                  << "' has invalid threads: " << bench.threads << " (must be 1-256)" << std::endl;
        return false;
      }
      // If external engine specified, check it exists
      if (!bench.enginePath.empty() && !std::filesystem::exists(bench.enginePath)) {
        std::cerr << "Validation error: engine not found for benchmark '" << bench.name
                  << "': " << bench.enginePath << std::endl;
        return false;
      }
    }

    return true;
  }

  std::vector<TestSuiteConfig> ArenaConfig::expandTestSuiteRuns() const {
    std::vector<TestSuiteConfig> expanded;

    for (const auto& run : testSuiteRuns) {
      for (const auto& suite : run.suites) {
        TestSuiteConfig config;

        // Get EPD path and optional overrides
        const std::string* epdPath = nullptr;
        std::optional<milliseconds> timeOverride;
        std::optional<chess::Depth> depthOverride;

        if (std::holds_alternative<std::string>(suite)) {
          epdPath = &std::get<std::string>(suite);
        }
        else {
          const auto& override = std::get<SuiteOverride>(suite);
          epdPath              = &override.path;
          timeOverride         = override.timePerMove;
          depthOverride        = override.maxDepth;
        }

        // Derive suite name from EPD path
        config.name = deriveSuiteNameFromPath(*epdPath);

        // Copy shared settings from run
        config.epdPath          = *epdPath;
        config.timePerMove      = timeOverride.value_or(run.timePerMove);
        config.maxDepth         = depthOverride.value_or(run.maxDepth);
        config.enginePath       = run.enginePath;
        config.engineVersion    = run.engineVersion;
        config.isolatePositions = run.isolatePositions;
        config.debugMode        = run.debugMode;
        config.commandLineArgs  = run.commandLineArgs;
        config.uciOptions       = run.uciOptions;
        config.parallelWorkers  = run.parallelWorkers;
        config.tag              = run.tag;

        expanded.push_back(std::move(config));
      }
    }

    return expanded;
  }

}// namespace arena
