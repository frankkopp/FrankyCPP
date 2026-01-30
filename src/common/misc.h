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

#ifndef FRANKYCPP_MISC_H
#define FRANKYCPP_MISC_H

//=============================================================================
// misc.h - Miscellaneous Utility Functions
//=============================================================================
//
// Contains various utility functions that don't fit into other categories.
//
// Functions:
//   printProgress() - Generate ASCII progress bar string
//
//=============================================================================

#include <string>
#include <format>

/// Generates an ASCII progress bar string.
/// @param percentage  Progress value from 0.0 to 1.0
/// @return            Formatted string like "50% [||||||||||          ]"
inline std::string printProgress(double percentage) {
  constexpr int pbarw  = 60;
  constexpr auto pbar = "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||";

  const int val  = static_cast<int>(percentage * 100);
  const int lpad = static_cast<int>(percentage * pbarw);
  const int rpad = pbarw - lpad;

  return std::format("%3d%% [%.*s%*s]", val, lpad, pbar, rpad, "");
}

#endif// FRANKYCPP_MISC_H
