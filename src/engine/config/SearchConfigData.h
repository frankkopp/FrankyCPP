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

#ifndef FRANKYCPP_SEARCHCONFIGDATA_H
#define FRANKYCPP_SEARCHCONFIGDATA_H

#include <array>
#include <sstream>
#include <string>
#include <unordered_set>

#include "common/Logging.h"
#include "engine/config/YamlHelpers.h"
#include "types/globals.h"
#include <yaml-cpp/yaml.h>

namespace engine::config {

  // Configuration struct for Search
  // All members have default values which are used as fallback
  // if no YAML config file is found or a value is missing in the file.
  struct SearchConfigData {
    // debug
    std::string CONFIG_SOURCE = "fallback";

    // time mgmt
    int MOVE_OVERHEAD_MS = 10;

    // book
    bool USE_BOOK         = true;
    std::string BOOK_PATH = "./books/book.txt";
    std::string BOOK_TYPE = "SIMPLE";// OpeningBook::BookFormat as string

    // pondering
    bool USE_PONDER = true;

    // core search flags
    bool USE_ALPHABETA = true;
    bool USE_PVS       = true;
    bool USE_ASP       = true;

    // quiescence
    bool USE_QUIESCENCE = true;

    // TT
    bool USE_TT       = true;
    bool USE_TT_VALUE = true;
    bool USE_EVAL_TT  = true;
    int TT_SIZE_MB    = 64;
    bool USE_QS_TT    = true;

    // move sorting
    bool USE_TT_PV_MOVE_SORT = true;
    bool USE_KILLER_MOVES    = true;
    bool USE_HISTORY_COUNTER = true;
    bool USE_HISTORY_MOVES   = true;
    bool USE_IID             = true;
    int IID_DEPTH            = 6;
    int IID_REDUCTION        = 2;

    // pruning
    bool USE_MDP             = true;
    bool USE_QS_STANDPAT_CUT = true;
    bool USE_QS_SEE          = true;
    bool USE_RAZORING        = true;
    int RAZOR_MARGIN         = 531;
    bool USE_RFP             = true;
    std::array<int, 4> RFP_MARGIN{0, 200, 400, 800};

    // null move pruning
    bool USE_NMP                  = true;
    int NMP_DEPTH                 = 3;
    int NMP_REDUCTION             = 2;
    bool USE_NMP_VERIFY           = true;
    int NMP_VERIFY_MIN_DEPTH      = 6;
    int NMP_VERIFY_MARGIN         = 2;
    int NMP_NEAR_MATE_MARGIN      = 64;
    bool USE_NMP_ZUG_GUARD        = true;
    int NMP_ZUG_NONPAWN_THRESHOLD = 0;

    // futility pruning
    bool USE_FP  = true;
    bool USE_QFP = true;
    std::array<int, 7> FP_MARGIN{0, 100, 200, 300, 500, 900, 1200};

    // LMR/LMP
    bool USE_LMR      = true;
    int LMR_MIN_DEPTH = 3;
    int LMR_MIN_MOVES = 3;
    bool USE_LMP      = true;
    std::array<int, 16> LMP_MOVES{0, 7, 9, 11, 13, 15, 17, 19, 22, 24, 27, 29, 32, 35, 38, 41};

    // extensions
    bool USE_EXTENSIONS    = true;
    bool USE_CHECK_EXT     = true;
    bool USE_THREAT_EXT    = false;
    bool USE_EXT_ADD_DEPTH = true;

    // singular extensions
    bool USE_SINGULAR_EXT   = true;
    int SINGULAR_MARGIN     = 64;  // centipawns below TT value to consider singular
    int SINGULAR_MIN_DEPTH  = 8;   // minimum depth to attempt singular extension
    int SINGULAR_REDUCTION  = 4;   // depth reduction for verification search

    // moves-left model
    int MOVES_LEFT_OPENING   = 36;
    int MOVES_LEFT_MIDGAME   = 28;
    int MOVES_LEFT_ENDGAME   = 16;
    int MOVES_LEFT_LOW_MAT   = 10;
    int MOVES_LEFT_QUEENLESS = 22;

    // thresholds
    int NPP_HEAVY_THRESHOLD = 10;
    int NPP_LIGHT_THRESHOLD = 4;

    // repetition & clamps
    int REPETITION_HMC_HIGH     = 80;
    int REPETITION_RISK_PENALTY = 6;
    int MOVES_LEFT_MIN_CLAMP    = 6;
    int MOVES_LEFT_MAX_CLAMP    = 50;

