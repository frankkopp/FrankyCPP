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

#ifndef FRANKYCPP_DIRECTION_H
#define FRANKYCPP_DIRECTION_H

//=============================================================================
// direction.h - Board Direction Offsets for Piece Movement
//=============================================================================
//
// Direction represents a step offset for moving on the 64-square board.
// Depends on: macros.h, square.h
//
// Encoding:
//   Signed offset in mailbox-64 representation (A1=0, H8=63)
//   Positive = north/east, Negative = south/west
//
// Constants:
//   NORTH = +8    SOUTH = -8    (rank change)
//   EAST  = +1    WEST  = -1    (file change)
//   NORTH_EAST = +9    SOUTH_WEST = -9   (diagonals)
//   NORTH_WEST = +7    SOUTH_EAST = -7   (anti-diagonals)
//
// Key Operations:
//   Direction::pawnPush(color)  - Returns +8 (WHITE) or -8 (BLACK)
//   Square + Direction          - Move square in direction
//   Square - Direction          - Move square in opposite direction
//
// Usage:
//   Square e4 = SQ_E4;
//   Square e5 = e4 + NORTH;           // one rank up
//   Square f5 = e4 + NORTH_EAST;      // diagonal
//   Direction push = Direction::pawnPush(WHITE);  // +8
//
//=============================================================================

#include "macros.h"
#include "square.h"

#include <cstdint>

namespace chess {

  class Direction {
    int_fast8_t v_{};// step in [-9, +9] domain for our use cases

  public:
    // constructors
    constexpr Direction() = default;
    constexpr explicit Direction(const int v) : v_{static_cast<int_fast8_t>(v)} {}

    // underlying value access
    constexpr int_fast8_t value() const { return v_; }

    // implicit conversion for arithmetic/macros compatibility
    // ReSharper disable once CppNonExplicitConversionOperator
    constexpr operator int() const { return v_; }

    // factory: pawn push direction for a color (branchless)
    static constexpr Direction pawnPush(const Color c) { return Direction{c.sign() * 8}; }
  };

  // Inline constexpr direction constants
  inline constexpr Direction NORTH{8};
  inline constexpr Direction EAST{1};
  inline constexpr Direction SOUTH{-8};
  inline constexpr Direction WEST{-1};
  inline constexpr Direction NORTH_EAST{9};
  inline constexpr Direction SOUTH_EAST{-7};
  inline constexpr Direction SOUTH_WEST{-9};
  inline constexpr Direction NORTH_WEST{7};

  // Additional operators to add/subtract a Direction to/from a Square
  // Could be invalid Square if int value of Direction + int value of Square are >63
  ENABLE_BASE2_OPERATORS_ON(Square, Direction)
  // Enable arithmetic and increment operators via existing macros
  ENABLE_FULL_OPERATORS_ON(Direction)

  // Compile-time sanity checks
  static_assert(sizeof(Direction) == sizeof(int_fast8_t), "Direction should be 1 byte");

}// namespace chess

#endif// FRANKYCPP_DIRECTION_H
