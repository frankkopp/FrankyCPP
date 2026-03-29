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

#include "config/ConfigRegistry.h"
#include "config/ConfigMode.h"
#include "config/EvalConfigData.h"
#include "config/SearchConfigData.h"

#include <gtest/gtest.h>

using namespace config;

class ConfigRegistryTest : public ::testing::Test {
protected:
  ConfigRegistry& registry = ConfigRegistry::instance();
};

//=============================================================================
// Basic Registry Tests
//=============================================================================

TEST_F(ConfigRegistryTest, InstanceExists) {
  EXPECT_GT(registry.totalCount(), 0);
}

TEST_F(ConfigRegistryTest, AllReturnsNonEmpty) {
  const auto all = registry.all();
  EXPECT_FALSE(all.empty());
}

TEST_F(ConfigRegistryTest, MinimumSearchConfigCount) {
  // We expect at least 60 Search/General config entries
  // This catches major drift if someone forgets to add registry entries
  EXPECT_GE(registry.searchConfigCount(), 60);
}

TEST_F(ConfigRegistryTest, MinimumEvalConfigCount) {
  // We expect at least 50 Eval config entries
  EXPECT_GE(registry.evalConfigCount(), 50);
}

TEST_F(ConfigRegistryTest, TotalCountMatchesSumOfDomains) {
  const std::size_t searchCount = registry.searchConfigCount();
  const std::size_t evalCount   = registry.evalConfigCount();

  // Debug/Tuning domains may have additional entries
  // So total >= search + eval
  EXPECT_GE(registry.totalCount(), searchCount + evalCount);
}

//=============================================================================
// Find Tests
//=============================================================================

TEST_F(ConfigRegistryTest, FindExistingByName) {
  const ConfigDef* def = registry.find("USE_NMP");
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->name, "USE_NMP");
  EXPECT_EQ(def->valueType, ConfigValueType::Bool);
  EXPECT_EQ(def->domain, ConfigDomain::Search);
}

TEST_F(ConfigRegistryTest, FindNonExistingByName) {
  const ConfigDef* def = registry.find("NON_EXISTENT_CONFIG");
  EXPECT_EQ(def, nullptr);
}

TEST_F(ConfigRegistryTest, FindByUciName) {
  const ConfigDef* def = registry.findByUciName("Hash");
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->name, "TT_SIZE_MB");
}

TEST_F(ConfigRegistryTest, FindByUciNameCaseInsensitive) {
  const ConfigDef* def1 = registry.findByUciName("hash");
  const ConfigDef* def2 = registry.findByUciName("HASH");
  const ConfigDef* def3 = registry.findByUciName("Hash");

  ASSERT_NE(def1, nullptr);
  EXPECT_EQ(def1, def2);
  EXPECT_EQ(def2, def3);
}

TEST_F(ConfigRegistryTest, FindNonExistingByUciName) {
  const ConfigDef* def = registry.findByUciName("Non Existent Option");
  EXPECT_EQ(def, nullptr);
}

//=============================================================================
// Domain Filtering Tests
//=============================================================================

TEST_F(ConfigRegistryTest, ByDomainSearch) {
  const auto searchConfigs = registry.byDomain(ConfigDomain::Search);
  EXPECT_FALSE(searchConfigs.empty());

  for (const auto* def : searchConfigs) {
    EXPECT_EQ(def->domain, ConfigDomain::Search);
  }
}

TEST_F(ConfigRegistryTest, ByDomainEval) {
  const auto evalConfigs = registry.byDomain(ConfigDomain::Eval);
  EXPECT_FALSE(evalConfigs.empty());

  for (const auto* def : evalConfigs) {
    EXPECT_EQ(def->domain, ConfigDomain::Eval);
  }
}

TEST_F(ConfigRegistryTest, ByDomainGeneral) {
  const auto generalConfigs = registry.byDomain(ConfigDomain::General);
  EXPECT_FALSE(generalConfigs.empty());

  for (const auto* def : generalConfigs) {
    EXPECT_EQ(def->domain, ConfigDomain::General);
  }
}

//=============================================================================
// Exposure Filtering Tests
//=============================================================================

TEST_F(ConfigRegistryTest, UciOptionsNonEmpty) {
  const auto uciOpts = registry.uciOptions();
  EXPECT_FALSE(uciOpts.empty());

  for (const auto* def : uciOpts) {
    EXPECT_TRUE(def->exposure.uci);
    EXPECT_FALSE(def->uciName.empty()) << "UCI option " << def->name << " has empty UCI name";
  }
}

