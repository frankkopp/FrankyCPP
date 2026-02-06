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
// MatchRunner_Test.cpp - Unit Tests for MatchRunner Output Parsing
//=============================================================================
//
// Tests for parsing cutechess-cli output:
//   - Standard score line parsing
//   - Multiple score lines (intermediate vs final)
//   - Breakdown lines (should be skipped)
//   - Edge cases and malformed output
//
//=============================================================================

#include "engine_arena/MatchRunner.h"
#include "engine_arena/ArenaConfig.h"

#include <gtest/gtest.h>

namespace arena {

class MatchRunnerParseTest : public ::testing::Test {
protected:
  ArenaConfig arenaConfig;
  MatchConfig matchConfig;

  void SetUp() override {
    // Set up minimal arena config
    arenaConfig.version = "v1.1";
    arenaConfig.resultsDir = "./test_results";
    arenaConfig.debugMode = false;

    // Set up minimal match config with all required fields
    matchConfig.name = "TestMatch";
    matchConfig.engine1Path = "path/to/engine1.exe";
    matchConfig.engine2Path = "path/to/engine2.exe";
    matchConfig.engine1Version = "1.0";
    matchConfig.engine2Version = "2.0";
    matchConfig.cutechessPath = "cutechess-cli.exe";
    matchConfig.openingBook = "";
    matchConfig.timeControl = "60+0.6";
    matchConfig.rounds = 100;
    matchConfig.concurrency = 1;
    matchConfig.outputPgn = "output.pgn";
  }
};

//=============================================================================
// Score Line Parsing Tests
//=============================================================================

TEST_F(MatchRunnerParseTest, StandardScoreLine_ParsedCorrectly) {
  MatchRunner runner(arenaConfig);

  std::string output = R"(
Started game 1 of 100 (engine1 vs engine2)
Finished game 1 (engine1 wins): engine1 wins by checkmate
Score of engine1 vs engine2: 1 - 0 - 0  [1.000] 1

Started game 2 of 100 (engine2 vs engine1)
Finished game 2 (draw): Draw by stalemate
Score of engine1 vs engine2: 1 - 0 - 1  [0.750] 2

... (more games)

Score of engine1 vs engine2: 65 - 15 - 20  [0.750] 100
)";

  MatchResult result = runner.parseOutput(output, matchConfig);

  EXPECT_EQ(result.engine1Wins, 65);
  EXPECT_EQ(result.engine2Wins, 15);
  EXPECT_EQ(result.draws, 20);
  EXPECT_DOUBLE_EQ(result.engine1Score, 75.0);  // 65 + 20*0.5
  EXPECT_DOUBLE_EQ(result.engine2Score, 25.0);  // 15 + 20*0.5
}

TEST_F(MatchRunnerParseTest, BreakdownLines_Skipped) {
  MatchRunner runner(arenaConfig);

  // cutechess-cli outputs per-color breakdown lines starting with "..."
  std::string output = R"(
Score of engine1 vs engine2: 65 - 15 - 20  [0.750] 100
...engine1 playing White: 35 - 5 - 10  [0.800] 50
...engine1 playing Black: 30 - 10 - 10  [0.700] 50
)";

  MatchResult result = runner.parseOutput(output, matchConfig);

  // Should use the main score line, not the breakdown lines
  EXPECT_EQ(result.engine1Wins, 65);
  EXPECT_EQ(result.engine2Wins, 15);
  EXPECT_EQ(result.draws, 20);
}

TEST_F(MatchRunnerParseTest, MultipleScoreLines_UsesLast) {
  MatchRunner runner(arenaConfig);

  // Multiple score lines from intermediate results - should use the last one
  std::string output = R"(
Score of engine1 vs engine2: 1 - 0 - 0  [1.000] 1
Score of engine1 vs engine2: 5 - 3 - 2  [0.600] 10
Score of engine1 vs engine2: 50 - 40 - 10  [0.550] 100
)";

  MatchResult result = runner.parseOutput(output, matchConfig);

  // Should use the last (final) score line
  EXPECT_EQ(result.engine1Wins, 50);
  EXPECT_EQ(result.engine2Wins, 40);
  EXPECT_EQ(result.draws, 10);
}

TEST_F(MatchRunnerParseTest, NoScoreLine_ThrowsException) {
  MatchRunner runner(arenaConfig);

  std::string output = R"(
Started game 1 of 100 (engine1 vs engine2)
Error: Engine crashed
)";

  EXPECT_THROW(runner.parseOutput(output, matchConfig), std::runtime_error);
}

TEST_F(MatchRunnerParseTest, EmptyOutput_ThrowsException) {
  MatchRunner runner(arenaConfig);

  std::string output = "";

  EXPECT_THROW(runner.parseOutput(output, matchConfig), std::runtime_error);
}

