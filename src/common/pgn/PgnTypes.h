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

#ifndef FRANKYCPP_PGNTYPES_H
#define FRANKYCPP_PGNTYPES_H

//=============================================================================
// PgnTypes.h - Shared types for PGN parsing
//=============================================================================
//
// Defines fundamental types used by the PGN parser and its consumers:
//   GameResult - Outcome of a chess game (White win, Draw, Black win, Unknown)
//
// The numeric values for GameResult (1.0, 0.5, 0.0) follow the standard
// convention used in chess databases and Texel tuning, always expressed
// from White's perspective.
//
//=============================================================================

#include <string>

namespace common::pgn {

  /// Game result from White's perspective.
  /// Numeric values match the standard labeling convention:
  ///   WHITE_WIN = 1.0, DRAW = 0.5, BLACK_WIN = 0.0
  enum class GameResult {
    WHITE_WIN, ///< "1-0"
    DRAW,      ///< "1/2-1/2"
    BLACK_WIN, ///< "0-1"
    UNKNOWN    ///< "*" or missing/unparseable result
  };

  /// Converts a GameResult to its numeric value from White's perspective.
  /// @param result  The game result
  /// @return 1.0 for White win, 0.5 for Draw, 0.0 for Black win, 0.5 for Unknown
  [[nodiscard]] constexpr double resultToDouble(const GameResult result) {
    switch (result) {
      case GameResult::WHITE_WIN: return 1.0;
      case GameResult::DRAW: return 0.5;
      case GameResult::BLACK_WIN: return 0.0;
      case GameResult::UNKNOWN: return 0.5; // treat unknown as draw for safety
    }
    return 0.5;
  }

  /// Converts a PGN result string to a GameResult enum.
  /// @param resultStr  The result string (e.g., "1-0", "0-1", "1/2-1/2", "*")
  /// @return Corresponding GameResult enum value
  [[nodiscard]] inline GameResult parseResultString(const std::string& resultStr) {
    if (resultStr == "1-0") return GameResult::WHITE_WIN;
    if (resultStr == "0-1") return GameResult::BLACK_WIN;
    if (resultStr == "1/2-1/2") return GameResult::DRAW;
    return GameResult::UNKNOWN;
  }

  /// Converts a GameResult to its PGN string representation.
  /// @param result  The game result
  /// @return PGN result string ("1-0", "0-1", "1/2-1/2", or "*")
  [[nodiscard]] inline std::string resultToString(const GameResult result) {
    switch (result) {
      case GameResult::WHITE_WIN: return "1-0";
      case GameResult::BLACK_WIN: return "0-1";
      case GameResult::DRAW: return "1/2-1/2";
      case GameResult::UNKNOWN: return "*";
    }
    return "*";
  }

} // namespace common::pgn

#endif // FRANKYCPP_PGNTYPES_H
