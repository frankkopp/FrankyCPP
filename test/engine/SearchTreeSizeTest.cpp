/*
 * MIT License
 *
 * Copyright (c) 2018-2020 Frank Kopp
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

#include <ctime>
#include <utility>

#include "Test_Fens.h"
#include "chesscore/Position.h"
#include "common/Logging.h"
#include "engine/Search.h"
#include "engine/SearchConfig.h"
#include "init.h"
#include "types/types.h"


#include <gtest/gtest.h>
using testing::Eq;

class SearchTreeSizeTest : public ::testing::Test {
public:
  static constexpr int DEPTH = 8;
  static constexpr MilliSec MOVE_TIME{0};
  static constexpr int START_FEN = 0;
  static constexpr int END_FEN   = 15;

  /* special is used to collect a dedicated stat */
  const uint64_t* ptrToSpecial1 = nullptr;
  const uint64_t* ptrToSpecial2 = nullptr;

  struct SingleTest {
    std::string name;
    uint64_t nodes    = 0;
    uint64_t nps      = 0;
    uint64_t depth    = 0;
    uint64_t extra    = 0;
    uint64_t time     = 0;
    uint64_t special1 = 0;
    uint64_t special2 = 0;
    Move move         = MOVE_NONE;
    Value value       = VALUE_NONE;
    std::string pv;
  };

  struct Result {
    std::string fen;
    std::vector<SingleTest> tests{};

    explicit Result(std::string _fen) : fen(std::move(_fen)){};
  };

  struct TestSums {
    uint64_t sumCounter{};
    uint64_t sumNodes{};
    uint64_t sumNps{};
    uint64_t sumDepth{};
    uint64_t sumExtra{};
    uint64_t sumTime{};
    uint64_t special1{};
    uint64_t special2{};
  };

  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::debug);
    Logger::get().TT_LOG->set_level(spdlog::level::debug);
    Logger::get().BOOK_LOG->set_level(spdlog::level::warn);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}

  Result featureMeasurements(int depth, MilliSec movetime, const std::string& fen);
  SingleTest measureTreeSize(Search& search, const Position& position, SearchLimits searchLimits, const std::string& featureName) const;
};

TEST_F(SearchTreeSizeTest, size_test) {
#ifndef NDEBUG
  GTEST_SKIP();
#endif
  fprintln("Start Search Tree Size Test for depth {}", DEPTH);

  // Prepare test fens
  std::vector<std::string> fens = Test_Fens::getFENs();
  std::vector<Result> results{};
  results.reserve(fens.size());
  auto iterStart = fens.begin() + START_FEN;
  auto iterEnd   = fens.begin() + START_FEN + END_FEN;
  if (iterEnd > fens.end()) iterEnd = fens.end();
  if (iterStart > iterEnd) iterStart = iterEnd;

  // Execute tests and store results
  for (auto fen = iterStart; fen != iterEnd; ++fen) {
    results.push_back(featureMeasurements(DEPTH, MOVE_TIME, *fen));
  }

  // Print result
  NEWLINE;
  fmt::print("################## Results for depth {} ##########################\n", DEPTH);
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
  fmt::print("SearchTime             : {:s}\n", str(MOVE_TIME));
  fmt::print("MaxDepth               : {:d}\n", DEPTH);
  fmt::print("Number of feature tests: {:d}\n", results[0].tests.size());
  fmt::print("Number of fens         : {:d}\n", END_FEN - START_FEN);
  fmt::print("Total tests            : {:d}\n\n", results[0].tests.size() * END_FEN - START_FEN);

  for (auto& sum : sums) {
    fprintln("Test: {:<12s}  Nodes: {:>16n}  Nps: {:>16n}  Time: {:>16n} Depth: {:>3d}/{:<3d} Special1: {:>16n} Special2: {:>16n}", sum.first.c_str(),
             sum.second.sumNodes / sum.second.sumCounter, sum.second.sumNps / sum.second.sumCounter,
             (sum.second.sumTime / 1'000'000) / sum.second.sumCounter, sum.second.sumDepth / sum.second.sumCounter, sum.second.sumExtra / sum.second.sumCounter,
             sum.second.special1 / sum.second.sumCounter, sum.second.special2 / sum.second.sumCounter);
  }
}

SearchTreeSizeTest::Result SearchTreeSizeTest::featureMeasurements(int depth, MilliSec movetime, const std::string& fen) {
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

  SearchConfig::USE_QUIESCENCE = false;
  //  SearchConfig::MAX_EXTRA_QDEPTH      = Depth{20};
  //  SearchConfig::USE_QS_SEE            = false;

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

  SearchConfig::USE_MDP             = false;
  SearchConfig::USE_QS_STANDPAT_CUT = false;
  SearchConfig::USE_QS_SEE          = false;
  //  SearchConfig::USE_RFP               = false;
  //  SearchConfig::USE_NMP               = false;
  //  SearchConfig::NMP_VERIFICATION      = false;
  //  SearchConfig::USE_EXTENSIONS        = false;
  //  SearchConfig::USE_FP                = false;
  //  SearchConfig::USE_EFP               = false;
  //  SearchConfig::USE_LMR               = false;
  //  SearchConfig::USE_RAZOR_PRUNING = false;
  //  SearchConfig::USE_LMP           = false;

  // ***********************************
  // TESTS

  ptrToSpecial1 = &search.getSearchStats().evaluations;
  ptrToSpecial2 = &search.getSearchStats().evalFromTT;

  // pure MiniMax
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "00 MINIMAX"));

  SearchConfig::USE_ALPHABETA = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "10 AlphaBeta"));

  SearchConfig::USE_PVS = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "15 PVS"));

  SearchConfig::USE_KILLER_MOVES    = true;
  SearchConfig::USE_HISTORY_COUNTER = true;
  SearchConfig::USE_HISTORY_MOVES   = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "20 History"));

  SearchConfig::USE_MDP = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "25 MDP"));

  SearchConfig::USE_TT = true;
  result.tests.push_back(measureTreeSize(search, position, searchLimits, "32 TT"));

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

  //  SearchConfig::USE_EXTENSIONS = true;
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "40 EXT"));
  //
  //  SearchConfig::USE_FP = true;
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "50 FP"));
  //
  //  SearchConfig::USE_EFP = true;
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "60 EFP"));
  //
  //  SearchConfig::USE_RFP = true;
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "70 RFP"));
  //
  //  SearchConfig::USE_ASPIRATION_WINDOW = true;
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "80 ASP"));
  //
  //  SearchConfig::USE_LMR = true;
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "90 LMR"));
  //
  //  SearchConfig::USE_NMP = true;
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "99 NMP"));

  //  SearchConfig::USE_RAZOR_PRUNING = true;
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "90 RAZOR"));

  //  SearchConfig::USE_IID = true;
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "30 IID"));

  //  SearchConfig::USE_LMP = true;
  //  result.tests.push_back(measureTreeSize(search, position, searchLimits, "80 LMP"));

  // ***********************************

  return result;
}

SearchTreeSizeTest::SingleTest
SearchTreeSizeTest::measureTreeSize(Search& search, const Position& position,
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
