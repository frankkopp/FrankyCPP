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

#include <array>
#include <string>

#include "macros.h"
#include "square.h"

// CastlingRights now encapsulates the castling state as a tiny class with
// a single 8-bit field while preserving existing bitwise semantics.
class CastlingRights {
  uint_fast8_t v_{};// 4-bit bitmask used; kept as fast 8-bit storage

public:
  // constructors
  constexpr CastlingRights() = default;// defaults to NO_CASTLING (0)
  constexpr explicit CastlingRights(const uint_fast8_t v) : v_{v} {}

  // underlying value access
  [[nodiscard]] constexpr uint_fast8_t value() const { return v_; }

  // implicit conversion to int for indexing, comparisons, and gtest EXPECT_EQ
  // ReSharper disable once CppNonExplicitConversionOperator
  constexpr operator int() const { return v_; }

  // Queries
  [[nodiscard]] constexpr bool isEmpty() const { return v_ == 0; }
  [[nodiscard]] constexpr bool hasAny(const CastlingRights other) const { return (v_ & other.v_) != 0; }
  [[nodiscard]] constexpr bool hasAll(const CastlingRights other) const { return (v_ & other.v_) == other.v_; }

  // Mutating bitwise-combining operators (use OR and AND-NOT semantics)
  constexpr CastlingRights& operator+=(const CastlingRights rhs) {
    v_ = static_cast<uint_fast8_t>(v_ | rhs.v_);
    return *this;
  }
  constexpr CastlingRights& operator-=(const CastlingRights rhs) {
    v_ = static_cast<uint_fast8_t>(v_ & static_cast<uint_fast8_t>(~rhs.v_));
    return *this;
  }

  // String representation as used in FEN (e.g., "KQkq")
  std::string str() const {
    if (isEmpty()) return "-";
    std::string cr_str;
    // Use "has" semantics as before
    if (*this == CastlingRights{1u << 0u}) cr_str += "K";// WHITE_OO
    if (*this == CastlingRights{1u << 1u}) cr_str += "Q";// WHITE_OOO
    if (*this == CastlingRights{1u << 2u}) cr_str += "k";// BLACK_OO
    if (*this == CastlingRights{1u << 3u}) cr_str += "q";// BLACK_OOO
    return cr_str;
  }

  // Friends to preserve free-operator syntax and semantics
  friend constexpr CastlingRights operator+(const CastlingRights lhs, const CastlingRights rhs) {
    return CastlingRights{static_cast<uint_fast8_t>(lhs.v_ | rhs.v_)};
  }
  friend constexpr CastlingRights operator-(const CastlingRights lhs, const CastlingRights rhs) {
    return CastlingRights{static_cast<uint_fast8_t>(lhs.v_ & static_cast<uint_fast8_t>(~rhs.v_))};
  }
  friend constexpr CastlingRights operator&(const CastlingRights lhs, const CastlingRights rhs) {
    return CastlingRights{static_cast<uint_fast8_t>(lhs.v_ & rhs.v_)};
  }
  friend constexpr CastlingRights operator|(const CastlingRights lhs, const CastlingRights rhs) {
    return CastlingRights{static_cast<uint_fast8_t>(lhs.v_ | rhs.v_)};
  }

  // "Has castling right" semantics for equality (compatibility):
  // returns true if (lhs & rhs) != 0, or both are 0
  friend constexpr bool operator==(const CastlingRights lhs, const CastlingRights rhs) {
    return (lhs.v_ & rhs.v_) != 0u || (lhs.v_ == 0u && rhs.v_ == 0u);
  }
  friend constexpr bool operator!=(const CastlingRights lhs, const CastlingRights rhs) { return !(lhs == rhs); }
};

// -----------------------------------------------------------------------------
// Global inline constants for compatibility with existing code
// Bit layout (from LSB): WHITE_OO, WHITE_OOO, BLACK_OO, BLACK_OOO
inline constexpr CastlingRights NO_CASTLING{0};         // 0000
inline constexpr CastlingRights WHITE_OO{1u << 0u};     // 0001
inline constexpr CastlingRights WHITE_OOO{1u << 1u};    // 0010
inline constexpr CastlingRights WHITE_CASTLING{0b0011u};// 0011
inline constexpr CastlingRights BLACK_OO{1u << 2u};     // 0100
inline constexpr CastlingRights BLACK_OOO{1u << 3u};    // 1000
inline constexpr CastlingRights BLACK_CASTLING{0b1100u};// 1100
inline constexpr CastlingRights ANY_CASTLING{0b1111u};  // 1111
inline constexpr int CR_LENGTH = 16;

namespace Castling {
  // pre-determined constants for squares which influence castling rights
  inline constexpr std::array<CastlingRights, SQ_LENGTH> castlingRights = []() constexpr {
    std::array<CastlingRights, SQ_LENGTH> cr{};
    cr[SQ_E1] = WHITE_CASTLING;
    cr[SQ_A1] = WHITE_OOO;
    cr[SQ_H1] = WHITE_OO;
    cr[SQ_E8] = BLACK_CASTLING;
    cr[SQ_A8] = BLACK_OOO;
    cr[SQ_H8] = BLACK_OO;
    return cr;
  }();
}// namespace Castling

// returns a string representing the castling rights as used in a FEN (e.g. KQkq)
inline std::string str(const CastlingRights cr) { return cr.str(); }

inline std::ostream& operator<<(std::ostream& os, const CastlingRights cr) {
  os << cr.str();
  return os;
}

ENABLE_INCR_OPERATORS_ON(CastlingRights)

#endif// FRANKYCPP_CASTLINGRIGHTS_H
