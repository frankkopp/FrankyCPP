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

#ifndef FRANKYCPP_SEARCHRESULT_H
#define FRANKYCPP_SEARCHRESULT_H

//=============================================================================
// SearchResult.h - Search Result Data Structure
//=============================================================================
//
// SearchResult stores the outcome and metadata from a completed search.
// Returned by Search::getLastSearchResult() after search completes.
// Depends on: types.h
//
// Contents:
//   - Best move and its evaluation
//   - Ponder move (opponent's expected reply)
//   - Search depth reached
//   - Node count and elapsed time
//   - Principal variation (PV)
//   - Flags for book moves and mate detection
//
// Usage:
//   search.waitWhileSearching();
//   SearchResult result = search.getLastSearchResult();
//   std::cout << "Best: " << result.bestMove.str()
//             << " Score: " << result.bestMoveValue.str() << std::endl;
//
//=============================================================================

#include "types/types.h"

/// Stores the result and metadata from a completed search.
struct SearchResult {
  /// Best move found by the search.
  Move bestMove = MOVE_NONE;

  /// Evaluation score of the best move (from side-to-move perspective).
  Value bestMoveValue = VALUE_NONE;

  /// Expected opponent reply (for pondering).
  /// Second move in the PV if available.
  Move ponderMove = MOVE_NONE;

  /// Search depth completed (full iterations).
  int depth = 0;

  /// Extra depth from extensions (selective depth beyond nominal depth).
  int extraDepth = 0;

  /// Total nodes visited during the search.
  uint64_t nodes = 0;

  /// True if bestMove came from the opening book (not searched).
  bool bookMove = false;

  /// True if bestMove came from tablebase probe at root.
  bool tbHit = false;

  /// True if a forced mate was found.
  bool mateFound = false;

  /// Total time spent on the search.
  nanoseconds time{};

  /// Principal variation (best line of play from both sides).
  MoveList pv{};

  /// Returns a string representation for debugging/logging.
  /// @return  Debug string with best move, score, ponder, and depth
  std::string str() const {
    const std::string source = bookMove ? "book move" : (tbHit ? "TB hit" : bestMoveValue.str());
    return "Best Move: " + bestMove.str() + " (" + source
           + ") " + "Ponder Move: " + ponderMove.str() + " Depth: " + std::to_string(depth)
           + "/" + std::to_string(extraDepth);
  }
};

/// Stream output operator for SearchResult.
inline std::ostream& operator<<(std::ostream& os, const SearchResult& searchResult) {
  os << searchResult.str();
  return os;
}

#endif// FRANKYCPP_SEARCHRESULT_H
