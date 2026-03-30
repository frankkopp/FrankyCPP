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

#include "tuning/optimizer/TuningParameter.h"

#include "config/ConfigDef.h"
#include "tuning/optimizer/TuningEntry.h"
#include "config/ConfigManager.h"
#include "config/ConfigRegistry.h"
#include "config/EvalConfigData.h"
#include "config/SearchConfigData.h"
#include "init.h"
#include "types/macros.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace tuning;
using namespace config;

class TuningParameterTest : public testing::Test {
public:
  static void SetUpTestSuite() {
    NEWLINE;
    init::init();
    NEWLINE;
  }

protected:
  void SetUp() override {
    // Reset config to defaults before each test
    ConfigManager::instance().resetToDefaults();
  }
};

// =========================================================================
// Basic construction tests
// =========================================================================

TEST_F(TuningParameterTest, buildFromRegistry_notEmpty) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  EXPECT_FALSE(params.empty());
  std::cout << "Total tunable parameters (expanded): " << params.size() << "\n";
}

TEST_F(TuningParameterTest, countTunableValues_matchesBuild) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);
  const auto count   = TuningParameter::countTunableValues();

  EXPECT_EQ(params.size(), count);
}

TEST_F(TuningParameterTest, expandedCount_greaterThanRegistryCount) {
  // IntArray entries expand to multiple params, so expanded count > registry tunable count
  const auto& registry      = ConfigRegistry::instance();
  const auto tunableCount   = registry.tunableOptions().size();
  const auto expandedCount  = TuningParameter::countTunableValues();

  EXPECT_GT(expandedCount, tunableCount)
    << "Expected expanded params > registry tunable count (arrays expand to elements)";

  std::cout << "Registry tunable entries: " << tunableCount << "\n";
  std::cout << "Expanded tunable params:  " << expandedCount << "\n";
}

// =========================================================================
// Scalar parameter tests
// =========================================================================

TEST_F(TuningParameterTest, scalarParams_haveCorrectValues) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  // Spot-check a few known scalar params
  const auto it = std::ranges::find_if(params, [](const auto& p) {
    return p.name == "TEMPO";
  });
  ASSERT_NE(it, params.end()) << "TEMPO parameter not found";
  EXPECT_EQ(it->currentValue, eval.TEMPO);
  EXPECT_EQ(it->originalValue, eval.TEMPO);
  EXPECT_EQ(it->arrayIndex, -1);
  EXPECT_EQ(it->arraySize, 0);
  EXPECT_EQ(it->configName, "TEMPO");
}

TEST_F(TuningParameterTest, scalarParams_boundsFromConfigDef) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  // ISOLATED_PAWN_MID_WEIGHT has minValue=-100, maxValue=0 in ConfigDef
  const auto it = std::ranges::find_if(params, [](const auto& p) {
    return p.name == "ISOLATED_PAWN_MID_WEIGHT";
  });
  ASSERT_NE(it, params.end());
  EXPECT_LE(it->minValue, it->currentValue);
  EXPECT_GE(it->maxValue, it->currentValue);
  // The ConfigDef has explicit bounds: -100 to 0
  EXPECT_EQ(it->minValue, -100);
  EXPECT_EQ(it->maxValue, 0);
}

TEST_F(TuningParameterTest, scalarParams_allNamesUnique) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  std::set<std::string> names;
  for (const auto& p : params) {
    const auto [_, inserted] = names.insert(p.name);
    EXPECT_TRUE(inserted) << "Duplicate parameter name: " << p.name;
  }
}

TEST_F(TuningParameterTest, scalarParams_noArrayFields) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  for (const auto& p : params) {
    if (p.arrayIndex == -1) {
      EXPECT_EQ(p.arraySize, 0) << "Scalar " << p.name << " should have arraySize=0";
      EXPECT_EQ(p.monotonicity, MonotonicityConstraint::NONE)
        << "Scalar " << p.name << " should have no monotonicity constraint";
    }
  }
}

// =========================================================================
// Array parameter tests
// =========================================================================

TEST_F(TuningParameterTest, arrayParams_expandedCorrectly) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  // KING_SAFETY_TABLE has 16 elements → 16 TuningParameters
  int kstCount = 0;
  for (const auto& p : params) {
    if (p.configName == "KING_SAFETY_TABLE") {
      kstCount++;
      EXPECT_GE(p.arrayIndex, 0);
      EXPECT_LT(p.arrayIndex, 16);
      EXPECT_EQ(p.arraySize, 16);
      // Name should be like "KING_SAFETY_TABLE[0]"
      EXPECT_TRUE(p.name.starts_with("KING_SAFETY_TABLE["))
        << "Unexpected name: " << p.name;
    }
  }
  EXPECT_EQ(kstCount, 16) << "Expected 16 KING_SAFETY_TABLE elements";
}

