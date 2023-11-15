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

#ifndef FRANKYCPP_FILE_H
#define FRANKYCPP_FILE_H

#include "macros.h"
#include <ostream>

// File represents a chess board file a-h
//  FILE_A      // 0
//	FILE_B      // 1
//	FILE_C      // 2
//	FILE_D      // 3
//	FILE_E      // 4
//	FILE_F      // 5
//	FILE_G      // 6
//	FILE_H      // 7
//	FILE_NONE   // 8
//  FILE_LENGTH // 8
enum File : uint_fast8_t {
  FILE_A,
  FILE_B,
  FILE_C,
  FILE_D,
  FILE_E,
  FILE_F,
  FILE_G,
  FILE_H,
  FILE_NONE,
  FILE_LENGTH = 9
};

// checks if file is a value of 0-7
constexpr bool validFile(const File f) {
  return f < 8;
}

// creates a file from a char
constexpr File makeFile(const char fileLabel) {
  const File f = static_cast<File>(fileLabel - 'a');
  return validFile(f) ? f : FILE_NONE;
}

// returns the distance between two files in king moves
inline int distance(const File f1, const File f2) {
  return std::abs(static_cast<int>(f2) - static_cast<int>(f1));
}

// returns a char representing the square (e.g. a1 or h8)
constexpr char str(const File f) {
  if (f >= 8) return '-';
  return static_cast<char>('a' + static_cast<char>(f));
}

inline std::ostream& operator<<(std::ostream& os, const File f) {
  os << str(f);
  return os;
}

ENABLE_INCR_OPERATORS_ON(File)

#endif//FRANKYCPP_FILE_H
