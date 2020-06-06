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

#ifndef FRANKYCPP_GLOBALS_H
#define FRANKYCPP_GLOBALS_H

#include "fmt/locale.h"
#include <cstdint>
#include <iostream>
#include <chrono>

// Here we define some global constants  to be used throughout the code.
// As types are imported likely everywhere this will be included in types.h

// standard chess starting position
constexpr const char* START_POSITION_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// game phase is a gelper value to determine in which phase a game is.
// A value of 24 indicates that all officers are still on the board.
// A value of 0 indicates that no officers are on the boardany longer.
constexpr const int GAME_PHASE_MAX = 24;

// max depth
constexpr const int MAX_DEPTH = 128;

// max number of moves
constexpr const int MAX_MOVES = 512;

// nanoseconds per second
constexpr const uint64_t nanoPerSec = 1'000'000'000;
// kilobyte
constexpr const uint64_t KB = 1024;
// megabyte
constexpr const uint64_t MB = KB * KB;
// gigabyte
constexpr const uint64_t GB = KB * MB;

// defines a locale for European style numbers
struct deLocaleDecimals : std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\03"; }
};

inline const std::locale deLocale(std::cout.getloc(), new deLocaleDecimals);

#endif//FRANKYCPP_GLOBALS_H
