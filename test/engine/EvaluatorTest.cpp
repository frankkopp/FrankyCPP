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

#include "Test_Fens.h"
#include "Test_Utils.h"

#include "chesscore/Position.h"
#include "common/Logging.h"
#include "init.h"
#include "types/types.h"

#include "config/ConfigManager.h"
#include "config/ConfigMode.h"
#include <engine/Evaluator.h>
#include <engine/PawnTT.h>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>

using namespace engine;
using namespace chess;
using namespace config;
using namespace common;

using testing::Eq;

auto& cm = ConfigManager::instance(); // Bind cm to ConfigManager singleton by reference

// centralize test eval config: set all USE_* flags
// to the given onoff value
// In production, all eval USE_* flags are CONFIG_CONST (static constexpr) — cannot be set at runtime.
// The function and all tests that call it are excluded from production builds.
#ifndef FRANKYCPP_PRODUCTION
void set_eval_config(const bool onoff) {
  // Values taken from src/engine/EvalConfig.h
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_MATERIAL   = onoff;
    e.USE_POSITIONAL = onoff;
    e.USE_TEMPO      = onoff;
    e.USE_LAZY_EVAL  = onoff;
    // pawn-specific toggles
    e.USE_PAWN_EVAL = onoff;
    e.USE_PAWN_TT   = onoff;
    // passed pawn rank bonus
    e.USE_PASSED_PAWN_RANK_BONUS = onoff;
    // piece-specific toggles
    e.USE_PIECE_EVAL           = onoff;
    e.USE_KNIGHT_MOBILITY      = onoff;
    e.USE_BISHOP_MOBILITY      = onoff;
    e.USE_ROOK_MOBILITY        = onoff;
    e.USE_ROOK_OPEN_FILE_BONUS = onoff;
    e.USE_QUEEN_MOBILITY       = onoff;
    e.USE_QUEEN_TROPISM        = onoff;
    e.USE_KING_EVAL            = onoff;
    e.USE_KING_SAFETY_SHIELD   = onoff;
    e.USE_GAMEPHASE_VALUE      = onoff;
    e.USE_BISHOP_PAIR_BONUS    = onoff;
  });
}
#endif // FRANKYCPP_PRODUCTION

class EvaluatorTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    init::init();
    NEWLINE;
    Logger::setLoggerLevel(Logger::get().CONFIG_LOG, spdlog::level::info);
    Logger::setLoggerLevel(Logger::get().TEST_LOG, spdlog::level::info);
    Logger::setLoggerLevel(Logger::get().EVAL_LOG, spdlog::level::info);
    Logger::setLoggerLevel(Logger::get().SEARCH_LOG, spdlog::level::info);
  }

protected:
  void SetUp() override {
  }
  void TearDown() override {}
};

TEST_F(EvaluatorTest, testFens) {
  const std::vector<std::string> allFens = Test_Fens::getFENs();
  Evaluator e{};
  for (const auto& f : allFens) {
    Position p{f};
    const Value v{e.evaluate(p)};
    fprintln("Value: {:<6} GPF: {:<20}  Fen: {}", std::to_string(v), p.getGamePhaseFactor(), f);
  }
}

// All tests below modify non-essential CONFIG_CONST eval flags at runtime.
// In production builds, these are frozen static constexpr — excluded from production.
#ifndef FRANKYCPP_PRODUCTION

// New test: ensure evaluation with only MATERIAL equals material difference from side to move
TEST_F(EvaluatorTest, EvaluateMaterialOnly) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_MATERIAL = true;
  });

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
      println(p.strBoard());
      ++printed;
    }

    ASSERT_EQ(v, expectedFinal) << "Material-only eval mismatch for FEN: " << fen;
  }
}

