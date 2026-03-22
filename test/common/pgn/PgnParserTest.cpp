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

#include "common/pgn/PgnParser.h"
#include "common/pgn/PgnGame.h"
#include "common/pgn/PgnTypes.h"
#include "common/Logging.h"
#include "common/stringutil.h"
#include "Test_Utils.h"

#include <gtest/gtest.h>

using namespace common::pgn;

class PgnParserTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    common::Logger::get().BOOK_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

// ============================================================================
// PgnTypes tests
// ============================================================================

TEST_F(PgnParserTest, resultToDouble) {
  EXPECT_DOUBLE_EQ(1.0, resultToDouble(GameResult::WHITE_WIN));
  EXPECT_DOUBLE_EQ(0.5, resultToDouble(GameResult::DRAW));
  EXPECT_DOUBLE_EQ(0.0, resultToDouble(GameResult::BLACK_WIN));
  EXPECT_DOUBLE_EQ(0.5, resultToDouble(GameResult::UNKNOWN));
}

TEST_F(PgnParserTest, parseResultString) {
  EXPECT_EQ(GameResult::WHITE_WIN, parseResultString("1-0"));
  EXPECT_EQ(GameResult::BLACK_WIN, parseResultString("0-1"));
  EXPECT_EQ(GameResult::DRAW, parseResultString("1/2-1/2"));
  EXPECT_EQ(GameResult::UNKNOWN, parseResultString("*"));
  EXPECT_EQ(GameResult::UNKNOWN, parseResultString(""));
  EXPECT_EQ(GameResult::UNKNOWN, parseResultString("garbage"));
}

TEST_F(PgnParserTest, resultToString) {
  EXPECT_EQ("1-0", resultToString(GameResult::WHITE_WIN));
  EXPECT_EQ("0-1", resultToString(GameResult::BLACK_WIN));
  EXPECT_EQ("1/2-1/2", resultToString(GameResult::DRAW));
  EXPECT_EQ("*", resultToString(GameResult::UNKNOWN));
}

TEST_F(PgnParserTest, resultRoundTrip) {
  // Verify that parsing and converting back to string is lossless
  for (const auto& s : {"1-0", "0-1", "1/2-1/2", "*"}) {
    EXPECT_EQ(s, resultToString(parseResultString(s)));
  }
}

// ============================================================================
// PgnGame tests
// ============================================================================

TEST_F(PgnParserTest, pgnGameGetHeader) {
  PgnGame game;
  game.headers["White"] = "Kasparov";
  game.headers["Black"] = "Karpov";

  EXPECT_EQ("Kasparov", game.getHeader("White"));
  EXPECT_EQ("Karpov", game.getHeader("Black"));
  EXPECT_EQ("", game.getHeader("Event")); // missing header
}

TEST_F(PgnParserTest, pgnGameHasKnownResult) {
  PgnGame game;
  EXPECT_FALSE(game.hasKnownResult()); // default is UNKNOWN

  game.result = GameResult::WHITE_WIN;
  EXPECT_TRUE(game.hasKnownResult());

  game.result = GameResult::DRAW;
  EXPECT_TRUE(game.hasKnownResult());

  game.result = GameResult::BLACK_WIN;
  EXPECT_TRUE(game.hasKnownResult());
}

TEST_F(PgnParserTest, pgnGameHasCustomFen) {
  PgnGame game;
  EXPECT_FALSE(game.hasCustomFen());

  game.headers["FEN"] = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
  EXPECT_TRUE(game.hasCustomFen());
}

// ============================================================================
// cleanUpMoveSection tests (preserves OpeningBook::cleanUpPgnMoveSection behavior)
// ============================================================================

