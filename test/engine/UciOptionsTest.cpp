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

#include "init.h"
#include "types/types.h"
#include "engine/SearchConfig.h"
#include "engine/UciOptions.h"

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
  UciOptions* pUciOptions = UciOptions::getInstance();
  UciHandler uciHandler{};

  auto o = pUciOptions->getOption("Clear Hash");
  fprintln("Option: {}", o->str());
  EXPECT_EQ("option name Clear Hash type button", o->str());

  o = pUciOptions->getOption("Hash");
  fprintln("Option: {}", o->str());
  EXPECT_EQ("option name Hash type spin default 64 min 0 max 4096", o->str());
  fprintln("Option current value: {}", o->currentValue);
  EXPECT_EQ("64", o->defaultValue);

  pUciOptions->setOption(&uciHandler, "Hash", "0");
  fprintln("Option: {}", o->currentValue);
  EXPECT_EQ("0", o->currentValue);
}

TEST_F(UciOptionsTest, getOption) {
  UciOptions* pUciOptions = UciOptions::getInstance();
  auto o = pUciOptions->getOption("Clear Hash");
  EXPECT_EQ("Clear Hash", o->nameID);
}

TEST_F(UciOptionsTest, setOption) {
  UciOptions* pUciOptions = UciOptions::getInstance();
  UciHandler uciHandler{};

  auto o = pUciOptions->getOption("Hash");
  EXPECT_EQ("Hash", o->nameID);
  EXPECT_EQ(std::to_string(SearchConfig::TT_SIZE_MB), o->currentValue);

  pUciOptions->setOption(&uciHandler, "Hash", "0");
  EXPECT_EQ("0", o->currentValue);
  EXPECT_EQ(SearchConfig::TT_SIZE_MB, 0);

  pUciOptions->setOption(&uciHandler, "Hash", "128");
  EXPECT_EQ("128", o->currentValue);
  EXPECT_EQ(SearchConfig::TT_SIZE_MB, 128);
}