TEST_F(EvaluatorTest, KnightMobility_CentralBeatsCorner) {
  // Enable only piece eval (knight mobility) to isolate the effect
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL      = true;
    e.USE_KNIGHT_MOBILITY = true;
    e.USE_GAMEPHASE_VALUE = true;
  });

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
  println(central.strBoard());
  fprintln("Corner Knight Eval:  {}", vCorner);
  println(corner.strBoard());
  fprintln("Knight mobility favors central knight over corner knight: {} > {}", vCentral, vCorner);
}

TEST_F(EvaluatorTest, Pawn_PassedBeatsBlocked) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PAWN_EVAL       = true;
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  // Passed pawn for White: single WP e5, no black pawns
  const Position passed{"6k1/8/8/4P3/8/8/8/6K1 w - - 0 1"};
  // Blocked pawn for White: WP e5 blocked by own WP e6 (only white pawns present)
  const Position blocked{"6k1/8/4P3/4P3/8/8/8/6K1 w - - 0 1"};

  const Value vPassed  = e.evaluate(passed);
  const Value vBlocked = e.evaluate(blocked);

  // Human visual check
  fprintln("Passed pawn eval: {}", vPassed);
  println(passed.strBoard());
  fprintln("Blocked pawn eval: {}", vBlocked);
  println(blocked.strBoard());
  fprintln("Passed > Blocked: {} > {}", vPassed, vBlocked);

  ASSERT_GT(vPassed, vBlocked) << "Passed pawn should evaluate higher than blocked pawn";
}

// Verify that an advanced passed pawn (rank 6) is worth more than a back-rank passed pawn (rank 2).
TEST_F(EvaluatorTest, Pawn_AdvancedPassedBeatsBackRankPassed) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PAWN_EVAL              = true;
    e.USE_PASSED_PAWN_RANK_BONUS = true;
    e.USE_GAMEPHASE_VALUE        = true;
  });

  Evaluator e{};

  // Advanced passed pawn for White on e6 (relative rank 6), no opponent pawns
  const Position advanced{"6k1/8/4P3/8/8/8/8/6K1 w - - 0 1"};
  // Back-rank passed pawn for White on e2 (relative rank 2), no opponent pawns
  const Position backrank{"6k1/8/8/8/8/8/4P3/6K1 w - - 0 1"};

  const Value vAdvanced = e.evaluate(advanced);
  const Value vBackrank = e.evaluate(backrank);

  // Human visual check
  fprintln("Advanced passed pawn (e6) eval: {}", vAdvanced);
  println(advanced.strBoard());
  fprintln("Back-rank passed pawn (e2) eval: {}", vBackrank);
  println(backrank.strBoard());
  fprintln("Advanced > Back-rank: {} > {}", vAdvanced, vBackrank);

  ASSERT_GT(vAdvanced, vBackrank)
      << "Advanced passed pawn should evaluate significantly higher than back-rank passed pawn";

  // The difference should be substantial (rank bonus for rank 6 vs rank 2)
  const int diff = static_cast<int>(vAdvanced) - static_cast<int>(vBackrank);
  fprintln("Difference: {} cp", diff);
  ASSERT_GE(diff, 20) << "Rank-based bonus should create at least 20cp difference between rank 6 and rank 2";
}

// Verify rank-based passed pawn works symmetrically for Black
TEST_F(EvaluatorTest, Pawn_AdvancedPassedBlackSymmetry) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PAWN_EVAL              = true;
    e.USE_PASSED_PAWN_RANK_BONUS = true;
    e.USE_GAMEPHASE_VALUE        = true;
  });

  Evaluator e{};

  // White advanced passed pawn on e6, from White POV
  const Position whiteAdvanced{"6k1/8/4P3/8/8/8/8/6K1 w - - 0 1"};
  // Black advanced passed pawn on e3 (relative rank 6 for Black), from Black POV
  const Position blackAdvanced{"6k1/8/8/8/8/4p3/8/6K1 b - - 0 1"};

  const Value vWhite = e.evaluate(whiteAdvanced);
  const Value vBlack = e.evaluate(blackAdvanced);

  fprintln("White advanced passed (e6, White POV): {}", vWhite);
  fprintln("Black advanced passed (e3, Black POV): {}", vBlack);

  // Both should be positive (advantage for side to move with the advanced passer)
  ASSERT_GT(vWhite, VALUE_ZERO) << "White advanced passer should be positive for White";
  ASSERT_GT(vBlack, VALUE_ZERO) << "Black advanced passer should be positive for Black";

  // Should be roughly symmetric (within tolerance for king PST differences)
  const int diff = std::abs(static_cast<int>(vWhite) - static_cast<int>(vBlack));
  ASSERT_LE(diff, 15) << "Passed pawn rank bonus should be roughly symmetric: White=" << vWhite << " Black=" << vBlack;
}