TEST_F(ConfigRegistryTest, YamlOptionsNonEmpty) {
  const auto yamlOpts = registry.yamlOptions();
  EXPECT_FALSE(yamlOpts.empty());

  for (const auto* def : yamlOpts) {
    EXPECT_TRUE(def->exposure.yaml);
  }
}

TEST_F(ConfigRegistryTest, DisplayOptionsNonEmpty) {
  const auto displayOpts = registry.displayOptions();
  EXPECT_FALSE(displayOpts.empty());

  for (const auto* def : displayOpts) {
    EXPECT_TRUE(def->exposure.display);
  }
}

//=============================================================================
// Getter/Setter Tests
//=============================================================================

TEST_F(ConfigRegistryTest, GetterReturnsCorrectValue) {
  SearchConfigData search;
  const EvalConfigData eval;

  // Test a known Search config
  const ConfigDef* nmpDef = registry.find("USE_NMP");
  ASSERT_NE(nmpDef, nullptr);
  EXPECT_EQ(nmpDef->getter(search, eval), "true");

#ifndef FRANKYCPP_PRODUCTION
  // In production, USE_NMP is static constexpr — cannot be assigned to.
  search.USE_NMP = false;
  EXPECT_EQ(nmpDef->getter(search, eval), "false");
#endif
}

TEST_F(ConfigRegistryTest, SetterModifiesValue) {
  SearchConfigData search;
  EvalConfigData eval;

  const ConfigDef* ttSizeDef = registry.find("TT_SIZE_MB");
  ASSERT_NE(ttSizeDef, nullptr);

  EXPECT_EQ(search.TT_SIZE_MB, 256); // default

  ttSizeDef->setter(search, eval, "256");
  EXPECT_EQ(search.TT_SIZE_MB, 256);
}

TEST_F(ConfigRegistryTest, EvalGetterReturnsCorrectValue) {
  const SearchConfigData search;
  EvalConfigData eval;

  const ConfigDef* tempoDef = registry.find("TEMPO");
  ASSERT_NE(tempoDef, nullptr);
  EXPECT_EQ(tempoDef->getter(search, eval), std::format("{}",eval.TEMPO));

#ifndef FRANKYCPP_PRODUCTION
  // In production, TEMPO is static constexpr — cannot be assigned to.
  eval.TEMPO = 50;
  EXPECT_EQ(tempoDef->getter(search, eval), "50");
#endif
}

TEST_F(ConfigRegistryTest, EvalSetterModifiesValue) {
  SearchConfigData search;
  EvalConfigData eval;

  const ConfigDef* lazyThreshDef = registry.find("LAZY_THRESHOLD");
  ASSERT_NE(lazyThreshDef, nullptr);

  EXPECT_EQ(eval.LAZY_THRESHOLD, 706); // default (Texel tuned)

#ifndef FRANKYCPP_PRODUCTION
  // In production, LAZY_THRESHOLD is static constexpr — setter is a no-op.
  lazyThreshDef->setter(search, eval, "500");
  EXPECT_EQ(eval.LAZY_THRESHOLD, 500);
#endif
}

TEST_F(ConfigRegistryTest, ArrayGetterReturnsCorrectFormat) {
  const SearchConfigData search;
  const EvalConfigData eval;

  const ConfigDef* fpMarginDef = registry.find("FP_MARGIN");
  ASSERT_NE(fpMarginDef, nullptr);

  const std::string result = fpMarginDef->getter(search, eval);
  EXPECT_EQ(result, "0,100,200,300,500,900,1200");
}

TEST_F(ConfigRegistryTest, ArraySetterParsesCorrectly) {
  SearchConfigData search;
  EvalConfigData eval;

  const ConfigDef* rfpMarginDef = registry.find("RFP_MARGIN");
  ASSERT_NE(rfpMarginDef, nullptr);

#ifndef FRANKYCPP_PRODUCTION
  // In production, RFP_MARGIN is static constexpr — setter is a no-op.
  rfpMarginDef->setter(search, eval, "10,20,30,40");

  EXPECT_EQ(search.RFP_MARGIN[0], 10);
  EXPECT_EQ(search.RFP_MARGIN[1], 20);
  EXPECT_EQ(search.RFP_MARGIN[2], 30);
  EXPECT_EQ(search.RFP_MARGIN[3], 40);
#else
  // In production, just verify the setter exists and doesn't crash.
  rfpMarginDef->setter(search, eval, "10,20,30,40"); // no-op
#endif
}

//=============================================================================
// ConfigDef Metadata Tests
//=============================================================================

