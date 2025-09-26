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

#ifndef FRANKYCPP_SQUARE_H
#define FRANKYCPP_SQUARE_H

#include "color.h"
#include "file.h"
#include "macros.h"
#include "rank.h"

#include <array>
#include <cstdint>
#include <format>
#include <iterator>
#include <string>
#include <string_view>

// Square represents exactly one square on a chess board backed by an unsigned value.
//  A1 = 0 ... H8 = 63, NONE = 64
class Square {
  std::uint8_t v_{};// 0..63 valid squares, 64 = NONE (fits in one byte)

public:
  // constructors
  constexpr Square() : v_(64) {}
  constexpr explicit Square(const unsigned v) : v_(static_cast<std::uint8_t>(v)) {}
  constexpr explicit Square(const int v) : v_(static_cast<std::uint8_t>(v)) {}

  // factory
  static constexpr Square of(const File f, const Rank r) { return Square{(static_cast<int>(r) << 3) + static_cast<int>(f)}; }

  // underlying value access
  constexpr auto value() const { return v_; }

  // implicit conversion for arithmetic/comparisons/array indexing
  // ReSharper disable once CppNonExplicitConversionOperator
  constexpr operator int() const { return v_; }

  /// Returns true if the square is valid (A1-H8).
  constexpr bool isValid() const {
    const int x = *this;
    return x >= 0 && x < 64;
  }

  /// Returns the file (column) of the square.
  constexpr File file() const { return File{(v_ & 0b00000111u)}; }

  /// Returns the rank (row) of the square.
  constexpr Rank rank() const { return Rank{static_cast<unsigned>(v_ >> 3U)}; }

  // parsing from UCI-like string (e.g., "e2")
  static Square fromString(std::string_view s);

  // distance to another square (Chebyshev distance)
  constexpr int distanceTo(Square other) const;

  // pawn push for given color
  constexpr Square pawnPush(Color c) const;

  // string representation (e.g., "a1")
  std::string str() const;

  // -----------------------------
  // Zero-overhead iteration support
  // -----------------------------
  struct iterator {
    using value_type        = Square;
    using difference_type   = int;
    using iterator_category = std::random_access_iterator_tag;// simple, contiguous numeric progression
    using pointer           = void;
    using reference         = Square;

    int i{};// current index (0..64)

    // clang-format off
    constexpr value_type operator*() const { return Square{i}; }
    constexpr iterator& operator++() { ++i; return *this; }
    constexpr iterator operator++(int) { const iterator tmp{*this}; ++(*this); return tmp; }
    constexpr iterator& operator--() { --i; return *this; }
    constexpr iterator operator--(int) { const iterator tmp{*this}; --(*this); return tmp; }
    constexpr iterator& operator+=(const difference_type n) { i += n; return *this; }
    constexpr iterator& operator-=(const difference_type n) { i -= n; return *this; }
    friend constexpr iterator operator+(iterator it, const difference_type n) { it += n; return it; }
    friend constexpr iterator operator+(const difference_type n, iterator it) { it += n; return it; }
    friend constexpr iterator operator-(iterator it, const difference_type n) { it -= n; return it; }
    friend constexpr difference_type operator-(const iterator a, const iterator b) { return a.i - b.i; }

    // equality/ordering comparisons
    constexpr bool operator==(const iterator& other) const { return i == other.i; }
    constexpr bool operator!=(const iterator& other) const { return i != other.i; }
    constexpr bool operator<(const iterator& other) const { return i < other.i; }
    constexpr bool operator>(const iterator& other) const { return i > other.i; }
    constexpr bool operator<=(const iterator& other) const { return i <= other.i; }
    constexpr bool operator>=(const iterator& other) const { return i >= other.i; }
    // clang-format on
  };

  struct range {
    int b{};// begin (inclusive)
    int e{};// end (exclusive)
    constexpr iterator begin() const { return iterator{b}; }
    constexpr iterator end() const { return iterator{e}; }
    constexpr int size() const { return e - b; }
    constexpr bool empty() const { return e <= b; }
  };

  // Factory helpers for ranges
  static constexpr range all() { return range{0, 64}; }
  static constexpr range valid() { return all(); }
  static constexpr range between(const Square firstInclusive, const Square lastInclusive) {
    return range{static_cast<int>(firstInclusive), static_cast<int>(lastInclusive) + 1};
  }
  constexpr range to(const Square lastInclusive) const {
    return range{static_cast<int>(value()), static_cast<int>(lastInclusive) + 1};
  }
};

// Square constants (global, to keep existing code unchanged)
// clang-format off
inline constexpr Square SQ_A1{0},  SQ_B1{1},  SQ_C1{2},  SQ_D1{3},  SQ_E1{4},  SQ_F1{5},  SQ_G1{6},  SQ_H1{7},
                        SQ_A2{8},  SQ_B2{9},  SQ_C2{10}, SQ_D2{11}, SQ_E2{12}, SQ_F2{13}, SQ_G2{14}, SQ_H2{15},
                        SQ_A3{16}, SQ_B3{17}, SQ_C3{18}, SQ_D3{19}, SQ_E3{20}, SQ_F3{21}, SQ_G3{22}, SQ_H3{23},
                        SQ_A4{24}, SQ_B4{25}, SQ_C4{26}, SQ_D4{27}, SQ_E4{28}, SQ_F4{29}, SQ_G4{30}, SQ_H4{31},
                        SQ_A5{32}, SQ_B5{33}, SQ_C5{34}, SQ_D5{35}, SQ_E5{36}, SQ_F5{37}, SQ_G5{38}, SQ_H5{39},
                        SQ_A6{40}, SQ_B6{41}, SQ_C6{42}, SQ_D6{43}, SQ_E6{44}, SQ_F6{45}, SQ_G6{46}, SQ_H6{47},
                        SQ_A7{48}, SQ_B7{49}, SQ_C7{50}, SQ_D7{51}, SQ_E7{52}, SQ_F7{53}, SQ_G7{54}, SQ_H7{55},
                        SQ_A8{56}, SQ_B8{57}, SQ_C8{58}, SQ_D8{59}, SQ_E8{60}, SQ_F8{61}, SQ_G8{62}, SQ_H8{63};
