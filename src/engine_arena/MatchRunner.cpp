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
#include "common/TimeUtils.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

// Suppress false positive Clangd warning about nlohmann/json template instantiation
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wc++20-extensions"
#endif

#include <nlohmann/json.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <regex>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#else
#include <cstdio>
#endif

namespace arena {

  using common::isoTimestamp;
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;
  using std::chrono::system_clock;

  MatchRunner::MatchRunner(const ArenaConfig& config)
      : arenaConfig(config) {
  }

  MatchResult MatchRunner::runMatch(const MatchConfig& matchConfig) const {
    // Calculate batch size: use configured value, or auto-calculate from concurrency
    // Auto: max(2, concurrency) rounded up to next even number
    int batchSize = matchConfig.batchSize;
    if (batchSize == 0) {
      // Auto-calculate: at least 2, at least concurrency, must be even
      batchSize = std::max(2, matchConfig.concurrency);
      if (batchSize % 2 != 0) {
        batchSize++; // Round up to even
      }
    }

    std::cout << "\n==================================================================" << std::endl;
    std::cout << "Running Match: " << matchConfig.name << std::endl;
    std::cout << "==================================================================" << std::endl;
    std::cout << "Engine 1:       " << matchConfig.engine1Path << std::endl;
    std::cout << "Engine 2:       " << matchConfig.engine2Path << std::endl;
    std::cout << "Opening Book:   " << matchConfig.openingBook << std::endl;
    std::cout << "Time Control:   " << matchConfig.timeControl << std::endl;
    std::cout << "Rounds:         " << matchConfig.rounds << std::endl;
    std::cout << "Concurrency:    " << matchConfig.concurrency << std::endl;
    std::cout << "Batch Size:     " << batchSize << (matchConfig.batchSize == 0 ? " (auto)" : "") << std::endl;
    std::cout << "Output PGN:     " << matchConfig.outputPgn << std::endl;


    // Validate configuration first
    validateMatchConfig(matchConfig);

    // Get UCI engine names by briefly starting each engine
    std::cout << "\nValidating engines and getting UCI names..." << std::endl;
    if (!matchConfig.engine1Options.empty()) {
      std::cout << "  Engine 1 UCI options: " << matchConfig.engine1Options << std::endl;
    }
    const std::string engine1Name = getUciEngineName(matchConfig.engine1Path, matchConfig.engine1Options);
    std::cout << "  Engine 1: " << engine1Name << std::endl;
    if (!matchConfig.engine2Options.empty()) {
      std::cout << "  Engine 2 UCI options: " << matchConfig.engine2Options << std::endl;
    }
    const std::string engine2Name = getUciEngineName(matchConfig.engine2Path, matchConfig.engine2Options);
    std::cout << "  Engine 2: " << engine2Name << std::endl;

    // Print the full cutechess-cli command for copy-paste reproduction
    const std::string fullCommand = buildCutechessCommand(matchConfig, engine1Name, engine2Name, matchConfig.rounds);
    std::cout << "\ncutechess-cli command (copy-paste ready):\n" << fullCommand << "\n" << std::endl;

    // Check for saved state (resumable match)
    const std::string stateFilePath = getStateFilePath(matchConfig);
    MatchState currentState;
    currentState.matchName       = matchConfig.name;
    currentState.totalRounds     = matchConfig.rounds;
    currentState.completedRounds = 0;
    currentState.engine1Wins     = 0;
    currentState.engine2Wins     = 0;
    currentState.draws           = 0;
    currentState.engine1Name     = engine1Name;
    currentState.engine2Name     = engine2Name;

    if (loadMatchState(stateFilePath, currentState)) {
      // Validate state matches current config
      if (currentState.totalRounds == matchConfig.rounds) {
        std::cout << "\n*** RESUMING MATCH ***" << std::endl;
        std::cout << "  State file:        " << stateFilePath << std::endl;
        std::cout << "  Completed rounds:  " << currentState.completedRounds << std::endl;
        std::cout << "  Remaining rounds:  " << (matchConfig.rounds - currentState.completedRounds) << std::endl;
        std::cout << "  Current score:     " << currentState.engine1Wins << " - "
                  << currentState.engine2Wins << " - " << currentState.draws
                  << " (W-L-D)" << std::endl;
      }
      else {
        std::cout << "\n  State file found but totalRounds mismatch - starting fresh" << std::endl;
        deleteMatchState(stateFilePath);
        currentState.completedRounds = 0;
        currentState.engine1Wins     = 0;
        currentState.engine2Wins     = 0;
        currentState.draws           = 0;
      }
    }

    // Check if already complete
    if (currentState.completedRounds >= matchConfig.rounds) {
      std::cout << "\n*** MATCH ALREADY COMPLETE ***" << std::endl;
      std::cout << "  All " << matchConfig.rounds << " games have been played." << std::endl;
      std::cout << "  Delete state file to restart: " << stateFilePath << std::endl;

      // Build result from saved state
      MatchResult result;
      result.arenaVersion   = arenaConfig.version;
      result.timestamp      = isoTimestamp();
      result.matchName      = matchConfig.name;
      result.tag            = matchConfig.tag;
      result.timeControl    = matchConfig.timeControl;
      result.rounds         = matchConfig.rounds;
      result.engine1Name    = currentState.engine1Name;
      result.engine1Version = matchConfig.engine1Version;
      result.engine1Path    = matchConfig.engine1Path;
      result.engine2Name    = currentState.engine2Name;
      result.engine2Version = matchConfig.engine2Version;
      result.engine2Path    = matchConfig.engine2Path;
      result.pgnPath        = matchConfig.outputPgn;
      result.engine1Wins    = currentState.engine1Wins;
      result.engine2Wins    = currentState.engine2Wins;
      result.draws          = currentState.draws;

      const int totalGames = result.engine1Wins + result.draws + result.engine2Wins;
      result.engine1Score  = result.engine1Wins + result.draws * 0.5;
      result.engine2Score  = result.engine2Wins + result.draws * 0.5;
      if (totalGames > 0) {
        const double score   = result.engine1Score / totalGames;
        result.eloDifference = calculateEloDifference(score, totalGames);
      }
      result.durationMs = 0;

      // Delete state file since match is complete
      deleteMatchState(stateFilePath);

      return result;
    }

    // Calculate batches (last batch may be smaller if rounds not divisible by batchSize)
    const int remainingRounds  = matchConfig.rounds - currentState.completedRounds;
    const int totalBatches     = (matchConfig.rounds + batchSize - 1) / batchSize;
    const int completedBatches = currentState.completedRounds / batchSize;
    const int remainingBatches = (remainingRounds + batchSize - 1) / batchSize;

    std::cout << "\n  Running " << remainingBatches << " batches of up to " << batchSize << " games each..." << std::endl;
    std::cout << std::endl;

    const auto matchStartTime = system_clock::now();

    // Run batches
    for (int batch = completedBatches; batch < totalBatches; ++batch) {
      const int batchNumber    = batch + 1;
      const int gamesCompleted = currentState.completedRounds;
      const int gamesLeft      = matchConfig.rounds - gamesCompleted;
      const int currentBatch   = std::min(batchSize, gamesLeft);

      std::cout << "------------------------------------------------------------------" << std::endl;
      std::cout << "Batch " << batchNumber << "/" << totalBatches
                << " (games " << (gamesCompleted + 1) << "-" << (gamesCompleted + currentBatch)
                << " of " << matchConfig.rounds << ")"
                << "  [" << matchConfig.name;
      if (!matchConfig.tag.empty()) {
        std::cout << " | " << matchConfig.tag;
      }
      std::cout << "]" << std::endl;
      std::cout << "  Current score: " << currentState.engine1Wins << " - "
                << currentState.engine2Wins << " - " << currentState.draws << std::endl;

      // Build command for this batch (last batch may be smaller)
      const std::string command = buildCutechessCommand(matchConfig, engine1Name, engine2Name, currentBatch);

      // Execute cutechess-cli for this batch
      std::string output;
      if (!executeCutechess(command, output)) {
        // Save state before throwing so we can resume
        currentState.timestamp = isoTimestamp();
        saveMatchState(stateFilePath, currentState);
        throw std::runtime_error("cutechess-cli execution failed - state saved for resumption");
      }

      // Parse batch result
      MatchResult batchResult = parseOutput(output, matchConfig, engine1Name, engine2Name);

      // Update cumulative state
      currentState.completedRounds += currentBatch;
      currentState.engine1Wins += batchResult.engine1Wins;
      currentState.engine2Wins += batchResult.engine2Wins;
      currentState.draws += batchResult.draws;
      currentState.timestamp = isoTimestamp();

      // Save state after each batch (enables resume on interrupt)
      saveMatchState(stateFilePath, currentState);

      std::cout << "  Batch result: +" << batchResult.engine1Wins << " - "
                << batchResult.engine2Wins << " - " << batchResult.draws << std::endl;
    }

    const auto matchEndTime  = system_clock::now();
    const auto totalDuration = duration_cast<milliseconds>(matchEndTime - matchStartTime).count();

    // Build final result
    MatchResult result;
    result.arenaVersion   = arenaConfig.version;
    result.timestamp      = isoTimestamp();
    result.matchName      = matchConfig.name;
    result.tag            = matchConfig.tag;
    result.timeControl    = matchConfig.timeControl;
    result.rounds         = matchConfig.rounds;
    result.engine1Name    = engine1Name;
    result.engine1Version = matchConfig.engine1Version;
    result.engine1Path    = matchConfig.engine1Path;
    result.engine2Name    = engine2Name;
    result.engine2Version = matchConfig.engine2Version;
    result.engine2Path    = matchConfig.engine2Path;
    result.pgnPath        = matchConfig.outputPgn;
    result.engine1Wins    = currentState.engine1Wins;
    result.engine2Wins    = currentState.engine2Wins;
    result.draws          = currentState.draws;
    result.durationMs     = totalDuration;

    const int totalGames = result.engine1Wins + result.draws + result.engine2Wins;
    result.engine1Score  = result.engine1Wins + result.draws * 0.5;
    result.engine2Score  = result.engine2Wins + result.draws * 0.5;
    if (totalGames > 0) {
      const double score   = result.engine1Score / totalGames;
      result.eloDifference = calculateEloDifference(score, totalGames);
    }

    // Match complete - delete state file
    deleteMatchState(stateFilePath);

    std::cout << "\n==================================================================" << std::endl;
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
                                                 const std::string& engine2Name,
                                                 const int rounds) const {
    std::ostringstream cmd;

    // Use quoted path for cutechess-cli
    cmd << "\"" << matchConfig.cutechessPath << "\"";

    // Debug mode (prints engine I/O communication)
    // Note: cutechess-cli requires "-debug all" not just "-debug"
    if (arenaConfig.debugMode) {
      cmd << " -debug all";
    }

    // Engine 1 - use UCI name for identification
    // Set dir= to the engine's parent directory so it resolves config/eval.yaml
    // relative to its own location, not the arena's CWD.
    cmd << " -engine cmd=\"" << matchConfig.engine1Path << "\"";
    cmd << " name=\"" << engine1Name << "\"";
    const auto engine1Dir = std::filesystem::path(matchConfig.engine1Path).parent_path();
    if (!engine1Dir.empty()) {
      cmd << " dir=\"" << engine1Dir.string() << "\"";
    }
    // Add UCI options for engine 1 (format: option.Name=Value)
    if (!matchConfig.engine1Options.empty()) {
      // Parse semicolon-separated options and convert to cutechess format
      std::istringstream iss(matchConfig.engine1Options);
      std::string option;
      while (std::getline(iss, option, ';')) {
        // Trim whitespace
        option.erase(0, option.find_first_not_of(" \t"));
        option.erase(option.find_last_not_of(" \t") + 1);
        if (!option.empty()) {
          // Convert "Name=Value" to "option.Name=Value"
          cmd << " option." << option;
        }
      }
    }

    // Engine 2 - use UCI name for identification
    // Set dir= to the engine's parent directory so it resolves config/eval.yaml
    // relative to its own location, not the arena's CWD.
    cmd << " -engine cmd=\"" << matchConfig.engine2Path << "\"";
    cmd << " name=\"" << engine2Name << "\"";
    const auto engine2Dir = std::filesystem::path(matchConfig.engine2Path).parent_path();
    if (!engine2Dir.empty()) {
      cmd << " dir=\"" << engine2Dir.string() << "\"";
    }
    // Add UCI options for engine 2 (format: option.Name=Value)
    if (!matchConfig.engine2Options.empty()) {
      // Parse semicolon-separated options and convert to cutechess format
      std::istringstream iss(matchConfig.engine2Options);
      std::string option;
      while (std::getline(iss, option, ';')) {
        // Trim whitespace
        option.erase(0, option.find_first_not_of(" \t"));
        option.erase(option.find_last_not_of(" \t") + 1);
        if (!option.empty()) {
          // Convert "Name=Value" to "option.Name=Value"
          cmd << " option." << option;
        }
      }
    }

    // Common settings applied to both engines via -each.
    // timemargin=N is a per-engine option in cutechess-cli (not a global flag).
    // It adds N ms tolerance before declaring a time loss, compensating for
    // engine post-stop overhead (joining threads, sending bestmove).
    cmd << " -each proto=uci tc=" << matchConfig.timeControl;
    if (matchConfig.timeMargin > 0) {
      cmd << " timemargin=" << matchConfig.timeMargin;
    }

    // Resign adjudication: auto-resign when losing by large margin for several moves.
    // Fishtest/OpenBench standard: movecount=3, score=600 (centipawns).
    if (matchConfig.resignMoveCount > 0) {
      cmd << " -resign movecount=" << matchConfig.resignMoveCount
          << " score=" << matchConfig.resignScore;
    }

    // Draw adjudication: declare draw after move N if score stays below threshold for M moves.
    // Fishtest/OpenBench standard: movenumber=40, movecount=8, score=10.
    if (matchConfig.drawMoveNumber > 0) {
      cmd << " -draw movenumber=" << matchConfig.drawMoveNumber
          << " movecount=" << matchConfig.drawMoveCount
          << " score=" << matchConfig.drawScore;
    }

    // Recover from engine crashes (restart engine and continue match)
    if (matchConfig.recover) {
      cmd << " -recover";
    }

    // Rounds (may be less than config if resuming a match)
    cmd << " -rounds " << rounds;

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
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string windowsCommand = "cmd.exe /c \"" + command + "\"";
    FILE* pipe                       = _popen(windowsCommand.c_str(), "r");
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
    result.timestamp    = isoTimestamp();

    // Match identification
    result.matchName   = matchConfig.name;
    result.tag         = matchConfig.tag;
    result.timeControl = matchConfig.timeControl;
    result.rounds      = matchConfig.rounds;

    // Engine 1 identification - use UCI name
    result.engine1Name    = engine1Name;
    result.engine1Version = matchConfig.engine1Version;
    result.engine1Path    = matchConfig.engine1Path;

    // Engine 2 identification - use UCI name
    result.engine2Name    = engine2Name;
    result.engine2Version = matchConfig.engine2Version;
    result.engine2Path    = matchConfig.engine2Path;

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
    auto words_end   = std::sregex_iterator();

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
      }
      else {
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
        found          = true;
        // DON'T break - continue to find the LAST valid match
      }
    }

    if (found) {
      // Use the LAST valid match (final result after all games)
      result.engine1Wins = std::stoi(lastValidMatch[3].str());
      result.engine2Wins = std::stoi(lastValidMatch[4].str());
      result.draws       = std::stoi(lastValidMatch[5].str());

      // Calculate scores (win=1, draw=0.5, loss=0)
      const int totalGames = result.engine1Wins + result.draws + result.engine2Wins;
      result.engine1Score  = result.engine1Wins + result.draws * 0.5;
      result.engine2Score  = result.engine2Wins + result.draws * 0.5;

      // Calculate ELO difference
      const double score   = result.engine1Score / totalGames;
      result.eloDifference = calculateEloDifference(score, totalGames);
    }
    else {
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

  std::string MatchRunner::getUciEngineName(const std::string& enginePath, const std::string& uciOptions) {
    // Start the engine briefly just to get its UCI name
    // This validates the engine works and gets the canonical name
    // Pass uciOptions to ensure engine initializes correctly (e.g., OwnBook=false to skip book loading)
    try {
      const UCIEngine engine(enginePath, "", false, uciOptions);
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


  std::string MatchRunner::getStateFilePath(const MatchConfig& matchConfig) {
    // State files go in results/matches/.state/
    const std::filesystem::path pgnPath(matchConfig.outputPgn);
    const std::filesystem::path stateDir = pgnPath.parent_path() / ".state";
    const std::string stateFileName      = pgnPath.stem().string() + ".state.json";
    return (stateDir / stateFileName).string();
  }

  bool MatchRunner::loadMatchState(const std::string& stateFilePath, MatchState& state) {
    if (!std::filesystem::exists(stateFilePath)) {
      return false;
    }

    std::ifstream file(stateFilePath);
    if (!file.is_open()) {
      return false;
    }

    try {
      const nlohmann::json j = nlohmann::json::parse(file);
      file.close();

      state.matchName       = j.value("matchName", "");
      state.totalRounds     = j.value("totalRounds", 0);
      state.completedRounds = j.value("completedRounds", 0);
      state.engine1Wins     = j.value("engine1Wins", 0);
      state.engine2Wins     = j.value("engine2Wins", 0);
      state.draws           = j.value("draws", 0);
      state.engine1Name     = j.value("engine1Name", "");
      state.engine2Name     = j.value("engine2Name", "");
      state.timestamp       = j.value("timestamp", "");
    } catch (const std::exception& e) {
      std::cerr << "Warning: Failed to parse match state file: " << e.what() << std::endl;
      file.close();
      return false;
    }

    return state.completedRounds > 0; // Only valid if we have some progress
  }

  void MatchRunner::saveMatchState(const std::string& stateFilePath, const MatchState& state) {
    // Ensure state directory exists
    const std::filesystem::path statePath(stateFilePath);
    if (statePath.has_parent_path()) {
      std::filesystem::create_directories(statePath.parent_path());
    }

    std::ofstream file(stateFilePath);
    if (!file.is_open()) {
      std::cerr << "Warning: Could not save match state to " << stateFilePath << std::endl;
      return;
    }

    // Build and write JSON
    const nlohmann::json j = {
      {"matchName", state.matchName},
      {"totalRounds", state.totalRounds},
      {"completedRounds", state.completedRounds},
      {"engine1Wins", state.engine1Wins},
      {"engine2Wins", state.engine2Wins},
      {"draws", state.draws},
      {"engine1Name", state.engine1Name},
      {"engine2Name", state.engine2Name},
      {"timestamp", state.timestamp}};

    file << j.dump(2) << "\n";
    file.close();
  }

  void MatchRunner::deleteMatchState(const std::string& stateFilePath) {
    if (std::filesystem::exists(stateFilePath)) {
      std::filesystem::remove(stateFilePath);
    }
  }

} // namespace arena
