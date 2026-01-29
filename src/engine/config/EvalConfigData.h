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

#include <sstream>
#include <string>
#include <unordered_set>

#include "common/Logging.h"
#include "engine/config/YamlHelpers.h"
#include <yaml-cpp/yaml.h>

namespace engine::config {

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

    std::string str() const {
      std::ostringstream os;
      os << "USE_MATERIAL: " << USE_MATERIAL << '\n'
         << "USE_POSITIONAL: " << USE_POSITIONAL << '\n'
         << "USE_TEMPO: " << USE_TEMPO << '\n'
         << "TEMPO: " << TEMPO << '\n'
         << "USE_PAWN_EVAL: " << USE_PAWN_EVAL << '\n';
      return os.str();
    }
  };

  namespace detail {
    inline void warnUnknownKey(const std::string& key) {
      LOG__WARN(Logger::get().EVAL_LOG, "Unknown key in Eval config: {}", key);
    }
  }// namespace detail

}// namespace engine::config

// ReSharper disable once CppRedundantNamespaceDefinition
namespace YAML {

  template<>
  struct convert<engine::config::EvalConfigData> {
    static Node encode(const engine::config::EvalConfigData& c) {
      Node n;
      n["EVAL_CONFIG_SOURCE"]            = c.EVAL_CONFIG_SOURCE;
      n["USE_MATERIAL"]                 = c.USE_MATERIAL;
      n["USE_POSITIONAL"]               = c.USE_POSITIONAL;
      n["USE_TEMPO"]                    = c.USE_TEMPO;
      n["TEMPO"]                        = c.TEMPO;
      n["USE_LAZY_EVAL"]                = c.USE_LAZY_EVAL;
      n["LAZY_THRESHOLD"]               = c.LAZY_THRESHOLD;
      n["USE_PAWN_EVAL"]                = c.USE_PAWN_EVAL;
      n["USE_PAWN_TT"]                  = c.USE_PAWN_TT;
      n["PAWN_TT_SIZE_MB"]              = c.PAWN_TT_SIZE_MB;
      n["ISOLATED_PAWN_MID_WEIGHT"]     = c.ISOLATED_PAWN_MID_WEIGHT;
      n["ISOLATED_PAWN_END_WEIGHT"]     = c.ISOLATED_PAWN_END_WEIGHT;
      n["DOUBLED_PAWN_MID_WEIGHT"]      = c.DOUBLED_PAWN_MID_WEIGHT;
      n["DOUBLED_PAWN_END_WEIGHT"]      = c.DOUBLED_PAWN_END_WEIGHT;
      n["PASSED_PAWN_MID_WEIGHT"]       = c.PASSED_PAWN_MID_WEIGHT;
      n["PASSED_PAWN_END_WEIGHT"]       = c.PASSED_PAWN_END_WEIGHT;
      n["BLOCKED_PAWN_MID_WEIGHT"]      = c.BLOCKED_PAWN_MID_WEIGHT;
      n["BLOCKED_PAWN_END_WEIGHT"]      = c.BLOCKED_PAWN_END_WEIGHT;
      n["PHALANX_PAWN_MID_WEIGHT"]      = c.PHALANX_PAWN_MID_WEIGHT;
      n["PHALANX_PAWN_END_WEIGHT"]      = c.PHALANX_PAWN_END_WEIGHT;
      n["SUPPORTED_PAWN_MID_WEIGHT"]    = c.SUPPORTED_PAWN_MID_WEIGHT;
      n["SUPPORTED_PAWN_END_WEIGHT"]    = c.SUPPORTED_PAWN_END_WEIGHT;
      n["USE_PIECE_EVAL"]               = c.USE_PIECE_EVAL;
      n["USE_BISHOP_PAIR_BONUS"]        = c.USE_BISHOP_PAIR_BONUS;
      n["BISHOP_PAIR_MID_BONUS"]        = c.BISHOP_PAIR_MID_BONUS;
      n["BISHOP_PAIR_END_BONUS"]        = c.BISHOP_PAIR_END_BONUS;
      n["USE_KNIGHT_MOBILITY"]          = c.USE_KNIGHT_MOBILITY;
      n["KNIGHT_MOBILITY_MID_PER_MOVE"] = c.KNIGHT_MOBILITY_MID_PER_MOVE;
      n["KNIGHT_MOBILITY_END_PER_MOVE"] = c.KNIGHT_MOBILITY_END_PER_MOVE;
      n["KNIGHT_LOW_MOBILITY_LEQ1_MID"] = c.KNIGHT_LOW_MOBILITY_LEQ1_MID;
      n["KNIGHT_LOW_MOBILITY_LEQ1_END"] = c.KNIGHT_LOW_MOBILITY_LEQ1_END;
      n["KNIGHT_LOW_MOBILITY_LEQ2_MID"] = c.KNIGHT_LOW_MOBILITY_LEQ2_MID;
      n["KNIGHT_LOW_MOBILITY_LEQ2_END"] = c.KNIGHT_LOW_MOBILITY_LEQ2_END;
      n["USE_BISHOP_MOBILITY"]          = c.USE_BISHOP_MOBILITY;
      n["BISHOP_MOBILITY_MID_PER_MOVE"] = c.BISHOP_MOBILITY_MID_PER_MOVE;
      n["BISHOP_MOBILITY_END_PER_MOVE"] = c.BISHOP_MOBILITY_END_PER_MOVE;
      n["BISHOP_LOW_MOBILITY_LEQ3_MID"] = c.BISHOP_LOW_MOBILITY_LEQ3_MID;
      n["BISHOP_LOW_MOBILITY_LEQ3_END"] = c.BISHOP_LOW_MOBILITY_LEQ3_END;
      n["USE_ROOK_MOBILITY"]            = c.USE_ROOK_MOBILITY;
      n["ROOK_MOBILITY_MID_PER_MOVE"]   = c.ROOK_MOBILITY_MID_PER_MOVE;
      n["ROOK_MOBILITY_END_PER_MOVE"]   = c.ROOK_MOBILITY_END_PER_MOVE;
      n["ROOK_LOW_MOBILITY_LEQ3_MID"]   = c.ROOK_LOW_MOBILITY_LEQ3_MID;
      n["ROOK_LOW_MOBILITY_LEQ3_END"]   = c.ROOK_LOW_MOBILITY_LEQ3_END;
      n["USE_ROOK_OPEN_FILE_BONUS"]     = c.USE_ROOK_OPEN_FILE_BONUS;
      n["ROOK_OPEN_FILE_MID_BONUS"]     = c.ROOK_OPEN_FILE_MID_BONUS;
      n["ROOK_OPEN_FILE_END_BONUS"]     = c.ROOK_OPEN_FILE_END_BONUS;
      n["ROOK_SEMIOPEN_FILE_MID_BONUS"] = c.ROOK_SEMIOPEN_FILE_MID_BONUS;
      n["ROOK_SEMIOPEN_FILE_END_BONUS"] = c.ROOK_SEMIOPEN_FILE_END_BONUS;
      n["USE_QUEEN_MOBILITY"]           = c.USE_QUEEN_MOBILITY;
      n["QUEEN_MOBILITY_MID_PER_MOVE"]  = c.QUEEN_MOBILITY_MID_PER_MOVE;
      n["QUEEN_MOBILITY_END_PER_MOVE"]  = c.QUEEN_MOBILITY_END_PER_MOVE;
      n["USE_QUEEN_TROPISM"]            = c.USE_QUEEN_TROPISM;
      n["QUEEN_TROPISM_MID_PER_STEP"]   = c.QUEEN_TROPISM_MID_PER_STEP;
      n["QUEEN_TROPISM_END_PER_STEP"]   = c.QUEEN_TROPISM_END_PER_STEP;
      n["USE_KING_EVAL"]                = c.USE_KING_EVAL;
      n["USE_KING_SAFETY_SHIELD"]       = c.USE_KING_SAFETY_SHIELD;
      n["KING_SHIELD_MID_PER_PAWN"]     = c.KING_SHIELD_MID_PER_PAWN;
      n["KING_SHIELD_END_PER_PAWN"]     = c.KING_SHIELD_END_PER_PAWN;
      n["USE_GAMEPHASE_VALUE"]          = c.USE_GAMEPHASE_VALUE;
      return n;
    }

