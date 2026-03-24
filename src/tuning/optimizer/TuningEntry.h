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

#ifndef FRANKYCPP_TUNINGENTRY_H
#define FRANKYCPP_TUNINGENTRY_H

//=============================================================================
// TuningEntry.h - Single Labeled Position for Texel Tuning
//=============================================================================
//
// Represents one position in the tuning dataset: a FEN string paired with
// the game result (from White's perspective) and activation flags indicating
// which parameter groups affect this position's evaluation.
//
// Why FEN string instead of Position object:
//   Position contains a historyState[1024] array making it ~33 KB each.
//   For 5M entries that would require ~160 GB. Storing the FEN string (~80
//   bytes) keeps 5M entries at ~400 MB. The TexelTuner reconstructs a
//   stack-local Position per thread during MSE computation.
//
// The result is always from White's perspective:
//   1.0 = White win, 0.5 = Draw, 0.0 = Black win
//
// The activeParamGroups bitset is populated by
// TexelTuner::computeActivationFlags() based on board state analysis.
// Until computed, all bits are set (all groups assumed active).
//
// The cachedSquaredError field is used by incremental MSE optimization:
// a full eval pass populates it, and subsequent per-parameter trials
// only re-evaluate entries whose param group is active, using cached
// errors for inactive entries to avoid redundant evaluation.
//
// Memory: ~88 bytes per entry (FEN string dominates). 5M entries ≈ 440 MB.
//
//=============================================================================

#include <bitset>
#include <string>
#include <utility>

namespace tuning {

  /// Maximum number of parameter groups for activation-flag optimization.
  /// Each group maps to a category of eval terms (pawn structure, knight, etc.).
  /// 16 groups covers all current eval categories with room for expansion.
  static constexpr int NUM_PARAM_GROUPS = 16;

  /// A single labeled position for Texel tuning.
  struct TuningEntry {
    std::string fen;                                   ///< FEN string (Position reconstructed on demand)
    float result = 0.0F;                               ///< Game result from White's perspective (1.0/0.5/0.0)
    std::bitset<NUM_PARAM_GROUPS> activeParamGroups;   ///< Which param groups affect this position's eval
    double cachedSquaredError = 0.0;                   ///< Cached (result - sigmoid(K, eval))² for incremental MSE

    /// Default constructor — creates an empty entry with all param groups active.
    TuningEntry() {
      activeParamGroups.set(); // all bits set = all groups assumed active
    }

    /// Construct from FEN string and result.
    /// @param fenStr  FEN string describing the position
    /// @param res     Game result from White's perspective (1.0, 0.5, or 0.0)
    TuningEntry(std::string fenStr, const float res)
      : fen(std::move(fenStr)), result(res) {
      activeParamGroups.set(); // all bits set until activation flags are computed
    }
  };

} // namespace tuning

#endif // FRANKYCPP_TUNINGENTRY_H
