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

// centralize test eval config: reset to hard-coded defaults first (ignoring YAML),
// then set all USE_* flags to the given onoff value.
// This ensures tests always run against the .h default numeric weights,
// making them independent of any YAML tuning changes.
// In production, all eval USE_* flags are CONFIG_CONST (static constexpr) — cannot be set at runtime.
// The function and all tests that call it are excluded from production builds.
#ifndef FRANKYCPP_PRODUCTION
void set_eval_config(const bool onoff) {
  // Reset all config to hard-coded struct defaults (EvalConfigData{} / SearchConfigData{})
  // so numeric weights are independent of YAML.
  cm.resetToHardcodedDefaults();
  // Now set USE_* flags
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
    e.USE_KNIGHT_OUTPOST       = onoff;
    e.USE_BISHOP_MOBILITY      = onoff;
    e.USE_BAD_BISHOP           = onoff;
    e.USE_ROOK_MOBILITY        = onoff;
    e.USE_ROOK_OPEN_FILE_BONUS = onoff;
    e.USE_ROOK_7TH_RANK_BONUS  = onoff;
    e.USE_ROOK_BEHIND_PASSER   = onoff;
    e.USE_QUEEN_MOBILITY       = onoff;
    e.USE_QUEEN_TROPISM        = onoff;
    e.USE_KING_EVAL            = onoff;
    e.USE_KING_SAFETY_SHIELD   = onoff;
    e.USE_KING_PAWN_PROXIMITY  = onoff;
    e.USE_KING_SAFETY_ATTACK   = onoff;
    e.USE_PAWN_STORM           = onoff;
    e.USE_KING_OPEN_FILE       = onoff;
    e.USE_SAFE_CHECK           = onoff;
    e.USE_GAMEPHASE_VALUE      = onoff;
    e.USE_BISHOP_PAIR_BONUS    = onoff;
    e.USE_PAWN_ADVANCE_BONUS   = onoff;
    e.USE_THREAT_EVAL          = onoff;
    e.USE_SPACE_EVAL           = onoff;
    e.USE_CONNECTED_ROOKS      = onoff;
    e.USE_MINOR_CONNECTIVITY   = onoff;
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

// Verify the dynamic rook-on-7th-rank bonus (not PSQT) rewards 7th rank over 5th rank.
TEST_F(EvaluatorTest, Rook_SeventhRankBonusBeatsFifthRank) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL          = true;
    e.USE_ROOK_7TH_RANK_BONUS = true;
    e.USE_ROOK_MOBILITY       = false; // isolate 7th rank bonus
    e.USE_ROOK_OPEN_FILE_BONUS = false;
    e.USE_POSITIONAL          = false; // no PSQT
    e.USE_MATERIAL            = false;
    e.USE_PAWN_EVAL           = false;
    e.USE_KING_EVAL           = false;
    e.USE_GAMEPHASE_VALUE     = true;
  });

  Evaluator e{};

  // Rook on 7th rank (d7) with pawns to avoid insufficiency
  const Position seventh{"6k1/3R4/8/8/8/8/p6P/6K1 w - - 0 1"};
  // Rook on 5th rank (d5) — same setup
  const Position fifth{"6k1/8/8/3R4/8/8/p6P/6K1 w - - 0 1"};

  const Value v7th  = e.evaluate(seventh);
  const Value v5th  = e.evaluate(fifth);

  fprintln("Rook 7th-rank bonus eval: {}", v7th);
  println(seventh.strBoard());
  fprintln("Rook 5th-rank eval:       {}", v5th);
  println(fifth.strBoard());
  fprintln("7th > 5th: {} > {}", v7th, v5th);

  ASSERT_GT(v7th, v5th) << "Rook on 7th rank should get a bonus over rook on 5th rank";
}

// Verify Black's rook on 2nd rank (= relative 7th) also gets the bonus.
TEST_F(EvaluatorTest, Rook_SeventhRankBonus_BlackSymmetry) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL          = true;
    e.USE_ROOK_7TH_RANK_BONUS = true;
    e.USE_ROOK_MOBILITY       = false;
    e.USE_ROOK_OPEN_FILE_BONUS = false;
    e.USE_POSITIONAL          = false;
    e.USE_MATERIAL            = false;
    e.USE_PAWN_EVAL           = false;
    e.USE_KING_EVAL           = false;
    e.USE_GAMEPHASE_VALUE     = true;
  });

  Evaluator e{};

  // White rook on 7th (d7), from White POV
  const Position white7th{"6k1/3R4/8/8/8/8/p6P/6K1 w - - 0 1"};
  // Black rook on 2nd (d2 = relative 7th for Black), from Black POV
  const Position black7th{"6k1/p6P/8/8/8/8/3r4/6K1 b - - 0 1"};

  const Value vWhite = e.evaluate(white7th);
  const Value vBlack = e.evaluate(black7th);

  fprintln("White rook 7th eval (White POV): {}", vWhite);
  fprintln("Black rook 2nd eval (Black POV): {}", vBlack);

  ASSERT_GT(vWhite, VALUE_ZERO) << "White rook on 7th should be positive for White";
  ASSERT_GT(vBlack, VALUE_ZERO) << "Black rook on 2nd (relative 7th) should be positive for Black";
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

