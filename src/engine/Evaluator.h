/*
 * MIT License
 *
 * Copyright (c) 2020 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef FRANKYCPP_EVALUATOR_H
#define FRANKYCPP_EVALUATOR_H

#include "PawnTT.h"
#include "chesscore/Position.h"
#include "types/types.h"

// Evaluator calculates a value for a chess positions by
// using various evaluation heuristics like material,
// positional values, pawn structure, etc.
class Evaluator {

  PawnTT pawnCache{0};

  Score score{};
  Score tmpScore{};

public:
  Evaluator();

  // Evaluate calculates a value for a chess positions by
  // using various evaluation heuristics like material,
  // positional values, pawn structure, etc.
  // It calls InitEval and then the internal evaluation function
  // which calculates the value for the position of the given
  // position for the current game phase and from the
  // view of the next player.
  Value evaluate(Position& p);

  // evaluates pawns and updating score in place
  void pawnEval(Position& p, Score& s);

  // ValueFromScore adds up the mid and end games scores after multiplying
  // them with the game phase factor
  static Value valueFromScore(const Score& score, double gamePhaseFactor);

  // convert value from white view to next player view
  static Value finalEval(const Position& p, Value value);

  void pieceEval(const Position& p, Score& s, Color us, PieceType pieceType);
  void knightEval(const Position& p, Score& s, Color us, Color them, Square sq);
  void bishopEval(const Position& p, Score& s, Color us, Color them, Square sq);
  void rookEval(const Position& p, Score& s, Color us, Color them, Square sq);
  void queenEval(const Position& p, Score& s, Color us, Color them, Square sq);
  void kingEval(const Position& p, Score& s, Color us);

  // do a prefetch for the pawn cache data
#ifdef EVAL_ENABLE_PREFETCH
  inline void prefetch(Key key) {
    pawnCache.prefetch(key);
  }
#endif
};

#endif//FRANKYCPP_EVALUATOR_H
