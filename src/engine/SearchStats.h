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

#ifndef FRANKYCPP_SEARCHSTATS_H
#define FRANKYCPP_SEARCHSTATS_H

#include "types/types.h"
#include <array>
#include <ostream>

// data structure to cluster all search statistics
struct SearchStats {

  int currentIterationDepth;
  int currentSearchDepth;
  int currentExtraSearchDepth;

  Move currentBestRootMove;
  Value currentBestRootMoveValue;

  MoveList currentVariation{};
  size_t currentRootMoveIndex;
  Move currentRootMove;

  uint64_t checkmates;
  uint64_t stalemates;
  uint64_t leafPositionsEvaluated;
  uint64_t evaluations;
  uint64_t perftNodeCount;

  uint64_t betaCuts;
  uint64_t betaCuts1st;
  uint64_t mdp;
  uint64_t standpatCuts;
  uint64_t razorings;
  uint64_t rfp_cuts;
  uint64_t nullMoveCuts;
  uint64_t fpPrunings;
  uint64_t qfpPrunings;

  uint64_t ttHit;
  uint64_t ttMiss;
  uint64_t TtCuts;
  uint64_t TtNoCuts;
  uint64_t evalFromTT;
  uint64_t NoTtMove;
  uint64_t iidSearches;
  uint64_t iidMoves;
  uint64_t TtMoveUsed;

  uint64_t rootPvsResearches;
  uint64_t pvsResearches;
  uint64_t aspirationResearches;
  uint64_t bestMoveChange;
  uint64_t lmrResearches;
  uint64_t lmrReductions;
  uint64_t lmpCuts;

  uint64_t checkExtension;
  uint64_t threatExtension;

  [[nodiscard]] std::string str() const {
    std::ostringstream os;
    os << *this;
    return os.str();
  };

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