// Verify king near own passed pawn evaluates better than king far from it.
TEST_F(EvaluatorTest, King_NearPassedPawnBeatsDistant) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_KING_EVAL           = true;
    e.USE_KING_PAWN_PROXIMITY = true;
    e.USE_KING_SAFETY_SHIELD  = false; // isolate proximity
    e.USE_PAWN_EVAL           = false;
    e.USE_POSITIONAL          = false; // no PSQT
    e.USE_MATERIAL            = false;
    e.USE_PIECE_EVAL          = false;
    e.USE_GAMEPHASE_VALUE     = true;
  });

  Evaluator e{};

  // White king near own passed pawn on e5 (king on e4, distance 1)
  const Position nearPos{"6k1/8/8/4P3/4K3/8/8/8 w - - 0 1"};
  // White king far from own passed pawn on e5 (king on a1, distance 4)
  const Position farPos{"6k1/8/8/4P3/8/8/8/K7 w - - 0 1"};

  const Value vNear = e.evaluate(nearPos);
  const Value vFar  = e.evaluate(farPos);

  fprintln("King near passed pawn eval: {}", vNear);
  println(nearPos.strBoard());
  fprintln("King far from passed pawn eval: {}", vFar);
  println(farPos.strBoard());
  fprintln("Near > Far: {} > {}", vNear, vFar);

  ASSERT_GT(vNear, vFar) << "King near own passed pawn should evaluate higher than king far away";
}

// Verify that a position with pieces attacking the enemy king is evaluated higher
// than one where the pieces don't attack the king zone.
TEST_F(EvaluatorTest, KingSafety_AttackedKingWorseThanSafe) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL         = true;
    e.USE_KING_EVAL          = true;
    e.USE_KING_SAFETY_ATTACK = true;
    e.USE_KING_SAFETY_SHIELD = false;
    e.USE_GAMEPHASE_VALUE    = true;
  });

  Evaluator e{};

  // White B+Q both attacking Black king zone on g8 (Bd5→f7, Qh5→h7 = 2 attackers, triggers penalty)
  const Position attacked{"6k1/5ppp/8/3B3Q/8/8/6PP/6K1 w - - 0 1"};
  // Same material but pieces far from Black king zone (Ba2→f7 = only 1 attacker, below ≥2 threshold)
  const Position safe{"6k1/5ppp/8/8/8/8/B5PP/4Q1K1 w - - 0 1"};

  const Value vAttacked = e.evaluate(attacked);
  const Value vSafe     = e.evaluate(safe);

  fprintln("Attacking king zone eval: {}", vAttacked);
  println(attacked.strBoard());
  fprintln("Safe (no attack) eval:    {}", vSafe);
  println(safe.strBoard());
  fprintln("Attacked > Safe: {} > {}", vAttacked, vSafe);

  ASSERT_GT(vAttacked, vSafe)
      << "Position with pieces attacking enemy king zone should evaluate higher for the attacker";
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

// =========================================================================
// KNIGHT OUTPOST TESTS
// =========================================================================

// Knight on an outpost square (d5, no enemy pawns on c/e files ahead)
// supported by own pawn should evaluate better than unsupported.
TEST_F(EvaluatorTest, KnightOutpost_SupportedBeatsUnsupported) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL      = true;
    e.USE_KNIGHT_OUTPOST  = true;
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  // Knight on d5 supported by pawn on c4, no enemy pawns on c/e files forward
  const Position supported{"6k1/p7/8/3N4/2P5/8/P7/6K1 w - - 0 1"};
  // Knight on d5 but NO supporting pawn, still an outpost (no enemy pawns on c/e forward)
  const Position unsupported{"6k1/p7/8/3N4/8/8/PP6/6K1 w - - 0 1"};

  const Value vSupported   = e.evaluate(supported);
  const Value vUnsupported = e.evaluate(unsupported);

  fprintln("Knight outpost supported eval:   {}", vSupported);
  println(supported.strBoard());
  fprintln("Knight outpost unsupported eval: {}", vUnsupported);
  println(unsupported.strBoard());
  fprintln("Supported > Unsupported: {} > {}", vSupported, vUnsupported);

  ASSERT_GT(vSupported, vUnsupported)
      << "Supported knight outpost should evaluate higher than unsupported";
}

// Knight on d5 with enemy pawn on e6 (can attack d5) should NOT get an outpost bonus.
TEST_F(EvaluatorTest, KnightOutpost_CanBeAttackedGetsNoBonus) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL      = true;
    e.USE_KNIGHT_OUTPOST  = true;
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  // Knight on d5, no enemy pawns on c/e forward → outpost
  const Position outpost{"6k1/p7/8/3N4/8/8/PP6/6K1 w - - 0 1"};
  // Knight on d5, but enemy pawn on e6 can attack it → NOT an outpost
  const Position notOutpost{"6k1/p7/4p3/3N4/8/8/PP6/6K1 w - - 0 1"};

  const Value vOutpost    = e.evaluate(outpost);
  const Value vNotOutpost = e.evaluate(notOutpost);

  fprintln("Outpost (no enemy pawn) eval:     {}", vOutpost);
  println(outpost.strBoard());
  fprintln("Not outpost (e6 pawn) eval:       {}", vNotOutpost);
  println(notOutpost.strBoard());
  fprintln("Outpost > Not outpost: {} > {}", vOutpost, vNotOutpost);

  ASSERT_GT(vOutpost, vNotOutpost)
      << "Knight on outpost square (no enemy pawn threat) should evaluate higher than attackable knight";
}

