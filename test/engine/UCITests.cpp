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

#include <memory>
#include <ranges>
#include <thread>

#include "chesscore/Position.h"
#include "common/Logging.h"
#include "config/ConfigRegistry.h"
#include "engine/Search.h"
#include "engine/UciHandler.h"
#include "engine/UciOptions.h"
#include "init.h"

#include <gtest/gtest.h>

using testing::Eq;

using namespace std;
using namespace engine;
using namespace chess;
using namespace config;
using namespace common;

class UCITest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().UCIHAND_LOG->set_level(spdlog::level::debug);
    Logger::get().SEARCH_LOG->set_level(spdlog::level::debug);
    Logger::get().TT_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {
    CONFIG_OVERRIDE(s.USE_BOOK = false;);
  }
  void TearDown() override {}
};

TEST_F(UCITest, uciTest) {

  string command       = "uci";
  string expectedStart = "id name";
  string expectedEnd   = "uciok\n";

  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  ostringstream os;
  UciHandler uciHandler(&is, &os);
  uciHandler.loop();
  string result = os.str();
  LOG__DEBUG(Logger::get().TEST_LOG, "RESPONSE: \n{}", result);

  EXPECT_EQ(expectedStart, result.substr(0, 7));
  EXPECT_EQ(expectedEnd, result.substr(result.size() - 6, result.size()));
}

TEST_F(UCITest, isreadyTest) {
  string command  = "isready";
  string expected = "readyok\n";

  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  ostringstream os;
  UciHandler uciHandler(&is, &os);
  uciHandler.loop();
  LOG__DEBUG(Logger::get().TEST_LOG, "RESPONSE: {}", os.str());
  EXPECT_TRUE(os.str().find("readyok") != string::npos);
}

TEST_F(UCITest, setoptionTest) {
  ostringstream os;
  const string command = "setoption name Hash value 2048";
  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  UciHandler uciHandler(&is, &os);
  uciHandler.loop();
  EXPECT_EQ("2048", UciOptions::getInstance()->getOption("Hash")->currentValue);
}

TEST_F(UCITest, getoptionsTest) {
  // First set some options to non-default values
  {
    ostringstream os;
#ifdef FRANKYCPP_PRODUCTION
    // In production, Use Quiescence is CONFIG_CONST and not exposed as UCI option
    const string command = "setoption name Hash value 512";
#else
    const string command = "setoption name Hash value 512\nsetoption name Use Quiescence value false";
#endif
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
  }

  // Now test getoptions command
  ostringstream os;
  const string command = "getoptions";
  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  UciHandler uciHandler(&is, &os);
  uciHandler.loop();
  string result = os.str();
  LOG__DEBUG(Logger::get().TEST_LOG, "RESPONSE: \n{}", result);

  // Verify the output contains option information with current values
  EXPECT_TRUE(result.find("option name") != string::npos);
  EXPECT_TRUE(result.find("type") != string::npos);
  EXPECT_TRUE(result.find("current") != string::npos);

  // Verify termination signal
  EXPECT_TRUE(result.find("optionsok") != string::npos);

  // Verify specific values we set
  EXPECT_TRUE(result.find("Hash") != string::npos);
  EXPECT_TRUE(result.find("spin current 512") != string::npos);
#ifndef FRANKYCPP_PRODUCTION
  // Use Quiescence is CONFIG_CONST — not exposed as UCI option in production
  EXPECT_TRUE(result.find("Use Quiescence") != string::npos);
  EXPECT_TRUE(result.find("check current false") != string::npos);
#endif
}

TEST_F(UCITest, clearHashTest) {
  // check that hash is enabled before testing clear hash command
  SearchConfigData searchConfig = ConfigManager::instance().search();
  EXPECT_TRUE(searchConfig.USE_TT);
  ostringstream os;
  string command = "isready\nsetoption name Clear Hash";
  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  UciHandler uciHandler(&is, &os);
  uciHandler.loop();
  string result = os.str();
  LOG__DEBUG(Logger::get().TEST_LOG, "RESPONSE: \n{}", result);
  EXPECT_TRUE(result.find("Hash cleared") != string::npos);
}

