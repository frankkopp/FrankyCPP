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

#ifndef FRANKYCPP_FILE_H
#define FRANKYCPP_FILE_H

#include "macros.h"
#include <format>

// File represents a chess board file a-h as a small class with an unsigned
// underlying value [0..8]
class File {
  std::uint8_t v_{};// 0..7 = A..H, 8 = NONE

public:
  // constructors
  constexpr File() : v_(8) {}
  constexpr explicit File(const unsigned v) : v_(v) {}
  constexpr explicit File(const int v) : v_(static_cast<unsigned>(v)) {}

  // underlying value access
  constexpr auto value() const { return v_; }

  /// implicit conversion for arithmetic/comparisons/array indexing
  // ReSharper disable once CppNonExplicitConversionOperator
  constexpr operator int() const { return v_; }

  /// Returns true if the file is valid (A-H).
  [[nodiscard]] constexpr bool isValid() const { return static_cast<int>(*this) < 8; }

  /// Returns the file as a character ('a'-'h'), or '-' if invalid.
  constexpr char toChar() const { return isValid() ? static_cast<char>('a' + static_cast<char>(static_cast<int>(*this))) : '-'; }

  /// Returns the file as a character string.
  constexpr char str() const { return toChar(); }

  /// Returns the distance between this file and another.
  constexpr int distance(const File other) const {
    const int d = static_cast<int>(other) - static_cast<int>(*this);
    return d < 0 ? -d : d;
  }

  /// Converts a file label ('a'-'h') to a File object, returns File{8} if invalid.
  static constexpr File fromChar(const char fileLabel) {
    const int idx = fileLabel - 'a';
    return 0 <= idx && idx < 8 ? File{idx} : File{8};
  }
};

// File constants
inline constexpr File FILE_A{0};
inline constexpr File FILE_B{1};
inline constexpr File FILE_C{2};
inline constexpr File FILE_D{3};
inline constexpr File FILE_E{4};
inline constexpr File FILE_F{5};
inline constexpr File FILE_G{6};
inline constexpr File FILE_H{7};
inline constexpr File FILE_NONE{8};
inline constexpr unsigned FILE_LENGTH = 9;

ENABLE_INCR_OPERATORS_ON(File)
ENABLE_COMPARISON_OPERATORS_ON(File)
ENABLE_MIXED_COMPARISONS_ON(File)
ENABLE_FORMATTER_AS_CHAR_ON(File);
ENABLE_OSTREAM_OPERATOR_AS_INT_ON(File);

#endif// FRANKYCPP_FILE_H
