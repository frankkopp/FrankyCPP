// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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

#ifndef FRANKYCPP_BITBOARD_H
#define FRANKYCPP_BITBOARD_H

#include "direction.h"
#include "piecetype.h"
#include "square.h"

#include <bit>
#include <bitset>
#include <cstdint>

// 64-bit Bitboard type for storing boards as bits
typedef uint64_t Bitboard;

// //////////////////////////////////////////////////////////////////
// Bitboard functions
// //////////////////////////////////////////////////////////////////

/**
 * Shifts a bitboard in the given direction.
 * @param d Direction
 * @param b Bitboard
 * @return shifted bitboard
 */
inline Bitboard shiftBb(const Direction d, const Bitboard b) {
  constexpr Bitboard FileABB = 0x0101010101010101ULL;
  constexpr Bitboard FileHBB = FileABB << 7;
  // move the bits and clear the left our right file
  // after the shift to erase bit jumping over
  switch (d) {
    case static_cast<int>(NORTH):
      return b << 8;
    case static_cast<int>(EAST):
      return b << 1 & ~FileABB;
    case static_cast<int>(SOUTH):
      return b >> 8;
    case static_cast<int>(WEST):
      return b >> 1 & ~FileHBB;
    case static_cast<int>(NORTH_EAST):
      return b << 9 & ~FileABB;
    case static_cast<int>(SOUTH_EAST):
      return b >> 7 & ~FileABB;
    case static_cast<int>(SOUTH_WEST):
      return b >> 9 & ~FileHBB;
    case static_cast<int>(NORTH_WEST):
      return b << 7 & ~FileHBB;
    default:;
  }
  return b;
}


// popcount() counts the number of non-zero bits in a bitboard
// Kept in a separate function to allow easy replacement if no built-in
// popcount is available for compiler.
// @return number of non-zero bits
inline int popcount(const Bitboard b) {
  return std::popcount(b);
}

// lsb() and msb() return the least/most significant bit in a non-zero bitboard
inline Square lsb(const Bitboard b) {
  if (!b) return SQ_NONE;
  return static_cast<Square>(std::countr_zero(b));
}

// lsb() and msb() return the least/most significant bit in a non-zero bitboard
inline Square msb(const Bitboard b) {
  if (!b) return SQ_NONE;
  return static_cast<Square>(63 - std::countl_zero(b));
}

// pop_lsb() finds and clears the least significant bit in a non-zero
// bitboard. Returns the cleared bit as a Square or SQ_NONE if
// bitboard was zero. The given Bitboard is changed in-place.
// Example:
// Bitboard b = ...;
// while (b) {
//   Square s = pop_lsb(b);
//   ...
// }
inline Square popLSB(Bitboard& b) {
  if (!b) return SQ_NONE;
  const Square s = lsb(b);
  b &= b - 1;
  return s;
}

// //////////////////////////////////////////////////////////////////
// Bitboard print functions
// //////////////////////////////////////////////////////////////////

// Prints a bitboard as a bitset
inline std::string str(const Bitboard b) {
  std::ostringstream os;
  os << std::bitset<64>(b);
  return os.str();
}

// Prints a bitboard in an 8x8 matrix for output on a console
inline std::string strBoard(const Bitboard b) {
  std::ostringstream os;
  os << "+---+---+---+---+---+---+---+---+\n";
  for (Rank r = RANK_8;; --r) {
    for (File f = FILE_A; f <= FILE_H; ++f) {
      os << (b & Square::of(f, r) ? "| X " : "|   ");
    }
    os << "|\n+---+---+---+---+---+---+---+---+\n";
    if (r == 0) break;
  }
  return os.str();
}

// StringGrouped returns a string representation of the 64 bits grouped in 8.
// Order is LSB to msb ==> A1 B1 ... G8 H8
inline std::string strGrouped(const Bitboard b) {
  std::ostringstream os;
  for (uint16_t i = 0; i < 64; i++) {
    if (i > 0 && i % 8 == 0) {
      os << ".";
    }
    os << (b & 1 << i ? "1" : "0");
  }
  os << " (" + std::to_string(b) + ")";
  return os.str();
}

#endif// FRANKYCPP_BITBOARD_H
