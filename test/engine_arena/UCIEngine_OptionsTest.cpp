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
// UCIEngine_OptionsTest.cpp - Unit Tests for UCI Options Parsing
//=============================================================================
//
// Tests the setUciOptions() method to ensure proper parsing of:
//   - Single-word option names (e.g., "Hash=256")
//   - Multi-word option names with spaces (e.g., "UCI_LimitStrength=true")
//   - Multiple options (semicolon-separated - REQUIRED!)
//   - Edge cases (empty strings, invalid format, attempting space separator)
//
// IMPORTANT: Semicolon (;) is the ONLY valid separator for multiple options!
//   Correct:   "Hash=256; Ponder=false"
//   Wrong:     "Hash=256 Ponder=false"  (space is NOT a separator - will fail!)
//
//=============================================================================

#include "engine_arena/UCIEngine.h"
#include "init.h"
#include "Test_Utils.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <sstream>

using namespace arena;

namespace {

// Helper: Get path to test engine
std::string getTestEnginePath() {
  std::string path = "cmake-build-win-release/src/FrankyCPP_v1.1.exe";
  if (std::filesystem::exists(path)) {
    return path;
  }
  path = "../src/FrankyCPP_v1.1.exe";
  if (std::filesystem::exists(path)) {
    return path;
  }
  path = "FrankyCPP_v1.1.exe";
  if (std::filesystem::exists(path)) {
    return path;
  }
  return "";
}

} // anonymous namespace

class UCIEngineOptionsTest : public ::testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }

protected:
  void SetUp() override {
    testEnginePath = getTestEnginePath();
    if (testEnginePath.empty()) {
      GTEST_SKIP() << "Test engine not found. Skipping UCI options tests.";
    }
    println("Using test engine at: " + testEnginePath);
  }
  std::string testEnginePath;
};

//=============================================================================
// Test 1: Single Option - Single Word Name
//=============================================================================

TEST_F(UCIEngineOptionsTest, SingleOption_SingleWordName) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true); // See the UCI communication

  // Test with single-word option name
  EXPECT_NO_THROW({
    engine.setUciOptions("Hash=256");
  });

  // Verify option was applied (FrankyCPP reports current values)
  auto options = engine.getOptions();
  EXPECT_EQ(options["Hash"], "256") << "Hash should be set to 256";

  SUCCEED();
}

//=============================================================================
// Test 2: Single Option - Multi-Word Name (UCI Spec Allows Spaces)
//=============================================================================

TEST_F(UCIEngineOptionsTest, SingleOption_MultiWordName) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Test with multi-word option names (UCI spec allows spaces)
  // Note: FrankyCPP uses single-word UCI standard names (OwnBook, Hash, etc.)
  // This test validates the parser can handle spaces if other engines use them
  EXPECT_NO_THROW({
    engine.setUciOptions("Test Option=value");
  });

  SUCCEED();
}

//=============================================================================
// Test 2b: FrankyCPP Standard UCI Options
//=============================================================================

TEST_F(UCIEngineOptionsTest, FrankyCPP_StandardOptions) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Test FrankyCPP's UCI standard options (single-word names)
  EXPECT_NO_THROW({
    engine.setUciOptions("OwnBook=false; Hash=256; Ponder=false");
  });

  // Verify options were applied
  auto options = engine.getOptions();
  EXPECT_EQ(options["OwnBook"], "false") << "OwnBook should be false";
  EXPECT_EQ(options["Hash"], "256") << "Hash should be 256";
  EXPECT_EQ(options["Ponder"], "false") << "Ponder should be false";

  SUCCEED();
}

//=============================================================================
// Test 3: Multiple Options - Semicolon Separated (CORRECT METHOD)
//=============================================================================

TEST_F(UCIEngineOptionsTest, MultipleOptions_SemicolonSeparated_Correct) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // CORRECT: Use semicolon separator for multiple options
  // This is unambiguous and always works
  EXPECT_NO_THROW({
    engine.setUciOptions("Hash=128; Ponder=false");
  });

  // Verify both options were applied correctly
  auto options = engine.getOptions();
  EXPECT_EQ(options["Hash"], "128") << "Hash should be 128";
  EXPECT_EQ(options["Ponder"], "false") << "Ponder should be false";

  SUCCEED();
}

//=============================================================================
// Test 4: Space is NOT a Separator (Negative Test)
//=============================================================================