// Verify Black knight outpost is correctly signed (benefits Black)
TEST_F(EvaluatorTest, KnightOutpost_BlackSymmetry) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL      = true;
    e.USE_KNIGHT_OUTPOST  = true;
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  // White knight outpost on d5 supported by c4 pawn
  const Position whiteOutpost{"6k1/p7/8/3N4/2P5/8/P7/6K1 w - - 0 1"};
  // Black knight outpost on d4 supported by c5 pawn (mirror)
  const Position blackOutpost{"6k1/p7/8/2p5/3n4/8/P7/6K1 b - - 0 1"};

  const Value vWhite = e.evaluate(whiteOutpost);
  const Value vBlack = e.evaluate(blackOutpost);

  fprintln("White outpost eval (White POV): {}", vWhite);
  fprintln("Black outpost eval (Black POV): {}", vBlack);

  ASSERT_GT(vWhite, VALUE_ZERO) << "White outpost should be positive for White";
  ASSERT_GT(vBlack, VALUE_ZERO) << "Black outpost should be positive for Black";

  const int diff = std::abs(static_cast<int>(vWhite) - static_cast<int>(vBlack));
  ASSERT_LE(diff, 10) << "Outpost bonus should be roughly symmetric: White=" << vWhite << " Black=" << vBlack;
}

// =========================================================================
// BAD BISHOP TESTS
// =========================================================================

// Bishop with many own pawns on its color should evaluate worse than with few.
TEST_F(EvaluatorTest, BadBishop_ManyPawnsOnColorWorseThanFew) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL      = true;
    e.USE_BAD_BISHOP      = true;
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  // White light-square bishop on f1, 4 white pawns on light squares (d3, e2, f3, g2)
  // Black has one pawn to avoid draw.
  const Position badBishop{"6k1/p7/8/8/8/3P1P2/4PBP1/6K1 w - - 0 1"};
  // White light-square bishop on f1, only 1 white pawn on light square (e2)
  // Other pawns on dark squares (d2, f2)
  const Position goodBishop{"6k1/p7/8/8/8/8/3PPBP1/6K1 w - - 0 1"};

  const Value vBad  = e.evaluate(badBishop);
  const Value vGood = e.evaluate(goodBishop);

  fprintln("Bad bishop (many pawns on color) eval:  {}", vBad);
  println(badBishop.strBoard());
  fprintln("Good bishop (few pawns on color) eval:  {}", vGood);
  println(goodBishop.strBoard());
  fprintln("Good > Bad: {} > {}", vGood, vBad);

  ASSERT_GT(vGood, vBad)
      << "Bishop with fewer own pawns on its color should evaluate higher";
}

// Verify that Black bad bishop penalty is correctly signed (hurts Black).
TEST_F(EvaluatorTest, BadBishop_BlackCorrectSign) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL      = true;
    e.USE_BAD_BISHOP      = true;
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  // Black has bad bishop (many pawns on bishop color), White has one pawn
  const Position blackBad{"6k1/4pbp1/3p1p2/8/8/8/P7/6K1 b - - 0 1"};
  // Black has good bishop (few pawns on bishop color)
  const Position blackGood{"6k1/3ppbp1/8/8/8/8/P7/6K1 b - - 0 1"};

  const Value vBad  = e.evaluate(blackBad);
  const Value vGood = e.evaluate(blackGood);

  fprintln("Black bad bishop eval (Black POV):  {}", vBad);
  fprintln("Black good bishop eval (Black POV): {}", vGood);

  ASSERT_GT(vGood, vBad)
      << "Black's good bishop should evaluate higher (from Black POV) than bad bishop";
}

// =========================================================================
// PAWN ADVANCEMENT BONUS TESTS
// =========================================================================

// Non-passed pawn on rank 5 should evaluate better than same pawn on rank 2.
TEST_F(EvaluatorTest, PawnAdvancement_AdvancedNonPassedBeatsBackRank) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PAWN_EVAL          = true;
    e.USE_PAWN_ADVANCE_BONUS = true;
    e.USE_GAMEPHASE_VALUE    = true;
  });

  Evaluator e{};

  // White pawn on d5 (rank 5, non-passed because black pawn on d7 blocks)
  const Position advanced{"3k4/3p4/8/3P4/8/8/8/3K4 w - - 0 1"};
  // White pawn on d2 (rank 2, non-passed because black pawn on d7 blocks)
  const Position backRank{"3k4/3p4/8/8/8/8/3P4/3K4 w - - 0 1"};

  const Value vAdvanced = e.evaluate(advanced);
  const Value vBackRank = e.evaluate(backRank);

  fprintln("Advanced non-passed pawn (d5) eval: {}", vAdvanced);
  println(advanced.strBoard());
  fprintln("Back-rank non-passed pawn (d2) eval: {}", vBackRank);
  println(backRank.strBoard());
  fprintln("Advanced > BackRank: {} > {}", vAdvanced, vBackRank);

  ASSERT_GT(vAdvanced, vBackRank)
      << "Advanced non-passed pawn should get advancement bonus over back-rank pawn";
}

// Verify advancement bonus does NOT apply to passed pawns (they have their own bonus).
TEST_F(EvaluatorTest, PawnAdvancement_PassedPawnDoesNotGetAdvanceBonus) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PAWN_EVAL          = true;
    e.USE_PAWN_ADVANCE_BONUS = true;
    e.USE_GAMEPHASE_VALUE    = true;
  });

  Evaluator e{};

  // White pawn on d5, no black pawns → passed. Should NOT get advance bonus.
  const Position passedAdv{"6k1/8/8/3P4/8/8/8/6K1 w - - 0 1"};

  // Disable advance bonus and re-evaluate to check there's no difference
  cm.applyOverrides([&](auto&, EvalConfigData& e2) {
    e2.USE_PAWN_ADVANCE_BONUS = false;
  });

  const Value vWithout = e.evaluate(passedAdv);

  cm.applyOverrides([&](auto&, EvalConfigData& e2) {
    e2.USE_PAWN_ADVANCE_BONUS = true;
  });

  const Value vWith = e.evaluate(passedAdv);

  fprintln("Passed pawn d5 WITH advance bonus:    {}", vWith);
  fprintln("Passed pawn d5 WITHOUT advance bonus: {}", vWithout);

  // Should be equal because passed pawns are excluded from advancement bonus
  ASSERT_EQ(vWith, vWithout)
      << "Passed pawns should not get the advancement bonus (they have their own rank-based bonus)";
}

