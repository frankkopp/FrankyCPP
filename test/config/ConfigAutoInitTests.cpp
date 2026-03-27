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

// FrankyCPP
// Auto-init and default-path loading tests for ConfigManager

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include "config/ConfigManager.h"
#include "config/ConfigPaths.h"

using namespace config;

namespace {

  std::filesystem::path makeTempDir(const std::string& name) {
    const auto ts = steady_clock::now().time_since_epoch().count();
    auto base     = std::filesystem::temp_directory_path() / ("frankycpp_auto_init_" + name + "_" + std::to_string(ts));
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    return base;
  }

  // Description: Verifies the ConfigManager singleton auto-loads on first access and sets the auto-loaded flag.
  TEST(ConfigAutoInitTests, AutoLoadFlagIsSetOnFirstUse) {
    // Accessing the singleton should have triggered auto-load in its constructor
    const auto& mgr = ConfigManager::instance();
    EXPECT_TRUE(mgr.wasAutoLoaded());
  }

  // Description: Ensures default repo config files at standard paths are loaded and known values are read.
  TEST(ConfigAutoInitTests, DefaultPathsLoadWhenPresent) {
    // Use the repository-provided config copied next to the test executable
    auto& mgr = ConfigManager::instance();

    // Make sure we are in a directory where ./config exists (the test runner's cwd)
    ASSERT_TRUE(std::filesystem::exists(ConfigPaths::SearchYaml()));
    ASSERT_TRUE(std::filesystem::exists(ConfigPaths::EvalYaml()));

    mgr.resetToDefaults();
    ASSERT_TRUE(mgr.loadFromFiles());

    // Validate that YAML files were actually loaded by checking the CONFIG_SOURCE markers
    // (set explicitly in both YAML files, not subject to tuning changes)
    EXPECT_EQ(mgr.search().CONFIG_SOURCE, "yaml-file");
    EXPECT_EQ(mgr.eval().EVAL_CONFIG_SOURCE, "yaml-file");
  }

  // Description: Confirms that when default config files are missing, loadFromFiles succeeds and keeps hard-coded defaults.
  TEST(ConfigAutoInitTests, DefaultPathsMissingUsesDefaultsAndSucceeds) {
    // Use explicit paths to non-existent files (config paths now resolve relative
    // to the exe directory, so CWD manipulation no longer affects default paths).
    const auto tmp = makeTempDir("missing");
    const auto nonExistentSearch = tmp / "config" / "search.yaml";
    const auto nonExistentEval   = tmp / "config" / "eval.yaml";

    auto& mgr = ConfigManager::instance();
    mgr.resetToDefaults();

    // Call loadFromFiles() with explicit paths that do not exist
    const bool ok = mgr.loadFromFiles(nonExistentSearch, nonExistentEval);
    EXPECT_TRUE(ok);

    // Expect defaults remain
    EXPECT_EQ(mgr.search().TT_SIZE_MB, SearchConfigData{}.TT_SIZE_MB);
    EXPECT_EQ(mgr.eval().TEMPO, EvalConfigData{}.TEMPO);
  }

  // Description: Checks that malformed YAML at default paths causes loadFromFiles to return false and roll back to the prior state.
#ifndef FRANKYCPP_PRODUCTION // In production, only essential config mutations (MOVE_OVERHEAD_MS) can be verified, so this test is dev-only.
  TEST(ConfigAutoInitTests, DefaultPathsMalformedRollsBackAndReturnsFalse) {
    // Prepare temp directory with malformed YAML files
    const auto tmp    = makeTempDir("malformed");
    const auto cfgDir = tmp / "config";
    std::error_code ec;
    std::filesystem::create_directories(cfgDir, ec);

    const auto malformedSearch = cfgDir / "search.yaml";
    const auto malformedEval   = cfgDir / "eval.yaml";

    // Write malformed YAML
    {
      std::ofstream ofs(malformedSearch);
      ofs << "not: [valid"; // malformed
    }
    {
      std::ofstream ofs(malformedEval);
      ofs << "invalid: true: :"; // malformed
    }

    auto& mgr = ConfigManager::instance();
    mgr.resetToDefaults();

    // Establish a non-default state to detect rollback
    mgr.applyOverrides([](SearchConfigData& s, EvalConfigData& e) {
      s.MOVE_OVERHEAD_MS = 77;
      e.TEMPO            = 66;
    });

    // Use explicit paths to the malformed files (not relying on CWD)
    const bool ok = mgr.loadFromFiles(malformedSearch, malformedEval);
    EXPECT_FALSE(ok);
    EXPECT_EQ(mgr.search().MOVE_OVERHEAD_MS, 77);
    EXPECT_EQ(mgr.eval().TEMPO, 66);
  }
#endif

  // Description: Ensures resetToDefaults restores values captured at initial auto-load, even after loads in other directories.
#ifndef FRANKYCPP_PRODUCTION // In production, only essential config mutations (MOVE_OVERHEAD_MS) can be verified, so this test is dev-only.
  TEST(ConfigAutoInitTests, ResetReturnsToInitiallyLoadedDefaults) {
    // Ensure we have repo YAML configs in current dir
    ASSERT_TRUE(std::filesystem::exists(ConfigPaths::SearchYaml()));
    ASSERT_TRUE(std::filesystem::exists(ConfigPaths::EvalYaml()));

    auto& mgr = ConfigManager::instance();

    // Start clean from initial defaults (captured at auto-load time)
    mgr.resetToDefaults();

    // Capture the initially loaded default values (from YAML at startup)
    const auto defaultTempo    = mgr.eval().TEMPO;
    const auto defaultConfigSrc = mgr.search().CONFIG_SOURCE;

    // Change some values to something definitely different from defaults
    const int nonDefaultTempo = (defaultTempo == 1) ? 2 : 1;
    mgr.applyOverrides([nonDefaultTempo](SearchConfigData& s, EvalConfigData& e) {
      s.CONFIG_SOURCE = "overridden";
      e.TEMPO         = nonDefaultTempo;
    });

    // Verify overrides took effect
    EXPECT_NE(mgr.eval().TEMPO, defaultTempo);
    EXPECT_EQ(mgr.search().CONFIG_SOURCE, "overridden");

    // Reset should restore the initially loaded YAML values
    mgr.resetToDefaults();
    EXPECT_EQ(mgr.eval().TEMPO, defaultTempo);
    EXPECT_EQ(mgr.search().CONFIG_SOURCE, defaultConfigSrc);

    // Now load from non-existent paths to get fallback values
    {
      const auto tmp = makeTempDir("reset_defaults_preserved");
      const auto nonExistentSearch = tmp / "config" / "search.yaml";
      const auto nonExistentEval   = tmp / "config" / "eval.yaml";
      ASSERT_TRUE(mgr.loadFromFiles(nonExistentSearch, nonExistentEval));
      // In the absence of YAML, fallback values apply — but crucially,
      // defaults stored at startup should remain unchanged.
    }

    // After coming back, reset should still restore to the initially loaded YAML values
    mgr.resetToDefaults();
    EXPECT_EQ(mgr.eval().TEMPO, defaultTempo);
    EXPECT_EQ(mgr.search().CONFIG_SOURCE, defaultConfigSrc);
  }
#endif

} // namespace
