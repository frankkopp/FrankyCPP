// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
//
// MIT License
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef FRANKYCPP_RANK_H
#define FRANKYCPP_RANK_H

#include "color.h"
#include "macros.h"

// Rank represents a chess board rank 1-8 as a small class with an unsigned underlying value [0..8]
class Rank {
  std::uint8_t v_{};// 0..7 = 1..8, 8 = NONE

public:
  // constructors
  constexpr Rank() : v_(8) {}
  constexpr explicit Rank(const unsigned v) : v_(v) {}
  constexpr explicit Rank(const int v) : v_(static_cast<unsigned>(v)) {}

  // underlying value access
  constexpr auto value() const { return v_; }

  // implicit conversion for arithmetic/comparisons/array indexing
  // ReSharper disable once CppNonExplicitConversionOperator
  constexpr operator int() const { return v_; }

  // member helpers
  [[nodiscard]] constexpr bool isValid() const { return static_cast<int>(*this) < 8; }
  constexpr char toChar() const { return isValid() ? static_cast<char>('1' + static_cast<char>(static_cast<int>(*this))) : '-'; }
  constexpr char str() const { return toChar(); }
  constexpr int distance(const Rank other) const {
    const int d = static_cast<int>(other) - static_cast<int>(*this);
    return d < 0 ? -d : d;
  }

  // static helpers
  static constexpr Rank fromChar(const char rankLabel) {
    const int idx = rankLabel - '1';
    return 0 <= idx && idx < 8 ? Rank{idx} : Rank{8};
  }
  static constexpr Rank promotionFor(const Color c) { return Rank{static_cast<unsigned>((c.sign() + 1) / 2 * 7)}; }
  static constexpr Rank pawnDoubleFor(const Color c) { return Rank{static_cast<unsigned>((7 - 3 * c.sign()) / 2)}; }
};

// Rank constants
inline constexpr Rank RANK_1{0};
inline constexpr Rank RANK_2{1};
inline constexpr Rank RANK_3{2};
inline constexpr Rank RANK_4{3};
inline constexpr Rank RANK_5{4};
inline constexpr Rank RANK_6{5};
inline constexpr Rank RANK_7{6};
inline constexpr Rank RANK_8{7};
inline constexpr Rank RANK_NONE{8};
inline constexpr unsigned RANK_LENGTH = 9;

inline std::ostream& operator<<(std::ostream& os, const Rank r) {
  os << r.str();
  return os;
}

ENABLE_INCR_OPERATORS_ON(Rank)

// Comparison operators for Rank
constexpr bool operator==(const Rank a, const Rank b) { return a.value() == b.value(); }
constexpr bool operator!=(const Rank a, const Rank b) { return a.value() != b.value(); }
constexpr bool operator<(const Rank a, const Rank b) { return a.value() < b.value(); }
constexpr bool operator<=(const Rank a, const Rank b) { return a.value() <= b.value(); }
constexpr bool operator>(const Rank a, const Rank b) { return a.value() > b.value(); }
constexpr bool operator>=(const Rank a, const Rank b) { return a.value() >= b.value(); }

// Mixed comparisons: Rank vs int
constexpr bool operator==(const Rank a, const int b) { return static_cast<int>(a.value()) == b; }
constexpr bool operator!=(const Rank a, const int b) { return static_cast<int>(a.value()) != b; }
constexpr bool operator<(const Rank a, const int b) { return static_cast<int>(a.value()) < b; }
constexpr bool operator<=(const Rank a, const int b) { return static_cast<int>(a.value()) <= b; }
constexpr bool operator>(const Rank a, const int b) { return static_cast<int>(a.value()) > b; }
constexpr bool operator>=(const Rank a, const int b) { return static_cast<int>(a.value()) >= b; }

// Mixed comparisons: int vs Rank
constexpr bool operator==(const int a, const Rank b) { return a == static_cast<int>(b.value()); }
constexpr bool operator!=(const int a, const Rank b) { return a != static_cast<int>(b.value()); }
constexpr bool operator<(const int a, const Rank b) { return a < static_cast<int>(b.value()); }
constexpr bool operator<=(const int a, const Rank b) { return a <= static_cast<int>(b.value()); }
constexpr bool operator>(const int a, const Rank b) { return a > static_cast<int>(b.value()); }
constexpr bool operator>=(const int a, const Rank b) { return a >= static_cast<int>(b.value()); }

#endif// FRANKYCPP_RANK_H
