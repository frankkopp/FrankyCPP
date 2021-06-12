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

#ifndef FRANKYCPP_MISC_H
#define FRANKYCPP_MISC_H

#include <string>
#include <fmt/printf.h>

// transforms the given string to lower case
inline std::string toLowerCase(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return str;
}

// transforms the given string to upper case
inline std::string toUpperCase(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(), [](
    unsigned char c) { return char(std::toupper(c)); });
  return str;
}

constexpr const char* boolStr (bool b) {
  return b ? "true" : "false";
}

constexpr const char* boolStr (int b) {
  return boolStr (bool (b));
}

inline std::string printProgress(double percentage) {
  constexpr const int pbarw = 60;
  constexpr const char* pbar = "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||";
  int val = static_cast<int>(percentage * 100);
  int lpad = static_cast<int>(percentage * pbarw);
  int rpad = pbarw - lpad;
  return fmt::sprintf("%3d%% [%.*s%*s]", val, lpad, pbar, rpad, "");
}

#endif//FRANKYCPP_MISC_H
