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
// Stale Data Prevention (Generation Counter):
//   The triangular PV table has a subtle bug potential: when update() copies
//   moves from the child row (ply+1), it may copy stale data from a previous
//   search branch. This happens because clear(ply) only sets table_[ply][ply]
//   to MOVE_NONE for O(1) performance, leaving the continuation cells intact.
//
//   Example scenario causing illegal PV moves:
//   1. Search branch A at ply 3: finds PV [Nxe5, Qh4, ...], updates table_[3]
//   2. Search backtracks, explores branch B at ply 2
//   3. Branch B at ply 3: clear(3) sets table_[3][3]=MOVE_NONE
//   4. Branch B at ply 3: gets beta cutoff -> NO update() called
//   5. Branch B at ply 3: table_[3] still has [..., ..., ..., MOVE_NONE, Qh4, ...]
//   6. Branch B at ply 2: finds best move, calls update(move, 2)
//   7. update() copies from table_[3][3] which is MOVE_NONE -> stops correctly
//   8. BUT: if ply 4 had stale data and ply 3 was updated in branch B,
//      that stale ply 4 data gets copied into the PV!
//
//   Solution: Track when each row was last updated using a generation counter.
//   update() only copies from child row if it was updated in the CURRENT search
//   (same generation). Cost: O(1) per update() - one comparison + one write.
//
// Benefits over std::vector<MoveList>:
//   - Zero heap allocations during search
//   - Contiguous memory for cache efficiency
//   - Simple indexed copy operations
//   - MOVE_NONE sentinel termination
//   - Generation counter prevents intra-search stale data propagation
//
// Memory: 128 × 128 × 4 bytes + 128 bytes + 1 byte ≈ 64.13 KB
//
// Usage:
//   PVTable pv;
//   pv.clearAll();              // At search start (increments generation)
//   pv.clear(ply);              // At node entry
//   pv.update(move, ply);       // When move improves alpha
//   Move best = pv.first();     // Get best move at root
//   MoveList line = pv.extract(); // For UCI output
//
//=============================================================================

#include "types/types.h"

#include <array>
#include <cstring>
#include <stdexcept>

namespace engine {
  using namespace chess;

  /// Triangular PV Table - zero-overhead wrapper for PV storage
  /// All methods inline/constexpr - compiler optimizes to direct array access
  class PVTable {
  public:
    static constexpr int MAX_PLY = DEPTH_MAX + 1; // 128

  private:
    std::array<std::array<Move, MAX_PLY>, MAX_PLY> table_{};

    // Generation counter to detect stale PV data from previous search branches.
    // Each row tracks when it was last updated. update() only copies from
    // child row if it has the current generation, preventing stale data propagation.
    // Using uint8_t (128 bytes) instead of uint16_t for better cache utilization.
    std::array<uint8_t, MAX_PLY> rowGeneration_{}; // Generation when each row was last updated
    uint8_t currentGeneration_{1};                 // Current search generation (0 = never updated)

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
    /// Full memset ensures no stale data anywhere. Generation counter is also
    /// incremented for defense-in-depth against intra-search stale data bugs.
    void clearAll() noexcept {
      // Clear the entire table - this is safe and only happens once per search
      static_assert(std::is_trivially_copyable_v<Move>);
      std::memset(table_.data(), 0, sizeof(table_));

      // Clear row generation tracking and increment generation counter.
      // This provides defense-in-depth against intra-search stale data
      // (see header documentation for the bug scenario).
      // Cost is negligible (128 bytes) since clearAll() only runs once per search.
      std::memset(rowGeneration_.data(), 0, sizeof(rowGeneration_));
      ++currentGeneration_;
    }

    /// Update PV: prepend move and copy child PV from ply+1 (if current generation)
    void update(const Move move, const Depth ply) {
      table_[ply][ply]    = move;
      rowGeneration_[ply] = currentGeneration_; // Mark this row as current

      // Bounds check: if ply+1 >= MAX_PLY, there's no child PV to copy
      if (ply + 1 >= MAX_PLY) {
        return;
      }

      // Lazy check: if child row is empty, no need to check generation or copy.
      // This is common at leaf nodes and avoids the rowGeneration_ cache access.
      // Also handles case where child row is stale (generation mismatch).
      if (table_[ply + 1][ply + 1] == MOVE_NONE
          || rowGeneration_[ply + 1] != currentGeneration_) {
        table_[ply][ply + 1] = MOVE_NONE;
        return;
      }

      // Child row has current generation data - copy it
      // See header documentation for detailed explanation of the stale data bug.
      int i = ply + 1;
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

} // namespace engine

#endif // FRANKYCPP_PVTABLE_H
