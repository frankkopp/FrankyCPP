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
// MatchRunner.cpp - Engine Arena Match Execution Implementation
//=============================================================================

#include "MatchRunner.h"
#include "UCIEngine.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#endif

namespace arena {

using std::chrono::system_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

MatchRunner::MatchRunner(const ArenaConfig& config)
    : arenaConfig(config) {
}

MatchResult MatchRunner::runMatch(const MatchConfig& matchConfig) const {
  std::cout << "\n==================================================================" << std::endl;
  std::cout << "Running Match: " << matchConfig.name << std::endl;
  std::cout << "==================================================================" << std::endl;
  std::cout << "Engine 1:       " << matchConfig.engine1Path << std::endl;
  std::cout << "Engine 2:       " << matchConfig.engine2Path << std::endl;
  std::cout << "Opening Book:   " << matchConfig.openingBook << std::endl;
  std::cout << "Time Control:   " << matchConfig.timeControl << std::endl;
  std::cout << "Rounds:         " << matchConfig.rounds << std::endl;
  std::cout << "Output PGN:     " << matchConfig.outputPgn << std::endl;
  std::cout << std::endl;

  // Validate configuration
  validateMatchConfig(matchConfig);

  // Get UCI engine names by briefly starting each engine
  // This validates engines work and gets canonical names from UCI protocol
  std::cout << "Validating engines and getting UCI names..." << std::endl;
  const std::string engine1Name = getUciEngineName(matchConfig.engine1Path);
  std::cout << "  Engine 1: " << engine1Name << std::endl;
  const std::string engine2Name = getUciEngineName(matchConfig.engine2Path);
  std::cout << "  Engine 2: " << engine2Name << std::endl;
  std::cout << std::endl;

  // Build cutechess-cli command
  const std::string command = buildCutechessCommand(matchConfig, engine1Name, engine2Name);
  std::cout << "Executing cutechess-cli..." << std::endl;
  std::cout << "Command: " << command << std::endl;
  std::cout << std::endl;

  // Execute cutechess-cli
  const auto startTime = system_clock::now();
  std::string output;
  if (!executeCutechess(command, output)) {
    throw std::runtime_error("cutechess-cli execution failed");
  }
  const auto endTime = system_clock::now();
  const auto duration = duration_cast<milliseconds>(endTime - startTime).count();

  // Parse output
  MatchResult result = parseOutput(output, matchConfig, engine1Name, engine2Name);
  result.durationMs = duration;

  std::cout << "\n------------------------------------------------------------------" << std::endl;
  std::cout << "Match Complete: " << matchConfig.name << std::endl;
  std::cout << "  " << result.engine1Name << ": " << result.engine1Wins << " wins, ";
  std::cout << result.draws << " draws, " << result.engine2Wins << " losses" << std::endl;
  std::cout << "  " << result.engine2Name << ": " << result.engine2Wins << " wins, ";
  std::cout << result.draws << " draws, " << result.engine1Wins << " losses" << std::endl;
  std::cout << "  Score: " << result.engine1Score << " - " << result.engine2Score << std::endl;
  std::cout << "  ELO Difference: " << std::fixed << std::setprecision(1)
            << (result.eloDifference > 0 ? "+" : "") << result.eloDifference << std::endl;
  std::cout << "  Duration: " << static_cast<double>(result.durationMs) / 1000.0 << "s" << std::endl;
  std::cout << "==================================================================" << std::endl;

  return result;
}

std::vector<MatchResult> MatchRunner::runAllMatches() const {
  std::vector<MatchResult> results;
  results.reserve(arenaConfig.matches.size());

  std::cout << "\n===================================================================" << std::endl;
  std::cout << "Running All Matches" << std::endl;
  std::cout << "===================================================================" << std::endl;
  std::cout << "Engine Version: " << arenaConfig.version << std::endl;
  std::cout << "Number of Matches: " << arenaConfig.matches.size() << std::endl;
  std::cout << "===================================================================" << std::endl;

  int matchNumber = 0;
  for (const auto& matchConfig : arenaConfig.matches) {
    matchNumber++;
    std::cout << "\n[" << matchNumber << "/" << arenaConfig.matches.size() << "] ";

    try {
      MatchResult result = runMatch(matchConfig);
      results.push_back(std::move(result));
    } catch (const std::exception& e) {
      std::cerr << "\nERROR: Failed to run match '" << matchConfig.name << "': "
                << e.what() << std::endl;
      throw; // Re-throw to allow caller to handle
    }
  }

  // Print summary
  std::cout << "\n===================================================================" << std::endl;
  std::cout << "All Matches Complete" << std::endl;
  std::cout << "===================================================================" << std::endl;

  for (const auto& result : results) {
    std::cout << "  " << result.matchName << ": "
              << result.engine1Score << " - " << result.engine2Score
              << " (ELO: " << std::fixed << std::setprecision(1)
              << (result.eloDifference > 0 ? "+" : "") << result.eloDifference << ")"
              << std::endl;
  }

  std::cout << "===================================================================" << std::endl;

  return results;
}

std::string MatchRunner::buildCutechessCommand(const MatchConfig& matchConfig,
                                               const std::string& engine1Name,
                                               const std::string& engine2Name) const {
  std::ostringstream cmd;

  // Use quoted path for cutechess-cli
  cmd << "\"" << matchConfig.cutechessPath << "\"";

  // Debug mode (prints engine I/O communication)
  // Note: cutechess-cli requires "-debug all" not just "-debug"
  if (arenaConfig.debugMode) {
    cmd << " -debug all";
  }

  // Engine 1 - use UCI name for identification
  cmd << " -engine cmd=\"" << matchConfig.engine1Path << "\"";
  cmd << " name=\"" << engine1Name << "\"";

  // Engine 2 - use UCI name for identification
  cmd << " -engine cmd=\"" << matchConfig.engine2Path << "\"";
  cmd << " name=\"" << engine2Name << "\"";

  // Common settings
  cmd << " -each proto=uci tc=" << matchConfig.timeControl;

  // Rounds
  cmd << " -rounds " << matchConfig.rounds;

  // Opening book
  if (!matchConfig.openingBook.empty()) {
    cmd << " -openings file=\"" << matchConfig.openingBook << "\" format=pgn order=random";
  }

  // Output PGN
  cmd << " -pgnout \"" << matchConfig.outputPgn << "\"";

  // Wait time between games (milliseconds)
  cmd << " -wait 1000";

  // Concurrency (number of games to run in parallel)
  // Note: concurrency > 1 can make results non-deterministic due to thread scheduling
  cmd << " -concurrency " << matchConfig.concurrency;


  return cmd.str();
}

bool MatchRunner::executeCutechess(const std::string& command, std::string& output) {
#ifdef _WIN32
  // Windows implementation using _popen
  // Wrap command in cmd.exe /c to properly handle paths with spaces
  const std::string windowsCommand = "cmd.exe /c \"" + command + "\"";
  FILE* pipe = _popen(windowsCommand.c_str(), "r");
  if (!pipe) {
    throw std::runtime_error("Failed to execute cutechess-cli: _popen failed");
  }

  // Read output
  std::array<char, 256> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
    // Print live output
    std::cout << buffer.data();
  }