TEST_F(TuningParameterTest, arrayParams_valuesMatchConfig) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  // Check PASSED_PAWN_RANK_MID_BONUS values match config
  for (const auto& p : params) {
    if (p.configName == "PASSED_PAWN_RANK_MID_BONUS" && p.arrayIndex >= 0) {
      EXPECT_EQ(p.currentValue, eval.PASSED_PAWN_RANK_MID_BONUS[p.arrayIndex])
        << "Mismatch at index " << p.arrayIndex;
      EXPECT_EQ(p.originalValue, p.currentValue);
    }
  }
}

TEST_F(TuningParameterTest, arrayParams_indicesContiguous) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  // Collect all PAWN_ADVANCE_MID_BONUS indices
  std::vector<int> indices;
  for (const auto& p : params) {
    if (p.configName == "PAWN_ADVANCE_MID_BONUS") {
      indices.push_back(p.arrayIndex);
    }
  }
  ASSERT_EQ(indices.size(), 4u) << "Expected 4 PAWN_ADVANCE_MID_BONUS elements";

  // Should be 0, 1, 2, 3
  std::ranges::sort(indices);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(indices[i], i);
  }
}

// =========================================================================
// Monotonicity constraint tests
// =========================================================================

TEST_F(TuningParameterTest, monotonicity_assignedToKnownArrays) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  const std::unordered_set<std::string> expectedNonDecreasing = {
    "KING_SAFETY_TABLE",
    "PASSED_PAWN_RANK_MID_BONUS",
    "PASSED_PAWN_RANK_END_BONUS",
    "PAWN_ADVANCE_MID_BONUS",
    "PAWN_ADVANCE_END_BONUS",
    "PAWN_STORM_MID_PENALTY",
  };

  for (const auto& p : params) {
    if (p.arrayIndex >= 0 && expectedNonDecreasing.contains(p.configName)) {
      EXPECT_EQ(p.monotonicity, MonotonicityConstraint::NON_DECREASING)
        << p.name << " should be NON_DECREASING";
    }
  }
}

TEST_F(TuningParameterTest, monotonicity_notOnScalars) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  for (const auto& p : params) {
    if (p.arrayIndex < 0) {
      EXPECT_EQ(p.monotonicity, MonotonicityConstraint::NONE)
        << "Scalar " << p.name << " should not have a monotonicity constraint";
    }
  }
}

// =========================================================================
// Parameter group tests
// =========================================================================

TEST_F(TuningParameterTest, paramGroups_allWithinRange) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  for (const auto& p : params) {
    EXPECT_GE(p.paramGroup, 0) << p.name << " has negative paramGroup";
    EXPECT_LT(p.paramGroup, NUM_PARAM_GROUPS) << p.name << " exceeds NUM_PARAM_GROUPS";
  }
}

TEST_F(TuningParameterTest, paramGroups_knownAssignments) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  const std::unordered_map<std::string, int> expectedGroups = {
    {"TEMPO",                     0},
    {"LAZY_THRESHOLD",            0},
    {"ISOLATED_PAWN_MID_WEIGHT",  1},
    {"PASSED_PAWN_MID_WEIGHT",    2},
    {"PAWN_ADVANCE_MID_BONUS",    3},
    {"BISHOP_PAIR_MID_BONUS",     4},
    {"KNIGHT_MOBILITY_MID_PER_MOVE", 5},
    {"BISHOP_MOBILITY_MID_PER_MOVE", 6},
    {"ROOK_MOBILITY_MID_PER_MOVE",   7},
    {"QUEEN_MOBILITY_MID_PER_MOVE",  8},
    {"KING_SHIELD_MID_PER_PAWN",     9},
    {"THREAT_BY_PAWN_MINOR_MID",    10},
    {"SPACE_BONUS_MID",             11},
    {"CONNECTED_ROOKS_MID_BONUS",   12},
  };

  for (const auto& p : params) {
    if (const auto it = expectedGroups.find(p.configName); it != expectedGroups.end()) {
      EXPECT_EQ(p.paramGroup, it->second)
        << p.name << " expected group " << it->second << " but got " << p.paramGroup;
    }
  }
}

TEST_F(TuningParameterTest, paramGroups_arrayElementsSameGroup) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  // All elements of the same array should have the same group
  std::unordered_map<std::string, int> arrayGroups;
  for (const auto& p : params) {
    if (p.arrayIndex >= 0) {
      if (const auto it = arrayGroups.find(p.configName); it != arrayGroups.end()) {
        EXPECT_EQ(p.paramGroup, it->second)
          << "Array " << p.configName << " element " << p.arrayIndex
          << " has different group than earlier element";
      }
      else {
        arrayGroups[p.configName] = p.paramGroup;
      }
    }
  }
}