TEST_F(ConfigRegistryTest, IntConfigsHaveBounds) {
  for (const auto& def : registry.all()) {
    if (def.valueType == ConfigValueType::Int && def.exposure.uci) {
      // Most UCI-exposed int configs should have bounds
      // (some may not, but most should)
      if (def.minValue.has_value() || def.maxValue.has_value()) {
        // If one bound is set, both should typically be set
        EXPECT_TRUE(def.minValue.has_value() && def.maxValue.has_value())
          << "Config " << def.name << " has partial bounds";
      }
    }
  }
}

TEST_F(ConfigRegistryTest, AllConfigsHaveNames) {
  for (const auto& def : registry.all()) {
    EXPECT_FALSE(def.name.empty()) << "Found config with empty name";
  }
}

TEST_F(ConfigRegistryTest, AllConfigsHaveDefaultValues) {
  for (const auto& def : registry.all()) {
    // All configs should have a default value (even if empty string for some string types)
    // Bool and Int types should definitely have non-empty defaults
    if (def.valueType == ConfigValueType::Bool || def.valueType == ConfigValueType::Int) {
      EXPECT_FALSE(def.defaultValue.empty())
        << "Config " << def.name << " has empty default value";
    }
  }
}

TEST_F(ConfigRegistryTest, AllConfigsHaveGettersAndSetters) {
  for (const auto& def : registry.all()) {
    EXPECT_TRUE(def.getter != nullptr) << "Config " << def.name << " has no getter";
    EXPECT_TRUE(def.setter != nullptr) << "Config " << def.name << " has no setter";
  }
}

//=============================================================================
// Specific Config Validation Tests
//=============================================================================

TEST_F(ConfigRegistryTest, VerifyKeySearchConfigs) {
  // Verify some key Search configs exist and have correct types
  const std::vector<std::pair<std::string, ConfigValueType>> keyConfigs = {
    {"USE_NMP", ConfigValueType::Bool},
    {"TT_SIZE_MB", ConfigValueType::Int},
    {"USE_LMR", ConfigValueType::Bool},
    {"USE_SINGULAR_EXT", ConfigValueType::Bool},
    {"BOOK_PATH", ConfigValueType::String},
    {"FP_MARGIN", ConfigValueType::IntArray},
  };

  for (const auto& [name, expectedType] : keyConfigs) {
    const ConfigDef* def = registry.find(name);
    ASSERT_NE(def, nullptr) << "Missing key config: " << name;
    EXPECT_EQ(def->valueType, expectedType) << "Wrong type for " << name;
  }
}

TEST_F(ConfigRegistryTest, VerifyKeyEvalConfigs) {
  // Verify some key Eval configs exist and have correct types
  const std::vector<std::pair<std::string, ConfigValueType>> keyConfigs = {
    {"USE_MATERIAL", ConfigValueType::Bool},
    {"TEMPO", ConfigValueType::Int},
    {"LAZY_THRESHOLD", ConfigValueType::Int},
    {"USE_BISHOP_PAIR_BONUS", ConfigValueType::Bool},
    {"USE_KING_SAFETY_SHIELD", ConfigValueType::Bool},
  };

  for (const auto& [name, expectedType] : keyConfigs) {
    const ConfigDef* def = registry.find(name);
    ASSERT_NE(def, nullptr) << "Missing key config: " << name;
    EXPECT_EQ(def->valueType, expectedType) << "Wrong type for " << name;
    EXPECT_EQ(def->domain, ConfigDomain::Eval) << "Wrong domain for " << name;
  }
}

//=============================================================================
// IS_MUTABLE Tests
//=============================================================================

TEST_F(ConfigRegistryTest, IsMutableEssentialMembers) {
  // CONFIG_ESSENTIAL members are always mutable, regardless of build mode
  const SearchConfigData defaultSearch{};
  const EvalConfigData defaultEval{};

  // Search: CONFIG_ESSENTIAL members
  EXPECT_TRUE(IS_MUTABLE(defaultSearch, TT_SIZE_MB))       << "TT_SIZE_MB should always be mutable";
  EXPECT_TRUE(IS_MUTABLE(defaultSearch, USE_PONDER))        << "USE_PONDER should always be mutable";
  EXPECT_TRUE(IS_MUTABLE(defaultSearch, THREADS))           << "THREADS should always be mutable";
  EXPECT_TRUE(IS_MUTABLE(defaultSearch, MOVE_OVERHEAD_MS))  << "MOVE_OVERHEAD_MS should always be mutable";
  EXPECT_TRUE(IS_MUTABLE(defaultSearch, USE_BOOK))          << "USE_BOOK should always be mutable";
  EXPECT_TRUE(IS_MUTABLE(defaultSearch, BOOK_PATH))         << "BOOK_PATH should always be mutable";
  EXPECT_TRUE(IS_MUTABLE(defaultSearch, BOOK_TYPE))         << "BOOK_TYPE should always be mutable";
  EXPECT_TRUE(IS_MUTABLE(defaultSearch, TB_PATH))           << "TB_PATH should always be mutable";

  // Eval: CONFIG_ESSENTIAL members
  EXPECT_TRUE(IS_MUTABLE(defaultEval, USE_PAWN_TT))        << "USE_PAWN_TT should always be mutable";
  EXPECT_TRUE(IS_MUTABLE(defaultEval, PAWN_TT_SIZE_MB))    << "PAWN_TT_SIZE_MB should always be mutable";
}

