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

#ifndef FRANKYCPP_TIMEUTILS_H
#define FRANKYCPP_TIMEUTILS_H

//=============================================================================
// TimeUtils.h - Unified Timestamp Generation
//=============================================================================
//
// Header-only timestamp functions. Replaces duplicated timestamp
// implementations that existed across multiple arena components.
//
// Functions:
//   isoTimestamp()      - UTC ISO 8601 for result files: "2026-03-08T14:30:22Z"
//   fileTimestamp()     - Sortable local time for filenames: "20260308_143022"
//   displayTimestamp()  - Local time for reports: "2026-03-08 14:30:22"
//
// Design Notes:
//   - isoTimestamp() uses UTC (gmtime) for consistency across time zones
//   - fileTimestamp() and displayTimestamp() use local time for user readability
//   - All functions are thread-safe (each call creates its own tm struct)
//
//=============================================================================

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace common {

  /// Returns current UTC time in ISO 8601 format for result files.
  /// Example: "2026-03-08T14:30:22Z"
  /// Uses gmtime (UTC) for cross-timezone consistency.
  [[nodiscard]] inline std::string isoTimestamp() {
    const auto now    = system_clock::now();
    const auto time_t = system_clock::to_time_t(now);
    std::tm tm{};

#ifdef _WIN32
    gmtime_s(&tm, &time_t);
#else
    gmtime_r(&time_t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
  }

  /// Returns current local time in sortable filename format.
  /// Example: "20260308_143022"
  /// Uses localtime for user-friendly filenames.
  [[nodiscard]] inline std::string fileTimestamp() {
    const auto now    = system_clock::now();
    const auto time_t = system_clock::to_time_t(now);
    std::tm tm{};

#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
  }

  /// Returns current local time in human-readable format for reports.
  /// Example: "2026-03-08 14:30:22"
  /// Uses localtime for user readability.
  [[nodiscard]] inline std::string displayTimestamp() {
    const auto now    = system_clock::now();
    const auto time_t = system_clock::to_time_t(now);
    std::tm tm{};

#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
  }

} // namespace common

#endif // FRANKYCPP_TIMEUTILS_H
