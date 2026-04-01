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

// SearchTreeSizeTest uses CONFIG_OVERRIDE on non-essential config members which
// become static constexpr in production builds — exclude entirely.
#ifndef FRANKYCPP_PRODUCTION

#include <unordered_map>

// no longer use fmt chrono; use our time utilities
#include "types/timeunits.h"

#include "SearchTreeSizeTest.h"

using namespace engine;
using namespace chess;
using namespace config;
using namespace enginetest;


SearchTreeSize::Result
SearchTreeSizeTest::featureMeasurements(const int d, const milliseconds mt, const std::string& fen) {
  Search search{};
  SearchLimits searchLimits{};
  searchLimits.depth = d;
  if (mt != milliseconds::zero()) {
    searchLimits.moveTime    = mt;
    searchLimits.timeControl = true;
  }
  SearchTreeSize::Result result(fen);
  const Position position(fen);

  CONFIG_OVERRIDE_START()
  // Threading
  s.THREADS = threads;

  // Book and pondering
  s.USE_BOOK   = false;
  s.USE_PONDER = false;

  // Core search algorithms
  s.USE_ALPHABETA = false;
  s.USE_PVS       = false;
  s.USE_ASP       = false;

  // Quiescence search
  s.USE_QUIESCENCE      = false;
  s.USE_QS_STANDPAT_CUT = false;
  s.USE_QS_SEE          = false;

  // Transposition table
  s.USE_TT              = false;
  s.TT_SIZE_MB          = 64;
  s.USE_TT_VALUE        = false;
  s.USE_TT_PV_MOVE_SORT = false;
  s.USE_QS_TT           = false;
  s.USE_EVAL_TT         = false;

  // Syzygy tablebase probing
  s.USE_TB_PROBE_ROOT   = false;
  s.TB_ROOT_IMMEDIATE   = false;
  s.USE_TB_PROBE_SEARCH = false;
  s.USE_TB_PROBE_PV     = false;

  // Move sorting
  s.USE_KILLER_MOVES    = false;
  s.USE_HISTORY_COUNTER = false;
  s.USE_HISTORY_MOVES   = false;

  s.USE_IID = false;
  s.USE_IIR = false;

  // Pruning techniques
  s.USE_IMPROVING     = false; // Track if position is improving vs 2 plies ago
  s.USE_MDP           = false;
  s.USE_RAZORING      = false;
  s.USE_RFP           = false;
  s.USE_RFP_IMPROVING = false;
  s.USE_NMP           = false;
  s.USE_NMP_VERIFY    = false;
  s.USE_NMP_IMPROVING = false;

  // Futility pruning
  s.USE_FP           = false;
  s.USE_QFP          = false;
  s.USE_FP_IMPROVING = false;

  // Late move reductions
  s.USE_LMR               = false;
  s.LMR_MIN_DEPTH         = 1;
  s.LMR_MIN_MOVES         = 3;
  s.LMR_USE_LOG_FORMULA   = false;
  s.LMR_LOG_BASE_DIV      = 2.00;
  s.USE_LMR_IMPROVING     = false;
  s.USE_LMR_CUTNODE       = false;
  s.LMR_CUTNODE_REDUCTION = 2;
  s.USE_LMR_HISTORY       = false;
  s.LMR_HISTORY_DIVISOR   = 8192;

  // Late move pruning
  s.USE_LMP           = false;
  s.USE_LMP_IMPROVING = false;

  // Extensions
  s.USE_EXTENSIONS        = false;
  s.USE_CHECK_EXT         = false;
  s.USE_CHECK_EXT_SEE     = false;
  s.USE_THREAT_EXT        = false;
  s.USE_EXT_ADD_DEPTH     = false;
  s.USE_SINGULAR_EXT      = false;

  // Best-move instability time management (disable for fixed-depth tests)
  s.USE_BESTMOVE_INSTABILITY = false;
  s.USE_EVAL_VOLATILITY      = false;
  CONFIG_OVERRIDE_END();

  // ***********************************
  // TESTS - Ordered by impact/importance
  // ***********************************

  // =====================================================================
  // GROUP 1: CORE SEARCH ALGORITHMS (Fundamental - highest impact)
  // =====================================================================

  // 1.1 Alpha-Beta: The foundation - massive pruning over MiniMax
  CONFIG_OVERRIDE(s.USE_ALPHABETA = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "AlphaBeta"));

  // 1.2 Principal Variation Search: Improves on alpha-beta with null-window
  CONFIG_OVERRIDE(s.USE_PVS = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "PVS"));

  // 1.3 Aspiration Windows: Narrow search window around expected value
  CONFIG_OVERRIDE(s.USE_ASP = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Aspiration"));

  // =====================================================================
  // GROUP 2: TRANSPOSITION TABLE (Critical for avoiding redundant work)
  // =====================================================================

  // 2.1 TT Core: Hash table for position caching
  CONFIG_OVERRIDE(s.USE_TT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TT"));

  // 2.2 TT Move Ordering: Use TT best move for sorting
  CONFIG_OVERRIDE(s.USE_TT_PV_MOVE_SORT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TT PV Sort"));

  // 2.3 TT Cutoffs: Use TT values for immediate cutoffs
  CONFIG_OVERRIDE(s.USE_TT_VALUE = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TT Cutoffs"));

  // 2.4 Eval TT: Cache evaluation scores
  CONFIG_OVERRIDE(s.USE_EVAL_TT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TT Eval"));

  // =====================================================================
  // GROUP 3: MOVE ORDERING (Critical for alpha-beta/PVS efficiency)
  // =====================================================================

  // 3.1a Internal Iterative Deepening (IID): Find good move when no TT hit (legacy)
  // CONFIG_OVERRIDE(s.USE_IID = true;);
  // CONFIG_OVERRIDE(s.USE_IIR = false;);
  // CONFIG_OVERRIDE(s.IID_DEPTH = 6;);
  // CONFIG_OVERRIDE(s.IID_REDUCTION = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "IID"));

  // 3.1b Internal Iterative Reduction (IIR): Modern alternative to IID
  // Note: IID and IIR are mutually exclusive - only enable one for testing
  CONFIG_OVERRIDE(s.USE_IID = false;);
  CONFIG_OVERRIDE(s.USE_IIR = true;);
  CONFIG_OVERRIDE(s.IIR_DEPTH = 4;);
  CONFIG_OVERRIDE(s.IIR_REDUCTION = 2;);
  // CONFIG_OVERRIDE(s.IIR_ALL_NODES = false;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "IIR PV Only"));
  CONFIG_OVERRIDE(s.IIR_ALL_NODES = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "IIR All Nodes"));

  // 3.2 Killer Moves: Remember refutation moves
  CONFIG_OVERRIDE(s.USE_KILLER_MOVES = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Killers"));

  // 3.3 History Heuristic: Score moves by success history
  CONFIG_OVERRIDE(s.USE_HISTORY_MOVES = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "History"));

  // 3.4 Counter Move History: Track response moves
  CONFIG_OVERRIDE(s.USE_HISTORY_COUNTER = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "CounterMove"));

  // =====================================================================
  // GROUP 4: QUIESCENCE SEARCH (Essential for tactical correctness)
  // =====================================================================

  // 4.1 Quiescence: Extend search until quiet position
  CONFIG_OVERRIDE(s.USE_QUIESCENCE = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Quiescence"));

  // 4.2 QS TT: Use TT in quiescence
  CONFIG_OVERRIDE(s.USE_QS_TT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "QS TT"));

  // 4.3 QS Stand-Pat: Allow standing pat if position is good
  CONFIG_OVERRIDE(s.USE_QS_STANDPAT_CUT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "QS StandPat"));

  // 4.4 QS SEE: Static Exchange Evaluation for capture ordering
  CONFIG_OVERRIDE(s.USE_QS_SEE = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "QS SEE"));

  // =====================================================================
  // GROUP 5: NULL MOVE PRUNING (Very high impact pruning)
  // =====================================================================

  CONFIG_OVERRIDE(s.USE_IMPROVING = true;);

  // 5.1 Null Move Pruning: Skip move and search with reduced depth
  CONFIG_OVERRIDE(s.USE_NMP = true;);
  CONFIG_OVERRIDE(s.NMP_DEPTH = 3;);
  CONFIG_OVERRIDE(s.NMP_REDUCTION = 2;);
  CONFIG_OVERRIDE(s.NMP_NEAR_MATE_MARGIN = 64;);
  CONFIG_OVERRIDE(s.USE_NMP_ZUG_GUARD = true;);
  CONFIG_OVERRIDE(s.NMP_ZUG_NONPAWN_THRESHOLD = 0;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "NMP"));

  // 5.2 NMP Verification: Re-search to avoid zugzwang issues
  CONFIG_OVERRIDE(s.USE_NMP_VERIFY = true;);
  CONFIG_OVERRIDE(s.NMP_VERIFY_MIN_DEPTH = 6;);
  CONFIG_OVERRIDE(s.NMP_VERIFY_MARGIN = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "NMP Verify"));

  // 5.3 NMP + Improving: Extra reduction when not improving
  CONFIG_OVERRIDE(s.USE_NMP_IMPROVING = true;);
  CONFIG_OVERRIDE(s.NMP_IMPROVING_REDUCTION = 1;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "NMP+Improving"));

  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "NMP"));

  // =====================================================================
  // GROUP 6: LATE MOVE REDUCTIONS (Major pruning technique)
  // =====================================================================

  // 6.1 LMR Core: Reduce depth for late moves
  CONFIG_OVERRIDE(s.USE_LMR = true;);
  CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 2;);
  CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR"));

  // 6.2 LMR Log Divisor Tuning
  CONFIG_OVERRIDE(s.LMR_USE_LOG_FORMULA = true;);
  CONFIG_OVERRIDE(s.LMR_LOG_BASE_DIV = 1.25;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR div=1.25"));

  // 6.3 LMR Min Depth/Moves Grid Search
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 1; s.LMR_MIN_MOVES = 1;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d1m1"));
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 1; s.LMR_MIN_MOVES = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d1m2"));
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 1; s.LMR_MIN_MOVES = 3;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d1m3"));
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 2; s.LMR_MIN_MOVES = 1;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d2m1"));
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 2; s.LMR_MIN_MOVES = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d2m2"));
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 2; s.LMR_MIN_MOVES = 3;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d2m3"));
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 3; s.LMR_MIN_MOVES = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d3m2"));
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 3; s.LMR_MIN_MOVES = 3;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d3m3"));

  // 6.4 LMR + Improving: Extra reduction when not improving
  CONFIG_OVERRIDE(s.USE_LMR_IMPROVING = true;);
  CONFIG_OVERRIDE(s.LMR_IMPROVING_REDUCTION = 1;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR+Impr r1"));
  // CONFIG_OVERRIDE(s.LMR_IMPROVING_REDUCTION = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR+Impr r2"));

  // 6.5 LMR + Cut Node: Extra reduction on expected cut nodes
  CONFIG_OVERRIDE(s.USE_LMR_CUTNODE = true;);
  CONFIG_OVERRIDE(s.LMR_CUTNODE_REDUCTION = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR+CutNode r2"));
  // CONFIG_OVERRIDE(s.LMR_CUTNODE_REDUCTION = 3;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR+CutNode r3"));

  // 6.6 LMR + History: Less reduction for good history moves
  CONFIG_OVERRIDE(s.USE_LMR_HISTORY = true; s.LMR_HISTORY_DIVISOR = 8192;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR+Hist 8192"));
  // CONFIG_OVERRIDE(s.LMR_HISTORY_DIVISOR = 4096;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR+Hist 4096"));

  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR"));

  // =====================================================================
  // GROUP 7: FORWARD PRUNING (Aggressive node reduction)
  // =====================================================================

  // 7.1 Mate Distance Pruning: Prune if mate already found
  CONFIG_OVERRIDE(s.USE_MDP = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "MDP"));

  // 7.2 Reverse Futility Pruning: Static eval cutoff
  CONFIG_OVERRIDE(s.USE_RFP = true;);
  CONFIG_OVERRIDE_START();
  s.RFP_MARGIN = std::array{0, 200, 400, 800};
  CONFIG_OVERRIDE_END();
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "RFP"));

  // 7.3 RFP + Improving: Reduce margin when not improving
  CONFIG_OVERRIDE(s.USE_RFP_IMPROVING = true;);
  CONFIG_OVERRIDE(s.RFP_IMPROVING_MARGIN = 40;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "RFP+Improving"));

  // 7.4 Razoring: Drop into QS if position looks bad
  CONFIG_OVERRIDE(s.USE_RAZORING = true;);
  CONFIG_OVERRIDE(s.RAZOR_MARGIN = 531;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Razoring"));

  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Forward Pruning"));

  // =====================================================================
  // GROUP 8: FUTILITY PRUNING (Prune hopeless moves)
  // =====================================================================

  // 8.1 Futility Pruning: Skip moves that can't raise alpha
  CONFIG_OVERRIDE(s.USE_FP = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "FP"));

  // 8.2 FP + Improving: Reduce margin when not improving
  CONFIG_OVERRIDE(s.USE_FP_IMPROVING = true;);
  CONFIG_OVERRIDE(s.FP_IMPROVING_MARGIN = 80;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "FP+Improving"));

  // 8.1 QS Futility Pruning: Futility in quiescence
  CONFIG_OVERRIDE(s.USE_QFP = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "QFP"));

  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Futil Pruning"));

  // =====================================================================
  // GROUP 9: LATE MOVE PRUNING (Prune late quiet moves)
  // =====================================================================

  // TODO: Needs tuning and more test points.

  // 9.1 Late Move Pruning: Skip late quiet moves at low depth
  CONFIG_OVERRIDE(s.USE_LMP = true;);
  CONFIG_OVERRIDE_START();
  s.LMP_MOVES = std::array{0, 7, 9, 11, 13, 15, 17, 19, 22, 24, 27, 29, 32, 35, 38, 41};
  CONFIG_OVERRIDE_END();
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMP"));

  // 9.2 LMP + Improving: Allow more moves when improving
  CONFIG_OVERRIDE(s.USE_LMP_IMPROVING = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMP+Improving"));

  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMP"));

  // =====================================================================
  // GROUP 10: SEARCH EXTENSIONS (Extend promising lines)
  // =====================================================================

  // 10.1 Extensions Core: Enable extension framework + AddDepth (realistic mode)
  CONFIG_OVERRIDE(s.USE_EXTENSIONS = true;);
  CONFIG_OVERRIDE(s.USE_EXT_ADD_DEPTH = true;); // Enable early for realistic testing

  // 10.2 Check Extension: Extend when move gives check
  CONFIG_OVERRIDE(s.USE_CHECK_EXT = true;);
  CONFIG_OVERRIDE(s.CHECK_EXT_MIN_DEPTH = 2;);
  CONFIG_OVERRIDE(s.CHECK_EXT_EARLY_LIMIT = 3;); // Test with old limit first
  CONFIG_OVERRIDE(s.USE_CHECK_EXT_SEE = false;); // Test without SEE first
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Ext Check"));

  // 10.2b Check Extension + SEE: Only extend non-losing checks
  CONFIG_OVERRIDE(s.USE_CHECK_EXT_SEE = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Ext Check+SEE"));

  // 10.2c Check Extension + SEE + No Limit (NEW DEFAULT)
  // With SEE filtering, we can safely extend all non-losing checks
  CONFIG_OVERRIDE(s.CHECK_EXT_EARLY_LIMIT = 99;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Ext Check+SEE NoLim"));

  // 10.3 Singular Extension: Extend when one move is clearly best
  CONFIG_OVERRIDE(s.USE_SINGULAR_EXT = true;);
  CONFIG_OVERRIDE(s.SINGULAR_MARGIN = 64;);
  CONFIG_OVERRIDE(s.SINGULAR_MIN_DEPTH = 8;); // Lowered from 8 to trigger more often
  CONFIG_OVERRIDE(s.SINGULAR_REDUCTION = 4;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Ext Sing NoTTBound"));

  // 10.4 Threat Extension: Extend on threat detection (experimental)
  CONFIG_OVERRIDE(s.USE_THREAT_EXT = true;);
  // CONFIG_OVERRIDE(s.THREAT_EXT_MATE_DEPTH = 3;);  // Test with mate-in-4 threshold first
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Ext Threat 3"));
  CONFIG_OVERRIDE(s.THREAT_EXT_MATE_DEPTH = 4;); // Test with mate-in-4 threshold first
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Ext Threat 4"));
  // CONFIG_OVERRIDE(s.THREAT_EXT_MATE_DEPTH = 6;);  // Test with mate-in-6 threshold
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Ext Threat 6"));

  // 10.5 Protection Only: Disable AddDepth to show effect of extension protection without extra depth
  // Extensions still protect moves from reductions (LMR/FP/LMP) but don't add depth
  // CONFIG_OVERRIDE(s.USE_EXT_ADD_DEPTH = false;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Ext NoAddDepth"));
  // Re-enable AddDepth for subsequent tests
  // CONFIG_OVERRIDE(s.USE_EXT_ADD_DEPTH = true;);

  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Extensions"));

  // =====================================================================
  // GROUP 11: SYZYGY TABLEBASES (Endgame perfection)
  // =====================================================================

  // TODO: Tablebases need more testing and tuning. They can have a huge impact on node count when
  //  they hit, but their effectiveness depends heavily on the position and how well the probing is
  //  integrated into the search. We should test various probing strategies and configurations to
  //  find the optimal approach.

  // 11.1 TB Root Probing: Probe TB at root for best move
  CONFIG_OVERRIDE(s.USE_TB_PROBE_ROOT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TB Root"));

  // 11.2 TB Root Immediate: Return TB result immediately
  // CONFIG_OVERRIDE(s.TB_ROOT_IMMEDIATE = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TB Root Immed"));

  // 11.3 TB Search Probing: Probe TB during search
  CONFIG_OVERRIDE(s.USE_TB_PROBE_SEARCH = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TB Search"));

  // 11.4 TB PV Probing: Probe TB on PV nodes
  CONFIG_OVERRIDE(s.USE_TB_PROBE_PV = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TB PV"));

  // CONFIG_OVERRIDE(s.TB_PROBE_DEPTH = 1;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TB ProbeDepth 1"));
  // CONFIG_OVERRIDE(s.TB_PROBE_DEPTH = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TB ProbeDepth 2"));
  CONFIG_OVERRIDE(s.TB_PROBE_DEPTH = 4;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TB ProbeDepth 4"));
  // CONFIG_OVERRIDE(s.TB_PROBE_DEPTH = 6;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "TB ProbeDepth 6"));

  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "Tablebases"));

  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "All Features"));

  // =====================================================================
  // WARMUP/BASELINE - Must be before first actual test
  // =====================================================================
  ptrToSpecial1 = &search.getSearchStats().betaCutsByIndex[0];
  ptrToSpecial2 = &search.getSearchStats().betaCutsByIndex[1];
  ptrToSpecial3 = &search.getSearchStats().pvNodes;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "Warmup"));
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "Baseline"));

  // ADD NEW FEATURE TESTS HERE (after baseline)

  return result;
}

