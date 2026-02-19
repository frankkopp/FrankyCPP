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
#include <set>
#include <string>

#include "types/globals.h"
#include <yaml-cpp/yaml.h>

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

  // Syzygy tablebase settings
  std::string TB_PATH;                 // Path to Syzygy tablebase files (empty = disabled)
  // Root probing (once per search, for best move selection)
  bool USE_TB_PROBE_ROOT         = true;   // Probe tablebases at root for best move
  bool TB_ROOT_IMMEDIATE     = false;  // Return TB move immediately without searching (false = search for PV)
  // Search probing (during tree search, for cutoffs)
  bool USE_TB_PROBE_SEARCH       = true;   // Probe tablebases during search for cutoffs
  bool USE_TB_PROBE_PV           = true;   // Probe tablebases on PV nodes (false = only non-PV for cutoffs)
  int TB_PROBE_DEPTH         = 1;      // Minimum depth to probe WDL in search (0 = always)
  int TB_PROBE_LIMIT         = 6;      // Max pieces for search TB probing (3-7)
  int TB_RULE50_THRESHOLD    = 80;     // HalfMoveClock threshold for DTZ check (>=100 disables)

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

  // LMR
  bool USE_LMR      = true;
  int LMR_MIN_DEPTH = 2;
  int LMR_MIN_MOVES = 2;
  // LMR formula selection (logarithmic vs linear)
  bool LMR_USE_LOG_FORMULA  = true; // Use logarithmic formula instead of linear
  double LMR_LOG_BASE_DIV   = 1.50; // Divisor for log formula: log(d)*log(m)/divisor

  // LMP
  bool USE_LMP      = true;
  std::array<int, 16> LMP_MOVES{0, 7, 9, 11, 13, 15, 17, 19, 22, 24, 27, 29, 32, 35, 38, 41};

  // extensions
  bool USE_EXTENSIONS       = true;
  bool USE_CHECK_EXT        = true;
  int CHECK_EXT_EARLY_LIMIT = 3;// only extend checks in first N moves per node
  bool USE_THREAT_EXT       = false;
  bool USE_EXT_ADD_DEPTH    = true;

  // singular extensions
  bool USE_SINGULAR_EXT  = true;
  int SINGULAR_MARGIN    = 64;// centipawns below TT value to consider singular
  int SINGULAR_MIN_DEPTH = 8; // minimum depth to attempt singular extension
  int SINGULAR_REDUCTION = 4; // depth reduction for verification search

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

  // best-move instability time management
  bool USE_BESTMOVE_INSTABILITY    = true;// enable best-move instability tracking
  int INSTABILITY_MIN_DEPTH        = 5;   // minimum depth to start tracking
  int INSTABILITY_STABLE_COUNT     = 3;   // consecutive stable iterations to trigger time reduction
  int INSTABILITY_CHANGE_THRESHOLD = 2;   // number of best-move changes to trigger time extension
  double INSTABILITY_STABLE_FACTOR = 0.80;// multiply remaining time by this when stable (< 1.0)
  double INSTABILITY_EXTEND_FACTOR = 1.25;// multiply remaining time by this when unstable (> 1.0)

  std::string str() const;
};

// Forward declaration for parseYamlConfig
std::set<std::string> parseYamlConfig(
    const YAML::Node& node,
    SearchConfigData& search,
    bool warnUnknown);

template<>
struct YAML::convert<SearchConfigData> {
  static Node encode(const SearchConfigData&) {
    // YAML encoding not used - config files are manually maintained.
    // Use generateConfigString() for human-readable output.
    return {};
  }

  static bool decode(const Node& n, SearchConfigData& c) {
    if (!n || !n.IsMap()) return false;
    parseYamlConfig(n, c, /* warnUnknown= */ true);
    return true;
  }
};// namespace YAML

#endif// FRANKYCPP_SEARCHCONFIGDATA_H
