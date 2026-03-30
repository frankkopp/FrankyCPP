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

#ifndef FRANKYCPP_EVALCONFIGDATA_H
#define FRANKYCPP_EVALCONFIGDATA_H

#include <array>
#include <set>
#include <string>

#include "config/ConfigMode.h"
#include <yaml-cpp/yaml.h>

namespace config {

  // Configuration struct for Evaluation
  // All members have default values which are used as fallback
  // if no YAML config file is found or a value is missing in the file.
  //
  // CONFIG_ESSENTIAL members are always mutable instance members (runtime-changeable in all builds).
  // CONFIG_CONST members are mutable instance members in development and static constexpr in production.
  // In production, CONFIG_CONST members become compile-time constants enabling dead-code elimination.
  struct EvalConfigData {
    // Debug
    CONFIG_ESSENTIAL std::string EVAL_CONFIG_SOURCE = "fallback";

    // master toggles
    CONFIG_CONST bool USE_MATERIAL   = true;
    CONFIG_CONST bool USE_POSITIONAL = true;

    // tempo
    CONFIG_CONST bool USE_TEMPO = true;
    CONFIG_CONST int TEMPO      = 3; // was: 4 — Phase 8 retune

    // lazy eval
    CONFIG_CONST bool USE_LAZY_EVAL = true;
    CONFIG_CONST int LAZY_THRESHOLD = 706; // was: 700 — Texel tuned

    // pawn eval
    CONFIG_CONST bool USE_PAWN_EVAL      = true;
    CONFIG_ESSENTIAL bool USE_PAWN_TT    = true;
    CONFIG_ESSENTIAL int PAWN_TT_SIZE_MB = 16;

    // pawn structure weights
    CONFIG_CONST int ISOLATED_PAWN_MID_WEIGHT  = -5;  // was: -7 — Phase 8 retune
    CONFIG_CONST int ISOLATED_PAWN_END_WEIGHT  = -7;  // was: -20
    CONFIG_CONST int DOUBLED_PAWN_MID_WEIGHT   = 0;   // was: -1 — zeroed by Phase 8 retune (removal candidate)
    CONFIG_CONST int DOUBLED_PAWN_END_WEIGHT   = -20; // was: -19 — Phase 8 retune
    CONFIG_CONST int PASSED_PAWN_MID_WEIGHT    = 0;   // was: 20 — zeroed by tuner
    CONFIG_CONST int PASSED_PAWN_END_WEIGHT    = 60;  // was: 69 — Phase 8 retune

    // Rank-based passed pawn bonus (indexed by relative rank 2..7, so array index 0..5).
    // Relative rank: for White = actual rank, for Black = 9 - actual rank.
    // When enabled, the rank bonus is added ON TOP of the flat PASSED_PAWN_*_WEIGHT.
    CONFIG_CONST bool USE_PASSED_PAWN_RANK_BONUS = true;
    // Mid/End bonus per relative rank: {rank2, rank3, rank4, rank5, rank6, rank7}
    CONFIG_CONST std::array<int, 6> PASSED_PAWN_RANK_MID_BONUS = {-55, -36, 8, 70, 70, 70};   // was: {-50,-33,12,65,70,70}
    CONFIG_CONST std::array<int, 6> PASSED_PAWN_RANK_END_BONUS = {26, 48, 57, 84, 100, 120};   // was: {18,41,51,84,101,120}
    CONFIG_CONST int BLOCKED_PAWN_MID_WEIGHT   = 0;   // was: -2 — zeroed by tuner
    CONFIG_CONST int BLOCKED_PAWN_END_WEIGHT   = -10; // was: -20
    CONFIG_CONST int PHALANX_PAWN_MID_WEIGHT   = 4;   // was: 5 — Phase 8 retune
    CONFIG_CONST int PHALANX_PAWN_END_WEIGHT   = 8;   // was: 9 — Phase 8 retune
    CONFIG_CONST int SUPPORTED_PAWN_MID_WEIGHT = 20;  // was: 21 — Phase 8 retune
    CONFIG_CONST int SUPPORTED_PAWN_END_WEIGHT = 0;   // was: 15 — zeroed by tuner

    // pawn advancement bonus: bonus for non-passed pawns that have advanced to rank 4+
    // Indexed by (relativeRank - 4): {rank4, rank5, rank6, rank7}
    CONFIG_CONST bool USE_PAWN_ADVANCE_BONUS = true;
    CONFIG_CONST std::array<int, 4> PAWN_ADVANCE_MID_BONUS = {6, 12, 12, 25};  // was: {11,12,12,25}
    CONFIG_CONST std::array<int, 4> PAWN_ADVANCE_END_BONUS = {0, 1, 18, 35};   // was: {0,4,18,35} — [0] zeroed: sign-flip

