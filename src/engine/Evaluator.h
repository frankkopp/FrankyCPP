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

#ifndef FRANKYCPP_EVALUATOR_H
#define FRANKYCPP_EVALUATOR_H

//=============================================================================
// Evaluator.h - Position Evaluation Engine
//=============================================================================
//
// Evaluator calculates a heuristic score for a chess position using classical
// evaluation techniques. The score represents the advantage for the side to move.
// Depends on: PawnTT.h, Position.h, EvalConfigData.h, types.h
//
// Evaluation Components:
//   - Material balance (piece values)
//   - Piece-square tables (positional bonuses, midgame + endgame)
//   - Pawn structure (isolated, doubled, passed, connected)
//   - Piece mobility (attack squares available)
//   - King safety (pawn shield, attacker proximity)
//
// Tapered Evaluation:
//   Scores are computed separately for midgame and endgame, then interpolated
//   based on game phase (determined by remaining material).
//
// Pawn Cache:
//   Pawn structure evaluation is cached in a dedicated hash table (PawnTT)
//   since pawn structure changes infrequently.
//
// Configuration:
//   All evaluation parameters are configurable via EvalConfigData (YAML).
//   Use CONFIG_OVERRIDE_START/END macros for testing overrides.
//
// Key Methods:
//   evaluate(position)       - Returns score from side-to-move perspective
//   pawnEval(position, s)    - Evaluates pawn structure into score
//   valueFromScore(s, phase) - Interpolates midgame/endgame scores
//   finalEval(position, v)   - Converts to side-to-move perspective
//
// Usage:
//   Evaluator eval;
//   Value score = eval.evaluate(position);  // Positive = good for side to move
//
//=============================================================================

#include "PawnTT.h"
#include "chesscore/Position.h"
#include "config/EvalConfigData.h"
#include "types/types.h"

struct SearchConfigData;

class Evaluator {

  PawnTT pawnCache{0};

  Score score{};
  Score tmpScore{};

  // reference to the Eval Config Data
  const EvalConfigData& EvalConfig;

public:
  Evaluator();

  /// Evaluates the position and returns a score from the side-to-move perspective.
  /// Combines material, positional, pawn structure, mobility, and king safety.
  /// @param p  The position to evaluate
  /// @return   Positive value = advantage for side to move
  Value evaluate(const Position& p);

  /// Evaluates pawn structure (isolated, doubled, passed, connected pawns).
  /// Results are cached in PawnTT for efficiency.
  /// @param p  The position to evaluate
  /// @param s  Score struct to update (midgame + endgame components)
  void pawnEval(const Position& p, Score& s);

  /// Interpolates midgame and endgame scores based on game phase.
  /// @param score            Score with midgame and endgame components
  /// @param gamePhaseFactor  0.0 (endgame) to 1.0 (midgame)
  /// @return                 Interpolated value
  static Value valueFromScore(const Score& score, double gamePhaseFactor);

  /// Converts value from white's perspective to side-to-move perspective.
  /// @param p      The position (to determine side to move)
  /// @param value  Value from white's perspective
  /// @return       Value from side-to-move perspective
  static Value finalEval(const Position& p, Value value);

  /// Evaluates all pieces of a given type for the specified color.
  /// @param p          The position to evaluate
  /// @param s          Score struct to update
  /// @param us         Color of pieces to evaluate
  /// @param pieceType  Type of piece to evaluate
  void pieceEval(const Position& p, Score& s, Color us, PieceType pieceType);

  /// Evaluates knight placement (mobility, outposts, centralization).
  /// @param p    The position to evaluate
  /// @param s    Score struct to update
  /// @param us   Color of the knight
  /// @param them Opponent color
  /// @param sq   Square of the knight
  void knightEval(const Position& p, Score& s, Color us, Color them, Square sq) const;

  /// Evaluates bishop placement (mobility, diagonals, fianchetto).
  /// @param p    The position to evaluate
  /// @param s    Score struct to update
  /// @param us   Color of the bishop
  /// @param them Opponent color
  /// @param sq   Square of the bishop
  void bishopEval(const Position& p, Score& s, Color us, Color them, Square sq) const;

  /// Evaluates rook placement (open files, 7th rank, connectivity).
  /// @param p    The position to evaluate
  /// @param s    Score struct to update
  /// @param us   Color of the rook
  /// @param them Opponent color
  /// @param sq   Square of the rook
  void rookEval(const Position& p, Score& s, Color us, Color them, Square sq) const;

  /// Evaluates queen placement (mobility, king proximity).
  /// @param p    The position to evaluate
  /// @param s    Score struct to update
  /// @param us   Color of the queen
  /// @param them Opponent color
  /// @param sq   Square of the queen
  void queenEval(const Position& p, Score& s, Color us, Color them, Square sq) const;

  /// Evaluates king safety (pawn shield, attacker proximity, castling).
  /// @param p   The position to evaluate
  /// @param s   Score struct to update
  /// @param us  Color of the king
  void kingEval(const Position& p, Score& s, Color us) const;

#ifdef EVAL_ENABLE_PREFETCH
  /// Prefetches pawn cache entry for the given key into CPU cache.
  void prefetch(const ZobristKey key) {
    pawnCache.prefetch(key);
  }
#endif

  /// Resizes pawn TT when EvalConfig changes. Mainly for unit tests.
  void onEvalConfigChanged() {
    if (EvalConfig.USE_PAWN_TT && EvalConfig.PAWN_TT_SIZE_MB > 0) {
      pawnCache.resize(static_cast<uint64_t>(EvalConfig.PAWN_TT_SIZE_MB));
    }
    else {
      // Keep the TT in the "disabled" state (mask==0, dummy slot allocated)
      pawnCache.resize(0);
    }
  }
};

#endif// FRANKYCPP_EVALUATOR_H
