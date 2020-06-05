/*
 * MIT License
 *
 * Copyright (c) 2020 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef FRANKYCPP_FILE_H
#define FRANKYCPP_FILE_H

#include <ostream>

enum File : int {
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
constexpr bool validFile(File f) {
  return !(f < 0 || f >= 8);
}

// creates a file from a char
constexpr File makeFile(char fileLabel) {
  const File f = static_cast<File>(fileLabel - 'a');
  return validFile(f) ? f : FILE_NONE;
}

// returns a string representing the square (e.g. a1 or h8)
constexpr char fileLabel(File f) {
  if (f < 0 || f >= 8) return '-';
  return 'a' + f;
}

inline std::ostream& operator<<(std::ostream& os, const File f) {
  os << fileLabel(f);
  return os;
}

#endif//FRANKYCPP_FILE_H
