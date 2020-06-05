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

#ifndef FRANKYCPP_TYPES_H
#define FRANKYCPP_TYPES_H

#include "fmt/locale.h"

// convenience macros
#define sleepForSec(x) std::this_thread::sleep_for(std::chrono::seconds(x));
#define NEWLINE std::cout << std::endl
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define println(s) std::cout << (s) << std::endl
#define fprint(...) std::cout << fmt::format(deLocale, __VA_ARGS__)
#define fprintln(...) fprint(__VA_ARGS__) << std::endl
#define DEBUG(...) std::cout << fmt::format(deLocale, "DEBUG {}:{} {}", __FILE__, __LINE__, __VA_ARGS__) << std::endl


///////////////////
// Global constants
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
extern const std::locale deLocale;

// include all type headers for convenience
#include "file.h"
#include "rank.h"
#include "square.h"
#include "color.h"
#include "direction.h"
#include "orientation.h"
#include "castlingrights.h"

#endif//FRANKYCPP_TYPES_H