void SearchTreeSizeTest::start() {

  fprintln("Start Search Tree Size Test for depth {}", depth);

  // Prepare test fens
  results.clear();
  results.reserve(fens.size());

  // Execute tests and store results
  for (auto& fen : fens) {
    try {
      const Position testPosition(fen);
      (void) testPosition; // avoid unused variable warning
    } catch (std::invalid_argument& e) {
      std::cerr << std::format("Invalid fen skipped: {} ({})", e.what(), fen) << std::endl;
      continue;
    }
    results.push_back(featureMeasurements(depth, movetime, fen));
  }

  // Print result
  NEWLINE;
  fprintln("################## Results for depth {} ##########################", depth);
  NEWLINE;
  fprintln("{:<20} | {:>6} | {:>8} | {:>15} | {:>12} | {:>12} | {:>7} | {:>15} | {:>15} | {:>15} | {} | {}",
           "Test Name", "Move", "Value", "Nodes", "Nps", "Time", "Depth", "Special1", "Special2", "Special3", "PV", "Fen");
  println("-----------------------------------------------------------------------"
          "---------------------------------------------------------------------------------");

  setlocale(LC_NUMERIC, "de_DE.UTF-8");
  std::unordered_map<std::string, SearchTreeSize::TestSums> sums{};

  for (const SearchTreeSize::Result& result : results) {
    fprintln("Fen: {}", result.fen);
    // ReSharper disable once CppUseStructuredBinding
    for (const SearchTreeSize::SingleTest& test : result.tests) {
      sums[test.name].sumCounter++;
      sums[test.name].sumNodes += test.nodes;
      sums[test.name].sumNps += test.nps;
      sums[test.name].sumTime += test.time;
      sums[test.name].sumDepth += test.depth;
      sums[test.name].sumExtra += test.extra;
      sums[test.name].special1 += test.special1;
      sums[test.name].special2 += test.special2;
      sums[test.name].special3 += test.special3;

      fprintln("{:<20} | {:>6} | {:>8} | {:>15L} | {:>12L} | {:>12L} | {:>3d}/{:<3d} | {:>15L} | {:>15L} | {:>15L} | {} | {}",
               test.name, test.move.str(), test.value.str(), test.nodes, test.nps,
               test.time / 1'000'000, test.depth, test.extra, test.special1, test.special2, test.special3, test.pv, result.fen);
    }
    NEWLINE;
  }

  println("----------------------------------------------------------------------------------------------------------------------------------------------");
  println("\n################## Totals/Avg results for each feature test ##################\n");
  fprintln("Date:                  : {}", format_now());
  fprintln("SearchTime             : {}", str(movetime));
  fprintln("MaxDepth               : {:d}", depth);
  fprintln("Threads                : {:d}", threads);
  fprintln("Number of feature tests: {:d}", results[0].tests.size());
  fprintln("Number of fens         : {:d}", fens.size());
  fprintln("Total tests            : {:d}\n", results[0].tests.size() * fens.size());

  // ReSharper disable once CppUseStructuredBinding
  for (const auto& test : results[0].tests) {
    const auto& sum        = sums[test.name];
    const auto avgSpecial1 = sum.special1 / sum.sumCounter;
    const auto avgSpecial2 = sum.special2 / sum.sumCounter;
    const auto avgSpecial3 = sum.special3 / sum.sumCounter;
    fprintln("Test: {:<20s}  Nodes: {:>16L}  Nps: {:>16L}  Time: {:>16L} Depth: {:>3d}/{:<3d} Special1: {:>12L} ({:>6L}) Special2: {:>12L} ({:>6L}) Special3: {:>12L} ({:>6L})", test.name.c_str(),
             sum.sumNodes / sum.sumCounter, sum.sumNps / sum.sumCounter,
             (sum.sumTime / 1'000'000) / sum.sumCounter, sum.sumDepth / sum.sumCounter, sum.sumExtra / sum.sumCounter,
             sum.special1, avgSpecial1, sum.special2, avgSpecial2, sum.special3, avgSpecial3);
  }
}

