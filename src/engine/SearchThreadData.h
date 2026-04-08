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

#ifndef FRANKYCPP_SEARCHTHREADDATA_H
#define FRANKYCPP_SEARCHTHREADDATA_H

//=============================================================================
// SearchThreadData.h - Per-Thread Search State for Lazy SMP
//=============================================================================
//
// SearchThreadData encapsulates all thread-local state needed for parallel search.
// Each search thread (main + helpers) owns one SearchThreadData instance.
//
// Shared state (TT, stop flag, time limits, position) lives in Search and is
// accessed via pointers/references.
//
// Thread-local state includes:
//   - PVTable: triangular PV storage
//   - plyStack: per-ply search state (MoveGenerators, killers, etc.)
//   - history: move ordering heuristics
//   - statistics: search statistics for debugging
//   - nodesVisited: node counter for this thread
//   - LMR_REDUCTION: precomputed LMR table
//
// Usage:
//   SearchThreadData& st = mainThread();  // or helpers[i]
//   st.pv.update(move, ply);
//   st.statistics.betaCuts++;
//   st.nodesVisited++;
//
//=============================================================================

#include "Evaluator.h"
#include "PVTable.h"
#include "PlyInfo.h"
#include "SearchStats.h"
#include "chesscore/History.h"
#include "chesscore/Position.h"
#include "types/staticmovelist.h"
#include "types/types.h"

#include <array>
#include <cmath>

namespace engine {
  using namespace chess;

  /// Per-thread search state for Lazy SMP.
  /// Each thread (main + helpers) owns one SearchThreadData instance.
  /// Shared state (TT, PawnTT, stop flag, time limits) lives in Search.
  struct SearchThreadData {
    /// Thread ID: 0 = main thread, 1..N-1 = helper threads
    int id = 0;

    /// Thread-local node counter (aggregated across threads for UCI reporting).
    /// NOT atomic: each thread only writes to its own counter, reads by other
    /// threads (getTotalNodes) are racy but acceptable for approximate statistics.
    /// This avoids the ~20x overhead of atomic increment on the hot path.
    uint64_t nodesVisited{0};

    /// Thread-local copy of the position for this search thread.
    /// Initialized from Search::position before helpers are launched.
    /// Each thread works on its own copy to avoid data races.
    Position position{};

    /// Per-thread evaluator with thread-local scratch variables.
    /// Uses shared PawnTT (set via evaluator.setPawnTT() from Search).
    Evaluator evaluator{};

    /// Triangular PV table for efficient PV storage (64KB, zero heap allocations)
    PVTable pv;

    /// Per-ply search state - unified struct for all ply-specific data
    /// Each PlyInfo owns its MoveGenerators via unique_ptr (heap-allocated)
    std::array<PlyInfo, DEPTH_MAX + 1> plyStack{};

    /// History heuristics for move ordering
    History history{};

    /// Search statistics for debugging and analysis
    SearchStats statistics{};

    /// MoveGenerator for PV extraction (reused to avoid allocation per call).
    /// Must be per-thread because extractPvWithTT() calls validateMove() which
    /// writes to internal buffers — shared access causes data race crashes.
    MoveGenerator pvMoveGenerator{};

    /// Thread-local root moves for this search.
    /// Each thread generates and maintains its own root move list.
    /// After each iteration, rootMoves[0] contains the best move with its score.
    MoveList rootMoves{};

    /// Highest fully completed iteration depth for this thread.
    /// Used for best-thread selection at search end.
    /// Reset to DEPTH_NONE before each search; updated after each successful iteration.
    Depth completedIterationDepth = DEPTH_NONE;

    /// Score from the last completed iteration.
    /// Used alongside completedIterationDepth for best-thread selection.
    Value lastIterationValue = VALUE_NONE;

    /// LMR reduction table pre-computed for depth 0..31 and moves searched 0..63
    /// Regenerated at search start based on config settings
    std::array<std::array<int, 64>, 32> LMR_REDUCTION{};

    // =========================================================================
    // Constructors
    // =========================================================================

