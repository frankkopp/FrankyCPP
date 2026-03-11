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

#include "config/ConfigPaths.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

using namespace config;

namespace {

  // Smoke test: loads config/search.yaml and checks that file is present and parseable.
  // Most config keys are commented out (using struct defaults), so we only check:
  // - CONFIG_SOURCE (always present to indicate yaml was loaded)
  // - _YAML_SMOKE_TEST_MARKER (dedicated marker for this test)
  TEST(ConfigYamlSmokeTest, LoadsSearchYamlAndHasExpectedFlatKeys) {
    const auto path = ConfigPaths::SearchYaml();
    ASSERT_TRUE(std::filesystem::exists(path)) << "Missing file: " << path.string();
    YAML::Node root = YAML::LoadFile(path.string());
    ASSERT_TRUE(root) << "YAML parse returned empty node for: " << path.string();

    // These keys must always be present (not commented out)
    EXPECT_TRUE(root["CONFIG_SOURCE"]) << "CONFIG_SOURCE should be present";
    EXPECT_TRUE(root["_YAML_SMOKE_TEST_MARKER"]) << "_YAML_SMOKE_TEST_MARKER should be present";
  }

  // Smoke test: loads config/eval.yaml and checks that file is present and parseable.
  // Most config keys are commented out (using struct defaults), so we only check:
  // - EVAL_CONFIG_SOURCE (always present to indicate yaml was loaded)
  // - _YAML_SMOKE_TEST_MARKER (dedicated marker for this test)
  TEST(ConfigYamlSmokeTest, LoadsEvalYamlAndHasExpectedFlatKeys) {
    const auto path = ConfigPaths::EvalYaml();
    ASSERT_TRUE(std::filesystem::exists(path)) << "Missing file: " << path.string();
    YAML::Node root = YAML::LoadFile(path.string());
    ASSERT_TRUE(root) << "YAML parse returned empty node for: " << path.string();

    // These keys must always be present (not commented out)
    EXPECT_TRUE(root["EVAL_CONFIG_SOURCE"]) << "EVAL_CONFIG_SOURCE should be present";
    EXPECT_TRUE(root["_YAML_SMOKE_TEST_MARKER"]) << "_YAML_SMOKE_TEST_MARKER should be present";
  }

} // namespace
