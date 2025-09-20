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

inline bool isBulkRun() {
  const auto* ut = testing::UnitTest::GetInstance();
  const bool cond = ut && ut->test_to_run_count() > 1;
  if (cond) {
    std::cout << "Bulk run detected - limiting depth to shorten test time" << std::endl;
  }
  return cond;
}

// centralize test eval config: set all USE_* flags
// to the given onoff value
void set_eval_config(const bool onoff) {
  // Values taken from src/engine/EvalConfig.h
  EvalConfig::USE_MATERIAL             = onoff;
  EvalConfig::USE_POSITIONAL           = onoff;
  EvalConfig::USE_TEMPO                = onoff;
  EvalConfig::USE_LAZY_EVAL            = onoff;
  EvalConfig::USE_PAWN_EVAL            = onoff;
  EvalConfig::USE_PAWN_TT              = onoff;
  EvalConfig::USE_PIECE_EVAL           = onoff;
  // piece-specific toggles
  EvalConfig::USE_KNIGHT_MOBILITY      = onoff;
  EvalConfig::USE_BISHOP_MOBILITY      = onoff;
  EvalConfig::USE_ROOK_MOBILITY        = onoff;
  EvalConfig::USE_ROOK_OPEN_FILE_BONUS = onoff;
  EvalConfig::USE_QUEEN_MOBILITY       = onoff;
  EvalConfig::USE_QUEEN_TROPISM        = onoff;
  EvalConfig::USE_KING_EVAL            = onoff;
  EvalConfig::USE_GAMEPHASE_VALUE      = onoff;
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

  int printed = 0;
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

    // Human visual check (print a few examples outside bulk runs)
    if (!isBulkRun() && printed < 3) {
      fprintln("MaterialOnly Fen: {}", fen);
      fprintln("Expected: {}  Got: {}", expectedFinal, v);
      fprintln(p.strBoard());
      ++printed;
    }

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
  const Position central{"8/p7/8/3k4/3N4/8/P7/4K3 w - - 0 1"};
  // Corner knight on a1, kings placed safely
  const Position corner{"8/p7/8/3k4/8/8/P7/N3K3 w - - 0 1"};

  const Value vCentral = e.evaluate(central);
  const Value vCorner  = e.evaluate(corner);

  ASSERT_GT(vCentral, vCorner) << "Knight mobility should favor central knight over corner knight";

  // Human visual check
  fprintln("Central Knight Eval: {}", vCentral);
  fprintln(central.strBoard());
  fprintln("Corner Knight Eval:  {}", vCorner);
  fprintln(corner.strBoard());
  fprintln("Knight mobility favors central knight over corner knight: {} > {}", vCentral, vCorner);
}

TEST_F(EvaluatorTest, Pawn_PassedBeatsBlocked) {
  set_eval_config(false);
  EvalConfig::USE_PAWN_EVAL       = true;
  EvalConfig::USE_GAMEPHASE_VALUE = true;

  Evaluator e{};

  // Passed pawn for White: single WP e5, no black pawns
  const Position passed{"6k1/8/8/4P3/8/8/8/6K1 w - - 0 1"};
  // Blocked pawn for White: WP e5 blocked by own WP e6 (only white pawns present)
  const Position blocked{"6k1/8/4P3/4P3/8/8/8/6K1 w - - 0 1"};

  const Value vPassed  = e.evaluate(passed);
  const Value vBlocked = e.evaluate(blocked);

  // Human visual check
  fprintln("Passed pawn eval: {}", vPassed);
  fprintln(passed.strBoard());
  fprintln("Blocked pawn eval: {}", vBlocked);
  fprintln(blocked.strBoard());
  fprintln("Passed > Blocked: {} > {}", vPassed, vBlocked);

  ASSERT_GT(vPassed, vBlocked) << "Passed pawn should evaluate higher than blocked pawn";
}

