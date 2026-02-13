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

#include <yaml-cpp/yaml.h>

// Configuration struct for Evaluation
// All members have default values which are used as fallback
// if no YAML config file is found or a value is missing in the file.
struct EvalConfigData {
    // Debug
    std::string EVAL_CONFIG_SOURCE = "fallback";

    // master toggles
    bool USE_MATERIAL   = true;
    bool USE_POSITIONAL = true;

    // tempo
    bool USE_TEMPO = true;
    int TEMPO      = 34;

    // lazy eval
    bool USE_LAZY_EVAL = true;
    int LAZY_THRESHOLD = 700;

    // pawn eval
    bool USE_PAWN_EVAL  = true;
    bool USE_PAWN_TT    = true;
    int PAWN_TT_SIZE_MB = 64;

    // pawn structure weights
    int ISOLATED_PAWN_MID_WEIGHT  = -10;
    int ISOLATED_PAWN_END_WEIGHT  = -20;
    int DOUBLED_PAWN_MID_WEIGHT   = -10;
    int DOUBLED_PAWN_END_WEIGHT   = -30;
    int PASSED_PAWN_MID_WEIGHT    = 20;
    int PASSED_PAWN_END_WEIGHT    = 40;
    int BLOCKED_PAWN_MID_WEIGHT   = -2;
    int BLOCKED_PAWN_END_WEIGHT   = -20;
    int PHALANX_PAWN_MID_WEIGHT   = 4;
    int PHALANX_PAWN_END_WEIGHT   = 4;
    int SUPPORTED_PAWN_MID_WEIGHT = 10;
    int SUPPORTED_PAWN_END_WEIGHT = 15;

    // piece eval
    bool USE_PIECE_EVAL = true;

    // bishop pair
    bool USE_BISHOP_PAIR_BONUS = true;
    int BISHOP_PAIR_MID_BONUS  = 20;
    int BISHOP_PAIR_END_BONUS  = 20;

    // knight mobility
    bool USE_KNIGHT_MOBILITY         = true;
    int KNIGHT_MOBILITY_MID_PER_MOVE = 3;
    int KNIGHT_MOBILITY_END_PER_MOVE = 2;
    int KNIGHT_LOW_MOBILITY_LEQ1_MID = -6;
    int KNIGHT_LOW_MOBILITY_LEQ1_END = -6;
    int KNIGHT_LOW_MOBILITY_LEQ2_MID = -3;
    int KNIGHT_LOW_MOBILITY_LEQ2_END = -3;

    // bishop mobility
    bool USE_BISHOP_MOBILITY         = true;
    int BISHOP_MOBILITY_MID_PER_MOVE = 2;
    int BISHOP_MOBILITY_END_PER_MOVE = 3;
    int BISHOP_LOW_MOBILITY_LEQ3_MID = -4;
    int BISHOP_LOW_MOBILITY_LEQ3_END = -2;

    // rook mobility and files
    bool USE_ROOK_MOBILITY           = true;
    int ROOK_MOBILITY_MID_PER_MOVE   = 2;
    int ROOK_MOBILITY_END_PER_MOVE   = 2;
    int ROOK_LOW_MOBILITY_LEQ3_MID   = -3;
    int ROOK_LOW_MOBILITY_LEQ3_END   = -3;
    bool USE_ROOK_OPEN_FILE_BONUS    = true;
    int ROOK_OPEN_FILE_MID_BONUS     = 10;
    int ROOK_OPEN_FILE_END_BONUS     = 8;
    int ROOK_SEMIOPEN_FILE_MID_BONUS = 5;
    int ROOK_SEMIOPEN_FILE_END_BONUS = 4;

    // queen
    bool USE_QUEEN_MOBILITY         = true;
    int QUEEN_MOBILITY_MID_PER_MOVE = 1;
    int QUEEN_MOBILITY_END_PER_MOVE = 1;
    bool USE_QUEEN_TROPISM          = true;
    int QUEEN_TROPISM_MID_PER_STEP  = 0;
    int QUEEN_TROPISM_END_PER_STEP  = 1;

    // king
    bool USE_KING_EVAL           = true;
    bool USE_KING_SAFETY_SHIELD  = true;
    int KING_SHIELD_MID_PER_PAWN = 5;
    int KING_SHIELD_END_PER_PAWN = 0;

    bool USE_GAMEPHASE_VALUE = true;

    std::string str() const;
};

// Forward declaration for parseYamlConfig
std::set<std::string> parseYamlConfig(
    const YAML::Node& node,
    EvalConfigData& eval,
    bool warnUnknown);

template<>
struct YAML::convert<EvalConfigData> {
  static Node encode(const EvalConfigData&) {
    // YAML encoding not used - config files are manually maintained.
    // Use generateConfigString() for human-readable output.
    return {};
  }

  static bool decode(const Node& n, EvalConfigData& c) {
    if (!n || !n.IsMap()) return false;
    parseYamlConfig(n, c, /* warnUnknown= */ true);
    return true;
  }
};// namespace YAML

#endif // FRANKYCPP_EVALCONFIGDATA_H