    static bool decode(const Node& n, engine::config::EvalConfigData& c) {
      if (!n || !n.IsMap()) return false;
      using engine::config::yaml::set_if_present;
      std::unordered_set<std::string> seen;

      set_if_present(n, "EVAL_CONFIG_SOURCE", c.EVAL_CONFIG_SOURCE, seen);
      set_if_present(n, "USE_MATERIAL", c.USE_MATERIAL, seen);
      set_if_present(n, "USE_POSITIONAL", c.USE_POSITIONAL, seen);
      set_if_present(n, "USE_TEMPO", c.USE_TEMPO, seen);
      set_if_present(n, "TEMPO", c.TEMPO, seen);
      set_if_present(n, "USE_LAZY_EVAL", c.USE_LAZY_EVAL, seen);
      set_if_present(n, "LAZY_THRESHOLD", c.LAZY_THRESHOLD, seen);
      set_if_present(n, "USE_PAWN_EVAL", c.USE_PAWN_EVAL, seen);
      set_if_present(n, "USE_PAWN_TT", c.USE_PAWN_TT, seen);
      set_if_present(n, "PAWN_TT_SIZE_MB", c.PAWN_TT_SIZE_MB, seen);
      set_if_present(n, "ISOLATED_PAWN_MID_WEIGHT", c.ISOLATED_PAWN_MID_WEIGHT, seen);
      set_if_present(n, "ISOLATED_PAWN_END_WEIGHT", c.ISOLATED_PAWN_END_WEIGHT, seen);
      set_if_present(n, "DOUBLED_PAWN_MID_WEIGHT", c.DOUBLED_PAWN_MID_WEIGHT, seen);
      set_if_present(n, "DOUBLED_PAWN_END_WEIGHT", c.DOUBLED_PAWN_END_WEIGHT, seen);
      set_if_present(n, "PASSED_PAWN_MID_WEIGHT", c.PASSED_PAWN_MID_WEIGHT, seen);
      set_if_present(n, "PASSED_PAWN_END_WEIGHT", c.PASSED_PAWN_END_WEIGHT, seen);
      set_if_present(n, "BLOCKED_PAWN_MID_WEIGHT", c.BLOCKED_PAWN_MID_WEIGHT, seen);
      set_if_present(n, "BLOCKED_PAWN_END_WEIGHT", c.BLOCKED_PAWN_END_WEIGHT, seen);
      set_if_present(n, "PHALANX_PAWN_MID_WEIGHT", c.PHALANX_PAWN_MID_WEIGHT, seen);
      set_if_present(n, "PHALANX_PAWN_END_WEIGHT", c.PHALANX_PAWN_END_WEIGHT, seen);
      set_if_present(n, "SUPPORTED_PAWN_MID_WEIGHT", c.SUPPORTED_PAWN_MID_WEIGHT, seen);
      set_if_present(n, "SUPPORTED_PAWN_END_WEIGHT", c.SUPPORTED_PAWN_END_WEIGHT, seen);
      set_if_present(n, "USE_PIECE_EVAL", c.USE_PIECE_EVAL, seen);
      set_if_present(n, "USE_BISHOP_PAIR_BONUS", c.USE_BISHOP_PAIR_BONUS, seen);
      set_if_present(n, "BISHOP_PAIR_MID_BONUS", c.BISHOP_PAIR_MID_BONUS, seen);
      set_if_present(n, "BISHOP_PAIR_END_BONUS", c.BISHOP_PAIR_END_BONUS, seen);
      set_if_present(n, "USE_KNIGHT_MOBILITY", c.USE_KNIGHT_MOBILITY, seen);
      set_if_present(n, "KNIGHT_MOBILITY_MID_PER_MOVE", c.KNIGHT_MOBILITY_MID_PER_MOVE, seen);
      set_if_present(n, "KNIGHT_MOBILITY_END_PER_MOVE", c.KNIGHT_MOBILITY_END_PER_MOVE, seen);
      set_if_present(n, "KNIGHT_LOW_MOBILITY_LEQ1_MID", c.KNIGHT_LOW_MOBILITY_LEQ1_MID, seen);
      set_if_present(n, "KNIGHT_LOW_MOBILITY_LEQ1_END", c.KNIGHT_LOW_MOBILITY_LEQ1_END, seen);
      set_if_present(n, "KNIGHT_LOW_MOBILITY_LEQ2_MID", c.KNIGHT_LOW_MOBILITY_LEQ2_MID, seen);
      set_if_present(n, "KNIGHT_LOW_MOBILITY_LEQ2_END", c.KNIGHT_LOW_MOBILITY_LEQ2_END, seen);
      set_if_present(n, "USE_BISHOP_MOBILITY", c.USE_BISHOP_MOBILITY, seen);
      set_if_present(n, "BISHOP_MOBILITY_MID_PER_MOVE", c.BISHOP_MOBILITY_MID_PER_MOVE, seen);
      set_if_present(n, "BISHOP_MOBILITY_END_PER_MOVE", c.BISHOP_MOBILITY_END_PER_MOVE, seen);
      set_if_present(n, "BISHOP_LOW_MOBILITY_LEQ3_MID", c.BISHOP_LOW_MOBILITY_LEQ3_MID, seen);
      set_if_present(n, "BISHOP_LOW_MOBILITY_LEQ3_END", c.BISHOP_LOW_MOBILITY_LEQ3_END, seen);
      set_if_present(n, "USE_ROOK_MOBILITY", c.USE_ROOK_MOBILITY, seen);
      set_if_present(n, "ROOK_MOBILITY_MID_PER_MOVE", c.ROOK_MOBILITY_MID_PER_MOVE, seen);
      set_if_present(n, "ROOK_MOBILITY_END_PER_MOVE", c.ROOK_MOBILITY_END_PER_MOVE, seen);
      set_if_present(n, "ROOK_LOW_MOBILITY_LEQ3_MID", c.ROOK_LOW_MOBILITY_LEQ3_MID, seen);
      set_if_present(n, "ROOK_LOW_MOBILITY_LEQ3_END", c.ROOK_LOW_MOBILITY_LEQ3_END, seen);
      set_if_present(n, "USE_ROOK_OPEN_FILE_BONUS", c.USE_ROOK_OPEN_FILE_BONUS, seen);
      set_if_present(n, "ROOK_OPEN_FILE_MID_BONUS", c.ROOK_OPEN_FILE_MID_BONUS, seen);
      set_if_present(n, "ROOK_OPEN_FILE_END_BONUS", c.ROOK_OPEN_FILE_END_BONUS, seen);
      set_if_present(n, "ROOK_SEMIOPEN_FILE_MID_BONUS", c.ROOK_SEMIOPEN_FILE_MID_BONUS, seen);
      set_if_present(n, "ROOK_SEMIOPEN_FILE_END_BONUS", c.ROOK_SEMIOPEN_FILE_END_BONUS, seen);
      set_if_present(n, "USE_QUEEN_MOBILITY", c.USE_QUEEN_MOBILITY, seen);
      set_if_present(n, "QUEEN_MOBILITY_MID_PER_MOVE", c.QUEEN_MOBILITY_MID_PER_MOVE, seen);
      set_if_present(n, "QUEEN_MOBILITY_END_PER_MOVE", c.QUEEN_MOBILITY_END_PER_MOVE, seen);
      set_if_present(n, "USE_QUEEN_TROPISM", c.USE_QUEEN_TROPISM, seen);
      set_if_present(n, "QUEEN_TROPISM_MID_PER_STEP", c.QUEEN_TROPISM_MID_PER_STEP, seen);
      set_if_present(n, "QUEEN_TROPISM_END_PER_STEP", c.QUEEN_TROPISM_END_PER_STEP, seen);
      set_if_present(n, "USE_KING_EVAL", c.USE_KING_EVAL, seen);
      set_if_present(n, "USE_KING_SAFETY_SHIELD", c.USE_KING_SAFETY_SHIELD, seen);
      set_if_present(n, "KING_SHIELD_MID_PER_PAWN", c.KING_SHIELD_MID_PER_PAWN, seen);
      set_if_present(n, "KING_SHIELD_END_PER_PAWN", c.KING_SHIELD_END_PER_PAWN, seen);
      set_if_present(n, "USE_GAMEPHASE_VALUE", c.USE_GAMEPHASE_VALUE, seen);

      for (auto it : n) {
        const std::string key = it.first.as<std::string>("");
        if (!key.empty() && !seen.contains(key)) {
          engine::config::detail::warnUnknownKey(key);
        }
      }
      return true;
    }
  };

}// namespace YAML

#endif // FRANKYCPP_EVALCONFIGDATA_H
