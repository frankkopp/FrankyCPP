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
  EvalConfigData eval;

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

  EXPECT_EQ(search.TT_SIZE_MB, 64);// default

  ttSizeDef->setter(search, eval, "256");
  EXPECT_EQ(search.TT_SIZE_MB, 256);
}

TEST_F(ConfigRegistryTest, EvalGetterReturnsCorrectValue) {
  SearchConfigData search;
  EvalConfigData eval;

  const ConfigDef* tempoDef = registry.find("TEMPO");
  ASSERT_NE(tempoDef, nullptr);
  EXPECT_EQ(tempoDef->getter(search, eval), "34");

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

  EXPECT_EQ(eval.LAZY_THRESHOLD, 700);// default

#ifndef FRANKYCPP_PRODUCTION
  // In production, LAZY_THRESHOLD is static constexpr — setter is a no-op.
  lazyThreshDef->setter(search, eval, "500");
  EXPECT_EQ(eval.LAZY_THRESHOLD, 500);
#endif
}

TEST_F(ConfigRegistryTest, ArrayGetterReturnsCorrectFormat) {
  SearchConfigData search;
  EvalConfigData eval;

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
  rfpMarginDef->setter(search, eval, "10,20,30,40");// no-op
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
