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
//   - Pawn structure (isolated, doubled, passed, connected, advancement)
//   - Piece mobility (attack squares available)
//   - Knight outposts (safe squares immune to pawn attacks)
//   - Bad bishop detection (own pawns on bishop's color)
//   - Rook placement (open files, 7th rank, behind passed pawns)
//   - King safety (pawn shield, attacker proximity, pawn storm, open files, safe checks)
//   - Threat evaluation (pawn attacks on pieces, minor attacks on majors, hanging pieces)
//
// Tapered Evaluation:
//   Scores are computed separately for midgame and endgame, then interpolated
//   based on game phase (determined by remaining material).
//
// Pawn Cache:
//   Pawn structure evaluation is cached in a shared PawnTT (passed via setPawnTT).
//   The PawnTT is shared across all threads for memory efficiency.
//   Each thread has its own Evaluator instance for thread-local scratch variables.
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
//   setPawnTT(pawnTT)        - Sets the shared pawn cache (call before evaluation)
//
// Thread Safety:
//   - Evaluator instances are per-thread (scratch variables are thread-local)
//   - PawnTT is shared across threads (thread-safe via atomic key)
//
// Usage:
//   PawnTT sharedPawnTT(4);  // Shared across threads
//   Evaluator eval;
//   eval.setPawnTT(&sharedPawnTT);
//   Value score = eval.evaluate(position);  // Positive = good for side to move
//
//=============================================================================

#include "PawnTT.h"
#include "chesscore/Position.h"
#include "config/EvalConfigData.h"
#include "types/types.h"

namespace config {
  struct SearchConfigData;
}

namespace engine {
  using namespace chess;

  class Evaluator {

    /// Pointer to shared PawnTT (owned by Search, shared across all threads)
    /// Set via setPawnTT() before evaluation. May be nullptr if pawn caching disabled.
    PawnTT* pawnCache = nullptr;

    /// Thread-local scratch variables for evaluation
    Score score{};
    Score tmpScore{};

    /// King safety attack accumulators (indexed by king color under attack).
    /// Reset in evaluate(), incremented in piece evals, consumed in kingEval().
    std::array<int,2> kingAttackCount{};   ///< Number of distinct pieces attacking king zone
    std::array<int,2>  kingAttackWeight{};  ///< Weighted sum (N=2, B=2, R=3, Q=4)

    /// Per-color attack map: union of all squares attacked by a side's pieces.
    /// Reset in evaluate() (king + pawn attacks), accumulated in piece evals
    /// (knight/bishop/rook/queen), consumed in kingEval() for safe check squares.
    std::array<Bitboard, 2> attackedBy{};

    /// Per-piece-type, per-color attack bitboards.
    /// Indexed as attackedByPT[PieceType][Color].
    /// Reset in evaluate(), populated alongside attackedBy[] in the pre-compute block
    /// (pawn/king attacks) and in each piece eval function (knight/bishop/rook/queen).
    /// Used by threatEval() to identify attacks by specific piece types.
    std::array<std::array<Bitboard, 2>, PT_LENGTH> attackedByPT{};

    /// Per-color passed pawn bitboard. Computed once in evaluate() and reused
    /// by rookEval (rook-behind-passer) and kingEval (king proximity to passers).
    std::array<Bitboard, 2> passedPawns{};

    // reference to the Eval Config Data
    const config::EvalConfigData& EvalConfig;

  public:
    Evaluator();

    /// Sets the shared pawn cache. Must be called before evaluate() if pawn caching is enabled.
    /// The PawnTT is owned by Search and shared across all Evaluator instances.
    /// @param pawnTT  Pointer to shared PawnTT (may be nullptr to disable caching)
    void setPawnTT(PawnTT* pawnTT) { pawnCache = pawnTT; }

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
    void knightEval(const Position& p, Score& s, Color us, Color them, Square sq);

    /// Evaluates bishop placement (mobility, diagonals, fianchetto).
    /// @param p    The position to evaluate
    /// @param s    Score struct to update
    /// @param us   Color of the bishop
    /// @param them Opponent color
    /// @param sq   Square of the bishop
    void bishopEval(const Position& p, Score& s, Color us, Color them, Square sq);

    /// Evaluates rook placement (open files, 7th rank, connectivity).
    /// @param p    The position to evaluate
    /// @param s    Score struct to update
    /// @param us   Color of the rook
    /// @param them Opponent color
    /// @param sq   Square of the rook
    void rookEval(const Position& p, Score& s, Color us, Color them, Square sq);

    /// Evaluates queen placement (mobility, king proximity).
    /// @param p    The position to evaluate
    /// @param s    Score struct to update
    /// @param us   Color of the queen
    /// @param them Opponent color
    /// @param sq   Square of the queen
    void queenEval(const Position& p, Score& s, Color us, Color them, Square sq);

    /// Evaluates king safety (pawn shield, attacker proximity, castling).
    /// @param p   The position to evaluate
    /// @param s   Score struct to update
    /// @param us  Color of the king
    void kingEval(const Position& p, Score& s, Color us) const;

    /// Evaluates threats: pieces attacked by lesser-value pieces, hanging pieces.
    /// Must be called AFTER all pieceEval() calls complete (needs full attackedBy[]
    /// and attackedByPT[][] data).
    /// Three tiers: (1) pawn attacks on pieces, (2) minor attacks on majors,
    /// (3) hanging pieces (attacked by us, not defended by them).
    /// @param p   The position to evaluate
    /// @param s   Score struct to update
    /// @param us  Color whose threats to evaluate (bonus for us)
    void threatEval(const Position& p, Score& s, Color us) const;

#ifdef EVAL_ENABLE_PREFETCH
    /// Prefetches pawn cache entry for the given key into CPU cache.
    /// No-op if pawnCache is nullptr.
    void prefetch(const ZobristKey key) const {
      if (pawnCache) {
        pawnCache->prefetch(key);
      }
    }
#endif

    /// Resets the evaluator state for a new game.
    /// Note: PawnTT is managed by Search, not Evaluator.
    /// score and tmpScore don't need clearing - they are reset at the start
    /// of evaluate() and pawnEval() respectively before each use.
    void reset() {
      // Nothing to reset - scratch variables are reset per-call
    }
  };

} // namespace engine

#endif // FRANKYCPP_EVALUATOR_H