  const int returnCode = _pclose(pipe);
  return returnCode == 0;
#else
  // Linux implementation using popen
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) {
    throw std::runtime_error("Failed to execute cutechess-cli: popen failed");
  }

  // Read output
  std::array<char, 256> buffer{};
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    output += buffer.data();
    // Print live output
    std::cout << buffer.data();
  }

  const int returnCode = pclose(pipe);
  return returnCode == 0;
#endif
}

MatchResult MatchRunner::parseOutput(const std::string& output,
                                     const MatchConfig& matchConfig,
                                     const std::string& engine1Name,
                                     const std::string& engine2Name) const {
  MatchResult result;

  // Arena metadata
  result.arenaVersion = arenaConfig.version;
  result.timestamp = getCurrentTimestamp();

  // Match identification
  result.matchName = matchConfig.name;
  result.timeControl = matchConfig.timeControl;
  result.rounds = matchConfig.rounds;

  // Engine 1 identification - use UCI name
  result.engine1Name = engine1Name;
  result.engine1Version = matchConfig.engine1Version;
  result.engine1Path = matchConfig.engine1Path;

  // Engine 2 identification - use UCI name
  result.engine2Name = engine2Name;
  result.engine2Version = matchConfig.engine2Version;
  result.engine2Path = matchConfig.engine2Path;

  // PGN path
  result.pgnPath = matchConfig.outputPgn;

  // Parse cutechess output for score line
  // Example: "Score of FrankyCPP_v1.1 vs FrankyCPP_v1.0: 65 - 15 - 20  [0.750] 100"
  // We need the main score line, not the per-color breakdown lines
  // Breakdown lines start with "..." so we need to check the context before the match
  std::regex scoreRegex(R"(Score of (.*?) vs (.*?): (\d+) - (\d+) - (\d+)\s+\[[0-9.]+\]\s+(\d+))");

  // Find all score line matches
  // cutechess-cli prints a score line after every game, so we need the LAST one (final result)
  auto words_begin = std::sregex_iterator(output.begin(), output.end(), scoreRegex);
  auto words_end = std::sregex_iterator();

  bool found = false;
  std::smatch lastValidMatch;

  for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
    const std::smatch& match = *i;

    // Get the position of this match in the output string
    const size_t matchPos = match.position();

    // Check if this line starts with "..." (breakdown line)
    // Find the start of the line containing this match
    size_t lineStart = output.rfind('\n', matchPos);
    if (lineStart == std::string::npos) {
      lineStart = 0;
    } else {
      lineStart++; // Move past the newline
    }

    // Check if the line starts with "..." (per-color breakdown or "White vs Black" line)
    bool isBreakdownLine = false;
    for (size_t j = lineStart; j < matchPos && j < output.length(); ++j) {
      if (output[j] != ' ' && output[j] != '\t') {
        if (output.substr(j, 3) == "...") {
          isBreakdownLine = true;
        }
        break;
      }
    }

    if (!isBreakdownLine) {
      // This is a main score line - keep it (will be updated by later matches)
      lastValidMatch = match;
      found = true;
      // DON'T break - continue to find the LAST valid match
    }
  }

  if (found) {
    // Use the LAST valid match (final result after all games)
    result.engine1Wins = std::stoi(lastValidMatch[3].str());
    result.engine2Wins = std::stoi(lastValidMatch[4].str());
    result.draws = std::stoi(lastValidMatch[5].str());

    // Calculate scores (win=1, draw=0.5, loss=0)
    const int totalGames = result.engine1Wins + result.draws + result.engine2Wins;
    result.engine1Score = result.engine1Wins + result.draws * 0.5;
    result.engine2Score = result.engine2Wins + result.draws * 0.5;

    // Calculate ELO difference
    const double score = result.engine1Score / totalGames;
    result.eloDifference = calculateEloDifference(score, totalGames);

  } else {
    throw std::runtime_error("Failed to parse cutechess-cli output: score line not found");
  }

  return result;
}