// =========================================================================
// Read/write round-trip tests
// =========================================================================

TEST_F(TuningParameterTest, applyToConfig_scalarRoundTrip) {
  // Modify a scalar parameter and verify it's written back correctly
  CONFIG_OVERRIDE_START()
    e.TEMPO = 34; // reset to known value
  CONFIG_OVERRIDE_END();

  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  auto params        = TuningParameter::buildFromRegistry(search, eval);

  auto it = std::ranges::find_if(params, [](const auto& p) { return p.name == "TEMPO"; });
  ASSERT_NE(it, params.end());
  EXPECT_EQ(it->currentValue, 34);

  // Modify and apply
  it->currentValue = 42;
  ConfigManager::instance().applyOverrides([&](auto& s, auto& e) {
    it->applyToConfig(s, e);
  });

  EXPECT_EQ(ConfigManager::instance().eval().TEMPO, 42);

  // Read back
  it->currentValue = 0; // reset local
  it->readFromConfig(ConfigManager::instance().search(), ConfigManager::instance().eval());
  EXPECT_EQ(it->currentValue, 42);
}

TEST_F(TuningParameterTest, applyToConfig_arrayElementRoundTrip) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  auto params        = TuningParameter::buildFromRegistry(search, eval);

  // Find KING_SAFETY_TABLE[5]
  auto it = std::ranges::find_if(params, [](const auto& p) {
    return p.configName == "KING_SAFETY_TABLE" && p.arrayIndex == 5;
  });
  ASSERT_NE(it, params.end());

  const int originalVal = it->currentValue;
  EXPECT_EQ(originalVal, eval.KING_SAFETY_TABLE[5]);

  // Modify element 5 and apply
  it->currentValue = 999;
  ConfigManager::instance().applyOverrides([&](auto& s, auto& e) {
    it->applyToConfig(s, e);
  });

  // Verify only element 5 changed, others preserved
  EXPECT_EQ(ConfigManager::instance().eval().KING_SAFETY_TABLE[5], 999);
  EXPECT_EQ(ConfigManager::instance().eval().KING_SAFETY_TABLE[4], eval.KING_SAFETY_TABLE[4]);
  EXPECT_EQ(ConfigManager::instance().eval().KING_SAFETY_TABLE[6], eval.KING_SAFETY_TABLE[6]);

  // Read back
  it->currentValue = 0;
  it->readFromConfig(ConfigManager::instance().search(), ConfigManager::instance().eval());
  EXPECT_EQ(it->currentValue, 999);
}

TEST_F(TuningParameterTest, readFromConfig_matchesDirectAccess) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  auto params        = TuningParameter::buildFromRegistry(search, eval);

  // Verify all params read correctly
  for (auto& p : params) {
    const int builtValue = p.currentValue;
    p.currentValue       = -9999; // scramble
    p.readFromConfig(search, eval);
    EXPECT_EQ(p.currentValue, builtValue)
      << "readFromConfig mismatch for " << p.name;
  }
}

// =========================================================================
// Bounds validation tests
// =========================================================================

TEST_F(TuningParameterTest, bounds_currentValueWithinRange) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  for (const auto& p : params) {
    EXPECT_LE(p.minValue, p.currentValue)
      << p.name << ": minValue=" << p.minValue << " > currentValue=" << p.currentValue;
    EXPECT_GE(p.maxValue, p.currentValue)
      << p.name << ": maxValue=" << p.maxValue << " < currentValue=" << p.currentValue;
  }
}

TEST_F(TuningParameterTest, bounds_minLessThanOrEqualMax) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  for (const auto& p : params) {
    EXPECT_LE(p.minValue, p.maxValue)
      << p.name << ": minValue=" << p.minValue << " > maxValue=" << p.maxValue;
  }
}

TEST_F(TuningParameterTest, delta_alwaysPositive) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  for (const auto& p : params) {
    EXPECT_GT(p.delta, 0) << p.name << " should have positive delta";
  }
}

// =========================================================================
// Summary / diagnostic test
// =========================================================================

TEST_F(TuningParameterTest, printSummary) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  // Count by type
  int scalars = 0, arrayElements = 0;
  std::unordered_map<int, int> groupCounts;
  for (const auto& p : params) {
    if (p.arrayIndex < 0) scalars++;
    else arrayElements++;
    groupCounts[p.paramGroup]++;
  }

  std::cout << "\n=== TuningParameter Summary ===\n";
  std::cout << "Total parameters: " << params.size() << "\n";
  std::cout << "  Scalars:        " << scalars << "\n";
  std::cout << "  Array elements: " << arrayElements << "\n";
  std::cout << "  Groups used:    " << groupCounts.size() << "\n";
  for (const auto& [group, count] : groupCounts) {
    std::cout << "    Group " << group << ": " << count << " params\n";
  }
  std::cout << "===============================\n";

  // Sanity: we expect roughly 82 scalars + ~36 array elements ≈ ~118 total
  EXPECT_GE(params.size(), 100u) << "Expected at least 100 expanded parameters";
}