TEST_F(EvaluatorTest, BishopMobility_CentralBeatsCorner) {
  set_eval_config(false);
  EvalConfig::USE_PIECE_EVAL        = true;
  EvalConfig::USE_BISHOP_MOBILITY   = true;
  EvalConfig::USE_GAMEPHASE_VALUE   = true;

  Evaluator e{};

  // Central bishop on d4, kings g1/g8, extra pawns to avoid insufficiency
  const Position central{"6k1/p7/8/8/3B4/8/7P/6K1 w - - 0 1"};
  // Corner bishop on a1
  const Position corner{"6k1/p7/8/8/8/8/7P/B5K1 w - - 0 1"};

  const Value vCentral = e.evaluate(central);
  const Value vCorner  = e.evaluate(corner);

  // Human visual check
  fprintln("Central Bishop Eval: {}", vCentral);
  fprintln(central.strBoard());
  fprintln("Corner Bishop Eval:  {}", vCorner);
  fprintln(corner.strBoard());
  fprintln("Bishop mobility favors central over corner: {} > {}", vCentral, vCorner);

  ASSERT_GT(vCentral, vCorner) << "Bishop mobility should favor central bishop over corner bishop";
}

TEST_F(EvaluatorTest, Rook_FileBonus_Open_gt_SemiOpen_gt_Closed) {
  set_eval_config(false);
  EvalConfig::USE_PIECE_EVAL            = true;
  EvalConfig::USE_ROOK_OPEN_FILE_BONUS  = true;
  EvalConfig::USE_ROOK_MOBILITY         = false; // isolate file bonus
  EvalConfig::USE_GAMEPHASE_VALUE       = true;

  Evaluator e{};

  // Closed file (both pawns on d-file): a7, d7, a2, d2
  Position closed{"6k1/p2p4/8/8/3R4/8/P2P4/6K1 w - - 0 1"};
  // Semi-open (opponent pawn only on d7): a7, d7, a2
  Position semiOpen{"6k1/p2p4/8/8/3R4/8/P7/6K1 w - - 0 1"};
  // Open (no pawns on d-file): a7, a2
  Position open{"6k1/p7/8/8/3R4/8/P7/6K1 w - - 0 1"};

  const Value vClosed   = e.evaluate(closed);
  const Value vSemiOpen = e.evaluate(semiOpen);
  const Value vOpen     = e.evaluate(open);

  // Human visual check
  fprintln("Rook closed-file eval: {}", vClosed);
  fprintln(closed.strBoard());
  fprintln("Rook semi-open-file eval: {}", vSemiOpen);
  fprintln(semiOpen.strBoard());
  fprintln("Rook open-file eval: {}", vOpen);
  fprintln(open.strBoard());
  fprintln("Open > Semi-open > Closed: {} > {} > {}", vOpen, vSemiOpen, vClosed);

  ASSERT_GT(vSemiOpen, vClosed) << "Semi-open file should be better than closed file for rook";
  ASSERT_GT(vOpen, vSemiOpen)  << "Open file should be better than semi-open file for rook";
}

TEST_F(EvaluatorTest, Rook_PSQT_SeventhRank_BetterThan_BackRank) {
  set_eval_config(false);
  EvalConfig::USE_POSITIONAL      = true;  // use PSQT
  EvalConfig::USE_PIECE_EVAL      = false; // avoid mobility/file
  EvalConfig::USE_KING_EVAL       = false; // avoid shield
  EvalConfig::USE_PAWN_EVAL       = false;
  EvalConfig::USE_GAMEPHASE_VALUE = true;

  Evaluator e{};

  // Rook on 7th vs back rank, kings only
  const Position seventh{"6k1/3R4/8/8/8/8/8/6K1 w - - 0 1"};
  const Position backrank{"6k1/8/8/8/8/8/8/3R2K1 w - - 0 1"};

  const Value v7th  = e.evaluate(seventh);
  const Value vBack = e.evaluate(backrank);

  // Human visual check
  fprintln("Rook 7th-rank eval: {}", v7th);
  fprintln(seventh.strBoard());
  fprintln("Rook back-rank eval: {}", vBack);
  fprintln(backrank.strBoard());
  fprintln("7th rank > back rank: {} > {}", v7th, vBack);

  ASSERT_GT(v7th, vBack) << "PSQT should reward rook on the 7th rank more than back rank";
}

