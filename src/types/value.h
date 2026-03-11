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

#ifndef FRANKYCPP_VALUE_H
#define FRANKYCPP_VALUE_H

//=============================================================================
// value.h - Chess Position Evaluation Value Type
//=============================================================================
//
// Value represents a position evaluation score in centipawns.
// Depends on: globals.h, macros.h, piece.h, piecetype.h
//
// Encoding:
//   16-bit signed integer (int16_t), range roughly -15000 to +15000
//   Positive = advantage for side to move, Negative = disadvantage
//
// Special Constants:
//   VALUE_ZERO   = 0       - Draw / equal position
//   VALUE_ONE    = 1       - Minimal unit
//   VALUE_MIN    = -10000  - Minimum "normal" evaluation
//   VALUE_MAX    = +10000  - Maximum "normal" evaluation (also checkmate)
//   VALUE_INF    = 15000   - Infinity (for alpha-beta bounds)
//   VALUE_NONE   = -15001  - Invalid / uninitialized
//   VALUE_CHECKMATE           = VALUE_MAX
//   VALUE_CHECKMATE_THRESHOLD = Threshold for mate scores
//
// Key Operations:
//   isValid()        - True if within valid range or VALUE_NONE
//   isCheckMate()    - True if value represents a checkmate score
//   str()            - UCI format ("cp 125" or "mate 3")
//   valueOf(pt)      - Piece type value (P=100, N=320, B=330, R=500, Q=900)
//   valueOf(piece)   - Piece value
//
// Usage:
//   Value eval = Value{150};           // +1.5 pawns advantage
//   Value mate = VALUE_CHECKMATE - 4;  // Mate in 2
//   std::string s = eval.str();        // "cp 150"
//
//=============================================================================

#include "globals.h"
#include "macros.h"
#include "piece.h"
#include "piecetype.h"
#include <cstdint>
#include <format>
#include <string>

namespace chess {

  class Value {
    int16_t v_{};

  public:
    // constructors
    constexpr Value() : v_{-15001} {}
    constexpr Value(const Value&) = default;
    constexpr explicit Value(const int v) : v_{static_cast<int16_t>(v)} {}

    // assignment
    constexpr Value& operator=(const Value&) = default;

    // accessors (aligned with File/Rank/Square)
    constexpr int16_t value() const { return v_; }

    // validation and classification (defined after VALUE_* constants)
    constexpr bool isValid() const;
    bool isCheckMate() const;

    // string representation (UCI-compatible: cp X or mate N) (defined after VALUE_* constants)
    std::string str() const;

    // keep unary plus; unary minus is provided by macros
    constexpr Value operator+() const { return *this; }

    // implicit conversion to int for backward compatibility (printing, comparisons, std::format helper)
    // Note: kept implicit for now to minimize churn; can be made explicit in a later step-by-step refactor.
    // ReSharper disable once CppNonExplicitConversionOperator
    constexpr operator int() const { return v_; }
  };

  static_assert(sizeof(Value) == sizeof(int16_t), "Value must stay 2 bytes");

  // Special: multiplication with double (used for gamePhaseValue)
  constexpr Value operator*(const Value a, const double d) { return Value{static_cast<int>(static_cast<int>(a) * d)}; }

  // Named constants preserved as global inline constexpr for minimal disruption
  inline constexpr Value VALUE_ZERO{0};
  inline constexpr Value VALUE_DRAW = VALUE_ZERO;
  inline constexpr Value VALUE_ONE{1};
  inline constexpr Value VALUE_INF{15000};
  inline constexpr Value VALUE_NONE{-(static_cast<int>(VALUE_INF) + static_cast<int>(VALUE_ONE))};
  inline constexpr Value VALUE_MIN{-10000};
  inline constexpr Value VALUE_MAX{10000};
  inline constexpr Value VALUE_CHECKMATE           = VALUE_MAX;
  inline constexpr Value VALUE_CHECKMATE_THRESHOLD = static_cast<Value>(VALUE_CHECKMATE - static_cast<int>(MAX_DEPTH) - 1);

  // Provide out-of-class definitions now that VALUE_* constants are visible
  constexpr bool Value::isValid() const {
    return (*this >= VALUE_MIN && *this <= VALUE_MAX) || *this == VALUE_NONE;
  }

  inline bool Value::isCheckMate() const {
    const int absVal = static_cast<int>(v_) < 0 ? -static_cast<int>(v_) : static_cast<int>(v_);
    return absVal > static_cast<int>(VALUE_CHECKMATE_THRESHOLD) && absVal <= static_cast<int>(VALUE_CHECKMATE);
  }

  inline std::string Value::str() const {
    if (isCheckMate()) {
      const bool neg   = static_cast<int>(v_) < 0;
      const int absVal = neg ? -static_cast<int>(v_) : static_cast<int>(v_);
      return std::string("mate ") + (neg ? "-" : "") + std::to_string((static_cast<int>(VALUE_CHECKMATE) - absVal + 1) / 2);
    }
    if (*this == VALUE_NONE) return "N/A";
    return std::string("cp ") + std::to_string(v_);
  }

  /** PieceType values */
  constexpr Value pieceTypeValue[] = {
    static_cast<Value>(0),    // no type
    static_cast<Value>(2000), // king
    static_cast<Value>(100),  // pawn
    static_cast<Value>(320),  // knight
    static_cast<Value>(330),  // bishop
    static_cast<Value>(500),  // rook
    static_cast<Value>(900),  // queen
  };

  // returns the value of the given piece type
  constexpr Value valueOf(const PieceType pt) { return pieceTypeValue[pt]; }

  // returns the value of the given piece
  constexpr Value valueOf(const Piece p) {
    const PieceType pieceType = typeOf(p);
    return pieceTypeValue[pieceType];
  }

  // Arithmetic and increments via shared macros (must be before constants using them)
  ENABLE_FULL_OPERATORS_ON(Value)
  ENABLE_INT_COMPOUND_ADDSUB_ON(Value)
  ENABLE_COMPARISON_OPERATORS_ON(Value)
  ENABLE_MIXED_COMPARISONS_ON(Value)
  ENABLE_OSTREAM_OPERATOR_AS_INT_ON(Value);

} // namespace chess

ENABLE_FORMATTER_AS_INT_ON(chess::Value);

#endif // FRANKYCPP_VALUE_H
