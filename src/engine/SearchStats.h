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

#ifndef FRANKYCPP_SEARCHSTATS_H
#define FRANKYCPP_SEARCHSTATS_H

//=============================================================================
// SearchStats.h - Search Statistics Tracking
//=============================================================================
//
// SearchStats collects detailed statistics about a search for analysis,
// debugging, and tuning purposes. Updated during search and accessible
// via Search::getSearchStats().
// Depends on: types.h
//
// Categories:
//   - Current state: depth, best move, current root move being searched
//   - Terminal nodes: checkmates, stalemates, leaf evaluations
//   - Pruning: beta cuts, null-move, futility, razoring, LMP
//   - Extensions: check extensions, threat extensions
//   - TT: hits, misses, cutoffs, eval reuse
//   - Research: PVS, aspiration, LMR re-searches
//
// Usage:
//   const SearchStats& stats = search.getSearchStats();
//   std::cout << "Beta cuts: " << stats.betaCuts
//             << " (1st move: " << stats.betaCuts1st << ")" << std::endl;
//   std::cout << stats.str();  // Full statistics dump
//
//=============================================================================

#include "types/types.h"
#include <ostream>

/// Collects detailed statistics about search behavior for analysis and tuning.
struct SearchStats {

  // === Current Search State ===

  /// Current iteration depth in iterative deepening.
  int currentIterationDepth;

  /// Current nominal search depth.
  int currentSearchDepth;

  /// Extra depth from extensions (selective depth).
  int currentExtraSearchDepth;

  /// Best move found so far at root.
  Move currentBestRootMove;

  /// Evaluation of the current best root move.
  Value currentBestRootMoveValue;

  /// Current principal variation being explored.
  MoveList currentVariation{};

  /// Index of root move currently being searched (0-based).
  size_t currentRootMoveIndex;

  /// Root move currently being searched.
  Move currentRootMove;

  // === Terminal Node Counts ===

  /// Number of checkmate positions found.
  uint64_t checkmates;

  /// Number of stalemate positions found.
  uint64_t stalemates;

  /// Number of leaf positions evaluated (quiescence leaves).
  uint64_t leafPositionsEvaluated;

  /// Total evaluation function calls.
  uint64_t evaluations;

  /// Node count for perft (if running perft).
  uint64_t perftNodeCount;

  // === Pruning Statistics ===

  /// Total beta cutoffs (fail-high).
  uint64_t betaCuts;

  /// Beta cutoffs on first move (indicates good move ordering).
  uint64_t betaCuts1st;

  /// Mate distance pruning cuts.
  uint64_t mdp;

  /// Stand-pat cutoffs in quiescence search.
  uint64_t standpatCuts;

  /// Razoring prunings (near-leaf nodes).
  uint64_t razorings;

  /// Reverse futility pruning cuts.
  uint64_t rfp_cuts;

  /// Null-move pruning cutoffs.
  uint64_t nullMoveCuts;

  /// Futility pruning cuts (moves skipped).
  uint64_t fpPrunings;

  /// Quiescence futility pruning cuts.
  uint64_t qfpPrunings;

  // === Transposition Table Statistics ===

  /// TT probe hits (entry with matching key found).
  uint64_t ttHit;

  /// TT probe misses (no matching entry).
  uint64_t ttMiss;

  /// TT hits that caused a cutoff (value usable).
  uint64_t TtCuts;

  /// TT hits that did not cause a cutoff.
  uint64_t TtNoCuts;

  /// Evaluations retrieved from TT instead of recalculating.
  uint64_t evalFromTT;

  /// TT hits without a stored move.
  uint64_t NoTtMove;

  /// Internal iterative deepening searches performed.
  uint64_t iidSearches;

  /// Moves found via IID.
  uint64_t iidMoves;

  /// TT moves used for move ordering.
  uint64_t TtMoveUsed;

  // === Re-search Statistics ===

  /// PVS re-searches at root (zero-window failed).
  uint64_t rootPvsResearches;

  /// PVS re-searches in non-root nodes.
  uint64_t pvsResearches;

  /// Aspiration window re-searches (fail-high or fail-low).
  uint64_t aspirationResearches;

  /// Number of times best move changed during search.
  uint64_t bestMoveChange;

  /// LMR re-searches (reduced search found better move).
  uint64_t lmrResearches;

  /// LMR reductions applied.
  uint64_t lmrReductions;

  /// Late move pruning cuts.
  uint64_t lmpCuts;

  // === Extension Statistics ===

  /// Check extensions applied.
  uint64_t checkExtension;

  /// Threat extensions applied.
  uint64_t threatExtension;

  /// Null-move verification re-searches that prevented a cutoff.
  uint64_t nullMoveVerifications;

  /// Returns a string representation of all statistics.
  /// @return  Formatted statistics string
  [[nodiscard]] std::string str() const {
    std::ostringstream os;
    os << *this;
    return os.str();
  };

  /// Stream output operator for full statistics dump.
  friend std::ostream& operator<<(std::ostream& os, const SearchStats& stats) {
    os.imbue(deLocale);
    os << "checkmates: " << stats.checkmates
       << " stalemates: " << stats.stalemates
       << " perft: " << stats.perftNodeCount
       << " leafPositionsEvaluated: " << stats.leafPositionsEvaluated
       << " evaluations: " << stats.evaluations
       << " betaCuts: " << stats.betaCuts
       << " betaCuts1st: " << stats.betaCuts1st
       << " rootPvsResearches: " << stats.rootPvsResearches
       << " pvsResearches: " << stats.pvsResearches
       << " aspirationResearches: " << stats.aspirationResearches
       << " bestMoveChange: " << stats.bestMoveChange
       << " mdp: " << stats.mdp
       << " razorings: " << stats.razorings
       << " rfp_cuts: " << stats.rfp_cuts
       << " nmp_cuts: " << stats.nullMoveCuts
       << " nmp_verify: " << stats.nullMoveVerifications
       << " fp_prunings: " << stats.fpPrunings
       << " qfp_prunings: " << stats.qfpPrunings
       << " lmrReductions: " << stats.lmrReductions
       << " lmrResearches: " << stats.lmrResearches
       << " check ext: " << stats.checkExtension
       << " threat ext: " << stats.threatExtension
       << " ttHit: " << stats.ttHit
       << " ttMiss: " << stats.ttMiss
       << " TtCuts: " << stats.TtCuts
       << " TtNoCuts: " << stats.TtNoCuts
       << " evalFromTT: " << stats.evalFromTT
       << " TtMoveUsed: " << stats.TtMoveUsed
       << " NoTtMove: " << stats.NoTtMove
       << " IID Searches: " << stats.iidSearches
       << " IID Moves: " << stats.iidMoves;
    return os;
  }
};


#endif//FRANKYCPP_SEARCHSTATS_H
