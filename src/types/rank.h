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

#ifndef FRANKYCPP_RANK_H
#define FRANKYCPP_RANK_H

#include "color.h"
#include "macros.h"
#include <format>

// Rank represents a chess board rank 1-8 as a small class with an unsigned
// underlying value [0..8]
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

  /// Returns true if the rank is valid (between 0 and 7).
  [[nodiscard]] constexpr bool isValid() const { return static_cast<int>(*this) < 8; }

  /// Converts the rank to its character representation ('1'-'8'), or '-' if invalid.
  constexpr char toChar() const { return isValid() ? static_cast<char>('1' + static_cast<char>(static_cast<int>(*this))) : '-'; }

  /// Returns the character representation of the rank.
  constexpr char str() const { return toChar(); }

  /// Returns the absolute distance between this rank and another.
  constexpr int distance(const Rank other) const {
    const int d = static_cast<int>(other) - static_cast<int>(*this);
    return d < 0 ? -d : d;
  }

  /// Converts a character ('1'-'8') to a Rank; returns RANK_NONE for invalid input.
  static constexpr Rank fromChar(const char rankLabel) {
    const int idx = rankLabel - '1';
    return 0 <= idx && idx < 8 ? Rank{idx} : Rank{8};
  }

  /// Returns the promotion rank for the given color.
  static constexpr Rank promotionFor(const Color c) {
    return Rank{static_cast<unsigned>((c.sign() + 1) / 2 * 7)};
  }

  /// Returns the double pawn move rank for the given color.
  static constexpr Rank pawnDoubleFor(const Color c) {
    return Rank{static_cast<unsigned>((7 - 3 * c.sign()) / 2)};
  }
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

ENABLE_INCR_OPERATORS_ON(Rank)
ENABLE_COMPARISON_OPERATORS_ON(Rank)
ENABLE_MIXED_COMPARISONS_ON(Rank)
ENABLE_FORMATTER_AS_CHAR_ON(Rank);
ENABLE_OSTREAM_OPERATOR_AS_INT_ON(Rank);

#endif// FRANKYCPP_RANK_H
