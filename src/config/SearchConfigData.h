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
  // RFP + improving: increase margin when not improving (prune less aggressively)
  // Rationale: "not improving" means eval may be unreliable, search more carefully
  bool USE_RFP_IMPROVING   = true; // Use improving flag to modulate RFP margin
  int RFP_IMPROVING_MARGIN = 40;   // Margin increase (cp) when not improving

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
  // NMP + improving: extra reduction when position is not improving
  bool USE_NMP_IMPROVING        = true; // Use improving flag to modulate NMP reduction
  int NMP_IMPROVING_REDUCTION   = 1;    // Extra NMP reduction depth when not improving

  // futility pruning
  bool USE_FP  = true;
  bool USE_QFP = true;
  std::array<int, 7> FP_MARGIN{0, 100, 200, 300, 500, 900, 1200};
  // FP + improving: increase margin when not improving (prune less aggressively)
  // Rationale: "not improving" means eval may be unreliable, search more carefully
  bool USE_FP_IMPROVING   = true; // Use improving flag to modulate FP margin
  int FP_IMPROVING_MARGIN = 80;   // Margin increase (cp) when not improving

  // improving flag
  bool USE_IMPROVING = true;// Master switch: track if eval is improving vs 2 plies ago

  // LMR
  bool USE_LMR      = true;
  int LMR_MIN_DEPTH = 2;
  int LMR_MIN_MOVES = 2;
  // LMR formula selection (logarithmic vs linear)
  bool LMR_USE_LOG_FORMULA  = true; // Use logarithmic formula instead of linear
  double LMR_LOG_BASE_DIV   = 1.25; // Divisor for log formula: log(d)*log(m)/divisor
  // LMR + improving: extra reduction when position is not improving
  bool USE_LMR_IMPROVING      = true; // Use improving flag to modulate LMR
  int LMR_IMPROVING_REDUCTION = 1;    // Extra reduction depth when not improving
  // LMR + history: adjust reduction based on move's history score
  bool USE_LMR_HISTORY   = true;  // Use history score to modulate LMR reduction
  // Divisor for history -> reduction conversion. History values are (1 << depth) per cutoff.
  // With divisor 8192 (= 1 << 13), a single cutoff at depth 13 gives 1 ply less reduction.
  // Lower divisor = more aggressive adjustment, higher = more conservative.
  int LMR_HISTORY_DIVISOR = 8192;
  // LMR + cut node: extra reduction on expected cut nodes
  // Cut nodes are expected to fail high quickly; late moves on cut nodes are very unlikely to be best.
  bool USE_LMR_CUTNODE = true;    // Use cut node flag to increase LMR reduction
  int LMR_CUTNODE_REDUCTION = 2;  // Extra reduction depth on cut nodes

  // LMP
  bool USE_LMP      = true;
  std::array<int, 16> LMP_MOVES{0, 7, 9, 11, 13, 15, 17, 19, 22, 24, 27, 29, 32, 35, 38, 41};
  // LMP + improving: allow more moves when position is improving
  bool USE_LMP_IMPROVING = true; // Use improving flag to modulate LMP threshold

  // extensions
  bool USE_EXTENSIONS       = true;
  bool USE_CHECK_EXT        = true;
  int CHECK_EXT_MIN_DEPTH   = 2;  // minimum depth to apply check extension
  int CHECK_EXT_EARLY_LIMIT = 99; // effectively no limit when SEE is enabled
  bool USE_CHECK_EXT_SEE    = true;// only extend checks with SEE >= 0 (non-losing)
  bool USE_THREAT_EXT       = true;
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
