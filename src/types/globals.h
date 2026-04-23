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

#ifndef FRANKYCPP_GLOBALS_H
#define FRANKYCPP_GLOBALS_H

//=============================================================================
// globals.h - Global Constants and Utilities
//=============================================================================
//
// This header defines fundamental constants and utilities used throughout
// FrankyCPP. It has no internal dependencies and is included transitively
// via types.h.
//
// Constants:
//   START_POSITION_FEN  - Standard chess starting position in FEN notation
//   GAME_PHASE_MAX      - Maximum game phase value (24 = all pieces on board)
//   MAX_DEPTH           - Maximum search depth (128 plies)
//   MAX_MOVES           - Maximum game history length in half-moves (1024)
//   KB, MB, GB          - Size constants for memory calculations
//
// Utilities:
//   locale            - European-style number formatting (1.000,00)
//
//=============================================================================

#include <chrono>
#include <cstdint>
#include <iostream>
#include <locale>

// standard chess starting position
constexpr auto START_POSITION_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// game phase is a helper value to determine in which phase a game is.
// A value of 24 indicates that all officers are still on the board.
// A value of 0 indicates that no officers are on the board any longer.
constexpr int GAME_PHASE_MAX = 24;

// max depth
constexpr int MAX_DEPTH = 128;

// max search threads (matches UCI Threads option maxValue)
constexpr int MAX_SEARCH_THREADS = 256;

// Maximum game history length in half-moves (plies).
// Must accommodate game plies + search depth (doMove/doNullMove during search
// push to the same history array). 1024 supports games up to ~896 plies
// with search depth up to 128.
constexpr int MAX_MOVES = 1024;

// nanoseconds per second
constexpr uint64_t nanoPerSec = 1'000'000'000;
// kilobyte
constexpr uint64_t KB = 1024;
// megabyte
constexpr uint64_t MB = KB * KB;
// gigabyte
constexpr uint64_t GB = KB * MB;

// defines a locale for European style numbers
struct deLocaleDecimals final : std::numpunct<char> {
protected:
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\03"; }
};

inline const std::locale projectLocale(std::cout.getloc(), new deLocaleDecimals);

#endif // FRANKYCPP_GLOBALS_H
