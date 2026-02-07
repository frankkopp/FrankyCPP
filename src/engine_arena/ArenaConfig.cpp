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

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>

namespace arena {

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
    } else {
      throw std::runtime_error("Missing required field: version");
    }

    // Load results directory
    if (root["resultsDir"]) {
      config.resultsDir = root["resultsDir"].as<std::string>();
    } else {
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

    // Load test suites
    if (root["testSuites"]) {
      for (const auto& suiteNode : root["testSuites"]) {
        TestSuiteConfig suite;
        suite.name = suiteNode["name"].as<std::string>();
        suite.epdPath = suiteNode["epdPath"].as<std::string>();

        // Time per move in milliseconds
        int timeMs = suiteNode["timePerMove"].as<int>();
        suite.timePerMove = milliseconds{timeMs};

        suite.maxDepth = static_cast<Depth>(suiteNode["maxDepth"].as<int>());

        // Optional: external engine path (empty = use internal engine)
        if (suiteNode["enginePath"]) {
          suite.enginePath = suiteNode["enginePath"].as<std::string>();
        }

        // Optional: engine version (explicit, not parsed from UCI name)
        if (suiteNode["engineVersion"]) {
          suite.engineVersion = suiteNode["engineVersion"].as<std::string>();
        }

        // Optional: isolate positions flag (default: true for fair comparison)
        if (suiteNode["isolatePositions"]) {
          suite.isolatePositions = suiteNode["isolatePositions"].as<bool>();
        }

        // Optional: debug mode (print UCI communication)
        if (suiteNode["debugMode"]) {
          suite.debugMode = suiteNode["debugMode"].as<bool>();
        }

        // Optional: command-line arguments for engine startup
        if (suiteNode["commandLineArgs"]) {
          suite.commandLineArgs = suiteNode["commandLineArgs"].as<std::string>();
        }

        // Optional: UCI options (setoption commands sent after initialization)
        if (suiteNode["uciOptions"]) {
          suite.uciOptions = suiteNode["uciOptions"].as<std::string>();
        }

        // Optional: parallel workers (1 = sequential, N>1 = parallel execution)
        if (suiteNode["parallelWorkers"]) {
          suite.parallelWorkers = suiteNode["parallelWorkers"].as<int>();
          if (suite.parallelWorkers < 1) {
            suite.parallelWorkers = 1;  // Minimum 1 worker
          }
        }

        config.testSuites.push_back(suite);
      }
    }

    // Load matches
    if (root["matches"]) {
      for (const auto& matchNode : root["matches"]) {
        MatchConfig match;
        match.name = matchNode["name"].as<std::string>();
        match.engine1Path = matchNode["engine1Path"].as<std::string>();
        match.engine2Path = matchNode["engine2Path"].as<std::string>();

        // Engine versions (optional - if not specified, extract from path)
        if (matchNode["engine1Version"]) {
          match.engine1Version = matchNode["engine1Version"].as<std::string>();
        } else {
          // Try to extract version from path (e.g., "FrankyCPP_v1.1.exe" -> "v1.1")
          match.engine1Version = "";  // Will be extracted from engine name later
        }

        if (matchNode["engine2Version"]) {
          match.engine2Version = matchNode["engine2Version"].as<std::string>();
        } else {
          match.engine2Version = "";  // Will be extracted from engine name later
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
        } else if (!config.cutechessPath.empty()) {
          match.cutechessPath = config.cutechessPath;
        } else {
          throw std::runtime_error("cutechessPath not specified for match: " + match.name);
        }

        match.openingBook = matchNode["openingBook"].as<std::string>();
        match.timeControl = matchNode["timeControl"].as<std::string>();
        match.rounds = matchNode["rounds"].as<int>();

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
    auto testFile = std::filesystem::path(resultsDir) / ".write_test";
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

  // Validate test suites
  for (const auto& suite : testSuites) {
    if (suite.name.empty() || suite.epdPath.empty()) {
      std::cerr << "Validation error: test suite has empty name or path" << std::endl;
      return false;
    }
    if (suite.maxDepth <= 0) {
      std::cerr << "Validation error: test suite '" << suite.name
                << "' has invalid maxDepth: " << suite.maxDepth << std::endl;
      return false;
    }
    // Validate timePerMove > 0
    if (suite.timePerMove.count() <= 0) {
      std::cerr << "Validation error: test suite '" << suite.name
                << "' has invalid timePerMove: " << suite.timePerMove.count() << "ms (must be > 0)" << std::endl;
      return false;
    }
    // Validate parallelWorkers > 0
    if (suite.parallelWorkers <= 0) {
      std::cerr << "Validation error: test suite '" << suite.name
                << "' has invalid parallelWorkers: " << suite.parallelWorkers << " (must be > 0)" << std::endl;
      return false;
    }
    // Check EPD file exists
    if (!std::filesystem::exists(suite.epdPath)) {
      std::cerr << "Validation error: EPD file not found for suite '" << suite.name
                << "': " << suite.epdPath << std::endl;
      std::cerr << "  Current working directory: "
                << std::filesystem::current_path() << std::endl;
      std::cerr << "  Tip: Make sure to run from project root or use absolute paths" << std::endl;
      return false;
    }
    // Validate external engine path is provided and exists
    if (suite.enginePath.empty()) {
      std::cerr << "Validation error: enginePath is required for suite '" << suite.name << "'" << std::endl;
      std::cerr << "  Arena test suites require an external UCI engine executable" << std::endl;
      return false;
    }
    if (!std::filesystem::exists(suite.enginePath)) {
      std::cerr << "Validation error: External engine not found for suite '" << suite.name
                << "': " << suite.enginePath << std::endl;
      std::cerr << "  Current working directory: "
                << std::filesystem::current_path() << std::endl;
      std::cerr << "  Tip: Make sure to run from project root or use absolute paths" << std::endl;
      return false;
    }
  }

  // Validate matches
  for (const auto& match : matches) {
    if (match.name.empty() || match.engine1Path.empty() ||
        match.engine2Path.empty() || match.cutechessPath.empty()) {
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
  }

  return true;
}

} // namespace arena