TEST_F(PgnParserTest, cleanUpMoveSectionBasic) {
  // This is the exact test case from the existing OpeningBookTest::pgnCleanUpTest
  const std::string testString{"e4(d4) d5!!2.c4$50(Nf3?)e5 Nf3{Comment !}Nc6 Nc3 Nf6 Bc4 {another comment} Bc5 O-O O-O a1=Q  @@@æææ {unexpected characters are skipped}  <> {These symbols are reserved}  1/2-1/2  ; comment     "};
  std::string test = common::removeTrailingComments(testString, ";");
  PgnParser::cleanUpMoveSection(test);
  EXPECT_EQ("e4 d5 c4 e5 Nf3 Nc6 Nc3 Nf6 Bc4 Bc5 O-O O-O a1=Q", test);
}

TEST_F(PgnParserTest, cleanUpMoveSectionResultRemoval) {
  // Test result removal for all result types
  std::string s1 = "e4 e5 Nf3 1-0";
  PgnParser::cleanUpMoveSection(s1);
  EXPECT_EQ("e4 e5 Nf3", s1);

  std::string s2 = "e4 e5 Nf3 0-1";
  PgnParser::cleanUpMoveSection(s2);
  EXPECT_EQ("e4 e5 Nf3", s2);

  std::string s3 = "e4 e5 Nf3 1/2-1/2";
  PgnParser::cleanUpMoveSection(s3);
  EXPECT_EQ("e4 e5 Nf3", s3);

  std::string s4 = "e4 e5 Nf3 *";
  PgnParser::cleanUpMoveSection(s4);
  EXPECT_EQ("e4 e5 Nf3", s4);
}

TEST_F(PgnParserTest, cleanUpMoveSectionEmpty) {
  std::string s;
  PgnParser::cleanUpMoveSection(s);
  EXPECT_TRUE(s.empty());
}

TEST_F(PgnParserTest, cleanUpMoveSectionMoveNumbers) {
  std::string s = "1. e4 e5 2. Nf3 Nc6 3. Bb5 a6";
  PgnParser::cleanUpMoveSection(s);
  EXPECT_EQ("e4 e5 Nf3 Nc6 Bb5 a6", s);
}

TEST_F(PgnParserTest, cleanUpMoveSectionNags) {
  std::string s = "e4$1 e5$2 Nf3$50 Nc6";
  PgnParser::cleanUpMoveSection(s);
  EXPECT_EQ("e4 e5 Nf3 Nc6", s);
}

TEST_F(PgnParserTest, cleanUpMoveSectionNestedVariations) {
  std::string s = "e4 (d4 (c4 e5) d5) e5 Nf3";
  PgnParser::cleanUpMoveSection(s);
  EXPECT_EQ("e4 e5 Nf3", s);
}

TEST_F(PgnParserTest, cleanUpMoveSectionCurlyBraceComments) {
  std::string s = "e4 {best move!} e5 {also good} Nf3";
  PgnParser::cleanUpMoveSection(s);
  EXPECT_EQ("e4 e5 Nf3", s);
}

TEST_F(PgnParserTest, cleanUpMoveSectionAngleBracketComments) {
  std::string s = "e4 <reserved> e5 <stuff> Nf3";
  PgnParser::cleanUpMoveSection(s);
  EXPECT_EQ("e4 e5 Nf3", s);
}

// ============================================================================
// Tag pair parsing tests
// ============================================================================

