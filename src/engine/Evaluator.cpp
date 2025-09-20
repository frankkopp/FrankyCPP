// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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

#include "common/Logging.h"
#include "types/types.h"

#include "Evaluator.h"

Evaluator::Evaluator() {
  if (EvalConfig::USE_PAWN_TT) {
    pawnCache.resize(EvalConfig::PAWN_TT_SIZE_MB);
  }
  else {
    LOG__INFO(Logger::get().EVAL_LOG, "Pawn Cache is disabled in configuration");
  }
}

Value Evaluator::evaluate(const Position& p) {
  // if not enough material on the board to achieve a mate it is a draw
  if (p.checkInsufficientMaterial()) {
    return VALUE_DRAW;
  }

  // Each position is evaluated from the view of the white
  // player. Before returning the value, this will be adjusted
  // to the next player's color.
  // All heuristics should return a value in centi pawns or
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
    const Value value = valueFromScore(score, gamePhaseFactor);
    if (value > EvalConfig::LAZY_THRESHOLD + EvalConfig::LAZY_THRESHOLD * gamePhaseFactor) {
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
  if (EvalConfig::USE_TEMPO) {
    score.midgame += static_cast<Value>(EvalConfig::TEMPO);
  }

  // calculate value depending on game phases
  Value value;
  if (EvalConfig::USE_GAMEPHASE_VALUE) {
    value = valueFromScore(score, gamePhaseFactor);
  }
  else {
    value = (score.midgame + score.endgame) / 2;
  }

  // normalize for the next player
  value = finalEval(p, value);

  return value;
}

inline Value Evaluator::finalEval(const Position& p, const Value value) {
  return value * (p.getNextPlayer() == WHITE ? 1 : -1);
}

inline Value Evaluator::valueFromScore(const Score& score, const double gamePhaseFactor) {
  return score.midgame * gamePhaseFactor + score.endgame * (1.0 - gamePhaseFactor);
}

inline void Evaluator::pawnEval(const Position& p, Score& s) {
  const Key key = p.getPawnZobristKey();

  // Branch-minimal TT probe: always safe (dummy slot when mask == 0)
  PawnTT::Entry* ep = pawnCache.getEntryPtr(key);

  // Fast hit check; when TT is off this is an inexpensive, well-predicted branch
  if (EvalConfig::USE_PAWN_TT && ep->key == key) {
    s.midgame += ep->midvalue;
    s.endgame += ep->endvalue;
    return;
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
      passed |= myPawns & rayForward | oppPawns & Bitboards::passedPawnMask[color][sq]
                  ? BbZero
                  : Bitboards::sqBb[sq];

      // blocked pawns
      blocked |= (myPawns | oppPawns) & rayForward
                   ? Bitboards::sqBb[sq]
                   : BbZero;

      // pawns as neighbours in a row = phalanx
      phalanx |= neighbours & Bitboards::sqToRankBb[sq]
                   ? Bitboards::sqBb[sq]
                   : BbZero;

      // pawn as neighbours in the row forward = supported pawns
      supported |= myPawns & neighbours & Bitboards::sqToRankBb[sq + pawnPush(color)];
    }

    // @formatter:off
    int midvalue = popcount(isolated) * EvalConfig::ISOLATED_PAWN_MID_WEIGHT;
    int endvalue = popcount(isolated) * EvalConfig::ISOLATED_PAWN_END_WEIGHT;
    midvalue += popcount(doubled) * EvalConfig::DOUBLED_PAWN_MID_WEIGHT;
    endvalue += popcount(doubled) * EvalConfig::DOUBLED_PAWN_END_WEIGHT;
    midvalue += popcount(passed) * EvalConfig::PASSED_PAWN_MID_WEIGHT;
    endvalue += popcount(passed) * EvalConfig::PASSED_PAWN_END_WEIGHT;
    midvalue += popcount(blocked) * EvalConfig::BLOCKED_PAWN_MID_WEIGHT;
    endvalue += popcount(blocked) * EvalConfig::BLOCKED_PAWN_END_WEIGHT;
    midvalue += popcount(phalanx) * EvalConfig::PHALANX_PAWN_MID_WEIGHT;
    endvalue += popcount(phalanx) * EvalConfig::PHALANX_PAWN_END_WEIGHT;
    midvalue += popcount(supported) * EvalConfig::SUPPORTED_PAWN_MID_WEIGHT;
    endvalue += popcount(supported) * EvalConfig::SUPPORTED_PAWN_END_WEIGHT;
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

  // Store back only when enabled; ep already points to the correct slot
  if (EvalConfig::USE_PAWN_TT) {
    pawnCache.put(ep, key, tmpScore);
  }

  s += tmpScore;

  //  LOG__DEBUG(Logger::get().EVAL_LOG, "Raw pawn eval: midvalue = {} and endvalue = {}", tmpScore.midgame, tmpScore.endgame);
}

inline void Evaluator::pieceEval(const Position& p, Score& s, const Color us, const PieceType pieceType) {

  // get pieces or return if none of given types or color is found
  Bitboard pieceBb = p.getPieceBb(us, pieceType);
  if (!pieceBb) {
    return;
  }

  tmpScore.midgame = VALUE_ZERO;
  tmpScore.endgame = VALUE_ZERO;

  // Each switch case allows for piece-type specific evaluation of all pieces of that type
  // of the given color and loops through all pieces of that type to call the specific evaluation
  // function for a single piece
  switch (pieceType) {
    case KNIGHT:
      // general evaluation for all pieces of this color

      // loop through all knights of this color
      while (pieceBb) {
        knightEval(p, s, us, ~us, popLSB(pieceBb));
      }
      break;
    case BISHOP:
      // general evaluation for all pieces of this color

      // bonus for a pair
      if (EvalConfig::USE_BISHOP_PAIR_BONUS && popcount(pieceBb) > 1) {
        s.midgame += EvalConfig::BISHOP_PAIR_MID_BONUS;
        s.endgame += EvalConfig::BISHOP_PAIR_END_BONUS;
      }

      // loop through all bishops of this color
      while (pieceBb) {
        bishopEval(p, s, us, ~us, popLSB(pieceBb));
      }
      break;
    case ROOK:
      // general evaluation for all pieces of this color

      // loop through all rooks of this color
      while (pieceBb) {
        rookEval(p, s, us, ~us, popLSB(pieceBb));
      }
      break;
    case QUEEN:
      // general evaluation for all pieces of this color

      // loop through all queens of this color
      while (pieceBb) {
        queenEval(p, s, us, ~us, popLSB(pieceBb));
      }
      break;
    default:
      break;
  }
}

inline void Evaluator::knightEval(const Position& p, Score& s, const Color us, Color, const Square sq) {
  if (EvalConfig::USE_KNIGHT_MOBILITY) {
    const Bitboard myOcc   = p.getOccupiedBb(us);
    const Bitboard attacks = getAttacksBb(KNIGHT, sq, BbZero);
    const int mobility     = popcount(attacks & ~myOcc);

    int mid = mobility * EvalConfig::KNIGHT_MOBILITY_MID_PER_MOVE;
    int end = mobility * EvalConfig::KNIGHT_MOBILITY_END_PER_MOVE;

    if (mobility <= 1) {
      mid += EvalConfig::KNIGHT_LOW_MOBILITY_LEQ1_MID;
      end += EvalConfig::KNIGHT_LOW_MOBILITY_LEQ1_END;
    }
    else if (mobility <= 2) {
      mid += EvalConfig::KNIGHT_LOW_MOBILITY_LEQ2_MID;
      end += EvalConfig::KNIGHT_LOW_MOBILITY_LEQ2_END;
    }

    if (us == WHITE) {
      s.midgame += static_cast<Value>(mid);
      s.endgame += static_cast<Value>(end);
    }
    else {
      s.midgame -= static_cast<Value>(mid);
      s.endgame -= static_cast<Value>(end);
    }
  }
}

inline void Evaluator::bishopEval(const Position& p, Score& s, const Color us, Color , const Square sq) {
  // Mobility for bishops (Tier 1)
  if (EvalConfig::USE_BISHOP_MOBILITY) {
    const Bitboard myOcc     = p.getOccupiedBb(us);
    const Bitboard occupied  = p.getOccupiedBb();
    const Bitboard attacks   = getAttacksBb(BISHOP, sq, occupied);
    const int mobility       = popcount(attacks & ~myOcc);

    int mid = mobility * EvalConfig::BISHOP_MOBILITY_MID_PER_MOVE;
    int end = mobility * EvalConfig::BISHOP_MOBILITY_END_PER_MOVE;

    if (mobility <= 3) {
      mid += EvalConfig::BISHOP_LOW_MOBILITY_LEQ3_MID;
      end += EvalConfig::BISHOP_LOW_MOBILITY_LEQ3_END;
    }

    if (us == WHITE) {
      s.midgame += static_cast<Value>(mid);
      s.endgame += static_cast<Value>(end);
    }
    else {
      s.midgame -= static_cast<Value>(mid);
      s.endgame -= static_cast<Value>(end);
    }
  }
}

inline void Evaluator::rookEval(const Position& p, Score& s, const Color us, const Color them, const Square sq) {
  int mid = 0;
  int end = 0;

  // Mobility
  if (EvalConfig::USE_ROOK_MOBILITY) {
    const Bitboard myOcc    = p.getOccupiedBb(us);
    const Bitboard occupied = p.getOccupiedBb();
    const Bitboard attacks  = getAttacksBb(ROOK, sq, occupied);
    const int mobility      = popcount(attacks & ~myOcc);

    mid += mobility * EvalConfig::ROOK_MOBILITY_MID_PER_MOVE;
    end += mobility * EvalConfig::ROOK_MOBILITY_END_PER_MOVE;

    if (mobility <= 3) {
      mid += EvalConfig::ROOK_LOW_MOBILITY_LEQ3_MID;
      end += EvalConfig::ROOK_LOW_MOBILITY_LEQ3_END;
    }
  }

  // Open/semi-open file bonuses
  if (EvalConfig::USE_ROOK_OPEN_FILE_BONUS) {
    const Bitboard fileMask   = Bitboards::sqToFileBb[sq];
    const Bitboard myPawns    = p.getPieceBb(us, PAWN);
    const Bitboard theirPawns = p.getPieceBb(them, PAWN);
    const bool myPawnOnFile   = (myPawns & fileMask) != 0;
    const bool theirPawnOnFile= (theirPawns & fileMask) != 0;

    if (!myPawnOnFile) {
      if (!theirPawnOnFile) {
        mid += EvalConfig::ROOK_OPEN_FILE_MID_BONUS;
        end += EvalConfig::ROOK_OPEN_FILE_END_BONUS;
      }
      else{
        mid += EvalConfig::ROOK_SEMIOPEN_FILE_MID_BONUS;
        end += EvalConfig::ROOK_SEMIOPEN_FILE_END_BONUS;
      }
    }
  }

  if (mid || end) {
    if (us == WHITE) {
      s.midgame += static_cast<Value>(mid);
      s.endgame += static_cast<Value>(end);
    }
    else {
      s.midgame -= static_cast<Value>(mid);
      s.endgame -= static_cast<Value>(end);
    }
  }
}

inline void Evaluator::queenEval(const Position& p, Score& s, const Color us, const Color them, const Square sq) {
  int mid = 0;
  int end = 0;

  // Mobility (Tier 1)
  if (EvalConfig::USE_QUEEN_MOBILITY) {
    const Bitboard myOcc    = p.getOccupiedBb(us);
    const Bitboard occupied = p.getOccupiedBb();
    const Bitboard attacks  = getAttacksBb(QUEEN, sq, occupied);
    const int mobility      = popcount(attacks & ~myOcc);

    mid += mobility * EvalConfig::QUEEN_MOBILITY_MID_PER_MOVE;
    end += mobility * EvalConfig::QUEEN_MOBILITY_END_PER_MOVE;
  }

  // Simple tropism towards enemy king (Tier 0/phase-scaled)
  if (EvalConfig::USE_QUEEN_TROPISM) {
    const Square ksq = p.getKingSquare(them);
    const int dist   = distance(sq, ksq); // 0..7
    const int closeness = 8 - dist;       // 1..8 (or 8 if dist==0)
    mid += closeness * EvalConfig::QUEEN_TROPISM_MID_PER_STEP;
    end += closeness * EvalConfig::QUEEN_TROPISM_END_PER_STEP;
  }

  if (mid || end) {
    if (us == WHITE) {
      s.midgame += static_cast<Value>(mid);
      s.endgame += static_cast<Value>(end);
    }
    else {
      s.midgame -= static_cast<Value>(mid);
      s.endgame -= static_cast<Value>(end);
    }
  }
}

inline void Evaluator::kingEval(const Position& p, Score& s, const Color us) {
  int mid = 0;
  int end = 0;

  const Square ksq = p.getKingSquare(us);

  // Pawn shield in front of king (midgame focus)
  if (EvalConfig::USE_KING_SAFETY_SHIELD) {
    int shieldCount = 0;
    const int dir   = (us == WHITE ? 1 : -1);
    const int kr    = rankOf(ksq);
    const int kf    = fileOf(ksq);

    for (int df = -1; df <= 1; ++df) {
      const int f = kf + df;
      if (f < FILE_A || f > FILE_H) continue;
      for (int dr = 1; dr <= 2; ++dr) {
        const int r = kr + dir * dr;
        if (r < RANK_1 || r > RANK_8) continue;
        const Square sq2 = squareOf(static_cast<File>(f), static_cast<Rank>(r));
        if (p.getPiece(sq2) == makePiece(us, PAWN)) {
          ++shieldCount;
        }
      }
    }
    mid += shieldCount * EvalConfig::KING_SHIELD_MID_PER_PAWN;
    end += shieldCount * EvalConfig::KING_SHIELD_END_PER_PAWN;
  }

  if (us == WHITE) {
    s.midgame += static_cast<Value>(mid);
    s.endgame += static_cast<Value>(end);
  }
  else {
    s.midgame -= static_cast<Value>(mid);
    s.endgame -= static_cast<Value>(end);
  }
}
