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

#ifndef FRANKYCPP_HANDICAP_H
#define FRANKYCPP_HANDICAP_H

//=============================================================================
// Handicap.h - Strength Limitation System
//=============================================================================
//
// Provides a simple handicap system where higher levels produce weaker play.
// Uses the MultiPV infrastructure to build a candidate move pool, then picks
// a suboptimal move via score-weighted probabilistic selection.
//
// Handicap levels:
//   0        = full strength (no-op, zero overhead)
//   1-2      = gentle weakening via time waste only (~50-100 ELO at fast TC)
//   3-9      = MultiPV overhead + suboptimal move selection (~200-300 ELO)
//   10-15    = depth cap + larger pool + wider threshold (~300-700 ELO)
//   16-20    = severe limits, near-total dominance by full strength
//
// Each level maps to five parameters:
//   - depthCap:       maximum search depth (limits strategic depth)
//   - multiPV:        forced MultiPV search count (spreads search effort)
//   - poolSize:       number of candidate moves to consider
//   - scoreThreshold: max centipawns below best move to consider (wider = weaker)
//   - timeFraction:   percentage of normal time budget (100 = full, <100 = reduced)
//
// Move selection uses Zobrist-key-seeded PRNG for deterministic, reproducible
// play from the same position. Moves within the score threshold are weighted
// proportionally (closer to best = higher weight), while moves outside the
// threshold are excluded entirely (hard cutoff). This produces natural-feeling
// weaker play at high levels and near-perfect play at low levels.
//
// Key Functions:
//   getHandicapParams(level) - Returns parameters for given handicap level
//   selectHandicapMove(...)  - Picks a move from the candidate pool
//
// Usage:
//   const auto params = handicap::getHandicapParams(5);
//   Move selected = handicap::selectHandicapMove(rootMoves, params.poolSize,
//                                                 params.scoreThreshold, zobristKey);
//
//=============================================================================

