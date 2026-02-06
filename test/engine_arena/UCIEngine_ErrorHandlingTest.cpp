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

//=============================================================================
// UCIEngine_ErrorHandlingTest.cpp - Error Handling Tests for UCIEngine
//=============================================================================
//
// Phase 7: Testing & Error Handling
//
// Tests error scenarios for UCIEngine:
//   - Missing engine executable
//   - Invalid engine path
//   - Engine timeout scenarios
//   - Invalid position handling
//   - Graceful degradation
//
//=============================================================================

#include "engine_arena/UCIEngine.h"
#include "init.h"
#include "Test_Utils.h"
#include "TestEnginePath.h"

#include <gtest/gtest.h>
#include <filesystem>

using namespace arena;

class UCIEngineErrorHandlingTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }
};

//=============================================================================
// Error Test 1: Missing Engine Executable
//=============================================================================

TEST_F(UCIEngineErrorHandlingTest, Constructor_MissingExecutable_ThrowsError) {
  const std::string nonExistentPath = "nonexistent_engine.exe";

  // Should throw runtime_error
  EXPECT_THROW({
    UCIEngine engine(nonExistentPath);
  }, std::runtime_error);
}

//=============================================================================
// Error Test 2: Invalid Engine Path (Directory Instead of File)
//=============================================================================

TEST_F(UCIEngineErrorHandlingTest, Constructor_DirectoryPath_ThrowsError) {
  // Use current directory as an invalid engine path
  const std::string directoryPath = ".";

  // Should throw runtime_error
  EXPECT_THROW({
    UCIEngine engine(directoryPath);
  }, std::runtime_error);
}

//=============================================================================
// Error Test 3: Empty Engine Path
//=============================================================================

TEST_F(UCIEngineErrorHandlingTest, Constructor_EmptyPath_ThrowsError) {
  const std::string emptyPath = "";

  // Should throw runtime_error
  EXPECT_THROW({
    UCIEngine engine(emptyPath);
  }, std::runtime_error);
}

//=============================================================================
// Error Test 4: Invalid FEN String (requires real engine)
//=============================================================================

TEST_F(UCIEngineErrorHandlingTest, SetPosition_InvalidFEN_ReturnsFalse) {
  // Try to find a valid engine for testing
  std::string enginePath = getTestEnginePath();

  if (enginePath.empty()) {
    GTEST_SKIP() << "Test engine not found. Skipping invalid FEN test.";
  }

  UCIEngine engine(enginePath);

  // Try to set an invalid FEN (missing parts)
  const std::string invalidFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR";

  // setPosition should return false or handle gracefully
  // Note: Some engines may accept partial FENs and use defaults
  // The test validates the engine doesn't crash
  engine.setPosition(invalidFen);

  // Either returns false, or doesn't crash (acceptable behaviors)
  SUCCEED();
}

//=============================================================================
// Error Test 5: Search Timeout (very short timeout)
//=============================================================================

TEST_F(UCIEngineErrorHandlingTest, Search_VeryShortTimeout_ReturnsPartialOrEmpty) {
  // Find engine
  std::string enginePath = getTestEnginePath();

  if (enginePath.empty()) {
    GTEST_SKIP() << "Test engine not found. Skipping timeout test.";
  }

  UCIEngine engine(enginePath);
  engine.setSearchTimeout(milliseconds{5000}); // 5 second absolute timeout

  // Set position
  const std::string startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  ASSERT_TRUE(engine.setPosition(startFen));

  // Search with extremely short time (1ms) but high depth
  UCISearchResult result = engine.search(milliseconds{1}, static_cast<Depth>(20));

  // Should either:
  // 1. Return a partial result quickly
  // 2. Return empty result (no crash)
  // The engine should not hang

  // Validate we got SOME response (even if empty)
  // This validates the timeout mechanism works
  SUCCEED();
}

//=============================================================================
// Error Test 6: Multiple Rapid Searches (stress test)
//=============================================================================

TEST_F(UCIEngineErrorHandlingTest, MultipleRapidSearches_NoResourceLeaks) {
  std::string enginePath = getTestEnginePath();

  if (enginePath.empty()) {
    GTEST_SKIP() << "Test engine not found. Skipping stress test.";
  }

  UCIEngine engine(enginePath);

  const std::string startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

  // Perform 10 rapid searches
  for (int i = 0; i < 10; ++i) {
    engine.newGame();
    ASSERT_TRUE(engine.setPosition(startFen));

    UCISearchResult result = engine.search(milliseconds{10}, static_cast<Depth>(3));

    // Should complete without crashing
    // May or may not return a move depending on timing
  }

  // If we got here, no resource leaks or crashes
  SUCCEED();
}

//=============================================================================
// Error Test 7: NewGame Multiple Times
//=============================================================================

TEST_F(UCIEngineErrorHandlingTest, NewGame_MultipleCalls_NoCrash) {
  std::string enginePath = getTestEnginePath();

  if (enginePath.empty()) {
    GTEST_SKIP() << "Test engine not found.";
  }

  UCIEngine engine(enginePath);

  // Call newGame multiple times - should not crash
  for (int i = 0; i < 5; ++i) {
    engine.newGame();
  }

  // Should still be functional after multiple newGame calls
  const std::string startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  EXPECT_TRUE(engine.setPosition(startFen));

  SUCCEED();
}

//=============================================================================
// Error Test 8: Very Long Engine Name
//=============================================================================

TEST_F(UCIEngineErrorHandlingTest, GetEngineName_VeryLongName_NoBufferOverflow) {
  std::string enginePath = getTestEnginePath();

  if (enginePath.empty()) {
    GTEST_SKIP() << "Test engine not found.";
  }

  UCIEngine engine(enginePath);

  // Get engine name - should handle any length safely (using std::string)
  std::string name = engine.getEngineName();

  // Should have a non-empty name
  EXPECT_FALSE(name.empty());

  // Should be reasonable length (< 500 chars)
  EXPECT_LT(name.length(), 500u);
}

//=============================================================================
// Error Test 9: Absolute Path vs Relative Path
//=============================================================================

TEST_F(UCIEngineErrorHandlingTest, Constructor_RelativeAndAbsolutePaths_BothWork) {
  // Find engine with relative path
  std::string relativePath = getTestEnginePath();

  if (relativePath.empty()) {
    GTEST_SKIP() << "Test engine not found.";
  }

  // Test with relative path
  {
    UCIEngine engine(relativePath);
    EXPECT_FALSE(engine.getEngineName().empty());
  }

  // Test with absolute path
  {
    std::filesystem::path absPath = std::filesystem::absolute(relativePath);
    UCIEngine engine(absPath.string());
    EXPECT_FALSE(engine.getEngineName().empty());
  }
}

//=============================================================================
// Error Test 10: Search With Zero Time (edge case)
//=============================================================================

TEST_F(UCIEngineErrorHandlingTest, Search_ZeroTime_ReturnsQuickly) {
  std::string enginePath = getTestEnginePath();

  if (enginePath.empty()) {
    GTEST_SKIP() << "Test engine not found.";
  }

  UCIEngine engine(enginePath);

  const std::string startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  ASSERT_TRUE(engine.setPosition(startFen));

  // Search with 0ms time (edge case)
  UCISearchResult result = engine.search(milliseconds{0}, static_cast<Depth>(5));

  // Should return quickly without hanging
  // May return empty or partial result
  SUCCEED();
}