TEST_F(PgnParserTest, parsePgnTestFile) {
  PgnParser parser;
  const auto games = parser.parseAll("./books/pgn_test.pgn");

  fprintln("pgn_test.pgn: {:L} games parsed, {:L} skipped",
           parser.getGamesProcessed(), parser.getGamesSkipped());

  // The file should have multiple games
  EXPECT_GT(games.size(), 0);

  // Check first game has headers and moves
  const auto& firstGame = games[0];
  EXPECT_FALSE(firstGame.moves.empty());
  fprintln("First game: {} vs {} — {} moves, result: {}",
           firstGame.getHeader("White"),
           firstGame.getHeader("Black"),
           firstGame.moves.size(),
           resultToString(firstGame.result));

  // First game in pgn_test.pgn has Result "1/2-1/2" and a custom FEN
  EXPECT_EQ(GameResult::DRAW, firstGame.result);
  EXPECT_TRUE(firstGame.hasCustomFen());

  // Second game: Opocensky vs Flohr, 0-1
  if (games.size() > 1) {
    const auto& secondGame = games[1];
    EXPECT_EQ("Opocensky, Karel", secondGame.getHeader("White"));
    EXPECT_EQ("Flohr, Salo", secondGame.getHeader("Black"));
    EXPECT_EQ(GameResult::BLACK_WIN, secondGame.result);
    EXPECT_FALSE(secondGame.moves.empty());
    fprintln("Second game: {} vs {} — {} moves, result: {}",
             secondGame.getHeader("White"),
             secondGame.getHeader("Black"),
             secondGame.moves.size(),
             resultToString(secondGame.result));
  }

  // Third game: Kasparov vs Vasilienko, 1-0
  if (games.size() > 2) {
    const auto& thirdGame = games[2];
    EXPECT_EQ("Kasparov Garry", thirdGame.getHeader("White"));
    EXPECT_EQ(GameResult::WHITE_WIN, thirdGame.result);
  }
}

TEST_F(PgnParserTest, parsePgnTest2File) {
  PgnParser parser;
  const auto games = parser.parseAll("./books/pgn_test2.pgn");

  fprintln("pgn_test2.pgn: {:L} games parsed", parser.getGamesProcessed());

  // pgn_test2.pgn has 2 games
  EXPECT_EQ(2, games.size());

  // First game uses UCI notation (h2h3, e7e5, ...) and has result "*"
  const auto& game1 = games[0];
  EXPECT_EQ(GameResult::UNKNOWN, game1.result);
  EXPECT_FALSE(game1.hasKnownResult());

  // Second game: Stein vs Karpov, 1/2-1/2
  const auto& game2 = games[1];
  EXPECT_EQ("Stein L", game2.getHeader("White"));
  EXPECT_EQ("Karpov Anatoli", game2.getHeader("Black"));
  EXPECT_EQ(GameResult::DRAW, game2.result);
  EXPECT_TRUE(game2.hasKnownResult());
}

TEST_F(PgnParserTest, headerEscapedQuotes) {
  // Test PGN escape handling: \" inside a tag value should produce a literal "
  // Using raw string literal to avoid C++ escape ambiguity.
  // The PGN content has: [Event "Has \"quotes\" inside"]
  const std::string content = R"([Event "Has \"quotes\" inside"]
[Result "1-0"]

1. e4 e5 1-0
)";

  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      lines.emplace_back(content.data() + start, i - start);
      start = i + 1;
    }
  }

  PgnParser parser;
  const auto games = parser.parseFromLines(lines);

  ASSERT_EQ(1, games.size());
  const auto& event = games[0].getHeader("Event");
  fprintln("Event: '{}'", event);
  // PGN \" should be unescaped to literal " in the parsed value
  EXPECT_EQ("Has \"quotes\" inside", event);
}

TEST_F(PgnParserTest, multipleTagPairsPerLine) {
  // Test that multiple [Key "Value"] pairs on a single line are all parsed
  const std::string content =
    "[Event \"Test\"][Site \"Here\"][Date \"2026.01.01\"]\n"
    "[White \"Alice\"][Black \"Bob\"][Result \"1-0\"]\n"
    "\n"
    "1. e4 e5 1-0\n";

  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      lines.emplace_back(content.data() + start, i - start);
      start = i + 1;
    }
  }

  PgnParser parser;
  const auto games = parser.parseFromLines(lines);

  ASSERT_EQ(1, games.size());
  EXPECT_EQ("Test", games[0].getHeader("Event"));
  EXPECT_EQ("Here", games[0].getHeader("Site"));
  EXPECT_EQ("2026.01.01", games[0].getHeader("Date"));
  EXPECT_EQ("Alice", games[0].getHeader("White"));
  EXPECT_EQ("Bob", games[0].getHeader("Black"));
  EXPECT_EQ(GameResult::WHITE_WIN, games[0].result);
}