TEST_F(UCITest, resizeHashTest) {
  // check that hash is enabled before testing clear hash command
  SearchConfigData searchConfig = ConfigManager::instance().search();
  EXPECT_TRUE(searchConfig.USE_TT);
  ostringstream os;
  string command = "isready\nsetoption name Hash value 512";
  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  UciHandler uciHandler(&is, &os);
  uciHandler.loop();
  string result = os.str();
  LOG__DEBUG(Logger::get().TEST_LOG, "RESPONSE: \n{}", result);
  EXPECT_TRUE(result.find("Resized hash") != string::npos);
}

TEST_F(UCITest, positionTest) {
  ostringstream os;
  // normal
  {
    string command = "position startpos moves e2e4 e7e5";
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
    EXPECT_EQ("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2", uciHandler.pPosition->strFen());
  }

  // castling
  {
    string command = "position fen r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 0 moves e1g1";
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
    EXPECT_EQ("r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQ1RK1 b kq - 1 1", uciHandler.pPosition->strFen());
  }

  // promotion
  {
    string command = "position fen 8/3P4/6K1/8/8/1k6/8/8 w - - 0 0 moves d7d8Q";
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
    EXPECT_EQ("3Q4/8/6K1/8/8/1k6/8/8 b - - 0 1", uciHandler.pPosition->strFen());
  }

  // normal
  {
    string command = "position moves e2e4 e7e5";
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
    EXPECT_EQ("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2", uciHandler.pPosition->strFen());
  }

  // normal
  {
    string command = "position fen rnbqkbnr/8/8/8/8/8/8/RNBQKBNR w KQkq - 0 1 moves e1e2 e8e7";
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
    EXPECT_EQ("rnbq1bnr/4k3/8/8/8/8/4K3/RNBQ1BNR w - - 2 2", uciHandler.pPosition->strFen());
  }

  // normal
  {
    string command = "position fen 7K/8/5pPk/6pP/1p1p2P1/1p1p4/1P1P4/8 w - - 0 12 moves g6g7";
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
    EXPECT_EQ("7K/6P1/5p1k/6pP/1p1p2P1/1p1p4/1P1P4/8 b - - 0 12", uciHandler.pPosition->strFen());
  }

  // normal
  {
    string command = "position fen 7K/6P1/5p1k/6pP/1p1p2P1/1p1p4/1P1P4/8 b - - 0 12 moves f6f5";
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
    EXPECT_EQ("7K/6P1/7k/5ppP/1p1p2P1/1p1p4/1P1P4/8 w - - 0 13", uciHandler.pPosition->strFen());
  }
}

TEST_F(UCITest, positionFenErrorTest) {
  ostringstream os;

  // 1. Invalid FEN string — error reported, position unchanged (start pos)
  {
    const string command = "position fen INVALID";
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
    const string result = os.str();
    LOG__DEBUG(Logger::get().TEST_LOG, "RESPONSE: {}", result);
    EXPECT_TRUE(result.find("info string") != string::npos);
    EXPECT_TRUE(result.find("Invalid FEN") != string::npos);
    // Position should still be the default start position
    EXPECT_EQ(START_POSITION_FEN, uciHandler.pPosition->strFen());
  }

  os.str("");
  os.clear();

  // 2. Empty FEN — "position fen" with no FEN tokens
  {
    const string command = "position fen";
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
    const string result = os.str();
    LOG__DEBUG(Logger::get().TEST_LOG, "RESPONSE: {}", result);
    EXPECT_TRUE(result.find("info string") != string::npos);
    EXPECT_TRUE(result.find("Invalid FEN") != string::npos);
    EXPECT_EQ(START_POSITION_FEN, uciHandler.pPosition->strFen());
  }

  os.str("");
  os.clear();

  // 3. Valid FEN — no error, position set correctly
  {
    const string command = "position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
    const string result = os.str();
    LOG__DEBUG(Logger::get().TEST_LOG, "RESPONSE: {}", result);
    EXPECT_TRUE(result.find("Invalid FEN") == string::npos);
    EXPECT_EQ(START_POSITION_FEN, uciHandler.pPosition->strFen());
  }

  os.str("");
  os.clear();

  // 4. Invalid FEN followed by moves — error on FEN, moves not applied, position unchanged
  {
    const string command = "position fen INVALID moves e2e4";
    LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
    istringstream is(command);
    UciHandler uciHandler(&is, &os);
    uciHandler.loop();
    const string result = os.str();
    LOG__DEBUG(Logger::get().TEST_LOG, "RESPONSE: {}", result);
    EXPECT_TRUE(result.find("info string") != string::npos);
    EXPECT_TRUE(result.find("Invalid FEN") != string::npos);
    EXPECT_EQ(START_POSITION_FEN, uciHandler.pPosition->strFen());
  }
}

