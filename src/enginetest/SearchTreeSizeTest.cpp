// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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
    s.USE_BOOK   = false;
    s.USE_PONDER = false;

    s.USE_ALPHABETA = false;
    s.USE_PVS       = false;
    s.USE_ASP       = false;

    s.USE_QUIESCENCE      = false;
    s.USE_QS_STANDPAT_CUT = false;
    s.USE_QS_SEE          = false;

    s.USE_TT              = false;
    s.TT_SIZE_MB          = 64;
    s.USE_TT_VALUE        = false;
    s.USE_TT_PV_MOVE_SORT = false;
    s.USE_QS_TT           = false;
    s.USE_EVAL_TT         = false;

    s.USE_TT_PV_MOVE_SORT = false;
    s.USE_KILLER_MOVES    = false;
    s.USE_HISTORY_COUNTER = false;
    s.USE_HISTORY_MOVES   = false;

    s.USE_MDP        = false;
    s.USE_RAZORING   = false;
    s.USE_RFP        = false;
    s.USE_NMP        = false;
    s.USE_NMP_VERIFY = false;
    s.USE_IID        = false;

    s.USE_FP  = false;
    s.USE_QFP = false;
    s.USE_LMR = false;
    s.USE_LMP = false;

    s.USE_EXTENSIONS    = false;
    s.USE_CHECK_EXT     = false;
    s.USE_THREAT_EXT    = false;
    s.USE_EXT_ADD_DEPTH = false;
  CONFIG_OVERRIDE_END();

  // ***********************************
  // TESTS

  ptrToSpecial1 = &search.getSearchStats().ttHit;
  ptrToSpecial2 = &search.getSearchStats().ttMiss;

  // pure MiniMax
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "00 MINIMAX"));

  CONFIG_OVERRIDE(s.USE_ALPHABETA = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "10 AlphaBeta"));

  CONFIG_OVERRIDE(s.USE_PVS = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "15 PVS"));

  CONFIG_OVERRIDE(s.USE_ASP = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "18 ASP"));

  CONFIG_OVERRIDE(s.USE_KILLER_MOVES    = true;);
  CONFIG_OVERRIDE(s.USE_HISTORY_COUNTER = true;);
  CONFIG_OVERRIDE(s.USE_HISTORY_MOVES   = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "20 History"));

  CONFIG_OVERRIDE(s.USE_IID = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "25 IID"));

  CONFIG_OVERRIDE(s.USE_TT = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "30 TT"));

  //  CONFIG_OVERRIDE(s.TT_SIZE_MB = 1'024;);
  //  search.resizeTT();
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "23 TT 1.024"));

  CONFIG_OVERRIDE(s.USE_TT_PV_MOVE_SORT = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "35 PVSort"));

  CONFIG_OVERRIDE(s.USE_TT_VALUE = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "36 TT Cuts"));

  CONFIG_OVERRIDE(s.USE_EVAL_TT = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "37 TT Eval"));

  CONFIG_OVERRIDE(s.USE_QUIESCENCE = true;);
  // result.tests.push_back(measureTreeSize(search, position, searchLimits, "40 QS"));

  CONFIG_OVERRIDE(s.USE_QS_TT = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "41 QS TT"));

  CONFIG_OVERRIDE(s.USE_QS_STANDPAT_CUT = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "42 QS SPAT"));

  CONFIG_OVERRIDE(s.USE_QS_SEE = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "43 QS SEE"));

  CONFIG_OVERRIDE(s.USE_MDP = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "50 MDP"));

  CONFIG_OVERRIDE(s.USE_RAZORING = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "50 RAZOR"));

  CONFIG_OVERRIDE(s.USE_RFP = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "51 RFP"));

  CONFIG_OVERRIDE(s.USE_NMP = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "52 NMP"));

  CONFIG_OVERRIDE(s.USE_NMP_VERIFY = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "53 NMP Ver"));

  CONFIG_OVERRIDE(s.USE_FP = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "60 FP"));

  CONFIG_OVERRIDE(s.USE_LMR = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "65 LMR"));

  CONFIG_OVERRIDE(s.USE_LMP = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "66 LMP"));

  CONFIG_OVERRIDE(s.USE_QFP = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "67 QFP"));

  CONFIG_OVERRIDE(s.USE_EXTENSIONS    = true;);
  CONFIG_OVERRIDE(s.USE_EXT_ADD_DEPTH = true;);
  CONFIG_OVERRIDE(s.USE_CHECK_EXT     = true;);
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "70 CEXT"));
  //  CONFIG_OVERRIDE(s.USE_THREAT_EXT = true;);
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "71 TEXT"));

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
  for (auto& sum : sums) {
    fprintln("Test: {:<12s}  Nodes: {:>16L}  Nps: {:>16L}  Time: {:>16L} Depth: {:>3d}/{:<3d} Special1: {:>16L} Special2: {:>16L}", sum.first.c_str(),
             sum.second.sumNodes / sum.second.sumCounter, sum.second.sumNps / sum.second.sumCounter,
             (sum.second.sumTime / 1'000'000) / sum.second.sumCounter, sum.second.sumDepth / sum.second.sumCounter, sum.second.sumExtra / sum.second.sumCounter,
             sum.second.special1 / sum.second.sumCounter, sum.second.special2 / sum.second.sumCounter);
  }
}

SearchTreeSize::SingleTest SearchTreeSizeTest::measureTreeSize(Search& search, const Position& position,
                                                               SearchLimits searchLimits, const std::string& featureName) const {

  NEWLINE;
  fprintln("Testing {} ####################################", featureName);
  fprintln("Position {}", position.strFen());
  NEWLINE;
  search.newGame();
  search.startSearch(position, std::move(searchLimits));
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
