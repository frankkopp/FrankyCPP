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

#ifndef FRANKYCPP_MOVELIST_H
#define FRANKYCPP_MOVELIST_H

#include "move.h"
#include <ostream>
#include <sstream>
#include <vector>

/// A collection of moves inheriting from std::vector
class MoveList : public std::vector<Move> {
public:
  using std::vector<Move>::vector;

  // returns a uci compatible string representation of the move list
  std::string str() const {
    std::ostringstream os;
    const auto n = this->size();
    for (std::size_t i = 0; i < n; ++i) {
      os << this->at(i);
      if (i + 1 < n) os << ' ';
    }
    return os.str();
  }

  std::string strVerbose() const {
    std::ostringstream os;
    os << "MoveList: size=" << this->size() << " [";
    for (auto itr = this->begin(); itr != this->end(); ++itr) {
      os << *itr;
      if (itr + 1 != this->end()) os << ", ";
    }
    os << "]";
    return os.str();
  }

  friend std::ostream& operator<<(std::ostream& os, const MoveList& moveList) {
    os << moveList.str();
    return os;
  }
};


#endif// FRANKYCPP_MOVELIST_H
