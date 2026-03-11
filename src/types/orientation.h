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

#ifndef FRANKYCPP_ORIENTATION_H
#define FRANKYCPP_ORIENTATION_H

//=============================================================================
// orientation.h - Compass Direction Enumeration
//=============================================================================
//
// Orientation represents the 8 compass directions from a square, used for
// ray attacks and direction-based lookups.
// No internal dependencies.
//
// Values (clockwise from NW):
//   NW = 0    N = 1    NE = 2
//   W  = 7             E  = 3
//   SW = 6    S = 5    SE = 4
//
//   OR_LENGTH = 8  (array sizing sentinel)
//
// Note: Different from Direction which uses step offsets (+8, -1, etc.).
//       Orientation is an index (0-7) for direction-indexed arrays.
//
// Usage:
//   Orientation dir = NE;
//   Bitboard rays = rayAttacks[sq][dir];
//
//=============================================================================

#include <cstdint>

namespace chess {
  enum Orientation : uint_fast8_t {
    NW,       // 0
    N,        // 1
    NE,       // 2
    E,        // 3
    SE,       // 4
    S,        // 5
    SW,       // 6
    W,        // 7
    OR_LENGTH // 8
  };
} // namespace chess

#endif // FRANKYCPP_ORIENTATION_H
