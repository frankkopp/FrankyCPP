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
#include "types/types.h"
#include <common/Logging.h>

Evaluator::Evaluator() {
  if (EvalConfig::USE_PAWN_TT) {
    pawnCache.resize(EvalConfig::PAWN_TT_SIZE_MB);
  }
  else {
    LOG__INFO(Logger::get().EVAL_LOG, "Pawn Cache is disabled in configuration");
  }
}

Value Evaluator::evaluate(Position& p) {

  // if not enough material on the board to achieve a mate it is a draw
  if (p.checkInsufficientMaterial()) {
    return VALUE_DRAW;
  }

  // Each position is evaluated from the view of the white
  // player. Before returning the value this will be adjusted
  // to the next player's color.
  // All heuristic should return a value in centi pawns or
  // have a dedicated configurable weight to adjust and test

  Score score{};

  const double gamePhaseFactor = p.getGamePhaseFactor();

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

  // early exit
  // arbitrary threshold - in early phases (game phase = 1.0) this is doubled
  // in late phases it stands as it is
  if (EvalConfig::USE_LAZY_EVAL) {
    auto th     = EvalConfig::LAZY_THRESHOLD + (EvalConfig::LAZY_THRESHOLD * gamePhaseFactor);
    Value value = valueFromScore(score, gamePhaseFactor);
    if (value > th) {
      return finalEval(p, value);
    }
  }

  // evaluate pawns
  if (EvalConfig::USE_PAWN_EVAL) {
    pawnEval(p, score);
  }

  // calculate value depending on game phases
  Value value = valueFromScore(score, gamePhaseFactor);

  // normalize for next player
  value = finalEval(p, value);

  return value;
}

inline Value Evaluator::finalEval(const Position& p, Value value) {
  return value * (p.getNextPlayer() == WHITE ? 1 : -1);
}

inline Value Evaluator::valueFromScore(const Score& score, double gamePhaseFactor) {
  return static_cast<Value>(score.midgame * gamePhaseFactor + score.endgame * (1.0 - gamePhaseFactor));
}

void Evaluator::pawnEval(Position& p, Score& score) {

  // check pawn tt

  if (EvalConfig::USE_PAWN_TT) {
    PawnTT::Entry* entryPtr = pawnCache.getEntryPtr(p.getPawnZobristKey());
    if (entryPtr->key == p.getPawnZobristKey()) {
      score.midgame += entryPtr->midvalue;
      score.endgame += entryPtr->endvalue;
      return;
    }
  }

  Score tmpScore{};

  // evaluations inspired by Stockfish
  for (Color color = WHITE; color <= BLACK; ++color) {
    const Bitboard myPawns  = p.getPieceBb(color, PAWN);
    const Bitboard oppPawns = p.getPieceBb(~color, PAWN);

    Bitboard isolated  = BbZero;
    Bitboard doubled   = BbZero;
    Bitboard passed    = BbZero;
    Bitboard blocked   = BbZero;
    Bitboard phalanx   = BbZero;// both pawns are counted
    Bitboard supported = BbZero;

    // LOOP through all pawns of this color and type
    Bitboard pawns = myPawns;
    while (pawns) {
      const Square sq           = popLSB(pawns);
      const Bitboard neighbours = myPawns & Bitboards::neighbourFilesMask[sq];
      const Bitboard rayForward = Bitboards::rays[color == WHITE ? N : S][sq];

      // isolated pawns
      isolated |= neighbours ? BbZero : Bitboards::sqBb[sq];

      // doubled pawns - any other of my pawns on same file
      doubled |= ~Bitboards::sqBb[sq] & myPawns & rayForward;

      // passed pawns - no opponent pawns in the area before me and no own pawn before me
      passed |= ((myPawns & rayForward) | (oppPawns & Bitboards::passedPawnMask[color][sq]))
                  ? BbZero
                  : Bitboards::sqBb[sq];

      // blocked pawns
      blocked |= ((myPawns | oppPawns) & rayForward)
                   ? Bitboards::sqBb[sq]
                   : BbZero;

      // pawns as neighbours in a row = phalanx
      phalanx |= (neighbours & Bitboards::sqToRankBb[sq])
                   ? Bitboards::sqBb[sq]
                   : BbZero;

      // pawn as neighbours in the row forward = supported pawns
      supported |= myPawns & neighbours & Bitboards::sqToRankBb[sq + pawnPush(color)];
    }

    //    fprintln("{} isolated : {} {}", !color ? "WHITE" : "BLACK", popcount(isolated), strGrouped(isolated));
    //    fprintln("{} doubled  : {} {}", !color ? "WHITE" : "BLACK", popcount(doubled), strGrouped(doubled));
    //    fprintln("{} passed   : {} {}", !color ? "WHITE" : "BLACK", popcount(passed), strGrouped(passed));
    //    fprintln("{} blocked  : {} {}", !color ? "WHITE" : "BLACK", popcount(blocked), strGrouped(blocked));
    //    fprintln("{} phalanx  : {} {}", !color ? "WHITE" : "BLACK", popcount(phalanx), strGrouped(phalanx));
    //    fprintln("{} supported: {} {}", !color ? "WHITE" : "BLACK", popcount(supported), strGrouped(supported));

    // @formatter:off
    int midvalue =  popcount(isolated)  * EvalConfig::ISOLATED_PAWN_MID_WEIGHT ;
    int endvalue =  popcount(isolated)  * EvalConfig::ISOLATED_PAWN_END_WEIGHT ;
    midvalue    +=  popcount(doubled)   * EvalConfig::DOUBLED_PAWN_MID_WEIGHT  ;
    endvalue    +=  popcount(doubled)   * EvalConfig::DOUBLED_PAWN_END_WEIGHT  ;
    midvalue    +=  popcount(passed)    * EvalConfig::PASSED_PAWN_MID_WEIGHT   ;
    endvalue    +=  popcount(passed)    * EvalConfig::PASSED_PAWN_END_WEIGHT   ;
    midvalue    +=  popcount(blocked)   * EvalConfig::BLOCKED_PAWN_MID_WEIGHT  ;
    endvalue    +=  popcount(blocked)   * EvalConfig::BLOCKED_PAWN_END_WEIGHT  ;
    midvalue    +=  popcount(phalanx)   * EvalConfig::PHALANX_PAWN_MID_WEIGHT  ;
    endvalue    +=  popcount(phalanx)   * EvalConfig::PHALANX_PAWN_END_WEIGHT  ;
    midvalue    +=  popcount(supported) * EvalConfig::SUPPORTED_PAWN_MID_WEIGHT;
    endvalue    +=  popcount(supported) * EvalConfig::SUPPORTED_PAWN_END_WEIGHT;
    // @formatter:on

    if (color == WHITE) {
      tmpScore.midgame += midvalue;
      tmpScore.endgame += endvalue;
    }
    else {
      tmpScore.midgame -= midvalue;
      tmpScore.endgame -= endvalue;
    }
    //    LOG__DEBUG(Logger::get().EVAL_LOG, "Raw pawn eval for {} results midvalue = {} and endvalue = {}", color ? "BLACK" : "WHITE", midvalue, endvalue);
  }// color loop

  // check pawn tt
  if (EvalConfig::USE_PAWN_TT) {
    pawnCache.put(p.getPawnZobristKey(), tmpScore);
  }

  score += tmpScore;

//  LOG__DEBUG(Logger::get().EVAL_LOG, "Raw pawn eval: midvalue = {} and endvalue = {}", tmpScore.midgame, tmpScore.endgame);
}