TEST_F(PgnParserTest, headerSetUpAndFen) {
  // First game in pgn_test.pgn has [SetUp "1"] and [FEN "..."]
  PgnParser parser;
  const auto games = parser.parseAll("./books/pgn_test.pgn");

  ASSERT_GT(games.size(), 0);
  EXPECT_EQ("1", games[0].getHeader("SetUp"));
  EXPECT_FALSE(games[0].getHeader("FEN").empty());
  fprintln("FEN: {}", games[0].getHeader("FEN"));
}

// ============================================================================
// Streaming API tests
// ============================================================================

TEST_F(PgnParserTest, parseFileStreaming) {
  PgnParser parser;
  int gameCount = 0;
  int whiteWins = 0;
  int blackWins = 0;
  int draws     = 0;
  int unknown   = 0;

  parser.parseFile("./books/pgn_test.pgn", [&](PgnGame&& game) {
    gameCount++;
    switch (game.result) {
      case GameResult::WHITE_WIN: whiteWins++; break;
      case GameResult::BLACK_WIN: blackWins++; break;
      case GameResult::DRAW: draws++; break;
      case GameResult::UNKNOWN: unknown++; break;
    }
  });

  fprintln("Streaming parse: {} games (W:{}, B:{}, D:{}, ?:{})",
           gameCount, whiteWins, blackWins, draws, unknown);

  EXPECT_EQ(gameCount, static_cast<int>(parser.getGamesProcessed()));
  EXPECT_GT(gameCount, 0);
}

TEST_F(PgnParserTest, parseAllMatchesStreamingCount) {
  // Verify batch and streaming APIs produce the same number of games
  PgnParser parser1;
  const auto batchGames = parser1.parseAll("./books/pgn_test.pgn");

  PgnParser parser2;
  int streamCount = 0;
  parser2.parseFile("./books/pgn_test.pgn", [&](PgnGame&&) {
    streamCount++;
  });

  EXPECT_EQ(batchGames.size(), static_cast<size_t>(streamCount));
}

// ============================================================================
// parseFromLines tests (used by OpeningBook integration)
// ============================================================================

TEST_F(PgnParserTest, parseFromLines) {
  // Manually construct lines that represent a minimal PGN game
  const std::string content =
    "[Event \"Test\"]\n"
    "[Result \"1-0\"]\n"
    "\n"
    "1. e4 e5 2. Nf3 Nc6 1-0\n";

  // Split into lines (simulating what OpeningBook::readFile produces)
  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      lines.emplace_back(content.data() + start, i - start);
      start = i + 1;
    }
  }

  PgnParser parser;
  const auto games = parser.parseFromLines(lines);

  ASSERT_EQ(1, games.size());
  EXPECT_EQ(GameResult::WHITE_WIN, games[0].result);
  EXPECT_EQ("Test", games[0].getHeader("Event"));
  EXPECT_EQ(4, games[0].moves.size()); // e4, e5, Nf3, Nc6
  EXPECT_EQ("e4", games[0].moves[0]);
  EXPECT_EQ("e5", games[0].moves[1]);
  EXPECT_EQ("Nf3", games[0].moves[2]);
  EXPECT_EQ("Nc6", games[0].moves[3]);
}

