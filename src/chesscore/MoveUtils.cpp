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
// MoveUtils.cpp - Move Utility Functions Implementation
//=============================================================================

#include "MoveUtils.h"

#include "MoveGenerator.h"
#include "Position.h"

#include <algorithm>
#include <cctype>

using namespace chess;

namespace chess {

  std::string normalizeMove(const std::string& move) {
    std::string normalized = move;

    // Convert to lowercase
    std::ranges::transform(normalized, normalized.begin(),
                           [](const unsigned char c) { return std::tolower(c); });

    // Remove common decoration characters including equals sign
    // Keep only: letters and digits (UCI format: e7e8q, not e7e8=q)
    normalized.erase(
      std::ranges::remove_if(normalized,
                             [](const char c) {
                               return !std::isalnum(static_cast<unsigned char>(c));
                             })
        .begin(),
      normalized.end());


    return normalized;
  }

  bool matchesExpectedMove(
    const std::string& actualMove,
    const std::vector<std::string>& expectedMoves,
    const Position& position) {

    if (actualMove.empty() || expectedMoves.empty()) {
      return false;
    }

    // Normalize actual move for comparison
    const std::string normalizedActual = normalizeMove(actualMove);

    // Fast path: Try direct string match (normalized)
    for (const auto& expected : expectedMoves) {
      std::string normalizedExpected = normalizeMove(expected);
      if (normalizedActual == normalizedExpected) {
        return true;
      }
    }

    // Slow path: Try parsing expected moves as SAN and converting to UCI
    // This handles cases where EPD contains SAN notation like "Nf3", "e4", "O-O"
    MoveGenerator mg;

    for (const auto& expected : expectedMoves) {
      // Try parsing as UCI long algebraic first
      // Note: getMoveFromUci expects lowercase (see MoveGenerator.cpp line 283)
      const std::string expectedLower = normalizeMove(expected);
      Move moveFromUci                = mg.getMoveFromUci(position, expectedLower);
      if (moveFromUci != MOVE_NONE) {
        // Compare the UCI string representation
        const std::string uciStr = normalizeMove(moveFromUci.str());
        if (normalizedActual == uciStr) {
          return true;
        }
      }

      // Try parsing as SAN
      // Note: getMoveFromSan also expects lowercase input for the target square
      Move moveFromSan = mg.getMoveFromSan(position, expected);
      if (moveFromSan != MOVE_NONE) {
        // Compare the UCI string representation
        const std::string sanToUci = normalizeMove(moveFromSan.str());
        if (normalizedActual == sanToUci) {
          return true;
        }
      }
    }

    return false;
  }

} // namespace chess
