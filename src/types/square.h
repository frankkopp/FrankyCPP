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

#ifndef FRANKYCPP_SQUARE_H
#define FRANKYCPP_SQUARE_H

#include "color.h"
#include "file.h"
#include "macros.h"
#include "rank.h"

#include <array>
#include <format>

// Square represent exactly on square on a chess board.
//  SqA1   // 0
//  SqB1   // 1
//  SqC1
//  SqD1
//  ...
//  SqG8
//  SqH8   // 63
//  SqNone // 64
// clang-format off
enum Square : uint_fast8_t {
  SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
  SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
  SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
  SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
  SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
  SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
  SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
  SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
  SQ_NONE,
  SQ_LENGTH = 64
};
// clang-format on

// checks if this is a valid square (int >= 0 and <64)
constexpr bool validSquare(const Square s) { return s < 64; }

// returns the square of the intersection of file and rank
constexpr Square squareOf(const File f, const Rank r) { return static_cast<Square>((r << 3) + f); }

// returns the file of this square
constexpr File fileOf(const Square s) { return static_cast<File>(s & 0b00000111u); }

// returns the rank of this square
constexpr Rank rankOf(const Square s) { return static_cast<Rank>(s >> 3); }

// creates a square from a string (uci style square e.g. e2, h7)
// only considers the first and second character, rest is ignored
// returns SQ_NONE if not a valid square
inline Square makeSquare(const std::string_view s) {
  if (s.length() < 2) return SQ_NONE;
  const File f = makeFile(s[0]);
  const Rank r = makeRank(s[1]);
  if (validFile(f) && validRank(r)) {
    return squareOf(f, r);
  }
  return SQ_NONE;
}

// creates a square from a string (uci style square e.g., e2, h7)
// only considers the first and second character; the rest is ignored
// returns SQ_NONE if not a valid square
inline Square makeSquare(const std::string& s) {
  return makeSquare(std::string_view{s});
}

// ENABLE_INCR_OPERATORS_ON(Square)
constexpr Square& operator++(Square& d) { return d = static_cast<Square>(static_cast<int>(d) + 1); }
constexpr Square& operator--(Square& d) { return d = static_cast<Square>(static_cast<int>(d) - 1); }

namespace Squares {
  constexpr std::array<std::array<int, SQ_NONE>, SQ_NONE> squareDistancePreCompute() {
    std::array<std::array<int, SQ_NONE>, SQ_NONE> dist{}; // zero-initialize (diagonal stays 0)
    // distance between squares (Chebyshev distance)
    for (Square sq1 = SQ_A1; sq1 <= SQ_H8; ++sq1) {
      for (Square sq2 = SQ_A1; sq2 <= SQ_H8; ++sq2) {
        if (sq1 != sq2) {
          const int f1   = fileOf(sq1);
          const int f2   = fileOf(sq2);
          const int r1   = rankOf(sq1);
          const int r2   = rankOf(sq2);
          const int df   = f1 > f2 ? (f1 - f2) : (f2 - f1);
          const int dr   = r1 > r2 ? (r1 - r2) : (r2 - r1);
          dist[sq1][sq2] = df > dr ? df : dr;
        }
      }
    }
    return dist;
  }
  // precomputed distances between all squares
  inline constexpr std::array<std::array<int, SQ_NONE>, SQ_NONE> squareDistance = squareDistancePreCompute();

  constexpr std::array<int, SQ_LENGTH> centerDistancePreCompute() {
    std::array<int, SQ_LENGTH> cd{};
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {
      if (fileOf(sq) <= FILE_D && rankOf(sq) >= RANK_5) {
        cd[sq] = squareDistance[sq][SQ_D5];
      }
      else if (fileOf(sq) >= FILE_E && rankOf(sq) >= RANK_5) {
        cd[sq] = squareDistance[sq][SQ_E5];
      }
      else if (fileOf(sq) <= FILE_D && rankOf(sq) <= RANK_4) {
        cd[sq] = squareDistance[sq][SQ_D4];
      }
      else if (fileOf(sq) >= FILE_E && rankOf(sq) <= RANK_4) {
        cd[sq] = squareDistance[sq][SQ_E4];
      }
    }
    return cd;
  }
  // precomputed distances from center squares (d4, d5, e4, e5)
  inline constexpr std::array<int, SQ_LENGTH> centerDistance = centerDistancePreCompute();

  // precomputed square names as char arrays for fast access
  inline constexpr std::array<std::array<char, 3>, SQ_LENGTH> squareNames = []() {
    std::array<std::array<char, 3>, SQ_LENGTH> names{};
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {
      names[sq] = {str(fileOf(sq)), str(rankOf(sq)), '\0'};
    }
    return names;
  }();
}// namespace Squares

// returns the precomputed distance between two squares
constexpr int distance(const Square s1, const Square s2) { return Squares::squareDistance[s1][s2]; }

// pawnPush returns the square of a pawn move of the given color
constexpr Square pawnPush(const Square s, const Color c) { return static_cast<Square>(s + (c == WHITE ? 8 : -8)); }

// returns a string representing the square (e.g. a1 or h8)
inline std::string str(const Square sq) {
  return Squares::squareNames[sq].data();
}

// stream output operator for Square
inline std::ostream& operator<<(std::ostream& os, const Square sq) {
  os << str(sq);
  return os;
}

// Make Square usable with std::format
template<>
struct std::formatter<Square> : formatter<string_view> {
  template<typename FormatContext>
  auto format(const Square sq, FormatContext& ctx) const {
    return formatter<string_view>::format(str(sq), ctx);
  }
};

#endif// FRANKYCPP_SQUARE_H