TEST_F(EvaluatorTest, BishopMobility_CentralBeatsCorner) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL      = true;
    e.USE_BISHOP_MOBILITY = true;
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  // Central bishop on d4, kings g1/g8, extra pawns to avoid insufficiency
  const Position central{"6k1/p7/8/8/3B4/8/7P/6K1 w - - 0 1"};
  // Corner bishop on a1
  const Position corner{"6k1/p7/8/8/8/8/7P/B5K1 w - - 0 1"};

  const Value vCentral = e.evaluate(central);
  const Value vCorner  = e.evaluate(corner);

  // Human visual check
  fprintln("Central Bishop Eval: {}", vCentral);
  println(central.strBoard());
  fprintln("Corner Bishop Eval:  {}", vCorner);
  println(corner.strBoard());
  fprintln("Bishop mobility favors central over corner: {} > {}", vCentral, vCorner);

  ASSERT_GT(vCentral, vCorner) << "Bishop mobility should favor central bishop over corner bishop";
}

TEST_F(EvaluatorTest, Rook_FileBonus_Open_gt_SemiOpen_gt_Closed) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL           = true;
    e.USE_ROOK_OPEN_FILE_BONUS = true;
    e.USE_ROOK_MOBILITY        = false; // isolate file bonus
    e.USE_GAMEPHASE_VALUE      = true;
  });

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
  println(closed.strBoard());
  fprintln("Rook semi-open-file eval: {}", vSemiOpen);
  println(semiOpen.strBoard());
  fprintln("Rook open-file eval: {}", vOpen);
  println(open.strBoard());
  fprintln("Open > Semi-open > Closed: {} > {} > {}", vOpen, vSemiOpen, vClosed);

  ASSERT_GT(vSemiOpen, vClosed) << "Semi-open file should be better than closed file for rook";
  ASSERT_GT(vOpen, vSemiOpen) << "Open file should be better than semi-open file for rook";
}

TEST_F(EvaluatorTest, Rook_PSQT_SeventhRank_BetterThan_BackRank) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_POSITIONAL      = true;  // use PSQT
    e.USE_PIECE_EVAL      = false; // avoid mobility/file
    e.USE_KING_EVAL       = false; // avoid shield
    e.USE_PAWN_EVAL       = false;
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  // Rook on 7th vs back rank, kings only
  const Position seventh{"6k1/3R4/8/8/8/8/8/6K1 w - - 0 1"};
  const Position backrank{"6k1/8/8/8/8/8/8/3R2K1 w - - 0 1"};

  const Value v7th  = e.evaluate(seventh);
  const Value vBack = e.evaluate(backrank);

  // Human visual check
  fprintln("Rook 7th-rank eval: {}", v7th);
  println(seventh.strBoard());
  fprintln("Rook back-rank eval: {}", vBack);
  println(backrank.strBoard());
  fprintln("7th rank > back rank: {} > {}", v7th, vBack);

  ASSERT_GT(v7th, vBack) << "PSQT should reward rook on the 7th rank more than back rank";
}

