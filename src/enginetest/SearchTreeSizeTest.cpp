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

#include <map>

// no longer use fmt chrono; use our time utilities
#include "types/timeunits.h"

#include "SearchTreeSizeTest.h"

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
  s.USE_IID             = false;

  // Pruning techniques
  s.USE_MDP        = false;
  s.USE_RAZORING   = false;
  s.USE_RFP        = false;
  s.USE_NMP        = false;
  s.USE_NMP_VERIFY = false;

  // Futility pruning
  s.USE_FP  = false;
  s.USE_QFP = false;

  // Late move reductions
  s.USE_LMR            = false;
  s.LMR_MIN_DEPTH      = 1;
  s.LMR_MIN_MOVES      = 3;
  s.LMR_USE_LOG_FORMULA = false;
  s.LMR_LOG_BASE_DIV   = 2.00;
  s.USE_LMR_IMPROVING  = false;

  // Late move pruning
  s.USE_LMP = false;

  // Extensions
  s.USE_EXTENSIONS    = false;
  s.USE_CHECK_EXT     = false;
  s.USE_THREAT_EXT    = false;
  s.USE_EXT_ADD_DEPTH = false;
  s.USE_SINGULAR_EXT  = false;

  // Best-move instability time management (disable for fixed-depth tests)
  s.USE_BESTMOVE_INSTABILITY = false;
  CONFIG_OVERRIDE_END();

  // ***********************************
  // TESTS

  ptrToSpecial1 = &search.getSearchStats().lmrReductions;
  ptrToSpecial2 = &search.getSearchStats().lmrResearches;

  // pure MiniMax
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "00 MINIMAX"));

  // Core search algorithms
  CONFIG_OVERRIDE(s.USE_ALPHABETA = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "10 AlphaBeta"));

  CONFIG_OVERRIDE(s.USE_PVS = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "15 PVS"));

  CONFIG_OVERRIDE(s.USE_ASP = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "18 ASP"));

  // Move sorting
  CONFIG_OVERRIDE(s.USE_KILLER_MOVES = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "20a Killer"));
  CONFIG_OVERRIDE(s.USE_HISTORY_COUNTER = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "20b HistCnt"));
  CONFIG_OVERRIDE(s.USE_HISTORY_MOVES = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "20c History"));

  CONFIG_OVERRIDE(s.USE_IID = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "25 IID"));

  // Transposition table
  CONFIG_OVERRIDE(s.USE_TT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "30 TT"));

  // CONFIG_OVERRIDE(s.TT_SIZE_MB = 1'024;);
  // search.resizeTT();
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "31 TT 1GB"));

  CONFIG_OVERRIDE(s.USE_TT_PV_MOVE_SORT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "35 PVSort"));

  CONFIG_OVERRIDE(s.USE_TT_VALUE = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "36 TT Cuts"));

  CONFIG_OVERRIDE(s.USE_EVAL_TT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "37 TT Eval"));

  // Quiescence search
  CONFIG_OVERRIDE(s.USE_QUIESCENCE = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "40 QS"));

  CONFIG_OVERRIDE(s.USE_QS_TT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "41 QS TT"));

  CONFIG_OVERRIDE(s.USE_QS_STANDPAT_CUT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "42 QS SPAT"));

  CONFIG_OVERRIDE(s.USE_QS_SEE = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "43 QS SEE"));

  // Pruning techniques
  CONFIG_OVERRIDE(s.USE_MDP = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "50 MDP"));

  CONFIG_OVERRIDE(s.USE_RAZORING = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "51 RAZOR"));

  CONFIG_OVERRIDE(s.USE_RFP = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "52 RFP"));

  CONFIG_OVERRIDE(s.USE_NMP = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "53 NMP"));

  CONFIG_OVERRIDE(s.USE_NMP_VERIFY = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "54 NMP Ver"));

  // Futility pruning
  CONFIG_OVERRIDE(s.USE_FP = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "60 FP"));

  CONFIG_OVERRIDE(s.USE_QFP = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "61 QFP"));

  // Late move pruning (NOT LMR - keep LMR off for LMR tests below)
  CONFIG_OVERRIDE(s.USE_LMP = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "66 LMP"));

  // Extensions
  CONFIG_OVERRIDE(s.USE_EXTENSIONS = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "70a EXT"));
  CONFIG_OVERRIDE(s.USE_EXT_ADD_DEPTH = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "70b ExtAdd"));
  CONFIG_OVERRIDE(s.USE_CHECK_EXT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "70c CEXT"));
  // CONFIG_OVERRIDE(s.USE_THREAT_EXT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "71 TEXT"));

  CONFIG_OVERRIDE(s.USE_SINGULAR_EXT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "72 SEXT"));

  // Syzygy tablebase probing
  CONFIG_OVERRIDE(s.USE_TB_PROBE_ROOT = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "80 TBRoot"));
  CONFIG_OVERRIDE(s.TB_ROOT_IMMEDIATE = false;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "81 TBRootS"));

  // CONFIG_OVERRIDE(s.TB_ROOT_IMMEDIATE = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "82 TBRooti"));

  CONFIG_OVERRIDE(s.USE_TB_PROBE_SEARCH = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "85 TBSearch"));
  CONFIG_OVERRIDE(s.USE_TB_PROBE_PV = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "86 TBPV"));

  // Note: LMR is intentionally kept OFF here - all features above are now enabled
  // CONFIG_OVERRIDE(s.USE_LMR = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "65 LMR"));

  // ***********************************
  // LMR Parameter Tuning Tests
  // ***********************************


  // Enable LMR for parameter tuning tests
  CONFIG_OVERRIDE(s.USE_LMR = true;);

  // // Test all combinations of LMR_MIN_DEPTH (1-4) x LMR_MIN_MOVES (1-4)
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 1;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 1;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d1m1"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 1;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d1m2"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 1;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 3;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d1m3"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 1;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 4;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d1m4"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 2;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 1;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d2m1"));

  CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 2;);
  CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "90 LMR d2m2"));

  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 2;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 3;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d2m3"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 2;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 4;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d2m4"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 3;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 1;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d3m1"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 3;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d3m2"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 3;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 3;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d3m3"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 3;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 4;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d3m4"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 4;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 1;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d4m1"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 4;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 2;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d4m2"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 4;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 3;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d4m3"));
  //
  // CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = 4;);
  // CONFIG_OVERRIDE(s.LMR_MIN_MOVES = 4;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR d4m4"));

  // Test logarithmic LMR formula (Stockfish-style)
  // Uses log(depth) * log(moves) / divisor instead of linear formula
  CONFIG_OVERRIDE(s.LMR_USE_LOG_FORMULA = true;);

  // CONFIG_OVERRIDE(s.LMR_LOG_BASE_DIV = 2.00;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR log/2.0"));

  // Test different divisor values for logarithmic formula
  CONFIG_OVERRIDE(s.LMR_LOG_BASE_DIV = 1.50;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "91 LMR log/1.5"));

  // CONFIG_OVERRIDE(s.LMR_LOG_BASE_DIV = 1.25;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR log/1.25"));
  //
  // CONFIG_OVERRIDE(s.LMR_LOG_BASE_DIV = 1.00;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "LMR log/1.0"));

  // ***********************************
  // LMR + Improving Flag Tests
  // ***********************************

  result.tests.push_back(measureTreeSize(search, position, searchLimits, "00 warmup"));
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "01 pre"));

  // Enable improving-based LMR: extra reduction when position is not improving
  CONFIG_OVERRIDE(s.USE_LMR_IMPROVING = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "92 LMR+Impr"));

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
      (void) testPosition;// avoid unused variable warning
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
  fprintln("{:<15} | {:>6} | {:>8} | {:>15} | {:>12} | {:>12} | {:>7} | {:>12} | {:>12} | {} | {}",
           "Test Name", "Move", "Value", "Nodes", "Nps", "Time", "Depth", "Special1", "Special2", "PV", "Fen");
  println("-----------------------------------------------------------------------"
          "-----------------------------------------------------------------------");

  setlocale(LC_NUMERIC, "de_DE.UTF-8");
  std::map<std::string, SearchTreeSize::TestSums> sums{};

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

      fprintln("{:<15} | {:>6} | {:>8} | {:>15L} | {:>12L} | {:>12L} | {:>3d}/{:<3d} | {:>12L} | {:>12L} | {} | {}",
               test.name, test.move.str(), test.value.str(), test.nodes, test.nps,
               test.time / 1'000'000, test.depth, test.extra, test.special1, test.special2, test.pv, result.fen);
    }
    NEWLINE;
  }

  println("----------------------------------------------------------------------------------------------------------------------------------------------");
  println("\n################## Totals/Avg results for each feature test ##################\n");
  fprintln("Date:                  : {}", format_now());
  fprintln("SearchTime             : {}", str(movetime));
  fprintln("MaxDepth               : {:d}", depth);
  fprintln("Number of feature tests: {:d}", results[0].tests.size());
  fprintln("Number of fens         : {:d}", fens.size());
  fprintln("Total tests            : {:d}\n", results[0].tests.size() * fens.size());

  // ReSharper disable once CppUseStructuredBinding
  for (const auto& sum : sums) {
    const auto avgSpecial1 = sum.second.special1 / sum.second.sumCounter;
    const auto avgSpecial2 = sum.second.special2 / sum.second.sumCounter;
    fprintln("Test: {:<12s}  Nodes: {:>16L}  Nps: {:>16L}  Time: {:>16L} Depth: {:>3d}/{:<3d} Special1: {:>10L} ({:>5L}) Special2: {:>10L} ({:>5L})", sum.first.c_str(),
             sum.second.sumNodes / sum.second.sumCounter, sum.second.sumNps / sum.second.sumCounter,
             (sum.second.sumTime / 1'000'000) / sum.second.sumCounter, sum.second.sumDepth / sum.second.sumCounter, sum.second.sumExtra / sum.second.sumCounter,
             sum.second.special1, avgSpecial1, sum.second.special2, avgSpecial2);
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
  test.pv       = search.getPV().str();

  return test;
}
