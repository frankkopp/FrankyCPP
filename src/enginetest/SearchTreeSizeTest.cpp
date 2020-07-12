/*
 * MIT License
 *
 * Copyright (c) 2018 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "SearchTreeSizeTest.h"
#include <engine/SearchConfig.h>

Result SearchTreeSizeTest::featureMeasurements(int depth, MilliSec movetime, const std::string& fen) {
  Search search{};
  SearchLimits searchLimits{};
  searchLimits.depth = depth;
  if (movetime != MilliSec::zero()) {
    searchLimits.moveTime    = movetime;
    searchLimits.timeControl = true;
  }
  Result result(fen);
  Position position(fen);

  // turn off all options
  SearchConfig::USE_BOOK   = false;
  SearchConfig::USE_PONDER = false;

  SearchConfig::USE_ALPHABETA = false;
  SearchConfig::USE_PVS       = false;
  SearchConfig::USE_ASP       = false;

  SearchConfig::USE_QUIESCENCE      = false;
  SearchConfig::USE_QS_STANDPAT_CUT = false;
  SearchConfig::USE_QS_SEE          = false;

  SearchConfig::USE_TT              = false;
  SearchConfig::TT_SIZE_MB          = 64;
  SearchConfig::USE_TT_VALUE        = false;
  SearchConfig::USE_TT_PV_MOVE_SORT = false;
  SearchConfig::USE_QS_TT           = false;
  SearchConfig::USE_EVAL_TT         = false;

  SearchConfig::USE_TT_PV_MOVE_SORT = false;
  SearchConfig::USE_KILLER_MOVES    = false;
  SearchConfig::USE_HISTORY_COUNTER = false;
  SearchConfig::USE_HISTORY_MOVES   = false;

  SearchConfig::USE_MDP      = false;
  SearchConfig::USE_RAZORING = false;
  SearchConfig::USE_RFP      = false;
  SearchConfig::USE_NMP      = false;
  SearchConfig::USE_IID      = false;

  SearchConfig::USE_FP  = false;
  SearchConfig::USE_QFP = false;
  SearchConfig::USE_LMR = false;
  SearchConfig::USE_LMP = false;

  SearchConfig::USE_EXTENSIONS    = false;
  SearchConfig::USE_CHECK_EXT     = false;
  SearchConfig::USE_THREAT_EXT    = false;
  SearchConfig::USE_EXT_ADD_DEPTH = false;

  // ***********************************
  // TESTS

  ptrToSpecial1 = &search.getSearchStats().checkExtension;
  ptrToSpecial2 = &search.getSearchStats().threatExtension;

  // pure MiniMax
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "00 MINIMAX"));

  SearchConfig::USE_ALPHABETA = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "10 AlphaBeta"));

  SearchConfig::USE_PVS = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "15 PVS"));

  SearchConfig::USE_ASP = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "18 ASP"));

  SearchConfig::USE_KILLER_MOVES    = true;
  SearchConfig::USE_HISTORY_COUNTER = true;
  SearchConfig::USE_HISTORY_MOVES   = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "20 History"));

  SearchConfig::USE_IID = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "25 IID"));

  SearchConfig::USE_TT = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "30 TT"));

  //  SearchConfig::TT_SIZE_MB = 1'024;
  //  search.resizeTT();
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "23 TT 1.024"));

  SearchConfig::USE_TT_PV_MOVE_SORT = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "35 PVSort"));

  SearchConfig::USE_TT_VALUE = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "36 TT Cuts"));

  SearchConfig::USE_EVAL_TT = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "37 TT Eval"));

  SearchConfig::USE_QUIESCENCE = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "40 QS"));

  SearchConfig::USE_QS_TT = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "41 QS TT"));

  SearchConfig::USE_QS_STANDPAT_CUT = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "42 QS SPAT"));

  SearchConfig::USE_QS_SEE = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "43 QS SEE"));

  SearchConfig::USE_MDP = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "50 MDP"));

  SearchConfig::USE_RAZORING = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "50 RAZOR"));

  SearchConfig::USE_RFP = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "51 RFP"));

  SearchConfig::USE_NMP = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "52 NMP"));

  SearchConfig::USE_FP = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "60 FP"));

  SearchConfig::USE_LMR = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "65 LMR"));

  SearchConfig::USE_LMP = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "66 LMP"));

  SearchConfig::USE_QFP = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "67 QFP"));

  SearchConfig::USE_EXTENSIONS    = true;
  SearchConfig::USE_EXT_ADD_DEPTH = true;
  SearchConfig::USE_CHECK_EXT     = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "70 CEXT"));
  //  SearchConfig::USE_THREAT_EXT = true;
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "71 TEXT"));

  return result;
}

void SearchTreeSizeTest::start() {

  fprintln("Start Search Tree Size Test for depth {}", depth);

  // Prepare test fens
  results.clear();
  results.reserve(fens.size());

  // Execute tests and store results
  for (auto fen = fens.begin(); fen != fens.end(); ++fen) {
    results.push_back(featureMeasurements(depth, movetime, *fen));
  }

  // Print result
  NEWLINE;
  fmt::print("################## Results for depth {} ##########################\n", depth);
  NEWLINE;
  fmt::print("{:<15s} | {:>6s} | {:>8s} | {:>15s} | {:>12s} | {:>12s} | {:>7s} | {:>12s} | {:>12s} | {} | {}\n",
             "Test Name", "Move", "Value", "Nodes", "Nps", "Time", "Depth", "Special1", "Special2", "PV", "Fen");
  println("-----------------------------------------------------------------------"
          "-----------------------------------------------------------------------");

  setlocale(LC_NUMERIC, "de_DE.UTF-8");
  std::map<std::string, TestSums> sums{};

  for (const Result& result : results) {
    fprintln("Fen: {}", result.fen);
    for (const SingleTest& test : result.tests) {
      sums[test.name].sumCounter++;
      sums[test.name].sumNodes += test.nodes;
      sums[test.name].sumNps += test.nps;
      sums[test.name].sumTime += test.time;
      sums[test.name].sumDepth += test.depth;
      sums[test.name].sumExtra += test.extra;
      sums[test.name].special1 += test.special1;
      sums[test.name].special2 += test.special2;

      fprintln("{:<15s} | {:>6s} | {:>8s} | {:>15n} | {:>12n} | {:>12n} | {:>3d}/{:<3d} | {:>12n} | {:>12n} | {} | {}",
               test.name, str(test.move), str(test.value), test.nodes, test.nps,
               (test.time / 1'000'000), test.depth, test.extra, test.special1, test.special2, test.pv, result.fen);
    }
    fmt::print("\n");
  }

  NEWLINE;

  fmt::print("----------------------------------------------------------------------------------------------------------------------------------------------");
  fmt::print("\n################## Totals/Avg results for each feature test ##################\n\n");

  std::time_t t = time(nullptr);
  fmt::print("Date                   : {:s}", ctime(&t));
  fmt::print("SearchTime             : {:s}\n", str(movetime));
  fmt::print("MaxDepth               : {:d}\n", depth);
  fmt::print("Number of feature tests: {:d}\n", results[0].tests.size());
  fmt::print("Number of fens         : {:d}\n", fens.size());
  fmt::print("Total tests            : {:d}\n\n", results[0].tests.size() * fens.size());

  for (auto& sum : sums) {
    fprintln("Test: {:<12s}  Nodes: {:>16n}  Nps: {:>16n}  Time: {:>16n} Depth: {:>3d}/{:<3d} Special1: {:>16n} Special2: {:>16n}", sum.first.c_str(),
             sum.second.sumNodes / sum.second.sumCounter, sum.second.sumNps / sum.second.sumCounter,
             (sum.second.sumTime / 1'000'000) / sum.second.sumCounter, sum.second.sumDepth / sum.second.sumCounter, sum.second.sumExtra / sum.second.sumCounter,
             sum.second.special1 / sum.second.sumCounter, sum.second.special2 / sum.second.sumCounter);
  }
}

SingleTest SearchTreeSizeTest::measureTreeSize(Search& search, const Position& position,
                                               SearchLimits searchLimits, const std::string& featureName) const {

  NEWLINE;
  fprintln("Testing {} ####################################", featureName);
  fprintln("Position {}", position.strFen());
  NEWLINE;
  search.newGame();
  search.startSearch(position, std::move(searchLimits));
  search.waitWhileSearching();

  SingleTest test{};
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
  test.pv       = str(search.getPV());

  return test;
}
