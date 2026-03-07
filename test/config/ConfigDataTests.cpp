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

#include <gtest/gtest.h>
#include <string>
#include <yaml-cpp/yaml.h>

#include "config/EvalConfigData.h"
#include "config/SearchConfigData.h"

using namespace config;


namespace {

  // Verifies baseline defaults for core SearchConfigData fields match the inline defaults.
  TEST(SearchConfigDataTests, DefaultsAreSet) {
    const SearchConfigData c;
    EXPECT_EQ(c.MOVE_OVERHEAD_MS, 10);
    EXPECT_TRUE(c.USE_TT);
    EXPECT_EQ(c.TT_SIZE_MB, 256);
    EXPECT_TRUE(c.USE_LMR);
    EXPECT_EQ(c.LMR_MIN_DEPTH, 2);
  }

  // Ensures partial YAML overrides are applied while unspecified fields keep defaults;
  // also verifies short sequences override only the prefix of fixed-size arrays.
  TEST(SearchConfigDataTests, PartialYamlOverridesAndArrays) {
    YAML::Node n;
    n["MOVE_OVERHEAD_MS"] = 25;
    n["TT_SIZE_MB"]       = 128;

#ifndef FRANKYCPP_PRODUCTION
    n["USE_TT"] = false;
    YAML::Node rfp;
    rfp.push_back(1);
    rfp.push_back(2);
    n["RFP_MARGIN"] = rfp;
#endif

    const auto c = n.as<SearchConfigData>();
    EXPECT_EQ(c.MOVE_OVERHEAD_MS, 25);
    EXPECT_EQ(c.TT_SIZE_MB, 128);

#ifndef FRANKYCPP_PRODUCTION
    EXPECT_FALSE(c.USE_TT);
    // array shorter than default -> first two overridden, rest unchanged
    EXPECT_EQ(c.RFP_MARGIN[0], 1);
    EXPECT_EQ(c.RFP_MARGIN[1], 2);
    EXPECT_EQ(c.RFP_MARGIN[2], 400);
    EXPECT_EQ(c.RFP_MARGIN[3], 800);
#endif
  }

  // Sanity-checks that str() returns a human-readable string containing key fields.
  TEST(SearchConfigDataTests, StrContainsExpectedFields) {
    SearchConfigData c;
    const auto s = c.str();
    EXPECT_NE(s.find("MOVE_OVERHEAD_MS:"), std::string::npos);
    EXPECT_NE(s.find("TT_SIZE_MB:"), std::string::npos);
  }

  // Verifies baseline defaults for core EvalConfigData fields match the inline defaults.
  TEST(EvalConfigDataTests, DefaultsAreSet) {
    const EvalConfigData c;
    EXPECT_TRUE(c.USE_MATERIAL);
    EXPECT_TRUE(c.USE_POSITIONAL);
    EXPECT_TRUE(c.USE_TEMPO);
    EXPECT_EQ(c.TEMPO, 34);
  }

  // Ensures partial YAML overrides work and unknown keys do not cause failures (lenient parsing).
  TEST(EvalConfigDataTests, PartialYamlOverridesAndUnknownKeysIgnored) {
#ifdef FRANKYCPP_PRODUCTION
    GTEST_SKIP() << "Skipping malformed YAML test in production since CONFIG_CONST members cannot be overridden at runtime for verification.";
#endif
    YAML::Node n;
    n["TEMPO"]       = 50;
    n["FOO_UNKNOWN"] = 123;

    const auto c = n.as<EvalConfigData>();
    EXPECT_EQ(c.TEMPO, 50);
    // an unknown key should not break parsing; other defaults remain
    EXPECT_TRUE(c.USE_MATERIAL);
  }

  // Sanity-checks that str() contains representative fields for quick human inspection.
  TEST(EvalConfigDataTests, StrContainsExpectedFields) {
    const EvalConfigData c;
    const auto s = c.str();
    EXPECT_NE(s.find("USE_MATERIAL:"), std::string::npos);
    EXPECT_NE(s.find("TEMPO:"), std::string::npos);
  }

}// namespace
