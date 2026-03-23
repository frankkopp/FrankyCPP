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

#include "tuning/extractor/PositionExtractor.h"

#include "init.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace tuning;

namespace {

  /// Helper: writes a string to a temporary PGN file and returns the path.
  std::string writeTempPgn(const std::string& content, const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream ofs(path);
    ofs << content;
    ofs.close();
    return path.string();
  }

  /// Helper: reads all lines from a file.
  std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream ifs(path);
    std::string line;
    while (std::getline(ifs, line)) {
      if (!line.empty()) {
        lines.push_back(line);
      }
    }
    return lines;
  }

  /// Helper: returns a temp output file path.
  std::string tempOutputPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
  }

  // A short complete game in PGN format (Scholar's Mate)
  // 1. e4 e5 2. Bc4 Nc6 3. Qh5 Nf6?? 4. Qxf7# 1-0
  constexpr auto SCHOLARS_MATE_PGN = R"(
[Event "Test"]
[Site "Test"]
[Date "2026.03.23"]
[Round "1"]
[White "Player1"]
[Black "Player2"]
[Result "1-0"]

1. e4 e5 2. Bc4 Nc6 3. Qh5 Nf6 4. Qxf7# 1-0
)";

  // A short drawn game
  constexpr auto SHORT_DRAW_PGN = R"(
[Event "Test"]
[Site "Test"]
[Date "2026.03.23"]
[Round "1"]
[White "Player1"]
[Black "Player2"]
[Result "1/2-1/2"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O Be7 6. Re1 b5 7. Bb3 d6
8. c3 O-O 9. h3 Na5 10. Bc2 c5 11. d4 Qc7 12. Nbd2 Bd7 1/2-1/2
)";

  // A game with time forfeit termination header
  constexpr auto TIME_FORFEIT_PGN = R"(
[Event "Test"]
[Site "Test"]
[Date "2026.03.23"]
[Round "1"]
[White "Player1"]
[Black "Player2"]
[Result "1-0"]
[Termination "time forfeit"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 1-0
)";

  // A game with unknown result
  constexpr auto UNKNOWN_RESULT_PGN = R"(
[Event "Test"]
[Site "Test"]
[Date "2026.03.23"]
[Round "1"]
[White "Player1"]
[Black "Player2"]
[Result "*"]

1. e4 e5 2. Nf3 Nc6 *
)";

  // Two games: one normal, one with illegal move termination
  constexpr auto MIXED_TERMINATION_PGN = R"(
[Event "Test"]
[Site "Test"]
[Date "2026.03.23"]
[Round "1"]
[White "Player1"]
[Black "Player2"]
[Result "0-1"]

1. d4 d5 2. c4 e6 3. Nc3 Nf6 4. Bg5 Be7 5. e3 O-O 6. Nf3 Nbd7
7. Rc1 c6 8. Bd3 dxc4 9. Bxc4 Nd5 10. Bxe7 Qxe7 0-1

[Event "Test"]
[Site "Test"]
[Date "2026.03.23"]
[Round "2"]
[White "Player1"]
[Black "Player2"]
[Result "1-0"]
[Termination "illegal move"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 1-0
)";

  // A longer game to test extraction rate (Ruy Lopez)
  constexpr auto LONGER_GAME_PGN = R"(
[Event "Test"]
[Site "Test"]
[Date "2026.03.23"]
[Round "1"]
[White "Player1"]
[Black "Player2"]
[Result "1/2-1/2"]

1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O Be7 6. Re1 b5 7. Bb3 d6
8. c3 O-O 9. h3 Na5 10. Bc2 c5 11. d4 Qc7 12. Nbd2 Bd7 13. Nf1 Rfe8
14. Ne3 g6 15. dxe5 dxe5 16. Nh2 Rad8 17. Qf3 Be6 18. Nhg4 Nxg4
19. Nxg4 Bxg4 20. Qxg4 Nc4 21. Qe2 Nd6 22. Bg5 f6 23. Be3 Nb7
24. Rad1 Na5 25. Rxd8 Rxd8 1/2-1/2
)";

} // anonymous namespace

class PositionExtractorTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }

