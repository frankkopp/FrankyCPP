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

#include "types/types.h"

#include "Evaluator.h"

#include "config/ConfigManager.h"

using namespace engine;
using namespace chess;
using namespace config;
using namespace common;

Evaluator::Evaluator()
    : EvalConfig(ConfigManager::instance().eval()) {
  // PawnTT is now managed by Search and passed via setPawnTT()
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
  // DEBUG - test of new config system
  if (EvalConfig.USE_MATERIAL) {
    score.midgame = static_cast<Value>(p.getMaterial(WHITE) - p.getMaterial(BLACK));
    score.endgame = score.midgame;
  }

  // positional value
  if (EvalConfig.USE_POSITIONAL) {
    score.midgame += static_cast<Value>(p.getMidPosValue(WHITE) - p.getMidPosValue(BLACK));
    score.endgame += static_cast<Value>(p.getEndPosValue(WHITE) - p.getEndPosValue(BLACK));
  }

  // early exit
  // arbitrary threshold - in early phases (game phase = 1.0) this is doubled
  // in late phases it stands as it is
  if (EvalConfig.USE_LAZY_EVAL) {
    const Value value = valueFromScore(score, gamePhaseFactor);
    if (value > static_cast<Value>(static_cast<int>(EvalConfig.LAZY_THRESHOLD + EvalConfig.LAZY_THRESHOLD * gamePhaseFactor))) {
      return finalEval(p, value);
    }
  }

  // evaluate pawns
  if (EvalConfig.USE_PAWN_EVAL) {
    pawnEval(p, score);
  }

  // evaluate pieces
  if (EvalConfig.USE_PIECE_EVAL) {
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
  if (EvalConfig.USE_KING_EVAL) {
    kingEval(p, score, WHITE);
    kingEval(p, score, BLACK);
  }

  // TEMPO Bonus for the side to move (helps with evaluation alternation -
  // less difference between side which makes aspiration search faster
  // (not empirically tested)
  if (EvalConfig.USE_TEMPO) {
    score.midgame += static_cast<Value>(EvalConfig.TEMPO);
  }

  // calculate value depending on game phases
  Value value;
  if (EvalConfig.USE_GAMEPHASE_VALUE) {
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
  return value * p.getNextPlayer().sign();
}

inline Value Evaluator::valueFromScore(const Score& score, const double gamePhaseFactor) {
  return score.midgame * gamePhaseFactor + score.endgame * (1.0 - gamePhaseFactor);
}

inline void Evaluator::pawnEval(const Position& p, Score& s) {
  // check pawn hash first
  ZobristKey key{0};
  if (EvalConfig.USE_PAWN_TT && pawnCache) {
    key = p.getPawnZobristKey();
    // Use probe() for thread-safe copy-on-read pattern.
    // This prevents races where another thread overwrites the entry
    // between key check and value reads.
    if (const auto entry = pawnCache->probe(key)) {
      s.midgame += entry->midvalue;
      s.endgame += entry->endvalue;
      return;
    }
  }

  tmpScore.midgame = VALUE_ZERO;
  tmpScore.endgame = VALUE_ZERO;

  // Pawn structure evaluation
  // See: https://www.chessprogramming.org/Pawn_Structure
  for (const Color color : Color::all()) {
    const Bitboard myPawns  = p.getPieceBb(color, PAWN);
    const Bitboard oppPawns = p.getPieceBb(~color, PAWN);

    Bitboard isolated  = BbZero;
    Bitboard doubled   = BbZero;
    Bitboard passed    = BbZero;
    Bitboard blocked   = BbZero;
    Bitboard phalanx   = BbZero; // both pawns are counted
    Bitboard supported = BbZero;

    // LOOP through all pawns of this color
    Bitboard pawns = myPawns;
    while (pawns) {
      const Square sq           = pawns.popLSB();
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
      supported |= myPawns & neighbours & Bitboards::sqToRankBb[sq + Direction::pawnPush(color)];
    }

    // clang-format off
    int midvalue = isolated.popcount() *  EvalConfig.ISOLATED_PAWN_MID_WEIGHT;
    int endvalue = isolated.popcount() *  EvalConfig.ISOLATED_PAWN_END_WEIGHT;
    midvalue    += doubled.popcount() *   EvalConfig.DOUBLED_PAWN_MID_WEIGHT;
    endvalue    += doubled.popcount() *   EvalConfig.DOUBLED_PAWN_END_WEIGHT;
    // clang-format on

    // Passed pawns: flat bonus + optional rank-based bonus
    {
      Bitboard passedCopy = passed;
      while (passedCopy) {
        const Square psq    = passedCopy.popLSB();
        // relative rank: for White rank is 0-based (RANK_1=0), for Black mirror it
        const int relRank   = color == WHITE ? static_cast<int>(psq.rank()) : 7 - static_cast<int>(psq.rank());
        // flat bonus per passed pawn
        midvalue += EvalConfig.PASSED_PAWN_MID_WEIGHT;
        endvalue += EvalConfig.PASSED_PAWN_END_WEIGHT;
        // rank-based bonus (relRank 2..7 maps to array index 0..5)
        if (EvalConfig.USE_PASSED_PAWN_RANK_BONUS && relRank >= 2 && relRank <= 7) {
          midvalue += EvalConfig.PASSED_PAWN_RANK_MID_BONUS[relRank - 2];
          endvalue += EvalConfig.PASSED_PAWN_RANK_END_BONUS[relRank - 2];
        }
      }
    }

    // clang-format off
    midvalue    += blocked.popcount() *   EvalConfig.BLOCKED_PAWN_MID_WEIGHT;
    endvalue    += blocked.popcount() *   EvalConfig.BLOCKED_PAWN_END_WEIGHT;
    midvalue    += phalanx.popcount() *   EvalConfig.PHALANX_PAWN_MID_WEIGHT;
    endvalue    += phalanx.popcount() *   EvalConfig.PHALANX_PAWN_END_WEIGHT;
    midvalue    += supported.popcount() * EvalConfig.SUPPORTED_PAWN_MID_WEIGHT;
    endvalue    += supported.popcount() * EvalConfig.SUPPORTED_PAWN_END_WEIGHT;
    // clang-format on

    // accumulate signed by color
    tmpScore.midgame += static_cast<Value>(midvalue * color.sign());
    tmpScore.endgame += static_cast<Value>(endvalue * color.sign());
    //    LOG__DEBUG(Logger::get().EVAL_LOG, "Raw pawn eval for {} results midvalue = {} and endvalue = {}", color ? "BLACK" : "WHITE", midvalue, endvalue);
  } // color loop

  // Store back only when enabled; get entry pointer for put()
  if (EvalConfig.USE_PAWN_TT && pawnCache && key != 0) {
    pawnCache->put(pawnCache->getEntryPtr(key), key, tmpScore);
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
        knightEval(p, s, us, ~us, pieceBb.popLSB());
      }
      break;
    case BISHOP:
      // general evaluation for all pieces of this color

      // bonus for a pair
      if (EvalConfig.USE_BISHOP_PAIR_BONUS && pieceBb.popcount() > 1) {
        s.midgame += static_cast<Value>(static_cast<int>(EvalConfig.BISHOP_PAIR_MID_BONUS) * us.sign());
        s.endgame += static_cast<Value>(static_cast<int>(EvalConfig.BISHOP_PAIR_END_BONUS) * us.sign());
      }

      // loop through all bishops of this color
      while (pieceBb) {
        bishopEval(p, s, us, ~us, pieceBb.popLSB());
      }
      break;
    case ROOK:
      // general evaluation for all pieces of this color

      // loop through all rooks of this color
      while (pieceBb) {
        rookEval(p, s, us, ~us, pieceBb.popLSB());
      }
      break;
    case QUEEN:
      // general evaluation for all pieces of this color

      // loop through all queens of this color
      while (pieceBb) {
        queenEval(p, s, us, ~us, pieceBb.popLSB());
      }
      break;
    default:
      break;
  }
}

inline void Evaluator::knightEval(const Position& p, Score& s, const Color us, Color /*unused*/, const Square sq) const {
  if (EvalConfig.USE_KNIGHT_MOBILITY) {
    const Bitboard myOcc   = p.getOccupiedBb(us);
    const Bitboard attacks = Attacks::attacks(KNIGHT, sq, BbZero);
    const int mobility     = (attacks & ~myOcc).popcount();

    int mid = mobility * EvalConfig.KNIGHT_MOBILITY_MID_PER_MOVE;
    int end = mobility * EvalConfig.KNIGHT_MOBILITY_END_PER_MOVE;

    if (mobility <= 1) {
      mid += EvalConfig.KNIGHT_LOW_MOBILITY_LEQ1_MID;
      end += EvalConfig.KNIGHT_LOW_MOBILITY_LEQ1_END;
    }
    else if (mobility <= 2) {
      mid += EvalConfig.KNIGHT_LOW_MOBILITY_LEQ2_MID;
      end += EvalConfig.KNIGHT_LOW_MOBILITY_LEQ2_END;
    }

    s.midgame += static_cast<Value>(mid * us.sign());
    s.endgame += static_cast<Value>(end * us.sign());
  }
}

inline void Evaluator::bishopEval(const Position& p, Score& s, const Color us, Color /*unused*/, const Square sq) const {
  // Mobility for bishops (Tier 1)
  if (EvalConfig.USE_BISHOP_MOBILITY) {
    const Bitboard myOcc    = p.getOccupiedBb(us);
    const Bitboard occupied = p.getOccupiedBb();
    const Bitboard attacks  = Attacks::attacks(BISHOP, sq, occupied);
    const int mobility      = (attacks & ~myOcc).popcount();

    int mid = mobility * EvalConfig.BISHOP_MOBILITY_MID_PER_MOVE;
    int end = mobility * EvalConfig.BISHOP_MOBILITY_END_PER_MOVE;

    if (mobility <= 3) {
      mid += EvalConfig.BISHOP_LOW_MOBILITY_LEQ3_MID;
      end += EvalConfig.BISHOP_LOW_MOBILITY_LEQ3_END;
    }

    s.midgame += static_cast<Value>(mid * us.sign());
    s.endgame += static_cast<Value>(end * us.sign());
  }
}

inline void Evaluator::rookEval(const Position& p, Score& s, const Color us, const Color them, const Square sq) const {
  int mid = 0;
  int end = 0;

  // Mobility
  if (EvalConfig.USE_ROOK_MOBILITY) {
    const Bitboard myOcc    = p.getOccupiedBb(us);
    const Bitboard occupied = p.getOccupiedBb();
    const Bitboard attacks  = Attacks::attacks(ROOK, sq, occupied);
    const int mobility      = (attacks & ~myOcc).popcount();

    mid += mobility * EvalConfig.ROOK_MOBILITY_MID_PER_MOVE;
    end += mobility * EvalConfig.ROOK_MOBILITY_END_PER_MOVE;

    if (mobility <= 3) {
      mid += EvalConfig.ROOK_LOW_MOBILITY_LEQ3_MID;
      end += EvalConfig.ROOK_LOW_MOBILITY_LEQ3_END;
    }
  }

  // Open/semi-open file bonuses
  if (EvalConfig.USE_ROOK_OPEN_FILE_BONUS) {
    const Bitboard fileMask    = Bitboards::sqToFileBb[sq];
    const Bitboard myPawns     = p.getPieceBb(us, PAWN);
    const Bitboard theirPawns  = p.getPieceBb(them, PAWN);
    const bool myPawnOnFile    = (myPawns & fileMask) != 0;
    const bool theirPawnOnFile = (theirPawns & fileMask) != 0;

    if (!myPawnOnFile) {
      if (!theirPawnOnFile) {
        mid += EvalConfig.ROOK_OPEN_FILE_MID_BONUS;
        end += EvalConfig.ROOK_OPEN_FILE_END_BONUS;
      }
      else {
        mid += EvalConfig.ROOK_SEMIOPEN_FILE_MID_BONUS;
        end += EvalConfig.ROOK_SEMIOPEN_FILE_END_BONUS;
      }
    }
  }

  if (mid || end) {
    s.midgame += static_cast<Value>(mid * us.sign());
    s.endgame += static_cast<Value>(end * us.sign());
  }
}

inline void Evaluator::queenEval(const Position& p, Score& s, const Color us, const Color them, const Square sq) const {
  int mid = 0;
  int end = 0;

  // Mobility (Tier 1)
  if (EvalConfig.USE_QUEEN_MOBILITY) {
    const Bitboard myOcc    = p.getOccupiedBb(us);
    const Bitboard occupied = p.getOccupiedBb();
    const Bitboard attacks  = Attacks::attacks(QUEEN, sq, occupied);
    const int mobility      = (attacks & ~myOcc).popcount();

    mid += mobility * EvalConfig.QUEEN_MOBILITY_MID_PER_MOVE;
    end += mobility * EvalConfig.QUEEN_MOBILITY_END_PER_MOVE;
  }

  // Simple tropism towards enemy king (Tier 0/phase-scaled)
  if (EvalConfig.USE_QUEEN_TROPISM) {
    const Square ksq    = p.getKingSquare(them);
    const int dist      = sq.distanceTo(ksq); // 0..7
    const int closeness = 8 - dist;           // 1..8 (or 8 if dist==0)
    mid += closeness * EvalConfig.QUEEN_TROPISM_MID_PER_STEP;
    end += closeness * EvalConfig.QUEEN_TROPISM_END_PER_STEP;
  }

  if (mid || end) {
    s.midgame += static_cast<Value>(mid * us.sign());
    s.endgame += static_cast<Value>(end * us.sign());
  }
}

inline void Evaluator::kingEval(const Position& p, Score& s, const Color us) const {
  int mid = 0;
  int end = 0;

  const Square ksq = p.getKingSquare(us);

  // Pawn shield in front of king (midgame focus)
  if (EvalConfig.USE_KING_SAFETY_SHIELD) {
    int shieldCount = 0;
    const int dir   = us.sign();
    const int kr    = ksq.rank();
    const int kf    = ksq.file();

    for (int df = -1; df <= 1; ++df) {
      const int f = kf + df;
      if (f < FILE_A || f > FILE_H) continue;
      for (int dr = 1; dr <= 2; ++dr) {
        const int r = kr + dir * dr;
        if (r < RANK_1 || r > RANK_8) continue;
        const Square sq2 = Square::of(static_cast<File>(f), static_cast<Rank>(r));
        if (p.getPiece(sq2) == makePiece(us, PAWN)) {
          ++shieldCount;
        }
      }
    }
    mid += shieldCount * EvalConfig.KING_SHIELD_MID_PER_PAWN;
    end += shieldCount * EvalConfig.KING_SHIELD_END_PER_PAWN;
  }

  s.midgame += static_cast<Value>(mid * us.sign());
  s.endgame += static_cast<Value>(end * us.sign());
}
