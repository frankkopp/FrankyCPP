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

#include "Test_Fens.h"

#include "chesscore/Position.h"
#include "common/Logging.h"
#include "init.h"
#include "types/types.h"

#include <engine/EvalConfig.h>
#include <engine/Evaluator.h>
#include <gtest/gtest.h>
using testing::Eq;

class EvaluatorTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
    Logger::get().TEST_LOG->set_level(spdlog::level::debug);
    Logger::get().EVAL_LOG->set_level(spdlog::level::debug);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(EvaluatorTest, testFens) {
  EvalConfig::USE_PAWN_EVAL        = true;
  EvalConfig::USE_PAWN_TT          = false;
  std::vector<std::string> allFens = Test_Fens::getFENs();
  Evaluator e{};
  for (auto f : allFens) {
    Position p{f};
    Value v{e.evaluate(p)};
    fprintln("Value: {:<6} GPF: {:<20}  Fen: {}", std::to_string(v), p.getGamePhaseFactor(), f);
  }
}