    /// Default constructor - creates a SearchThreadData with id 0
    SearchThreadData() : SearchThreadData(0) {}

    /// Constructor with thread ID
    explicit SearchThreadData(const int threadId) : id(threadId) {}

    // =========================================================================
    // Reset Methods
    // =========================================================================

    /// Resets all search state for a new game (full reset including history).
    /// Preserves MoveGenerator heap allocations for reuse.
    void reset() {
      nodesVisited = 0;
      statistics = SearchStats{};
      completedIterationDepth = DEPTH_NONE;
      lastIterationValue = VALUE_NONE;
      rootMoves.clear();
      pv.clearAll();
      for (auto& plyInfo : plyStack) {
        plyInfo.resetSearchState();
      }
      history.reset();
    }

    /// Resets thread state for a new search. Does NOT clear history tables
    /// (history is preserved across searches for better move ordering).
    /// @param rootPosition  Position to copy for this thread's local search
    /// @param pawnTT        Shared PawnTT to inject into evaluator
    /// @param lmrUseLog     Whether LMR uses logarithmic formula
    /// @param lmrLogDiv     LMR log divisor
    /// @param useHistory    Whether history-based move ordering is enabled
    void resetForNewSearch(const Position& rootPosition, PawnTT* pawnTT,
                           const bool lmrUseLog, const double lmrLogDiv,
                           const bool useHistory) {
      // Reset counters, statistics, and best-thread selection state
      nodesVisited            = 0;
      statistics              = SearchStats{};
      completedIterationDepth = DEPTH_NONE;
      lastIterationValue      = VALUE_NONE;

      // Copy root position to thread-local storage.
      // Each thread works on its own copy to avoid data races during search.
      position = rootPosition;

      // Clear thread-local root moves (will be populated during root search)
      rootMoves.clear();

      // Set shared PawnTT on this thread's evaluator
      evaluator.setPawnTT(pawnTT);

      // Regenerate LMR table based on current config
      regenerateLmrTable(lmrUseLog, lmrLogDiv);

      // Clear PV table
      pv.clearAll();

      // Initialize per-ply search state (MoveGenerators, history pointers, etc.)
      for (int i = DEPTH_NONE; i < DEPTH_MAX; i++) {
        plyStack[i].resetSearchState();
        if (useHistory) {
          plyStack[i].mg->setHistoryData(&history);
        }
      }
    }

    // =========================================================================
    // LMR Table Generation
    // =========================================================================

    /// Linear formula: 1 + round(depth * movesSearched * 0.0035)
    static constexpr int lmr_reduction_linear(const int depth, const int movesSearched) noexcept {
      // exact integer rounding of 35/10000
      return 1 + (depth * movesSearched * 35 + 5000) / 10000;
    }

    /// Logarithmic formula: log(depth) * log(moves) / divisor
    /// This provides more gradual reductions that scale better at higher depths
    static int lmr_reduction_log(const int depth, const int movesSearched, const double divisor) noexcept {
      if (depth <= 1 || movesSearched <= 1) return 1;
      return static_cast<int>(std::lround(std::log(depth) * std::log(movesSearched) / divisor));
    }

    /// Regenerates the LMR table based on config settings
    /// @param useLogFormula  If true, use logarithmic formula; otherwise linear
    /// @param logBaseDivisor Divisor for logarithmic formula (only used if useLogFormula is true)
    void regenerateLmrTable(const bool useLogFormula, const double logBaseDivisor) {
      if (useLogFormula) {
        for (std::size_t d = 0; d < 32; ++d) {
          for (std::size_t m = 0; m < 64; ++m) {
            LMR_REDUCTION[d][m] = lmr_reduction_log(static_cast<int>(d), static_cast<int>(m), logBaseDivisor);
          }
        }
      }
      else {
        for (std::size_t d = 0; d < 32; ++d) {
          for (std::size_t m = 0; m < 64; ++m) {
            LMR_REDUCTION[d][m] = lmr_reduction_linear(static_cast<int>(d), static_cast<int>(m));
          }
        }
      }
    }
  };

} // namespace engine

#endif // FRANKYCPP_SEARCHTHREADDATA_H
