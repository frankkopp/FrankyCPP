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

#ifndef FRANKYCPP_PLYINFO_H
#define FRANKYCPP_PLYINFO_H

//=============================================================================
// PlyInfo.h - Per-Ply Search State
//=============================================================================
//
// PlyInfo groups all per-ply search state into a unified struct.
// This improves cache locality and prepares for future enhancements
// like Lazy SMP and continuation history.
//
// The plyStack array in Search holds PlyInfo for each ply level.
// Each PlyInfo owns its MoveGenerators via unique_ptr (heap-allocated
// due to their large size).
//
// Usage:
//   auto& info = plyStack[ply];
//   auto* myMg = info.excludedMove != MOVE_NONE ? info.mgSingular.get() : info.mg.get();
//   myMg->resetOnDemand();
//
//=============================================================================

#include "chesscore/MoveGenerator.h"
#include "types/move.h"
#include "types/value.h"

#include <memory>

namespace engine {
  using namespace chess;

  /// Per-ply search state - groups all ply-specific data together
  struct PlyInfo {
    // MoveGenerators owned by this PlyInfo (heap-allocated due to large size)
    std::unique_ptr<MoveGenerator> mg;         // Normal search MoveGenerator
    std::unique_ptr<MoveGenerator> mgSingular; // Singular verification MoveGenerator

    // Move tracking
    Move currentMove{MOVE_NONE};  // Move being searched at this ply
    Move excludedMove{MOVE_NONE}; // Excluded move for singular extension

    // Evaluation
    Value staticEval{VALUE_NONE}; // Static evaluation at this ply

    // Search state
    int moveCount{0};    // Number of moves searched at this ply
    bool inCheck{false}; // Is side to move in check?

    // Default constructor - creates MoveGenerators
    PlyInfo()
        : mg(std::make_unique<MoveGenerator>()),
          mgSingular(std::make_unique<MoveGenerator>()) {}

    // Resets all search state fields to defaults (preserves MoveGenerator allocations)
    void resetSearchState() {
      mg->reset();
      mgSingular->reset();
      currentMove  = MOVE_NONE;
      excludedMove = MOVE_NONE;
      staticEval   = VALUE_NONE;
      moveCount    = 0;
      inCheck      = false;
    }

    // Future: Continuation history pointers (Phase 4)
    // PieceToHistory* continuationHistory[6]{};
  };

} // namespace engine

#endif // FRANKYCPP_PLYINFO_H