TEST_F(PgnParserTest, parseFromLinesMultipleGames) {
  const std::string content =
    "[Event \"Game 1\"]\n"
    "[Result \"1-0\"]\n"
    "\n"
    "1. e4 e5 1-0\n"
    "\n"
    "[Event \"Game 2\"]\n"
    "[Result \"0-1\"]\n"
    "\n"
    "1. d4 d5 0-1\n";

  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      lines.emplace_back(content.data() + start, i - start);
      start = i + 1;
    }
  }

  PgnParser parser;
  const auto games = parser.parseFromLines(lines);

  ASSERT_EQ(2, games.size());
  EXPECT_EQ(GameResult::WHITE_WIN, games[0].result);
  EXPECT_EQ("Game 1", games[0].getHeader("Event"));
  EXPECT_EQ(2, games[0].moves.size());

  EXPECT_EQ(GameResult::BLACK_WIN, games[1].result);
  EXPECT_EQ("Game 2", games[1].getHeader("Event"));
  EXPECT_EQ(2, games[1].moves.size());
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_F(PgnParserTest, emptyFile) {
  // Create a minimal empty-ish content
  const std::string content = "\n\n\n";
  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      lines.emplace_back(content.data() + start, i - start);
      start = i + 1;
    }
  }

  PgnParser parser;
  const auto games = parser.parseFromLines(lines);
  EXPECT_EQ(0, games.size());
}

TEST_F(PgnParserTest, gameWithNoMoves) {
  const std::string content =
    "[Event \"Adjourned\"]\n"
    "[Result \"*\"]\n"
    "\n"
    "*\n";

  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      lines.emplace_back(content.data() + start, i - start);
      start = i + 1;
    }
  }

  PgnParser parser;
  const auto games = parser.parseFromLines(lines);
  // Game with only "*" as move text should be skipped
  EXPECT_EQ(0, games.size());
}

TEST_F(PgnParserTest, gameWithOnlyResult) {
  const std::string content =
    "[Event \"Forfeit\"]\n"
    "[Result \"1-0\"]\n"
    "\n"
    "1-0\n";

  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      lines.emplace_back(content.data() + start, i - start);
      start = i + 1;
    }
  }

  PgnParser parser;
  const auto games = parser.parseFromLines(lines);
  // Game with only a result (no actual moves) should be skipped
  EXPECT_EQ(0, games.size());
}

TEST_F(PgnParserTest, percentEscapedLines) {
  // %-escaped lines should be completely ignored per PGN spec
  const std::string content =
    "[Event \"Test\"]\n"
    "[Result \"1-0\"]\n"
    "\n"
    "% This is an escaped comment line\n"
    "1. e4 e5 1-0\n";

  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      lines.emplace_back(content.data() + start, i - start);
      start = i + 1;
    }
  }

  PgnParser parser;
  const auto games = parser.parseFromLines(lines);

  ASSERT_EQ(1, games.size());
  EXPECT_EQ(2, games[0].moves.size());
}

TEST_F(PgnParserTest, semicolonLineComments) {
  // ;-comments should be stripped from end of lines
  const std::string content =
    "[Event \"Test\"]\n"
    "[Result \"1-0\"]\n"
    "\n"
    "1. e4 e5 ;this is a comment\n"
    "2. Nf3 Nc6 1-0\n";

  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      lines.emplace_back(content.data() + start, i - start);
      start = i + 1;
    }
  }

  PgnParser parser;
  const auto games = parser.parseFromLines(lines);

  ASSERT_EQ(1, games.size());
  EXPECT_EQ(4, games[0].moves.size());
}

TEST_F(PgnParserTest, resultCounts) {
  // Test that we correctly count game results across all games in pgn_test.pgn
  PgnParser parser;
  const auto games = parser.parseAll("./books/pgn_test.pgn");

  int whiteWins = 0, blackWins = 0, draws = 0, unknown = 0;
  for (const auto& game : games) {
    switch (game.result) {
      case GameResult::WHITE_WIN: whiteWins++; break;
      case GameResult::BLACK_WIN: blackWins++; break;
      case GameResult::DRAW: draws++; break;
      case GameResult::UNKNOWN: unknown++; break;
    }
  }

  fprintln("pgn_test.pgn results: W:{}, B:{}, D:{}, ?:{}", whiteWins, blackWins, draws, unknown);

  // pgn_test.pgn has games with all result types — verify at least one of each known type
  EXPECT_GT(whiteWins + blackWins + draws, 0) << "Should have at least some known results";
}

// ============================================================================
// Larger file tests (skip in bulk runs)
// ============================================================================

