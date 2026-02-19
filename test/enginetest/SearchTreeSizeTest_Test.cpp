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

#include <ctime>

#include "Test_Fens.h"
#include "Test_Utils.h"
#include "common/Logging.h"
#include "engine/Search.h"
#include "enginetest/SearchTreeSizeTest.h"
#include "init.h"

#include <gtest/gtest.h>

using testing::Eq;


class SearchTreeSizeTest_Test : public testing::Test {
public:
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
};

TEST_F(SearchTreeSizeTest_Test, size_test) {
  static constexpr milliseconds MOVE_TIME{0};

  const int START_FEN = isBulkRun() ? 0 : 0;
  const int END_FEN   = isBulkRun() ? 4 : 30;
  const int DEPTH     = isBulkRun() ? 4 : 10;

  // Prepare test fens
  // get sub vector of fens to test
  std::vector<std::string> allFens = Test_Fens::getFENs();
  auto iterStart                   = allFens.begin() + START_FEN;
  auto iterEnd                     = allFens.begin() + START_FEN + END_FEN;
  if (iterEnd > allFens.end()) iterEnd = allFens.end();
  if (iterStart > iterEnd) iterStart = iterEnd;
  const std::vector testFens(iterStart, iterEnd);

  // execute tests
  SearchTreeSizeTest stst(DEPTH, MOVE_TIME, testFens);
  stst.start();
}

TEST_F(SearchTreeSizeTest_Test, 10secondSearchNodesCount) {
  if (isBulkRun()) {
    GTEST_SKIP() << "Skipping debug test in bulk run to save time";
  }

  const Position p{"5k2/1rn2p2/3pb1p1/7p/p3PP2/PnNBK2P/3N2P1/1R6 w - - 0 1 "};
  SearchLimits sl{};
  Search s{};
  sl.timeControl = true;
  sl.moveTime    = 16s;
  s.isReady();
  s.startSearch(p, sl);
  s.waitWhileSearching();

  // Use the new formatDetailedStats method (FEN is stored in SearchResult)
  fprintln("{}", s.formatDetailedStats());
}


/*
################## Totals/Avg results for each feature test ##################

Date:                  : 2025-09-29 15:15:14
SearchTime             : 0.000 s
MaxDepth               : 8
Number of feature tests: 21
Number of fens         : 20
Total tests            : 420

Test: 10 AlphaBeta  Nodes:       12.766.281  Nps:        6.006.681  Time:            2.116 Depth:   8/8   Special1:                0 Special2:                0
Test: 15 PVS        Nodes:        8.203.912  Nps:        5.860.349  Time:            1.379 Depth:   8/8   Special1:                0 Special2:                0
Test: 18 ASP        Nodes:        8.550.269  Nps:        5.901.356  Time:            1.435 Depth:   8/8   Special1:                0 Special2:                0
Test: 20 History    Nodes:        7.688.573  Nps:        5.711.835  Time:            1.324 Depth:   8/8   Special1:                0 Special2:                0
Test: 25 IID        Nodes:        7.772.694  Nps:        5.810.893  Time:            1.330 Depth:   8/8   Special1:                0 Special2:                0
Test: 30 TT         Nodes:        7.689.835  Nps:        4.269.452  Time:            1.571 Depth:   8/8   Special1:        1.497.276 Special2:          649.706
Test: 35 PVSort     Nodes:        5.768.173  Nps:        5.168.289  Time:            1.092 Depth:   8/8   Special1:        1.227.944 Special2:          529.630
Test: 36 TT Cuts    Nodes:        2.266.578  Nps:        4.398.074  Time:              481 Depth:   8/8   Special1:          252.361 Special2:          472.820
Test: 37 TT Eval    Nodes:        2.266.578  Nps:        4.320.910  Time:              487 Depth:   8/8   Special1:          252.362 Special2:          472.820
Test: 41 QS TT      Nodes:        4.380.979  Nps:        2.785.247  Time:            1.484 Depth:   8/27  Special1:          732.758 Special2:        3.648.270
Test: 42 QS SPAT    Nodes:        1.583.266  Nps:        3.070.011  Time:              498 Depth:   8/18  Special1:          429.191 Special2:        1.154.106
Test: 43 QS SEE     Nodes:        1.561.537  Nps:        2.944.521  Time:              500 Depth:   8/18  Special1:          390.247 Special2:        1.171.325
Test: 50 MDP        Nodes:        1.251.968  Nps:        2.918.246  Time:              397 Depth:   8/17  Special1:          301.118 Special2:          948.570
Test: 50 RAZOR      Nodes:        1.151.381  Nps:        2.843.066  Time:              374 Depth:   8/16  Special1:          280.721 Special2:          872.206
Test: 51 RFP        Nodes:          721.472  Nps:        2.942.379  Time:              220 Depth:   8/16  Special1:          217.476 Special2:          505.662
Test: 52 NMP        Nodes:          234.484  Nps:        2.632.239  Time:               71 Depth:   8/15  Special1:           61.391 Special2:          173.831
Test: 60 FP         Nodes:          217.506  Nps:        2.459.789  Time:               71 Depth:   8/15  Special1:           55.778 Special2:          162.786
Test: 65 LMR        Nodes:          203.562  Nps:        2.442.830  Time:               66 Depth:   8/15  Special1:           52.146 Special2:          152.524
Test: 66 LMP        Nodes:          189.591  Nps:        2.370.515  Time:               63 Depth:   8/15  Special1:           46.261 Special2:          144.614
Test: 67 QFP        Nodes:          185.605  Nps:        2.338.179  Time:               63 Depth:   8/15  Special1:           46.032 Special2:          140.858
Test: 70 CEXT       Nodes:          219.606  Nps:        2.361.098  Time:               76 Depth:   8/16  Special1:           53.384 Special2:          168.198
*/
