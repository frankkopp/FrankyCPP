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

        // cutechessPath can be specified globally or per-match
        if (matchNode["cutechessPath"]) {
          match.cutechessPath = matchNode["cutechessPath"].as<std::string>();
        } else if (root["cutechessPath"]) {
          match.cutechessPath = root["cutechessPath"].as<std::string>();
        } else {
          throw std::runtime_error("cutechessPath not specified for match: " + match.name);
        }

        match.openingBook = matchNode["openingBook"].as<std::string>();
        match.timeControl = matchNode["timeControl"].as<std::string>();
        match.rounds = matchNode["rounds"].as<int>();
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

  // Check results directory path is valid
  if (resultsDir.empty()) {
    std::cerr << "Validation error: resultsDir is empty" << std::endl;
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
    // Check EPD file exists
    if (!std::filesystem::exists(suite.epdPath)) {
      std::cerr << "Validation error: EPD file not found for suite '" << suite.name
                << "': " << suite.epdPath << std::endl;
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
