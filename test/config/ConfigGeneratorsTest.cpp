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

#include "config/ConfigGenerators.h"
#include "config/ConfigRegistry.h"
#include "config/EvalConfigData.h"
#include "config/SearchConfigData.h"

#include <gtest/gtest.h>

class ConfigGeneratorsTest : public ::testing::Test {
protected:
  SearchConfigData search;
  EvalConfigData eval;

  void SetUp() override {
    // Reset to default values before each test
    search = SearchConfigData{};
    eval   = EvalConfigData{};
  }
};

//=============================================================================
// generateConfigString Tests
//=============================================================================

TEST_F(ConfigGeneratorsTest, GenerateConfigStringNotEmpty) {
  const std::string output = generateConfigString(search, eval);
  fprintln("Generated Config String:\n{}", output); // Debug output
  EXPECT_FALSE(output.empty());
}

TEST_F(ConfigGeneratorsTest, GenerateConfigStringContainsDomainHeaders) {
  const std::string output = generateConfigString(search, eval);
  fprintln("Generated Config String:\n{}", output); // Debug output
  // Should have domain headers
  EXPECT_NE(output.find("=== General ==="), std::string::npos);
  EXPECT_NE(output.find("=== Search ==="), std::string::npos);
  EXPECT_NE(output.find("=== Eval ==="), std::string::npos);
}

TEST_F(ConfigGeneratorsTest, GenerateConfigStringContainsKeySearchConfigs) {
  const std::string output = generateConfigString(search, eval);

  // Verify some key Search configs are present
  EXPECT_NE(output.find("USE_NMP:"), std::string::npos);
  EXPECT_NE(output.find("TT_SIZE_MB:"), std::string::npos);
  EXPECT_NE(output.find("USE_LMR:"), std::string::npos);
  EXPECT_NE(output.find("USE_SINGULAR_EXT:"), std::string::npos);
}

TEST_F(ConfigGeneratorsTest, GenerateConfigStringContainsKeyEvalConfigs) {
  const std::string output = generateConfigString(search, eval);

  // Verify some key Eval configs are present
  EXPECT_NE(output.find("USE_MATERIAL:"), std::string::npos);
  EXPECT_NE(output.find("TEMPO:"), std::string::npos);
  EXPECT_NE(output.find("LAZY_THRESHOLD:"), std::string::npos);
}

TEST_F(ConfigGeneratorsTest, GenerateConfigStringContainsAllDisplayConfigs) {
  const std::string output = generateConfigString(search, eval);
  const auto& registry     = ConfigRegistry::instance();

  // Every config with display=true should appear in output
  for (const auto* def : registry.displayOptions()) {
    const std::string searchFor = def->name + ":";
    EXPECT_NE(output.find(searchFor), std::string::npos)
        << "Missing config in output: " << def->name;
  }
}

TEST_F(ConfigGeneratorsTest, GenerateConfigStringReflectsValues) {
  // Modify a value and verify it appears in output
  search.TT_SIZE_MB = 256;
  const std::string output = generateConfigString(search, eval);

  EXPECT_NE(output.find("TT_SIZE_MB: 256"), std::string::npos);
}

TEST_F(ConfigGeneratorsTest, GenerateConfigStringForDomainFiltersCorrectly) {
  const std::string searchOnly = generateConfigStringForDomain(search, eval, ConfigDomain::Search);

  // Should contain Search header
  EXPECT_NE(searchOnly.find("=== Search ==="), std::string::npos);

  // Should NOT contain other domain headers
  EXPECT_EQ(searchOnly.find("=== Eval ==="), std::string::npos);
  EXPECT_EQ(searchOnly.find("=== General ==="), std::string::npos);
}

