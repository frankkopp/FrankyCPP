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

#include "config/ConfigMode.h"
#include "types/globals.h"
#include <yaml-cpp/yaml.h>

// Configuration struct for Search
// All members have default values which are used as fallback
// if no YAML config file is found or a value is missing in the file.
//
// CONFIG_ESSENTIAL members are always mutable instance members (runtime-changeable in all builds).
// CONFIG_CONST members are mutable instance members in development and static constexpr in production.
// In production, CONFIG_CONST members become compile-time constants enabling dead-code elimination.
struct SearchConfigData {
  // debug
  CONFIG_ESSENTIAL std::string CONFIG_SOURCE = "fallback";

  // time mgmt
  CONFIG_ESSENTIAL int MOVE_OVERHEAD_MS = 10;

  // Multi-threading (Lazy SMP)
  CONFIG_ESSENTIAL int THREADS = 1;           // Number of search threads (1 = single-threaded, no SMP overhead)
  CONFIG_CONST int SMP_HELPER_START_DEPTH = 4;// Depth at which to launch helper threads (allows TT priming)

  // book
  CONFIG_ESSENTIAL bool USE_BOOK         = true;
  CONFIG_ESSENTIAL std::string BOOK_PATH = "./books/book.txt";
  CONFIG_ESSENTIAL std::string BOOK_TYPE = "SIMPLE";// OpeningBook::BookFormat as string

  // pondering
  CONFIG_ESSENTIAL bool USE_PONDER = true;

  // core search flags
  CONFIG_CONST bool USE_ALPHABETA = true;
  CONFIG_CONST bool USE_PVS       = true;
  CONFIG_CONST bool USE_ASP       = true;

  // quiescence
  CONFIG_CONST bool USE_QUIESCENCE = true;

  // TT
  CONFIG_CONST bool USE_TT       = true;
  CONFIG_CONST bool USE_TT_VALUE = true;
  CONFIG_CONST bool USE_EVAL_TT  = true;
  CONFIG_ESSENTIAL int TT_SIZE_MB = 64;
  CONFIG_CONST bool USE_QS_TT    = true;

  // Syzygy tablebase settings
  CONFIG_ESSENTIAL std::string TB_PATH;// Path to Syzygy tablebase files (empty = disabled)
  // Root probing (once per search, for best move selection)
  CONFIG_CONST bool USE_TB_PROBE_ROOT = true; // Probe tablebases at root for best move
  CONFIG_CONST bool TB_ROOT_IMMEDIATE = false;// Return TB move immediately without searching (false = search for PV)
  // Search probing (during tree search, for cutoffs)
  CONFIG_CONST bool USE_TB_PROBE_SEARCH    = true;// Probe tablebases during search for cutoffs
  CONFIG_CONST bool USE_TB_PROBE_PV        = true;// Probe tablebases on PV nodes (false = only non-PV for cutoffs)
  CONFIG_CONST int TB_PROBE_DEPTH          = 1;   // Minimum depth to probe WDL in search (Stockfish default: 1)
  CONFIG_CONST int TB_PROBE_LIMIT          = 6;   // Max pieces for search TB probing (3-7)
  CONFIG_CONST int TB_RULE50_THRESHOLD     = 80;  // HalfMoveClock threshold for DTZ check (>=100 disables)
  CONFIG_CONST bool TB_CACHE_PREWARM       = true;// Pre-warm OS file cache at startup
  CONFIG_CONST int TB_CACHE_PREWARM_PIECES = 6;   // Max pieces to pre-warm (3-6, higher = more startup time)

  // move sorting
  CONFIG_CONST bool USE_TT_PV_MOVE_SORT = true;
  CONFIG_CONST bool USE_KILLER_MOVES    = true;
  CONFIG_CONST bool USE_HISTORY_COUNTER = true;
  CONFIG_CONST bool USE_HISTORY_MOVES   = true;

  // Internal Iterative Deepening (IID) - legacy approach
  // Does a reduced-depth mini-search to find a good first move when no TT move
  // Note: Largely obsolete - PV-only restriction makes it rarely trigger due to TT
  CONFIG_CONST bool USE_IID      = false;// Disabled - IIR is more effective
  CONFIG_CONST int IID_DEPTH     = 6;
  CONFIG_CONST int IID_REDUCTION = 2;