TEST_F(EvaluatorTest, QueenMobility_CentralBeatsCorner) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL      = true;
    e.USE_QUEEN_MOBILITY  = true;
    e.USE_QUEEN_TROPISM   = false; // isolate mobility
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  const Position central{"6k1/p7/8/8/3Q4/8/7P/6K1 w - - 0 1"};
  const Position corner{"6k1/p7/8/8/8/8/7P/Q5K1 w - - 0 1"};

  const Value vCentral = e.evaluate(central);
  const Value vCorner  = e.evaluate(corner);

  // Human visual check
  fprintln("Central Queen Eval: {}", vCentral);
  println(central.strBoard());
  fprintln("Corner Queen Eval:  {}", vCorner);
  println(corner.strBoard());
  fprintln("Queen mobility favors central over corner: {} > {}", vCentral, vCorner);

  ASSERT_GT(vCentral, vCorner) << "Queen mobility should favor central queen over corner queen";
}

TEST_F(EvaluatorTest, King_PSQT_CenterBeatsCorner_Endgameish) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_POSITIONAL      = true; // PSQT
    e.USE_PIECE_EVAL      = false;
    e.USE_PAWN_EVAL       = false;
    e.USE_KING_EVAL       = false; // avoid dynamic shield
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  // Central king e4 vs corner king a1; symmetric pawns to avoid insufficiency
  const Position central{"6k1/p7/8/8/4K3/8/P7/8 w - - 0 1"};
  const Position cornerK{"6k1/p7/8/8/8/8/P7/K7 w - - 0 1"};

  const Value vCentral = e.evaluate(central);
  const Value vCorner  = e.evaluate(cornerK);

  // Human visual check
  fprintln("Central King Eval: {}", vCentral);
  println(central.strBoard());
  fprintln("Corner King Eval:  {}", vCorner);
  println(cornerK.strBoard());
  fprintln("PSQT favors central king in endgame: {} > {}", vCentral, vCorner);

  ASSERT_GT(vCentral, vCorner) << "PSQT should favor central king in endgame-like positions";
}

TEST_F(EvaluatorTest, BishopPairBonus_TwoBishopsBeats_BishopKnight) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL        = true;
    e.USE_BISHOP_PAIR_BONUS = true;
    e.USE_BISHOP_MOBILITY   = false; // isolate pair bonus
    e.USE_MATERIAL          = false; // equalize material influence
    e.USE_POSITIONAL        = false; // no PSQT influence
    e.USE_PAWN_EVAL         = false;
    e.USE_KING_EVAL         = false;
    e.USE_GAMEPHASE_VALUE   = true;
  });

  Evaluator e{};

  // White has a bishop pair; Black has a bishop+knight. Add a pawn each to avoid insufficiency.
  // Pair: white bishops c1,f1; black bishop c8, knight b8; kings h1/g8; pawns a2/a7.
  const Position pairPos{"1n1b2k1/p7/8/8/8/8/P7/2B2B1K w - - 0 1"};
  // No pair: replace white f1 bishop by knight.
  const Position noPairPos{"1n1b2k1/p7/8/8/8/8/P7/2B2N1K w - - 0 1"};

  const Value vPair   = e.evaluate(pairPos);
  const Value vNoPair = e.evaluate(noPairPos);

  // Human visual check
  fprintln("Bishop pair eval: {}", vPair);
  println(pairPos.strBoard());
  fprintln("Bishop+Knight eval: {}", vNoPair);
  println(noPairPos.strBoard());
  fprintln("Pair > NoPair: {} > {}", vPair, vNoPair);

  ASSERT_GT(vPair, vNoPair) << "Bishop pair bonus should favor two bishops over bishop+knight when material/PSQT are off";
}

