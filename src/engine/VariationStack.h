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

#ifndef FRANKYCPP_VARIATIONSTACK_H
#define FRANKYCPP_VARIATIONSTACK_H

//=============================================================================
// VariationStack.h - Fixed-Size Move Stack for Current Variation
//=============================================================================
//
// VariationStack provides a fixed-size stack for tracking the current search
// variation. Optimized for the hot path where push_back/pop_back are called
// millions of times during search.
// Depends on: types.h, Move
//
// Benefits over MoveList (std::vector<Move>):
//   - Zero heap allocations
//   - No bounds checking overhead
//   - Cache-friendly contiguous storage
//   - Predictable memory layout
//
// Memory: 128 × 4 bytes = 512 bytes
//
// Usage:
//   VariationStack var;
//   var.push_back(move);   // Add move to stack
//   var.pop_back();        // Remove last move
//   var.clear();           // Reset stack
//   for (const auto& m : var) { ... }  // Iterate
//
//=============================================================================

#include "types/types.h"

#include <array>
#include <sstream>
#include <string>

/// Fixed-size stack for tracking current search variation
/// Zero heap allocation, cache-friendly, called millions of times in hot path
class VariationStack {
public:
  static constexpr int MAX_PLY = DEPTH_MAX + 1;// 128

private:
  std::array<Move, MAX_PLY> moves_{};
  int size_{0};

public:
  // =========================================================================
  // Stack Operations - hot path, must be fast
  // =========================================================================

  constexpr void push_back(const Move m) noexcept {
    moves_[size_++] = m;
  }

  constexpr void pop_back() noexcept {
    --size_;
  }

  constexpr void clear() noexcept {
    size_ = 0;
  }

  // =========================================================================
  // Query Operations
  // =========================================================================

  [[nodiscard]] constexpr int size() const noexcept {
    return size_;
  }

  [[nodiscard]] constexpr bool empty() const noexcept {
    return size_ == 0;
  }

  [[nodiscard]] constexpr Move& operator[](const int i) noexcept {
    return moves_[i];
  }

  [[nodiscard]] constexpr const Move& operator[](const int i) const noexcept {
    return moves_[i];
  }

  [[nodiscard]] constexpr Move& back() noexcept {
    return moves_[size_ - 1];
  }

  [[nodiscard]] constexpr const Move& back() const noexcept {
    return moves_[size_ - 1];
  }

  // =========================================================================
  // Iterator Support - for range-based for loops
  // =========================================================================

  [[nodiscard]] constexpr Move* begin() noexcept {
    return moves_.data();
  }

  [[nodiscard]] constexpr Move* end() noexcept {
    return moves_.data() + size_;
  }

  [[nodiscard]] constexpr const Move* begin() const noexcept {
    return moves_.data();
  }

  [[nodiscard]] constexpr const Move* end() const noexcept {
    return moves_.data() + size_;
  }

  // =========================================================================
  // String Output - only called periodically for UCI, not hot path
  // =========================================================================

  [[nodiscard]] std::string str() const {
    std::ostringstream os;
    for (int i = 0; i < size_; ++i) {
      if (i > 0) os << ' ';
      os << moves_[i].str();
    }
    return os.str();
  }
};

#endif// FRANKYCPP_VARIATIONSTACK_H