// =========================================================================
// ROOK BEHIND PASSED PAWN TESTS
// =========================================================================

// Rook behind own passed pawn should evaluate better than rook in front of it.
TEST_F(EvaluatorTest, RookBehindPasser_BehindBeatsBefore) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL         = true;
    e.USE_ROOK_BEHIND_PASSER = true;
    e.USE_PAWN_EVAL          = true; // needed for passed pawn detection context
    e.USE_GAMEPHASE_VALUE    = true;
  });

  Evaluator e{};

  // White rook on d1 behind own passed pawn on d5 (no black pawns on c/d/e forward)
  const Position behind{"6k1/8/8/3P4/8/8/8/3R2K1 w - - 0 1"};
  // White rook on d7 IN FRONT of own passed pawn on d5
  const Position inFront{"3R2k1/8/8/3P4/8/8/8/6K1 w - - 0 1"};

  const Value vBehind  = e.evaluate(behind);
  const Value vInFront = e.evaluate(inFront);

  fprintln("Rook behind own passer (d1) eval:  {}", vBehind);
  println(behind.strBoard());
  fprintln("Rook in front of passer (d7) eval: {}", vInFront);
  println(inFront.strBoard());
  fprintln("Behind > InFront: {} > {}", vBehind, vInFront);

  ASSERT_GT(vBehind, vInFront)
      << "Rook behind own passed pawn should evaluate higher than rook in front";
}

// Rook behind enemy passed pawn (blocking it) should also get a bonus.
TEST_F(EvaluatorTest, RookBehindPasser_BehindEnemyPasserBeatsElsewhere) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL         = true;
    e.USE_ROOK_BEHIND_PASSER = true;
    e.USE_PAWN_EVAL          = true;
    e.USE_GAMEPHASE_VALUE    = true;
  });

  Evaluator e{};

  // Black passed pawn on d4, White rook on d7 (behind it = north of pawn advancing south)
  const Position behindEnemy{"6k1/3R4/8/8/3p4/8/8/6K1 w - - 0 1"};
  // Same pawn, but White rook on a7 (not on the passer's file)
  const Position elsewhere{"6k1/R7/8/8/3p4/8/8/6K1 w - - 0 1"};

  const Value vBehind    = e.evaluate(behindEnemy);
  const Value vElsewhere = e.evaluate(elsewhere);

  fprintln("Rook behind enemy passer (d7) eval: {}", vBehind);
  println(behindEnemy.strBoard());
  fprintln("Rook elsewhere (a7) eval:           {}", vElsewhere);
  println(elsewhere.strBoard());
  fprintln("Behind enemy passer > Elsewhere: {} > {}", vBehind, vElsewhere);

  ASSERT_GT(vBehind, vElsewhere)
      << "Rook behind enemy passed pawn should evaluate higher than rook off the file";
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

// =========================================================================
// PAWN STORM TESTS (Phase 2)
// =========================================================================

// Opponent pawns advancing toward our king should penalize our position.
TEST_F(EvaluatorTest, PawnStorm_AdvancingPawnsPenalizeKing) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_KING_EVAL       = true;
    e.USE_PAWN_STORM      = true;
    e.USE_GAMEPHASE_VALUE = false; // midgame-only feature; disable phase blending to avoid zeroing
  });

  Evaluator e{};

  // White king on g1 with pawns f2/g2/h2; black pawns on g4/h4 (storm approaching king)
  const Position storm{"6k1/8/8/8/6pp/8/5PPP/6K1 w - - 0 1"};
  // White king on g1 with pawns f2/g2/h2; black pawns on g7/h7 (far away, no storm)
  const Position noStorm{"6k1/6pp/8/8/8/8/5PPP/6K1 w - - 0 1"};

  const Value vStorm   = e.evaluate(storm);
  const Value vNoStorm = e.evaluate(noStorm);

  fprintln("Storm (black pawns advancing) eval: {}", vStorm);
  println(storm.strBoard());
  fprintln("No storm eval:                      {}", vNoStorm);
  println(noStorm.strBoard());
  fprintln("NoStorm > Storm: {} > {}", vNoStorm, vStorm);

  ASSERT_GT(vNoStorm, vStorm)
      << "Position with pawn storm toward king should evaluate worse for defender";
}

// Verify pawn storm works symmetrically for Black.
TEST_F(EvaluatorTest, PawnStorm_BlackKingUnderStorm) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_KING_EVAL       = true;
    e.USE_PAWN_STORM      = true;
    e.USE_GAMEPHASE_VALUE = false; // midgame-only feature; disable phase blending
  });

  Evaluator e{};

  // Black king on g8 with pawns f7/g7/h7; white pawns on g5/h5 (storm approaching black king)
  const Position stormBlack{"6k1/5ppp/8/6PP/8/8/8/6K1 b - - 0 1"};
  // Black king on g8 with pawns f7/g7/h7; white pawns on g2/h2 (far away, no storm)
  const Position noStormBlack{"6k1/5ppp/8/8/8/8/6PP/6K1 b - - 0 1"};

  const Value vStorm   = e.evaluate(stormBlack);
  const Value vNoStorm = e.evaluate(noStormBlack);

  fprintln("Black under storm eval (Black POV): {}", vStorm);
  fprintln("Black no storm eval (Black POV):    {}", vNoStorm);

  ASSERT_GT(vNoStorm, vStorm)
      << "Black position with pawn storm should evaluate worse for Black";
}

