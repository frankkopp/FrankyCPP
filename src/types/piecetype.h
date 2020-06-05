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

#ifndef FRANKYCPP_PIECETYPE_H
#define FRANKYCPP_PIECETYPE_H

#include <ostream>
#include <string>

enum PieceType : int {
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
constexpr bool validPieceType(PieceType pt) {
  return !(pt < 0 || pt >= 7);
}

// returns the game phase value of the piece type to
// compute the current game phase in relation to the
// pieces currently on the board.
constexpr int gamePhaseValue(PieceType pt) {
  const int gamePhaseValue[] = {
    0,// no type
    0,// king
    0,// pawn
    1,// knight
    1,// bishop
    2,// rook
    4 // queen
  };
  return gamePhaseValue[pt];
}

// single char label for the piece type (one of " KPNBRQ")
constexpr char pieceTypeLabel(PieceType pt) {
  if (!validPieceType(pt)) return '-';
  return std::string(" KPNBRQ")[pt];
}

inline std::ostream& operator<<(std::ostream& os, const PieceType pt) {
  os << pieceTypeLabel(pt);
  return os;
}

#endif//FRANKYCPP_PIECETYPE_H
