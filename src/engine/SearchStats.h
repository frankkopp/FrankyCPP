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
//             << " (1st move: " << stats.betaCutsByIndex[0] << ")" << std::endl;
//   std::cout << stats.str();  // Full statistics dump
//
//=============================================================================

#include "types/types.h"
#include <iomanip>
#include <ostream>

/// Collects detailed statistics about search behavior for analysis and tuning.
struct SearchStats {

  // === Current Search State ===

  /// Current iteration depth in iterative deepening.
  int currentIterationDepth = 0;

  /// Current nominal search depth.
  int currentSearchDepth = 0;

  /// Extra depth from extensions (selective depth).
  int currentExtraSearchDepth = 0;

  /// Best move found so far at root.
  Move currentBestRootMove = MOVE_NONE;

  /// Evaluation of the current best root move.
  Value currentBestRootMoveValue = VALUE_NONE;

  /// Current principal variation being explored (fixed-size stack, zero heap allocations).
  VariationStack currentVariation{};

  /// Index of root move currently being searched (0-based).
  size_t currentRootMoveIndex = 0;

  /// Root move currently being searched.
  Move currentRootMove = MOVE_NONE;

  // === Node Type Counts ===

  /// Number of PV nodes searched (full window).
  uint64_t pvNodes = 0;

  /// Number of non-PV nodes searched (null window).
  uint64_t nonPvNodes = 0;

  /// Number of main search nodes (depth > 0, excludes qsearch).
  uint64_t searchNodes = 0;

  /// Number of quiescence search nodes.
  uint64_t qsearchNodes = 0;

  // === Terminal Node Counts ===

  /// Number of checkmate positions found.
  uint64_t checkmates = 0;

  /// Number of stalemate positions found.
  uint64_t stalemates = 0;

  /// Number of leaf positions evaluated (quiescence leaves).
  uint64_t leafPositionsEvaluated = 0;

  /// Total evaluation function calls.
  uint64_t evaluations = 0;

  /// Node count for perft (if running perft).
  uint64_t perftNodeCount = 0;

  // === Pruning Statistics ===

  /// Total beta cutoffs (fail-high).
  uint64_t betaCuts = 0;

  /// Beta cutoffs by move index (0-9, index 9 = moves 10+).
  /// Index 0 = first-move cutoffs (indicates good move ordering).
  static constexpr int BETA_CUTS_INDEX_SIZE = 10;
  uint64_t betaCutsByIndex[BETA_CUTS_INDEX_SIZE] = {};

  /// Mate distance pruning cuts.
  uint64_t mdp = 0;

  /// Stand-pat cutoffs in quiescence search.
  uint64_t standpatCuts = 0;

  /// Razoring prunings (near-leaf nodes).
  uint64_t razorings = 0;

  /// Reverse futility pruning cuts.
  uint64_t rfp_cuts = 0;

  /// Null-move pruning cutoffs.
  uint64_t nullMoveCuts = 0;

  /// Futility pruning cuts (moves skipped).
  uint64_t fpPrunings = 0;

  /// Quiescence futility pruning cuts.
  uint64_t qfpPrunings = 0;

  // === Transposition Table Statistics ===

  /// TT probe hits (entry with matching key found).
  uint64_t ttHit = 0;

  /// TT probe misses (no matching entry).
  uint64_t ttMiss = 0;

  /// TT hits that caused a cutoff (value usable).
  uint64_t TtCuts = 0;

  /// TT hits that did not cause a cutoff.
  uint64_t TtNoCuts = 0;

  /// Evaluations retrieved from TT instead of recalculating.
  uint64_t evalFromTT = 0;

  /// TT hits without a stored move.
  uint64_t NoTtMove = 0;

  /// Internal iterative deepening searches performed.
  uint64_t iidSearches = 0;

  /// Moves found via IID.
  uint64_t iidMoves = 0;

  /// TT moves used for move ordering.
  uint64_t TtMoveUsed = 0;

  // === Re-search Statistics ===

  /// PVS re-searches at root (zero-window failed).
  uint64_t rootPvsResearches = 0;

  /// PVS re-searches in non-root nodes.
  uint64_t pvsResearches = 0;

  /// Aspiration window re-searches (fail-high or fail-low).
  uint64_t aspirationResearches = 0;

  /// Number of times best move changed during search.
  uint64_t bestMoveChange = 0;

  /// LMR re-searches (reduced search found better move).
  uint64_t lmrResearches = 0;

  /// LMR reductions applied.
  uint64_t lmrReductions = 0;

