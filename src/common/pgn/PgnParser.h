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

#ifndef FRANKYCPP_PGNPARSER_H
#define FRANKYCPP_PGNPARSER_H

//=============================================================================
// PgnParser.h - Reusable PGN file parser
//=============================================================================
//
// Parses PGN (Portable Game Notation) files into structured PgnGame objects.
// Extracted and generalized from OpeningBook::readGamesPgn() to serve as
// shared infrastructure for both the opening book and tuning tools.
//
// Features:
//   - Streaming API (game-by-game callback) for memory-efficient large files
//   - Batch API (returns all games) for simpler usage
//   - Robust handling of PGN edge cases:
//     * Comments: {curly}, (parenthesized variations), ;line, %escape
//     * NAGs ($1, $2, etc.)
//     * Tag brackets <...>
//     * Non-ASCII characters
//     * Multiple games per file
//   - Header tag extraction (Event, Site, Result, FEN, etc.)
//   - Game result parsing from [Result] tag
//
// The move cleanup logic (cleanUpPgnMoveSection) is preserved from the
// original OpeningBook implementation which has been battle-tested against
// large PGN databases (superbook.pgn, 190K+ games).
//
// Thread Safety:
//   PgnParser instances are NOT thread-safe. Use separate instances per
//   thread if parsing in parallel.
//
// Usage:
//   PgnParser parser;
//
//   // Streaming (memory-efficient for large files):
//   parser.parseFile("games.pgn", [](PgnGame&& game) {
//       // process each game as it's parsed
//   });
//
//   // Batch (simpler, loads all games into memory):
//   auto games = parser.parseAll("games.pgn");
//
//   // From lines already loaded in memory:
//   auto games = parser.parseFromLines(lines);
//
//=============================================================================

#include "common/pgn/PgnGame.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace common::pgn {

  class PgnParser {

    /// Statistics from the last parse operation
    uint64_t gamesProcessed = 0;
    uint64_t gamesSkipped   = 0;

  public:
    /// Callback type for streaming parse. Called once for each complete game.
    using GameCallback = std::function<void(PgnGame&&)>;

    /// Parses a PGN file, calling the callback for each complete game found.
    /// This is the memory-efficient API for large files — only one game is
    /// in memory at a time (plus the raw file buffer).
    /// @param filePath      Path to the PGN file
    /// @param gameCallback  Called once for each parsed game
    void parseFile(const std::string& filePath, const GameCallback& gameCallback);

    /// Parses a PGN file and returns all games as a vector.
    /// Simpler API but requires all games to fit in memory.
    /// @param filePath  Path to the PGN file
    /// @return Vector of parsed games
    [[nodiscard]] std::vector<PgnGame> parseAll(const std::string& filePath);

    /// Parses PGN content from lines already loaded in memory.
    /// Useful when the caller has already read the file (e.g., OpeningBook).
    /// @param lines  Vector of string_views pointing to file content lines
    /// @return Vector of parsed games
    [[nodiscard]] std::vector<PgnGame> parseFromLines(const std::vector<std::string_view>& lines);

    /// Parses PGN content from lines, calling the callback for each game.
    /// @param lines         Vector of string_views pointing to file content lines
    /// @param gameCallback  Called once for each parsed game
    void parseFromLines(const std::vector<std::string_view>& lines, const GameCallback& gameCallback);

    /// Cleans up a PGN move section string by removing comments, NAGs,
    /// variations, move numbers, result tokens, and non-ASCII characters.
    /// The cleaned string contains only SAN move tokens separated by spaces.
    ///
    /// This is a static utility — exposed for testing and reuse by OpeningBook.
    /// Preserves the exact logic from the original OpeningBook implementation.
    ///
    /// @param str  Move section string to clean (modified in place)
    static void cleanUpMoveSection(std::string& str);

  private:
    /// Reads a file into memory and splits into lines (string_views).
    /// Uses the same fast read approach as OpeningBook::readFile().
    /// @param filePath  Path to the file to read
    /// @param fileData  Output: vector holding the raw file data
    /// @return Vector of string_views into fileData
    [[nodiscard]] static std::vector<std::string_view> readFileLines(
      const std::string& filePath,
      std::vector<char>& fileData);

    /// Identifies game boundaries in a sequence of lines and processes each game.
    /// A new game starts when a tag pair line ([...]) appears after an empty line.
    /// @param lines         The PGN file lines
    /// @param gameCallback  Called once for each parsed game
    void processGames(const std::vector<std::string_view>& lines, const GameCallback& gameCallback);

    /// Parses a single game from a range of lines [gameStart, gameEnd).
    /// Extracts headers from tag pair lines and moves from the move text section.
    /// @param lines      The full PGN file lines
    /// @param gameStart  First line of this game (inclusive)
    /// @param gameEnd    Last line of this game (exclusive)
    /// @return Parsed game, or std::nullopt if the game has no moves
    [[nodiscard]] static std::optional<PgnGame> parseOneGame(
      const std::vector<std::string_view>& lines,
      size_t gameStart,
      size_t gameEnd);

    /// Parses a PGN tag pair (e.g., [Event "Casual Game"]) from a line.
    /// Handles escaped quotes (\") and backslashes (\\) in values.
    /// Supports multiple tag pairs on one line by accepting a start position
    /// and returning the position after the parsed tag pair.
    /// @param line      The line to parse
    /// @param startPos  Position to start searching for '[' (0 for first call)
    /// @param key       Output: tag name
    /// @param value     Output: tag value (unescaped)
    /// @return Position after the closing ']', or std::string_view::npos if no tag found
    [[nodiscard]] static size_t parseTagPair(
      std::string_view line, size_t startPos,
      std::string& key, std::string& value);

  public:
    // Getters
    [[nodiscard]] uint64_t getGamesProcessed() const { return gamesProcessed; }
    [[nodiscard]] uint64_t getGamesSkipped() const { return gamesSkipped; }
  };

} // namespace common::pgn

#endif // FRANKYCPP_PGNPARSER_H
