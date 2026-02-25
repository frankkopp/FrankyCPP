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

#include <string>

#include "common/stringutil.h"
#include "config/ConfigManager.h"
#include "engine/UciOptions.h"
#include "init.h"

#include <engine/UciHandler.h>
#include <gtest/gtest.h>

using testing::Eq;

class UciOptionsTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
  }

  // reference to the Search Config Data
  const SearchConfigData& SearchConfig = ConfigManager::instance().search();
  // reference to the Search Config Data
  const EvalConfigData& EvalConfig = ConfigManager::instance().eval();

protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(UciOptionsTest, initAndStr) {
  UciOptions* pUciOptions = UciOptions::getInstance();
  UciHandler uciHandler{};

  auto o = pUciOptions->getOption("Clear Hash");
  fprintln("Option: {}", o->str());
  EXPECT_EQ("option name Clear Hash type button", o->str());

  o = pUciOptions->getOption("Hash");
  fprintln("Option: {}", o->str());
  EXPECT_TRUE(o->str().rfind("option name Hash type spin default 64 min 0 max 4096", 0) == 0);
  fprintln("Option current value: {}", o->currentValue);
  EXPECT_EQ("64", o->defaultValue);

  pUciOptions->setOption(&uciHandler, "Hash", "0");
  fprintln("Option: {}", o->currentValue);
  EXPECT_EQ("0", o->currentValue);
}

TEST_F(UciOptionsTest, getOption) {
  UciOptions* pUciOptions = UciOptions::getInstance();
  const auto* const o     = pUciOptions->getOption("Clear Hash");
  EXPECT_EQ("Clear Hash", o->nameID);
}

TEST_F(UciOptionsTest, setOption) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  UciOptions* pUciOptions = UciOptions::getInstance();
  UciHandler uciHandler{};

  const auto o = pUciOptions->getOption("Hash");
  EXPECT_EQ("Hash", o->nameID);
  EXPECT_EQ(std::to_string(SearchConfig.TT_SIZE_MB), o->currentValue);

  pUciOptions->setOption(&uciHandler, "Hash", "0");
  EXPECT_EQ("0", o->currentValue);
  EXPECT_EQ(SearchConfig.TT_SIZE_MB, 0);

  pUciOptions->setOption(&uciHandler, "Hash", "128");
  EXPECT_EQ("128", o->currentValue);
  EXPECT_EQ(SearchConfig.TT_SIZE_MB, 128);
}

TEST_F(UciOptionsTest, resetToDefaults_restores_defaults_and_applies_handlers) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  UciOptions* pUciOptions = UciOptions::getInstance();
  UciHandler uciHandler{};

  // Gather some options and their defaults
  const auto oHash   = pUciOptions->getOption("Hash");
  const auto oPonder = pUciOptions->getOption("Ponder");
  const auto oThreat = pUciOptions->getOption("Use Threat Extension");
  ASSERT_NE(oHash, nullptr);
  ASSERT_NE(oPonder, nullptr);
  ASSERT_NE(oThreat, nullptr);

  const int defaultHash    = parseIntOr(oHash->defaultValue);
  const bool defaultPonder = oPonder->defaultValue == std::string("true");
  const bool defaultThreat = oThreat->defaultValue == std::string("true");

  // Change values away from defaults
  const int altHash     = (defaultHash == 4096 ? defaultHash - 1 : defaultHash + 1);
  const char* altPonder = defaultPonder ? "false" : "true";
  const char* altThreat = defaultThreat ? "false" : "true";

  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Hash", std::to_string(altHash)));
  EXPECT_EQ(oHash->currentValue, std::to_string(altHash));
  EXPECT_EQ(SearchConfig.TT_SIZE_MB, altHash);

  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Ponder", altPonder));
  EXPECT_EQ(oPonder->currentValue, std::string(altPonder));
  EXPECT_EQ(SearchConfig.USE_PONDER, (std::string(altPonder) == "true"));

  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Use Threat Extension", altThreat));
  EXPECT_EQ(oThreat->currentValue, std::string(altThreat));
  EXPECT_EQ(SearchConfig.USE_THREAT_EXT, (std::string(altThreat) == "true"));

  // Now reset to defaults and verify both option current values and configs
  pUciOptions->resetToDefaults(&uciHandler);

  EXPECT_EQ(oHash->currentValue, oHash->defaultValue);
  EXPECT_EQ(SearchConfig.TT_SIZE_MB, defaultHash);

  EXPECT_EQ(oPonder->currentValue, oPonder->defaultValue);
  EXPECT_EQ(SearchConfig.USE_PONDER, defaultPonder);

  EXPECT_EQ(oThreat->currentValue, oThreat->defaultValue);
  EXPECT_EQ(SearchConfig.USE_THREAT_EXT, defaultThreat);
}