    // piece eval
    CONFIG_CONST bool USE_PIECE_EVAL = true;

    // bishop pair
    CONFIG_CONST bool USE_BISHOP_PAIR_BONUS = true;
    CONFIG_CONST int BISHOP_PAIR_MID_BONUS  = 56; // was: 52 — Phase 8 retune
    CONFIG_CONST int BISHOP_PAIR_END_BONUS  = 63; // was: 66 — Phase 8 retune

    // knight mobility
    CONFIG_CONST bool USE_KNIGHT_MOBILITY         = true;
    CONFIG_CONST int KNIGHT_MOBILITY_MID_PER_MOVE = 3;
    CONFIG_CONST int KNIGHT_MOBILITY_END_PER_MOVE = 4;   // was: 2
    CONFIG_CONST int KNIGHT_LOW_MOBILITY_LEQ1_MID = 0;   // was: -6 — zeroed by tuner
    CONFIG_CONST int KNIGHT_LOW_MOBILITY_LEQ1_END = -6;  // was: -10 — Phase 8 retune
    CONFIG_CONST int KNIGHT_LOW_MOBILITY_LEQ2_MID = 0;   // was: -3 — zeroed by tuner
    CONFIG_CONST int KNIGHT_LOW_MOBILITY_LEQ2_END = 0;   // was: -3 — zeroed by tuner

    // knight outpost: bonus for knight on a square that cannot be attacked by enemy pawns
    // (ranks 4-6 relative, center files preferred)
    CONFIG_CONST bool USE_KNIGHT_OUTPOST            = true;
    CONFIG_CONST int KNIGHT_OUTPOST_SUPPORTED_MID   = 37; // was: 39 — Phase 8 retune
    CONFIG_CONST int KNIGHT_OUTPOST_SUPPORTED_END   = 19; // was: 21 — Phase 8 retune
    CONFIG_CONST int KNIGHT_OUTPOST_UNSUPPORTED_MID = 38; // was: 39 — Phase 8 retune
    CONFIG_CONST int KNIGHT_OUTPOST_UNSUPPORTED_END = 2;  // was: 5 — Phase 8 retune

    // bishop mobility
    CONFIG_CONST bool USE_BISHOP_MOBILITY         = true;
    CONFIG_CONST int BISHOP_MOBILITY_MID_PER_MOVE = 1;   // was: 2 — Phase 8 retune
    CONFIG_CONST int BISHOP_MOBILITY_END_PER_MOVE = 6;   // was: 3
    CONFIG_CONST int BISHOP_LOW_MOBILITY_LEQ3_MID = 0;   // was: -4 — zeroed by tuner
    CONFIG_CONST int BISHOP_LOW_MOBILITY_LEQ3_END = -26; // was: -24 — Phase 8 retune

    // bad bishop: penalty when own pawns are on the same color squares as the bishop
    CONFIG_CONST bool USE_BAD_BISHOP          = true;
    CONFIG_CONST int BAD_BISHOP_PER_PAWN_MID  = 0; // was: -3 — zeroed by tuner (removal candidate)
    CONFIG_CONST int BAD_BISHOP_PER_PAWN_END  = 0; // was: -5 — zeroed by tuner (removal candidate)

    // rook mobility and files
    CONFIG_CONST bool USE_ROOK_MOBILITY           = true;
    CONFIG_CONST int ROOK_MOBILITY_MID_PER_MOVE   = 1;  // was: 2 — Phase 8 retune
    CONFIG_CONST int ROOK_MOBILITY_END_PER_MOVE   = 9;  // was: 2
    CONFIG_CONST int ROOK_LOW_MOBILITY_LEQ3_MID   = 0;  // was: -3 — zeroed by tuner
    CONFIG_CONST int ROOK_LOW_MOBILITY_LEQ3_END   = 0;  // was: -3 — zeroed by tuner
    CONFIG_CONST bool USE_ROOK_OPEN_FILE_BONUS    = true;
    CONFIG_CONST int ROOK_OPEN_FILE_MID_BONUS     = 26; // was: 24 — Phase 8 retune
    CONFIG_CONST int ROOK_OPEN_FILE_END_BONUS     = 7;  // was: 9 — Phase 8 retune
    CONFIG_CONST int ROOK_SEMIOPEN_FILE_MID_BONUS = 14; // was: 11 — Phase 8 retune
    CONFIG_CONST int ROOK_SEMIOPEN_FILE_END_BONUS = 14; // was: 4

    // rook on 7th rank (relative to its color)
    CONFIG_CONST bool USE_ROOK_7TH_RANK_BONUS = true;
    CONFIG_CONST int ROOK_7TH_RANK_MID_BONUS  = 42; // was: 41 — Phase 8 retune
    CONFIG_CONST int ROOK_7TH_RANK_END_BONUS  = 7;  // was: 9 — Phase 8 retune