    std::string str() const {
      std::ostringstream os;
      os << "MOVE_OVERHEAD_MS: " << MOVE_OVERHEAD_MS << '\n'
         << "USE_BOOK: " << USE_BOOK << '\n'
         << "BOOK_PATH: " << BOOK_PATH << '\n'
         << "BOOK_TYPE: " << BOOK_TYPE << '\n'
         << "USE_PONDER: " << USE_PONDER << '\n'
         << "USE_ALPHABETA: " << USE_ALPHABETA << '\n'
         << "USE_PVS: " << USE_PVS << '\n'
         << "USE_ASP: " << USE_ASP << '\n'
         << "USE_QUIESCENCE: " << USE_QUIESCENCE << '\n'
         << "TT_SIZE_MB: " << TT_SIZE_MB << '\n';
      return os.str();
    }
  };

  namespace detail {
    inline void warnUnknownKey(const std::string& key, const char* context) {
      LOG__WARN(Logger::get().SEARCH_LOG, "Unknown key in {} config: {}", context, key);
    }
  }// namespace detail

}// namespace engine::config

template<>
struct YAML::convert<engine::config::SearchConfigData> {
  static Node encode(const engine::config::SearchConfigData& c) {
    Node n;
    n["CONFIG_SOURCE"] = c.CONFIG_SOURCE;

    n["MOVE_OVERHEAD_MS"]    = c.MOVE_OVERHEAD_MS;
    n["USE_BOOK"]            = c.USE_BOOK;
    n["BOOK_PATH"]           = c.BOOK_PATH;
    n["BOOK_TYPE"]           = c.BOOK_TYPE;
    n["USE_PONDER"]          = c.USE_PONDER;
    n["USE_ALPHABETA"]       = c.USE_ALPHABETA;
    n["USE_PVS"]             = c.USE_PVS;
    n["USE_ASP"]             = c.USE_ASP;
    n["USE_QUIESCENCE"]      = c.USE_QUIESCENCE;
    n["USE_TT"]              = c.USE_TT;
    n["USE_TT_VALUE"]        = c.USE_TT_VALUE;
    n["USE_EVAL_TT"]         = c.USE_EVAL_TT;
    n["TT_SIZE_MB"]          = c.TT_SIZE_MB;
    n["USE_QS_TT"]           = c.USE_QS_TT;
    n["USE_TT_PV_MOVE_SORT"] = c.USE_TT_PV_MOVE_SORT;
    n["USE_KILLER_MOVES"]    = c.USE_KILLER_MOVES;
    n["USE_HISTORY_COUNTER"] = c.USE_HISTORY_COUNTER;
    n["USE_HISTORY_MOVES"]   = c.USE_HISTORY_MOVES;
    n["USE_IID"]             = c.USE_IID;
    n["IID_DEPTH"]           = c.IID_DEPTH;
    n["IID_REDUCTION"]       = c.IID_REDUCTION;
    n["USE_MDP"]             = c.USE_MDP;
    n["USE_QS_STANDPAT_CUT"] = c.USE_QS_STANDPAT_CUT;
    n["USE_QS_SEE"]          = c.USE_QS_SEE;
    n["USE_RAZORING"]        = c.USE_RAZORING;
    n["RAZOR_MARGIN"]        = c.RAZOR_MARGIN;
    n["USE_RFP"]             = c.USE_RFP;
    {
      Node arr;
      for (int v : c.RFP_MARGIN) arr.push_back(v);
      n["RFP_MARGIN"] = arr;
    }
    n["USE_NMP"]                   = c.USE_NMP;
    n["NMP_DEPTH"]                 = c.NMP_DEPTH;
    n["NMP_REDUCTION"]             = c.NMP_REDUCTION;
    n["USE_NMP_VERIFY"]            = c.USE_NMP_VERIFY;
    n["NMP_VERIFY_MIN_DEPTH"]      = c.NMP_VERIFY_MIN_DEPTH;
    n["NMP_VERIFY_MARGIN"]         = c.NMP_VERIFY_MARGIN;
    n["NMP_NEAR_MATE_MARGIN"]      = c.NMP_NEAR_MATE_MARGIN;
    n["USE_NMP_ZUG_GUARD"]         = c.USE_NMP_ZUG_GUARD;
    n["NMP_ZUG_NONPAWN_THRESHOLD"] = c.NMP_ZUG_NONPAWN_THRESHOLD;
    n["USE_FP"]                    = c.USE_FP;
    n["USE_QFP"]                   = c.USE_QFP;
    {
      Node arr;
      for (int v : c.FP_MARGIN) arr.push_back(v);
      n["FP_MARGIN"] = arr;
    }
    n["USE_LMR"]       = c.USE_LMR;
    n["LMR_MIN_DEPTH"] = c.LMR_MIN_DEPTH;
    n["LMR_MIN_MOVES"] = c.LMR_MIN_MOVES;
    n["USE_LMP"]       = c.USE_LMP;
    {
      Node arr;
      for (int v : c.LMP_MOVES) arr.push_back(v);
      n["LMP_MOVES"] = arr;
    }
    n["USE_EXTENSIONS"]          = c.USE_EXTENSIONS;
    n["USE_CHECK_EXT"]           = c.USE_CHECK_EXT;
    n["USE_THREAT_EXT"]          = c.USE_THREAT_EXT;
    n["USE_EXT_ADD_DEPTH"]       = c.USE_EXT_ADD_DEPTH;
    n["USE_SINGULAR_EXT"]        = c.USE_SINGULAR_EXT;
    n["SINGULAR_MARGIN"]         = c.SINGULAR_MARGIN;
    n["SINGULAR_MIN_DEPTH"]      = c.SINGULAR_MIN_DEPTH;
    n["SINGULAR_REDUCTION"]      = c.SINGULAR_REDUCTION;
    n["MOVES_LEFT_OPENING"]      = c.MOVES_LEFT_OPENING;
    n["MOVES_LEFT_MIDGAME"]      = c.MOVES_LEFT_MIDGAME;
    n["MOVES_LEFT_ENDGAME"]      = c.MOVES_LEFT_ENDGAME;
    n["MOVES_LEFT_LOW_MAT"]      = c.MOVES_LEFT_LOW_MAT;
    n["MOVES_LEFT_QUEENLESS"]    = c.MOVES_LEFT_QUEENLESS;
    n["NPP_HEAVY_THRESHOLD"]     = c.NPP_HEAVY_THRESHOLD;
    n["NPP_LIGHT_THRESHOLD"]     = c.NPP_LIGHT_THRESHOLD;
    n["REPETITION_HMC_HIGH"]     = c.REPETITION_HMC_HIGH;
    n["REPETITION_RISK_PENALTY"] = c.REPETITION_RISK_PENALTY;
    n["MOVES_LEFT_MIN_CLAMP"]    = c.MOVES_LEFT_MIN_CLAMP;
    n["MOVES_LEFT_MAX_CLAMP"]    = c.MOVES_LEFT_MAX_CLAMP;

    return n;
  }

