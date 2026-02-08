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

#ifndef FRANKYCPP_STATICMOVELIST_H
#define FRANKYCPP_STATICMOVELIST_H

//=============================================================================
// staticmovelist.h - Fixed-Capacity Move Container Template
//=============================================================================
//
// StaticMoveList<N> is a fixed-capacity container for chess moves, replacing
// both the old std::vector-based MoveList and VariationStack with a unified
// zero-allocation implementation.
// Depends on: move.h
//
// Benefits:
//   - Zero heap allocations (critical for hot path performance)
//   - Cache-friendly contiguous storage
//   - Predictable memory layout
//   - STL-compatible interface (iterators, range-based for)
//   - Supports std::ranges algorithms (sort, for_each, etc.)
//
// Template Parameter:
//   Capacity - Maximum number of moves the container can hold
//
// Type Aliases:
//   MoveList       = StaticMoveList<256>  - For move generation (max legal ~218)
//   VariationStack = StaticMoveList<128>  - For search variation (MAX_PLY)
//
// Memory:
//   MoveList:       256 × 4 bytes = 1024 bytes (1 KB)
//   VariationStack: 128 × 4 bytes =  512 bytes
//
// Usage:
//   MoveList moves;
//   moves.push_back(Move::normal(SQ_E2, SQ_E4));
//   moves.push_back(Move::normal(SQ_D2, SQ_D4));
//
//   std::string uci = moves.str();  // "e2e4 d2d4"
//   for (const Move& m : moves) { ... }  // STL iteration
//
//   std::ranges::stable_sort(moves, comparator);  // STL algorithms work
//
//=============================================================================

#include "move.h"

#include <array>
#include <cassert>
#include <ostream>
#include <sstream>
#include <stdexcept>

/// Fixed-capacity move container - zero heap allocation, STL-compatible
/// @tparam Capacity Maximum number of moves the container can hold
template<size_t Capacity>
class StaticMoveList {
public:
  static constexpr size_t MAX_SIZE = Capacity;

private:
  std::array<Move, Capacity> data_{};
  size_t size_{0};

public:
  // =========================================================================
  // Constructors
  // =========================================================================

  constexpr StaticMoveList() noexcept = default;

  // =========================================================================
  // Stack Operations - hot path, must be fast
  // =========================================================================

  constexpr void push_back(const Move m) noexcept {
    assert(size_ < Capacity && "StaticMoveList capacity exceeded");
    data_[size_++] = m;
  }

  constexpr void pop_back() noexcept {
    assert(size_ > 0 && "pop_back on empty StaticMoveList");
    --size_;
  }

  constexpr void clear() noexcept {
    size_ = 0;
  }

  // =========================================================================
  // Size & Capacity
  // =========================================================================

  [[nodiscard]] constexpr size_t size() const noexcept {
    return size_;
  }

  [[nodiscard]] constexpr bool empty() const noexcept {
    return size_ == 0;
  }

  [[nodiscard]] static constexpr size_t capacity() noexcept {
    return Capacity;
  }

  [[nodiscard]] static constexpr size_t max_size() noexcept {
    return Capacity;
  }

  // =========================================================================
  // Element Access - unchecked (fast)
  // =========================================================================

  [[nodiscard]] constexpr Move& operator[](const size_t i) noexcept {
    assert(i < size_ && "StaticMoveList index out of bounds");
    return data_[i];
  }

  [[nodiscard]] constexpr const Move& operator[](const size_t i) const noexcept {
    assert(i < size_ && "StaticMoveList index out of bounds");
    return data_[i];
  }

  [[nodiscard]] constexpr Move& back() noexcept {
    assert(size_ > 0 && "back() on empty StaticMoveList");
    return data_[size_ - 1];
  }

  [[nodiscard]] constexpr const Move& back() const noexcept {
    assert(size_ > 0 && "back() on empty StaticMoveList");
    return data_[size_ - 1];
  }

  [[nodiscard]] constexpr Move& front() noexcept {
    assert(size_ > 0 && "front() on empty StaticMoveList");
    return data_[0];
  }

  [[nodiscard]] constexpr const Move& front() const noexcept {
    assert(size_ > 0 && "front() on empty StaticMoveList");
    return data_[0];
  }

  // =========================================================================
  // Element Access - bounds checked (throws std::out_of_range)
  // =========================================================================

  [[nodiscard]] constexpr Move& at(const size_t i) {
    if (i >= size_) {
      throw std::out_of_range("StaticMoveList::at() index out of range");
    }
    return data_[i];
  }

  [[nodiscard]] constexpr const Move& at(const size_t i) const {
    if (i >= size_) {
      throw std::out_of_range("StaticMoveList::at() index out of range");
    }
    return data_[i];
  }

  // =========================================================================
  // Data Access
  // =========================================================================

  [[nodiscard]] constexpr Move* data() noexcept {
    return data_.data();
  }

  [[nodiscard]] constexpr const Move* data() const noexcept {
    return data_.data();
  }

  // =========================================================================
  // Iterator Support - for range-based for loops and STL algorithms
  // =========================================================================

  [[nodiscard]] constexpr Move* begin() noexcept {
    return data_.data();
  }

  [[nodiscard]] constexpr Move* end() noexcept {
    return data_.data() + size_;
  }

  [[nodiscard]] constexpr const Move* begin() const noexcept {
    return data_.data();
  }

  [[nodiscard]] constexpr const Move* end() const noexcept {
    return data_.data() + size_;
  }

  [[nodiscard]] constexpr const Move* cbegin() const noexcept {
    return data_.data();
  }

  [[nodiscard]] constexpr const Move* cend() const noexcept {
    return data_.data() + size_;
  }

  // =========================================================================
  // String Output - not hot path, called for UCI output
  // =========================================================================

  /// Returns a UCI-compatible space-separated move string
  [[nodiscard]] std::string str() const {
    std::ostringstream os;
    for (size_t i = 0; i < size_; ++i) {
      if (i > 0) os << ' ';
      os << data_[i].str();
    }
    return os.str();
  }

  /// Returns a verbose string representation with size info
  [[nodiscard]] std::string strVerbose() const {
    std::ostringstream os;
    os << "MoveList: size=" << size_ << " [";
    for (size_t i = 0; i < size_; ++i) {
      os << data_[i];
      if (i + 1 < size_) os << ", ";
    }
    os << "]";
    return os.str();
  }

  // =========================================================================
  // Stream Output
  // =========================================================================

  friend std::ostream& operator<<(std::ostream& os, const StaticMoveList& ml) {
    os << ml.str();
    return os;
  }
};

//=============================================================================
// Type Aliases
//=============================================================================

/// MoveList for move generation - capacity 256 covers all legal move scenarios
/// (theoretical maximum is ~218 legal moves in any position)
using MoveList = StaticMoveList<256>;

/// VariationStack for tracking current search variation - capacity 128 (MAX_PLY)
using VariationStack = StaticMoveList<128>;

#endif// FRANKYCPP_STATICMOVELIST_H