TEST_F(UCIEngineOptionsTest, SpaceSeparator_DoesNotWork) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // IMPORTANT: Space is NOT a separator!
  // The parser ONLY recognizes semicolon (;) as a separator.
  //
  // "Hash=256 Ponder=true" will be parsed as:
  //   - name: "Hash"
  //   - value: "256 Ponder=true"  (everything after = until semicolon or end)
  //
  // The engine will reject this as malformed:
  //   "Could not set option: Hash = 256 Ponder=true"
  //
  // This test demonstrates the error - it should NOT crash,
  // but the option will NOT be set correctly.

  EXPECT_NO_THROW({
    engine.setUciOptions("Hash=256 Ponder=true");
  });

  // Do NOT verify options here - space is NOT a separator!
  // The engine WILL reject this as malformed.
  // MUST use semicolon: "Hash=256; Ponder=false"

  SUCCEED();
}

//=============================================================================
// Test 5: Multi-Word Option Names with Underscores
//=============================================================================

TEST_F(UCIEngineOptionsTest, MultiWordNames_WithUnderscores) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Test option names with underscores (common UCI pattern)
  // Note: FrankyCPP uses spaces in names, not underscores, so these won't be recognized
  EXPECT_NO_THROW({
    engine.setUciOptions("UCI_EngineAbout=TestEngine; UCI_ShowCurrLine=false");
  });

  SUCCEED();
}

//=============================================================================
// Test 6: Boolean Values
//=============================================================================

TEST_F(UCIEngineOptionsTest, BooleanValues) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Test boolean option values
  EXPECT_NO_THROW({
    engine.setUciOptions("OwnBook=false; Ponder=true");
  });

  // Verify boolean values were applied
  auto options = engine.getOptions();
  EXPECT_EQ(options["OwnBook"], "false") << "OwnBook should be false";
  EXPECT_EQ(options["Ponder"], "true") << "Ponder should be true";

  SUCCEED();
}

//=============================================================================
// Test 7: Numeric Values
//=============================================================================

TEST_F(UCIEngineOptionsTest, NumericValues) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Test numeric option values (using FrankyCPP's actual numeric options)
  EXPECT_NO_THROW({
    engine.setUciOptions("Hash=512; Move Overhead=100");
  });

  // Verify numeric values were applied
  auto options = engine.getOptions();
  EXPECT_EQ(options["Hash"], "512") << "Hash should be 512";
  EXPECT_EQ(options["Move Overhead"], "100") << "Move Overhead should be 100";

  SUCCEED();
}

//=============================================================================
// Test 8: Empty String
//=============================================================================

TEST_F(UCIEngineOptionsTest, EmptyString) {
  UCIEngine engine(testEnginePath);

  // Empty string should be handled gracefully (no options sent)
  EXPECT_NO_THROW({
    engine.setUciOptions("");
  });

  SUCCEED();
}

//=============================================================================
// Test 9: Whitespace Handling
//=============================================================================

TEST_F(UCIEngineOptionsTest, WhitespaceHandling) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Test with extra whitespace (should be trimmed)
  EXPECT_NO_THROW({
    engine.setUciOptions("  Hash=256  ;  Ponder=false  ");
  });

  // Verify whitespace was properly trimmed and options applied
  auto options = engine.getOptions();
  EXPECT_EQ(options["Hash"], "256") << "Hash should be 256 (whitespace trimmed)";
  EXPECT_EQ(options["Ponder"], "false") << "Ponder should be false (whitespace trimmed)";

  SUCCEED();
}

//=============================================================================
// Test 10: Invalid Format - Missing Equals
//=============================================================================

TEST_F(UCIEngineOptionsTest, InvalidFormat_MissingEquals) {
  UCIEngine engine(testEnginePath);

  // Invalid format should be handled gracefully (warning printed, but no crash)
  EXPECT_NO_THROW({
    engine.setUciOptions("InvalidOption");
  });

  SUCCEED();
}

//=============================================================================
// Test 11: Invalid Format - Empty Name or Value
//=============================================================================

TEST_F(UCIEngineOptionsTest, InvalidFormat_EmptyNameOrValue) {
  UCIEngine engine(testEnginePath);

  // Invalid format with empty name or value
  EXPECT_NO_THROW({
    engine.setUciOptions("=256"); // Empty name
    engine.setUciOptions("Hash="); // Empty value
  });

  SUCCEED();
}

//=============================================================================
// Test 12: Mixed Valid and Invalid Options
//=============================================================================