protected:
  void SetUp() override {
    // Clean up any leftover temp files
  }

  void TearDown() override {
    // Clean up temp files
    for (const auto& f : tempFiles) {
      std::filesystem::remove(f);
    }
  }

  /// Tracks temp files for cleanup.
  std::vector<std::string> tempFiles;

  /// Helper: extract with default config, return stats.
  ExtractionStats runExtraction(const std::string& pgnContent,
                                const std::string& testName,
                                const ExtractionConfig& config = ExtractionConfig{}) {
    const auto pgnPath = writeTempPgn(pgnContent, testName + ".pgn");
    const auto outPath = tempOutputPath(testName + ".txt");
    tempFiles.push_back(pgnPath);
    tempFiles.push_back(outPath);

    PositionExtractor extractor;
    extractor.extract(pgnPath, outPath, config);
    return extractor.getStats();
  }
};

// ============================================================================
// Filter 0: Game-level filtering
// ============================================================================

TEST_F(PositionExtractorTest, skipUnknownResult) {
  const auto stats = runExtraction(UNKNOWN_RESULT_PGN, "skip_unknown");
  EXPECT_EQ(stats.gamesTotal, 1);
  EXPECT_EQ(stats.gamesSkippedUnknownResult, 1);
  EXPECT_EQ(stats.gamesProcessed, 0);
  EXPECT_EQ(stats.positionsExtracted, 0);
}

TEST_F(PositionExtractorTest, skipTimeForfeit) {
  const auto stats = runExtraction(TIME_FORFEIT_PGN, "skip_time_forfeit");
  EXPECT_EQ(stats.gamesTotal, 1);
  EXPECT_EQ(stats.gamesSkippedTermination, 1);
  EXPECT_EQ(stats.gamesProcessed, 0);
  EXPECT_EQ(stats.positionsExtracted, 0);
}

TEST_F(PositionExtractorTest, skipIllegalMoveTermination) {
  const auto stats = runExtraction(MIXED_TERMINATION_PGN, "skip_illegal");
  // Two games: first is normal (0-1), second has [Termination "illegal move"]
  EXPECT_EQ(stats.gamesTotal, 2);
  EXPECT_EQ(stats.gamesSkippedTermination, 1);
  EXPECT_EQ(stats.gamesProcessed, 1);
}

TEST_F(PositionExtractorTest, noFilterWhenTerminationDisabled) {
  ExtractionConfig config;
  config.skipTermination = false;
  const auto stats = runExtraction(TIME_FORFEIT_PGN, "no_skip_term", config);
  EXPECT_EQ(stats.gamesTotal, 1);
  EXPECT_EQ(stats.gamesSkippedTermination, 0);
  EXPECT_EQ(stats.gamesProcessed, 1);
}

// ============================================================================
// Filter 1: Early move filtering
// ============================================================================

TEST_F(PositionExtractorTest, filterEarlyMoves) {
  // Scholar's Mate: 7 half-moves (4. Qxf7# is the 7th half-move)
  // With minHalfMove=4, half-moves 1-3 are filtered, 4-7 pass Filter 1
  ExtractionConfig config;
  config.minHalfMove = 4;
  const auto stats = runExtraction(SCHOLARS_MATE_PGN, "filter_early", config);
  EXPECT_EQ(stats.totalPositionsSeen, 7);
  EXPECT_EQ(stats.filteredEarlyMove, 3); // half-moves 1, 2, 3
  // Remaining 4 positions may be further filtered by check/capture filters
}

TEST_F(PositionExtractorTest, highMinHalfMoveFiltersAll) {
  ExtractionConfig config;
  config.minHalfMove = 100; // way more than 7 moves
  const auto stats = runExtraction(SCHOLARS_MATE_PGN, "filter_all_early", config);
  EXPECT_EQ(stats.totalPositionsSeen, 7);
  EXPECT_EQ(stats.filteredEarlyMove, 7);
  EXPECT_EQ(stats.positionsExtracted, 0);
}

// ============================================================================
// Filter 2: In-check filtering
// ============================================================================