TEST_F(ConfigGeneratorsTest, ShowAllIncludesNonDisplayConfigs) {
  const std::string normalOutput = generateConfigString(search, eval, false);
  const std::string allOutput    = generateConfigString(search, eval, true);

  // The "showAll" output should be at least as long as normal
  EXPECT_GE(allOutput.size(), normalOutput.size());

  // CONFIG_SOURCE has display=false, should appear in showAll but not normal
  // (if there are any non-display configs)
  const auto& registry = ConfigRegistry::instance();
  for (const auto& def : registry.all()) {
    if (!def.exposure.display) {
      const std::string searchFor = def.name + ":";
      EXPECT_EQ(normalOutput.find(searchFor), std::string::npos)
          << "Non-display config should not appear in normal output: " << def.name;
      EXPECT_NE(allOutput.find(searchFor), std::string::npos)
          << "Non-display config should appear in showAll output: " << def.name;
    }
  }
}

//=============================================================================
// SearchConfigData::str() Tests
//=============================================================================

TEST_F(ConfigGeneratorsTest, SearchConfigStrNotEmpty) {
  const std::string output = search.str();
  EXPECT_FALSE(output.empty());
}

TEST_F(ConfigGeneratorsTest, SearchConfigStrContainsGeneralAndSearch) {
  const std::string output = search.str();

  // Should have General and Search headers
  EXPECT_NE(output.find("=== General ==="), std::string::npos);
  EXPECT_NE(output.find("=== Search ==="), std::string::npos);

  // Should NOT have Eval header
  EXPECT_EQ(output.find("=== Eval ==="), std::string::npos);
}

TEST_F(ConfigGeneratorsTest, SearchConfigStrContainsKeyConfigs) {
  const std::string output = search.str();

  // Key general configs
  EXPECT_NE(output.find("MOVE_OVERHEAD_MS:"), std::string::npos);
  EXPECT_NE(output.find("USE_BOOK:"), std::string::npos);

  // Key search configs
  EXPECT_NE(output.find("USE_NMP:"), std::string::npos);
  EXPECT_NE(output.find("USE_LMR:"), std::string::npos);
  EXPECT_NE(output.find("TT_SIZE_MB:"), std::string::npos);
}

#ifndef FRANKYCPP_PRODUCTION // In production, only essential config mutations (MOVE_OVERHEAD_MS) can be verified, so this test is dev-only.
TEST_F(ConfigGeneratorsTest, SearchConfigStrReflectsModifiedValues) {
  search.TT_SIZE_MB = 512;
  search.USE_NMP    = false;

  const std::string output = search.str();

  EXPECT_NE(output.find("TT_SIZE_MB: 512"), std::string::npos);
  EXPECT_NE(output.find("USE_NMP: false"), std::string::npos);
}
#endif

//=============================================================================
// EvalConfigData::str() Tests
//=============================================================================

TEST_F(ConfigGeneratorsTest, EvalConfigStrNotEmpty) {
  const std::string output = eval.str();
  EXPECT_FALSE(output.empty());
}

TEST_F(ConfigGeneratorsTest, EvalConfigStrContainsEvalOnly) {
  const std::string output = eval.str();

  // Should have Eval header
  EXPECT_NE(output.find("=== Eval ==="), std::string::npos);

  // Should NOT have General or Search headers
  EXPECT_EQ(output.find("=== General ==="), std::string::npos);
  EXPECT_EQ(output.find("=== Search ==="), std::string::npos);
}

TEST_F(ConfigGeneratorsTest, EvalConfigStrContainsKeyConfigs) {
  const std::string output = eval.str();

  // Key eval configs
  EXPECT_NE(output.find("USE_MATERIAL:"), std::string::npos);
  EXPECT_NE(output.find("TEMPO:"), std::string::npos);
  EXPECT_NE(output.find("LAZY_THRESHOLD:"), std::string::npos);
  EXPECT_NE(output.find("USE_PAWN_EVAL:"), std::string::npos);
}

