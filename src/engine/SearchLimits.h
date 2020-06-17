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

#ifndef FRANKYCPP_SEARCHLIMITS_H
#define FRANKYCPP_SEARCHLIMITS_H

#include <cstdint>
#include <ostream>
#include <types/types.h>

// Limits is data structure to hold all information about how
// a search of the chess games shall be controlled.
// Search needs to read these and determine the necessary limits.
// E.g. time controlled game or not
struct SearchLimits {
  // no time control
  bool infinite = false;
  bool ponder   = false;
  int mate      = 0;

  // extra limits
  int depth      = 0;
  uint64_t nodes = 0;
  MoveList moves{};

  //  time control;
  bool timeControl   = false;
  MilliSec whiteTime = MilliSec{0};
  MilliSec blackTime = MilliSec{0};
  MilliSec whiteInc  = MilliSec{0};
  MilliSec blackInc  = MilliSec{0};
  MilliSec moveTime  = MilliSec{0};

  // parameter
  int movesToGo = 0;

  std::string str() const {
    std::ostringstream os;
    os << "infinite: " << infinite << " ponder: " << ponder << " mate: " << mate << " depth: " << depth << " nodes: " << nodes << " moves: " << moves << " timeControl: " << timeControl << " whiteTime: " << whiteTime.count() << " blackTime: " << blackTime.count() << " whiteInc: " << whiteInc.count() << " blackInc: " << blackInc.count() << " moveTime: " << moveTime.count() << " movesToGo: " << movesToGo;
    return os.str();
  }

  friend std::ostream& operator<<(std::ostream& os, const SearchLimits& limits) {
    os << limits.str();
    return os;
  }
};

#endif//FRANKYCPP_SEARCHLIMITS_H
