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
    CONFIG_CONST int TEMPO      = 34;

    // lazy eval
    CONFIG_CONST bool USE_LAZY_EVAL = true;
    CONFIG_CONST int LAZY_THRESHOLD = 700;

    // pawn eval
    CONFIG_CONST bool USE_PAWN_EVAL      = true;
    CONFIG_ESSENTIAL bool USE_PAWN_TT    = true;
    CONFIG_ESSENTIAL int PAWN_TT_SIZE_MB = 16;

    // pawn structure weights
    CONFIG_CONST int ISOLATED_PAWN_MID_WEIGHT  = -10;
    CONFIG_CONST int ISOLATED_PAWN_END_WEIGHT  = -20;
    CONFIG_CONST int DOUBLED_PAWN_MID_WEIGHT   = -10;
    CONFIG_CONST int DOUBLED_PAWN_END_WEIGHT   = -30;
    CONFIG_CONST int PASSED_PAWN_MID_WEIGHT    = 20;
    CONFIG_CONST int PASSED_PAWN_END_WEIGHT    = 40;
    CONFIG_CONST int BLOCKED_PAWN_MID_WEIGHT   = -2;
    CONFIG_CONST int BLOCKED_PAWN_END_WEIGHT   = -20;
    CONFIG_CONST int PHALANX_PAWN_MID_WEIGHT   = 4;
    CONFIG_CONST int PHALANX_PAWN_END_WEIGHT   = 4;
    CONFIG_CONST int SUPPORTED_PAWN_MID_WEIGHT = 10;
    CONFIG_CONST int SUPPORTED_PAWN_END_WEIGHT = 15;

    // piece eval
    CONFIG_CONST bool USE_PIECE_EVAL = true;

    // bishop pair
    CONFIG_CONST bool USE_BISHOP_PAIR_BONUS = true;
    CONFIG_CONST int BISHOP_PAIR_MID_BONUS  = 20;
    CONFIG_CONST int BISHOP_PAIR_END_BONUS  = 20;

    // knight mobility
    CONFIG_CONST bool USE_KNIGHT_MOBILITY         = true;
    CONFIG_CONST int KNIGHT_MOBILITY_MID_PER_MOVE = 3;
    CONFIG_CONST int KNIGHT_MOBILITY_END_PER_MOVE = 2;
    CONFIG_CONST int KNIGHT_LOW_MOBILITY_LEQ1_MID = -6;
    CONFIG_CONST int KNIGHT_LOW_MOBILITY_LEQ1_END = -6;
    CONFIG_CONST int KNIGHT_LOW_MOBILITY_LEQ2_MID = -3;
    CONFIG_CONST int KNIGHT_LOW_MOBILITY_LEQ2_END = -3;

    // bishop mobility
    CONFIG_CONST bool USE_BISHOP_MOBILITY         = true;
    CONFIG_CONST int BISHOP_MOBILITY_MID_PER_MOVE = 2;
    CONFIG_CONST int BISHOP_MOBILITY_END_PER_MOVE = 3;
    CONFIG_CONST int BISHOP_LOW_MOBILITY_LEQ3_MID = -4;
    CONFIG_CONST int BISHOP_LOW_MOBILITY_LEQ3_END = -2;

    // rook mobility and files
    CONFIG_CONST bool USE_ROOK_MOBILITY           = true;
    CONFIG_CONST int ROOK_MOBILITY_MID_PER_MOVE   = 2;
    CONFIG_CONST int ROOK_MOBILITY_END_PER_MOVE   = 2;
    CONFIG_CONST int ROOK_LOW_MOBILITY_LEQ3_MID   = -3;
    CONFIG_CONST int ROOK_LOW_MOBILITY_LEQ3_END   = -3;
    CONFIG_CONST bool USE_ROOK_OPEN_FILE_BONUS    = true;
    CONFIG_CONST int ROOK_OPEN_FILE_MID_BONUS     = 10;
    CONFIG_CONST int ROOK_OPEN_FILE_END_BONUS     = 8;
    CONFIG_CONST int ROOK_SEMIOPEN_FILE_MID_BONUS = 5;
    CONFIG_CONST int ROOK_SEMIOPEN_FILE_END_BONUS = 4;

    // queen
    CONFIG_CONST bool USE_QUEEN_MOBILITY         = true;
    CONFIG_CONST int QUEEN_MOBILITY_MID_PER_MOVE = 1;
    CONFIG_CONST int QUEEN_MOBILITY_END_PER_MOVE = 1;
    CONFIG_CONST bool USE_QUEEN_TROPISM          = true;
    CONFIG_CONST int QUEEN_TROPISM_MID_PER_STEP  = 0;
    CONFIG_CONST int QUEEN_TROPISM_END_PER_STEP  = 1;

    // king
    CONFIG_CONST bool USE_KING_EVAL           = true;
    CONFIG_CONST bool USE_KING_SAFETY_SHIELD  = true;
    CONFIG_CONST int KING_SHIELD_MID_PER_PAWN = 5;
    CONFIG_CONST int KING_SHIELD_END_PER_PAWN = 0;

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