TEST_F(EvaluatorTest, QueenMobility_CentralBeatsCorner) {
  set_eval_config(false);
  EvalConfig::USE_PIECE_EVAL        = true;
  EvalConfig::USE_QUEEN_MOBILITY    = true;
  EvalConfig::USE_QUEEN_TROPISM     = false; // isolate mobility
  EvalConfig::USE_GAMEPHASE_VALUE   = true;

  Evaluator e{};

  const Position central{"6k1/p7/8/8/3Q4/8/7P/6K1 w - - 0 1"};
  const Position corner{"6k1/p7/8/8/8/8/7P/Q5K1 w - - 0 1"};

  const Value vCentral = e.evaluate(central);
  const Value vCorner  = e.evaluate(corner);

  // Human visual check
  fprintln("Central Queen Eval: {}", vCentral);
  fprintln(central.strBoard());
  fprintln("Corner Queen Eval:  {}", vCorner);
  fprintln(corner.strBoard());
  fprintln("Queen mobility favors central over corner: {} > {}", vCentral, vCorner);

  ASSERT_GT(vCentral, vCorner) << "Queen mobility should favor central queen over corner queen";
}

TEST_F(EvaluatorTest, King_PSQT_CenterBeatsCorner_Endgameish) {
  set_eval_config(false);
  EvalConfig::USE_POSITIONAL      = true;  // PSQT
  EvalConfig::USE_PIECE_EVAL      = false;
  EvalConfig::USE_PAWN_EVAL       = false;
  EvalConfig::USE_KING_EVAL       = false; // avoid dynamic shield
  EvalConfig::USE_GAMEPHASE_VALUE = true;

  Evaluator e{};

  // Central king e4 vs corner king a1; symmetric pawns to avoid insufficiency
  const Position central{"6k1/p7/8/8/4K3/8/P7/8 w - - 0 1"};
  const Position cornerK{"6k1/p7/8/8/8/8/P7/K7 w - - 0 1"};

  const Value vCentral = e.evaluate(central);
  const Value vCorner  = e.evaluate(cornerK);

  // Human visual check
  fprintln("Central King Eval: {}", vCentral);
  fprintln(central.strBoard());
  fprintln("Corner King Eval:  {}", vCorner);
  fprintln(cornerK.strBoard());
  fprintln("PSQT favors central king in endgame: {} > {}", vCentral, vCorner);

  ASSERT_GT(vCentral, vCorner) << "PSQT should favor central king in endgame-like positions";
}

TEST_F(EvaluatorTest, BishopPairBonus_TwoBishopsBeats_BishopKnight) {
  set_eval_config(false);
  EvalConfig::USE_PIECE_EVAL       = true;
  EvalConfig::USE_BISHOP_MOBILITY  = false; // isolate pair bonus
  EvalConfig::USE_MATERIAL         = false; // equalize material influence
  EvalConfig::USE_POSITIONAL       = false; // no PSQT influence
  EvalConfig::USE_PAWN_EVAL        = false;
  EvalConfig::USE_KING_EVAL        = false;
  EvalConfig::USE_GAMEPHASE_VALUE  = true;

  Evaluator e{};

  // White has bishop pair; Black has bishop+knight. Add a pawn each to avoid insufficiency.
  // Pair: white bishops c1,f1; black bishop c8, knight b8; kings h1/g8; pawns a2/a7.
  const Position pairPos{"1n1b2k1/p7/8/8/8/8/P7/2B2B1K w - - 0 1"};
  // No pair: replace white f1 bishop by knight.
  const Position noPairPos{"1n1b2k1/p7/8/8/8/8/P7/2B2N1K w - - 0 1"};

  const Value vPair   = e.evaluate(pairPos);
  const Value vNoPair = e.evaluate(noPairPos);

  // Human visual check
  fprintln("Bishop pair eval: {}", vPair);
  fprintln(pairPos.strBoard());
  fprintln("Bishop+Knight eval: {}", vNoPair);
  fprintln(noPairPos.strBoard());
  fprintln("Pair > NoPair: {} > {}", vPair, vNoPair);

  ASSERT_GT(vPair, vNoPair) << "Bishop pair bonus should favor two bishops over bishop+knight when material/PSQT are off";
}