SearchTreeSize::SingleTest SearchTreeSizeTest::measureTreeSize(Search& search, const Position& position,
                                                               const SearchLimits& searchLimits, const std::string& featureName) const {

  NEWLINE;
  fprintln("Testing {} ####################################", featureName);
  fprintln("Position {}", position.strFen());
  NEWLINE;
  search.newGame();
  search.startSearch(position, searchLimits);
  search.waitWhileSearching();

  SearchTreeSize::SingleTest test{};
  test.name     = featureName;
  test.nodes    = search.getLastSearchResult().nodes;
  test.move     = search.getLastSearchResult().bestMove;
  test.value    = search.getLastSearchResult().bestMoveValue;
  test.nps      = nps(search.getLastSearchResult().nodes, search.getLastSearchResult().time);
  test.time     = search.getLastSearchResult().time.count();
  test.depth    = search.getLastSearchResult().depth;
  test.extra    = search.getLastSearchResult().extraDepth;
  test.special1 = ptrToSpecial1 ? *ptrToSpecial1 : 0;
  test.special2 = ptrToSpecial2 ? *ptrToSpecial2 : 0;
  test.special3 = ptrToSpecial3 ? *ptrToSpecial3 : 0;
  test.pv       = search.getPV().str();

  return test;
}

#endif // FRANKYCPP_PRODUCTION
