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

#ifndef FRANKYCPP_PIECETYPE_H
#define FRANKYCPP_PIECETYPE_H

//=============================================================================
// piecetype.h - Chess Piece Type Enumeration
//=============================================================================
//
// PieceType represents the type of a chess piece (without color).
// Depends on: macros.h
//
// Values:
//   PT_NONE   = 0    - No piece / invalid
//   KING      = 1    - King (non-sliding)
//   PAWN      = 2    - Pawn (non-sliding)
//   KNIGHT    = 3    - Knight (non-sliding)
//   BISHOP    = 4    - Bishop (sliding)
//   ROOK      = 5    - Rook (sliding)
//   QUEEN     = 6    - Queen (sliding)
//   PT_LENGTH = 7    - Sentinel / array size
//
// Bit Pattern Design:
//   Non-sliding pieces: (pt & 0b0100) == 0 && pt != 0
//   Sliding pieces:     (pt & 0b0100) != 0 && pt < 7
//
// Key Operations:
//   validPieceType(pt)   - True if PT_NONE through QUEEN
//   gamePhaseValue(pt)   - Phase contribution (K=0, P=0, N=1, B=1, R=2, Q=4)
//   str(pt)              - Single char label (" KPNBRQ")
//
// Usage:
//   PieceType pt = KNIGHT;
//   bool sliding = (pt & 0b0100) != 0;  // false
//   int phase = gamePhaseValue(pt);     // 1
//
//=============================================================================

#include "macros.h"
#include <string>

enum PieceType : uint_fast8_t {
  PT_NONE, // 0
  KING,    // 1 non sliding
  PAWN,    // 2 non sliding
  KNIGHT,  // 3 non sliding
  BISHOP,  // 4 sliding
  ROOK,    // 5 sliding
  QUEEN,   // 6 sliding
  PT_LENGTH// 7
};

// checks if piece type is a value of 0 - 6
constexpr bool validPieceType(const PieceType pt) { return pt < 7; }

constexpr int phaseValue[] = {
  0,// no type
  0,// king
  0,// pawn
  1,// knight
  1,// bishop
  2,// rook
  4 // queen
};

inline auto pieceLabels = std::string(" KPNBRQ");

// returns the game phase value of the piece type to
// compute the current game phase in relation to the
// pieces currently on the board.
constexpr int gamePhaseValue(const PieceType pt) { return phaseValue[pt]; }


// single char label for the piece type (one of " KPNBRQ")
constexpr char str(const PieceType pt) {
  if (!validPieceType(pt)) return '-';
  return pieceLabels[pt];
}

inline std::ostream& operator<<(std::ostream& os, const PieceType pt) {
  os << str(pt);
  return os;
}

ENABLE_INCR_OPERATORS_ON(PieceType)

#endif// FRANKYCPP_PIECETYPE_H
