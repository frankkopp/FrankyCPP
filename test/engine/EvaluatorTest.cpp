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

// add headers for timing output similar to SpeedTests
#include <chrono>
#include <sstream>

using testing::Eq;

// centralize test eval config: set all USE_* flags
// to the given onoff value
void set_eval_config(const bool onoff) {
  // Values taken from src/engine/EvalConfig.h
  EvalConfig::USE_MATERIAL        = onoff;
  EvalConfig::USE_POSITIONAL      = onoff;
  EvalConfig::USE_TEMPO           = onoff;
  EvalConfig::USE_LAZY_EVAL       = onoff;
  EvalConfig::USE_PAWN_EVAL       = onoff;
  EvalConfig::USE_PAWN_TT         = onoff;
  EvalConfig::USE_PIECE_EVAL      = onoff;
  EvalConfig::USE_KNIGHT_MOBILITY = onoff;
  EvalConfig::USE_KING_EVAL       = onoff;
  EvalConfig::USE_GAMEPHASE_VALUE = onoff;
}

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
  const std::vector<std::string> allFens = Test_Fens::getFENs();
  Evaluator e{};
  for (auto f : allFens) {
    Position p{f};
    const Value v{e.evaluate(p)};
    fprintln("Value: {:<6} GPF: {:<20}  Fen: {}", std::to_string(v), p.getGamePhaseFactor(), f);
  }
}

// New test: ensure evaluation with only MATERIAL equals material difference from side to move
TEST_F(EvaluatorTest, EvaluateMaterialOnly) {
  set_eval_config(false);
  EvalConfig::USE_MATERIAL = true;

  const auto fens = Test_Fens::getFENs();
  Evaluator e{};

  for (const auto& fen : fens) {
    Position p{fen};

    // expected from white's view: pure material difference
    Value expectedWhiteView;
    if (p.checkInsufficientMaterial()) {
      expectedWhiteView = VALUE_DRAW;
    }
    else {
      expectedWhiteView = static_cast<Value>(p.getMaterial(WHITE) - p.getMaterial(BLACK));
    }

    // convert to the next player's perspective (finalEval logic)
    const Value expectedFinal = p.getNextPlayer() == WHITE ? expectedWhiteView : -expectedWhiteView;

    const Value v = e.evaluate(p);
    ASSERT_EQ(v, expectedFinal) << "Material-only eval mismatch for FEN: " << fen;
  }
}

TEST_F(EvaluatorTest, KnightMobility_CentralBeatsCorner) {
  // Enable only piece eval (knight mobility) to isolate the effect
  set_eval_config(false);
  EvalConfig::USE_PIECE_EVAL      = true;
  EvalConfig::USE_KNIGHT_MOBILITY = true;
  EvalConfig::USE_GAMEPHASE_VALUE = true;

  Evaluator e{};

  // Add one pawn per side to avoid early draw by insufficient material
  // Central knight on d4, kings placed safely
  Position central{"8/p7/8/3k4/3N4/8/P7/4K3 w - - 0 1"};
  // Corner knight on a1, kings placed safely
  Position corner{"8/p7/8/3k4/8/8/P7/N3K3 w - - 0 1"};

  const Value vCentral = e.evaluate(central);
  const Value vCorner  = e.evaluate(corner);

  ASSERT_GT(vCentral, vCorner) << "Knight mobility should favor central knight over corner knight";
}

// New timing test modeled after SpeedTests::TimingExtendedDoMoveUndoMove
TEST_F(EvaluatorTest, TimingEvaluateFens) {
  using namespace std::chrono;

  // Configuration similar to other timing tests
  constexpr int rounds = 5;
  // Keep iterations moderate as we evaluate a whole set of FENs each iteration
  // ReSharper disable once CppTooWideScope
  constexpr int iterations = 100'000;

  // Ensure evaluator configuration uses defaults from EvalConfig
  set_eval_config(true);
  EvalConfig::USE_LAZY_EVAL = false;// disable lazy eval for timing test to have all positions fully evaluated
  EvalConfig::USE_KNIGHT_MOBILITY = false;

  // Gather and prepare positions once to avoid measuring FEN parsing
  const std::vector<std::string> allFens = Test_Fens::getFENs();
  std::vector<Position> positions;
  positions.reserve(allFens.size());
  for (const auto& f : allFens) positions.emplace_back(f.c_str());

  Evaluator e{};

  for (int r = 1; r <= rounds; ++r) {
    fprintln("Round {}", r);

    // Accumulator prevents the optimizer from removing evaluation work
    volatile int64_t acc = 0;

    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
      for (auto& p : positions) {
        const Value v{e.evaluate(p)};
        acc += static_cast<int64_t>(v);
      }
    }
    auto elapsed = duration_cast<nanoseconds>(high_resolution_clock::now() - start);

    std::ostringstream os;
    os.flags(std::cout.flags());
    os.imbue(deLocale);
    os.precision(os.precision());

    const uint64_t totalEvals = static_cast<uint64_t>(iterations) * positions.size();

    os << "Evaluate took " << elapsed.count() << " ns for " << iterations << " iterations with " << positions.size() << " positions" << std::endl;
    os << "Evaluate took " << (elapsed.count() / (totalEvals ? totalEvals : 1)) << " ns per evaluation" << std::endl;
    os << "Evaluations per sec " << ((totalEvals * nanoPerSec) / (elapsed.count() ? elapsed.count() : 1)) << " eps" << std::endl;
    os << "Accumulated value " << acc << std::endl;
    std::cout << os.str() << std::endl;
  }
}
