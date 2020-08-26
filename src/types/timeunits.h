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

#ifndef FRANKYCPP_TIMEUNITS_H
#define FRANKYCPP_TIMEUNITS_H

#include <chrono>
#include <iomanip>
#include <sstream>

using namespace std::chrono_literals;
using namespace std::chrono;

typedef time_point<std::chrono::high_resolution_clock> TimePoint;

// returns a string representation of the milliseconds as a fraction of a seconds
// with a DE locale
//  Examples:
//  5,021 s
inline std::string str(milliseconds s) {
  return fmt::format(deLocale, "{:.3f} s", static_cast<double>(s.count()) / 1e3);
}

// returns a string representation of the nanoseconds as a fraction of a seconds
// with a DE locale
//  Examples:
//  5,021456234 s
inline std::string str(nanoseconds s) {
  return fmt::format(deLocale, "{:.9f} s", static_cast<double>(s.count()) / 1e9);
}

// returns a string representation of the duration as a human readable string
// with a DE locale
// Examples:
//  59y:325d:20h:33m:19s:008.800.999ns
//  20d:13h:53m:19s:008.800.999ns
//  33m:19s:008.800.999ns
//  1s:000.000.999ns
//  1.000.099ns
//  10.000ns
//  100ns
template<typename T>
inline std::string format(T timeunit) {
  nanoseconds ns = duration_cast<nanoseconds>(timeunit);
  std::ostringstream os;
  bool foundNonZero  = false;
  os.fill('0');
  typedef duration<int, std::ratio<86400*365>> years;
  const auto y = duration_cast<years>(ns);
  if (y.count()) {
    foundNonZero = true;
    os << y.count() << "y:";
    ns -= y;
  }
  typedef duration<int, std::ratio<86400>> days;
  const auto d = duration_cast<days>(ns);
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

// returns the nodes per second from nano seconds given as uint64_t
inline uint64_t nps(uint64_t nodes, uint64_t ns) {
  if (!ns) return nodes;
  return nodes * nanoPerSec / ns;
}

// returns the nodes per second from milli seconds
template <typename T>
inline uint64_t nps(uint64_t nodes, T timeunit) {
  nanoseconds ns = duration_cast<nanoseconds>(timeunit);
  if (!ns.count()) return nodes;
  return (nodes * nanoPerSec) / ns.count();
}

inline nanoseconds elapsedSince(const TimePoint tp) {
  return high_resolution_clock::now() - tp;
}

// faster on Apple - returns nanoseconds
inline unsigned long long int nowFast() {
#if defined(__APPLE__)
  // this C function is much faster than c++ chrono
  return clock_gettime_nsec_np(CLOCK_UPTIME_RAW_APPROX);
#else
  return std::chrono::high_resolution_clock::now().time_since_epoch().count();
#endif
}

// convenience for std::chrono::high_resolution_clock::now()
constexpr auto currentTime = std::chrono::high_resolution_clock::now;

#define SLEEP(t) std::this_thread::sleep_for(t)
#define NANOSECONDS(t) std::chrono::duration_cast<std::chrono::nanoseconds>(t)
#define MILLISECONDS(t) std::chrono::duration_cast<std::chrono::milliseconds>(t)

#endif//FRANKYCPP_TIMEUNITS_H
