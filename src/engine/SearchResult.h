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

#ifndef FRANKYCPP_SEARCHRESULT_H
#define FRANKYCPP_SEARCHRESULT_H

#include "types/types.h"

struct SearchResult {
  Move    bestMove      = MOVE_NONE;
  Value   bestMoveValue = VALUE_NONE;
  Move    ponderMove    = MOVE_NONE;
  int64_t time          = 0;
  int     depth         = 0;
  int     extraDepth    = 0;

  std::string str () const {
    return "Best Move: " + ::str(bestMove)
           + " (" + std::to_string (bestMoveValue) + ") " + "Ponder Move: "
           + ::str(ponderMove) + " Depth: " + std::to_string (depth)
           + "/" + std::to_string (extraDepth);
  }
};

inline std::ostream& operator<< (std::ostream& os, const SearchResult& searchResult) {
  os << searchResult.str ();
  return os;
}

#endif//FRANKYCPP_SEARCHRESULT_H
