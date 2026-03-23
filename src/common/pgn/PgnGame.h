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

#ifndef FRANKYCPP_PGNGAME_H
#define FRANKYCPP_PGNGAME_H

//=============================================================================
// PgnGame.h - Structured representation of a parsed PGN game
//=============================================================================
//
// Holds the parsed content of a single PGN game:
//   - Tag pair headers (Event, Site, Date, White, Black, Result, FEN, etc.)
//   - Move list as SAN strings (cleaned of comments, NAGs, variations)
//   - Game result as a typed enum
//
// This is the output of PgnParser — consumers (opening book, position
// extractor, etc.) work with PgnGame objects rather than raw PGN text.
//
//=============================================================================

#include "common/pgn/PgnTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace common::pgn {

  /// Structured representation of one parsed PGN game.
  struct PgnGame {
    /// Tag pair headers (key → value), e.g., "Event" → "Casual Game".
    /// Keys are stored without brackets, values without quotes.
    std::unordered_map<std::string, std::string> headers;

    /// Move strings in SAN notation, cleaned of comments, NAGs, and variations.
    /// Example: {"e4", "d5", "c4", "e5", "Nf3", "Nc6"}
    std::vector<std::string> moves;

    /// Raw comment text for each move, extracted from {}-blocks before cleanup.
    /// Parallel to moves[]: moveComments[i] is the comment for moves[i].
    /// Empty string if no comment was found for that move.
    /// Example: {"book", "book", "+1.32/11 6.9s", "-1.28/12 6.8s"}
    /// Populated by PgnParser; empty if PGN has no inline comments.
    std::vector<std::string> moveComments;

    /// Game result parsed from the [Result] header tag.
    GameResult result = GameResult::UNKNOWN;

    /// Returns the value of a header tag, or empty string if not present.
    /// @param key  Header tag name (e.g., "White", "FEN", "Event")
    /// @return     Header value, or empty string if key not found
    [[nodiscard]] std::string getHeader(const std::string& key) const {
      const auto it = headers.find(key);
      return (it != headers.end()) ? it->second : std::string{};
    }

    /// Returns true if this game has a known result (not UNKNOWN / "*").
    [[nodiscard]] bool hasKnownResult() const {
      return result != GameResult::UNKNOWN;
    }

    /// Returns true if this game has a FEN header (non-standard start position).
    [[nodiscard]] bool hasCustomFen() const {
      return headers.contains("FEN");
    }
  };

} // namespace common::pgn

#endif // FRANKYCPP_PGNGAME_H
