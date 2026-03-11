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

#ifndef FRANKYCPP_TYPES_INIT_H
#define FRANKYCPP_TYPES_INIT_H

//=============================================================================
// init.h (types) - Type System Initialization
//=============================================================================
//
// Provides initialization for runtime-computed type tables.
// Depends on: attacks.h
//
// Currently initializes:
//   - Attacks:: magic bitboard tables (sliding piece attacks)
//
// Usage:
//   Types::init();  // Call once at program startup
//
// Note: Most type tables are constexpr and require no initialization.
//       Only the Attacks tables need runtime initialization due to size.
//
//=============================================================================

#include "attacks.h"

namespace chess {
  namespace Types {
    inline void init() {
      Attacks::init();
    }
  } // namespace Types
} // namespace chess

#endif // FRANKYCPP_TYPES_INIT_H