TEST_F(UCIEngineOptionsTest, MixedValidAndInvalid) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Mix of valid and invalid options - valid ones should be processed
  EXPECT_NO_THROW({
    engine.setUciOptions("Hash=256; InvalidOption; Ponder=false");
  });

  SUCCEED();
}

//=============================================================================
// Test 13: FrankyCPP Specific Options (UCI Standard Names)
//=============================================================================

TEST_F(UCIEngineOptionsTest, FrankyCPP_SpecificOptions) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Test FrankyCPP-specific options
  EXPECT_NO_THROW({
    engine.setUciOptions("OwnBook=false; Hash=256; Move Overhead=50");
  });

  // Verify all options were applied
  auto options = engine.getOptions();
  EXPECT_EQ(options["OwnBook"], "false") << "OwnBook should be false";
  EXPECT_EQ(options["Hash"], "256") << "Hash should be 256";
  EXPECT_EQ(options["Move Overhead"], "50") << "Move Overhead should be 50";

  SUCCEED();
}

//=============================================================================
// Test 14: Long Option String with Standard Names
//=============================================================================

TEST_F(UCIEngineOptionsTest, LongOptionString) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Test with multiple FrankyCPP options
  EXPECT_NO_THROW({
    engine.setUciOptions(
      "Hash=256; "
      "OwnBook=false; "
      "Ponder=false; "
      "Move Overhead=100"
    );
  });

  // Verify all options in the long string were applied
  auto options = engine.getOptions();
  EXPECT_EQ(options["Hash"], "256") << "Hash should be 256";
  EXPECT_EQ(options["OwnBook"], "false") << "OwnBook should be false";
  EXPECT_EQ(options["Ponder"], "false") << "Ponder should be false";
  EXPECT_EQ(options["Move Overhead"], "100") << "Move Overhead should be 100";

  SUCCEED();
}

//=============================================================================
// Test 15: Options After newGame
//=============================================================================

TEST_F(UCIEngineOptionsTest, OptionsAfter_newGame) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Set options, call newGame, verify engine still works
  EXPECT_NO_THROW({
    engine.setUciOptions("Hash=256");
    engine.newGame();

    // Set position and search to verify engine still functional
    const std::string startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    engine.setPosition(startFen);

    UCISearchResult result = engine.search(milliseconds{100}, static_cast<Depth>(5));

    // Should return a move (engine still works after options + newGame)
    EXPECT_FALSE(result.bestMove.empty());
  });
}

//=============================================================================
// Test 16: Options with setOption vs setUciOptions
//=============================================================================

TEST_F(UCIEngineOptionsTest, SetOption_vs_SetUciOptions) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Both methods should work
  EXPECT_NO_THROW({
    // Single option via setOption
    engine.setOption("Hash", "128");

    // Multiple options via setUciOptions
    engine.setUciOptions("Ponder=false");
  });

  // Verify both methods applied their options correctly
  auto options = engine.getOptions();
  EXPECT_EQ(options["Hash"], "128") << "Hash should be 128 (set via setOption)";
  EXPECT_EQ(options["Ponder"], "false") << "Ponder should be false (set via setUciOptions)";

  SUCCEED();
}

//=============================================================================
// Test 17: Stress Test - Rapid Option Changes
//=============================================================================

TEST_F(UCIEngineOptionsTest, StressTest_RapidOptionChanges) {
  UCIEngine engine(testEnginePath);

  // Set options multiple times rapidly
  EXPECT_NO_THROW({
    for (int i = 0; i < 5; ++i) {
      engine.setUciOptions("Hash=128");
      engine.setUciOptions("Hash=256");
    }
  });

  // Verify final value is correct after rapid changes
  auto options = engine.getOptions();
  EXPECT_EQ(options["Hash"], "256") << "Hash should be 256 (last value set)";

  SUCCEED();
}

//=============================================================================
// Test 18: UCI Standard Options
//=============================================================================

TEST_F(UCIEngineOptionsTest, UCI_StandardOptions) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Test standard UCI options that most engines support
  EXPECT_NO_THROW({
    engine.setUciOptions("Hash=256; Ponder=false");
  });

  // Verify UCI standard options were applied
  auto options = engine.getOptions();
  EXPECT_EQ(options["Hash"], "256") << "Hash should be 256";
  EXPECT_EQ(options["Ponder"], "false") << "Ponder should be false";

  SUCCEED();
}

//=============================================================================
// Test 19: Space Handling - Parser Capability Test
//=============================================================================

