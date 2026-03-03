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

#ifndef FRANKYCPP_SEARCHLIMITS_H
#define FRANKYCPP_SEARCHLIMITS_H

//=============================================================================
// SearchLimits.h - Search Control Parameters
//=============================================================================
//
// SearchLimits holds all parameters that control how the search operates.
// This includes time control, depth limits, node limits, and special modes.
// Depends on: types.h
//
// Control Modes:
//   - Infinite: Search until explicitly stopped (analysis mode)
//   - Ponder: Think on opponent's time (search continues after bestmove)
//   - Mate search: Search for mate in N moves
//   - Fixed depth: Stop after reaching specified depth
//   - Fixed nodes: Stop after visiting specified number of nodes
//   - Fixed time: Stop after moveTime milliseconds
//   - Time control: Manage time based on remaining clock and increment
//
// UCI Commands Mapped:
//   go infinite       -> infinite = true
//   go ponder         -> ponder = true
//   go mate N         -> mate = N
//   go depth N        -> depth = N
//   go nodes N        -> nodes = N
//   go movetime N     -> moveTime = N
//   go wtime/btime    -> whiteTime/blackTime, timeControl = true
//   go winc/binc      -> whiteInc/blackInc
//   go movestogo N    -> movesToGo = N
//   go searchmoves    -> moves list
//
// Usage:
//   SearchLimits limits;
//   limits.timeControl = true;
//   limits.whiteTime = milliseconds{60000};
//   limits.whiteInc = milliseconds{1000};
//   search.startSearch(position, limits);
//
//=============================================================================

#include <cstdint>
#include <ostream>
#include <types/types.h>

namespace engine {
  using namespace chess;

  /// Holds all parameters controlling search behavior.
  /// Populated from the UCI "go" command and read by the Search class.
  struct SearchLimits {

    // === Special Modes (no time control) ===

    /// If true, search indefinitely until stopped (UCI: "go infinite").
    bool infinite = false;

    /// If true, search in pondering mode on opponent's time.
    /// Search continues after sending bestmove until "ponderhit" or "stop".
    bool ponder = false;

    /// If > 0, search for mate in this many moves (UCI: "go mate N").
    int mate = 0;

    // === Hard Limits ===

    /// Maximum search depth in plies. 0 = no limit (UCI: "go depth N").
    int depth = 0;

    /// Maximum nodes to visit. 0 = no limit (UCI: "go nodes N").
    uint64_t nodes = 0;

    /// Restrict search to these moves only (UCI: "go searchmoves ...").
    MoveList moves{};

    // === Time Control ===

    /// If true, time management is active based on clock times below.
    bool timeControl = false;

    /// White's remaining time on clock (UCI: "go wtime N").
    milliseconds whiteTime{0};

    /// Black's remaining time on clock (UCI: "go btime N").
    milliseconds blackTime{0};

    /// White's increment per move (UCI: "go winc N").
    milliseconds whiteInc{0};

    /// Black's increment per move (UCI: "go binc N").
    milliseconds blackInc{0};

    /// Fixed time per move, overrides clock-based calculation (UCI: "go movetime N").
    milliseconds moveTime{0};

    /// Moves remaining until next time control. 0 = sudden death.
    /// Used to divide remaining time appropriately (UCI: "go movestogo N").
    int movesToGo = 0;

    /// Returns a string representation of all search limits for debugging.
    /// @return Debug string with all field values
    [[nodiscard]] std::string str() const {
      std::ostringstream os;
      os << "infinite: " << infinite << " ponder: " << ponder << " mate: " << mate << " depth: " << depth << " nodes: " << nodes << " moves: " << moves << " timeControl: " << timeControl << " whiteTime: " << whiteTime.count() << " blackTime: " << blackTime.count() << " whiteInc: " << whiteInc.count() << " blackInc: " << blackInc.count() << " moveTime: " << moveTime.count() << " movesToGo: " << movesToGo;
      return os.str();
    }

    /// Stream output operator for debugging.
    friend std::ostream& operator<<(std::ostream& os, const SearchLimits& limits) {
      os << limits.str();
      return os;
    }
  };

}// namespace engine

#endif// FRANKYCPP_SEARCHLIMITS_H