  // Internal Iterative Reduction (IIR) - modern alternative to IID
  // Simply reduces depth when no TT move available (Stockfish approach)
  // Much more effective than IID because it applies to ALL node types
  // Note: USE_IID and USE_IIR are mutually exclusive - only enable one!
  CONFIG_CONST bool USE_IIR       = true;// Enabled - 36% node reduction in testing
  CONFIG_CONST int IIR_DEPTH      = 4;   // Minimum depth to apply IIR
  CONFIG_CONST int IIR_REDUCTION  = 2;   // How much to reduce depth
  CONFIG_CONST bool IIR_ALL_NODES = true;// Apply to all nodes (true) or PV only (false)

  // pruning
  CONFIG_CONST bool USE_MDP             = true;
  CONFIG_CONST bool USE_QS_STANDPAT_CUT = true;
  CONFIG_CONST bool USE_QS_SEE          = true;
  CONFIG_CONST bool USE_RAZORING        = true;
  CONFIG_CONST int RAZOR_MARGIN         = 531;
  CONFIG_CONST bool USE_RFP             = true;
  CONFIG_CONST std::array<int, 4> RFP_MARGIN{0, 200, 400, 800};
  // RFP + improving: increase margin when not improving (prune less aggressively)
  // Rationale: "not improving" means eval may be unreliable, search more carefully
  CONFIG_CONST bool USE_RFP_IMPROVING   = true;// Use improving flag to modulate RFP margin
  CONFIG_CONST int RFP_IMPROVING_MARGIN = 40;  // Margin increase (cp) when not improving

  // null move pruning
  CONFIG_CONST bool USE_NMP                  = true;
  CONFIG_CONST int NMP_DEPTH                 = 3;
  CONFIG_CONST int NMP_REDUCTION             = 2;
  CONFIG_CONST bool USE_NMP_VERIFY           = true;
  CONFIG_CONST int NMP_VERIFY_MIN_DEPTH      = 6;
  CONFIG_CONST int NMP_VERIFY_MARGIN         = 2;
  CONFIG_CONST int NMP_NEAR_MATE_MARGIN      = 64;
  CONFIG_CONST bool USE_NMP_ZUG_GUARD        = true;
  CONFIG_CONST int NMP_ZUG_NONPAWN_THRESHOLD = 0;
  // NMP + improving: extra reduction when position is not improving
  CONFIG_CONST bool USE_NMP_IMPROVING      = true;// Use improving flag to modulate NMP reduction
  CONFIG_CONST int NMP_IMPROVING_REDUCTION = 1;   // Extra NMP reduction depth when not improving

  // futility pruning
  CONFIG_CONST bool USE_FP  = true;
  CONFIG_CONST bool USE_QFP = true;
  CONFIG_CONST std::array<int, 7> FP_MARGIN{0, 100, 200, 300, 500, 900, 1200};
  // FP + improving: increase margin when not improving (prune less aggressively)
  // Rationale: "not improving" means eval may be unreliable, search more carefully
  CONFIG_CONST bool USE_FP_IMPROVING   = true;// Use improving flag to modulate FP margin
  CONFIG_CONST int FP_IMPROVING_MARGIN = 80;  // Margin increase (cp) when not improving

  // improving flag
  CONFIG_CONST bool USE_IMPROVING = true;// Master switch: track if eval is improving vs 2 plies ago

  // LMR
  CONFIG_CONST bool USE_LMR      = true;
  CONFIG_CONST int LMR_MIN_DEPTH = 2;
  CONFIG_CONST int LMR_MIN_MOVES = 2;
  // LMR formula selection (logarithmic vs linear)
  CONFIG_CONST bool LMR_USE_LOG_FORMULA = true;// Use logarithmic formula instead of linear
  CONFIG_CONST double LMR_LOG_BASE_DIV  = 1.25;// Divisor for log formula: log(d)*log(m)/divisor
  // LMR + improving: extra reduction when position is not improving
  CONFIG_CONST bool USE_LMR_IMPROVING      = true;// Use improving flag to modulate LMR
  CONFIG_CONST int LMR_IMPROVING_REDUCTION = 1;   // Extra reduction depth when not improving
  // LMR + history: adjust reduction based on move's history score
  CONFIG_CONST bool USE_LMR_HISTORY = true;// Use history score to modulate LMR reduction
  // Divisor for history -> reduction conversion. History values are (1 << depth) per cutoff.
  // With divisor 8192 (= 1 << 13), a single cutoff at depth 13 gives 1 ply less reduction.
  // Lower divisor = more aggressive adjustment, higher = more conservative.
  CONFIG_CONST int LMR_HISTORY_DIVISOR = 8192;
  // LMR + cut node: extra reduction on expected cut nodes
  // Cut nodes are expected to fail high quickly; late moves on cut nodes are very unlikely to be best.
  CONFIG_CONST bool USE_LMR_CUTNODE      = true;// Use cut node flag to increase LMR reduction
  CONFIG_CONST int LMR_CUTNODE_REDUCTION = 2;   // Extra reduction depth on cut nodes

