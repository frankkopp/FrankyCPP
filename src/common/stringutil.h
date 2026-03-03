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

#ifndef FRANKYCPP_STRINGUTIL_H
#define FRANKYCPP_STRINGUTIL_H

//=============================================================================
// stringutil.h - String Manipulation Utilities
//=============================================================================
//
// Provides efficient string manipulation functions used throughout the engine.
// All functions are designed for performance with minimal allocations.
//
// Functions:
//   splitFast()              - Split string by delimiters into vector
//   trimFast()               - Remove leading/trailing whitespace
//   removeTrailingComments() - Strip comments from end of string
//   toLowerCase()            - Convert to lowercase (copy or in-place)
//   toUpperCase()            - Convert to uppercase (copy or in-place)
//   boolStr()                - Convert bool to "true"/"false" string
//   truncate()               - Truncate string to width with ".." suffix
//
// Template Support:
//   splitFast, trimFast, and truncate work with both std::string and
//   std::string_view, avoiding unnecessary copies when views are sufficient.
//
// Performance Notes:
//   trimFast is significantly faster than regex-based or find_first_not_of
//   alternatives. See commented benchmarks at end of file.
//
// Credits:
//   splitFast inspired by: https://gitlab.com/tbeu/wcx_setfolderdate
//   See also: https://stackoverflow.com/a/236803/8520615
//
//=============================================================================

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "common/Logging.h"

namespace common {

  /// Splits a string or string_view into parts at each delimiter character.
  /// Empty parts between consecutive delimiters are skipped.
  /// @tparam StringType  std::string or std::string_view
  /// @param str          String to split
  /// @param container    Vector to append parts to (not cleared first)
  /// @param delims       Delimiter characters (default: space)
  template<typename StringType>
  void splitFast(const StringType& str, std::vector<StringType>& container, const std::string& delims = " ") {
    for (auto first = str.data(), second = str.data(), end = first + str.size();
         second != end && first != end;
         first = second + 1) {
      second = std::find_first_of(first, end, std::cbegin(delims), std::cend(delims));
      if (first != second) {
        container.emplace_back(first, second - first);
      }
    }
  }

