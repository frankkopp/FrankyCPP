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

#ifndef FRANKYCPP_HISTORY_H
#define FRANKYCPP_HISTORY_H

//=============================================================================
// History.h - Move Ordering Heuristics Data
//=============================================================================
//
// History stores heuristic data collected during search to improve move
// ordering in subsequent searches. Better move ordering leads to more
// beta cutoffs and faster search.
// Depends on: types.h
//
// Heuristics Stored:
//   - History Heuristic: Counts how often a move caused a beta cutoff.
//     Moves that frequently cause cutoffs are likely good and should be
//     searched earlier. Indexed by [color][from_square][to_square].
//
//   - Counter Move Heuristic: Stores the move that refuted a previous move.
//     If move A was played and move B caused a cutoff, B is stored as the
//     counter to A. Indexed by [from_square][to_square] of the previous move.
//
// Usage in Search:
//   - After a beta cutoff, update history count for the cutoff move
//   - Store the cutoff move as a counter to the previous move
//   - MoveGenerator uses this data to sort quiet moves
//
// Lifetime:
//   - Cleared at the start of each new search (or periodically aged)
//   - Accumulated within a single search
//
//=============================================================================

#include <types/types.h>

#include <array>
#include <cstring>

namespace chess {

  /// Stores move ordering heuristic data for the search.
  /// Updated during search and used by MoveGenerator for move sorting.
  struct History {
    /// History heuristic table: counts beta cutoffs per move.
    /// Indexed by [color][from_square][to_square].
    /// Higher values indicate moves that frequently cause cutoffs.
    std::array<std::array<std::array<int, 64>, 64>, 2> historyCount{};

    /// Counter move table: stores the move that refuted a previous move.
    /// Indexed by [prev_from_square][prev_to_square].
    /// Returns the move that caused a cutoff after the indexed move was played.
    std::array<std::array<Move, 64>, 64> counterMoves{};

    /// Reset all history data to initial state.
    /// More efficient than assigning a new instance as it operates in-place.
    void reset() {
      // Use memset for efficient bulk zeroing (~49KB total)
      std::memset(historyCount.data(), 0, sizeof(historyCount));
      std::memset(counterMoves.data(), 0, sizeof(counterMoves));
    }
  };

} // namespace chess

#endif // FRANKYCPP_HISTORY_H
