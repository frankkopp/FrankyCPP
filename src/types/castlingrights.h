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

#ifndef FRANKYCPP_CASTLINGRIGHTS_H
#define FRANKYCPP_CASTLINGRIGHTS_H

#include "macros.h"
#include "square.h"
#include <iostream>

// CastlingRights encodes the castling state e.g. available castling
// and defines functions to change this state
//  NO_CASTLING = 0,                                 // 0000
//  WHITE_OO,                                        // 0001
//  WHITE_OOO      = WHITE_OO << 1,                  // 0010
//  WHITE_CASTLING = WHITE_OO | WHITE_OOO,           // 0011
//  BLACK_OO       = WHITE_OO << 2,                  // 0100
//  BLACK_OOO      = WHITE_OO << 3,                  // 1000
//  BLACK_CASTLING = BLACK_OO | BLACK_OOO,           // 1100
//  ANY_CASTLING   = WHITE_CASTLING | BLACK_CASTLING,// 1111
//  CR_LENGTH      = 16
enum CastlingRights : uint_fast8_t {
  NO_CASTLING = 0,                                 // 0000
  WHITE_OO,                                        // 0001
  WHITE_OOO      = WHITE_OO << 1,                  // 0010
  WHITE_CASTLING = WHITE_OO | WHITE_OOO,           // 0011
  BLACK_OO       = WHITE_OO << 2,                  // 0100
  BLACK_OOO      = WHITE_OO << 3,                  // 1000
  BLACK_CASTLING = BLACK_OO | BLACK_OOO,           // 1100
  ANY_CASTLING   = WHITE_CASTLING | BLACK_CASTLING,// 1111
  CR_LENGTH      = 16
};

namespace Castling {
  // pre determined constants for squares which influence castling rights
  inline CastlingRights castlingRights[SQ_LENGTH];
  inline void initCastlingRights() {
    castlingRights[SQ_E1] = WHITE_CASTLING;
    castlingRights[SQ_A1] = WHITE_OOO;
    castlingRights[SQ_H1] = WHITE_OO;
    castlingRights[SQ_E8] = BLACK_CASTLING;
    castlingRights[SQ_A8] = BLACK_OOO;
    castlingRights[SQ_H8] = BLACK_OO;
  }
}// namespace Castling

// remove castling rights
constexpr CastlingRights operator-(CastlingRights cr1, CastlingRights cr2) {
  return static_cast<CastlingRights>(cr1 & ~cr2);
}

// remove castling rights
constexpr CastlingRights& operator-=(CastlingRights& cr1, CastlingRights cr2) {
  return cr1 = static_cast<CastlingRights>(cr1 & ~cr2);
}

// add castling rights
constexpr CastlingRights operator+(CastlingRights cr1, CastlingRights cr2) {
  return static_cast<CastlingRights>(cr1 | cr2);
}

// add castling rights
constexpr CastlingRights& operator+=(CastlingRights& cr1, CastlingRights cr2) {
  return cr1 = static_cast<CastlingRights>(cr1 | cr2);
}

// Has castling right
constexpr bool operator==(CastlingRights cr1, CastlingRights cr2) {
  return (cr1 & cr2) || (cr1 == 0 && cr2 == 0);
}

// returns a string representing the castling rights as used in a FEN (e.g. KQkq)
inline std::string str(CastlingRights cr) {
  if (cr == NO_CASTLING) {
    return "-";
  }
  std::string cr_str;
  if (cr == WHITE_OO) cr_str += "K";
  if (cr == WHITE_OOO) cr_str += "Q";
  if (cr == BLACK_OO) cr_str += "k";
  if (cr == BLACK_OOO) cr_str += "q";
  return cr_str;
}

inline std::ostream& operator<<(std::ostream& os, const CastlingRights cr) {
  os << str(cr);
  return os;
}

ENABLE_INCR_OPERATORS_ON(CastlingRights)

#endif//FRANKYCPP_CASTLINGRIGHTS_H
