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

#ifndef FRANKYCPP_COLOR_H
#define FRANKYCPP_COLOR_H

#include "macros.h"

// Color represents chess side to move as a small class with an unsigned underlying value
//  WHITE   = 0,
//  BLACK   = 1,
//  NOCOLOR = 2
class Color {
  unsigned v_{}; // 0..1 valid, 2 = NONE

public:
  // number of valid colors (without NOCOLOR)
  static constexpr int LENGTH = 2;

  // constructors
  constexpr Color() : v_(2) {}
  constexpr explicit Color(const unsigned v) : v_(v) {}
  constexpr explicit Color(const int v) : v_(static_cast<unsigned>(v)) {}

  // underlying value access
  [[nodiscard]] constexpr unsigned value() const { return v_; }

  // implicit conversion for arithmetic/comparisons/array indexing
  // ReSharper disable once CppNonExplicitConversionOperator
  constexpr operator int() const { return static_cast<int>(v_); }

  // member helpers
  [[nodiscard]] constexpr bool isValid() const { return static_cast<int>(*this) < 2; }
  [[nodiscard]] constexpr char toChar() const { return isValid() ? (static_cast<int>(*this) == 0 ? 'w' : 'b') : '-'; }
  [[nodiscard]] constexpr char str() const { return toChar(); }

  // convenience
  [[nodiscard]] constexpr Color opposite() const { return Color{(value() ^ 1U)}; }
  // sign() allows to avoid branches in some calculations
  [[nodiscard]] constexpr int   sign() const { return static_cast<int>(*this) == 0 ? 1 : -1; }

  // iterator support over valid colors [WHITE, BLACK]
  class iterator {
    int cur_{}; // index of current color [0..LENGTH]
  public:
    using value_type = Color;
    using difference_type = int;
    using reference = Color; // value-like
    using pointer = void;

    constexpr explicit iterator(const int c) : cur_(c) {}
    constexpr Color operator*() const { return Color{cur_}; }
    constexpr iterator& operator++() {
      ++cur_;
      return *this;
    }
    friend constexpr bool operator==(const iterator& a, const iterator& b) { return a.cur_ == b.cur_; }
    friend constexpr bool operator!=(const iterator& a, const iterator& b) { return !(a == b); }
  };

  class range {
  public:
    static constexpr iterator begin() { return iterator{0}; }
    static constexpr iterator end() { return iterator{LENGTH}; }
  };

  // Returns an iterable over WHITE and BLACK (excludes NOCOLOR)
  [[nodiscard]] static constexpr range all() { return range{}; }
};

// Backward-compatible constants and sizes
inline constexpr Color WHITE{0};
inline constexpr Color BLACK{1};
inline constexpr Color NOCOLOR{2};
inline constexpr unsigned COLOR_LENGTH = Color::LENGTH;

// returns the opposite color (kept for compatibility)
constexpr Color operator~(const Color c) { return Color{(c.value() ^ 1U)}; }

ENABLE_INCR_OPERATORS_ON (Color)
ENABLE_OSTREAM_OPERATOR_ON (Color)

#endif//FRANKYCPP_COLOR_H
