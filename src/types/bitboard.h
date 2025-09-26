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

#ifndef FRANKYCPP_BITBOARD_H
#define FRANKYCPP_BITBOARD_H

#include "direction.h"
#include "piecetype.h"
#include "square.h"

#include <bit>
#include <bitset>
#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>

// //////////////////////////////////////////////////////////////////
// Bitboard value-type
// //////////////////////////////////////////////////////////////////

// Small, trivially copyable wrapper around an unsigned 64-bit integer.
// Designed to be zero-cost: all operators are constexpr/inlined to keep
// existing performance characteristics of raw uint64_t bitboards.
class Bitboard {
  std::uint64_t v_{}; // underlying 64-bit mask

public:
  // constructors
  constexpr Bitboard() = default;

  // implicit to allow constexpr literal initializers like: constexpr Bitboard BbZero = 0;
  constexpr Bitboard(const std::uint64_t v) : v_{v} {}

  // access
  constexpr std::uint64_t value() const { return v_; }

  // implicit conversion to underlying for interop with intrinsics/APIs
  // ReSharper disable once CppNonExplicitConversionOperator
  constexpr operator std::uint64_t() const { return v_; }

  // truthiness (used in many fast-paths)
  constexpr explicit operator bool() const { return v_ != 0ULL; }

  // clang-format off
  // compounds arithmetic and shifts
  constexpr Bitboard operator~() const { return Bitboard{~v_}; }
  constexpr Bitboard& operator&=(const Bitboard b) { v_ &= b.v_; return *this; }
  constexpr Bitboard& operator|=(const Bitboard b) { v_ |= b.v_; return *this; }
  constexpr Bitboard& operator^=(const Bitboard b) { v_ ^= b.v_; return *this; }
  constexpr Bitboard& operator+=(const Bitboard b) { v_ += b.v_; return *this; }
  constexpr Bitboard& operator-=(const Bitboard b) { v_ -= b.v_; return *this; }
  constexpr Bitboard& operator<<=(const int sh) { v_ <<= sh; return *this; }
  constexpr Bitboard& operator>>=(const int sh) { v_ >>= sh; return *this; }
  // arithmetic and shifts (friends to allow symmetric conversions)
  friend constexpr Bitboard operator&(const Bitboard a, const Bitboard b) { return Bitboard{a.v_ & b.v_}; }
  friend constexpr Bitboard operator|(const Bitboard a, const Bitboard b) { return Bitboard{a.v_ | b.v_}; }
  friend constexpr Bitboard operator^(const Bitboard a, const Bitboard b) { return Bitboard{a.v_ ^ b.v_}; }
  friend constexpr Bitboard operator+(const Bitboard a, const Bitboard b) { return Bitboard{a.v_ + b.v_}; }
  friend constexpr Bitboard operator-(const Bitboard a, const Bitboard b) { return Bitboard{a.v_ - b.v_}; }
  friend constexpr Bitboard operator<<(const Bitboard a, const int sh) { return Bitboard{a.v_ << sh}; }
  friend constexpr Bitboard operator>>(const Bitboard a, const int sh) { return Bitboard{a.v_ >> sh}; }
  // // clang-format on

  // population count
  constexpr int popcount() const { return std::popcount(v_); }

  // index of least/most significant bit (Square), or SQ_NONE if empty
  constexpr Square lsb() const {
    if (v_ == 0ULL) return SQ_NONE;
    return static_cast<Square>(std::countr_zero(v_));
  }

  // index of most significant bit (Square), or SQ_NONE if empty
  constexpr Square msb() const {
    if (v_ == 0ULL) return SQ_NONE;
    return static_cast<Square>(63 - std::countl_zero(v_));
  }

  // clears and returns least significant bit (Square), or SQ_NONE if empty
  constexpr Square popLSB() {
    if (v_ == 0ULL) return SQ_NONE;
    const Square s = lsb();
    v_ &= v_ - 1ULL;
    return s;
  }

  // directional shift (with edge masking, same semantics as old shiftBb)
  constexpr Bitboard shifted(const Direction d) const {
    constexpr std::uint64_t FileABB = 0x0101010101010101ULL;
    constexpr std::uint64_t FileHBB = FileABB << 7;
    switch (static_cast<int>(d)) {
      case static_cast<int>(NORTH): return Bitboard{v_ << 8};
      case static_cast<int>(EAST): return Bitboard{(v_ << 1) & ~FileABB};
      case static_cast<int>(SOUTH): return Bitboard{v_ >> 8};
      case static_cast<int>(WEST): return Bitboard{(v_ >> 1) & ~FileHBB};
      case static_cast<int>(NORTH_EAST): return Bitboard{(v_ << 9) & ~FileABB};
      case static_cast<int>(SOUTH_EAST): return Bitboard{(v_ >> 7) & ~FileABB};
      case static_cast<int>(SOUTH_WEST): return Bitboard{(v_ >> 9) & ~FileHBB};
      case static_cast<int>(NORTH_WEST): return Bitboard{(v_ << 7) & ~FileHBB};
      default:;
    }
    return *this;
  }

  // string helpers
  std::string str() const {
    std::ostringstream os;
    os << std::bitset<64>(v_);
    return os.str();
  }
  std::string strBoard() const {
    std::ostringstream os;
    os << "+---+---+---+---+---+---+---+---+\n";
    for (Rank r = RANK_8;; --r) {
      for (File f = FILE_A; f <= FILE_H; ++f) {
        const int idx = (static_cast<int>(r) << 3) + static_cast<int>(f);
        const std::uint64_t mask = 1ULL << idx;
        os << (v_ & mask ? "| X " : "|   ");
      }
      os << "|\n+---+---+---+---+---+---+---+---+\n";
      if (r == 0) break;
    }
    return os.str();
  }
  std::string strGrouped() const {
    std::ostringstream os;
    for (unsigned i = 0; i < 64; ++i) {
      if (i > 0 && i % 8 == 0) os << ".";
      os << (v_ >> i & 1ULL ? "1" : "0");
    }
    os << " (" << v_ << ")";
    return os.str();
  }
};

// Compile-time sanity
static_assert(sizeof(Bitboard) == sizeof(std::uint64_t), "Bitboard must be 8 bytes");
static_assert(std::is_trivially_copyable_v<Bitboard>, "Bitboard should be trivially copyable");

#endif// FRANKYCPP_BITBOARD_H
