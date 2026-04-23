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

#include "common/Logging.h"
#include "enginetest/EdpTest.h"
#include "enginetest/EpdParser.h"
#include "enginetest/TestTypes.h"
#include "init.h"

#include <gtest/gtest.h>

using namespace common;
using namespace chess;
using namespace enginetest;

class EpdParser_Test : public testing::Test {
public:
  static void SetUpTestSuite() {
    init::init();
    Logger::get().TSUITE_LOG->set_level(spdlog::level::warn);
  }

protected:
  void SetUp() override {}
  void TearDown() override {}
};

// ============================================================================
// parseOneLine Tests - Valid EPD Lines
// ============================================================================

TEST_F(EpdParser_Test, parseOneLine_ValidBestMove) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4 d4 ; id \"Opening\" ;";

  const auto result = EpdParser::parseOneLine(line);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->getId(), "Opening");
  EXPECT_EQ(result->getFen(), "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  EXPECT_EQ(result->getType(), TestType::BM);
  EXPECT_EQ(result->getTargetMoves().size(), 2);
}

TEST_F(EpdParser_Test, parseOneLine_ValidAvoidMove) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 am a3 h3 ; id \"Avoid flank\" ;";

  const auto result = EpdParser::parseOneLine(line);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->getId(), "Avoid flank");
  EXPECT_EQ(result->getType(), TestType::AM);
  EXPECT_EQ(result->getTargetMoves().size(), 2);
}

TEST_F(EpdParser_Test, parseOneLine_ValidDirectMate) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "8/8/8/8/8/8/6K1/6Qr b - - 0 1 dm 1 ; id \"Mate in 1\" ;";

  const auto result = EpdParser::parseOneLine(line);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->getId(), "Mate in 1");
  EXPECT_EQ(result->getType(), TestType::DM);
  EXPECT_EQ(result->getMateDepth(), 1);
}

TEST_F(EpdParser_Test, parseOneLine_NoId) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4 ;";

  const auto result = EpdParser::parseOneLine(line);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->getId(), "no ID");
}

TEST_F(EpdParser_Test, parseOneLine_MovesWithAnnotations) {
  // Annotations like ! and ? should be stripped
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4! d4?? ; id \"Test\" ;";

  const auto result = EpdParser::parseOneLine(line);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->getTargetMoves().size(), 2);
}

// ============================================================================
// parseOneLine Tests - Invalid EPD Lines
// ============================================================================

TEST_F(EpdParser_Test, parseOneLine_EmptyLine) {
  const std::string line;
  const auto result = EpdParser::parseOneLine(line);
  EXPECT_FALSE(result.has_value());
}

TEST_F(EpdParser_Test, parseOneLine_CommentOnly) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "# This is a comment";
  const auto result      = EpdParser::parseOneLine(line);
  EXPECT_FALSE(result.has_value());
}

TEST_F(EpdParser_Test, parseOneLine_WhitespaceOnly) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "   \t  \n  ";
  const auto result      = EpdParser::parseOneLine(line);
  EXPECT_FALSE(result.has_value());
}

TEST_F(EpdParser_Test, parseOneLine_InvalidFen) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "invalid_fen bm e4 ; id \"Bad FEN\" ;";
  const auto result      = EpdParser::parseOneLine(line);
  EXPECT_FALSE(result.has_value());
}

TEST_F(EpdParser_Test, parseOneLine_InvalidTestType) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 xx e4 ; id \"Bad type\" ;";
  const auto result      = EpdParser::parseOneLine(line);
  EXPECT_FALSE(result.has_value());
}

TEST_F(EpdParser_Test, parseOneLine_InvalidMove) {
  // Qa8 is not legal from starting position
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm Qa8 ; id \"Illegal\" ;";
  const auto result      = EpdParser::parseOneLine(line);
  EXPECT_FALSE(result.has_value());
}

TEST_F(EpdParser_Test, parseOneLine_InvalidMateDepth) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "8/8/8/8/8/8/6K1/6Qr b - - 0 1 dm abc ; id \"Bad depth\" ;";
  const auto result      = EpdParser::parseOneLine(line);
  EXPECT_FALSE(result.has_value());
}

TEST_F(EpdParser_Test, parseOneLine_ZeroMateDepth) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "8/8/8/8/8/8/6K1/6Qr b - - 0 1 dm 0 ; id \"Zero depth\" ;";
  const auto result      = EpdParser::parseOneLine(line);
  EXPECT_FALSE(result.has_value());
}

TEST_F(EpdParser_Test, parseOneLine_NegativeMateDepth) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "8/8/8/8/8/8/6K1/6Qr b - - 0 1 dm -1 ; id \"Negative\" ;";
  const auto result      = EpdParser::parseOneLine(line);
  EXPECT_FALSE(result.has_value());
}

// ============================================================================
// parseOneLine Tests - Edge Cases
// ============================================================================

TEST_F(EpdParser_Test, parseOneLine_TrailingComment) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4 ; id \"Test\" ; # comment";
  const auto result      = EpdParser::parseOneLine(line);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->getId(), "Test");
}

TEST_F(EpdParser_Test, parseOneLine_ExtraWhitespace) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "  rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1   bm   e4   ;   id \"Test\" ;  ";
  const auto result      = EpdParser::parseOneLine(line);
  ASSERT_TRUE(result.has_value());
}

TEST_F(EpdParser_Test, parseOneLine_MultipleSpacesBetweenMoves) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4    d4   Nf3 ; id \"Test\" ;";
  const auto result      = EpdParser::parseOneLine(line);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->getTargetMoves().size(), 3);
}

// ============================================================================
// parseFile Tests
// ============================================================================

TEST_F(EpdParser_Test, parseFile_NonExistentFile) {
  const std::vector<EpdTest> tests = EpdParser::parseFile("nonexistent_file_xyz.epd");
  EXPECT_TRUE(tests.empty());
}

TEST_F(EpdParser_Test, parseFile_ValidFile) {
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += "/test/testsets/franky_tests.epd";

  const std::vector<EpdTest> tests = EpdParser::parseFile(filePath);

  EXPECT_EQ(tests.size(), 13); // Known to have 13 valid tests
}

TEST_F(EpdParser_Test, parseFile_MixedValidInvalid) {
  // This test would require creating a temporary test file
  // For now, we rely on franky_tests.epd which may contain comments
  std::string filePath = FrankyCPP_PROJECT_ROOT;
  filePath += "/test/testsets/franky_tests.epd";

  const std::vector<EpdTest> tests = EpdParser::parseFile(filePath);

  // Should only contain valid tests, invalid lines are skipped
  EXPECT_GT(tests.size(), 0);
}

// ============================================================================
// Expected Move Setting
// ============================================================================

TEST_F(EpdParser_Test, parseOneLine_ExpectedMoveSet) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 bm e4 d4 ; id \"Test\" ;";

  const auto result = EpdParser::parseOneLine(line);

  ASSERT_TRUE(result.has_value());
  // Expected move should be the first move in the list
  EXPECT_TRUE(result->getExpectedMove().isValid());
}

TEST_F(EpdParser_Test, parseOneLine_DirectMateNoExpectedMove) {
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string line = "8/8/8/8/8/8/6K1/6Qr b - - 0 1 dm 1 ; id \"Mate\" ;";

  const auto result = EpdParser::parseOneLine(line);

  ASSERT_TRUE(result.has_value());
  // DM tests don't have an expected move set
  EXPECT_EQ(result->getExpectedMove(), MOVE_NONE);
}
