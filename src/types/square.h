/*
 * MIT License
 *
 * Copyright (c) 2020 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef FRANKYCPP_SQUARE_H
#define FRANKYCPP_SQUARE_H

#include <ostream>

#include "macros.h"
#include "color.h"
#include "file.h"
#include "rank.h"

// Square represent exactly on square on a chess board.
//  SqA1   // 0
//  SqB1   // 1
//  SqC1
//  SqD1
//  ...
//  SqG8
//  SqH8   // 63
//  SqNone // 64
enum Square : int { // @formatter:off
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
// @formatter:on

// precomputed arrays
namespace Squares {
  inline int squareDistance[SQ_NONE][SQ_NONE];
  inline int centerDistance[SQ_LENGTH];
}

// checks if this is a valid square (int >= 0 and <64)
constexpr bool validSquare(Square s) { return !(s & ~0b11'1111); }

// returns the square of the intersection of file and rank
constexpr Square squareOf (File f, Rank r) { return Square ((r << 3) + f); }

// returns the file of this square
constexpr File fileOf (Square s) { return File (s & 7); }

// returns the rank of this square
constexpr Rank rankOf (Square s) { return Rank (s >> 3); }

// creates a square from a string (uci style square e.g. e2, h7)
inline Square makeSquare(std::string s) {
  const File f = makeFile(s[0]);
  const Rank r = makeRank(s[1]);
  if (validFile(f) && validRank(r)) {
      return squareOf(f,r);
  }
  return SQ_NONE;
}

// returns the precomputed distance between two squares
constexpr int distance(Square s1, Square s2) { return Squares::squareDistance[s1][s2]; }

// pawnPush returns the square of a pawn move of the given color
constexpr Square pawnPush(Square s, Color c) { return static_cast<Square>(s + (c == WHITE ? 8 : -8)); }

// returns a string representing the square (e.g. a1 or h8)
inline std::string str(Square sq) {
  return validSquare(sq) ? std::string{str(fileOf(sq)), str(rankOf(sq)) } : "--";
}

inline std::ostream& operator<< (std::ostream& os, const Square sq) {
  os << str(sq);
  return os;
}

ENABLE_INCR_OPERATORS_ON (Square)

// precomputed by types::init()
namespace Squares {

  inline void squareDistancePreCompute() {
    // distance between squares
    for (Square sq1 = SQ_A1; sq1 <= SQ_H8; ++sq1) {
      for (Square sq2 = SQ_A1; sq2 <= SQ_H8; ++sq2) {
        if (sq1 != sq2) {
          squareDistance[sq1][sq2] =
            static_cast<int8_t>(std::max(distance(fileOf(sq1), fileOf(sq2)),
                                         distance(rankOf(sq1), rankOf(sq2))));
        }
      }
    }
  }

  inline void centerDistancePreCompute() {
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {
      // left upper quadrant
      if (fileOf(sq) <= FILE_D && rankOf(sq) >= RANK_5) {
        centerDistance[sq] = squareDistance[sq][SQ_D5];
        // right upper quadrant
      } else if (fileOf(sq) >= FILE_E && rankOf(sq) >= RANK_5) {
        centerDistance[sq] = squareDistance[sq][SQ_E5];
        // left lower quadrant
      } else if (fileOf(sq) <= FILE_D && rankOf(sq) <= RANK_4) {
        centerDistance[sq] = squareDistance[sq][SQ_D4];
        // right lower quadrant
      } else if (fileOf(sq) >= FILE_E && rankOf(sq) <= RANK_4) {
        centerDistance[sq] = squareDistance[sq][SQ_E4];
      }
    }
  }
}

#endif//FRANKYCPP_SQUARE_H