// =========================================================================
// KING OPEN FILE TESTS (Phase 2)
// =========================================================================

// King with open files nearby should evaluate worse than king with intact pawn cover.
TEST_F(EvaluatorTest, KingOpenFile_OpenFilesPenalizeKing) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_KING_EVAL       = true;
    e.USE_KING_OPEN_FILE  = true;
    e.USE_GAMEPHASE_VALUE = false; // midgame-only feature; disable phase blending
  });

  Evaluator e{};

  // White king on g1 with NO pawns on f/g/h files (3 open files near king)
  const Position openFiles{"6k1/pppppppp/8/8/8/8/PPPPP3/6K1 w - - 0 1"};
  // White king on g1 with pawns on f2, g2, h2 (all files covered)
  const Position closedFiles{"6k1/pppppppp/8/8/8/8/PPPPPPP1/6K1 w - - 0 1"};

  const Value vOpen   = e.evaluate(openFiles);
  const Value vClosed = e.evaluate(closedFiles);

  fprintln("Open files near king eval:   {}", vOpen);
  println(openFiles.strBoard());
  fprintln("Closed files near king eval: {}", vClosed);
  println(closedFiles.strBoard());
  fprintln("Closed > Open: {} > {}", vClosed, vOpen);

  ASSERT_GT(vClosed, vOpen)
      << "King with open files nearby should evaluate worse than king with pawn cover";
}

// Semi-open file (no own pawn, enemy pawn present) should be penalized less than fully open.
TEST_F(EvaluatorTest, KingOpenFile_SemiOpenLessThanFullyOpen) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_KING_EVAL       = true;
    e.USE_KING_OPEN_FILE  = true;
    e.USE_GAMEPHASE_VALUE = false; // midgame-only feature; disable phase blending
  });

  Evaluator e{};

  // White king on g1; f-file fully open (no pawns at all), g/h have white pawns.
  // Black king on a8 with a/b files covered — not affected by f-file change.
  const Position fullyOpen{"k7/pp6/8/8/8/8/PPPPP1PP/6K1 w - - 0 1"};
  // Same but black pawn on f7 → f-file is semi-open for White king (no own pawn, enemy pawn present)
  const Position semiOpen{"k7/pp3p2/8/8/8/8/PPPPP1PP/6K1 w - - 0 1"};

  const Value vFullyOpen = e.evaluate(fullyOpen);
  const Value vSemiOpen  = e.evaluate(semiOpen);

  fprintln("Fully open f-file near king eval: {}", vFullyOpen);
  fprintln("Semi-open f-file near king eval:  {}", vSemiOpen);
  fprintln("SemiOpen > FullyOpen: {} > {}", vSemiOpen, vFullyOpen);

  // Fully open should be worse (more negative penalty) than semi-open
  ASSERT_GT(vSemiOpen, vFullyOpen)
      << "Fully open file near king should be penalized more than semi-open";
}

// =========================================================================
// SAFE CHECK SQUARES TESTS (Phase 2)
// =========================================================================

// Exposed king (many safe check squares) should evaluate worse than sheltered king.
// Requires USE_PIECE_EVAL to populate attackedBy[] (individual piece features remain off,
// only the attackedBy accumulation runs — no score noise from mobility/outpost/etc.).
TEST_F(EvaluatorTest, SafeCheck_ExposedKingWorseThanSheltered) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL      = true;  // needed to populate attackedBy[]
    e.USE_KING_EVAL       = true;
    e.USE_SAFE_CHECK      = true;
    e.USE_GAMEPHASE_VALUE = false; // midgame-only feature; disable phase blending
  });

  Evaluator e{};

  // White king on f3 (exposed, walked out into the open) — black has full piece set
  const Position exposed{"rnbqkbnr/pppppppp/8/8/4P3/5K2/PPPP1PPP/RNBQ1BNR w kq - 0 1"};
  // White king on e1 (sheltered behind pawns and pieces) — same black setup
  const Position sheltered{"rnbqkbnr/pppppppp/8/8/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 1"};

  const Value vExposed   = e.evaluate(exposed);
  const Value vSheltered = e.evaluate(sheltered);

  fprintln("Exposed king (f3) eval:   {}", vExposed);
  println(exposed.strBoard());
  fprintln("Sheltered king (e1) eval: {}", vSheltered);
  println(sheltered.strBoard());
  fprintln("Sheltered > Exposed: {} > {}", vSheltered, vExposed);

  ASSERT_GT(vSheltered, vExposed)
      << "Sheltered king with pawns should have fewer safe check squares and evaluate better";
}

// Toggle test: enabling safe check should change the evaluation.
// Requires USE_PIECE_EVAL to populate attackedBy[] for the safeMask filter.
// Uses an asymmetric position (white king exposed, black castled) so penalties don't cancel.
TEST_F(EvaluatorTest, SafeCheck_ToggleChangesEval) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_PIECE_EVAL      = true;  // needed to populate attackedBy[]
    e.USE_KING_EVAL       = true;
    e.USE_SAFE_CHECK      = false; // off first
    e.USE_GAMEPHASE_VALUE = false; // raw (mid+end)/2 to avoid phase blending masking the difference
  });

  Evaluator e{};

  // White king exposed on f3, black castled kingside — asymmetric king safety
  const Position pos{"r1bq1rk1/pppppppp/2n2n2/2b5/4P3/3P1K2/PPP2PPP/RNBQ1BNR w - - 0 1"};

  const Value vWithout = e.evaluate(pos);

  cm.applyOverrides([&](auto&, EvalConfigData& e2) {
    e2.USE_SAFE_CHECK = true; // now on
  });

  const Value vWith = e.evaluate(pos);

  fprintln("Without safe check: {}", vWithout);
  fprintln("With safe check:    {}", vWith);

  // Values should differ (asymmetric king exposure means unequal penalties)
  ASSERT_NE(vWith, vWithout)
      << "Enabling safe check should change the evaluation";
}

