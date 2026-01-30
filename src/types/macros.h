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

#ifndef FRANKYCPP_MACROS_H
#define FRANKYCPP_MACROS_H

//=============================================================================
// macros.h - Convenience Macros and Operator Generators
//=============================================================================
//
// This header provides utility macros for the FrankyCPP codebase.
// Depends on: globals.h
//
// Categories:
//
// 1. OUTPUT MACROS
//    println(s)          - Print string with newline
//    fprint(...)         - Formatted print (European locale)
//    fprintln(...)       - Formatted print with newline
//    DEBUG(...)          - Debug output with file:line prefix
//
// 2. OPERATOR GENERATORS
//    These macros generate arithmetic/comparison operators for enum-like types.
//    A common C++ pattern for strongly-typed enums that need arithmetic.
//
//    ENABLE_BASE_OPERATORS_ON(T)       - +, -, unary-, +=, -=
//    ENABLE_INCR_OPERATORS_ON(T)       - ++, --
//    ENABLE_FULL_OPERATORS_ON(T)       - All above + *, /, mixed int ops
//    ENABLE_COMPARISON_OPERATORS_ON(T) - ==, !=, <, <=, >, >=
//    ENABLE_MIXED_COMPARISONS_ON(T)    - T vs int comparisons
//
// 3. FORMATTER GENERATORS
//    ENABLE_FORMATTER_AS_STRING_VIEW_ON(T) - std::format via T::str()
//    ENABLE_FORMATTER_AS_CHAR_ON(T)        - std::format for char types
//    ENABLE_FORMATTER_AS_INT_ON(T)         - std::format as integer
//
// 4. OSTREAM GENERATORS
//    ENABLE_OSTREAM_OPERATOR_AS_INT_ON(T)  - operator<< as int
//    ENABLE_OSTREAM_OPERATOR_AS_STR_ON(T)  - operator<< via T::str()
//
// Usage:
//    enum class MyType : int { ... };
//    ENABLE_FULL_OPERATORS_ON(MyType)
//    ENABLE_FORMATTER_AS_INT_ON(MyType)
//
//=============================================================================

#include "globals.h"

#define sleepForSec(x) std::this_thread::sleep_for(std::chrono::seconds(x));
#define NEWLINE std::cout << std::endl
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define println(s) std::cout << (s) << std::endl
#define fprint(...) std::cout << std::format(deLocale, __VA_ARGS__)
#define fprintln(...) fprint(__VA_ARGS__) << std::endl
#define DEBUG(...) std::cout << std::format(deLocale, "DEBUG {}:{} {}", __FILE__, __LINE__, __VA_ARGS__) << std::endl
#define TICK(tp) fprintln("{:L} ns: function: {}() line: {}", elapsedSince(tp).count(), __FUNCTION__, __LINE__)

// Convenience macros to define custom operators on strongly-typed enum types.
// A common C++ pattern for enums that need arithmetic operators.
// See: https://www.chessprogramming.org/Score#702
#define ENABLE_BASE_OPERATORS_ON(T)                                                                         \
  constexpr T operator+(T d1, T d2) { return static_cast<T>(static_cast<int>(d1) + static_cast<int>(d2)); } \
  constexpr T operator-(T d1, T d2) { return static_cast<T>(static_cast<int>(d1) - static_cast<int>(d2)); } \
  constexpr T operator-(T d) { return static_cast<T>(-static_cast<int>(d)); }                               \
  constexpr T& operator+=(T& d1, T d2) { return d1 = d1 + d2; }                                             \
  constexpr T& operator-=(T& d1, T d2) { return d1 = d1 - d2; }

#define ENABLE_BASE2_OPERATORS_ON(T1, T2)                                                                       \
  constexpr T1 operator+(T1 d1, T2 d2) { return static_cast<T1>(static_cast<int>(d1) + static_cast<int>(d2)); } \
  constexpr T1 operator-(T1 d1, T2 d2) { return static_cast<T1>(static_cast<int>(d1) - static_cast<int>(d2)); } \
  constexpr T1 operator-(T1 d) { return static_cast<T1>(-static_cast<int>(d)); }                                \
  constexpr T1& operator+=(T1& d1, T2 d2) { return d1 = d1 + d2; }                                              \
  constexpr T1& operator-=(T1& d1, T2 d2) { return d1 = d1 - d2; }

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

