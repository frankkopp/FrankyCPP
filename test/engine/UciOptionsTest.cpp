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

#include <string>

#include "engine/SearchConfig.h"
#include "engine/UciOptions.h"
#include "init.h"

#include <engine/UciHandler.h>
#include <gtest/gtest.h>
using testing::Eq;

class UciOptionsTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(UciOptionsTest, initAndStr) {
  const UciOptions* pUciOptions = UciOptions::getInstance();
  UciHandler uciHandler{};

  auto o = pUciOptions->getOption("Clear Hash");
  fprintln("Option: {}", o->str());
  EXPECT_EQ("option name Clear Hash type button", o->str());

  o = pUciOptions->getOption("Hash");
  fprintln("Option: {}", o->str());
  EXPECT_TRUE(o->str().rfind("option name Hash type spin default 64 min 0 max 4096 current ", 0) == 0);
  fprintln("Option current value: {}", o->currentValue);
  EXPECT_EQ("64", o->defaultValue);

  pUciOptions->setOption(&uciHandler, "Hash", "0");
  fprintln("Option: {}", o->currentValue);
  EXPECT_EQ("0", o->currentValue);
}

TEST_F(UciOptionsTest, getOption) {
  const UciOptions* pUciOptions = UciOptions::getInstance();
  const auto o = pUciOptions->getOption("Clear Hash");
  EXPECT_EQ("Clear Hash", o->nameID);
}

TEST_F(UciOptionsTest, setOption) {
  const UciOptions* pUciOptions = UciOptions::getInstance();
  UciHandler uciHandler{};

  const auto o = pUciOptions->getOption("Hash");
  EXPECT_EQ("Hash", o->nameID);
  EXPECT_EQ(std::to_string(SearchConfig::TT_SIZE_MB), o->currentValue);

  pUciOptions->setOption(&uciHandler, "Hash", "0");
  EXPECT_EQ("0", o->currentValue);
  EXPECT_EQ(SearchConfig::TT_SIZE_MB, 0);

  pUciOptions->setOption(&uciHandler, "Hash", "128");
  EXPECT_EQ("128", o->currentValue);
  EXPECT_EQ(SearchConfig::TT_SIZE_MB, 128);
}

TEST_F(UciOptionsTest, resetToDefaults_restores_defaults_and_applies_handlers) {
  const UciOptions* pUciOptions = UciOptions::getInstance();
  UciHandler uciHandler{};

  // Gather some options and their defaults
  const auto oHash    = pUciOptions->getOption("Hash");
  const auto oPonder  = pUciOptions->getOption("Ponder");
  const auto oThreat  = pUciOptions->getOption("Use Threat Extension");
  ASSERT_NE(oHash, nullptr);
  ASSERT_NE(oPonder, nullptr);
  ASSERT_NE(oThreat, nullptr);

  const int  defaultHash     = UciOptions::getInt(oHash->defaultValue);
  const bool defaultPonder   = (oPonder->defaultValue == std::string("true"));
  const bool defaultThreat   = (oThreat->defaultValue == std::string("true"));

  // Change values away from defaults
  const int  altHash = (defaultHash == 4096 ? defaultHash - 1 : defaultHash + 1);
  const char* altPonder = defaultPonder ? "false" : "true";
  const char* altThreat = defaultThreat ? "false" : "true";

  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Hash", std::to_string(altHash)));
  EXPECT_EQ(oHash->currentValue, std::to_string(altHash));
  EXPECT_EQ(SearchConfig::TT_SIZE_MB, altHash);

  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Ponder", altPonder));
  EXPECT_EQ(oPonder->currentValue, std::string(altPonder));
  EXPECT_EQ(SearchConfig::USE_PONDER, (std::string(altPonder) == "true"));

  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Use Threat Extension", altThreat));
  EXPECT_EQ(oThreat->currentValue, std::string(altThreat));
  EXPECT_EQ(SearchConfig::USE_THREAT_EXT, (std::string(altThreat) == "true"));

  // Now reset to defaults and verify both option current values and configs
  pUciOptions->resetToDefaults(&uciHandler);

  EXPECT_EQ(oHash->currentValue, oHash->defaultValue);
  EXPECT_EQ(SearchConfig::TT_SIZE_MB, defaultHash);

  EXPECT_EQ(oPonder->currentValue, oPonder->defaultValue);
  EXPECT_EQ(SearchConfig::USE_PONDER, defaultPonder);

  EXPECT_EQ(oThreat->currentValue, oThreat->defaultValue);
  EXPECT_EQ(SearchConfig::USE_THREAT_EXT, defaultThreat);
}

TEST_F(UciOptionsTest, resetButton_exists_and_resets) {
  const UciOptions* pUciOptions = UciOptions::getInstance();
  UciHandler uciHandler{};

  const auto oReset = pUciOptions->getOption("Reset to Defaults");
  ASSERT_NE(oReset, nullptr);
  EXPECT_EQ("option name Reset to Defaults type button", oReset->str());

  // Change some options
  const auto oHash   = pUciOptions->getOption("Hash");
  const auto oPonder = pUciOptions->getOption("Ponder");
  ASSERT_NE(oHash, nullptr);
  ASSERT_NE(oPonder, nullptr);

  const int  defaultHash   = UciOptions::getInt(oHash->defaultValue);
  const bool defaultPonder = (oPonder->defaultValue == std::string("true"));

  const int altHash = (defaultHash == 4096 ? defaultHash - 1 : defaultHash + 1);
  const char* altPonder = defaultPonder ? "false" : "true";

  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Hash", std::to_string(altHash)));
  EXPECT_EQ(SearchConfig::TT_SIZE_MB, altHash);

  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Ponder", altPonder));
  EXPECT_EQ(SearchConfig::USE_PONDER, (std::string(altPonder) == "true"));

  // Invoke the button (value is ignored by the handler)
  EXPECT_TRUE(pUciOptions->setOption(&uciHandler, "Reset to Defaults", ""));

  // Verify reset happened
  EXPECT_EQ(oHash->currentValue, oHash->defaultValue);
  EXPECT_EQ(SearchConfig::TT_SIZE_MB, defaultHash);

  EXPECT_EQ(oPonder->currentValue, oPonder->defaultValue);
  EXPECT_EQ(SearchConfig::USE_PONDER, defaultPonder);
}