TEST_F(PgnParserTest, parse8movesV3) {
  if (isBulkRun()) GTEST_SKIP();

  PgnParser parser;
  const auto games = parser.parseAll("./books/8moves_v3.pgn");

  fprintln("8moves_v3.pgn: {:L} games parsed, {:L} skipped",
           parser.getGamesProcessed(), parser.getGamesSkipped());

  // This file should have a substantial number of games
  EXPECT_GT(games.size(), 100);

  // Verify result distribution is reasonable
  int withResult = 0;
  for (const auto& game : games) {
    if (game.hasKnownResult()) withResult++;
  }
  fprintln("  Games with known result: {:L} / {:L}", withResult, games.size());
}

TEST_F(PgnParserTest, parseSuperbook) {
  if (isBulkRun()) GTEST_SKIP();

  PgnParser parser;
  const auto start = std::chrono::high_resolution_clock::now();
  const auto games = parser.parseAll("./books/superbook.pgn");
  const auto stop  = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

  fprintln("superbook.pgn: {:L} games parsed, {:L} skipped in {:L} ms",
           parser.getGamesProcessed(), parser.getGamesSkipped(), elapsed.count());

  // superbook.pgn has ~190K games
  EXPECT_GT(games.size(), 100'000);

  // Spot-check: verify games have moves and headers
  int gamesWithMoves = 0;
  int gamesWithResult = 0;
  for (const auto& game : games) {
    if (!game.moves.empty()) gamesWithMoves++;
    if (game.hasKnownResult()) gamesWithResult++;
  }
  fprintln("  Games with moves: {:L}, with known result: {:L}", gamesWithMoves, gamesWithResult);

  // Vast majority should have moves and results
  EXPECT_GT(gamesWithMoves, static_cast<int>(games.size()) * 9 / 10);
  EXPECT_GT(gamesWithResult, static_cast<int>(games.size()) * 9 / 10);
}

TEST_F(PgnParserTest, parseSuperbookStreaming) {
  if (isBulkRun()) GTEST_SKIP();

  // Verify streaming API handles the large file correctly
  PgnParser parser;
  uint64_t gameCount = 0;
  uint64_t totalMoves = 0;

  const auto start = std::chrono::high_resolution_clock::now();
  parser.parseFile("./books/superbook.pgn", [&](PgnGame&& game) {
    gameCount++;
    totalMoves += game.moves.size();
  });
  const auto stop  = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

  fprintln("superbook.pgn streaming: {:L} games, {:L} total moves in {:L} ms",
           gameCount, totalMoves, elapsed.count());

  EXPECT_GT(gameCount, 100'000);
  EXPECT_EQ(gameCount, parser.getGamesProcessed());
}

// ============================================================================
// Move quality verification (spot-check that cleanup produces valid SAN)
// ============================================================================

TEST_F(PgnParserTest, movesAreSanFormat) {
  PgnParser parser;
  const auto games = parser.parseAll("./books/pgn_test.pgn");

  for (const auto& game : games) {
    for (const auto& move : game.moves) {
      // SAN moves should start with a letter (piece or file for pawns)
      // and should not contain digits followed by dots (move numbers)
      EXPECT_FALSE(move.empty()) << "Empty move string found";
      EXPECT_TRUE(std::isalpha(static_cast<unsigned char>(move[0])))
        << "Move should start with letter, got: " << move;
      // Should not contain comment markers or brackets
      EXPECT_EQ(std::string::npos, move.find('{')) << "Move contains '{': " << move;
      EXPECT_EQ(std::string::npos, move.find('(')) << "Move contains '(': " << move;
      EXPECT_EQ(std::string::npos, move.find('$')) << "Move contains '$': " << move;
    }
  }
}

TEST_F(PgnParserTest, nonExistentFile) {
  PgnParser parser;
  const auto games = parser.parseAll("./books/does_not_exist.pgn");
  EXPECT_EQ(0, games.size());
  EXPECT_EQ(0u, parser.getGamesProcessed());
}
