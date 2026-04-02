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

#include <algorithm>
#include <format>

using namespace engine;
using namespace chess;
using namespace config;
using namespace common;

Evaluator::Evaluator()
    : EvalConfig(ConfigManager::instance().eval()) {
  // PawnTT is managed by Search and passed via setPawnTT()
}

std::string EvalTrace::str() const {
  if (insufficientMaterial) {
    return "eval: draw (insufficient material)";
  }
  if (lazyExit) {
    return std::format("eval: {} (lazy exit) | material mg={} eg={} | positional mg={} eg={} | phase={:.2f}",
                       total.str(),
                       static_cast<int>(material.midgame), static_cast<int>(material.endgame),
                       static_cast<int>(positional.midgame), static_cast<int>(positional.endgame),
                       phase);
  }
  return std::format(
    "eval: {} (white: {}) | mat({},{}) pos({},{}) pawn({},{}) pieces({},{}) "
    "threats({},{}) coord({},{}) king({},{}) tempo({},{}) | phase={:.2f}",
    total.str(), totalWhite.str(),
    static_cast<int>(material.midgame), static_cast<int>(material.endgame),
    static_cast<int>(positional.midgame), static_cast<int>(positional.endgame),
    static_cast<int>(pawn.midgame), static_cast<int>(pawn.endgame),
    static_cast<int>(pieces.midgame), static_cast<int>(pieces.endgame),
    static_cast<int>(threats.midgame), static_cast<int>(threats.endgame),
    static_cast<int>(coordination.midgame), static_cast<int>(coordination.endgame),
    static_cast<int>(kingSafety.midgame), static_cast<int>(kingSafety.endgame),
    static_cast<int>(tempo.midgame), static_cast<int>(tempo.endgame),
    phase);
}