    // rook behind passed pawn: bonus for rook behind own or enemy passed pawns
    CONFIG_CONST bool USE_ROOK_BEHIND_PASSER     = true;
    CONFIG_CONST int ROOK_BEHIND_PASSER_OWN_MID  = 1;  // was: 10
    CONFIG_CONST int ROOK_BEHIND_PASSER_OWN_END  = 22; // was: 24 — Phase 8 retune
    CONFIG_CONST int ROOK_BEHIND_PASSER_OPP_MID  = 18; // was: 13 — Phase 8 retune
    CONFIG_CONST int ROOK_BEHIND_PASSER_OPP_END  = 17; // was: 19 — Phase 8 retune

    // queen
    CONFIG_CONST bool USE_QUEEN_MOBILITY         = true;
    CONFIG_CONST int QUEEN_MOBILITY_MID_PER_MOVE = 1;
    CONFIG_CONST int QUEEN_MOBILITY_END_PER_MOVE = 10; // was: 11 — Phase 8 retune
    CONFIG_CONST bool USE_QUEEN_TROPISM          = true;
    CONFIG_CONST int QUEEN_TROPISM_MID_PER_STEP  = 1;  // was: 2 — Phase 8 retune
    CONFIG_CONST int QUEEN_TROPISM_END_PER_STEP  = 20; // was: 1

    // king
    CONFIG_CONST bool USE_KING_EVAL           = true;
    CONFIG_CONST bool USE_KING_SAFETY_SHIELD  = true;
    CONFIG_CONST int KING_SHIELD_MID_PER_PAWN = 0; // was: 5 — zeroed by tuner
    CONFIG_CONST int KING_SHIELD_END_PER_PAWN = 0;

    // king-pawn proximity in endgame
    // Bonus for king close to own passed pawns, bonus for king close to enemy passed pawns (defending).
    CONFIG_CONST bool USE_KING_PAWN_PROXIMITY       = true;
    CONFIG_CONST int KING_OWN_PASSED_PROXIMITY_END  = 4;  // was: 5 — Phase 8 retune
    CONFIG_CONST int KING_OPP_PASSED_PROXIMITY_END  = 20; // was: 3

    // king safety: attack evaluation (midgame only)
    // Counts attacker pieces on the enemy king zone and applies a non-linear penalty.
    CONFIG_CONST bool USE_KING_SAFETY_ATTACK       = true;
    CONFIG_CONST int KING_ATTACK_WEIGHT_KNIGHT     = 2;
    CONFIG_CONST int KING_ATTACK_WEIGHT_BISHOP     = 2;
    CONFIG_CONST int KING_ATTACK_WEIGHT_ROOK       = 3;
    CONFIG_CONST int KING_ATTACK_WEIGHT_QUEEN      = 3; // was: 4
    // Non-linear penalty table indexed by total attack weight (clamped to 0..15)
    // Texel tuned: sharper curve — low danger decreased, high danger increased
    CONFIG_CONST std::array<int, 16> KING_SAFETY_TABLE = {
      0, 0, 5, 15, 18, 46, 50, 112, 134, 134, 225, 316, 358, 380, 380, 400};
    // was: {0,0,5,15,19,47,51,116,135,135,227,307,348,380,380,400} — Phase 8 retune

    // pawn storm: penalty when opponent pawns advance toward our king (midgame only)
    // Indexed by (relativeRank - 4): {rank4, rank5, rank6, rank7}
    CONFIG_CONST bool USE_PAWN_STORM = true;
    CONFIG_CONST std::array<int, 4> PAWN_STORM_MID_PENALTY = {0, 10, 10, 50}; // was: {0,15,15,50} — [0] zeroed: sign-flip

    // open file near king: penalty for open/semi-open files adjacent to king (midgame only)
    CONFIG_CONST bool USE_KING_OPEN_FILE            = true;
    CONFIG_CONST int KING_OPEN_FILE_MID_PENALTY     = -25; // was: -24 — Phase 8 retune
    CONFIG_CONST int KING_SEMIOPEN_FILE_MID_PENALTY = 0;   // was: -10 — zeroed by tuner

    // safe check squares: penalty for squares from which enemy can give check without being captured
    // Per-piece-type penalties (midgame only, negative values = penalty for the defending side)
    CONFIG_CONST bool USE_SAFE_CHECK          = true;
    CONFIG_CONST int SAFE_CHECK_KNIGHT_MID    = -15; // was: -10
    CONFIG_CONST int SAFE_CHECK_BISHOP_MID    = 0;   // was: -8 — zeroed by tuner (removal candidate)
    CONFIG_CONST int SAFE_CHECK_ROOK_MID      = -16; // was: -17 — Phase 8 retune
    CONFIG_CONST int SAFE_CHECK_QUEEN_MID     = -11; // was: -15

