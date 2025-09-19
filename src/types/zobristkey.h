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

#ifndef FRANKYCPP_ZOBRISTKEY_H
#define FRANKYCPP_ZOBRISTKEY_H

#include <cassert>
#include <cstdint>

/** 64 bit Key for zobrist etc. */
typedef uint64_t Key;

/// from Stockfish:
/// xorshift64star Pseudo-Random Number Generator
/// This class is based on original code written and dedicated
/// to the public domain by Sebastiano Vigna (2014).
/// It has the following characteristics:
///
///  -  Outputs 64-bit numbers
///  -  Passes Dieharder and SmallCrush test batteries
///  -  Does not require warm-up, no zeroland to escape
///  -  Internal state is a single 64-bit integer
///  -  Period is 2^64 - 1
///  -  Speed: 1.60 ns/call (Core i7 @3.40GHz)
///
/// For further analysis see
///   <http://vigna.di.unimi.it/ftp/papers/xorshift.pdf>
class PRNG {
  uint64_t s;

  uint64_t rand64() {
    s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
    return s * 2685821657736338717LL;
  }

public:
  explicit PRNG(const uint64_t seed) : s(seed) { assert(seed); }

  template<typename T>
  T rand() { return T(rand64()); }

  // Special generator used to fast init magic numbers.
  // Output values only have 1/8th of their bits set on average.
  template<typename T>
  T sparse_rand() { return T(rand64() & rand64() & rand64()); }
};

#endif//FRANKYCPP_ZOBRISTKEY_H
