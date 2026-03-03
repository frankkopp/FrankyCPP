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

#ifndef FRANKYCPP_DEPTH_H
#define FRANKYCPP_DEPTH_H

//=============================================================================
// depth.h - Search Depth Type
//=============================================================================
//
// Depth represents the remaining search depth in plies (half-moves).
// Depends on: macros.h
//
// Values:
//   DEPTH_NONE / DEPTH_ZERO = 0  - Leaf node (quiescence)
//   DEPTH_ONE = 1                - Frontier node
//   DEPTH_TWO = 2                - Pre-frontier node
//   DEPTH_MAX = 127              - Maximum search depth
//
// Aliases (search terminology):
//   DEPTH_FRONTIER        = 1    - One ply from horizon
//   DEPTH_PRE_FRONTIER    = 2    - Two plies from horizon
//   DEPTH_PREPRE_FRONTIER = 3    - Three plies from horizon
//
// Usage:
//   Depth d = DEPTH_FOUR;
//   d -= DEPTH_ONE;         // Now DEPTH_THREE
//   if (d <= DEPTH_ZERO) { /* quiescence search */ }
//
//=============================================================================

#include "macros.h"
#include <format>

namespace chess {

  enum Depth : int {
    DEPTH_NONE  = 0,
    DEPTH_ONE   = 1,
    DEPTH_TWO   = 2,
    DEPTH_THREE = 3,
    DEPTH_FOUR  = 4,

    DEPTH_ZERO            = DEPTH_NONE,
    DEPTH_FRONTIER        = DEPTH_ONE,
    DEPTH_PRE_FRONTIER    = DEPTH_TWO,
    DEPTH_PREPRE_FRONTIER = DEPTH_THREE,

    DEPTH_MAX = 127
  };

  ENABLE_FULL_OPERATORS_ON(Depth)
  ENABLE_OSTREAM_OPERATOR_AS_INT_ON(Depth);

}// namespace chess

ENABLE_FORMATTER_AS_INT_ON(chess::Depth);

#endif// FRANKYCPP_DEPTH_H
