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

#ifndef FRANKYCPP_MOVETYPE_H
#define FRANKYCPP_MOVETYPE_H

//=============================================================================
// movetype.h - Move Type Enumeration and Bit Encoding Constants
//=============================================================================
//
// Defines the four types of chess moves and the bit layout constants
// used to encode/decode moves in the Move class.
// No internal dependencies.
//
// MoveType Values (pre-shifted for efficient encoding):
//   NORMAL    = 0 << 14    - Regular move (non-special)
//   PROMOTION = 1 << 14    - Pawn promotion
//   ENPASSANT = 2 << 14    - En passant capture
//   CASTLING  = 3 << 14    - Castling move
//
// MoveShifts Namespace:
//   Bit positions and masks for Move encoding:
//   - FROM_SHIFT (6), PROM_TYPE_SHIFT (12), MOVE_TYPE_SHIFT (14), VALUE_SHIFT (16)
//   - TO_MASK, FROM_MASK, PROM_TYPE_MASK, MOVE_TYPE_MASK, MOVE_MASK, VALUE_MASK
//
// Key Operations:
//   validMoveType(mt)  - True if valid move type (0-3)
//   str(mt)            - Single char label ('n', 'p', 'e', 'c')
//
// Usage:
//   MoveType mt = PROMOTION;
//   bool valid = validMoveType(mt);  // true
//   char c = str(mt);                // 'p'
//
//=============================================================================

// MoveShifts defines the bit shifts and masks for encoding and decoding moves.
namespace MoveShifts {
  constexpr unsigned int FROM_SHIFT      = 6u;
  constexpr unsigned int PROM_TYPE_SHIFT = 12u;
  constexpr unsigned int MOVE_TYPE_SHIFT = 14u;
  constexpr unsigned int VALUE_SHIFT     = 16u;

  constexpr unsigned int SQUARE_MASK    = 0b111111u;
  constexpr unsigned int TO_MASK        = SQUARE_MASK;
  constexpr unsigned int FROM_MASK      = SQUARE_MASK << FROM_SHIFT;
  constexpr unsigned int PROM_TYPE_MASK = 0b11u << PROM_TYPE_SHIFT;
  constexpr unsigned int MOVE_TYPE_MASK = 0b11u << MOVE_TYPE_SHIFT;

  constexpr unsigned int MOVE_MASK  = 0xFFFFu;               // first 16-bit
  constexpr unsigned int VALUE_MASK = 0xFFFFu << VALUE_SHIFT;// second 16-bit
}// namespace MoveShifts

enum MoveType : unsigned int {
  NORMAL    = 0 << MoveShifts::MOVE_TYPE_SHIFT,
  PROMOTION = 1 << MoveShifts::MOVE_TYPE_SHIFT,
  ENPASSANT = 2 << MoveShifts::MOVE_TYPE_SHIFT,
  CASTLING  = 3 << MoveShifts::MOVE_TYPE_SHIFT
};

// checks if move type is a value of 0 - 3
constexpr bool validMoveType(const MoveType mt) {
  return mt <= CASTLING;
}

inline auto moveTypeLabel = std::string("npec");

// single char label for the piece type (one of " npec")
constexpr char str(const MoveType mt) {
  if (!validMoveType(mt)) return '-';
  return moveTypeLabel[mt];
}

inline std::ostream& operator<<(std::ostream& os, const MoveType mt) {
  os << str(mt);
  return os;
}

#endif// FRANKYCPP_MOVETYPE_H