// =========================================================================
// THREAT EVALUATION TESTS (Phase 3)
// =========================================================================

// Tier 1: pawn attacks on pieces — white pawn attacking black rook should help white.
TEST_F(EvaluatorTest, ThreatByPawn_PawnAttacksRook) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_MATERIAL        = true;
    e.USE_PIECE_EVAL      = true;  // needed to populate attackedByPT for non-pawn pieces
    e.USE_THREAT_EVAL     = true;
    e.USE_GAMEPHASE_VALUE = false;
  });

  Evaluator e{};

  // White pawn on e5 attacks black rook on d6 (rook under pawn attack)
  const Position threatened{"6k1/8/3r4/4P3/8/8/8/6K1 w - - 0 1"};
  // Same material but rook safe on d7 (not attacked by pawn)
  const Position safe{"6k1/3r4/8/4P3/8/8/8/6K1 w - - 0 1"};

  const Value vThreatened = e.evaluate(threatened);
  const Value vSafe       = e.evaluate(safe);

  fprintln("Pawn threatens rook eval (White POV): {}", vThreatened);
  fprintln("Rook safe eval (White POV):           {}", vSafe);

  // White should be better when the pawn attacks the rook
  ASSERT_GT(vThreatened, vSafe)
      << "Position with pawn attacking rook should evaluate better for white";
}

// Tier 2: minor attacks on major — knight attacking queen (non-reciprocal attack pattern).
TEST_F(EvaluatorTest, ThreatByMinor_KnightAttacksQueen) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_MATERIAL        = true;
    e.USE_PIECE_EVAL      = true;
    e.USE_THREAT_EVAL     = true;
    e.USE_GAMEPHASE_VALUE = false;
  });

  Evaluator e{};

  // White knight on e5 attacks black queen on f7 (knight attacks are non-reciprocal —
  // queen on f7 does NOT attack e5, so no counter-threat making the knight hanging)
  const Position threatened{"6k1/5q2/8/4N3/8/8/8/6K1 w - - 0 1"};
  // Same material, queen on h7 (not attacked by knight from e5)
  const Position safe{"6k1/7q/8/4N3/8/8/8/6K1 w - - 0 1"};

  const Value vThreatened = e.evaluate(threatened);
  const Value vSafe       = e.evaluate(safe);

  fprintln("Knight threatens queen eval (White POV): {}", vThreatened);
  fprintln("Queen safe eval (White POV):             {}", vSafe);

  ASSERT_GT(vThreatened, vSafe)
      << "Position with knight attacking queen should evaluate better for white";
}

// Tier 3: hanging piece — black knight attacked by white bishop, not defended by black.
// Pawns are added so the position is not flagged as insufficient material (KBvKN is drawn).
TEST_F(EvaluatorTest, ThreatHanging_UndefendedPiece) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_MATERIAL        = true;
    e.USE_PIECE_EVAL      = true;
    e.USE_THREAT_EVAL     = true;
    e.USE_GAMEPHASE_VALUE = false;
  });

  Evaluator e{};

  // Black knight on d5 attacked by white bishop on b3 — no black piece defends d5.
  // Each side has a pawn to avoid insufficient material detection.
  const Position hanging{"6k1/p7/8/3n4/8/1B6/P7/6K1 w - - 0 1"};
  // Same material but black knight on g6 (not attacked by bishop on b3)
  const Position safe{"6k1/p7/6n1/8/8/1B6/P7/6K1 w - - 0 1"};

  const Value vHanging = e.evaluate(hanging);
  const Value vSafe    = e.evaluate(safe);

  fprintln("Hanging knight eval (White POV): {}", vHanging);
  fprintln("Safe knight eval (White POV):    {}", vSafe);

  ASSERT_GT(vHanging, vSafe)
      << "Position with hanging enemy knight should evaluate better for white";
}

// Toggle test: enabling/disabling USE_THREAT_EVAL should change evaluation.
TEST_F(EvaluatorTest, ThreatEval_ToggleChangesEval) {
  set_eval_config(true);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_THREAT_EVAL = false; // start with threats OFF
  });

  Evaluator e{};

  // Position with threats: white pawn attacks black knight, white bishop attacks black rook
  const Position pos{"6k1/8/3r4/2n1P3/8/1B6/8/6K1 w - - 0 1"};

  const Value vWithout = e.evaluate(pos);

  cm.applyOverrides([&](auto&, EvalConfigData& e2) {
    e2.USE_THREAT_EVAL = true; // now on
  });

  const Value vWith = e.evaluate(pos);

  fprintln("Without threat eval: {}", vWithout);
  fprintln("With threat eval:    {}", vWith);

  // Values should differ (asymmetric threats)
  ASSERT_NE(vWith, vWithout)
      << "Enabling threat eval should change the evaluation";
}

