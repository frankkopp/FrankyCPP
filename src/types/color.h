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

#ifndef FRANKYCPP_COLOR_H
#define FRANKYCPP_COLOR_H

#include <ostream>
#include "macros.h"

// Color represents constants for each chess color White and Black
//  WHITE        = 0,
//  BLACK        = 1,
//  NOCOLOR      = 2,
//  COLOR_LENGTH = 2
enum Color : uint_fast8_t {
  WHITE        = 0,
  BLACK        = 1,
  NOCOLOR      = 2,
  COLOR_LENGTH = 2
};

// checks if rank is a value of 0-7
constexpr bool validColor(const Color c) {
  return c < 2;
}

// returns the opposite color
constexpr Color operator~(const Color c) { return static_cast<Color>(c ^ 1); }

// moveDirection returns positive 1 for White and negative 1 (-1) for Black
constexpr int moveDirection(Color c) { return c == WHITE ? 1 : -1; }

// returns a char representing the color (e.g. w or b)
constexpr char str(Color c) {
  if (!validColor(c)) return '-';
  return c == WHITE ? 'w' : 'b';
}

inline std::ostream& operator<<(std::ostream& os, const Color c) {
  os << str(c);
  return os;
}

ENABLE_INCR_OPERATORS_ON (Color)

#endif//FRANKYCPP_COLOR_H
