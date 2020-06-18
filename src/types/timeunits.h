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


typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;
typedef std::chrono::milliseconds MilliSec;
typedef std::chrono::nanoseconds NanoSec;

inline std::string str(MilliSec s) {
  return fmt::format(deLocale, "{:.3f} s", static_cast<double>(s.count())/1e3);
}

inline std::string str(NanoSec s) {
  return fmt::format(deLocale, "{:.9f} s", static_cast<double>(s.count())/1e9);
}


#endif//FRANKYCPP_TIMEUNITS_H
