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

#ifndef FRANKYCPP_ENGINE_ARENA_MATCHRUNNER_H
#define FRANKYCPP_ENGINE_ARENA_MATCHRUNNER_H

//=============================================================================
// MatchRunner.h - Engine Arena Match Execution via cutechess-cli
//=============================================================================
//
// MatchRunner executes engine-vs-engine matches using cutechess-cli and
// parses the results for version comparison.
// Depends on: ArenaConfig.h, ArenaResults.h
//
// Responsibilities:
//   - Build cutechess-cli command line from MatchConfig
//   - Execute cutechess-cli subprocess and capture output
//   - Parse cutechess results (W/D/L counts)
//   - Calculate ELO difference from match score
//   - Return structured MatchResult for persistence
//
// cutechess-cli Integration:
//   cutechess-cli is an external tool (https://github.com/cutechess/cutechess)
//   that manages chess matches between UCI engines. We invoke it as a subprocess
//   and parse its textual output.
//
//   Typical cutechess-cli command:
//     cutechess-cli.exe
//       -engine cmd=engine1.exe name="Engine1"
//       -engine cmd=engine2.exe name="Engine2"
//       -each proto=uci tc=10+0.1
//       -rounds 100
//       -openings file=book.pgn format=pgn order=random
//       -pgnout output.pgn
//       -wait 1000
//
// Output Parsing:
//   cutechess-cli prints match statistics at the end:
//   "Score of Engine1 vs Engine2: 65 - 15 - 20  [0.750]"
//   Regex: "Score of (.*) vs (.*): (\d+) - (\d+) - (\d+)"
//
// ELO Calculation:
//   ELO difference = -400 * log10(1/score - 1)
//   where score = (wins + 0.5*draws) / totalGames
//   Example: 0.750 score => ~174 ELO advantage
//
// Error Handling:
//   - Throws std::runtime_error if cutechess-cli not found
//   - Throws std::runtime_error if cutechess execution fails
//   - Throws std::runtime_error if output parsing fails
//   - Validates all paths exist before execution
//
// Thread Safety:
//   - Not thread-safe (runs single match at a time)
//   - Use separate MatchRunner instances for parallel matches
//
// Usage:
//   MatchRunner runner(config);
//   MatchResult result = runner.runMatch(config.matches[0]);
//   std::cout << "ELO difference: " << result.eloDifference << std::endl;
//
// Multiple Matches:
//   auto allResults = runner.runAllMatches();
//   for (const auto& result : allResults) {
//     std::cout << result.matchName << ": " << result.engine1Score << " - "
//               << result.engine2Score << std::endl;
//   }
//
//=============================================================================

#include "ArenaConfig.h"
#include "ArenaResults.h"
#include "../common/gtest_friends.h"

#include <string>
#include <vector>

namespace arena {

/// State of a match in progress (for resumption)
struct MatchState {
  std::string matchName;          ///< Match identifier
  int totalRounds = 0;            ///< Total rounds configured
  int completedRounds = 0;        ///< Rounds completed so far
  int engine1Wins = 0;            ///< Engine 1 wins
  int engine2Wins = 0;            ///< Engine 2 wins
  int draws = 0;                  ///< Draw count
  std::string engine1Name;        ///< Engine 1 UCI name
  std::string engine2Name;        ///< Engine 2 UCI name
  std::string timestamp;          ///< Last update timestamp
};

/// Executes engine-vs-engine matches via cutechess-cli
class MatchRunner {
  // Allow test classes to access private methods
  FRIEND_TEST(MatchRunnerParseTest, StandardScoreLine_ParsedCorrectly);
  FRIEND_TEST(MatchRunnerParseTest, BreakdownLines_Skipped);
  FRIEND_TEST(MatchRunnerParseTest, MultipleScoreLines_UsesLast);
  FRIEND_TEST(MatchRunnerParseTest, NoScoreLine_ThrowsException);
  FRIEND_TEST(MatchRunnerParseTest, EmptyOutput_ThrowsException);
  FRIEND_TEST(MatchRunnerParseTest, AllDraws_ParsedCorrectly);
  FRIEND_TEST(MatchRunnerParseTest, DecisiveResult_ParsedCorrectly);
  FRIEND_TEST(MatchRunnerParseTest, SingleGame_ParsedCorrectly);
  FRIEND_TEST(MatchRunnerParseTest, LargeNumbers_ParsedCorrectly);
  FRIEND_TEST(MatchRunnerParseTest, EngineNamesWithSpaces_ParsedCorrectly);
  FRIEND_TEST(MatchRunnerParseTest, MetadataPopulated_Correctly);
  FRIEND_TEST(MatchRunnerParseTest, EloCalculation_Draw);
  FRIEND_TEST(MatchRunnerParseTest, EloCalculation_Winning);
  // State file tests for match resumption
  FRIEND_TEST(MatchRunnerStateTest, GetStateFilePath_ReturnsCorrectPath);
  FRIEND_TEST(MatchRunnerStateTest, SaveAndLoadMatchState_RoundTrip);
  FRIEND_TEST(MatchRunnerStateTest, LoadMatchState_NonexistentFile);
  FRIEND_TEST(MatchRunnerStateTest, DeleteMatchState_RemovesFile);

public:
  /// Creates a MatchRunner with the given configuration
  /// @param config Arena configuration containing match definitions
  explicit MatchRunner(const ArenaConfig& config);