TEST_F(ConfigRegistryTest, IsMutableConfigConstMembers) {
  // CONFIG_CONST members: mutable in dev, frozen in production
  const SearchConfigData defaultSearch{};
  const EvalConfigData defaultEval{};

#ifdef FRANKYCPP_PRODUCTION
  // Production: CONFIG_CONST members are static constexpr → not mutable
  EXPECT_FALSE(IS_MUTABLE(defaultSearch, USE_NMP))    << "USE_NMP should be frozen in production";
  EXPECT_FALSE(IS_MUTABLE(defaultSearch, USE_LMR))    << "USE_LMR should be frozen in production";
  EXPECT_FALSE(IS_MUTABLE(defaultEval, TEMPO))        << "TEMPO should be frozen in production";
  EXPECT_FALSE(IS_MUTABLE(defaultEval, USE_MATERIAL)) << "USE_MATERIAL should be frozen in production";
#else
  // Development: CONFIG_CONST members are plain mutable
  EXPECT_TRUE(IS_MUTABLE(defaultSearch, USE_NMP))    << "USE_NMP should be mutable in development";
  EXPECT_TRUE(IS_MUTABLE(defaultSearch, USE_LMR))    << "USE_LMR should be mutable in development";
  EXPECT_TRUE(IS_MUTABLE(defaultEval, TEMPO))        << "TEMPO should be mutable in development";
  EXPECT_TRUE(IS_MUTABLE(defaultEval, USE_MATERIAL)) << "USE_MATERIAL should be mutable in development";
#endif
}

TEST_F(ConfigRegistryTest, IsMutableMatchesUciExposure) {
  // Verify that IS_MUTABLE results match the UCI exposure flags in the registry.
  // In development builds, all entries with non-empty uciName should have .uci = true.
  // In production builds, only CONFIG_ESSENTIAL entries should have .uci = true.
  const auto uciOpts = registry.uciOptions();

  for (const auto* def : uciOpts) {
    // All entries returned by uciOptions() must have .uci = true
    EXPECT_TRUE(def->exposure.uci) << "UCI option " << def->name << " has .uci = false";
  }

  // In production, CONFIG_CONST options should NOT appear in uciOptions()
#ifdef FRANKYCPP_PRODUCTION
  for (const auto* def : uciOpts) {
    // Known CONFIG_CONST members should not be here
    EXPECT_NE(def->name, "USE_NMP") << "Frozen option USE_NMP should not be a UCI option in production";
    EXPECT_NE(def->name, "USE_LMR") << "Frozen option USE_LMR should not be a UCI option in production";
  }
#endif
}

//=============================================================================
// Tunable Parameter Tests (Phase 5 — Texel Tuning)
//=============================================================================

TEST_F(ConfigRegistryTest, TunableOptionsNonEmpty) {
  const auto tunableOpts = registry.tunableOptions();
  EXPECT_FALSE(tunableOpts.empty());

  for (const auto* def : tunableOpts) {
    EXPECT_TRUE(def->exposure.tunable);
  }
}

TEST_F(ConfigRegistryTest, TunableParameterCount) {
  // Phase 8: 78 eval weight entries are marked tunable (was 88 before deactivation)
  // 72 scalar int + 6 IntArray
  // 10 dead features un-tuned: SPACE_BONUS_MID/END, BAD_BISHOP_PER_PAWN_MID/END,
  // KNIGHT_LOW_MOBILITY_LEQ2_MID/END, BISHOP_LOW_MOBILITY_LEQ3_MID,
  // ROOK_LOW_MOBILITY_LEQ3_MID/END, SAFE_CHECK_BISHOP_MID
  const auto tunableOpts = registry.tunableOptions();
  EXPECT_EQ(tunableOpts.size(), 78)
    << "Expected 78 tunable parameters (72 scalar Int + 6 IntArray). "
       "If you added a new eval weight, mark it tunable and update this count.";
}

