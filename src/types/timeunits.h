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

using namespace std::chrono_literals ;
using namespace std::chrono ;

typedef time_point<std::chrono::high_resolution_clock> TimePoint;
typedef milliseconds MilliSec;
typedef nanoseconds NanoSec;


inline std::string str(MilliSec s) {
  return fmt::format(deLocale, "{:.3f} s", static_cast<double>(s.count())/1e3);
}

inline std::string str(NanoSec s) {
  return fmt::format(deLocale, "{:.9f} s", static_cast<double>(s.count())/1e9);
}

// returns the nodes per second from milli seconds
inline uint64_t nps(uint64_t nodes, MilliSec ms) {
  if (!ms.count()) return nodes;
  return nodes * 1'000 / ms.count() ; // +1 to avoid division by zero
}

// returns the nodes per second from nano seconds
inline uint64_t nps(uint64_t nodes, NanoSec ms) {
  if (!ms.count()) return nodes;
  return nodes * 1'000'000'000 / ms.count(); // +1 to avoid division by zero
}

inline NanoSec elapsedSince(const TimePoint tp) {
  return high_resolution_clock::now() - tp;
}

// convenience for std::chrono::high_resolution_clock::now()
constexpr auto now = high_resolution_clock::now;

#define SLEEP(t) std::this_thread::sleep_for(t)
#define NANOSECONDS(t) std::chrono::duration_cast<std::chrono::nanoseconds>(t)
#define MILLISECONDS(t) std::chrono::duration_cast<std::chrono::milliseconds>(t)

#endif//FRANKYCPP_TIMEUNITS_H