// Verify that Black's bishop pair benefits Black (not White).
// Regression test for sign bug where bishop pair bonus was not multiplied by us.sign().
TEST_F(EvaluatorTest, BishopPairBonus_BlackPairCorrectSign) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL        = true;
    e.USE_BISHOP_PAIR_BONUS = true;
    e.USE_BISHOP_MOBILITY   = false; // isolate pair bonus
    e.USE_MATERIAL          = false;
    e.USE_POSITIONAL        = false;
    e.USE_PAWN_EVAL         = false;
    e.USE_KING_EVAL         = false;
    e.USE_GAMEPHASE_VALUE   = true;
  });

  Evaluator e{};

  // Black has bishop pair; White has bishop+knight. Pawns to avoid insufficiency.
  // Black bishops on c8, f8; White bishop c1, knight b1; kings h1/g8; pawns a2/a7.
  const Position blackPair{"2b2bk1/p7/8/8/8/8/P7/1NB4K b - - 0 1"};
  // Neither side has pair: Black bishop+knight; White bishop+knight.
  const Position noPair{"1nb3k1/p7/8/8/8/8/P7/1NB4K b - - 0 1"};

  const Value vBlackPair = e.evaluate(blackPair);
  const Value vNoPair    = e.evaluate(noPair);

  // Human visual check
  fprintln("Black bishop pair eval (from Black POV): {}", vBlackPair);
  println(blackPair.strBoard());
  fprintln("No pair eval (from Black POV): {}", vNoPair);
  println(noPair.strBoard());
  fprintln("Black pair should be better for Black: {} > {}", vBlackPair, vNoPair);

  ASSERT_GT(vBlackPair, vNoPair)
      << "Black's bishop pair should benefit Black (positive from Black's perspective)";
}

// Verify symmetry: White pair bonus == Black pair bonus (same magnitude, opposite sign from white POV)
TEST_F(EvaluatorTest, BishopPairBonus_Symmetry) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL        = true;
    e.USE_BISHOP_PAIR_BONUS = true;
    e.USE_BISHOP_MOBILITY   = false;
    e.USE_MATERIAL          = false;
    e.USE_POSITIONAL        = false;
    e.USE_PAWN_EVAL         = false;
    e.USE_KING_EVAL         = false;
    e.USE_GAMEPHASE_VALUE   = true;
  });

  Evaluator e{};

  // White has bishop pair, Black has knight+bishop
  const Position whitePair{"1n1b2k1/p7/8/8/8/8/P7/2B2B1K w - - 0 1"};
  // Black has bishop pair, White has knight+bishop (mirror)
  const Position blackPair{"2b2b1k/p7/8/8/8/8/P7/1N1B2K1 b - - 0 1"};

  const Value vWhitePair = e.evaluate(whitePair);
  const Value vBlackPair = e.evaluate(blackPair);

  // Human visual check
  fprintln("White pair eval (White POV): {}", vWhitePair);
  fprintln("Black pair eval (Black POV): {}", vBlackPair);

  // Both should be positive (pair holder has advantage) and similar magnitude
  ASSERT_GT(vWhitePair, VALUE_ZERO) << "White pair should give White a positive eval";
  ASSERT_GT(vBlackPair, VALUE_ZERO) << "Black pair should give Black a positive eval";

  // Allow small tolerance for PSQT / king square differences
  const int diff = std::abs(static_cast<int>(vWhitePair) - static_cast<int>(vBlackPair));
  ASSERT_LE(diff, 5) << "Bishop pair bonus should be roughly symmetric: White=" << vWhitePair << " Black=" << vBlackPair;
}

