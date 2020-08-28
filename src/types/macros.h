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

#ifndef FRANKYCPP_MACROS_H
#define FRANKYCPP_MACROS_H

#include "globals.h"

// Here we define some convenience macros to be used throughout the code.
// As types are imported likely everywhere this will be included in types.h

#define sleepForSec(x) std::this_thread::sleep_for(std::chrono::seconds(x));
#define NEWLINE std::cout << std::endl
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define println(s) std::cout << (s) << std::endl
#define fprint(...) std::cout << fmt::format(deLocale, __VA_ARGS__)
#define fprintln(...) fprint(__VA_ARGS__) << std::endl
#define DEBUG(...) std::cout << fmt::format(deLocale, "DEBUG {}:{} {}", __FILE__, __LINE__, __VA_ARGS__) << std::endl
#define TICK(tp) fprintln("{:L} ns: function: {}() line: {}", elapsedSince(tp).count(), __FUNCTION__, __LINE__)

// These are convenience macros to define custom operators on our types.
// This idea and code is taken from Stockfish
#define ENABLE_BASE_OPERATORS_ON(T)                                                                         \
  constexpr T operator+(T d1, T d2) { return static_cast<T>(static_cast<int>(d1) + static_cast<int>(d2)); } \
  constexpr T operator-(T d1, T d2) { return static_cast<T>(static_cast<int>(d1) - static_cast<int>(d2)); } \
  constexpr T operator-(T d) { return static_cast<T>(-static_cast<int>(d)); }                               \
  constexpr T& operator+=(T& d1, T d2) { return d1 = d1 + d2; }                                             \
  constexpr T& operator-=(T& d1, T d2) { return d1 = d1 - d2; }

#define ENABLE_INCR_OPERATORS_ON(T)                                                     \
  constexpr T& operator++(T& d) { return d = static_cast<T>(static_cast<int>(d) + 1); } \
  constexpr T& operator--(T& d) { return d = static_cast<T>(static_cast<int>(d) - 1); }

#define ENABLE_FULL_OPERATORS_ON(T)                                                            \
  ENABLE_BASE_OPERATORS_ON(T)                                                                  \
  ENABLE_INCR_OPERATORS_ON(T)                                                                  \
  constexpr T operator+(int i, T d) { return static_cast<T>(i + static_cast<int>(d)); }        \
  constexpr T operator+(T d, int i) { return static_cast<T>(static_cast<int>(d) + i); }        \
  constexpr T operator-(int i, T d) { return static_cast<T>(i - static_cast<int>(d)); }        \
  constexpr T operator-(T d, int i) { return static_cast<T>(static_cast<int>(d) - i); }        \
  constexpr T operator*(int i, T d) { return static_cast<T>(i * static_cast<int>(d)); }        \
  constexpr T operator*(T d, int i) { return static_cast<T>(static_cast<int>(d) * i); }        \
  constexpr T operator/(T d, int i) { return static_cast<T>(static_cast<int>(d) / i); }        \
  constexpr int operator/(T d1, T d2) { return static_cast<int>(d1) / static_cast<int>(d2); }  \
  constexpr T& operator*=(T& d, int i) { return d = static_cast<T>(static_cast<int>(d) * i); } \
  constexpr T& operator/=(T& d, int i) { return d = static_cast<T>(static_cast<int>(d) / i); }


#endif//FRANKYCPP_MACROS_H