  // LMP
  CONFIG_CONST bool USE_LMP = true;
  CONFIG_CONST std::array<int, 16> LMP_MOVES{0, 7, 9, 11, 13, 15, 17, 19, 22, 24, 27, 29, 32, 35, 38, 41};
  // LMP + improving: allow more moves when position is improving
  CONFIG_CONST bool USE_LMP_IMPROVING = true;// Use improving flag to modulate LMP threshold

  // extensions
  CONFIG_CONST bool USE_EXTENSIONS       = true;
  CONFIG_CONST bool USE_CHECK_EXT        = true;
  CONFIG_CONST int CHECK_EXT_MIN_DEPTH   = 2;   // minimum depth to apply check extension
  CONFIG_CONST int CHECK_EXT_EARLY_LIMIT = 99;  // effectively no limit when SEE is enabled
  CONFIG_CONST bool USE_CHECK_EXT_SEE    = true;// only extend checks with SEE >= 0 (non-losing)
  CONFIG_CONST bool USE_THREAT_EXT       = true;
  CONFIG_CONST int THREAT_EXT_MATE_DEPTH = 4;// Mate-in-N threshold for threat detection (VALUE_CHECKMATE - 2*N)
  CONFIG_CONST bool USE_EXT_ADD_DEPTH    = true;

  // singular extensions
  CONFIG_CONST bool USE_SINGULAR_EXT = true;
  // Require BETA/EXACT TT bound - DISABLED: filters 99.98% of candidates, verification search is sufficient
  // TODO: Consider removing this option entirely after strength testing
  CONFIG_CONST bool USE_SINGULAR_TT_BOUND = false;
  CONFIG_CONST int SINGULAR_MARGIN        = 64;// centipawns below TT value to consider singular
  CONFIG_CONST int SINGULAR_MIN_DEPTH     = 8; // minimum depth to attempt singular extension
  CONFIG_CONST int SINGULAR_REDUCTION     = 4; // depth reduction for verification search

  // moves-left model
  CONFIG_CONST int MOVES_LEFT_OPENING   = 36;
  CONFIG_CONST int MOVES_LEFT_MIDGAME   = 28;
  CONFIG_CONST int MOVES_LEFT_ENDGAME   = 16;
  CONFIG_CONST int MOVES_LEFT_LOW_MAT   = 10;
  CONFIG_CONST int MOVES_LEFT_QUEENLESS = 22;

  // thresholds
  CONFIG_CONST int NPP_HEAVY_THRESHOLD = 10;
  CONFIG_CONST int NPP_LIGHT_THRESHOLD = 4;

  // repetition & clamps
  CONFIG_CONST int REPETITION_HMC_HIGH     = 80;
  CONFIG_CONST int REPETITION_RISK_PENALTY = 6;
  CONFIG_CONST int MOVES_LEFT_MIN_CLAMP    = 6;
  CONFIG_CONST int MOVES_LEFT_MAX_CLAMP    = 50;

  // best-move instability time management
  CONFIG_CONST bool USE_BESTMOVE_INSTABILITY    = true;// enable best-move instability tracking
  CONFIG_CONST int INSTABILITY_MIN_DEPTH        = 5;   // minimum depth to start tracking
  CONFIG_CONST int INSTABILITY_STABLE_COUNT     = 3;   // consecutive stable iterations to trigger time reduction
  CONFIG_CONST int INSTABILITY_CHANGE_THRESHOLD = 2;   // number of best-move changes to trigger time extension
  CONFIG_CONST double INSTABILITY_STABLE_FACTOR = 0.80;// multiply remaining time by this when stable (< 1.0)
  CONFIG_CONST double INSTABILITY_EXTEND_FACTOR = 1.25;// multiply remaining time by this when unstable (> 1.0)

  // extra time cap - prevents unbounded time extensions in fail-low cascades
  CONFIG_CONST double MAX_EXTRA_TIME_FACTOR = 2.0;// max extra time as multiple of base time (2.0 = max 3x total)

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