TEST_F(UCITest, goPerft) {
  ostringstream os;
  int endDepth = 4;
#ifndef NDEBUG
  endDepth = 2;
#endif

  const string command = "perft 1 " + to_string(endDepth);
  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  UciHandler uciHandler(&is, &os);
  uciHandler.loop();
  while (os.str().find("Perft finished") == std::string::npos) {
    std::this_thread::sleep_for(seconds(1));
  }
}

TEST_F(UCITest, goError) {
  ostringstream os;
  string command = "go nothing";
  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  UciHandler uciHandler(&is, &os);
  std::string token;
  is >> skipws >> token;
  SearchLimits sl;
  EXPECT_FALSE(uciHandler.readSearchLimits(is, sl));
}

TEST_F(UCITest, goInfinite) {
  ostringstream os;
  string command = "go infinite";
  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  UciHandler uciHandler(&is, &os);
  std::string token;
  is >> skipws >> token;
  SearchLimits sl{};
  EXPECT_TRUE(uciHandler.readSearchLimits(is, sl));
  EXPECT_TRUE(sl.infinite);
  EXPECT_FALSE(sl.ponder);
  EXPECT_FALSE(sl.timeControl);
  EXPECT_EQ(0, sl.depth);
}

TEST_F(UCITest, goPonder) {
  ostringstream os;
  string command = "go ponder";
  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  UciHandler uciHandler(&is, &os);
  std::string token;
  is >> skipws >> token;
  SearchLimits sl{};
  EXPECT_TRUE(uciHandler.readSearchLimits(is, sl));
  EXPECT_FALSE(sl.infinite);
  EXPECT_TRUE(sl.ponder);
  EXPECT_FALSE(sl.timeControl);
  EXPECT_EQ(0, sl.depth);
}

TEST_F(UCITest, goMate) {
  ostringstream os;
  string command = "go mate 4";
  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  UciHandler uciHandler(&is, &os);
  std::string token;
  is >> skipws >> token;
  SearchLimits sl{};
  EXPECT_TRUE(uciHandler.readSearchLimits(is, sl));
  EXPECT_FALSE(sl.infinite);
  EXPECT_FALSE(sl.ponder);
  EXPECT_FALSE(sl.timeControl);
  EXPECT_EQ(4, sl.mate);
  EXPECT_EQ(0, sl.depth);
}

TEST_F(UCITest, testingBugs) {
  // 8/7R/3K4/8/8/P3k3/7p/4r3 b - - 5 75
  const string command = "position fen 8/7R/3K4/8/8/P3k3/7p/4r3 b - - 5 75 moves h2h1q h7h3";
  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
  istringstream is(command);
  ostringstream os;
  UciHandler uciHandler(&is, &os);
  uciHandler.loop();
  LOG__DEBUG(Logger::get().TEST_LOG, "RESPONSE: {}", os.str());
  EXPECT_TRUE(os.str().find("Invalid move") == string::npos);
}

