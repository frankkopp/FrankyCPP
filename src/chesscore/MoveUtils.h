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

#ifndef FRANKYCPP_MOVEUTILS_H
#define FRANKYCPP_MOVEUTILS_H

//=============================================================================
// MoveUtils.h - Move Utility Functions
//=============================================================================
//
// This utility provides functions to compare and normalize moves in different notations
// (UCI long algebraic vs Standard Algebraic Notation).
//
// Use Case:
//   External UCI engines return moves in long algebraic notation (e.g., "e2e4"),
//   while EPD test files may contain moves in either format. This utility
//   handles the comparison and conversion between formats.
//
// Supported Formats:
//   - UCI Long Algebraic: "e2e4", "e7e8q", "e1g1" (castling)
//   - Standard Algebraic Notation (SAN): "e4", "Nf3", "O-O", "exd5", "e8=Q"
//
// Key Functions:
//   matchesExpectedMove()  - Compare actual move against expected move(s)
//   normalizeMove()        - Normalize move string for comparison
//
// Implementation Strategy:
//   1. Try direct string match first (fast path for UCI vs UCI)
//   2. If no match, try parsing SAN and converting to long algebraic
//   3. Compare normalized versions to handle notation variations
//
// Usage:
//   Position pos("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
//   std::string actualMove = "e2e4";
//   std::vector<std::string> expectedMoves = {"e4", "d4"};
//   bool matches = matchesExpectedMove(actualMove, expectedMoves, pos);
//
//=============================================================================

#include <string>
#include <vector>

namespace chess {

  // Forward declarations
  class Position;

  /// Compare actual move against expected move(s) in different notations
  /// @param actualMove UCI long algebraic move (e.g., "e2e4")
  /// @param expectedMoves List of expected moves (may be UCI or SAN format)
  /// @param position Current board position (needed for SAN parsing)
  /// @return True if actualMove matches any expected move
  bool matchesExpectedMove(
    const std::string& actualMove,
    const std::vector<std::string>& expectedMoves,
    const Position& position);

  /// Normalize move string for comparison (lowercase, remove decoration)
  /// @param move Move string to normalize
  /// @return Normalized move string
  std::string normalizeMove(const std::string& move);

} // namespace chess

#endif // FRANKYCPP_MOVEUTILS_H