  static bool decode(const Node& n, engine::config::SearchConfigData& c) {
    if (!n || !n.IsMap()) return false;
    using engine::config::yaml::set_array_if_present;
    using engine::config::yaml::set_if_present;
    std::unordered_set<std::string> seen;

    set_if_present(n, "CONFIG_SOURCE", c.CONFIG_SOURCE, seen);
    set_if_present(n, "MOVE_OVERHEAD_MS", c.MOVE_OVERHEAD_MS, seen);
    set_if_present(n, "USE_BOOK", c.USE_BOOK, seen);
    set_if_present(n, "BOOK_PATH", c.BOOK_PATH, seen);
    set_if_present(n, "BOOK_TYPE", c.BOOK_TYPE, seen);
    set_if_present(n, "USE_PONDER", c.USE_PONDER, seen);
    set_if_present(n, "USE_ALPHABETA", c.USE_ALPHABETA, seen);
    set_if_present(n, "USE_PVS", c.USE_PVS, seen);
    set_if_present(n, "USE_ASP", c.USE_ASP, seen);
    set_if_present(n, "USE_QUIESCENCE", c.USE_QUIESCENCE, seen);
    set_if_present(n, "USE_TT", c.USE_TT, seen);
    set_if_present(n, "USE_TT_VALUE", c.USE_TT_VALUE, seen);
    set_if_present(n, "USE_EVAL_TT", c.USE_EVAL_TT, seen);
    set_if_present(n, "TT_SIZE_MB", c.TT_SIZE_MB, seen);
    set_if_present(n, "USE_QS_TT", c.USE_QS_TT, seen);
    set_if_present(n, "USE_TT_PV_MOVE_SORT", c.USE_TT_PV_MOVE_SORT, seen);
    set_if_present(n, "USE_KILLER_MOVES", c.USE_KILLER_MOVES, seen);
    set_if_present(n, "USE_HISTORY_COUNTER", c.USE_HISTORY_COUNTER, seen);
    set_if_present(n, "USE_HISTORY_MOVES", c.USE_HISTORY_MOVES, seen);
    set_if_present(n, "USE_IID", c.USE_IID, seen);
    set_if_present(n, "IID_DEPTH", c.IID_DEPTH, seen);
    set_if_present(n, "IID_REDUCTION", c.IID_REDUCTION, seen);
    set_if_present(n, "USE_MDP", c.USE_MDP, seen);
    set_if_present(n, "USE_QS_STANDPAT_CUT", c.USE_QS_STANDPAT_CUT, seen);
    set_if_present(n, "USE_QS_SEE", c.USE_QS_SEE, seen);
    set_if_present(n, "USE_RAZORING", c.USE_RAZORING, seen);
    set_if_present(n, "RAZOR_MARGIN", c.RAZOR_MARGIN, seen);
    set_if_present(n, "USE_RFP", c.USE_RFP, seen);
    set_array_if_present(n, "RFP_MARGIN", c.RFP_MARGIN, seen);
    set_if_present(n, "USE_NMP", c.USE_NMP, seen);
    set_if_present(n, "NMP_DEPTH", c.NMP_DEPTH, seen);
    set_if_present(n, "NMP_REDUCTION", c.NMP_REDUCTION, seen);
    set_if_present(n, "USE_NMP_VERIFY", c.USE_NMP_VERIFY, seen);
    set_if_present(n, "NMP_VERIFY_MIN_DEPTH", c.NMP_VERIFY_MIN_DEPTH, seen);
    set_if_present(n, "NMP_VERIFY_MARGIN", c.NMP_VERIFY_MARGIN, seen);
    set_if_present(n, "NMP_NEAR_MATE_MARGIN", c.NMP_NEAR_MATE_MARGIN, seen);
    set_if_present(n, "USE_NMP_ZUG_GUARD", c.USE_NMP_ZUG_GUARD, seen);
    set_if_present(n, "NMP_ZUG_NONPAWN_THRESHOLD", c.NMP_ZUG_NONPAWN_THRESHOLD, seen);
    set_if_present(n, "USE_FP", c.USE_FP, seen);
    set_if_present(n, "USE_QFP", c.USE_QFP, seen);
    set_array_if_present(n, "FP_MARGIN", c.FP_MARGIN, seen);
    set_if_present(n, "USE_LMR", c.USE_LMR, seen);
    set_if_present(n, "LMR_MIN_DEPTH", c.LMR_MIN_DEPTH, seen);
    set_if_present(n, "LMR_MIN_MOVES", c.LMR_MIN_MOVES, seen);
    set_if_present(n, "USE_LMP", c.USE_LMP, seen);
    set_array_if_present(n, "LMP_MOVES", c.LMP_MOVES, seen);
    set_if_present(n, "USE_EXTENSIONS", c.USE_EXTENSIONS, seen);
    set_if_present(n, "USE_CHECK_EXT", c.USE_CHECK_EXT, seen);
    set_if_present(n, "USE_THREAT_EXT", c.USE_THREAT_EXT, seen);
    set_if_present(n, "USE_EXT_ADD_DEPTH", c.USE_EXT_ADD_DEPTH, seen);
    set_if_present(n, "USE_SINGULAR_EXT", c.USE_SINGULAR_EXT, seen);
    set_if_present(n, "SINGULAR_MARGIN", c.SINGULAR_MARGIN, seen);
    set_if_present(n, "SINGULAR_MIN_DEPTH", c.SINGULAR_MIN_DEPTH, seen);
    set_if_present(n, "SINGULAR_REDUCTION", c.SINGULAR_REDUCTION, seen);
    set_if_present(n, "MOVES_LEFT_OPENING", c.MOVES_LEFT_OPENING, seen);
    set_if_present(n, "MOVES_LEFT_MIDGAME", c.MOVES_LEFT_MIDGAME, seen);
    set_if_present(n, "MOVES_LEFT_ENDGAME", c.MOVES_LEFT_ENDGAME, seen);
    set_if_present(n, "MOVES_LEFT_LOW_MAT", c.MOVES_LEFT_LOW_MAT, seen);
    set_if_present(n, "MOVES_LEFT_QUEENLESS", c.MOVES_LEFT_QUEENLESS, seen);
    set_if_present(n, "NPP_HEAVY_THRESHOLD", c.NPP_HEAVY_THRESHOLD, seen);
    set_if_present(n, "NPP_LIGHT_THRESHOLD", c.NPP_LIGHT_THRESHOLD, seen);
    set_if_present(n, "REPETITION_HMC_HIGH", c.REPETITION_HMC_HIGH, seen);
    set_if_present(n, "REPETITION_RISK_PENALTY", c.REPETITION_RISK_PENALTY, seen);
    set_if_present(n, "MOVES_LEFT_MIN_CLAMP", c.MOVES_LEFT_MIN_CLAMP, seen);
    set_if_present(n, "MOVES_LEFT_MAX_CLAMP", c.MOVES_LEFT_MAX_CLAMP, seen);

    // Log unknown keys
    for (auto it : n) {
      const std::string key = it.first.as<std::string>("");
      if (!key.empty() && !seen.contains(key)) {
        engine::config::detail::warnUnknownKey(key, "Search");
      }
    }
    return true;
  }
};// namespace YAML

#endif // FRANKYCPP_SEARCHCONFIGDATA_H
