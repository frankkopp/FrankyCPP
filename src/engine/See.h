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

#ifndef FRANKYCPP_SEE_H
#define FRANKYCPP_SEE_H

//=============================================================================
// See.h - Static Exchange Evaluation (SEE)
//=============================================================================
//
// SEE evaluates the material outcome of a capture sequence on a single square.
// Used to determine if a capture is likely winning, losing, or equal without
// actually making the moves.
// Depends on: types.h, Position
//
// Algorithm:
//   Uses the "swap algorithm" to simulate alternating captures on a square,
//   always using the least valuable attacker. The result indicates the net
//   material gain/loss if both sides play optimally on that square.
//   Credit: https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
//
// Uses in Search:
//   - Move ordering: captures with SEE >= 0 are searched before SEE < 0
//   - Pruning: losing captures (SEE < 0) can be pruned or reduced
//   - Quiescence search: only search captures with SEE >= threshold
//
// Special Cases:
//   - En passant captures always return +100 (pawn value) as the captured
//     pawn is not on the target square, making SEE calculation complex
//   - Non-capturing moves return Value(0)
//
// Usage:
//   Value score = See::see(position, captureMove);
//   if (score >= VALUE_ZERO) {
//     // Winning or equal capture - search it
//   } else {
//     // Losing capture - consider pruning or reducing
//   }
//
//=============================================================================

#include "chesscore/fwd.h"
#include <types/types.h>

namespace engine::See {
  using namespace chess;

  /// Evaluates the static exchange score for a capture move.
  /// The move should not have been made on the position yet.
  /// Uses the swap algorithm to simulate the capture sequence.
  /// @param p     Position (will be read but not modified)
  /// @param move  Capturing move to evaluate
  /// @return      Net material value of the exchange.
  ///              Positive = winning, negative = losing, zero = equal.
  /// @note        En passant captures always return +100 (pawn value).
  /// @note        Non-capturing moves return Value(0).
  Value see(const Position& p, Move move);

  /// Finds the least valuable piece of the given color in the bitboard.
  /// When multiple pieces of the same type are available, returns the
  /// square of the least significant bit (arbitrary but consistent).
  /// @param p        Position to query piece types
  /// @param bitboard Bitboard of candidate pieces
  /// @param color    Color of pieces to consider
  /// @return         Square of least valuable attacker, or SQ_NONE if empty
  Square getLeastValuablePiece(const Position& p, Bitboard bitboard, Color color);

  /// Returns a bitboard of all pieces of the given color attacking a square.
  /// @param p       Position to analyze
  /// @param square  Target square
  /// @param color   Attacking color
  /// @return        Bitboard with bits set for each attacking piece
  Bitboard attacksTo(const Position& p, Square square, Color color);

  /// Returns new sliding attacks revealed after a piece is removed.
  /// When a piece captures and is removed from the board, it may unblock
  /// a sliding piece (bishop, rook, queen) that can now attack the target.
  /// Only looks at slider pieces as only their attacks can be revealed.
  /// @param p        Position to analyze
  /// @param square   Target square being attacked
  /// @param occupied Updated occupancy bitboard (after piece removal)
  /// @param color    Color of potential new attackers
  /// @return         Bitboard of newly revealed sliding attackers
  Bitboard revealedAttacks(const Position& p, Square square, Bitboard occupied, Color color);

} // namespace engine::See


#endif // FRANKYCPP_SEE_H
