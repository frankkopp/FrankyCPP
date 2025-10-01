// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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

#include "../../src/engine/config/ConfigPaths.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

namespace {

  // Smoke test: loads config/search.yaml and checks representative flat keys exist
  // to ensure file presence and basic YAML parsing work.
  TEST(ConfigYamlSmokeTest, LoadsSearchYamlAndHasExpectedFlatKeys) {
    const auto path = ConfigPaths::SearchYaml();
    ASSERT_TRUE(std::filesystem::exists(path)) << "Missing file: " << path.string();
    YAML::Node root = YAML::LoadFile(path.string());
    ASSERT_TRUE(root) << "YAML parse returned empty node for: " << path.string();

    // Flat keys expected from SearchConfig.h
    EXPECT_TRUE(root["MOVE_OVERHEAD_MS"]);
    EXPECT_TRUE(root["USE_BOOK"]);
    EXPECT_TRUE(root["BOOK_PATH"]);
    EXPECT_TRUE(root["USE_ALPHABETA"]);
    EXPECT_TRUE(root["USE_QUIESCENCE"]);
    EXPECT_TRUE(root["TT_SIZE_MB"]);
    EXPECT_TRUE(root["RFP_MARGIN"]) << "RFP_MARGIN array should be present";
    EXPECT_TRUE(root["FP_MARGIN"]) << "FP_MARGIN array should be present";
  }

  // Smoke test: loads config/eval.yaml and checks representative flat keys exist
  // to ensure file presence and basic YAML parsing work.
  TEST(ConfigYamlSmokeTest, LoadsEvalYamlAndHasExpectedFlatKeys) {
    const auto path = ConfigPaths::EvalYaml();
    ASSERT_TRUE(std::filesystem::exists(path)) << "Missing file: " << path.string();
    YAML::Node root = YAML::LoadFile(path.string());
    ASSERT_TRUE(root) << "YAML parse returned empty node for: " << path.string();

    // Flat keys expected from EvalConfig.h
    EXPECT_TRUE(root["USE_MATERIAL"]);
    EXPECT_TRUE(root["USE_POSITIONAL"]);
    EXPECT_TRUE(root["USE_TEMPO"]);
    EXPECT_TRUE(root["TEMPO"]);
    EXPECT_TRUE(root["USE_PAWN_EVAL"]);
    EXPECT_TRUE(root["BISHOP_PAIR_MID_BONUS"]);
  }

}// namespace
