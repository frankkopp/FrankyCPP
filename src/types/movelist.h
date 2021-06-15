// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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

#include <vector>
#include <sstream>
#include "move.h"

/// A collection of moves using a std::vector
typedef std::vector<Move> MoveList;

// returns a uci compatible string representation of the move list
inline std::string str(const MoveList& moveList) {
  std::ostringstream os;
  for (Move m : moveList) {
    os << m;
    if (m != moveList.back()) os << " ";
  }
  return os.str();
}

inline std::string strVerbose(const MoveList& moveList) {
  std::ostringstream os;
  os << "MoveList: size=" << moveList.size() << " [";
  for (auto itr = moveList.begin(); itr != moveList.end(); ++itr) {
    os << *itr;
    if (itr != moveList.end() - 1) os << ", ";
  }
  os << "]";
  return os.str();
}

inline std::ostream& operator<< (std::ostream& os, const MoveList& moveList) {
  os << str(moveList);
  return os;
}

#endif//FRANKYCPP_MOVELIST_H