TEST_F(EvaluatorTest, RookMobility_CentralBeatsEdge_FileBonusesOff) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL           = true;
    e.USE_ROOK_MOBILITY        = true;
    e.USE_ROOK_OPEN_FILE_BONUS = false; // isolate mobility
    e.USE_POSITIONAL           = false; // no PSQT influence
    e.USE_MATERIAL             = false;
    e.USE_PAWN_EVAL            = false;
    e.USE_KING_EVAL            = false;
    e.USE_GAMEPHASE_VALUE      = true;
  });

  Evaluator e{};

  // Central rook d4 with own pieces blocking 3 directions; still has east moves (e4..h4)
  const Position central{"6k1/8/8/3P4/2PR4/3P4/8/6K1 w - - 0 1"};
  // Edge rook a1 boxed in by own a2 and b1 -> 0 mobility
  const Position edge{"6k1/8/8/8/8/8/P7/RP4K1 w - - 0 1"};

  const Value vCentral = e.evaluate(central);
  const Value vEdge    = e.evaluate(edge);

  // Human visual check
  fprintln("Central rook eval: {}", vCentral);
  println(central.strBoard());
  fprintln("Edge rook eval:    {}", vEdge);
  println(edge.strBoard());
  fprintln("Central > Edge (mobility): {} > {}", vCentral, vEdge);

  ASSERT_GT(vCentral, vEdge) << "Rook mobility should favor central rook when file bonuses and PSQT are off";
}

TEST_F(EvaluatorTest, QueenTropism_CloserBeatsFarther_EndgameOnly) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL      = true;
    e.USE_QUEEN_MOBILITY  = false; // isolate tropism
    e.USE_QUEEN_TROPISM   = true;  // endgame-only weight
    e.USE_POSITIONAL      = false; // no PSQT influence
    e.USE_MATERIAL        = false;
    e.USE_PAWN_EVAL       = false;
    e.USE_KING_EVAL       = false;
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  // Closer: Qg7 near black king h8
  const Position closer{"7k/6Q1/8/8/8/8/8/6K1 w - - 0 1"};
  // Farther: Qa1 far from black king h8
  const Position farther{"7k/8/8/8/8/8/8/Q5K1 w - - 0 1"};

  const Value vCloser  = e.evaluate(closer);
  const Value vFarther = e.evaluate(farther);

  // Human visual check
  fprintln("Queen closer eval:  {}", vCloser);
  println(closer.strBoard());
  fprintln("Queen farther eval: {}", vFarther);
  println(farther.strBoard());
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
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_LAZY_EVAL       = false; // disable lazy eval for timing test to have all positions fully evaluated
    e.USE_KNIGHT_MOBILITY = false;
  });

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
    os.imbue(projectLocale);
    os.precision(os.precision());

    const uint64_t totalEvals = static_cast<uint64_t>(iterations) * positions.size();

    os << "Evaluate took " << elapsed.count() << " ns for " << iterations << " iterations with " << positions.size() << " positions" << std::endl;
    os << "Evaluate took " << (elapsed.count() / (totalEvals ? totalEvals : 1)) << " ns per evaluation" << std::endl;
    os << "Evaluations per sec " << ((totalEvals * nanoPerSec) / (elapsed.count() ? elapsed.count() : 1)) << " eps" << std::endl;
    std::cout << os.str() << std::endl;
  }
}