//
// TEST_F(UCITest, goMateDepth) {
//  ostringstream os;
//  Engine engine;
//
//  string command = "go mate 4 depth 4";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  engine.stopSearch();
//  engine.waitWhileSearching();
//
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPerft());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isInfinite());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isTimeControl());
//  EXPECT_EQ(4, engine.getSearchLimitsPtr()->getMate());
//  EXPECT_EQ(4, engine.getSearchLimitsPtr()->getMaxDepth());
//}
//
// TEST_F(UCITest, goMateTime) {
//  ostringstream os;
//  Engine engine;
//
//  string command = "go mate 4 movetime 15";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  engine.stopSearch();
//  engine.waitWhileSearching();
//
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPerft());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isInfinite());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isTimeControl());
//  EXPECT_EQ(4, engine.getSearchLimitsPtr()->getMate());
//  EXPECT_EQ(PLY_MAX, engine.getSearchLimitsPtr()->getMaxDepth());
//  EXPECT_EQ(15, engine.getSearchLimitsPtr()->getMoveTime());
//}
//
// TEST_F(UCITest, goMateDepthTime) {
//  ostringstream os;
//  Engine engine;
//
//  string command = "go mate 4 depth 4 movetime 15";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  engine.stopSearch();
//  engine.waitWhileSearching();
//
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isInfinite());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isTimeControl());
//  EXPECT_EQ(4, engine.getSearchLimitsPtr()->getMate());
//  EXPECT_EQ(4, engine.getSearchLimitsPtr()->getMaxDepth());
//  EXPECT_EQ(15, engine.getSearchLimitsPtr()->getMoveTime());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPerft());
//
//}
//
// TEST_F(UCITest, goTimed) {
//  ostringstream os;
//  Engine engine;
//
//  string command = "go wtime 500001 btime 500002";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  engine.stopSearch();
//  engine.waitWhileSearching();
//
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPerft());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isInfinite());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isTimeControl());
//  EXPECT_EQ(PLY_MAX, engine.getSearchLimitsPtr()->getMaxDepth());
//  EXPECT_EQ(500'001, engine.getSearchLimitsPtr()->getWhiteTime());
//  EXPECT_EQ(500'002, engine.getSearchLimitsPtr()->getBlackTime());
//}
//
// TEST_F(UCITest, goMovestogo) {
//  ostringstream os;
//  Engine engine;
//
//  // normal game with time for each player and remaining moves until time control
//  string command = "go wtime 300001 btime 300002 movestogo 20";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  engine.stopSearch();
//  engine.waitWhileSearching();
//
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPerft());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isInfinite());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isTimeControl());
//  EXPECT_EQ(PLY_MAX, engine.getSearchLimitsPtr()->getMaxDepth());
//  EXPECT_EQ(300'001, engine.getSearchLimitsPtr()->getWhiteTime());
//  EXPECT_EQ(300'002, engine.getSearchLimitsPtr()->getBlackTime());
//  EXPECT_EQ(20, engine.getSearchLimitsPtr()->getMovesToGo());
//}
//
// TEST_F(UCITest, goInc) {
//  ostringstream os;
//  Engine engine;
//
//  // normal game with time for each player and increases per move
//  string command = "go wtime 300001 btime 300002 winc 2001 binc 2002";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  engine.stopSearch();
//  engine.waitWhileSearching();
//
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPerft());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isInfinite());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isTimeControl());
//  EXPECT_EQ(PLY_MAX, engine.getSearchLimitsPtr()->getMaxDepth());
//  EXPECT_EQ(300'001, engine.getSearchLimitsPtr()->getWhiteTime());
//  EXPECT_EQ(300'002, engine.getSearchLimitsPtr()->getBlackTime());
//  EXPECT_EQ(2001, engine.getSearchLimitsPtr()->getWhiteInc());
//  EXPECT_EQ(2002, engine.getSearchLimitsPtr()->getBlackInc());
//}
//
// TEST_F(UCITest, goMovetime) {
//  ostringstream os;
//  Engine engine;
//
//  // move time limited
//  string command = "go movetime 5000";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  engine.stopSearch();
//  engine.waitWhileSearching();
//
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPerft());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isInfinite());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isTimeControl());
//  EXPECT_EQ(0, engine.getSearchLimitsPtr()->getMate());
//  EXPECT_EQ(PLY_MAX, engine.getSearchLimitsPtr()->getMaxDepth());
//  EXPECT_EQ(5000, engine.getSearchLimitsPtr()->getMoveTime());
//}
//
// TEST_F(UCITest, goDepth) {
//  ostringstream os;
//  Engine engine;
//  // depth only limited
//  string command = "go depth 5";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  engine.stopSearch();
//  engine.waitWhileSearching();
//
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPerft());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isInfinite());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isTimeControl());
//  EXPECT_EQ(1, engine.getSearchLimitsPtr()->getStartDepth());
//  EXPECT_EQ(5, engine.getSearchLimitsPtr()->getMaxDepth());
//  EXPECT_EQ(0, engine.getSearchLimitsPtr()->getNodes());
//  engine.stopSearch();
//  engine.waitWhileSearching();
//}
//
// TEST_F(UCITest, goNodes) {
//  ostringstream os;
//  Engine engine;
//  // nodes only limited
//  string command = "go nodes 1000000";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  engine.stopSearch();
//  engine.waitWhileSearching();
//
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPerft());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isInfinite());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isTimeControl());
//  EXPECT_EQ(1, engine.getSearchLimitsPtr()->getStartDepth());
//  EXPECT_EQ(PLY_MAX, engine.getSearchLimitsPtr()->getMaxDepth());
//  EXPECT_EQ(1'000'000, engine.getSearchLimitsPtr()->getNodes());
//}
//
// TEST_F(UCITest, goNodesDepth) {
//  ostringstream os;
//  Engine engine;
//  // nodes and depth limited
//  string command = "go nodes 1000000 depth 5";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  engine.stopSearch();
//  engine.waitWhileSearching();
//
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPerft());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isInfinite());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isTimeControl());
//  EXPECT_EQ(1, engine.getSearchLimitsPtr()->getStartDepth());
//  EXPECT_EQ(5, engine.getSearchLimitsPtr()->getMaxDepth());
//  EXPECT_EQ(1'000'000, engine.getSearchLimitsPtr()->getNodes());
//}
//
// TEST_F(UCITest, goMoves) {
//  ostringstream os;
//  Engine engine;
//  // move time limited with a list of moves to search
//  string command = "go movetime 15 searchmoves d2d4 e2e4";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  engine.stopSearch();
//  engine.waitWhileSearching();
//
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPerft());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isInfinite());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isTimeControl());
//  EXPECT_EQ(0, engine.getSearchLimitsPtr()->getMate());
//  EXPECT_EQ(PLY_MAX, engine.getSearchLimitsPtr()->getMaxDepth());
//  EXPECT_EQ(15, engine.getSearchLimitsPtr()->getMoveTime());
//  EXPECT_EQ(createMove("d2d4"), engine.getSearchLimitsPtr()->getMoves().front());
//  EXPECT_EQ(createMove("e2e4"), engine.getSearchLimitsPtr()->getMoves().back());
//}
//
// TEST_F(UCITest, moveTest) {
//  ostringstream os;
//  Engine engine;
//
//  string command = "position startpos moves e2e4";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  command = "go wtime 60000 btime 60000 winc 2000 binc 2000 movestogo 40";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(5);
//  engine.stopSearch();
//  LOG__DEBUG(Logger::get().TEST_LOG, "Waiting until search ends...");
//  engine.waitWhileSearching();
//  LOG__DEBUG(Logger::get().TEST_LOG, "SEARCH ENDED");
//
//}
//
// TEST_F(UCITest, moveTestDepth) {
//  ostringstream os;
//  Engine engine;
//
//  string command = "position startpos moves e2e4";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  command = "go depth 5";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  LOG__DEBUG(Logger::get().TEST_LOG, "Waiting until search ends...");
//  engine.waitWhileSearching();
//  LOG__DEBUG(Logger::get().TEST_LOG, "SEARCH ENDED");
//
//}
//
// TEST_F(UCITest, ponderRunningStop) {
//  ostringstream os;
//  Engine engine;
//
//  // stop while pondering
//  string command = "position startpos moves e2e4 e7e5";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  command = "go ponder wtime 600000 btime 600000";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_TRUE(engine.isSearching());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isPonder());
//
//  command = "stop";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_FALSE(engine.isSearching());
//  EXPECT_TRUE(engine.getLastResult().valid);
//
//  // mate position should finish search quickly
//  // position startpos moves d2d4 d7d5 e2e4 d5e4 d4d5 g8f6 b1c3 c7c6 g1f3 e4f3 g2f3 c6d5 f1b5 c8d7 b5d7 d8d7 e1e2 d5d4 d1d4 d7d4 c3d5 d4c4 e2d1 c4d5 d1e1 d5f3 e1d2 b8c6 h1d1 e8c8
//  // go wtime 37776 btime 45570 movestogo 25 ponder
//}
//
// TEST_F(UCITest, ponderFinishedStop) {
//  ostringstream os;
//  Engine engine;
//
//  // stop when pondering has already finished
//  string command = "position fen 8/8/8/8/8/6K1/R7/6k1 w - - 0 8";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  command = "go ponder wtime 600000 btime 600000";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//
//  EXPECT_TRUE(engine.isSearching());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isPonder());
//
//  command = "stop";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_FALSE(engine.isSearching());
//  EXPECT_TRUE(engine.getLastResult().valid);
//
//}
//
// TEST_F(UCITest, ponderMiss) {
//  ostringstream os;
//  Engine engine;
//
//  EngineConfig::ponder = true;
//
//  // black just played e7e6 and sent ponder on d2d4
//  string command = "position startpos moves e2e4 e7e6 d2d4";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  // black to ponder on d2d4
//  command = "go wtime 600000 btime 600000 ponder";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_TRUE(engine.isSearching());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isPonder());
//  sleepForSec(1);
//
//  // user played different move (g1h3) - ponder miss
//  command = "stop";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_FALSE(engine.isSearching());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  sleepForSec(1);
//
//  // black getting new position after ponder miss
//  command = "position startpos moves e2e4 e7e6 g1h3";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  // black to search on new position
//  command = "go wtime 600000 btime 600000";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_TRUE(engine.isSearching());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//
//  // stop search
//  command = "stop";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_FALSE(engine.isSearching());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//
//  LOG__DEBUG(Logger::get().TEST_LOG, "Waiting until search ends...");
//  engine.waitWhileSearching();
//  LOG__DEBUG(Logger::get().TEST_LOG, "SEARCH ENDED");
//}
//
//
// TEST_F(UCITest, ponderFinishedMiss) {
//  ostringstream os;
//  Engine engine;
//
//  EngineConfig::ponder = true;
//
//  // black just played e7e6 and sent ponder on d2d4
//  string command = "position fen 8/8/8/8/8/6K1/R7/6k1 w - - 0 8";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  // black to ponder on d2d4
//  command = "go wtime 600000 btime 600000 ponder";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_TRUE(engine.isSearching());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isPonder());
//  sleepForSec(1);
//
//  // user played different move (g1h3) - ponder miss
//  command = "stop";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_FALSE(engine.isSearching());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//  sleepForSec(1);
//
//  // black getting new position after ponder miss
//  command = "position startpos moves e2e4 e7e6 g1h3";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  // black to search on new position
//  command = "go wtime 600000 btime 600000";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_TRUE(engine.isSearching());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//
//  // stop search
//  command = "stop";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_FALSE(engine.isSearching());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//
//  LOG__DEBUG(Logger::get().TEST_LOG, "Waiting until search ends...");
//  engine.waitWhileSearching();
//  LOG__DEBUG(Logger::get().TEST_LOG, "SEARCH ENDED");
//}
//
// TEST_F(UCITest, ponderFinishedHit) {
//  ostringstream os;
//  Engine engine;
//
//  EngineConfig::ponder = true;
//  SearchConfig::USE_BOOK = false;
//
//  string command = "setoption name Ponder value true";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is = istringstream(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  command = "position startpos";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop();
//
//  command = "go wtime 300000 btime 300000 ponder";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_TRUE(engine.isSearching());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isPonder());
//  sleepForSec(1);
//
//  command = "ponderhit";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_TRUE(engine.isSearching());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//
//  // stop search
//  command = "stop";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_FALSE(engine.isSearching());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//
//  LOG__DEBUG(Logger::get().TEST_LOG, "Waiting until search ends...");
//  engine.waitWhileSearching();
//  LOG__DEBUG(Logger::get().TEST_LOG, "SEARCH ENDED");
//}
//
//
// TEST_F(UCITest, ponderHit) {
//  ostringstream os;
//  Engine engine;
//
//  EngineConfig::ponder = true;
//  SearchConfig::USE_BOOK = false;
//
//  string command = "setoption name Ponder value true";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  istringstream is = istringstream(command);
//  UCIHandler uciHandler( &is, &os);
//  uciHandler.loop();
//
//  command = "position startpos moves e2e4 e7e6 d2d4";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop();
//
//  command = "go wtime 300000 btime 300000 ponder";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_TRUE(engine.isSearching());
//  EXPECT_TRUE(engine.getSearchLimitsPtr()->isPonder());
//  sleepForSec(1);
//
//  command = "ponderhit";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(2);
//  EXPECT_TRUE(engine.isSearching());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//
//  // stop search
//  command = "stop";
//  LOG__INFO(Logger::get().TEST_LOG, "COMMAND: {}", command);
//  is = istringstream(command);
//  uciHandler.loop(&is);
//
//  sleepForSec(1);
//  EXPECT_FALSE(engine.isSearching());
//  EXPECT_FALSE(engine.getSearchLimitsPtr()->isPonder());
//
//  LOG__DEBUG(Logger::get().TEST_LOG, "Waiting until search ends...");
//  engine.waitWhileSearching();
//  LOG__DEBUG(Logger::get().TEST_LOG, "SEARCH ENDED");
//
//}
//