TEST_F(PositionExtractorTest, filterInCheck) {
  // Scholar's Mate ends with Qxf7# — the final position is checkmate (in check)
  // Also Qh5 may give check... but in standard Scholar's Mate it doesn't give check
  // Position after Qxf7# is checkmate (in check) — should be filtered
  ExtractionConfig config;
  config.minHalfMove = 1; // Don't filter early moves
  const auto stats = runExtraction(SCHOLARS_MATE_PGN, "filter_check", config);
  // The last position (after Qxf7#) should be filtered as "in check"
  EXPECT_GT(stats.filteredInCheck, 0);
}

// ============================================================================
// Filter 3: Capture/promotion filtering
// ============================================================================

TEST_F(PositionExtractorTest, filterCaptures) {
  // LONGER_GAME_PGN has several non-checking captures (dxe5, Nxg4, Bxg4, Qxg4, Rxd8)
  // Note: Scholar's Mate only capture (Qxf7#) also gives check, so filter 2 catches it first
  ExtractionConfig config;
  config.minHalfMove    = 1;
  config.skipCaptures   = true;
  config.skipPromotions = true;
  const auto stats = runExtraction(LONGER_GAME_PGN, "filter_captures", config);
  EXPECT_GT(stats.filteredCapture, 0);
}

TEST_F(PositionExtractorTest, noFilterWhenCapturesDisabled) {
  ExtractionConfig configOff;
  configOff.minHalfMove    = 1;
  configOff.skipCaptures   = false;
  configOff.skipPromotions = false;
  const auto statsOff = runExtraction(LONGER_GAME_PGN, "no_filter_cap", configOff);

  ExtractionConfig configOn;
  configOn.minHalfMove    = 1;
  configOn.skipCaptures   = true;
  configOn.skipPromotions = true;
  const auto statsOn = runExtraction(LONGER_GAME_PGN, "filter_cap2", configOn);

  // With capture filter on, fewer positions should be extracted
  EXPECT_GT(statsOff.positionsExtracted, statsOn.positionsExtracted);
  EXPECT_GT(statsOn.filteredCapture, 0);
}

// ============================================================================
// Filter 4: Trivial endgame filtering
// ============================================================================

TEST_F(PositionExtractorTest, filterEndgame) {
  // Normal games from opening should never hit the endgame filter
  ExtractionConfig config;
  config.minHalfMove = 1;
  config.minPieces   = 6;
  const auto stats = runExtraction(SCHOLARS_MATE_PGN, "filter_endgame", config);
  // Scholar's Mate doesn't reach an endgame — filter should not trigger
  EXPECT_EQ(stats.filteredEndgame, 0);
}

TEST_F(PositionExtractorTest, highMinPiecesFiltersMore) {
  ExtractionConfig config;
  config.minHalfMove = 1;
  config.minPieces   = 40; // More than 32 pieces ever exist — filters everything
  const auto stats = runExtraction(SCHOLARS_MATE_PGN, "filter_endgame_high", config);
  // Everything not already filtered by check/capture should be filtered by endgame
  EXPECT_EQ(stats.positionsExtracted, 0);
}

// ============================================================================
// Output format
// ============================================================================

TEST_F(PositionExtractorTest, outputFormatCorrect) {
  const auto pgnPath = writeTempPgn(SHORT_DRAW_PGN, "output_format.pgn");
  const auto outPath = tempOutputPath("output_format.txt");
  tempFiles.push_back(pgnPath);
  tempFiles.push_back(outPath);

  ExtractionConfig config;
  config.minHalfMove = 1;
  PositionExtractor extractor;
  extractor.extract(pgnPath, outPath, config);

  const auto lines = readLines(outPath);
  ASSERT_FALSE(lines.empty());

  // Each line should match format: <FEN> [<result>]
  for (const auto& line : lines) {
    // Must end with [0.0], [0.5], or [1.0]
    const bool hasResult = line.ends_with("[1.0]") || line.ends_with("[0.5]") || line.ends_with("[0.0]");
    EXPECT_TRUE(hasResult) << "Line doesn't end with result label: " << line;

    // FEN should have at least 4 space-separated fields
    const auto bracketPos = line.find('[');
    ASSERT_NE(bracketPos, std::string::npos);
    const auto fen = line.substr(0, bracketPos - 1); // -1 for the space before [
    int spaces = 0;
    for (const char c : fen) {
      if (c == ' ') spaces++;
    }
    EXPECT_GE(spaces, 4) << "FEN has too few fields: " << fen;
  }
}

