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

#ifndef FRANKYCPP_MOVETYPE_H
#define FRANKYCPP_MOVETYPE_H

// MoveType is used for the different move types we use to encode moves.
//  Normal    = 0
//  Promotion = 1
//  EnPassant = 2
//  Castling  = 3
enum MoveType {
  NORMAL,
  PROMOTION,
  ENPASSANT,
  CASTLING
};

// checks if move type is a value of 0 - 3
constexpr bool validMoveType(MoveType mt) {
  return !(mt < 0); // || mt >= 4);
}

// single char label for the piece type (one of " KPNBRQ")
constexpr char str(MoveType mt) {
  if (!validMoveType(mt)) return '-';
  return std::string("npec")[mt];
}

inline std::ostream& operator<<(std::ostream& os, const MoveType mt) {
  os << str(mt);
  return os;
}

#endif//FRANKYCPP_MOVETYPE_H
