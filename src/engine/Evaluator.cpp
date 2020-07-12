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

#include "Evaluator.h"
#include "EvalConfig.h"

Evaluator::Evaluator() {}

Value Evaluator::evaluate(Position& p) {

  // if not enough material on the board to achieve a mate it is a draw
  if (p.checkInsufficientMaterial()) {
    return VALUE_DRAW;
  }

  Score score{};

  // material
  if (EvalConfig::USE_MATERIAL) {
    score.midgame = p.getMaterial(WHITE) - p.getMaterial(BLACK);
    score.endgame = score.midgame;
  }

  // positional value
  if (EvalConfig::USE_POSITIONAL) {
    score.midgame += p.getMidPosValue(WHITE) - p.getMidPosValue(BLACK);
    score.endgame += p.getEndPosValue(WHITE) - p.getEndPosValue(BLACK);
  }

  // TEMPO Bonus for the side to move (helps with evaluation alternation -
  // less difference between side which makes aspiration search faster
  // (not empirically tested)
  score.midgame += EvalConfig::TEMPO;

  // calculate value depending on game phases
  const double gamePhaseFactor = p.getGamePhaseFactor();
  Value value                  = static_cast<Value>(score.midgame * gamePhaseFactor + score.endgame * (1.0 - gamePhaseFactor));

  // normalize for next player
  value *= p.getNextPlayer() == WHITE ? 1 : -1;

  return value;
}