  /// Runs a single match and returns detailed results
  /// @param matchConfig Match configuration
  /// @return MatchResult with game outcomes and ELO calculation
  /// @throws std::runtime_error if cutechess-cli not found or execution fails
  MatchResult runMatch(const MatchConfig& matchConfig) const;

  /// Runs all configured matches sequentially
  /// @return Vector of MatchResult, one per configured match
  /// @throws std::runtime_error if any match fails
  std::vector<MatchResult> runAllMatches() const;

private:
  const ArenaConfig& arenaConfig; ///< Reference to arena configuration

  /// Builds cutechess-cli command line from match configuration
  /// @param matchConfig Match configuration
  /// @param engine1Name UCI name of engine 1
  /// @param engine2Name UCI name of engine 2
  /// @param rounds Number of rounds to play (may differ from config if resuming)
  /// @return Command line string for subprocess execution
  std::string buildCutechessCommand(const MatchConfig& matchConfig,
                                    const std::string& engine1Name,
                                    const std::string& engine2Name,
                                    int rounds) const;

  /// Executes cutechess-cli command and captures output
  /// @param command Full cutechess-cli command line
  /// @param output [out] Captured stdout/stderr from cutechess
  /// @return true if execution succeeded, false otherwise
  static bool executeCutechess(const std::string& command, std::string& output);

  /// Parses cutechess-cli output for match results
  /// @param output cutechess-cli stdout/stderr text
  /// @param matchConfig Original match configuration
  /// @param engine1Name UCI name of engine 1
  /// @param engine2Name UCI name of engine 2
  /// @return Parsed MatchResult structure
  /// @throws std::runtime_error if parsing fails
  MatchResult parseOutput(const std::string& output,
                          const MatchConfig& matchConfig,
                          const std::string& engine1Name,
                          const std::string& engine2Name) const;

  /// Calculates ELO difference from match score
  /// @param score Match score for engine1 (0.0 to 1.0)
  /// @param games Total number of games played
  /// @return ELO rating difference (positive = engine1 stronger)
  static double calculateEloDifference(double score, int games);

  /// Validates that all required files exist before match execution
  /// @param matchConfig Match configuration to validate
  /// @throws std::runtime_error if any required file is missing
  static void validateMatchConfig(const MatchConfig& matchConfig);

  /// Gets UCI engine name by starting the engine and reading "id name" response
  /// @param enginePath Full path to engine executable
  /// @return Engine name from UCI protocol (e.g., "FrankyCPP v1.1")
  /// @throws std::runtime_error if engine fails to start or respond
  static std::string getUciEngineName(const std::string& enginePath, const std::string& uciOptions = "");

  /// Extracts engine name from path (fallback if UCI fails)
  /// @param enginePath Full path to engine executable
  /// @return Engine name derived from filename
  static std::string extractEngineName(const std::string& enginePath);

  /// Generates current timestamp in ISO 8601 format
  /// @return Timestamp string (e.g., "2026-02-01T14:30:22Z")
  static std::string getCurrentTimestamp();

  /// Gets the state file path for a match
  /// @param matchConfig Match configuration
  /// @return Path to state file (e.g., results/matches/.state/matchname.state.json)
  static std::string getStateFilePath(const MatchConfig& matchConfig);

  /// Loads match state from file if it exists
  /// @param stateFilePath Path to state file
  /// @param state [out] Loaded state
  /// @return true if state was loaded, false if no state file exists
  static bool loadMatchState(const std::string& stateFilePath, MatchState& state);

  /// Saves match state to file
  /// @param stateFilePath Path to state file
  /// @param state State to save
  static void saveMatchState(const std::string& stateFilePath, const MatchState& state);

  /// Deletes match state file (called when match completes)
  /// @param stateFilePath Path to state file
  static void deleteMatchState(const std::string& stateFilePath);
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_MATCHRUNNER_H
