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

#ifndef FRANKYCPP_VALUE_H
#define FRANKYCPP_VALUE_H

#include "globals.h"
#include "piece.h"
#include "piecetype.h"
#include <cstdint>
#include <format>
#include <string>

// Small value class wrapping a 16-bit signed score.
class Value {
  int16_t v_{};

public:
  // constructors
  constexpr Value()             = default;
  constexpr Value(const Value&) = default;
  constexpr explicit Value(const int v) : v_{static_cast<int16_t>(v)} {}

  // assignment
  constexpr Value& operator=(const Value&) = default;

  // accessors (aligned with File/Rank/Square)
  constexpr int16_t value() const { return v_; }
  constexpr int toInt() const { return static_cast<int>(v_); }

  // validation and classification (defined after VALUE_* constants)
  [[nodiscard]] constexpr bool isValid() const;
  [[nodiscard]] bool isCheckMate() const;

  // string representation (UCI-compatible: cp X or mate N) (defined after VALUE_* constants)
  std::string str() const;

  // clang-format off
  // unary
  constexpr Value operator+() const { return *this; }
  constexpr Value operator-() const { return Value{-toInt()}; }

  // compound assign with Value
  constexpr Value& operator+=(const Value other) { v_ = static_cast<int16_t>(toInt() + other.toInt()); return *this; }
  constexpr Value& operator-=(const Value other) { v_ = static_cast<int16_t>(toInt() - other.toInt()); return *this; }
  // compound with int
  constexpr Value& operator+=(const int i) { v_ = static_cast<int16_t>(toInt() + i); return *this; }
  constexpr Value& operator-=(const int i) { v_ = static_cast<int16_t>(toInt() - i); return *this; }
  constexpr Value& operator*=(const int i) { v_ = static_cast<int16_t>(toInt() * i); return *this; }
  constexpr Value& operator/=(const int i) { v_ = static_cast<int16_t>(toInt() / i); return *this; }

  // increment/decrement
  constexpr Value& operator++() { v_ = static_cast<int16_t>(toInt() + 1); return *this; }
  constexpr Value& operator--() { v_ = static_cast<int16_t>(toInt() - 1); return *this; }

  // comparisons with Value
  friend constexpr bool operator==(const Value a, const Value b) { return a.v_ == b.v_; }
  friend constexpr bool operator!=(const Value a, const Value b) { return !(a == b); }
  friend constexpr bool operator<(const Value a, const Value b) { return a.toInt() < b.toInt(); }
  friend constexpr bool operator>(const Value a, const Value b) { return b < a; }
  friend constexpr bool operator<=(const Value a, const Value b) { return b >= a; }
  friend constexpr bool operator>=(const Value a, const Value b) { return !(a < b); }

  // arithmetic with Value
  friend constexpr Value operator+(const Value a, const Value b) { return Value{a.toInt() + b.toInt()}; }
  friend constexpr Value operator-(const Value a, const Value b) { return Value{a.toInt() - b.toInt()}; }

  // arithmetic with int
  friend constexpr Value operator+(const Value a, const int i) { return Value{a.toInt() + i}; }
  friend constexpr Value operator+(const int i, const Value a) { return Value{i + a.toInt()}; }
  friend constexpr Value operator-(const Value a, const int i) { return Value{a.toInt() - i}; }
  friend constexpr Value operator-(const int i, const Value a) { return Value{i - a.toInt()}; }
  friend constexpr Value operator*(const Value a, const int i) { return Value{a.toInt() * i}; }
  friend constexpr Value operator*(const int i, const Value a) { return Value{i * a.toInt()}; }
  friend constexpr Value operator/(const Value a, const int i) { return Value{a.toInt() / i}; }
  // division Value/Value returns int (as before via macros)
  friend constexpr int operator/(const Value a, const Value b) { return a.toInt() / b.toInt(); }

  // special: multiplication with double (used for gamePhaseValue)
  friend constexpr Value operator*(const Value a, const double d) { return Value{static_cast<int>(a.toInt() * d)}; }
  // clang-format on

  // stream output handled via str() below

  // implicit conversion to int for backward compatibility (printing, comparisons, std::format helper)
  // Note: kept implicit for now to minimize churn; can be made explicit in a later step-by-step refactor.
  // ReSharper disable once CppNonExplicitConversionOperator
  constexpr operator int() const { return toInt(); }
};

static_assert(sizeof(Value) == sizeof(int16_t), "Value must stay 2 bytes");

// Named constants preserved as global inline constexpr for minimal disruption
inline constexpr Value VALUE_ZERO{0};
inline constexpr Value VALUE_DRAW = VALUE_ZERO;
inline constexpr Value VALUE_ONE{1};
inline constexpr Value VALUE_INF{15000};
inline constexpr Value VALUE_NONE{-(static_cast<int>(VALUE_INF) + static_cast<int>(VALUE_ONE))};
inline constexpr Value VALUE_MIN{-10000};
inline constexpr Value VALUE_MAX{10000};
inline constexpr Value VALUE_CHECKMATE           = VALUE_MAX;
inline constexpr Value VALUE_CHECKMATE_THRESHOLD = VALUE_CHECKMATE - static_cast<int>(MAX_DEPTH) - 1;

// Provide out-of-class definitions now that VALUE_* constants are visible
constexpr bool Value::isValid() const {
  return (*this >= VALUE_MIN && *this <= VALUE_MAX) || *this == VALUE_NONE;
}

inline bool Value::isCheckMate() const {
  const int absVal = toInt() < 0 ? -toInt() : toInt();
  return absVal > static_cast<int>(VALUE_CHECKMATE_THRESHOLD) && absVal <= static_cast<int>(VALUE_CHECKMATE);
}

inline std::string Value::str() const {
  if (isCheckMate()) {
    const bool neg   = toInt() < 0;
    const int absVal = neg ? -toInt() : toInt();
    return std::string("mate ") + (neg ? "-" : "") + std::to_string((static_cast<int>(VALUE_CHECKMATE) - absVal + 1) / 2);
  }
  if (*this == VALUE_NONE) return "N/A";
  return std::string("cp ") + std::to_string(toInt());
}

/** PieceType values */
constexpr Value pieceTypeValue[] = {
  static_cast<Value>(0),   // no type
  static_cast<Value>(2000),// king
  static_cast<Value>(100), // pawn
  static_cast<Value>(320), // knight
  static_cast<Value>(330), // bishop
  static_cast<Value>(500), // rook
  static_cast<Value>(900), // queen
};

// returns the value of the given piece type
constexpr Value valueOf(const PieceType pt) { return pieceTypeValue[pt]; }

// returns the value of the given piece
constexpr Value valueOf(const Piece p) {
  const PieceType pieceType = typeOf(p);
  return pieceTypeValue[pieceType];
}

// Returns a UCI compatible std::string for the score in cp or in mate in ply
inline std::ostream& operator<<(std::ostream& os, const Value v) {
  os << v.str();
  return os;
}


// Make Value usable with std::format (C++20)
template<>
struct std::formatter<Value> : formatter<int> {
  template<typename FormatContext>
  auto format(const Value v, FormatContext& ctx) const {
    return formatter<int>::format(static_cast<int>(v), ctx);
  }
};

#endif// FRANKYCPP_VALUE_H