double MatchRunner::calculateEloDifference(const double score, const int games) {
  // Avoid division by zero and invalid log values
  if (score <= 0.0 || score >= 1.0 || games == 0) {
    return 0.0;
  }

  // Standard ELO formula: ELO_diff = -400 * log10(1/score - 1)
  const double eloDiff = -400.0 * std::log10(1.0 / score - 1.0);

  return eloDiff;
}

void MatchRunner::validateMatchConfig(const MatchConfig& matchConfig) {
  // Check cutechess-cli exists
  if (!std::filesystem::exists(matchConfig.cutechessPath)) {
    throw std::runtime_error("cutechess-cli not found: " + matchConfig.cutechessPath);
  }

  // Check engine 1 exists
  if (!std::filesystem::exists(matchConfig.engine1Path)) {
    throw std::runtime_error("Engine 1 not found: " + matchConfig.engine1Path);
  }

  // Check engine 2 exists
  if (!std::filesystem::exists(matchConfig.engine2Path)) {
    throw std::runtime_error("Engine 2 not found: " + matchConfig.engine2Path);
  }

  // Check opening book exists (if specified)
  if (!matchConfig.openingBook.empty() && !std::filesystem::exists(matchConfig.openingBook)) {
    throw std::runtime_error("Opening book not found: " + matchConfig.openingBook);
  }

  // Ensure output directory exists
  const std::filesystem::path outputPath(matchConfig.outputPgn);
  if (outputPath.has_parent_path()) {
    std::filesystem::create_directories(outputPath.parent_path());
  }
}

std::string MatchRunner::getUciEngineName(const std::string& enginePath) {
  // Start the engine briefly just to get its UCI name
  // This validates the engine works and gets the canonical name
  try {
    UCIEngine engine(enginePath);
    std::string name = engine.getEngineName();
    if (name.empty()) {
      throw std::runtime_error("Engine returned empty name");
    }
    return name;
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to get UCI name from engine '" + enginePath + "': " + e.what());
  }
}

std::string MatchRunner::extractEngineName(const std::string& enginePath) {
  const std::filesystem::path path(enginePath);
  std::string filename = path.stem().string(); // Get filename without extension
  // Normalize: replace underscores with spaces to match UCI naming convention
  // (UCI "id name" typically uses spaces, e.g., "FrankyCPP v1.1")
  std::ranges::replace(filename, '_', ' ');
  return filename;
}

std::string MatchRunner::getCurrentTimestamp() {
  const auto now = system_clock::now();
  auto time_t = system_clock::to_time_t(now);
  std::tm tm{};

#ifdef _WIN32
  gmtime_s(&tm, &time_t);
#else
  gmtime_r(&time_t, &tm);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

} // namespace arena