TEST_F(PositionExtractorTest, drawLabeledCorrectly) {
  const auto pgnPath = writeTempPgn(SHORT_DRAW_PGN, "draw_label.pgn");
  const auto outPath = tempOutputPath("draw_label.txt");
  tempFiles.push_back(pgnPath);
  tempFiles.push_back(outPath);

  ExtractionConfig config;
  config.minHalfMove = 1;
  PositionExtractor extractor;
  extractor.extract(pgnPath, outPath, config);

  const auto lines = readLines(outPath);
  ASSERT_FALSE(lines.empty());
  // All lines from a drawn game should have [0.5]
  for (const auto& line : lines) {
    EXPECT_TRUE(line.ends_with("[0.5]")) << "Draw game position not labeled 0.5: " << line;
  }
}

// ============================================================================
// Stats accuracy
// ============================================================================

TEST_F(PositionExtractorTest, statsConsistency) {
  ExtractionConfig config;
  config.minHalfMove = 1;
  const auto stats = runExtraction(LONGER_GAME_PGN, "stats_consistency", config);

  // Basic consistency: all positions are either filtered or extracted
  const int totalFiltered = stats.filteredEarlyMove + stats.filteredInCheck
                            + stats.filteredCapture + stats.filteredEndgame
                            + stats.filteredQsearch;
  EXPECT_EQ(stats.totalPositionsSeen, totalFiltered + stats.positionsExtracted);

  // Game stats
  EXPECT_EQ(stats.gamesTotal, 1);
  EXPECT_EQ(stats.gamesProcessed, 1);
  EXPECT_EQ(stats.gamesDraws, 1);
}

TEST_F(PositionExtractorTest, resultDistribution) {
  // Combine two games with different results
  const std::string combined = std::string(SCHOLARS_MATE_PGN) + "\n" + SHORT_DRAW_PGN;
  const auto stats = runExtraction(combined, "result_dist");
  EXPECT_EQ(stats.gamesTotal, 2);
  EXPECT_EQ(stats.gamesProcessed, 2);
  EXPECT_EQ(stats.gamesWhiteWins, 1); // Scholar's Mate is 1-0
  EXPECT_EQ(stats.gamesDraws, 1);     // Short draw is 1/2-1/2
}

// ============================================================================
// Filter 5: Qsearch stability (optional)
// ============================================================================

TEST_F(PositionExtractorTest, qsearchFilterDisabledByDefault) {
  ExtractionConfig config;
  config.minHalfMove = 1;
  EXPECT_FALSE(config.qsearchFilter);
  const auto stats = runExtraction(LONGER_GAME_PGN, "qs_disabled", config);
  EXPECT_EQ(stats.filteredQsearch, 0);
}

TEST_F(PositionExtractorTest, qsearchFilterReducesPositions) {
  // Run with and without qsearch filter — qsearch should filter some positions
  ExtractionConfig configOff;
  configOff.minHalfMove = 1;
  configOff.qsearchFilter = false;
  const auto statsOff = runExtraction(LONGER_GAME_PGN, "qs_off", configOff);

  ExtractionConfig configOn;
  configOn.minHalfMove = 1;
  configOn.qsearchFilter = true;
  configOn.qsearchThreshold = 1; // Very low threshold — should filter most positions
  const auto statsOn = runExtraction(LONGER_GAME_PGN, "qs_on", configOn);

  // With a threshold of 1 cp, qsearch will filter many positions
  EXPECT_GE(statsOff.positionsExtracted, statsOn.positionsExtracted);
  // And the qsearch counter should be nonzero
  EXPECT_GT(statsOn.filteredQsearch, 0);
}

// ============================================================================
// Integration test with real PGN file
// ============================================================================

