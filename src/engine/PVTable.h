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

#ifndef FRANKYCPP_PVTABLE_H
#define FRANKYCPP_PVTABLE_H

//=============================================================================
// PVTable.h - Triangular Principal Variation Table
//=============================================================================
//
// PVTable implements a triangular PV table for efficient storage and retrieval
// of the principal variation during alpha-beta search.
// Depends on: types.h, Move, MoveList
//
// Memory Layout (triangular usage):
//   pv[0][0..n] = full PV from root
//   pv[1][1..n] = PV continuation from ply 1
//   pv[2][2..n] = PV continuation from ply 2
//   ...
//
// Benefits over std::vector<MoveList>:
//   - Zero heap allocations during search
//   - Contiguous memory for cache efficiency
//   - Simple indexed copy operations
//   - MOVE_NONE sentinel termination
//
// Memory: 128 × 128 × 4 bytes = 64 KB
//
// Usage:
//   PVTable pv;
//   pv.clearAll();              // At search start
//   pv.clear(ply);              // At node entry
//   pv.update(move, ply);       // When move improves alpha
//   Move best = pv.first();     // Get best move at root
//   MoveList line = pv.extract(); // For UCI output
//
//=============================================================================

#include "types/types.h"

#include <array>
#include <cstring>

/// Triangular PV Table - zero-overhead wrapper for PV storage
/// All methods inline/constexpr - compiler optimizes to direct array access
class PVTable {
public:
  static constexpr int MAX_PLY = DEPTH_MAX + 1;// 128

private:
  std::array<std::array<Move, MAX_PLY>, MAX_PLY> table_{};

public:
  // =========================================================================
  // Direct Access - zero overhead (compiles to raw array access)
  // =========================================================================

  /// Direct access to move at [ply][index] - zero overhead
  [[nodiscard]] constexpr Move& operator()(const Depth ply, const int index) noexcept {
    return table_[ply][index];
  }
  [[nodiscard]] constexpr const Move& operator()(const Depth ply, const int index) const noexcept {
    return table_[ply][index];
  }

  // =========================================================================
  // Semantic Operations
  // =========================================================================

  /// Clear PV at given ply (O(1) - single assignment)
  constexpr void clear(const Depth ply) noexcept {
    table_[ply][ply] = MOVE_NONE;
  }

  /// Clear entire table (called once at search start)
  void clearAll() noexcept {
    // std::array is contiguous, so memset works safely
    // MOVE_NONE has raw value 0, so zeroing the memory is correct
    std::memset(table_.data(), 0, sizeof(table_));
  }

  /// Update PV: prepend move and copy child PV from ply+1
  constexpr void update(const Move move, const Depth ply) noexcept {
    table_[ply][ply] = move;
    int i            = ply + 1;
    while (table_[ply + 1][i] != MOVE_NONE && i < MAX_PLY) {
      table_[ply][i] = table_[ply + 1][i];
      ++i;
    }
    if (i < MAX_PLY) {
      table_[ply][i] = MOVE_NONE;
    }
  }

  // =========================================================================
  // Query Operations
  // =========================================================================

  /// Get first move at ply (equivalent to pv[ply][ply])
  [[nodiscard]] constexpr Move first(const Depth ply = DEPTH_NONE) const noexcept {
    return table_[ply][ply];
  }

  /// Check if PV at ply is empty
  [[nodiscard]] constexpr bool empty(const Depth ply = DEPTH_NONE) const noexcept {
    return table_[ply][ply] == MOVE_NONE;
  }

  /// Check if PV has at least minLength moves starting from ply
  [[nodiscard]] constexpr bool hasLength(const Depth ply, const int minLength) const noexcept {
    for (int i = 0; i < minLength; ++i) {
      if (table_[ply][ply + i] == MOVE_NONE) return false;
    }
    return true;
  }

  /// Get PV length at ply
  [[nodiscard]] constexpr int length(const Depth ply = DEPTH_NONE) const noexcept {
    int len = 0;
    while (table_[ply][ply + len] != MOVE_NONE && ply + len < MAX_PLY) {
      ++len;
    }
    return len;
  }

  // =========================================================================
  // Extraction (for UCI output - only called at iteration end)
  // =========================================================================

  /// Extract PV as MoveList starting from given ply
  [[nodiscard]] MoveList extract(const Depth ply = DEPTH_NONE) const {
    MoveList result;
    for (int i = ply; table_[ply][i] != MOVE_NONE && i < MAX_PLY; ++i) {
      result.push_back(table_[ply][i]);
    }
    return result;
  }

  // =========================================================================
  // Low-level Access (for special cases)
  // =========================================================================

  /// Direct row access (returns pointer to first element of row)
  [[nodiscard]] constexpr Move* row(const Depth ply) noexcept {
    return table_[ply].data();
  }
  [[nodiscard]] constexpr const Move* row(const Depth ply) const noexcept {
    return table_[ply].data();
  }
};

#endif// FRANKYCPP_PVTABLE_H
