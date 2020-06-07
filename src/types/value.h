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

#ifndef FRANKYCPP_VALUE_H
#define FRANKYCPP_VALUE_H

#include "macros.h"
#include "globals.h"
#include "piece.h"
#include "piecetype.h"
#include <cstdint>
#include <string>

enum Value : int16_t {
  VALUE_ZERO                = 0,
  VALUE_DRAW                = VALUE_ZERO,
  VALUE_ONE                 = 1,
  VALUE_INF                 = 15000,
  VALUE_NONE                = -VALUE_INF - VALUE_ONE,
  VALUE_MIN                 = -10000,
  VALUE_MAX                 = 10000,
  VALUE_CHECKMATE           = VALUE_MAX,
  VALUE_CHECKMATE_THRESHOLD = VALUE_CHECKMATE - static_cast<Value>(MAX_DEPTH) - 1,
};


// checks if value is a value of min - max
constexpr bool validValue(Value v) {
  return (v >= VALUE_MIN && v <= VALUE_MAX) || v == VALUE_NONE;
}

/** PieceType values */
constexpr const Value pieceTypeValue[] = {
  Value(0),   // no type
  Value(2000),// king
  Value(100), // pawn
  Value(320), // knight
  Value(330), // bishop
  Value(500), // rook
  Value(900), // queen
};

// returns the value of the given piece type
constexpr Value valueOf(const PieceType pt) { return pieceTypeValue[pt]; }

// returns the value of the given piece
constexpr Value valueOf(const Piece p) {
  const PieceType pieceType = typeOf(p);
  return pieceTypeValue[pieceType];
}

/** Returns true if value is considered a checkmate */
inline bool isCheckMateValue(const Value value) {
  return std::abs(value) > VALUE_CHECKMATE_THRESHOLD && std::abs(value) <= VALUE_CHECKMATE;
}

// Returns a UCI compatible std::string for the score in cp or in mate in ply
inline std::string str(Value value) {
  std::string scoreString;
  if (isCheckMateValue(value)) {
    scoreString = "mate ";
    scoreString += value < 0 ? "-" : "";
    scoreString += std::to_string((VALUE_CHECKMATE - std::abs(value) + 1) / 2);
  }
  else if (value == VALUE_NONE) {
    scoreString = "N/A";
  }
  else {
    scoreString = "cp " + std::to_string(value);
  }
  return scoreString;
}

inline std::ostream& operator<<(std::ostream& os, const Value v) {
  os << str(v);
  return os;
}

ENABLE_FULL_OPERATORS_ON(Value)

#endif//FRANKYCPP_VALUE_H