EvalTrace Evaluator::evaluateTrace(const Position& p) {
  EvalTrace trace{};

  // insufficient material check
  if (p.checkInsufficientMaterial()) {
    trace.insufficientMaterial = true;
    trace.total = VALUE_DRAW;
    return trace;
  }

  // Reset evaluator state (same as evaluate())
  score.midgame           = VALUE_ZERO;
  score.endgame           = VALUE_ZERO;
  kingAttackCount[WHITE]  = 0;
  kingAttackCount[BLACK]  = 0;
  kingAttackWeight[WHITE] = 0;
  kingAttackWeight[BLACK] = 0;
  attackedBy[WHITE]       = BbZero;
  attackedBy[BLACK]       = BbZero;
  passedPawns[WHITE]      = BbZero;
  passedPawns[BLACK]      = BbZero;
  std::memset(&attackedByPT, 0, sizeof(attackedByPT));

  const double gamePhaseFactor = p.getGamePhaseFactor();
  trace.phase = gamePhaseFactor;

  // material
  if (EvalConfig.USE_MATERIAL) {
    score.midgame = static_cast<Value>(p.getMaterial(WHITE) - p.getMaterial(BLACK));
    score.endgame = score.midgame;
    trace.material = score;
  }

  // positional value
  if (EvalConfig.USE_POSITIONAL) {
    const Score before = score;
    score.midgame += static_cast<Value>(p.getMidPosValue(WHITE) - p.getMidPosValue(BLACK));
    score.endgame += static_cast<Value>(p.getEndPosValue(WHITE) - p.getEndPosValue(BLACK));
    trace.positional = {score.midgame - before.midgame, score.endgame - before.endgame};
  }

  // lazy eval early exit
  if (EvalConfig.USE_LAZY_EVAL) {
    const Value value = valueFromScore(score, gamePhaseFactor);
    if (value > static_cast<Value>(static_cast<int>(EvalConfig.LAZY_THRESHOLD + EvalConfig.LAZY_THRESHOLD * gamePhaseFactor))) {
      trace.lazyExit = true;
      trace.totalWhite = value;
      trace.total = finalEval(p, value);
      return trace;
    }
  }

  // Pre-compute attack data (same as evaluate())
  {
    attackedBy[WHITE] = Bitboards::nonSliderAttacks[KING][p.getKingSquare(WHITE)];
    attackedBy[BLACK] = Bitboards::nonSliderAttacks[KING][p.getKingSquare(BLACK)];
    attackedByPT[KING][WHITE] = attackedBy[WHITE];
    attackedByPT[KING][BLACK] = attackedBy[BLACK];
    Bitboard wp = p.getPieceBb(WHITE, PAWN);
    while (wp) { attackedByPT[PAWN][WHITE] |= Bitboards::pawnAttacks[WHITE][wp.popLSB()]; }
    attackedBy[WHITE] |= attackedByPT[PAWN][WHITE];
    Bitboard bp = p.getPieceBb(BLACK, PAWN);
    while (bp) { attackedByPT[PAWN][BLACK] |= Bitboards::pawnAttacks[BLACK][bp.popLSB()]; }
    attackedBy[BLACK] |= attackedByPT[PAWN][BLACK];
  }

  // pawn eval
  if (EvalConfig.USE_PAWN_EVAL) {
    const Score before = score;
    pawnEval(p, score);
    trace.pawn = {score.midgame - before.midgame, score.endgame - before.endgame};
  }
  else {
    for (const Color c : Color::all()) {
      const Bitboard myPawns  = p.getPieceBb(c, PAWN);
      const Bitboard oppPawns = p.getPieceBb(~c, PAWN);
      Bitboard passed         = BbZero;
      Bitboard pawns          = myPawns;
      while (pawns) {
        const Square sq    = pawns.popLSB();
        const Bitboard fwd = Bitboards::rays[c == WHITE ? N : S][sq];
        if (!(myPawns & fwd) && !(oppPawns & Bitboards::passedPawnMask[c][sq])) {
          passed |= Bitboards::sqBb[sq];
        }
      }
      passedPawns[c] = passed;
    }
  }

  // piece eval
  if (EvalConfig.USE_PIECE_EVAL) {
    const Score before = score;
    pieceEval(p, score, WHITE, KNIGHT);
    pieceEval(p, score, BLACK, KNIGHT);
    pieceEval(p, score, WHITE, BISHOP);
    pieceEval(p, score, BLACK, BISHOP);
    pieceEval(p, score, WHITE, ROOK);
    pieceEval(p, score, BLACK, ROOK);
    pieceEval(p, score, WHITE, QUEEN);
    pieceEval(p, score, BLACK, QUEEN);
    trace.pieces = {score.midgame - before.midgame, score.endgame - before.endgame};
  }

  // threat eval
  if (EvalConfig.USE_THREAT_EVAL) {
    const Score before = score;
    threatEval(p, score, WHITE);
    threatEval(p, score, BLACK);
    trace.threats = {score.midgame - before.midgame, score.endgame - before.endgame};
  }

  // coordination eval
  if (EvalConfig.USE_CONNECTED_ROOKS || EvalConfig.USE_MINOR_CONNECTIVITY) {
    const Score before = score;
    coordinationEval(p, score, WHITE);
    coordinationEval(p, score, BLACK);
    trace.coordination = {score.midgame - before.midgame, score.endgame - before.endgame};
  }

  // king eval
  if (EvalConfig.USE_KING_EVAL) {
    const Score before = score;
    kingEval(p, score, WHITE);
    kingEval(p, score, BLACK);
    trace.kingSafety = {score.midgame - before.midgame, score.endgame - before.endgame};
  }

  // tempo
  if (EvalConfig.USE_TEMPO) {
    score.midgame += static_cast<Value>(EvalConfig.TEMPO);
    trace.tempo = {static_cast<Value>(EvalConfig.TEMPO), VALUE_ZERO};
  }

  // final value
  Value value;
  if (EvalConfig.USE_GAMEPHASE_VALUE) {
    value = valueFromScore(score, gamePhaseFactor);
  }
  else {
    value = (score.midgame + score.endgame) / 2;
  }

  trace.totalWhite = value;
  trace.total = finalEval(p, value);
  return trace;
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

  score.midgame           = VALUE_ZERO;
  score.endgame           = VALUE_ZERO;
  kingAttackCount[WHITE]  = 0;
  kingAttackCount[BLACK]  = 0;
  kingAttackWeight[WHITE] = 0;
  kingAttackWeight[BLACK] = 0;
  attackedBy[WHITE]       = BbZero;
  attackedBy[BLACK]       = BbZero;
  passedPawns[WHITE]      = BbZero;
  passedPawns[BLACK]      = BbZero;

  // Reset per-piece-type attack maps (112 bytes — single memset is faster than element-wise loop)
  std::memset(&attackedByPT, 0, sizeof(attackedByPT));

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

  // Pre-compute reusable data for piece and king evaluation.
  // King and pawn attacks are computed here; piece attacks are accumulated
  // in each piece eval function. passedPawns are computed in pawnEval()
  // (and cached in PawnTT) for reuse by rookEval and kingEval.
  {
    // King attacks
    attackedBy[WHITE] = Bitboards::nonSliderAttacks[KING][p.getKingSquare(WHITE)];
    attackedBy[BLACK] = Bitboards::nonSliderAttacks[KING][p.getKingSquare(BLACK)];
    attackedByPT[KING][WHITE] = attackedBy[WHITE];
    attackedByPT[KING][BLACK] = attackedBy[BLACK];

    // Pawn attacks
    Bitboard wp = p.getPieceBb(WHITE, PAWN);
    while (wp) { attackedByPT[PAWN][WHITE] |= Bitboards::pawnAttacks[WHITE][wp.popLSB()]; }
    attackedBy[WHITE] |= attackedByPT[PAWN][WHITE];
    Bitboard bp = p.getPieceBb(BLACK, PAWN);
    while (bp) { attackedByPT[PAWN][BLACK] |= Bitboards::pawnAttacks[BLACK][bp.popLSB()]; }
    attackedBy[BLACK] |= attackedByPT[PAWN][BLACK];
  }

  // evaluate pawns — also computes passedPawns[] for rookEval/kingEval
  if (EvalConfig.USE_PAWN_EVAL) {
    pawnEval(p, score);
  }
  else {
    // Fallback: compute passedPawns[] when pawn eval is disabled,
    // so rookEval (rook-behind-passer) and kingEval (king proximity) still work.
    for (const Color c : Color::all()) {
      const Bitboard myPawns  = p.getPieceBb(c, PAWN);
      const Bitboard oppPawns = p.getPieceBb(~c, PAWN);
      Bitboard passed         = BbZero;
      Bitboard pawns          = myPawns;
      while (pawns) {
        const Square sq    = pawns.popLSB();
        const Bitboard fwd = Bitboards::rays[c == WHITE ? N : S][sq];
        if (!(myPawns & fwd) && !(oppPawns & Bitboards::passedPawnMask[c][sq])) {
          passed |= Bitboards::sqBb[sq];
        }
      }
      passedPawns[c] = passed;
    }
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

  // evaluate threats (requires fully populated attackedBy[] and attackedByPT[][])
  if (EvalConfig.USE_THREAT_EVAL) {
    threatEval(p, score, WHITE);
    threatEval(p, score, BLACK);
  }


  // evaluate piece coordination (connected rooks, minor connectivity)
  if (EvalConfig.USE_CONNECTED_ROOKS || EvalConfig.USE_MINOR_CONNECTIVITY) {
    coordinationEval(p, score, WHITE);
    coordinationEval(p, score, BLACK);
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
  // ReSharper disable once CppDFAConstantConditions
  if (EvalConfig.USE_PAWN_TT && pawnCache) {
    // ReSharper disable once CppDFAUnreachableCode
    key = p.getPawnZobristKey();
    // Use probe() for thread-safe copy-on-read pattern.
    // This prevents races where another thread overwrites the entry
    // between key check and value reads.
    if (const auto entry = pawnCache->probe(key)) {
      s.midgame += entry->midvalue;
      s.endgame += entry->endvalue;
      // Restore cached passed-pawn bitboards so rookEval/kingEval can use them.
      passedPawns[WHITE] = entry->passedWhite;
      passedPawns[BLACK] = entry->passedBlack;
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

    // Store passed pawns for this color — used by rookEval and kingEval.
    passedPawns[color] = passed;

    // clang-format off
    int midvalue = isolated.popcount() *  EvalConfig.ISOLATED_PAWN_MID_WEIGHT;
    int endvalue = isolated.popcount() *  EvalConfig.ISOLATED_PAWN_END_WEIGHT;
    // No DOUBLED_PAWN_MID_WEIGHT — Texel tuning zeroed it (Phase 9). Only END has signal.
    endvalue    += doubled.popcount() *   EvalConfig.DOUBLED_PAWN_END_WEIGHT;
    // clang-format on

    // Passed pawns: flat bonus + optional rank-based bonus
    {
      Bitboard passedCopy = passed;
      while (passedCopy) {
        const Square psq = passedCopy.popLSB();
        // relative rank: for White rank is 0-based (RANK_1=0), for Black mirror it
        const int relRank = color == WHITE ? static_cast<int>(psq.rank()) : 7 - static_cast<int>(psq.rank());
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

    // Pawn advancement bonus: bonus for non-passed pawns that have advanced to rank 4+ (relative).
    // Advanced pawns control space and restrict enemy pieces even when not passed.
    if (EvalConfig.USE_PAWN_ADVANCE_BONUS) {
      // Non-passed advanced pawns = all pawns minus passed pawns, on ranks 4-7
      Bitboard advancedNonPassed = myPawns & ~passed;
      while (advancedNonPassed) {
        const Square asq  = advancedNonPassed.popLSB();
        const int relRank = color == WHITE ? static_cast<int>(asq.rank()) : 7 - static_cast<int>(asq.rank());
        if (relRank >= 4 && relRank <= 7) {
          midvalue += EvalConfig.PAWN_ADVANCE_MID_BONUS[relRank - 4];
          endvalue += EvalConfig.PAWN_ADVANCE_END_BONUS[relRank - 4];
        }
      }
    }

    // accumulate signed by color
    tmpScore.midgame += static_cast<Value>(midvalue * color.sign());
    tmpScore.endgame += static_cast<Value>(endvalue * color.sign());
    //    LOG__DEBUG(Logger::get().EVAL_LOG, "Raw pawn eval for {} results midvalue = {} and endvalue = {}", color ? "BLACK" : "WHITE", midvalue, endvalue);
  } // color loop

  // Store back only when enabled; get entry pointer for put()
  // ReSharper disable once CppDFAConstantConditions
  // ReSharper disable once CppDFAUnreachableCode
  if (EvalConfig.USE_PAWN_TT && pawnCache && key != 0) {
    // ReSharper disable once CppDFAUnreachableCode
    pawnCache->put(pawnCache->getEntryPtr(key), key, tmpScore, passedPawns[WHITE], passedPawns[BLACK]);
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

inline void Evaluator::knightEval(const Position& p, Score& s, const Color us, Color /*unused*/, const Square sq) {
  const Bitboard attacks = Bitboards::nonSliderAttacks[KNIGHT][sq];
  attackedBy[us] |= attacks;
  attackedByPT[KNIGHT][us] |= attacks;

  if (EvalConfig.USE_KNIGHT_MOBILITY) {
    const Bitboard myOcc = p.getOccupiedBb(us);
    const int mobility   = (attacks & ~myOcc).popcount();

    int mid = mobility * EvalConfig.KNIGHT_MOBILITY_MID_PER_MOVE;
    int end = mobility * EvalConfig.KNIGHT_MOBILITY_END_PER_MOVE;

    if (mobility <= 1) {
      mid += EvalConfig.KNIGHT_LOW_MOBILITY_LEQ1_MID;
      end += EvalConfig.KNIGHT_LOW_MOBILITY_LEQ1_END;
    }
    // No LEQ2 threshold — Texel tuning zeroed it (Phase 9). Only LEQ1 has signal.

    s.midgame += static_cast<Value>(mid * us.sign());
    s.endgame += static_cast<Value>(end * us.sign());
  }

  // Knight outpost: bonus for knight on a square (ranks 4-6 relative) that
  // cannot be attacked by enemy pawns. Extra bonus if supported by own pawn.
  if (EvalConfig.USE_KNIGHT_OUTPOST) {
    const Color them  = ~us;
    const int relRank = us == WHITE ? static_cast<int>(sq.rank()) : 7 - static_cast<int>(sq.rank());
    if (relRank >= 3 && relRank <= 5) { // ranks 4-6 (0-based: 3-5)
      // Check if any enemy pawn can attack this square.
      // Enemy pawn attacks this square if an enemy pawn is on an adjacent file
      // and forward of this square (from enemy's perspective, i.e., behind from ours).
      const Bitboard oppPawns = p.getPieceBb(them, PAWN);
      // passedPawnMask gives us squares in front on same+adjacent files.
      // For outpost detection we only need adjacent files (a pawn on the same file can't attack sq).
      const Bitboard forwardAdjacentFiles = Bitboards::passedPawnMask[us][sq] & Bitboards::neighbourFilesMask[sq];
      const bool canBeAttacked            = (oppPawns & forwardAdjacentFiles) != 0;
      if (!canBeAttacked) {
        // Check if supported by own pawn (own pawn attacks this square)
        const Bitboard myPawns = p.getPieceBb(us, PAWN);
        // A pawn of our color supports sq if it's diagonally behind sq
        const Bitboard pawnSupport = Bitboards::pawnAttacks[them][sq]; // squares where our pawn would be to attack sq
        if (myPawns & pawnSupport) {
          s.midgame += static_cast<Value>(EvalConfig.KNIGHT_OUTPOST_SUPPORTED_MID * us.sign());
          s.endgame += static_cast<Value>(EvalConfig.KNIGHT_OUTPOST_SUPPORTED_END * us.sign());
        }
        else {
          s.midgame += static_cast<Value>(EvalConfig.KNIGHT_OUTPOST_UNSUPPORTED_MID * us.sign());
          s.endgame += static_cast<Value>(EvalConfig.KNIGHT_OUTPOST_UNSUPPORTED_END * us.sign());
        }
      }
    }
  }

  // King safety: count attacks on enemy king zone
  if (EvalConfig.USE_KING_SAFETY_ATTACK) {
    const Color them             = ~us;
    const Bitboard enemyKingZone = Bitboards::nonSliderAttacks[KING][p.getKingSquare(them)];
    if (attacks & enemyKingZone) {
      ++kingAttackCount[them];
      kingAttackWeight[them] += EvalConfig.KING_ATTACK_WEIGHT_KNIGHT;
    }
  }
}

inline void Evaluator::bishopEval(const Position& p, Score& s, const Color us, Color /*unused*/, const Square sq) {
  const Bitboard occupied = p.getOccupiedBb();
  const Bitboard attacks  = Attacks::attacks(BISHOP, sq, occupied);
  attackedBy[us] |= attacks;
  attackedByPT[BISHOP][us] |= attacks;

  // Mobility
  if (EvalConfig.USE_BISHOP_MOBILITY) {
    const Bitboard myOcc = p.getOccupiedBb(us);
    const int mobility   = (attacks & ~myOcc).popcount();

    const int mid = mobility * EvalConfig.BISHOP_MOBILITY_MID_PER_MOVE;
    int end = mobility * EvalConfig.BISHOP_MOBILITY_END_PER_MOVE;

    if (mobility <= 3) {
      // No BISHOP_LOW_MOBILITY_LEQ3_MID — Texel tuning zeroed it (Phase 9). Only END has signal.
      end += EvalConfig.BISHOP_LOW_MOBILITY_LEQ3_END;
    }

    s.midgame += static_cast<Value>(mid * us.sign());
    s.endgame += static_cast<Value>(end * us.sign());
  }

  // Bad bishop per-pawn penalty: REMOVED — Texel tuning zeroed both MID and END
  // across all datasets (Phase 9, 2026-03). USE_BAD_BISHOP toggle also removed.

  // King safety: count attacks on enemy king zone
  if (EvalConfig.USE_KING_SAFETY_ATTACK) {
    const Color them             = ~us;
    const Bitboard enemyKingZone = Bitboards::nonSliderAttacks[KING][p.getKingSquare(them)];
    if (attacks & enemyKingZone) {
      ++kingAttackCount[them];
      kingAttackWeight[them] += EvalConfig.KING_ATTACK_WEIGHT_BISHOP;
    }
  }
}

inline void Evaluator::rookEval(const Position& p, Score& s, const Color us, const Color them, const Square sq) {
  int mid = 0;
  int end = 0;

  const Bitboard occupied = p.getOccupiedBb();
  const Bitboard attacks  = Attacks::attacks(ROOK, sq, occupied);
  attackedBy[us] |= attacks;
  attackedByPT[ROOK][us] |= attacks;

  // Mobility
  if (EvalConfig.USE_ROOK_MOBILITY) {
    const Bitboard myOcc = p.getOccupiedBb(us);
    const int mobility   = (attacks & ~myOcc).popcount();

    mid += mobility * EvalConfig.ROOK_MOBILITY_MID_PER_MOVE;
    end += mobility * EvalConfig.ROOK_MOBILITY_END_PER_MOVE;
    // No ROOK_LOW_MOBILITY_LEQ3 — Texel tuning zeroed both MID and END (Phase 9).
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

  // Rook on 7th rank bonus (relative: rank 7 for White, rank 2 for Black)
  if (EvalConfig.USE_ROOK_7TH_RANK_BONUS) {
    const int relRank = us == WHITE ? static_cast<int>(sq.rank()) : 7 - static_cast<int>(sq.rank());
    if (relRank == 6) { // RANK_7 = 6 (0-based)
      mid += EvalConfig.ROOK_7TH_RANK_MID_BONUS;
      end += EvalConfig.ROOK_7TH_RANK_END_BONUS;
    }
  }

  // Rook behind passed pawn: bonus for rook behind own or enemy passed pawns on the same file.
  // "Behind" means on the opposite side of the pawn's push direction.
  // Uses pre-computed passedPawns[] from evaluate().
  if (EvalConfig.USE_ROOK_BEHIND_PASSER) {
    const Bitboard fileMask = Bitboards::sqToFileBb[sq];

    // Check own passed pawns on same file
    {
      Bitboard ownPassedOnFile = passedPawns[us] & fileMask;
      while (ownPassedOnFile) {
        const Square psq        = ownPassedOnFile.popLSB();
        const Bitboard backward = Bitboards::rays[us == WHITE ? S : N][psq];
        if (backward & Bitboards::sqBb[sq]) {
          mid += EvalConfig.ROOK_BEHIND_PASSER_OWN_MID;
          end += EvalConfig.ROOK_BEHIND_PASSER_OWN_END;
        }
      }
    }

    // Check enemy passed pawns on same file
    {
      Bitboard oppPassedOnFile = passedPawns[them] & fileMask;
      while (oppPassedOnFile) {
        const Square psq        = oppPassedOnFile.popLSB();
        const Bitboard backward = Bitboards::rays[them == WHITE ? S : N][psq];
        if (backward & Bitboards::sqBb[sq]) {
          mid += EvalConfig.ROOK_BEHIND_PASSER_OPP_MID;
          end += EvalConfig.ROOK_BEHIND_PASSER_OPP_END;
        }
      }
    }
  }

  // King safety: count attacks on enemy king zone
  if (EvalConfig.USE_KING_SAFETY_ATTACK) {
    const Bitboard enemyKingZone = Bitboards::nonSliderAttacks[KING][p.getKingSquare(them)];
    if (attacks & enemyKingZone) {
      ++kingAttackCount[them];
      kingAttackWeight[them] += EvalConfig.KING_ATTACK_WEIGHT_ROOK;
    }
  }

  if (mid || end) {
    s.midgame += static_cast<Value>(mid * us.sign());
    s.endgame += static_cast<Value>(end * us.sign());
  }
}

inline void Evaluator::queenEval(const Position& p, Score& s, const Color us, const Color them, const Square sq) {
  int mid = 0;
  int end = 0;

  const Bitboard occupied = p.getOccupiedBb();
  const Bitboard attacks  = Attacks::attacks(QUEEN, sq, occupied);
  attackedBy[us] |= attacks;
  attackedByPT[QUEEN][us] |= attacks;

  // Mobility
  if (EvalConfig.USE_QUEEN_MOBILITY) {
    const Bitboard myOcc = p.getOccupiedBb(us);
    const int mobility   = (attacks & ~myOcc).popcount();

    mid += mobility * EvalConfig.QUEEN_MOBILITY_MID_PER_MOVE;
    end += mobility * EvalConfig.QUEEN_MOBILITY_END_PER_MOVE;
  }

  // Simple tropism towards enemy king
  if (EvalConfig.USE_QUEEN_TROPISM) {
    const Square ksq    = p.getKingSquare(them);
    const int dist      = sq.distanceTo(ksq); // 0..7
    const int closeness = 8 - dist;           // 1..8 (or 8 if dist==0)
    mid += closeness * EvalConfig.QUEEN_TROPISM_MID_PER_STEP;
    end += closeness * EvalConfig.QUEEN_TROPISM_END_PER_STEP;
  }

  // King safety: count attacks on enemy king zone
  if (EvalConfig.USE_KING_SAFETY_ATTACK) {
    const Bitboard enemyKingZone = Bitboards::nonSliderAttacks[KING][p.getKingSquare(them)];
    if (attacks & enemyKingZone) {
      ++kingAttackCount[them];
      kingAttackWeight[them] += EvalConfig.KING_ATTACK_WEIGHT_QUEEN;
    }
  }

  if (mid || end) {
    s.midgame += static_cast<Value>(mid * us.sign());
    s.endgame += static_cast<Value>(end * us.sign());
  }
}

inline void Evaluator::kingEval(const Position& p, Score& s, const Color us) const {
  int mid = 0;
  int end = 0;

  const Color them        = ~us;
  const Square ksq        = p.getKingSquare(us);
  const Bitboard myPawns  = p.getPieceBb(us, PAWN);
  const Bitboard oppPawns = p.getPieceBb(them, PAWN);

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

  // Pawn storm: penalty when opponent's pawns advance toward our king (midgame focus).
  // Checks opponent pawns on king's file and adjacent files; ranks 4+ (relative to us) are threats.
  if (EvalConfig.USE_PAWN_STORM) {
    const Bitboard kingFileMask = Bitboards::sqToFileBb[ksq] | Bitboards::neighbourFilesMask[ksq];
    Bitboard stormPawns         = oppPawns & kingFileMask;
    while (stormPawns) {
      const Square psq = stormPawns.popLSB();
      // Relative rank from our perspective: how close is this pawn to our back rank
      const int relRank = us == WHITE ? static_cast<int>(psq.rank()) : 7 - static_cast<int>(psq.rank());
      // relRank uses "our" perspective: rank 4+ means the pawn is deep in our territory
      // For White: enemy pawns on rank 4 (our rank 4) = rel 3 from Black's view, but from White's
      // perspective it's "enemy approaching". We reverse: the threat rank from our side is (7 - relRank).
      const int threatRank = 7 - relRank; // how advanced toward us (7 = on our back rank)
      if (threatRank >= 4 && threatRank <= 7) {
        mid -= EvalConfig.PAWN_STORM_MID_PENALTY[threatRank - 4];
      }
    }
  }

  // Open file near king: penalty for open/semi-open files on king's file and adjacent files (midgame).
  // An open file near the king allows enemy rooks/queens to penetrate.
  if (EvalConfig.USE_KING_OPEN_FILE) {
    const int kf = ksq.file();
    for (int df = -1; df <= 1; ++df) {
      const int f = kf + df;
      if (f < FILE_A || f > FILE_H) continue;
      const Bitboard fileMask  = FileABB << f;
      const bool ownPawnOnFile = (myPawns & fileMask) != 0;
      const bool oppPawnOnFile = (oppPawns & fileMask) != 0;
      if (!ownPawnOnFile) {
        if (!oppPawnOnFile) {
          mid += EvalConfig.KING_OPEN_FILE_MID_PENALTY; // fully open
        }
        else {
          mid += EvalConfig.KING_SEMIOPEN_FILE_MID_PENALTY; // semi-open
        }
      }
    }
  }

  // King proximity to passed pawns (endgame only).
  // Uses pre-computed passedPawns[] from evaluate().
  // Not cached in PawnTT because king position isn't part of the pawn key.
  if (EvalConfig.USE_KING_PAWN_PROXIMITY) {
    // Bonus for king close to own passed pawns (can escort to promotion)
    {
      Bitboard passed = passedPawns[us];
      while (passed) {
        const Square psq    = passed.popLSB();
        const int closeness = 7 - ksq.distanceTo(psq); // 0..7
        end += closeness * EvalConfig.KING_OWN_PASSED_PROXIMITY_END;
      }
    }

    // Bonus for king close to enemy passed pawns (can block/capture them)
    {
      Bitboard passed = passedPawns[them];
      while (passed) {
        const Square psq    = passed.popLSB();
        const int closeness = 7 - ksq.distanceTo(psq); // 0..7
        end += closeness * EvalConfig.KING_OPP_PASSED_PROXIMITY_END;
      }
    }
  }

  // King safety: apply non-linear penalty based on accumulated attacker weight (midgame only).
  // kingAttackWeight[us] was accumulated in piece evals for pieces attacking THIS king's zone.
  if (EvalConfig.USE_KING_SAFETY_ATTACK && kingAttackCount[us] >= 2) {
    const int idx     = std::min(kingAttackWeight[us], 15);
    const int penalty = EvalConfig.KING_SAFETY_TABLE[idx];
    mid -= penalty; // penalty reduces this king's safety (negative for us)
  }

  // Safe check squares: penalty for squares from which the enemy can give check
  // without the checking piece being captured. Only counts squares that are
  // (1) reachable by actual enemy pieces (attackedBy[them]), (2) not defended
  // by us, and (3) only for piece types the enemy actually has on the board.
  if (EvalConfig.USE_SAFE_CHECK) {
    const Bitboard occupied    = p.getOccupiedBb();
    const Bitboard ourAttacks  = attackedBy[us];
    const Bitboard enemyReach  = attackedBy[them];
    // Safe = enemy can reach it, we don't defend it
    const Bitboard safeMask    = enemyReach & ~ourAttacks;

    // Knight safe checks (only if enemy has knights)
    if (p.getPieceBb(them, KNIGHT)) {
      const Bitboard checkSquares = Bitboards::nonSliderAttacks[KNIGHT][ksq];
      const int safeChecks        = (checkSquares & safeMask).popcount();
      mid += safeChecks * EvalConfig.SAFE_CHECK_KNIGHT_MID;
    }
    // Bishop safe checks: REMOVED — Texel tuning zeroed SAFE_CHECK_BISHOP_MID (Phase 9).
    // Knight/rook/queen safe checks retained.
    // Rook safe checks (only if enemy has rooks)
    if (p.getPieceBb(them, ROOK)) {
      const Bitboard checkSquares = Attacks::attacks(ROOK, ksq, occupied);
      const int safeChecks        = (checkSquares & safeMask).popcount();
      mid += safeChecks * EvalConfig.SAFE_CHECK_ROOK_MID;
    }
    // Queen safe checks (only if enemy has queens)
    if (p.getPieceBb(them, QUEEN)) {
      const Bitboard checkSquares = Attacks::attacks(QUEEN, ksq, occupied);
      const int safeChecks        = (checkSquares & safeMask).popcount();
      mid += safeChecks * EvalConfig.SAFE_CHECK_QUEEN_MID;
    }
  }

  s.midgame += static_cast<Value>(mid * us.sign());
  s.endgame += static_cast<Value>(end * us.sign());
}

inline void Evaluator::threatEval(const Position& p, Score& s, const Color us) const {
  int mid = 0;
  int end = 0;

  const Color them = ~us;

  // Our attack maps by piece type (populated in pre-compute block and piece evals)
  const Bitboard ourPawnAttacks  = attackedByPT[PAWN][us];
  const Bitboard ourMinorAttacks = attackedByPT[KNIGHT][us] | attackedByPT[BISHOP][us];

  // Enemy pieces by type (exclude king — can't be "threatened" in eval sense)
  const Bitboard enemyMinors = p.getPieceBb(them, KNIGHT) | p.getPieceBb(them, BISHOP);
  const Bitboard enemyRooks  = p.getPieceBb(them, ROOK);
  const Bitboard enemyQueens = p.getPieceBb(them, QUEEN);

  // Tier 1: pawn attacks on pieces
  // A pawn attacking any piece is always a meaningful threat regardless of defense.
  {
    const int pawnThreatsMinor = (enemyMinors & ourPawnAttacks).popcount();
    const int pawnThreatsRook  = (enemyRooks & ourPawnAttacks).popcount();
    const int pawnThreatsQueen = (enemyQueens & ourPawnAttacks).popcount();
    mid += pawnThreatsMinor * EvalConfig.THREAT_BY_PAWN_MINOR_MID;
    end += pawnThreatsMinor * EvalConfig.THREAT_BY_PAWN_MINOR_END;
    mid += pawnThreatsRook * EvalConfig.THREAT_BY_PAWN_ROOK_MID;
    end += pawnThreatsRook * EvalConfig.THREAT_BY_PAWN_ROOK_END;
    mid += pawnThreatsQueen * EvalConfig.THREAT_BY_PAWN_QUEEN_MID;
    end += pawnThreatsQueen * EvalConfig.THREAT_BY_PAWN_QUEEN_END;
  }

  // Tier 2: minor piece attacks on major pieces (rook/queen)
  {
    const int minorThreatsRook  = (enemyRooks & ourMinorAttacks).popcount();
    const int minorThreatsQueen = (enemyQueens & ourMinorAttacks).popcount();
    mid += minorThreatsRook * EvalConfig.THREAT_BY_MINOR_ROOK_MID;
    // No THREAT_BY_MINOR_ROOK_END — Texel tuning sign-flipped it (Phase 9). Only MID has signal.
    mid += minorThreatsQueen * EvalConfig.THREAT_BY_MINOR_QUEEN_MID;
    end += minorThreatsQueen * EvalConfig.THREAT_BY_MINOR_QUEEN_END;
  }

  // Tier 3: hanging pieces (attacked by us, not defended by them)
  {
    const Bitboard enemyPieces = p.getOccupiedBb(them) & ~p.getPieceBb(them, KING);
    const Bitboard hanging     = enemyPieces & attackedBy[us] & ~attackedBy[them];
    const int hangingCount     = hanging.popcount();
    mid += hangingCount * EvalConfig.THREAT_HANGING_MID;
    end += hangingCount * EvalConfig.THREAT_HANGING_END;
  }

  if (mid || end) {
    s.midgame += static_cast<Value>(mid * us.sign());
    s.endgame += static_cast<Value>(end * us.sign());
  }
}

inline void Evaluator::coordinationEval(const Position& p, Score& s, const Color us) const {
  int mid = 0;
  int end = 0;

  // Connected rooks: bonus when two rooks are on the same rank or file
  // with no pieces between them.
  if (EvalConfig.USE_CONNECTED_ROOKS) {
    const Bitboard rooks = p.getPieceBb(us, ROOK);
    if (rooks.popcount() >= 2) {
      Bitboard remaining = rooks;
      while (remaining) {
        const Square r1 = remaining.popLSB();
        Bitboard others = remaining; // rooks after r1
        while (others) {
          const Square r2 = others.popLSB();
          if (r1.file() == r2.file() || r1.rank() == r2.rank()) {
            const Bitboard between = Bitboards::intermediateBb[r1][r2];
            if (!(p.getOccupiedBb() & between)) {
              // No CONNECTED_ROOKS_MID_BONUS — Texel tuning sign-flipped it (Phase 9). Only END has signal.
              end += EvalConfig.CONNECTED_ROOKS_END_BONUS;
            }
          }
        }
      }
    }
  }

  // Minor piece connectivity: bonus when a knight/bishop is defended by another minor piece.
  if (EvalConfig.USE_MINOR_CONNECTIVITY) {
    const Bitboard knights    = p.getPieceBb(us, KNIGHT);
    const Bitboard bishops    = p.getPieceBb(us, BISHOP);
    const Bitboard knightAtks = attackedByPT[KNIGHT][us];
    const Bitboard bishopAtks = attackedByPT[BISHOP][us];

    // Count minor pieces defended by another minor piece
    const int connections = (knights & bishopAtks).popcount()
                          + (bishops & knightAtks).popcount()
                          + (knights & knightAtks).popcount()
                          + (bishops & bishopAtks).popcount();
    mid += connections * EvalConfig.MINOR_CONNECTIVITY_MID_BONUS;
    end += connections * EvalConfig.MINOR_CONNECTIVITY_END_BONUS;
  }

  if (mid || end) {
    s.midgame += static_cast<Value>(mid * us.sign());
    s.endgame += static_cast<Value>(end * us.sign());
  }
}