  /// Removes whitespace from beginning and end of string.
  /// Whitespace: space, tab, newline, vertical tab, form feed, carriage return.
  /// @tparam StringType  std::string or std::string_view
  /// @param s            String to trim
  /// @return             Trimmed copy (or view if input is string_view)
  template<typename StringType>
  StringType trimFast(const StringType& s) {
    const int l = static_cast<int>(s.length());
    int a = 0, b = l - 1;
    char c;
    while (a < l && ((c = s[a]) == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r')) ++a;
    while (b > a && ((c = s[b]) == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r')) --b;
    return s.substr(a, 1 + b - a);
  }

  /// Removes trailing portion of string after a comment marker.
  /// @tparam StringType   std::string or std::string_view
  /// @param s             String to process
  /// @param commentMarker Comment start string (e.g., "//" or "#")
  /// @return              String with comment removed, or original if no marker
  template<typename StringType>
  StringType removeTrailingComments(const StringType& s, const std::string& commentMarker) {
    const auto pos = s.find(commentMarker);
    if (pos != StringType::npos) {
      return s.substr(0, pos);
    }
    return s;
  }

  /// Converts string to lowercase (returns copy).
  /// @param s  Input string
  /// @return   Lowercase copy
  inline std::string toLowerCase(const std::string& s) {
    std::string str(s);
    std::ranges::transform(str, str.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return str;
  }

  /// Converts string to lowercase in place.
  /// @param str  String to modify
  inline void toLowerCase(std::string& str) {
    std::ranges::transform(str, str.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
  }

  /// Converts string to uppercase (returns copy).
  /// @param s  Input string
  /// @return   Uppercase copy
  inline std::string toUpperCase(const std::string& s) {
    std::string str(s);
    std::ranges::transform(str, str.begin(), [](const unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return str;
  }

  /// Converts string to uppercase in place.
  /// @param str  String to modify
  inline void toUpperCase(std::string& str) {
    std::ranges::transform(str, str.begin(), [](const unsigned char c) { return static_cast<char>(std::toupper(c)); });
  }

  /// Converts boolean to "true" or "false" string literal.
  /// @param b  Boolean value
  /// @return   "true" or "false"
  constexpr const char* boolStr(const bool b) {
    return b ? "true" : "false";
  }

  /// Converts integer to "true" or "false" string literal.
  /// @param b  Integer value (0 = false, non-zero = true)
  /// @return   "true" or "false"
  constexpr const char* boolStr(const int b) {
    return boolStr(static_cast<bool>(b));
  }

  /// Truncates a string to fit within a specified width.
  /// If the string is longer than width, it is truncated and ".." is appended.
  /// Works with both std::string and std::string_view.
  /// @tparam StringType  std::string or std::string_view
  /// @param str    String to truncate
  /// @param width  Maximum width (must be > 0; if <= 2, returns ".." for long strings)
  /// @return       Truncated string as std::string
  template<typename StringType>
  [[nodiscard]] std::string truncate(const StringType& str, const int width) {
    if (width <= 0) {
      return {};
    }
    if (static_cast<int>(str.length()) <= width) {
      return std::string(str);
    }
    if (width <= 2) {
      return "..";
    }
    return std::string(str.substr(0, width - 2)) + "..";
  }

  //=============================================================================
  // String Parsing - Throwing variants (caller handles exceptions)
  //=============================================================================

  /// Parses string to integer. Throws std::invalid_argument or std::out_of_range on invalid input.
  /// @param v  String to parse
  /// @return   Parsed integer value
  /// @throws   std::invalid_argument if no conversion could be performed
  /// @throws   std::out_of_range if value is out of int range
  [[nodiscard]] inline int parseInt(const std::string& v) {
    return std::stoi(v);
  }

  /// Parses string to double. Throws std::invalid_argument or std::out_of_range on invalid input.
  /// @param v  String to parse
  /// @return   Parsed double value
  /// @throws   std::invalid_argument if no conversion could be performed
  /// @throws   std::out_of_range if value is out of double range
  [[nodiscard]] inline double parseDouble(const std::string& v) {
    return std::stod(v);
  }

  /// Parses string to boolean. Never throws.
  /// Returns true for: "true", "1", "on", "yes", "+" (case-insensitive)
  /// Returns false for all other values including empty string.
  /// @param v  String to parse
  /// @return   Parsed boolean value
  [[nodiscard]] inline bool parseBool(const std::string& v) {
    if (v.empty()) return false;
    const std::string lower = toLowerCase(v);
    return lower == "true" || lower == "1" || lower == "on" || lower == "yes" || lower == "+";
  }

  /// Identity function for string parsing (for template consistency).
  /// @param v  String value
  /// @return   Same string value
  [[nodiscard]] inline std::string parseString(const std::string& v) {
    return v;
  }

  //=============================================================================
  // String Parsing - Safe variants (return default on invalid, with warning)
  //=============================================================================

  /// Parses string to integer, returning default value on failure.
  /// Logs a warning if parsing fails.
  /// @param v            String to parse
  /// @param defaultValue Value to return if parsing fails (default: 0)
  /// @return             Parsed integer or default value
  [[nodiscard]] inline int parseIntOr(const std::string& v, const int defaultValue = 0) {
    try {
      return std::stoi(v);
    } catch (const std::exception& e) {
      LOG__WARN(Logger::get().CONFIG_LOG,
                "Failed to parse integer from '{}': {} - using default {}", v, e.what(), defaultValue);
      return defaultValue;
    }
  }

  /// Parses string to double, returning default value on failure.
  /// Logs a warning if parsing fails.
  /// @param v            String to parse
  /// @param defaultValue Value to return if parsing fails (default: 0.0)
  /// @return             Parsed double or default value
  [[nodiscard]] inline double parseDoubleOr(const std::string& v, const double defaultValue = 0.0) {
    try {
      return std::stod(v);
    } catch (const std::exception& e) {
      LOG__WARN(Logger::get().CONFIG_LOG,
                "Failed to parse double from '{}': {} - using default {}", v, e.what(), defaultValue);
      return defaultValue;
    }
  }

  //=============================================================================
  // String Parsing - Optional variants (distinguish invalid from valid-zero)
  //=============================================================================

  /// Parses string to integer, returning nullopt on failure.
  /// @param v  String to parse
  /// @return   Parsed integer or nullopt if invalid
  [[nodiscard]] inline std::optional<int> tryParseInt(const std::string& v) {
    try {
      return std::stoi(v);
    } catch (...) {
      return std::nullopt;
    }
  }

  /// Parses string to double, returning nullopt on failure.
  /// @param v  String to parse
  /// @return   Parsed double or nullopt if invalid
  [[nodiscard]] inline std::optional<double> tryParseDouble(const std::string& v) {
    try {
      return std::stod(v);
    } catch (...) {
      return std::nullopt;
    }
  }

}// namespace common

// slower alternatives for trimming
// Round  1 Test  1: 5.684.239.320 ns (   100%) (  5,68423932 sec) ( 56.842,3932 ns avg per test)
// Round  1 Test  2: 5.081.285.270 ns (    89%) (  5,08128527 sec) ( 50.812,8527 ns avg per test)
// Round  1 Test  3:   19.676.920 ns (     0%) (  0,01967692 sec) (    196,7692 ns avg per test)
// Round  1 Test  4:   44.201.860 ns (   224%) (  0,04420186 sec) (    442,0186 ns avg per test)
// Round  1 Test  5:   16.635.990 ns (    37%) (  0,01663599 sec) (    166,3599 ns avg per test)
// Round  1 Test  6:    2.149.750 ns (    12%) (  0,00214975 sec) (     21,4975 ns avg per test)
// 5 = trimFast(string), 6 = trimFast(string_view)
// inline std::string trimRegex(const std::string& toTrim) {
//  const std::regex trimWhiteSpace(R"(^\s+|\s+$)");
//  return std::regex_replace(toTrim, trimWhiteSpace, "");
//}
//
// inline std::string trimRegex(const std::string_view& toTrim) {
//  const std::regex trimWhiteSpace(R"(^\s+|\s+$)");
//  // create a trimmed copy of the string as regex can't handle string_view :(
//  return std::regex_replace(std::string{toTrim}, trimWhiteSpace, "");
//}
//
// inline std::string trimFindNot(const std::string& toTrim,
//                               const std::string& whitespace = " \t\n\v\f\r") {
//  const auto strBegin = toTrim.find_first_not_of(whitespace);
//  if (strBegin == std::string::npos) {
//    return "";// no content
//  }
//  const auto strEnd   = toTrim.find_last_not_of(whitespace);
//  const auto strRange = strEnd - strBegin + 1;
//  return toTrim.substr(strBegin, strRange);
//}
//
// inline std::string& ltrim(std::string& str) {
//  auto it2 =  std::find_if( str.begin() , str.end() , [](char ch){ return !std::isspace<char>(ch , std::locale::classic() ) ; } );
//  str.erase( str.begin() , it2);
//  return str;
//}
//
// inline std::string& rtrim(std::string& str) {
//  auto it1 =  std::find_if( str.rbegin() , str.rend() , [](char ch){ return !std::isspace<char>(ch , std::locale::classic() ) ; } );
//  str.erase( it1.base() , str.end() );
//  return str;
//}
//
// inline std::string& trimFindIf(std::string& str) {
//  return ltrim(rtrim(str));
//}


#endif// FRANKYCPP_STRINGUTIL_H
