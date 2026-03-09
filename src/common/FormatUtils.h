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

#ifndef FRANKYCPP_FORMATUTILS_H
#define FRANKYCPP_FORMATUTILS_H

//=============================================================================
// FormatUtils.h - Shared Formatting Utilities
//=============================================================================
//
// Header-only formatting functions for numbers, nodes, time, and text layout.
// Uses projectLocale for locale-aware formatting ("." thousands, "," decimal).
//
// Functions:
//   fixedWidth(str, width)    - Pad or truncate string to exact column width
//   formatNumber(value)       - Thousands-separated integer (e.g., "1.234.567")
//   formatNodes(nodes)        - Compact node count (e.g., "12,4M", "3,2B")
//   formatTime(timeMs)        - Compact time display (e.g., "1,2s", "3,5m")
//   formatDelta(delta)        - Signed integer delta (e.g., "+5", "-3", "0")
//   formatPercent(value)      - Percentage value (e.g., "85,5%", "100,0%")
//   formatDeltaPercent(delta) - Signed percentage (e.g., "+2,5%", "-1,0%")
//
//=============================================================================

#include "types/globals.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace common {

  /// Pad or truncate a string to an exact column width.
  /// If the string is longer than width, it is truncated with ".." suffix.
  /// @param str   Input string
  /// @param width Target column width
  /// @return      String of exactly `width` characters
  [[nodiscard]] inline std::string fixedWidth(const std::string& str, const size_t width) {
    if (width < 3) {
      return str.substr(0, width);
    }
    if (str.length() > width) {
      return str.substr(0, width - 2) + "..";
    }
    return str + std::string(width - str.length(), ' ');
  }

  /// Formats an integer with thousands separator using projectLocale.
  /// Example: 1234567 -> "1.234.567"
  /// @param value Integer value (negative values get a leading '-')
  /// @return      Formatted string with projectLocale thousands separators
  [[nodiscard]] inline std::string formatNumber(const int64_t value) {
    std::ostringstream oss;
    oss.imbue(projectLocale);
    oss << value;
    return oss.str();
  }

  /// Formats a node count with compact suffix using projectLocale.
  /// Example: 12400000 -> "12,4M"
  /// Uses B (billions), M (millions), K (thousands) suffixes.
  /// @param nodes Node count as double
  /// @return      Compact formatted string
  [[nodiscard]] inline std::string formatNodes(const double nodes) {
    if (nodes >= 1e9) {
      std::ostringstream oss;
      oss.imbue(projectLocale);
      oss << std::fixed << std::setprecision(1) << (nodes / 1e9) << "B";
      return oss.str();
    }
    if (nodes >= 1e6) {
      std::ostringstream oss;
      oss.imbue(projectLocale);
      oss << std::fixed << std::setprecision(1) << (nodes / 1e6) << "M";
      return oss.str();
    }
    if (nodes >= 1e3) {
      std::ostringstream oss;
      oss.imbue(projectLocale);
      oss << std::fixed << std::setprecision(1) << (nodes / 1e3) << "K";
      return oss.str();
    }
    return std::to_string(static_cast<int64_t>(nodes));
  }

  /// Formats a time in milliseconds compactly using projectLocale.
  /// Example: 1234ms -> "1,2s"
  /// Uses m (minutes), s (seconds), ms (milliseconds) suffixes.
  /// @param timeMs Time in milliseconds
  /// @return       Compact formatted string
  [[nodiscard]] inline std::string formatTime(const double timeMs) {
    std::ostringstream oss;
    oss.imbue(projectLocale);
    if (timeMs >= 60000) {
      oss << std::fixed << std::setprecision(1) << (timeMs / 60000) << "m";
      return oss.str();
    }
    if (timeMs >= 1000) {
      oss << std::fixed << std::setprecision(1) << (timeMs / 1000) << "s";
      return oss.str();
    }
    oss << std::fixed << std::setprecision(0) << timeMs << "ms";
    return oss.str();
  }

  /// Formats a delta value with sign (e.g., +5, -3, 0).
  /// @param delta Integer delta
  /// @return      Signed string representation
  [[nodiscard]] inline std::string formatDelta(const int delta) {
    if (delta > 0) return "+" + std::to_string(delta);
    if (delta < 0) return std::to_string(delta);
    return "0";
  }

  /// Formats a percentage value using projectLocale.
  /// Example: 85.5 -> "85,5%", 100.0 -> "100,0%"
  /// @param value     Percentage value
  /// @param precision Decimal places (default: 1)
  /// @return          Formatted percentage string
  [[nodiscard]] inline std::string formatPercent(const double value, const int precision = 1) {
    std::ostringstream oss;
    oss.imbue(projectLocale);
    oss << std::fixed << std::setprecision(precision) << value << "%";
    return oss.str();
  }

  /// Formats a delta percentage with sign using projectLocale.
  /// Example: 2.5 -> "+2,5%", -1.0 -> "-1,0%"
  /// @param delta Percentage delta
  /// @return      Signed percentage string
  [[nodiscard]] inline std::string formatDeltaPercent(const double delta) {
    std::ostringstream oss;
    oss.imbue(projectLocale);
    oss << std::fixed << std::setprecision(1);
    if (delta > 0) oss << "+";
    oss << delta << "%";
    return oss.str();
  }

} // namespace common

#endif // FRANKYCPP_FORMATUTILS_H