TEST_F(UCIEngineOptionsTest, SpaceHandling_InNamesAndValues) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Test that the parser CAN handle spaces (per UCI spec)
  // Even though FrankyCPP uses single-word names, the parser should support spaces
  // for compatibility with other engines that may use multi-word option names
  EXPECT_NO_THROW({
    // Parser should handle these correctly (even if engine doesn't recognize them)
    engine.setUciOptions("Test Option=value");
    engine.setUciOptions("Another Test=123");

    // Multiple options with semicolon separator
    engine.setUciOptions("Option One=true; Option Two=false");
  });

  // The important thing is that the parser doesn't crash
  // and correctly separates name from value even with spaces

  SUCCEED();
}

//=============================================================================
// Test 20: FrankyCPP Real-World Configuration
//=============================================================================

TEST_F(UCIEngineOptionsTest, FrankyCPP_RealWorldConfig) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Test realistic FrankyCPP configuration
  EXPECT_NO_THROW({
    engine.setUciOptions(
      "OwnBook=false; "
      "Hash=512; "
      "Ponder=false; "
      "Move Overhead=50"
    );
  });

  // These should send:
  // setoption name OwnBook value false
  // setoption name Hash value 512
  // setoption name Ponder value false
  // setoption name Move Overhead value 50

  // Verify all options in real-world config were applied
  auto options = engine.getOptions();
  EXPECT_EQ(options["OwnBook"], "false") << "OwnBook should be false";
  EXPECT_EQ(options["Hash"], "512") << "Hash should be 512";
  EXPECT_EQ(options["Ponder"], "false") << "Ponder should be false";
  EXPECT_EQ(options["Move Overhead"], "50") << "Move Overhead should be 50";

  SUCCEED();
}

//=============================================================================
// Test 21: Verify Options Applied (Using getOptions)
//=============================================================================

TEST_F(UCIEngineOptionsTest, VerifyOptionsApplied_GetOptions) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Set some options
  EXPECT_NO_THROW({
    engine.setUciOptions("OwnBook=false; Hash=256");
  });

  // Get current options and verify they were applied
  auto options = engine.getOptions();

  // Note: This verification works with FrankyCPP because it reports "current" field
  // For standard UCI engines (like Stockfish), this may return original defaults
  // since UCI protocol has no standard way to query current option values.

  // Verify OwnBook was set to false
  ASSERT_TRUE(options.find("OwnBook") != options.end())
    << "OwnBook option not found in engine options";
  EXPECT_EQ(options["OwnBook"], "false")
    << "OwnBook should be set to false (FrankyCPP reports current value)";

  // Verify Hash was set to 256
  ASSERT_TRUE(options.find("Hash") != options.end())
    << "Hash option not found in engine options";
  EXPECT_EQ(options["Hash"], "256")
    << "Hash should be set to 256 (FrankyCPP reports current value)";

  SUCCEED();
}

//=============================================================================
// Test 22: Verify Multiple Option Changes
//=============================================================================

TEST_F(UCIEngineOptionsTest, VerifyMultipleOptionChanges) {
  UCIEngine engine(testEnginePath);
  engine.setDebugMode(true);

  // Note: This test works with FrankyCPP (reports "current" field)
  // For standard UCI engines, this may not detect changes correctly

  // Set initial options
  engine.setUciOptions("Hash=128; Ponder=true");

  auto options1 = engine.getOptions();
  EXPECT_EQ(options1["Hash"], "128");
  EXPECT_EQ(options1["Ponder"], "true");

  // Change options
  engine.setUciOptions("Hash=512; Ponder=false");

  auto options2 = engine.getOptions();
  EXPECT_EQ(options2["Hash"], "512") << "Hash should be updated to 512 (FrankyCPP current value)";
  EXPECT_EQ(options2["Ponder"], "false") << "Ponder should be updated to false (FrankyCPP current value)";

  SUCCEED();
}

//=============================================================================
// Test 23: GetOptions Without Setting Any
//=============================================================================

TEST_F(UCIEngineOptionsTest, GetOptions_WithoutSettingAny) {
  UCIEngine engine(testEnginePath);

  // Get default options without setting anything
  auto options = engine.getOptions();

  // Should have some options (FrankyCPP has many UCI options)
  EXPECT_GT(options.size(), 0u) << "Engine should report some UCI options";

  // FrankyCPP standard options should be present
  EXPECT_TRUE(options.find("Hash") != options.end()) << "Hash option should exist";
  EXPECT_TRUE(options.find("OwnBook") != options.end()) << "OwnBook option should exist";

  SUCCEED();
}