// Symmetric position should have approximately equal threat impact.
TEST_F(EvaluatorTest, ThreatEval_SymmetricPosition) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_MATERIAL        = true;
    e.USE_POSITIONAL      = true;
    e.USE_PIECE_EVAL      = true;
    e.USE_THREAT_EVAL     = true;
    e.USE_GAMEPHASE_VALUE = true;
  });

  Evaluator e{};

  // Fully symmetric position — threats should cancel out
  const Position symmetric{"r1bqkbnr/pppppppp/2n5/8/8/2N5/PPPPPPPP/R1BQKBNR w KQkq - 0 1"};

  const Value vWithThreats = e.evaluate(symmetric);

  cm.applyOverrides([&](auto&, EvalConfigData& e2) {
    e2.USE_THREAT_EVAL = false;
  });

  const Value vWithoutThreats = e.evaluate(symmetric);

  fprintln("Symmetric with threats:    {}", vWithThreats);
  fprintln("Symmetric without threats: {}", vWithoutThreats);

  // Difference should be small (≤ 5 cp) since threats are symmetric
  const int diff = std::abs(static_cast<int>(vWithThreats) - static_cast<int>(vWithoutThreats));
  ASSERT_LE(diff, 5)
      << "Symmetric position should have minimal threat eval difference, got " << diff;
}

// =========================================================================
// SPACE EVALUATION TESTS (Phase 3.1)
// =========================================================================

// Position with more space (pawns on e4,d4 controlling center) should evaluate better.
TEST_F(EvaluatorTest, SpaceEval_MoreSpaceBetter) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_SPACE_EVAL = true;
    // Note: attackedByPT[PAWN] is always pre-computed, no need for USE_PIECE_EVAL.
    // USE_MATERIAL disabled — we're testing space only; the positions have unequal
    // pawn counts and material would dominate the evaluation.
  });

  Evaluator e{};

  // White has pawns on all 8 files (strong center, lots of space behind pawn chain)
  const Position bigSpace{"rnbqkbnr/pppppppp/8/8/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 1"};
  // White has pawns only on a2,h2 — only 2 pawn files, much less space
  const Position noSpace{"rnbqkbnr/pppppppp/8/8/8/8/P6P/RNBQKBNR w KQkq - 0 1"};

  const Value vBigSpace = e.evaluate(bigSpace);
  const Value vNoSpace  = e.evaluate(noSpace);

  fprintln("Big space eval (White POV):  {}", vBigSpace);
  fprintln("No space eval (White POV):   {}", vNoSpace);

  // White to move: higher value = better for White.
  // bigSpace has 8 pawn files (24 space squares) vs noSpace with 2 files (6 squares).
  // Both sides' Black space is identical (24), so the net difference comes from White's side.
  ASSERT_GT(vBigSpace, vNoSpace)
      << "Position with more space behind pawn chain should evaluate better";
}

// Toggle test: enabling/disabling USE_SPACE_EVAL should change evaluation.
TEST_F(EvaluatorTest, SpaceEval_ToggleChangesEval) {
  set_eval_config(true);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_SPACE_EVAL = false;
  });

  Evaluator e{};

  // Asymmetric pawn structure — space differs between sides
  const Position pos{"r1bqkbnr/pppppppp/2n5/8/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 0 1"};

  const Value vWithout = e.evaluate(pos);

  cm.applyOverrides([&](auto&, EvalConfigData& e2) {
    e2.USE_SPACE_EVAL = true;
  });

  const Value vWith = e.evaluate(pos);

  fprintln("Without space eval: {}", vWithout);
  fprintln("With space eval:    {}", vWith);

  ASSERT_NE(vWith, vWithout)
      << "Enabling space eval should change the evaluation";
}

// =========================================================================
// PIECE COORDINATION TESTS (Phase 3.3)
// =========================================================================

// Connected rooks on the same file should evaluate better than separated rooks.
TEST_F(EvaluatorTest, ConnectedRooks_SameFileBonus) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_MATERIAL        = true;
    e.USE_PIECE_EVAL      = true;
    e.USE_CONNECTED_ROOKS = true;
    e.USE_GAMEPHASE_VALUE = false;
  });

  Evaluator e{};

  // White rooks on d1 and d4 — same file, nothing between (d2,d3 empty)
  const Position connected{"4k3/8/8/8/3R4/8/8/3R2K1 w - - 0 1"};
  // White rooks on a1 and h1 — different files, not connected
  const Position separated{"4k3/8/8/8/8/8/8/R5KR w - - 0 1"};

  const Value vConnected = e.evaluate(connected);
  const Value vSeparated = e.evaluate(separated);

  fprintln("Connected rooks eval (White POV): {}", vConnected);
  fprintln("Separated rooks eval (White POV): {}", vSeparated);

  ASSERT_GT(vConnected, vSeparated)
      << "Connected rooks on same file should evaluate better than separated rooks";
}

// Connected rooks on the same rank should also get bonus.
TEST_F(EvaluatorTest, ConnectedRooks_SameRankBonus) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_MATERIAL        = true;
    e.USE_PIECE_EVAL      = true;
    e.USE_CONNECTED_ROOKS = true;
    e.USE_GAMEPHASE_VALUE = false;
  });

  Evaluator e{};

  // White rooks on a1 and h1 — same rank, pieces between (king on g1)
  const Position blocked{"4k3/8/8/8/8/8/8/R5KR w - - 0 1"};
  // White rooks on d1 and e1 — same rank, nothing between
  const Position connected{"4k3/8/8/8/8/8/8/3RRK2 w - - 0 1"};

  const Value vBlocked   = e.evaluate(blocked);
  const Value vConnected = e.evaluate(connected);

  fprintln("Blocked rooks eval (White POV):   {}", vBlocked);
  fprintln("Connected rank eval (White POV):  {}", vConnected);

  ASSERT_GT(vConnected, vBlocked)
      << "Connected rooks on same rank with no pieces between should evaluate better";
}