#ifndef FRANKYCPP_PRODUCTION // In production, only essential config mutations (MOVE_OVERHEAD_MS) can be verified, so this test is dev-only.
TEST_F(ConfigGeneratorsTest, EvalConfigStrReflectsModifiedValues) {
  eval.TEMPO           = 50;
  eval.LAZY_THRESHOLD  = 500;

  const std::string output = eval.str();

  EXPECT_NE(output.find("TEMPO: 50"), std::string::npos);
  EXPECT_NE(output.find("LAZY_THRESHOLD: 500"), std::string::npos);
}
#endif

//=============================================================================
// Array Value Tests
//=============================================================================

TEST_F(ConfigGeneratorsTest, ArrayValuesFormattedCorrectly) {
  const std::string output = search.str();

  // Check FP_MARGIN array is present and formatted as comma-separated
  EXPECT_NE(output.find("FP_MARGIN: 0,100,200,300,500,900,1200"), std::string::npos);
}

//=============================================================================
// parseYamlConfig Tests (Phase 3)
//=============================================================================

TEST_F(ConfigGeneratorsTest, ParseYamlConfigBasicScalars) {
  YAML::Node node;
  node["TT_SIZE_MB"]  = 256;
  node["BOOK_PATH"]   = "/custom/path/book.txt";

#ifndef FRANKYCPP_PRODUCTION
  node["USE_NMP"]     = false;
#endif

  SearchConfigData s;
  const auto parsed = parseYamlConfig(node, s);

  EXPECT_EQ(s.TT_SIZE_MB, 256);
  EXPECT_EQ(s.BOOK_PATH, "/custom/path/book.txt");
#ifndef FRANKYCPP_PRODUCTION
  EXPECT_FALSE(s.USE_NMP);
  EXPECT_EQ(parsed.size(), 3);
  EXPECT_TRUE(parsed.contains("USE_NMP"));
  EXPECT_TRUE(parsed.contains("TT_SIZE_MB"));
  EXPECT_TRUE(parsed.contains("BOOK_PATH"));
#endif
}

#ifndef FRANKYCPP_PRODUCTION // In production, only essential config mutations (MOVE_OVERHEAD_MS) can be verified, so this test is dev-only.
TEST_F(ConfigGeneratorsTest, ParseYamlConfigBoolValues) {
  YAML::Node node;
  node["USE_TT"]     = true;
  node["USE_PONDER"] = false;

  SearchConfigData s;
  s.USE_TT     = false;  // Start with opposite values
  s.USE_PONDER = true;

  parseYamlConfig(node, s);

  EXPECT_TRUE(s.USE_TT);
  EXPECT_FALSE(s.USE_PONDER);
}
#endif

TEST_F(ConfigGeneratorsTest, ParseYamlConfigDoubleValues) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  YAML::Node node;
  node["INSTABILITY_STABLE_FACTOR"] = 0.75;
  node["INSTABILITY_EXTEND_FACTOR"] = 1.5;

  SearchConfigData s;
  parseYamlConfig(node, s);

  EXPECT_NEAR(s.INSTABILITY_STABLE_FACTOR, 0.75, 1e-9);
  EXPECT_NEAR(s.INSTABILITY_EXTEND_FACTOR, 1.5, 1e-9);
}

TEST_F(ConfigGeneratorsTest, ParseYamlConfigArrayAsSequence) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  YAML::Node node;
  node["FP_MARGIN"] = std::vector{0, 150, 250, 350, 550, 950, 1250};

  SearchConfigData s;
  parseYamlConfig(node, s);

  EXPECT_EQ(s.FP_MARGIN[0], 0);
  EXPECT_EQ(s.FP_MARGIN[1], 150);
  EXPECT_EQ(s.FP_MARGIN[2], 250);
  EXPECT_EQ(s.FP_MARGIN[3], 350);
  EXPECT_EQ(s.FP_MARGIN[4], 550);
  EXPECT_EQ(s.FP_MARGIN[5], 950);
  EXPECT_EQ(s.FP_MARGIN[6], 1250);
}