TEST_F(PositionExtractorTest, integrationSmallPgnFile) {
  // Use the smallest available real PGN file (100 games)
  const std::string pgnPath = "../../results/matches/v0.4_vs_v1.1_blitz_100.pgn";
  if (!std::filesystem::exists(pgnPath)) {
    GTEST_SKIP() << "PGN file not found: " << pgnPath;
  }

  const auto outPath = tempOutputPath("integration_v04_v11.txt");
  tempFiles.push_back(outPath);

  ExtractionConfig config;
  // Use defaults
  PositionExtractor extractor;
  extractor.extract(pgnPath, outPath, config);
  const auto& stats = extractor.getStats();

  // Should process many games and extract many positions
  EXPECT_GT(stats.gamesTotal, 50);
  EXPECT_GT(stats.gamesProcessed, 50);
  EXPECT_GT(stats.positionsExtracted, 100);

  // Verify output file is non-empty and has valid format
  const auto lines = readLines(outPath);
  ASSERT_FALSE(lines.empty());
  for (const auto& line : lines) {
    const bool hasResult = line.ends_with("[1.0]") || line.ends_with("[0.5]") || line.ends_with("[0.0]");
    EXPECT_TRUE(hasResult) << "Invalid output line: " << line;
  }

  // Print stats for manual inspection
  stats.printSummary(pgnPath, outPath, 0);
}

// ============================================================================
// Summary printing
// ============================================================================

TEST_F(PositionExtractorTest, printSummaryDoesNotCrash) {
  constexpr ExtractionStats stats{};
  // Should not crash even with all zeros
  testing::internal::CaptureStdout();
  stats.printSummary("input.pgn", "output.txt", 0);
  const auto output = testing::internal::GetCapturedStdout();
  EXPECT_TRUE(output.find("Extraction Summary") != std::string::npos);
}

// ============================================================================
// Filter 6: Score contradiction filter
// ============================================================================

TEST_F(PositionExtractorTest, parseSearchScore_positiveScore) {
  // "+1.32/11 6.9s" → +132 cp
  const auto score = PositionExtractor::parseSearchScore("+1.32/11 6.9s");
  ASSERT_TRUE(score.has_value());
  EXPECT_EQ(score.value(), 132);
}

TEST_F(PositionExtractorTest, parseSearchScore_negativeScore) {
  // "-1.28/12 6.8s" → -128 cp
  const auto score = PositionExtractor::parseSearchScore("-1.28/12 6.8s");
  ASSERT_TRUE(score.has_value());
  EXPECT_EQ(score.value(), -128);
}

TEST_F(PositionExtractorTest, parseSearchScore_zeroScore) {
  // "0.00/19 0.008s" → 0 cp
  const auto score = PositionExtractor::parseSearchScore("0.00/19 0.008s");
  ASSERT_TRUE(score.has_value());
  EXPECT_EQ(score.value(), 0);
}

TEST_F(PositionExtractorTest, parseSearchScore_mateScore) {
  // "+M15/15 14s" → +10000 cp
  const auto score = PositionExtractor::parseSearchScore("+M15/15 14s");
  ASSERT_TRUE(score.has_value());
  EXPECT_EQ(score.value(), 10000);
}

TEST_F(PositionExtractorTest, parseSearchScore_negativeMate) {
  // "-M16/20 7.8s" → -10000 cp
  const auto score = PositionExtractor::parseSearchScore("-M16/20 7.8s");
  ASSERT_TRUE(score.has_value());
  EXPECT_EQ(score.value(), -10000);
}

TEST_F(PositionExtractorTest, parseSearchScore_bookMove) {
  // "book" → no score
  const auto score = PositionExtractor::parseSearchScore("book");
  EXPECT_FALSE(score.has_value());
}

TEST_F(PositionExtractorTest, parseSearchScore_empty) {
  const auto score = PositionExtractor::parseSearchScore("");
  EXPECT_FALSE(score.has_value());
}

TEST_F(PositionExtractorTest, parseSearchScore_largeScore) {
  // "+98.71/22 27s" → +9871 cp
  const auto score = PositionExtractor::parseSearchScore("+98.71/22 27s");
  ASSERT_TRUE(score.has_value());
  EXPECT_EQ(score.value(), 9871);
}

TEST_F(PositionExtractorTest, scoreFilterDisabledByDefault) {
  ExtractionConfig config;
  EXPECT_FALSE(config.scoreFilter);
  const auto stats = runExtraction(LONGER_GAME_PGN, "score_disabled", config);
  EXPECT_EQ(stats.filteredScoreContradiction, 0);
}