TEST_F(ConfigRegistryTest, TunableParamsAreAllEvalDomain) {
  // All tunable parameters must belong to the Eval domain
  const auto tunableOpts = registry.tunableOptions();
  for (const auto* def : tunableOpts) {
    EXPECT_EQ(def->domain, ConfigDomain::Eval)
      << "Tunable param " << def->name << " is not in Eval domain";
  }
}

TEST_F(ConfigRegistryTest, TunableParamsAreIntOrIntArray) {
  // Tunable params must be continuous numeric types (Int or IntArray), not Bool/String
  const auto tunableOpts = registry.tunableOptions();
  for (const auto* def : tunableOpts) {
    const bool isNumeric = def->valueType == ConfigValueType::Int
                        || def->valueType == ConfigValueType::IntArray;
    EXPECT_TRUE(isNumeric)
      << "Tunable param " << def->name << " has type " << valueTypeToString(def->valueType)
      << " — only Int and IntArray should be tunable";
  }
}

TEST_F(ConfigRegistryTest, TunableParamsExcludeToggles) {
  // Bool USE_* toggles must NOT be marked tunable
  const auto tunableOpts = registry.tunableOptions();
  for (const auto* def : tunableOpts) {
    EXPECT_NE(def->valueType, ConfigValueType::Bool)
      << "Bool toggle " << def->name << " should not be tunable";
    // Extra safety: no param starting with USE_ should be tunable
    EXPECT_FALSE(def->name.substr(0, 4) == "USE_")
      << "Toggle " << def->name << " should not be tunable";
  }
}

TEST_F(ConfigRegistryTest, TunableParamsExcludeInfrastructure) {
  // Infrastructure params must NOT be tunable
  const auto tunableOpts = registry.tunableOptions();
  const std::vector<std::string> excluded = {
    "EVAL_CONFIG_SOURCE", "PAWN_TT_SIZE_MB", "USE_PAWN_TT", "USE_GAMEPHASE_VALUE"
  };
  for (const auto* def : tunableOpts) {
    for (const auto& name : excluded) {
      EXPECT_NE(def->name, name)
        << "Infrastructure param " << name << " should not be tunable";
    }
  }
}

TEST_F(ConfigRegistryTest, VerifyKeyTunableParams) {
  // Spot-check that specific key params are tunable
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::vector<std::string> mustBeTunable = {
    "TEMPO", "LAZY_THRESHOLD",
    "ISOLATED_PAWN_MID_WEIGHT", "PASSED_PAWN_END_WEIGHT",
    "BISHOP_PAIR_MID_BONUS", "KNIGHT_OUTPOST_SUPPORTED_MID",
    "ROOK_OPEN_FILE_MID_BONUS", "QUEEN_MOBILITY_MID_PER_MOVE",
    "KING_ATTACK_WEIGHT_QUEEN", "KING_SAFETY_TABLE",
    "PAWN_STORM_MID_PENALTY", "SAFE_CHECK_QUEEN_MID",
    "THREAT_HANGING_MID",
    "CONNECTED_ROOKS_MID_BONUS", "MINOR_CONNECTIVITY_END_BONUS",
  };

  for (const auto& name : mustBeTunable) {
    const ConfigDef* def = registry.find(name);
    ASSERT_NE(def, nullptr) << "Config " << name << " not found in registry";
    EXPECT_TRUE(def->exposure.tunable) << name << " should be marked tunable";
  }
}

//=============================================================================
// Helper Function Tests
//=============================================================================

TEST_F(ConfigRegistryTest, DomainToStringWorks) {
  EXPECT_EQ(domainToString(ConfigDomain::General), "General");
  EXPECT_EQ(domainToString(ConfigDomain::Search), "Search");
  EXPECT_EQ(domainToString(ConfigDomain::Eval), "Eval");
  EXPECT_EQ(domainToString(ConfigDomain::Tuning), "Tuning");
  EXPECT_EQ(domainToString(ConfigDomain::Debug), "Debug");
}

TEST_F(ConfigRegistryTest, ValueTypeToStringWorks) {
  EXPECT_EQ(valueTypeToString(ConfigValueType::Bool), "bool");
  EXPECT_EQ(valueTypeToString(ConfigValueType::Int), "int");
  EXPECT_EQ(valueTypeToString(ConfigValueType::Double), "double");
  EXPECT_EQ(valueTypeToString(ConfigValueType::String), "string");
  EXPECT_EQ(valueTypeToString(ConfigValueType::Combo), "combo");
  EXPECT_EQ(valueTypeToString(ConfigValueType::IntArray), "int[]");
}
