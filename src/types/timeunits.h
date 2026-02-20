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

#ifndef FRANKYCPP_TIMEUNITS_H
#define FRANKYCPP_TIMEUNITS_H

//=============================================================================
// timeunits.h - Time Utilities and Chrono Helpers
//=============================================================================
//
// Provides time measurement and formatting utilities using std::chrono.
// No internal dependencies (uses only standard library).
//
// Types:
//   TimePoint              - high_resolution_clock time point
//   milliseconds, etc.     - std::chrono duration types (via using)
//
// Key Functions:
//   currentTime()          - Get current time point
//   elapsedSince(tp)       - Nanoseconds elapsed since time point
//   str(milliseconds)      - Format as "X.XXX s"
//   str(nanoseconds)       - Format as "X.XXXXXXXXX s"
//   format_now(fmt)        - Format current local time
//   nps(nodes, time)       - Calculate nodes per second
//
// Usage:
//   TimePoint start = currentTime();
//   // ... do work ...
//   auto elapsed = elapsedSince(start);
//   std::cout << str(duration_cast<milliseconds>(elapsed));
//
//   uint64_t speed = nps(nodeCount, elapsed);
//
//=============================================================================

#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

using namespace std::chrono_literals;
using namespace std::chrono;

typedef time_point<high_resolution_clock> TimePoint;

/// Formats current local time as a string.
/// @param fmt  strftime format string (default: "%Y-%m-%d %H:%M:%S")
/// @return     Formatted time string (e.g., "2026-01-30 14:30:45")
inline std::string format_now(const char* fmt = "%Y-%m-%d %H:%M:%S") {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &t);
#else
  ::localtime_r(&t, &tm);
#endif
  std::ostringstream os;
  os << std::put_time(&tm, fmt);
  return os.str();
}

/// Formats milliseconds as seconds with 3 decimal places (DE locale).
/// @param s  Duration in milliseconds
/// @return   String like "5,021 s"
inline std::string str(const milliseconds s) {
  return std::format(deLocale, "{:.3Lf} s", static_cast<double>(s.count()) / 1e3);
}

/// Formats nanoseconds as seconds with 9 decimal places (DE locale).
/// @param s  Duration in nanoseconds
/// @return   String like "5,021456234 s"
inline std::string str(const nanoseconds s) {
  return std::format(deLocale, "{:.9Lf} s", static_cast<double>(s.count()) / 1e9);
}

/// Formats a duration as a human-readable string with full breakdown.
/// Uses DE locale. Limited to ~292 years due to int64 nanosecond representation.
/// @tparam Rep     Duration representation type
/// @tparam Period  Duration period type
/// @param timeunit Duration to format
/// @return         String like "20d:13h:53m:19s:008.800.999ns" or "100ns"
/// @throws std::overflow_error if duration exceeds int64 nanosecond range
template <class Rep, class Period>
std::string format(duration<Rep, Period> timeunit) {

  // protect against overflow when converting to nanoseconds
  using Unit         = duration<double, nanoseconds::period>;
  constexpr Unit min   = nanoseconds::min();
  constexpr Unit max   = nanoseconds::max();
  const Unit sTimeUnit = timeunit;
  if (sTimeUnit < min || sTimeUnit > max)
    throw std::overflow_error("formatting a duration failed due to overflow of int64 nanoseconds");

  nanoseconds ns = duration_cast<nanoseconds>(timeunit);
  std::ostringstream os;
  bool foundNonZero  = false;
  os.fill('0');
  const auto y = duration_cast<duration<int, std::ratio<86400 * 365>>>(ns);
  if (y.count()) {
    foundNonZero = true;
    os << y.count() << "y:";
    ns -= y;
  }
  const auto d = duration_cast<duration<int, std::ratio<86400>>>(ns);
  if (d.count()) {
    foundNonZero = true;
    os << d.count() << "d:";
    ns -= d;
  }
  const auto h = duration_cast<hours>(ns);
  if (h.count() || foundNonZero) {
    foundNonZero = true;
    os << h.count() << "h:";
    ns -= h;
  }
  const auto m = duration_cast<minutes>(ns);
  if (m.count() || foundNonZero) {
    foundNonZero = true;
    os << m.count() << "m:";
    ns -= m;
  }
  const auto s = duration_cast<seconds>(ns);
  if (s.count() || foundNonZero) {
    foundNonZero = true;
    os << s.count() << "s:";
    ns -= s;
  }
  const auto ms = duration_cast<milliseconds>(ns);
  if (ms.count() || foundNonZero) {
    if (foundNonZero) {
      os << std::setw(3);
    }
    os << ms.count() << ".";
    ns -= ms;
    foundNonZero = true;
  }
  const auto us = duration_cast<microseconds>(ns);
  if (us.count() || foundNonZero) {
    if (foundNonZero) {
      os << std::setw(3);
    }
    os << us.count() << ".";
    ns -= us;
  }
  os << std::setw(3) << ns.count() << "ns" ;
  return os.str();
}

/// Calculates nodes per second from node count and elapsed nanoseconds.
/// @param nodes  Number of nodes processed
/// @param ns     Elapsed time in nanoseconds (as uint64_t)
/// @return       Nodes per second (returns nodes if ns is 0)
inline uint64_t nps(const uint64_t nodes, const uint64_t ns) {
  if (!ns) return nodes;
  return nodes * nanoPerSec / ns;
}

/// Calculates nodes per second from node count and elapsed duration.
/// @tparam T     Duration type (e.g., milliseconds, nanoseconds)
/// @param nodes  Number of nodes processed
/// @param timeunit  Elapsed time as chrono duration
/// @return       Nodes per second (returns nodes if duration is 0)
template <typename T>
uint64_t nps(const uint64_t nodes, T timeunit) {
  const nanoseconds ns = duration_cast<nanoseconds>(timeunit);
  if (!ns.count()) return nodes;
  return nodes * nanoPerSec / ns.count();
}

/// Returns nanoseconds elapsed since the given time point.
/// @param tp  Starting time point
/// @return    Duration in nanoseconds
inline nanoseconds elapsedSince(const TimePoint tp) {
  return high_resolution_clock::now() - tp;
}

/// Returns current time as raw nanosecond count (fast path for Apple).
/// @return  Nanoseconds since epoch
inline unsigned long long int nowFast() {
  return high_resolution_clock::now().time_since_epoch().count();
}

/// Alias for std::chrono::high_resolution_clock::now().
constexpr auto currentTime = high_resolution_clock::now;

/// Convenience macros for common chrono operations
#define SLEEP(t) std::this_thread::sleep_for(t)
#define NANOSECONDS(t) std::chrono::duration_cast<std::chrono::nanoseconds>(t)
#define MILLISECONDS(t) std::chrono::duration_cast<std::chrono::milliseconds>(t)

#endif//FRANKYCPP_TIMEUNITS_H