//=============================================================================
// Test: Set All UCI Options to Defaults
// Purpose: Simulate what Arena does when saving engine configuration.
//          This helps debug freezing issues by showing exactly what happens
//          when each option is set.
//=============================================================================
TEST_F(UCITest, setAllOptionsToDefaults) {
  // Build a single command string with all setoption commands
  // This simulates what Arena does: sends all options in one session
  const auto& registry     = ConfigRegistry::instance();
  const auto uciOptionDefs = registry.uciOptions();

  // Build command string: uci + isready + all setoptions
  string commands = "uci\nisready\n";

  int optionCount = 0;
  for (const auto* def : uciOptionDefs) {
    const string& optionName   = def->uciName;
    const string& defaultValue = def->defaultValue;

    commands += "setoption name " + optionName + " value " + defaultValue + "\n";
    optionCount++;

    LOG__INFO(Logger::get().TEST_LOG, "  [{}] Will set: {} = {} (type={})",
              optionCount, optionName, defaultValue, valueTypeToString(def->valueType));
  }

  LOG__INFO(Logger::get().TEST_LOG, "=== Sending {} setoption commands in single session ===", optionCount);
  LOG__DEBUG(Logger::get().TEST_LOG, "Commands:\n{}", commands);

  // Execute all commands in a single UciHandler session
  ostringstream os;
  istringstream is(commands);
  UciHandler uciHandler(&is, &os);
  uciHandler.loop();

  const string response = os.str();
  LOG__DEBUG(Logger::get().TEST_LOG, "Response:\n{}", response);

  // Verify all options were set correctly
  LOG__INFO(Logger::get().TEST_LOG, "=== Verifying options ===");
  int successCount = 0;
  for (const auto* def : uciOptionDefs) {
    const string& optionName   = def->uciName;
    const string& defaultValue = def->defaultValue;

    const UciOption* opt = UciOptions::getInstance()->getOption(optionName);
    if (opt) {
      if (opt->currentValue == defaultValue) {
        successCount++;
        LOG__DEBUG(Logger::get().TEST_LOG, "  OK: {} = {}", optionName, opt->currentValue);
      }
      else {
        LOG__WARN(Logger::get().TEST_LOG, "  MISMATCH: {} expected={}, actual={}",
                  optionName, defaultValue, opt->currentValue);
      }
    }
    else {
      LOG__ERROR(Logger::get().TEST_LOG, "  ERROR: Option '{}' not found!", optionName);
    }
  }

  LOG__INFO(Logger::get().TEST_LOG, "=== Summary ===");
  LOG__INFO(Logger::get().TEST_LOG, "Total options: {}", optionCount);
  LOG__INFO(Logger::get().TEST_LOG, "Successfully set: {}", successCount);

  EXPECT_GT(optionCount, 0) << "Should have processed at least some options";
  EXPECT_EQ(successCount, optionCount) << "All options should be set successfully";
}

//=============================================================================
// Test: Set All UCI Options Using resetToDefaults Method
// Purpose: Test the built-in resetToDefaults() method that does the same thing
//=============================================================================
TEST_F(UCITest, resetAllOptionsToDefaults) {
  ostringstream os;
  istringstream is("isready");

  LOG__INFO(Logger::get().TEST_LOG, "=== Creating UciHandler and resetting all options ===");
  UciHandler uciHandler(&is, &os);
  uciHandler.loop();

  // Use the built-in reset function
  UciOptions::getInstance()->resetToDefaults(&uciHandler);

  LOG__INFO(Logger::get().TEST_LOG, "=== All options reset to defaults ===");

  // Verify all options are at their default values
  const string optionsStr = UciOptions::getInstance()->str();
  LOG__DEBUG(Logger::get().TEST_LOG, "Current options:\n{}", optionsStr);

  SUCCEED();
}
