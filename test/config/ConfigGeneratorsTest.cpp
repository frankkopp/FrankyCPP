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

TEST_F(ConfigGeneratorsTest, SearchConfigStrReflectsModifiedValues) {
  search.TT_SIZE_MB = 512;
  search.USE_NMP    = false;

  const std::string output = search.str();

  EXPECT_NE(output.find("TT_SIZE_MB: 512"), std::string::npos);
  EXPECT_NE(output.find("USE_NMP: false"), std::string::npos);
}

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

TEST_F(ConfigGeneratorsTest, EvalConfigStrReflectsModifiedValues) {
  eval.TEMPO           = 50;
  eval.LAZY_THRESHOLD  = 500;

  const std::string output = eval.str();

  EXPECT_NE(output.find("TEMPO: 50"), std::string::npos);
  EXPECT_NE(output.find("LAZY_THRESHOLD: 500"), std::string::npos);
}

//=============================================================================
// Array Value Tests
//=============================================================================

TEST_F(ConfigGeneratorsTest, ArrayValuesFormattedCorrectly) {
  const std::string output = search.str();

  // Check FP_MARGIN array is present and formatted as comma-separated
  EXPECT_NE(output.find("FP_MARGIN: 0,100,200,300,500,900,1200"), std::string::npos);
}
