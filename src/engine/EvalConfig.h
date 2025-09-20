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

#ifndef FRANKYCPP_EVALCONFIG_H
#define FRANKYCPP_EVALCONFIG_H

#include "types/types.h"

namespace EvalConfig {

  inline bool USE_MATERIAL   = true;
  inline bool USE_POSITIONAL = true;

  inline bool USE_TEMPO = true;
  inline int TEMPO      = 34;

  inline bool USE_LAZY_EVAL  = true;
  inline auto LAZY_THRESHOLD = Value{700};

  inline bool USE_PAWN_EVAL  = true;
  inline bool USE_PAWN_TT    = true;
  inline int PAWN_TT_SIZE_MB = 64;

  inline int ISOLATED_PAWN_MID_WEIGHT  = -10;
  inline int ISOLATED_PAWN_END_WEIGHT  = -20;
  inline int DOUBLED_PAWN_MID_WEIGHT   = -10;
  inline int DOUBLED_PAWN_END_WEIGHT   = -30;
  inline int PASSED_PAWN_MID_WEIGHT    = 20;
  inline int PASSED_PAWN_END_WEIGHT    = 40;
  inline int BLOCKED_PAWN_MID_WEIGHT   = -2;
  inline int BLOCKED_PAWN_END_WEIGHT   = -20;
  inline int PHALANX_PAWN_MID_WEIGHT   = 4;
  inline int PHALANX_PAWN_END_WEIGHT   = 4;
  inline int SUPPORTED_PAWN_MID_WEIGHT = 10;
  inline int SUPPORTED_PAWN_END_WEIGHT = 15;

  inline bool USE_PIECE_EVAL        = true;
  inline auto BISHOP_PAIR_MID_BONUS = Value{20};
  inline auto BISHOP_PAIR_END_BONUS = Value{20};

  // Tier 1: Knight mobility configuration
  inline bool USE_KNIGHT_MOBILITY = true;
  // Linear mobility weights per available target square (excludes own-occupied)
  inline int KNIGHT_MOBILITY_MID_PER_MOVE = 3;
  inline int KNIGHT_MOBILITY_END_PER_MOVE = 2;
  // Low-mobility penalties applied in addition to linear term
  inline int KNIGHT_LOW_MOBILITY_LEQ1_MID = -6;// mobility <= 1
  inline int KNIGHT_LOW_MOBILITY_LEQ1_END = -6;
  inline int KNIGHT_LOW_MOBILITY_LEQ2_MID = -3;// mobility <= 2 (but > 1)
  inline int KNIGHT_LOW_MOBILITY_LEQ2_END = -3;

  // Tier 1: Bishop mobility configuration
  inline bool USE_BISHOP_MOBILITY         = true;
  inline int BISHOP_MOBILITY_MID_PER_MOVE = 2;
  inline int BISHOP_MOBILITY_END_PER_MOVE = 3; // bishops stronger in endgame
  inline int BISHOP_LOW_MOBILITY_LEQ3_MID = -4;// mobility <= 3
  inline int BISHOP_LOW_MOBILITY_LEQ3_END = -2;

  // Tier 1: Rook mobility and file presence
  inline bool USE_ROOK_MOBILITY           = true;
  inline int ROOK_MOBILITY_MID_PER_MOVE   = 2;
  inline int ROOK_MOBILITY_END_PER_MOVE   = 2;
  inline int ROOK_LOW_MOBILITY_LEQ3_MID   = -3;
  inline int ROOK_LOW_MOBILITY_LEQ3_END   = -3;
  inline bool USE_ROOK_OPEN_FILE_BONUS    = true;
  inline int ROOK_OPEN_FILE_MID_BONUS     = 10;// no pawns on file
  inline int ROOK_OPEN_FILE_END_BONUS     = 8;
  inline int ROOK_SEMIOPEN_FILE_MID_BONUS = 5;// no own pawn on file
  inline int ROOK_SEMIOPEN_FILE_END_BONUS = 4;

  // Tier 1: Queen mobility and simple king tropism
  inline bool USE_QUEEN_MOBILITY         = true;
  inline int QUEEN_MOBILITY_MID_PER_MOVE = 1;// small to avoid overvaluing
  inline int QUEEN_MOBILITY_END_PER_MOVE = 1;
  inline bool USE_QUEEN_TROPISM          = true;// closer to enemy king gives small bonus
  inline int QUEEN_TROPISM_MID_PER_STEP  = 0;   // disabled in midgame by default
  inline int QUEEN_TROPISM_END_PER_STEP  = 1;   // (8 - distance) * weight

  // King evaluation
  inline bool USE_KING_EVAL             = true;
  inline bool USE_KING_SAFETY_SHIELD    = true;// pawn shield in front of king (midgame)
  inline int KING_SHIELD_MID_PER_PAWN   = 5;   // per pawn in the shield zone
  inline int KING_SHIELD_END_PER_PAWN   = 0;   // no effect in endgame

  inline bool USE_GAMEPHASE_VALUE = true;
}// namespace EvalConfig

#endif// FRANKYCPP_EVALCONFIG_H