TEST_F(UciOptionsTest, resetButton_exists_and_resets) {
  UciOptions* pUciOptions = UciOptions::getInstance();
  UciHandler uciHandler{};

  const auto oReset = pUciOptions->getOption("Reset to Defaults");
  ASSERT_NE(oReset, nullptr);
  EXPECT_EQ("option name Reset to Defaults type button", oReset->str());

  // Change some options
  const auto oHash   = pUciOptions->getOption("Hash");
  const auto oPonder = pUciOptions->getOption("Ponder");
  ASSERT_NE(oHash, nullptr);
  ASSERT_NE(oPonder, nullptr);

  const int defaultHash    = parseIntOr(oHash->defaultValue);
  const bool defaultPonder = oPonder->defaultValue == std::string("true");

  const int altHash     = (defaultHash == 4096 ? defaultHash - 1 : defaultHash + 1);
  const char* altPonder = defaultPonder ? "false" : "true";

#ifndef FRANKYCPP_PRODUCTION
  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Hash", std::to_string(altHash)));
  EXPECT_EQ(SearchConfig.TT_SIZE_MB, altHash);

  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Ponder", altPonder));
  EXPECT_EQ(SearchConfig.USE_PONDER, (std::string(altPonder) == "true"));
#endif

  // Invoke the button (value is ignored by the handler)
  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Reset to Defaults", ""));

  // Verify reset happened
  EXPECT_EQ(oHash->currentValue, oHash->defaultValue);
  EXPECT_EQ(SearchConfig.TT_SIZE_MB, defaultHash);

  EXPECT_EQ(oPonder->currentValue, oPonder->defaultValue);
  EXPECT_EQ(SearchConfig.USE_PONDER, defaultPonder);
}

TEST_F(UciOptionsTest, searchConfig_nonArray_options_present_and_settable) {
  UciOptions* pUciOptions = UciOptions::getInstance();
  UciHandler uciHandler{};

  // Presence checks for newly added options
  EXPECT_NE(pUciOptions->getOption("Book Path"), nullptr);
  EXPECT_NE(pUciOptions->getOption("Book Format"), nullptr);
  EXPECT_NE(pUciOptions->getOption("Moves Left Opening"), nullptr);
  EXPECT_NE(pUciOptions->getOption("Moves Left Midgame"), nullptr);
  EXPECT_NE(pUciOptions->getOption("Moves Left Endgame"), nullptr);
  EXPECT_NE(pUciOptions->getOption("Moves Left Low Material"), nullptr);
  EXPECT_NE(pUciOptions->getOption("Moves Left Queenless"), nullptr);
  EXPECT_NE(pUciOptions->getOption("NPP Heavy Threshold"), nullptr);
  EXPECT_NE(pUciOptions->getOption("NPP Light Threshold"), nullptr);
  EXPECT_NE(pUciOptions->getOption("Repetition HMC High"), nullptr);
  EXPECT_NE(pUciOptions->getOption("Repetition Risk Penalty"), nullptr);
  EXPECT_NE(pUciOptions->getOption("Moves Left Min Clamp"), nullptr);
  EXPECT_NE(pUciOptions->getOption("Moves Left Max Clamp"), nullptr);
  EXPECT_NE(pUciOptions->getOption("LMR Min Depth"), nullptr);
  EXPECT_NE(pUciOptions->getOption("LMR Min Moves"), nullptr);

  // Behavior checks: set a few and verify SearchConfig updates
  // Book Format
  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Book Format", "SAN"));
  EXPECT_EQ(SearchConfig.BOOK_TYPE, "SAN");
  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Book Format", "SIMPLE"));
  EXPECT_EQ(SearchConfig.BOOK_TYPE, "SIMPLE");

#ifndef FRANKYCPP_PRODUCTION
  // Moves Left Opening
  const int oldMlo = SearchConfig.MOVES_LEFT_OPENING;
  const int newMlo = oldMlo == 36 ? 35 : 36;
  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Moves Left Opening", std::to_string(newMlo)));
  EXPECT_EQ(SearchConfig.MOVES_LEFT_OPENING, newMlo);

  // LMR Min Depth
  const int oldLmd = static_cast<int>(SearchConfig.LMR_MIN_DEPTH);
  const int newLmd = oldLmd == 3 ? 4 : 3;
  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "LMR Min Depth", std::to_string(newLmd)));
  EXPECT_EQ(static_cast<int>(SearchConfig.LMR_MIN_DEPTH), newLmd);
#endif
}
