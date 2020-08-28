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

#include "common/Logging.h"
#include "types/types.h"

#include "EvalConfig.h"
#include "Evaluator.h"

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

  score.midgame = VALUE_ZERO;
  score.endgame = VALUE_ZERO;

  const double gamePhaseFactor = p.getGamePhaseFactor();

  // material
  if (EvalConfig::USE_MATERIAL) {
    score.midgame = static_cast<Value>(p.getMaterial(WHITE) - p.getMaterial(BLACK));
    score.endgame = score.midgame;
  }

  // positional value
  if (EvalConfig::USE_POSITIONAL) {
    score.midgame += static_cast<Value>(p.getMidPosValue(WHITE) - p.getMidPosValue(BLACK));
    score.endgame += static_cast<Value>(p.getEndPosValue(WHITE) - p.getEndPosValue(BLACK));
  }

  // early exit
  // arbitrary threshold - in early phases (game phase = 1.0) this is doubled
  // in late phases it stands as it is
  if (EvalConfig::USE_LAZY_EVAL) {
    Value value = valueFromScore(score, gamePhaseFactor);
    if (value > EvalConfig::LAZY_THRESHOLD + (EvalConfig::LAZY_THRESHOLD * gamePhaseFactor)) {
      return finalEval(p, value);
    }
  }

  // evaluate pawns
  if (EvalConfig::USE_PAWN_EVAL) {
    pawnEval(p, score);
  }

  // evaluate pieces
  if (EvalConfig::USE_PIECE_EVAL) {
    pieceEval(p, score, WHITE, KNIGHT);
    pieceEval(p, score, BLACK, KNIGHT);
    pieceEval(p, score, WHITE, BISHOP);
    pieceEval(p, score, BLACK, BISHOP);
    pieceEval(p, score, WHITE, ROOK);
    pieceEval(p, score, BLACK, ROOK);
    pieceEval(p, score, WHITE, QUEEN);
    pieceEval(p, score, BLACK, QUEEN);
  }

  // evaluate kings
  if (EvalConfig::USE_KING_EVAL) {
    kingEval(p, score, WHITE);
    kingEval(p, score, BLACK);
  }

  // TEMPO Bonus for the side to move (helps with evaluation alternation -
  // less difference between side which makes aspiration search faster
  // (not empirically tested)
  score.midgame += static_cast<Value>(EvalConfig::TEMPO);

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

void Evaluator::pawnEval(Position& p, Score& s) {
  PawnTT::Entry* entryPtr{};
  const Key key = p.getPawnZobristKey();

  // check pawn tt
  if (EvalConfig::USE_PAWN_TT) {
    entryPtr = pawnCache.getEntryPtr(key);
    if (entryPtr->key == key) {
      s.midgame += entryPtr->midvalue;
      s.endgame += entryPtr->endvalue;
      return;
    }
  }

  tmpScore.midgame = VALUE_ZERO;
  tmpScore.endgame = VALUE_ZERO;

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

    // LOOP through all pawns of this color 
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
      tmpScore.midgame += static_cast<Value>(midvalue);
      tmpScore.endgame += static_cast<Value>(endvalue);
    }
    else {
      tmpScore.midgame -= static_cast<Value>(midvalue);
      tmpScore.endgame -= static_cast<Value>(endvalue);
    }
    //    LOG__DEBUG(Logger::get().EVAL_LOG, "Raw pawn eval for {} results midvalue = {} and endvalue = {}", color ? "BLACK" : "WHITE", midvalue, endvalue);
  }// color loop

  // check pawn tt
  if (EvalConfig::USE_PAWN_TT) {
    pawnCache.put(entryPtr, key, tmpScore);
  }

  s += tmpScore;

  //  LOG__DEBUG(Logger::get().EVAL_LOG, "Raw pawn eval: midvalue = {} and endvalue = {}", tmpScore.midgame, tmpScore.endgame);
}

void Evaluator::pieceEval(const Position& p, Score& s, Color us, PieceType pieceType) {

  // get pieces or return if none of given type or color is found
  Bitboard pieceBb = p.getPieceBb(us, pieceType);
  if (!pieceBb) {
    return;
  }

  tmpScore.midgame = VALUE_ZERO;
  tmpScore.endgame = VALUE_ZERO;

  // piece type specific evaluation which are done once
  // for all pieces of one type
  switch (pieceType) {
    case KNIGHT:
      while (pieceBb) {
        knightEval(p, s, us, ~us, popLSB(pieceBb));
      }
      break;
    case BISHOP:
      // bonus for pair
      if (popcount(pieceBb) > 1) {
        s.midgame += EvalConfig::BISHOP_PAIR_MID_BONUS;
        s.endgame += EvalConfig::BISHOP_PAIR_END_BONUS;
      }
      while (pieceBb) {
        bishopEval(p, s, us, ~us, popLSB(pieceBb));
      }
      break;
    case ROOK:
      while (pieceBb) {
        rookEval(p, s, us, ~us, popLSB(pieceBb));
      }
      break;
    case QUEEN:
      while (pieceBb) {
        queenEval(p, s, us, ~us, popLSB(pieceBb));
      }
      break;
    default:
      break;
  }
}

void Evaluator::knightEval(const Position& p, Score& s, Color us, Color them, Square sq) {
  // TODO: Knight eval
}

void Evaluator::bishopEval(const Position& p, Score& s, Color us, Color them, Square sq) {
  // TODO: Bishop eval
}

void Evaluator::rookEval(const Position& p, Score& s, Color us, Color them, Square sq) {
  // TODO: Rook eval
}

void Evaluator::queenEval(const Position& p, Score& s, Color us, Color them, Square sq) {
  // TODO: Queen eval
}

void Evaluator::kingEval(const Position& p, Score& s, Color us) {
  // TODO: King eval
}