  /// LMR history adjustments: count of moves where history reduced LMR (good moves).
  uint64_t lmrHistoryLessReduction = 0;

  /// LMR history: total depth reduction avoided due to positive history (cumulative).
  int64_t lmrHistoryDepthSaved = 0;

  /// LMR cut node reductions: count of moves where cut node status increased reduction.
  uint64_t lmrCutNodeReductions = 0;

  /// Late move pruning cuts.
  uint64_t lmpCuts = 0;

  // === Improving Flag Statistics ===

  /// Nodes where position is improving (static eval > eval 2 plies ago).
  uint64_t improvingTrue = 0;

  /// Nodes where position is not improving.
  uint64_t improvingFalse = 0;

  // === Extension Statistics ===

  /// Check extensions applied.
  uint64_t checkExtension = 0;

  /// Threat extensions applied.
  uint64_t threatExtension = 0;

  /// Singular extension searches performed (verification searches).
  uint64_t singularSearches = 0;

  /// Singular extension candidates filtered by ttBound (not BETA/EXACT).
  uint64_t singularFilteredByBound = 0;

  /// Singular extensions applied (TT move proven singular).
  uint64_t singularExtension = 0;

  /// Null-move verification re-searches that prevented a cutoff.
  uint64_t nullMoveVerifications = 0;

  // === Tablebase Statistics ===

  /// Successful tablebase probes at root position.
  uint64_t tbRootHits = 0;

  /// Successful tablebase WDL probes during search.
  uint64_t tbSearchHits = 0;

  /// Positions where TB probe failed during search (position not in TB).
  uint64_t tbSearchMisses = 0;

  /// Branches cut off due to TB result (beta cutoff or exact draw).
  uint64_t tbSearchCutoffs = 0;

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
    // PV vs non-PV node statistics
    const uint64_t totalNodes = stats.pvNodes + stats.nonPvNodes;
    os << "pvNodes: " << stats.pvNodes;
    if (totalNodes > 0) {
      const double pvPct = 100.0 * static_cast<double>(stats.pvNodes) / static_cast<double>(totalNodes);
      os << " (" << std::fixed << std::setprecision(2) << pvPct << "%)";
    }
     os << " nonPvNodes: " << stats.nonPvNodes
        << " searchNodes: " << stats.searchNodes
        << " qsearchNodes: " << stats.qsearchNodes
        << " checkmates: " << stats.checkmates
       << " stalemates: " << stats.stalemates
       << " perft: " << stats.perftNodeCount
       << " leafPositionsEvaluated: " << stats.leafPositionsEvaluated
       << " evaluations: " << stats.evaluations
       << " betaCuts: " << stats.betaCuts
       << " betaCutsByIdx: [";
    // Output percentages for each index bucket
    if (stats.betaCuts > 0) {
      for (int i = 0; i < BETA_CUTS_INDEX_SIZE; ++i) {
        if (i > 0) os << ", ";
        const double pct = 100.0 * static_cast<double>(stats.betaCutsByIndex[i]) / static_cast<double>(stats.betaCuts);
        os << std::fixed << std::setprecision(1) << pct << "%";
      }
    } else {
      os << "n/a";
    }
    os << "]"
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
       << " lmrCutNode: " << stats.lmrCutNodeReductions
       << " lmrHistLess: " << stats.lmrHistoryLessReduction
       << " lmrHistDepthSaved: " << stats.lmrHistoryDepthSaved
       << " lmrResearches: " << stats.lmrResearches
       << " improvingTrue: " << stats.improvingTrue
       << " improvingFalse: " << stats.improvingFalse
       << " check ext: " << stats.checkExtension
       << " threat ext: " << stats.threatExtension
       << " singular searches: " << stats.singularSearches
       << " singular ext: " << stats.singularExtension
       << " ttHit: " << stats.ttHit
       << " ttMiss: " << stats.ttMiss
       << " TtCuts: " << stats.TtCuts
       << " TtNoCuts: " << stats.TtNoCuts
       << " evalFromTT: " << stats.evalFromTT
       << " TtMoveUsed: " << stats.TtMoveUsed
       << " NoTtMove: " << stats.NoTtMove
       << " tbRootHits: " << stats.tbRootHits
       << " tbSearchHits: " << stats.tbSearchHits
       << " tbSearchMisses: " << stats.tbSearchMisses
       << " tbSearchCutoffs: " << stats.tbSearchCutoffs
       << " IID Searches: " << stats.iidSearches
       << " IID Moves: " << stats.iidMoves;
    return os;
  }
};


#endif//FRANKYCPP_SEARCHSTATS_H