    // threat evaluation: bonus for pieces attacked by lesser-value pieces and hanging pieces
    // Three tiers: (1) pawn attacks on pieces, (2) minor attacks on majors, (3) hanging pieces
    CONFIG_CONST bool USE_THREAT_EVAL          = true;
    // Tier 1: pawn attacks on pieces (per victim piece type)
    CONFIG_CONST int THREAT_BY_PAWN_MINOR_MID  = 41;  // was: 44 — Phase 8 retune
    CONFIG_CONST int THREAT_BY_PAWN_MINOR_END  = 13;  // was: 12 — Phase 8 retune
    CONFIG_CONST int THREAT_BY_PAWN_ROOK_MID   = 45;  // was: 48 — Phase 8 retune
    CONFIG_CONST int THREAT_BY_PAWN_ROOK_END   = 16;  // was: 18 — Phase 8 retune
    CONFIG_CONST int THREAT_BY_PAWN_QUEEN_MID  = 28;  // was: 30 — Phase 8 retune
    CONFIG_CONST int THREAT_BY_PAWN_QUEEN_END  = 3;   // was: 10 — Phase 8 retune
    // Tier 2: minor piece (knight/bishop) attacks on major pieces (rook/queen)
    CONFIG_CONST int THREAT_BY_MINOR_ROOK_MID  = 63;  // was: 55 — Phase 8 retune
    CONFIG_CONST int THREAT_BY_MINOR_ROOK_END  = 0;   // was: 0 — zeroed: sign-flip (removal candidate)
    CONFIG_CONST int THREAT_BY_MINOR_QUEEN_MID = 26;  // was: 28 — Phase 8 retune
    CONFIG_CONST int THREAT_BY_MINOR_QUEEN_END = 18;  // was: 20 — Phase 8 retune
    // Tier 3: hanging pieces (attacked by us, not defended by them)
    CONFIG_CONST int THREAT_HANGING_MID        = 6;   // was: 7 — Phase 8 retune
    CONFIG_CONST int THREAT_HANGING_END        = 42;  // was: 43 — Phase 8 retune

    // space evaluation: bonus for safe squares behind own pawn chain (midgame-weighted)
    // Space = squares on ranks 2-4 (relative) behind own pawns, not attacked by enemy pawns.
    // DISABLED: Confirmed dead by Texel tuner — sign-flipped in all 3 datasets.
    CONFIG_CONST bool USE_SPACE_EVAL  = false;
    CONFIG_CONST int SPACE_BONUS_MID  = 0; // was: 3 — zeroed: sign-flip (removal candidate)
    CONFIG_CONST int SPACE_BONUS_END  = 0; // was: 1 — zeroed: sign-flip (removal candidate)

    // connected rooks: bonus when two rooks are on the same rank or file with no pieces between
    // ENABLED: Texel tuner found strong endgame bonus (+46); MID sign-flipped and zeroed.
    CONFIG_CONST bool USE_CONNECTED_ROOKS      = true;  // was: false — enabled by Texel tuning
    CONFIG_CONST int CONNECTED_ROOKS_MID_BONUS = 0;     // was: 8 — zeroed: sign-flip (removal candidate)
    CONFIG_CONST int CONNECTED_ROOKS_END_BONUS = 39;    // was: 46 — Phase 8 retune

    // minor piece connectivity: bonus when a knight/bishop is defended by another minor piece
    // ENABLED: Texel tuner found positive values for both phases.
    CONFIG_CONST bool USE_MINOR_CONNECTIVITY      = true; // was: false — enabled by Texel tuning
    CONFIG_CONST int MINOR_CONNECTIVITY_MID_BONUS = 1;    // was: 4
    CONFIG_CONST int MINOR_CONNECTIVITY_END_BONUS = 3;    // was: 5 — Phase 8 retune

    CONFIG_CONST bool USE_GAMEPHASE_VALUE = true;

    std::string str() const;
  };

  // Forward declaration for parseYamlConfig
  std::set<std::string> parseYamlConfig(
    const YAML::Node& node,
    EvalConfigData& eval,
    bool warnUnknown);

} // namespace config

template<>
struct YAML::convert<config::EvalConfigData> {
  static Node encode(const config::EvalConfigData&) {
    // YAML encoding not used - config files are manually maintained.
    // Use generateConfigString() for human-readable output.
    return {};
  }

  static bool decode(const Node& n, config::EvalConfigData& c) {
    if (!n || !n.IsMap()) return false;
    config::parseYamlConfig(n, c, /* warnUnknown= */ true);
    return true;
  }
}; // namespace YAML

#endif // FRANKYCPP_EVALCONFIGDATA_H
