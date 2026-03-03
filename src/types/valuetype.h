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

#ifndef FRANKYCPP_VALUETYPE_H
#define FRANKYCPP_VALUETYPE_H

//=============================================================================
// valuetype.h - Transposition Table Entry Value Type
//=============================================================================
//
// ValueType indicates the bound type for a value stored in the transposition
// table. Used to determine how the cached value can be used during search.
// No internal dependencies.
//
// Values:
//   NONE  = 0  - No valid value (uninitialized entry)
//   EXACT = 1  - Exact score (PV node, fully searched)
//   ALPHA = 2  - Upper bound (fail-low, no move beat alpha)
//   BETA  = 3  - Lower bound (fail-high, beta cutoff)
//
// Search Semantics:
//   EXACT: Value is the true minimax score; can be used directly
//   ALPHA: True value <= stored value; useful if stored <= alpha
//   BETA:  True value >= stored value; useful if stored >= beta
//
// Usage:
//   TTEntry entry = tt->probe(key);
//   if (entry.type == EXACT) return entry.value;
//   if (entry.type == BETA && entry.value >= beta) return entry.value;
//   if (entry.type == ALPHA && entry.value <= alpha) return entry.value;
//
//=============================================================================

#include <cstdint>

namespace chess {
  enum ValueType : uint_fast8_t {
    NONE  = 0,
    EXACT = 1,
    ALPHA = 2,
    BETA  = 3,
  };
}// namespace chess

#endif// FRANKYCPP_VALUETYPE_H