// New: enable comparison operators for T vs T using implicit conversion to int
#define ENABLE_COMPARISON_OPERATORS_ON(T)                                                      \
  constexpr bool operator==(T a, T b) { return static_cast<int>(a) == static_cast<int>(b); }   \
  constexpr bool operator!=(T a, T b) { return static_cast<int>(a) != static_cast<int>(b); }   \
  constexpr bool operator<(T a, T b) { return static_cast<int>(a) < static_cast<int>(b); }     \
  constexpr bool operator<=(T a, T b) { return static_cast<int>(a) <= static_cast<int>(b); }   \
  constexpr bool operator>(T a, T b) { return static_cast<int>(a) > static_cast<int>(b); }     \
  constexpr bool operator>=(T a, T b) { return static_cast<int>(a) >= static_cast<int>(b); }

// New: enable mixed comparisons between T and int in both directions
#define ENABLE_MIXED_COMPARISONS_ON(T)                                         \
  constexpr bool operator==(T a, int b) { return static_cast<int>(a) == b; }   \
  constexpr bool operator!=(T a, int b) { return static_cast<int>(a) != b; }   \
  constexpr bool operator<(T a, int b) { return static_cast<int>(a) < b; }     \
  constexpr bool operator<=(T a, int b) { return static_cast<int>(a) <= b; }   \
  constexpr bool operator>(T a, int b) { return static_cast<int>(a) > b; }     \
  constexpr bool operator>=(T a, int b) { return static_cast<int>(a) >= b; }   \
  constexpr bool operator==(int a, T b) { return a == static_cast<int>(b); }   \
  constexpr bool operator!=(int a, T b) { return a != static_cast<int>(b); }   \
  constexpr bool operator<(int a, T b) { return a < static_cast<int>(b); }     \
  constexpr bool operator<=(int a, T b) { return a <= static_cast<int>(b); }   \
  constexpr bool operator>(int a, T b) { return a > static_cast<int>(b); }     \
  constexpr bool operator>=(int a, T b) { return a >= static_cast<int>(b); }

// New: enable compound assignment with int (+= and -=)
#define ENABLE_INT_COMPOUND_ADDSUB_ON(T)                                                       \
  constexpr T& operator+=(T& d, int i) { return d = static_cast<T>(static_cast<int>(d) + i); } \
  constexpr T& operator-=(T& d, int i) { return d = static_cast<T>(static_cast<int>(d) - i); }

// New: std::formatter<T> specializations via macros
// Use when T has str() returning std::string or std::string_view-compatible value
#define ENABLE_FORMATTER_AS_STRING_VIEW_ON(T)                        \
  template<>                                                         \
  struct std::formatter<T> : std::formatter<std::string_view> {      \
    template<typename FormatContext>                                 \
    auto format(const T v, FormatContext& ctx) const {               \
      return std::formatter<std::string_view>::format(v.str(), ctx); \
    }                                                                \
  }

// Use when T has str() returning a single char
#define ENABLE_FORMATTER_AS_CHAR_ON(T)                   \
  template<>                                             \
  struct std::formatter<T> : std::formatter<char> {      \
    template<typename FormatContext>                     \
    auto format(const T v, FormatContext& ctx) const {   \
      return std::formatter<char>::format(v.str(), ctx); \
    }                                                    \
  }

// Use when T can be formatted as int (via implicit or explicit conversion)
#define ENABLE_FORMATTER_AS_INT_ON(T)                               \
  template<>                                                        \
  struct std::formatter<T> : std::formatter<int> {                  \
    template<typename FormatContext>                                \
    auto format(const T v, FormatContext& ctx) const {              \
      return std::formatter<int>::format(static_cast<int>(v), ctx); \
    }                                                               \
  }

// ostream operator<< via macro using static_cast<int64_t>
#define ENABLE_OSTREAM_OPERATOR_AS_INT_ON(T)                     \
  inline std::ostream& operator<<(std::ostream& os, const T v) { \
    os << static_cast<int64_t>(v);                               \
    return os;                                                   \
  }

// ostream operator<< via macro using static_cast<int64_t>
#define ENABLE_OSTREAM_OPERATOR_AS_STR_ON(T)                     \
  inline std::ostream& operator<<(std::ostream& os, const T v) { \
    os << v.str();                                               \
    return os;                                                   \
  }

#endif// FRANKYCPP_MACROS_H