#include "types/types.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace engine::handicap {
  using namespace chess;

  /// Parameters for a given handicap level.
  struct HandicapParams {
    int depthCap;       ///< Maximum search depth (DEPTH_MAX for unlimited)
    int multiPV;        ///< Forced MultiPV search count (1 = no inflation)
    int poolSize;       ///< Number of candidate moves to consider for suboptimal selection
    int scoreThreshold; ///< Max centipawns below best move to include in selection
    int timeFraction;   ///< Time budget percentage (100 = full time, <100 = reduced thinking time)
  };

  /// Maximum handicap level supported.
  static constexpr int MAX_HANDICAP = 20;

  // clang-format off
  /// Lookup table mapping handicap level (0–20) to parameters.
  /// Level 0 = full strength (no-op).
  /// Higher levels = weaker play via shallower search, larger pool, wider threshold.
  static constexpr std::array<HandicapParams, MAX_HANDICAP + 1> HANDICAP_TABLE {{
    //  {depthCap, multiPV, poolSize, scoreThreshold, timeFraction}
    {  DEPTH_MAX,  1,  1,    0,  100  },   //  0: full strength
    {  DEPTH_MAX,  1,  1,    0,   90  },   //  1: time reduction only (90% time budget)
    {  DEPTH_MAX,  1,  1,    0,   80  },   //  2: time reduction only (80% time budget)
    {  DEPTH_MAX,  2,  2,    2,  100  },   //  3: MultiPV=2 kicks in
    {  DEPTH_MAX,  2,  2,    4,  100  },   //  4:
    {  DEPTH_MAX,  2,  2,    6,  100  },   //  5:
    {  DEPTH_MAX,  2,  2,    9,  100  },   //  6:
    {  DEPTH_MAX,  2,  2,   12,  100  },   //  7:
    {  DEPTH_MAX,  3,  3,   16,  100  },   //  8:
    {  DEPTH_MAX,  3,  3,   22,  100  },   //  9:
    {         24,  3,  3,   28,  100  },   // 10: depth cap starts
    {         22,  4,  4,   35,  100  },   // 11:
    {         20,  4,  4,   45,  100  },   // 12:
    {         18,  5,  5,   60,  100  },   // 13:
    {         16,  5,  5,   80,  100  },   // 14:
    {         14,  6,  6,  100,  100  },   // 15:
    {         12,  7,  7,  130,  100  },   // 16:
    {         10,  8,  8,  170,  100  },   // 17:
    {          9,  9,  9,  220,  100  },   // 18:
    {          8, 10, 10,  280,  100  },   // 19:
    {          7, 12, 12,  360,  100  },   // 20: weakest
  }};
  // clang-format on

  /// Returns handicap parameters for the given level.
  /// Level is clamped to [0, MAX_HANDICAP].
  /// @param level  Handicap level (0 = full strength, 20 = weakest)
  /// @return       HandicapParams with depthCap, multiPV, poolSize, scoreThreshold
  [[nodiscard]] constexpr HandicapParams getHandicapParams(const int level) noexcept {
    return HANDICAP_TABLE[static_cast<size_t>(std::clamp(level, 0, MAX_HANDICAP))];
  }

  /// Splitmix64 one-shot: produces a well-distributed 64-bit value from a seed.
  /// Used to convert Zobrist keys into uniform random values for move selection.
  /// @param seed  Input seed (typically position Zobrist key)
  /// @return      Pseudo-random 64-bit value
  [[nodiscard]] constexpr uint64_t splitmix64(uint64_t seed) noexcept {
    seed += 0x9e3779b97f4a7c15ULL;
    seed = (seed ^ seed >> 30) * 0xbf58476d1ce4e5b9ULL;
    seed = (seed ^ seed >> 27) * 0x94d049bb133111ebULL;
    return seed ^ seed >> 31;
  }

  /// Selects a handicap move from the candidate pool using score-weighted random selection.
  ///
  /// Moves within the score threshold are weighted proportionally:
  ///   weight(i) = max(0, threshold - (bestScore - moveScore(i)))
  /// Moves outside the threshold (gap > threshold) get weight 0 and are excluded.
  /// This makes the threshold a hard cutoff — only near-equal moves are considered
  /// at low handicap levels, producing subtle inaccuracies rather than blunders.
  ///
  /// If all candidates outside index 0 have weight 0 (gap > threshold for all),
  /// the best move is returned (no deviation).
  ///
  /// The PRNG is seeded with the position's Zobrist key, making the selection
  /// deterministic for the same position (reproducible in testing, no flickering).
  ///
  /// @param rootMoves       Sorted root moves (best first, with value() attached)
  /// @param poolSize        Number of candidates to consider (clamped to rootMoves.size())
  /// @param scoreThreshold  Max centipawns below best to include (hard cutoff)
  /// @param zobristKey      Position hash for deterministic PRNG seeding
  /// @return                Selected move (always valid if rootMoves is non-empty)
  [[nodiscard]] inline Move selectHandicapMove(
    const MoveList& rootMoves,
    const int poolSize,
    const int scoreThreshold,
    const uint64_t zobristKey) {

    // Safety: if no moves, return MOVE_NONE
    if (rootMoves.empty()) {
      return MOVE_NONE;
    }

    // Clamp pool to available moves
    const int effectivePool = std::min(poolSize, static_cast<int>(rootMoves.size()));

    // Single candidate — no selection needed
    if (effectivePool == 1) {
      return rootMoves[0];
    }

    // Best score is rootMoves[0] (list is sorted descending by value)
    const int bestScore = rootMoves[0].value();

    // Compute weights for each candidate.
    // weight = max(0, threshold - gap): hard cutoff at threshold.
    // Moves within threshold get proportionally higher weights;
    // moves outside threshold get weight 0 and are excluded entirely.
    int totalWeight = 0;
    std::array<int, 256> weights{};

    for (int i = 0; i < effectivePool; ++i) {
      const int moveScore = rootMoves[i].value();
      const int gap       = bestScore - moveScore;
      const int weight    = std::max(0, scoreThreshold - gap);
      weights[i]          = weight;
      totalWeight += weight;
    }

    // If no candidate has positive weight (all outside threshold), play best move
    if (totalWeight == 0) {
      return rootMoves[0];
    }

    // Weighted random selection using splitmix64
    const uint64_t rng = splitmix64(zobristKey);
    int target         = static_cast<int>(rng % static_cast<uint64_t>(totalWeight));

    for (int i = 0; i < effectivePool; ++i) {
      if (weights[i] == 0) {
        continue; // skip excluded candidates
      }
      target -= weights[i];
      if (target < 0) {
        return rootMoves[i];
      }
    }

    // Fallback (should not happen due to integer arithmetic)
    return rootMoves[0];
  }

} // namespace engine::handicap

#endif // FRANKYCPP_HANDICAP_H