inline constexpr Square SQ_NONE{64};
inline constexpr unsigned SQ_LENGTH = 64;
// clang-format on

ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_COMPARISON_OPERATORS_ON(Square)
ENABLE_MIXED_COMPARISONS_ON(Square)
ENABLE_FORMATTER_AS_STRING_VIEW_ON(Square);
ENABLE_OSTREAM_OPERATOR_AS_STR_ON (Square)

// Precomputed square distances and names for fast access
namespace Squares {
  constexpr std::array<std::array<int, SQ_LENGTH>, SQ_LENGTH> squareDistancePreCompute() {
    std::array<std::array<int, SQ_LENGTH>, SQ_LENGTH> dist{};// zero-initialize (diagonal stays 0)
    // distance between squares (Chebyshev distance)
    for (Square sq1 = SQ_A1; sq1 <= SQ_H8; ++sq1) {
      for (Square sq2 = SQ_A1; sq2 <= SQ_H8; ++sq2) {
        if (sq1 != sq2) {
          const int f1   = sq1.file();
          const int f2   = sq2.file();
          const int r1   = sq1.rank();
          const int r2   = sq2.rank();
          const int df   = f1 > f2 ? f1 - f2 : f2 - f1;
          const int dr   = r1 > r2 ? r1 - r2 : r2 - r1;
          dist[sq1][sq2] = df > dr ? df : dr;
        }
      }
    }
    return dist;
  }
  // precomputed distances between all squares
  inline constexpr std::array<std::array<int, SQ_LENGTH>, SQ_LENGTH> squareDistance = squareDistancePreCompute();

  constexpr std::array<int, SQ_LENGTH> centerDistancePreCompute() {
    std::array<int, SQ_LENGTH> cd{};
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {
      if (static_cast<int>(sq.file()) <= static_cast<int>(FILE_D) && static_cast<int>(sq.rank()) >= static_cast<int>(RANK_5)) {
        cd[sq] = squareDistance[sq][SQ_D5];
      }
      else if (static_cast<int>(sq.file()) >= static_cast<int>(FILE_E) && static_cast<int>(sq.rank()) >= static_cast<int>(RANK_5)) {
        cd[sq] = squareDistance[sq][SQ_E5];
      }
      else if (static_cast<int>(sq.file()) <= static_cast<int>(FILE_D) && static_cast<int>(sq.rank()) <= static_cast<int>(RANK_4)) {
        cd[sq] = squareDistance[sq][SQ_D4];
      }
      else if (static_cast<int>(sq.file()) >= static_cast<int>(FILE_E) && static_cast<int>(sq.rank()) <= static_cast<int>(RANK_4)) {
        cd[sq] = squareDistance[sq][SQ_E4];
      }
    }
    return cd;
  }
  // precomputed distances from center squares (d4, d5, e4, e5)
  inline constexpr std::array<int, SQ_LENGTH> centerDistance = centerDistancePreCompute();

  // precomputed square names as char arrays for fast access
  inline constexpr std::array<std::array<char, 3>, SQ_LENGTH> squareNames = []() {
    std::array<std::array<char, 3>, SQ_LENGTH> names{};
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {
      names[sq] = {sq.file().str(), sq.rank().str(), '\0'};
    }
    return names;
  }();
}// namespace Squares

// Out-of-class inline definitions depending on the pre-computations above

inline Square Square::fromString(const std::string_view s) {
  if (s.length() < 2) return SQ_NONE;
  const File f = File::fromChar(s[0]);
  const Rank r = Rank::fromChar(s[1]);
  if (f.isValid() && r.isValid()) return Square::of(f, r);
  return SQ_NONE;
}

constexpr int Square::distanceTo(const Square other) const {
  return Squares::squareDistance[*this][other];
}

constexpr Square Square::pawnPush(const Color c) const {
  return Square{static_cast<int>(*this) + 8 * c.sign()};
}

inline std::string Square::str() const {
  return Squares::squareNames[*this].data();
}

// Compile-time sanity checks to lock in representation guarantees
static_assert(sizeof(Square) == 1, "Square should be 1 byte");
static_assert(alignof(Square) == alignof(std::uint8_t), "Square alignment should match uint8_t");
static_assert(static_cast<int>(SQ_A1) == 0 && static_cast<int>(SQ_H8) == 63, "Square constants must map 0..63");
static_assert(static_cast<int>(SQ_NONE) == 64, "SQ_NONE must be 64");
// Iterator/range sanity
static_assert(std::is_trivially_copyable_v<Square::iterator>, "Iterator should be trivially copyable");
static_assert(std::is_trivially_copyable_v<Square::range>, "Range should be trivially copyable");

#endif// FRANKYCPP_SQUARE_H