// =========================================================================
// Phase 6.10 — Additional edge case tests
// =========================================================================

TEST_F(TuningParameterTest, applyToConfig_ClampsToMaxBounds) {
  // When currentValue exceeds maxValue, applyToConfig should still set the value
  // (clamping is the tuner's job, not the parameter's — but test the behavior)
  CONFIG_OVERRIDE_START()
    e.TEMPO = 34;
  CONFIG_OVERRIDE_END();

  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  auto params        = TuningParameter::buildFromRegistry(search, eval);

  auto it = std::ranges::find_if(params, [](const auto& p) { return p.name == "TEMPO"; });
  ASSERT_NE(it, params.end());

  // Set value beyond maxValue and apply — should still write
  it->currentValue = it->maxValue + 100;
  ConfigManager::instance().applyOverrides([&](auto& s, auto& e) {
    it->applyToConfig(s, e);
  });

  // Value is written as-is (no clamping in applyToConfig itself)
  EXPECT_EQ(ConfigManager::instance().eval().TEMPO, it->maxValue + 100);
}

TEST_F(TuningParameterTest, applyToConfig_NegativeBeyondMin) {
  CONFIG_OVERRIDE_START()
    e.ISOLATED_PAWN_MID_WEIGHT = -10;
  CONFIG_OVERRIDE_END();

  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  auto params        = TuningParameter::buildFromRegistry(search, eval);

  auto it = std::ranges::find_if(params, [](const auto& p) {
    return p.name == "ISOLATED_PAWN_MID_WEIGHT";
  });
  ASSERT_NE(it, params.end());

  // Set value below minValue
  it->currentValue = it->minValue - 50;
  ConfigManager::instance().applyOverrides([&](auto& s, auto& e) {
    it->applyToConfig(s, e);
  });

  EXPECT_EQ(ConfigManager::instance().eval().ISOLATED_PAWN_MID_WEIGHT, it->minValue - 50);
}

TEST_F(TuningParameterTest, delta_defaultValueIsOne) {
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  // All params should have delta=1 (default for coordinate descent)
  for (const auto& p : params) {
    EXPECT_EQ(p.delta, 1) << p.name << " should have delta=1";
  }
}

TEST_F(TuningParameterTest, expandedCount_pinnedValue) {
  // Pin the expanded count to catch unintended changes when adding new config entries.
  // This should be updated when new tunable params are added.
  const auto expandedCount = TuningParameter::countTunableValues();

  // As of Phase 8 retune: 75 registry entries expand to ~109 individual params
  // (69 scalar Int + 6 IntArray: KING_SAFETY_TABLE[16] + PASSED_PAWN_RANK_MID_BONUS[6] +
  //  PASSED_PAWN_RANK_END_BONUS[6] + PAWN_ADVANCE_MID_BONUS[4] + PAWN_ADVANCE_END_BONUS[4] +
  //  PAWN_STORM_MID_PENALTY[4])
  // 69 + 16 + 6 + 6 + 4 + 4 + 4 = 109
  std::cout << "Pinned expanded tunable count: " << expandedCount << "\n";
  EXPECT_EQ(expandedCount, 109U)
    << "If you added new tunable params, update this pinned count";
}

TEST_F(TuningParameterTest, arrayParams_singleElementArray) {
  // If an array has a single element, it should still be treated as an array param
  // with arrayIndex=0 and arraySize=1. Currently no such param exists in the registry,
  // but verify the general contract: all array params have valid index/size.
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  for (const auto& p : params) {
    if (p.arrayIndex >= 0) {
      EXPECT_GE(p.arraySize, 1) << p.name << " array should have size >= 1";
      EXPECT_LT(p.arrayIndex, p.arraySize) << p.name << " index should be < size";
    }
  }
}

TEST_F(TuningParameterTest, originalValue_matchesDefault) {
  // After building from a fresh (default) config, originalValue should match currentValue
  const auto& search = ConfigManager::instance().search();
  const auto& eval   = ConfigManager::instance().eval();
  const auto params  = TuningParameter::buildFromRegistry(search, eval);

  for (const auto& p : params) {
    EXPECT_EQ(p.originalValue, p.currentValue)
      << p.name << " originalValue should match currentValue on fresh build";
  }
}
