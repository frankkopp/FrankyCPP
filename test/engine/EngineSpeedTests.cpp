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

#include "chesscore/Position.h"
#include "engine/Search.h"
#include "init.h"
#include "types/types.h"

#include <gtest/gtest.h>

using testing::Eq;

using namespace std::chrono;
using namespace std;

class EngineSpeedTests : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::debug);
    Logger::get().TT_LOG->set_level(spdlog::level::debug);
    Logger::get().EVAL_LOG->set_level(spdlog::level::debug);
    Logger::get().UCIHAND_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

// 5.8. All Features: NPS: 2.788.209
// 5.8. All Features: NPS: 2.788.209 (+piece eval)
// 4.5.2025 GROOT: Search depth was 14(38) with 149.971.501 nodes visited. NPS = 5.000.489 nps
// 22.9.2025 GROOT: Search depth was 13(36) with 96.508.284 nodes visited. NPS = 3.218.665 nps
// (new evaluation features)
// 27.9.2925 GROOT: Search depth was 13(31) with 93.946.149 nodes visited. NPS = 3.716.830 nps
// after migrations to classes and time management changes
TEST_F(EngineSpeedTests, npsTest) {
  CONFIG_OVERRIDE_START()
  s.TT_SIZE_MB          = 64;
  s.USE_BOOK            = false;
  s.USE_ALPHABETA       = true;
  s.USE_PVS             = true;
  s.USE_TT              = true;
  s.USE_TT_VALUE        = true;
  s.USE_EVAL_TT         = true;
  s.USE_MDP             = true;
  s.USE_HISTORY_COUNTER = true;
  s.USE_HISTORY_MOVES   = true;
  s.USE_QUIESCENCE      = true;
  s.USE_QS_STANDPAT_CUT = true;
  s.USE_QS_SEE          = true;
  s.USE_QS_TT           = true;
  s.USE_RAZORING        = true;
  s.USE_RFP             = true;
  s.USE_NMP             = true;
  s.USE_IID             = true;
  s.USE_FP              = true;
  s.USE_LMR             = true;
  s.USE_LMP             = true;
  CONFIG_OVERRIDE_END();
  // EvalConfig::TEMPO                 = 34;
  // EvalConfig::USE_MATERIAL          = true;
  // EvalConfig::USE_POSITIONAL        = true;

  // EvalConfig::USE_PAWN_EVAL         = false;
  // EvalConfig::USE_PAWN_TT           = true;
  // EvalConfig::PAWN_TT_SIZE_MB       = 64;

  // EvalConfig::USE_PIECE_EVAL        = false;
  // EvalConfig::USE_KING_EVAL         = true;

  //  Position p{"2rr2k1/1p2qp1p/1pn1pp2/1N6/3P4/P6P/1P2QPP1/2R2RK1 w - - 0 1 "};
  const Position p{};
  Search s{};
  s.isReady();
  SearchLimits sl{};
  sl.timeControl = true;
  // sl.moveTime    = 30s;
  sl.whiteTime = 1000s;
  sl.blackTime = 1000s;
  s.startSearch(p, sl);
  EXPECT_TRUE(s.isSearching());
  EXPECT_FALSE(s.hasResult());
  s.waitWhileSearching();
  EXPECT_TRUE(s.hasResult());
  fprintln("NPS: {:L}", nps(s.getLastSearchResult().nodes, s.getLastSearchResult().time));
}