TEST_F(MatchRunnerParseTest, AllDraws_ParsedCorrectly) {
  MatchRunner runner(arenaConfig);

  std::string output = R"(
Score of engine1 vs engine2: 0 - 0 - 100  [0.500] 100
)";

  MatchResult result = runner.parseOutput(output, matchConfig);

  EXPECT_EQ(result.engine1Wins, 0);
  EXPECT_EQ(result.engine2Wins, 0);
  EXPECT_EQ(result.draws, 100);
  EXPECT_DOUBLE_EQ(result.engine1Score, 50.0);
  EXPECT_DOUBLE_EQ(result.engine2Score, 50.0);
}

TEST_F(MatchRunnerParseTest, DecisiveResult_ParsedCorrectly) {
  MatchRunner runner(arenaConfig);

  std::string output = R"(
Score of engine1 vs engine2: 100 - 0 - 0  [1.000] 100
)";

  MatchResult result = runner.parseOutput(output, matchConfig);

  EXPECT_EQ(result.engine1Wins, 100);
  EXPECT_EQ(result.engine2Wins, 0);
  EXPECT_EQ(result.draws, 0);
  EXPECT_DOUBLE_EQ(result.engine1Score, 100.0);
  EXPECT_DOUBLE_EQ(result.engine2Score, 0.0);
}

TEST_F(MatchRunnerParseTest, SingleGame_ParsedCorrectly) {
  MatchRunner runner(arenaConfig);

  std::string output = R"(
Score of engine1 vs engine2: 1 - 0 - 0  [1.000] 1
)";

  MatchResult result = runner.parseOutput(output, matchConfig);

  EXPECT_EQ(result.engine1Wins, 1);
  EXPECT_EQ(result.engine2Wins, 0);
  EXPECT_EQ(result.draws, 0);
}

TEST_F(MatchRunnerParseTest, LargeNumbers_ParsedCorrectly) {
  MatchRunner runner(arenaConfig);

  std::string output = R"(
Score of engine1 vs engine2: 5000 - 4000 - 1000  [0.550] 10000
)";

  MatchResult result = runner.parseOutput(output, matchConfig);

  EXPECT_EQ(result.engine1Wins, 5000);
  EXPECT_EQ(result.engine2Wins, 4000);
  EXPECT_EQ(result.draws, 1000);
}

TEST_F(MatchRunnerParseTest, EngineNamesWithSpaces_ParsedCorrectly) {
  MatchRunner runner(arenaConfig);

  std::string output = R"(
Score of FrankyCPP v1.1 vs Stockfish 16 Dev: 65 - 15 - 20  [0.750] 100
)";

  MatchResult result = runner.parseOutput(output, matchConfig);

  EXPECT_EQ(result.engine1Wins, 65);
  EXPECT_EQ(result.engine2Wins, 15);
  EXPECT_EQ(result.draws, 20);
}

TEST_F(MatchRunnerParseTest, MetadataPopulated_Correctly) {
  MatchRunner runner(arenaConfig);

  std::string output = R"(
Score of engine1 vs engine2: 65 - 15 - 20  [0.750] 100
)";

  MatchResult result = runner.parseOutput(output, matchConfig);

  // Check metadata is populated from config
  EXPECT_EQ(result.arenaVersion, "v1.1");
  EXPECT_EQ(result.matchName, "TestMatch");
  EXPECT_EQ(result.timeControl, "60+0.6");
  EXPECT_EQ(result.rounds, 100);
  EXPECT_EQ(result.engine1Version, "1.0");
  EXPECT_EQ(result.engine2Version, "2.0");
  EXPECT_EQ(result.pgnPath, "output.pgn");
}

//=============================================================================
// ELO Calculation Tests
//=============================================================================

TEST_F(MatchRunnerParseTest, EloCalculation_Draw) {
  MatchRunner runner(arenaConfig);

  // 50% score should give ~0 ELO difference
  std::string output = R"(
Score of engine1 vs engine2: 25 - 25 - 50  [0.500] 100
)";

  MatchResult result = runner.parseOutput(output, matchConfig);

  // ELO difference should be close to 0 for 50% score
  EXPECT_NEAR(result.eloDifference, 0.0, 1.0);
}

TEST_F(MatchRunnerParseTest, EloCalculation_Winning) {
  MatchRunner runner(arenaConfig);

  // 75% score should give positive ELO
  std::string output = R"(
Score of engine1 vs engine2: 65 - 15 - 20  [0.750] 100
)";

  MatchResult result = runner.parseOutput(output, matchConfig);

  // ELO difference should be positive and significant
  EXPECT_GT(result.eloDifference, 100.0);
}

} // namespace arena