TEST_F(ConfigGeneratorsTest, ParseYamlConfigMissingKeysPreserveDefaults) {
  const YAML::Node node;  // Empty node

  SearchConfigData s        = {};
  const int originalTTSize = s.TT_SIZE_MB;
  const bool originalUseNMP = s.USE_NMP;

  const auto parsed = parseYamlConfig(node, s);

  EXPECT_EQ(parsed.size(), 0);
  EXPECT_EQ(s.TT_SIZE_MB, originalTTSize);
  EXPECT_EQ(s.USE_NMP, originalUseNMP);
}

TEST_F(ConfigGeneratorsTest, ParseYamlConfigUnknownKeysNotParsed) {
  YAML::Node node;
  node["UNKNOWN_KEY"]         = "value";
  node["ANOTHER_UNKNOWN_KEY"] = 123;

  SearchConfigData s;
  // warnUnknown=false to suppress warning logs in test
  const auto parsed = parseYamlConfig(node, s, /* warnUnknown= */ false);

  EXPECT_EQ(parsed.size(), 0);
}

TEST_F(ConfigGeneratorsTest, ParseYamlConfigEvalConfig) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  YAML::Node node;
  node["TEMPO"]            = 50;
  node["LAZY_THRESHOLD"]   = 500;
  node["USE_LAZY_EVAL"]    = false;

  EvalConfigData e;
  const auto parsed = parseYamlConfig(node, e);

  EXPECT_EQ(e.TEMPO, 50);
  EXPECT_EQ(e.LAZY_THRESHOLD, 500);
  EXPECT_FALSE(e.USE_LAZY_EVAL);
  EXPECT_EQ(parsed.size(), 3);
}

TEST_F(ConfigGeneratorsTest, ParseYamlConfigMixedSearchAndEval) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  YAML::Node node;
  node["USE_NMP"]        = false;
  node["TT_SIZE_MB"]     = 128;
  node["TEMPO"]          = 40;
  node["LAZY_THRESHOLD"] = 600;

  SearchConfigData s;
  EvalConfigData e;
  const auto parsed = parseYamlConfig(node, s, e);

  EXPECT_FALSE(s.USE_NMP);
  EXPECT_EQ(s.TT_SIZE_MB, 128);
  EXPECT_EQ(e.TEMPO, 40);
  EXPECT_EQ(e.LAZY_THRESHOLD, 600);
  EXPECT_EQ(parsed.size(), 4);
}

TEST_F(ConfigGeneratorsTest, ParseYamlConfigInvalidNodeReturnsEmpty) {
  YAML::Node node;  // Default node is null/undefined

  SearchConfigData s;
  const auto parsed = parseYamlConfig(YAML::Node(), s);

  EXPECT_EQ(parsed.size(), 0);
}

TEST_F(ConfigGeneratorsTest, ParseYamlConfigRFPMarginArray) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  YAML::Node node;
  node["RFP_MARGIN"] = std::vector<int>{0, 250, 500, 1000};

  SearchConfigData s;
  parseYamlConfig(node, s);

  EXPECT_EQ(s.RFP_MARGIN[0], 0);
  EXPECT_EQ(s.RFP_MARGIN[1], 250);
  EXPECT_EQ(s.RFP_MARGIN[2], 500);
  EXPECT_EQ(s.RFP_MARGIN[3], 1000);
}

TEST_F(ConfigGeneratorsTest, ParseYamlConfigLMPMovesArray) {
#ifdef FRANKYCPP_PRODUCTION
  GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif

  YAML::Node node;
  YAML::Node arr;
  for (int i = 0; i < 16; ++i) {
    arr.push_back(i * 2);
  }
  node["LMP_MOVES"] = arr;

  SearchConfigData s;
  parseYamlConfig(node, s);

  for (int i = 0; i < 16; ++i) {
    EXPECT_EQ(s.LMP_MOVES[i], i * 2);
  }
}