// New timing suite: measure impact of each EvalConfig USE_* feature on performance
TEST_F(EvaluatorTest, Timing_EvalConfig_FeatureImpact) {
  if (isBulkRun()) {
    GTEST_SKIP() << "Skipping timing test in bulk run to save time";
  }
  using namespace std::chrono;

  // Prepare a stable set of positions once (avoid counting FEN parsing)
  const std::vector<std::string> allFens = Test_Fens::getFENs();
  ASSERT_FALSE(allFens.empty()) << "Test_Fens::getFENs() returned no positions";

  std::vector<Position> positions;
  positions.reserve(allFens.size());
  for (const auto& f : allFens) positions.emplace_back(f.c_str());

  // Create shared PawnTT and per-thread Evaluator
  PawnTT pawnTT{static_cast<uint64_t>(cm.eval().PAWN_TT_SIZE_MB)};
  Evaluator evaluator{};
  evaluator.setPawnTT(&pawnTT);

  // Helper to update PawnTT when config changes
  auto updatePawnTT = [&]() {
    const auto& evalCfg = cm.eval();
    if (evalCfg.USE_PAWN_TT && evalCfg.PAWN_TT_SIZE_MB > 0) {
      pawnTT.resize(static_cast<uint64_t>(evalCfg.PAWN_TT_SIZE_MB));
    }
    else {
      pawnTT.resize(0);
    }
  };

  // Timing parameters: keep modest to avoid long test duration
  constexpr int repeats    = 5;     // take best-of 'repeats' to reduce noise
  constexpr int iterations = 25000; // per repeat, per position set

  auto measure_ns = [&](const int iters) -> uint64_t {
    volatile int64_t acc = 0; // prevent optimizing away
    const auto start     = high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
      for (auto& p : positions) {
        const Value v{evaluator.evaluate(p)};
        acc += static_cast<int64_t>(v);
      }
    }
    return static_cast<uint64_t>(duration_cast<nanoseconds>(high_resolution_clock::now() - start).count());
  };

  auto best_of_n = [&](const int iters) -> uint64_t {
    uint64_t best = std::numeric_limits<uint64_t>::max();
    for (int r = 0; r < repeats; ++r) {
      const uint64_t ns = measure_ns(iters);
      best              = std::min(best, ns);
    }
    return best == std::numeric_limits<uint64_t>::max() ? 0ull : best;
  };

  // Define dependency-aware cases. Baseline first.
  struct Case {
    std::string label;
    std::function<void()> apply;
  };

  auto make_cases = [&] {
    std::vector<Case> cases;

    // Helper: turn all features ON, disable LAZY, then apply a mutator to EvalConfig
    auto disable = [&](auto fn) {
      set_eval_config(true); // all features ON
      cm.applyOverrides([&](auto&, EvalConfigData& e) {
        e.USE_LAZY_EVAL = false; // ensure lazy eval is OFF for timing
        fn(e);                   // apply specific disables
      });
    };

    // Baseline: all features ON, LAZY OFF
    cases.push_back({"Baseline (all features ON; LAZY OFF)", [&] {
                       set_eval_config(true);
                       cm.applyOverrides([&](auto&, EvalConfigData& e) { e.USE_LAZY_EVAL = false; });
                     }});

    // Each case disables exactly the named feature(s) while leaving others ON
    cases.push_back({"Disable MATERIAL only", [&] {
                       disable([](EvalConfigData& e) { e.USE_MATERIAL = false; });
                     }});
    cases.push_back({"Disable POSITIONAL only", [&] {
                       disable([](EvalConfigData& e) { e.USE_POSITIONAL = false; });
                     }});
    cases.push_back({"Disable TEMPO only", [&] {
                       disable([](EvalConfigData& e) { e.USE_TEMPO = false; });
                     }});

    // Piece eval umbrella off
    cases.push_back({"Disable PIECE_EVAL only", [&] {
                       disable([](EvalConfigData& e) { e.USE_PIECE_EVAL = false; });
                     }});

    // Piece sub-features (PIECE_EVAL stays ON)
    cases.push_back({"Disable BISHOP_PAIR_BONUS (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_BISHOP_PAIR_BONUS = false; });
                     }});
    cases.push_back({"Disable KNIGHT_MOBILITY (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_KNIGHT_MOBILITY = false; });
                     }});
    cases.push_back({"Disable BISHOP_MOBILITY (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_BISHOP_MOBILITY = false; });
                     }});
    cases.push_back({"Disable ROOK_MOBILITY (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_ROOK_MOBILITY = false; });
                     }});
    cases.push_back({"Disable ROOK_OPEN_FILE_BONUS (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_ROOK_OPEN_FILE_BONUS = false; });
                     }});
    cases.push_back({"Disable QUEEN_MOBILITY (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_QUEEN_MOBILITY = false; });
                     }});
    cases.push_back({"Disable QUEEN_TROPISM (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_QUEEN_TROPISM = false; });
                     }});

    // King eval
    cases.push_back({"Disable KING_EVAL only", [&] {
                       disable([](EvalConfigData& e) { e.USE_KING_EVAL = false; });
                     }});
    cases.push_back({"Disable KING_SAFETY_SHIELD (with KING_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_KING_SAFETY_SHIELD = false; });
                     }});

    cases.push_back({"Disable GAMEPHASE_VALUE only", [&] {
                       disable([](EvalConfigData& e) { e.USE_GAMEPHASE_VALUE = false; });
                     }});

    // Pawn eval
    cases.push_back({"Disable PAWN_EVAL only", [&] {
                       disable([](EvalConfigData& e) { e.USE_PAWN_EVAL = false; });
                     }});
    cases.push_back({"Disable PAWN_TT (with PAWN_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_PAWN_TT = false; });
                     }});

    // Final base case: everything OFF (including LAZY)
    cases.push_back({"All features OFF", [&] {
                       set_eval_config(false);
                     }});

    return cases;
  };

  auto cases = make_cases();

  const uint64_t totalEvals = static_cast<uint64_t>(iterations) * positions.size();

  // Precompute label width for a nice alignment
  size_t maxLabel = 4; // length of "Case"
  for (const auto& [label, apply] : cases) maxLabel = std::max(maxLabel, label.size());
  const int labelW    = static_cast<int>(maxLabel);
  constexpr int colW1 = 14; // ns/eval
  constexpr int colW2 = 14; // evals/sec
  constexpr int colW3 = 8;  // delta %

  // Print header
  {
    std::ostringstream hdr;
    hdr.flags(std::cout.flags());
    hdr.imbue(projectLocale);
    hdr << std::left << std::setw(labelW) << "Case" << " | "
        << std::right << std::setw(colW1) << "ns/eval" << " | "
        << std::setw(colW2) << "evals/sec" << " | "
        << std::setw(colW3) << "delta";
    std::cout << std::endl;
    std::cout << hdr.str() << std::endl;
    std::cout << std::string(labelW + 3 + colW1 + 3 + colW2 + 3 + colW3, '-') << std::endl;
  }

  // Helper to print one result line in aligned columns
  auto print_result = [&](const std::string& label, const uint64_t total_ns, const uint64_t baseline_ns) {
    std::ostringstream os;
    os.flags(std::cout.flags());
    os.imbue(projectLocale);

    const uint64_t nsPerEval = total_ns / (totalEvals ? totalEvals : 1);
    const uint64_t eps       = (totalEvals * nanoPerSec) / (total_ns ? total_ns : 1);

    std::string deltaStr;
    if (baseline_ns > 0) {
      const double base_ns_per = static_cast<double>(baseline_ns) / (totalEvals ? totalEvals : 1);
      const double ns_per      = static_cast<double>(total_ns) / (totalEvals ? totalEvals : 1);
      const double deltaPct    = base_ns_per > 0.0 ? ((ns_per - base_ns_per) / base_ns_per) * 100.0 : 0.0;
      std::ostringstream ds;
      ds.setf(std::ios::fixed);
      ds << std::showpos << std::setprecision(1) << deltaPct << "%";
      deltaStr = ds.str();
    }
    else {
      deltaStr = "base";
    }

    os << std::left << std::setw(labelW) << label << " | "
       << std::right << std::setw(colW1) << nsPerEval << " | "
       << std::setw(colW2) << eps << " | "
       << std::setw(colW3) << deltaStr;

    std::cout << os.str() << std::endl;
  };

  // The first case is baseline
  uint64_t baseline_ns = 0;
  bool first           = true;
  for (const auto& [label, apply] : cases) {
    apply();
    // Ensure PawnTT adapts to changed config (e.g., PAWN_TT size/toggle)
    updatePawnTT();
    const uint64_t ns     = best_of_n(iterations);
    const bool isBaseline = first;
    if (first) {
      baseline_ns = ns;
      first       = false;
    }
    print_result(label, ns, isBaseline ? 0 : baseline_ns);
  }
}

#endif // FRANKYCPP_PRODUCTION
