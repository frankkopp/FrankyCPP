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

// File represents a chess board file a-h as a small class with an unsigned underlying value [0..8]
class File {
  std::uint8_t v_{};// 0..7 = A..H, 8 = NONE

public:
  // constructors
  constexpr File() : v_(8) {}
  constexpr explicit File(const unsigned v) : v_(v) {}
  constexpr explicit File(const int v) : v_(static_cast<unsigned>(v)) {}

  // underlying value access
  constexpr auto value() const { return v_; }

  // implicit conversion for arithmetic/comparisons/array indexing
  // ReSharper disable once CppNonExplicitConversionOperator
  constexpr operator int() const { return v_; }

  // member helpers
  [[nodiscard]] constexpr bool isValid() const { return static_cast<int>(*this) < 8; }
  constexpr char toChar() const { return isValid() ? static_cast<char>('a' + static_cast<char>(static_cast<int>(*this))) : '-'; }
  constexpr char str() const { return toChar(); }
  constexpr int distance(const File other) const {
    const int d = static_cast<int>(other) - static_cast<int>(*this);
    return d < 0 ? -d : d;
  }

  // static helpers
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

inline std::ostream& operator<<(std::ostream& os, const File f) {
  os << f.str();
  return os;
}

ENABLE_INCR_OPERATORS_ON(File)

// Comparison operators for File
constexpr bool operator==(const File a, const File b) { return a.value() == b.value(); }
constexpr bool operator!=(const File a, const File b) { return a.value() != b.value(); }
constexpr bool operator<(const File a, const File b) { return a.value() < b.value(); }
constexpr bool operator<=(const File a, const File b) { return a.value() <= b.value(); }
constexpr bool operator>(const File a, const File b) { return a.value() > b.value(); }
constexpr bool operator>=(const File a, const File b) { return a.value() >= b.value(); }

// Mixed comparisons: File vs int
constexpr bool operator==(const File a, const int b) { return static_cast<int>(a.value()) == b; }
constexpr bool operator!=(const File a, const int b) { return static_cast<int>(a.value()) != b; }
constexpr bool operator<(const File a, const int b) { return static_cast<int>(a.value()) < b; }
constexpr bool operator<=(const File a, const int b) { return static_cast<int>(a.value()) <= b; }
constexpr bool operator>(const File a, const int b) { return static_cast<int>(a.value()) > b; }
constexpr bool operator>=(const File a, const int b) { return static_cast<int>(a.value()) >= b; }

// Mixed comparisons: int vs File
constexpr bool operator==(const int a, const File b) { return a == static_cast<int>(b.value()); }
constexpr bool operator!=(const int a, const File b) { return a != static_cast<int>(b.value()); }
constexpr bool operator<(const int a, const File b) { return a < static_cast<int>(b.value()); }
constexpr bool operator<=(const int a, const File b) { return a <= static_cast<int>(b.value()); }
constexpr bool operator>(const int a, const File b) { return a > static_cast<int>(b.value()); }
constexpr bool operator>=(const int a, const File b) { return a >= static_cast<int>(b.value()); }

#endif// FRANKYCPP_FILE_H