// Minor connectivity: knight defended by bishop should get bonus.
TEST_F(EvaluatorTest, MinorConnectivity_KnightDefendedByBishop) {
  set_eval_config(false);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_MATERIAL           = true;
    e.USE_PIECE_EVAL         = true;
    e.USE_MINOR_CONNECTIVITY = true;
    e.USE_GAMEPHASE_VALUE    = false;
  });

  Evaluator e{};

  // White bishop on c4 defends knight on e6 (bishop diagonal reaches e6)
  // Adding pawns to avoid insufficient material
  const Position connected{"4k3/p7/4n3/8/2B5/8/P4N2/6K1 w - - 0 1"};
  // White bishop on a2 and knight on h3 — far apart, no connectivity
  const Position separated{"4k3/p7/8/8/8/7N/B4n2/6K1 w - - 0 1"};

  const Value vConnected = e.evaluate(connected);
  const Value vSeparated = e.evaluate(separated);

  fprintln("Connected minors eval (White POV): {}", vConnected);
  fprintln("Separated minors eval (White POV): {}", vSeparated);

  // Connected should be better (or at least equal — other positional factors may dominate)
  ASSERT_GE(vConnected, vSeparated)
      << "Connected minor pieces should evaluate at least as well as separated ones";
}

// Toggle test: enabling coordination features should change evaluation.
TEST_F(EvaluatorTest, CoordinationEval_ToggleChangesEval) {
  set_eval_config(true);
  cm.applyOverrides([&](auto&, EvalConfigData& e) {
    e.USE_CONNECTED_ROOKS    = false;
    e.USE_MINOR_CONNECTIVITY = false;
  });

  Evaluator e{};

  // Position where White Nd4 and Nf3 mutually defend each other (2 connections),
  // while Black pieces on starting squares have 0 minor connectivity.
  const Position pos{"rnbqkbnr/pppppppp/8/8/3NP3/5N2/PPPP1PPP/R1BQKB1R w KQkq - 0 1"};

  const Value vWithout = e.evaluate(pos);

  cm.applyOverrides([&](auto&, EvalConfigData& e2) {
    e2.USE_CONNECTED_ROOKS    = true;
    e2.USE_MINOR_CONNECTIVITY = true;
  });

  const Value vWith = e.evaluate(pos);

  fprintln("Without coordination eval: {}", vWithout);
  fprintln("With coordination eval:    {}", vWith);

  ASSERT_NE(vWith, vWithout)
      << "Enabling coordination eval should change the evaluation";
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
  Logger::setLoggerLevel(Logger::get().EVAL_LOG, spdlog::level::warn);

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
    return best == std::numeric_limits<uint64_t>::max() ? 0ULL : best;
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
    cases.push_back({"Disable KNIGHT_OUTPOST (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_KNIGHT_OUTPOST = false; });
                     }});
    cases.push_back({"Disable BISHOP_MOBILITY (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_BISHOP_MOBILITY = false; });
                     }});
    cases.push_back({"Disable BAD_BISHOP (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_BAD_BISHOP = false; });
                     }});
    cases.push_back({"Disable ROOK_MOBILITY (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_ROOK_MOBILITY = false; });
                     }});
    cases.push_back({"Disable ROOK_OPEN_FILE_BONUS (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_ROOK_OPEN_FILE_BONUS = false; });
                     }});
    cases.push_back({"Disable ROOK_BEHIND_PASSER (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_ROOK_BEHIND_PASSER = false; });
                     }});
    cases.push_back({"Disable QUEEN_MOBILITY (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_QUEEN_MOBILITY = false; });
                     }});
    cases.push_back({"Disable QUEEN_TROPISM (with PIECE_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_QUEEN_TROPISM = false; });
                     }});

    // Threat eval
    cases.push_back({"Disable THREAT_EVAL only", [&] {
                       disable([](EvalConfigData& e) { e.USE_THREAT_EVAL = false; });
                     }});

    // Space eval
    cases.push_back({"Disable SPACE_EVAL only", [&] {
                       disable([](EvalConfigData& e) { e.USE_SPACE_EVAL = false; });
                     }});

    // Piece coordination
    cases.push_back({"Disable CONNECTED_ROOKS only", [&] {
                       disable([](EvalConfigData& e) { e.USE_CONNECTED_ROOKS = false; });
                     }});
    cases.push_back({"Disable MINOR_CONNECTIVITY only", [&] {
                       disable([](EvalConfigData& e) { e.USE_MINOR_CONNECTIVITY = false; });
                     }});

    // King eval
    cases.push_back({"Disable KING_EVAL only", [&] {
                       disable([](EvalConfigData& e) { e.USE_KING_EVAL = false; });
                     }});
    cases.push_back({"Disable KING_SAFETY_SHIELD (with KING_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_KING_SAFETY_SHIELD = false; });
                     }});
    cases.push_back({"Disable PAWN_STORM (with KING_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_PAWN_STORM = false; });
                     }});
    cases.push_back({"Disable KING_OPEN_FILE (with KING_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_KING_OPEN_FILE = false; });
                     }});
    cases.push_back({"Disable SAFE_CHECK (with KING_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_SAFE_CHECK = false; });
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
    cases.push_back({"Disable PAWN_ADVANCE_BONUS (with PAWN_EVAL)", [&] {
                       disable([](EvalConfigData& e) { e.USE_PAWN_ADVANCE_BONUS = false; });
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