TEST_F(EvaluatorTest, RookMobility_CentralBeatsEdge_FileBonusesOff) {
  set_eval_config(false);
  EvalConfig::USE_PIECE_EVAL            = true;
  EvalConfig::USE_ROOK_MOBILITY         = true;
  EvalConfig::USE_ROOK_OPEN_FILE_BONUS  = false; // isolate mobility
  EvalConfig::USE_POSITIONAL            = false; // no PSQT influence
  EvalConfig::USE_MATERIAL              = false;
  EvalConfig::USE_PAWN_EVAL             = false;
  EvalConfig::USE_KING_EVAL             = false;
  EvalConfig::USE_GAMEPHASE_VALUE       = true;

  Evaluator e{};

  // Central rook d4 with own pieces blocking 3 directions; still has east moves (e4..h4)
  const Position central{"6k1/8/8/3P4/2PR4/3P4/8/6K1 w - - 0 1"};
  // Edge rook a1 boxed in by own a2 and b1 -> 0 mobility
  const Position edge{"6k1/8/8/8/8/8/P7/RP4K1 w - - 0 1"};

  const Value vCentral = e.evaluate(central);
  const Value vEdge    = e.evaluate(edge);

  // Human visual check
  fprintln("Central rook eval: {}", vCentral);
  fprintln(central.strBoard());
  fprintln("Edge rook eval:    {}", vEdge);
  fprintln(edge.strBoard());
  fprintln("Central > Edge (mobility): {} > {}", vCentral, vEdge);

  ASSERT_GT(vCentral, vEdge) << "Rook mobility should favor central rook when file bonuses and PSQT are off";
}

TEST_F(EvaluatorTest, QueenTropism_CloserBeatsFarther_EndgameOnly) {
  set_eval_config(false);
  EvalConfig::USE_PIECE_EVAL        = true;
  EvalConfig::USE_QUEEN_MOBILITY    = false; // isolate tropism
  EvalConfig::USE_QUEEN_TROPISM     = true;  // endgame-only weight
  EvalConfig::USE_POSITIONAL        = false; // no PSQT influence
  EvalConfig::USE_MATERIAL          = false;
  EvalConfig::USE_PAWN_EVAL         = false;
  EvalConfig::USE_KING_EVAL         = false;
  EvalConfig::USE_GAMEPHASE_VALUE   = true;

  Evaluator e{};

  // Closer: Qg7 near black king h8
  const Position closer{"7k/6Q1/8/8/8/8/8/6K1 w - - 0 1"};
  // Farther: Qa1 far from black king h8
  const Position farther{"7k/8/8/8/8/8/8/Q5K1 w - - 0 1"};

  const Value vCloser  = e.evaluate(closer);
  const Value vFarther = e.evaluate(farther);

  // Human visual check
  fprintln("Queen closer eval:  {}", vCloser);
  fprintln(closer.strBoard());
  fprintln("Queen farther eval: {}", vFarther);
  fprintln(farther.strBoard());
  fprintln("Closer > Farther (tropism): {} > {}", vCloser, vFarther);

  ASSERT_GT(vCloser, vFarther) << "Queen tropism should reward being closer to enemy king (endgame-only), with mobility/PSQT/material off";
}

// New timing test modeled after SpeedTests::TimingExtendedDoMoveUndoMove
TEST_F(EvaluatorTest, TimingEvaluateFens) {
  if (isBulkRun()) {
    GTEST_SKIP() << "Skipping timing test in bulk run to save time";
  }
  using namespace std::chrono;

  // Configuration similar to other timing tests
  constexpr int rounds = 5;
  // Keep iterations moderate as we evaluate a whole set of FENs each iteration
  // ReSharper disable once CppTooWideScope
  constexpr int iterations = 100'000;

  // Ensure evaluator configuration uses defaults from EvalConfig
  set_eval_config(true);
  EvalConfig::USE_LAZY_EVAL = false; // disable lazy eval for timing test to have all positions fully evaluated
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
