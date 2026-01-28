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

#include "engine/config/ConfigManager.h"
#include "engine/config/ConfigPaths.h"

using engine::config::ConfigManager;
using engine::config::EvalConfigData;
using engine::config::SearchConfigData;

namespace {

  // RAII helper to change current working directory for the scope
  struct ScopedCwd {
    std::filesystem::path prev;
    explicit ScopedCwd(const std::filesystem::path& p) : prev(std::filesystem::current_path()) {
      std::error_code ec;
      std::filesystem::create_directories(p, ec);
      std::filesystem::current_path(p, ec);
    }
    ~ScopedCwd() {
      std::error_code ec;
      std::filesystem::current_path(prev, ec);
    }
  };

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

    // Validate a couple of representative values from repo YAMLs
    EXPECT_EQ(mgr.search().TT_SIZE_MB, 64);// from config/search.yaml
    EXPECT_EQ(mgr.eval().TEMPO, 34);       // from config/eval.yaml
  }

  // Description: Confirms that when default config files are missing, loadFromFiles succeeds and keeps hard-coded defaults.
  TEST(ConfigAutoInitTests, DefaultPathsMissingUsesDefaultsAndSucceeds) {
    // Switch to a fresh temp directory with no config folder
    const auto tmp = makeTempDir("missing");
    ScopedCwd cwd(tmp);

    auto& mgr = ConfigManager::instance();
    mgr.resetToDefaults();

    // Call loadFromFiles() with default paths (which do not exist here)
    const bool ok = mgr.loadFromFiles();
    EXPECT_TRUE(ok);

    // Expect defaults remain
    EXPECT_EQ(mgr.search().TT_SIZE_MB, SearchConfigData{}.TT_SIZE_MB);
    EXPECT_EQ(mgr.eval().TEMPO, EvalConfigData{}.TEMPO);
  }

  // Description: Checks that malformed YAML at default paths causes loadFromFiles to return false and roll back to the prior state.
  TEST(ConfigAutoInitTests, DefaultPathsMalformedRollsBackAndReturnsFalse) {
    // Prepare temp directory with malformed YAML files in ./config
    const auto tmp = makeTempDir("malformed");
    ScopedCwd cwd(tmp);
    const auto cfgDir = tmp / "config";
    std::error_code ec;
    std::filesystem::create_directories(cfgDir, ec);

    // Write malformed YAML
    {
      std::ofstream ofs(cfgDir / "search.yaml");
      ofs << "not: [valid";// malformed
    }
    {
      std::ofstream ofs(cfgDir / "eval.yaml");
      ofs << "invalid: true: :";// malformed
    }

    auto& mgr = ConfigManager::instance();
    mgr.resetToDefaults();

    // Establish a non-default state to detect rollback
    mgr.applyOverrides([](SearchConfigData& s, EvalConfigData& e) {
      s.MOVE_OVERHEAD_MS = 77;
      e.TEMPO            = 66;
    });

    const bool ok = mgr.loadFromFiles();
    EXPECT_FALSE(ok);
    EXPECT_EQ(mgr.search().MOVE_OVERHEAD_MS, 77);
    EXPECT_EQ(mgr.eval().TEMPO, 66);
  }

  // Description: Ensures resetToDefaults restores values captured at initial auto-load, even after loads in other directories.
  TEST(ConfigAutoInitTests, ResetReturnsToInitiallyLoadedDefaults) {
    // Ensure we have repo YAML configs in current dir
    ASSERT_TRUE(std::filesystem::exists(ConfigPaths::SearchYaml()));
    ASSERT_TRUE(std::filesystem::exists(ConfigPaths::EvalYaml()));

    auto& mgr = ConfigManager::instance();

    // Start clean from initial defaults (captured at auto-load time)
    mgr.resetToDefaults();

    // Change some values
    mgr.applyOverrides([](SearchConfigData& s, EvalConfigData& e){
        s.TT_SIZE_MB = 999;
        e.TEMPO = 1;
    });

    // Reset should restore the initially loaded YAML values (64 and 34 from repo configs)
    mgr.resetToDefaults();
    EXPECT_EQ(mgr.search().TT_SIZE_MB, 64);
    EXPECT_EQ(mgr.eval().TEMPO, 34);

    // Now move to a dir with missing config and load defaults (fallback)
    {
      const auto tmp = makeTempDir("reset_defaults_preserved");
      ScopedCwd cwd(tmp);
        ASSERT_TRUE(mgr.loadFromFiles());
        // In the absence of YAML, fallback values apply (TT_SIZE_MB defaults to hard-coded 64, TEMPO to 34 in current repo)
        // but crucially, defaults stored at startup should remain unchanged.
    }

    // After coming back, reset should still restore to the initially loaded YAML values
    mgr.resetToDefaults();
    EXPECT_EQ(mgr.search().TT_SIZE_MB, 64);
    EXPECT_EQ(mgr.eval().TEMPO, 34);
  }

}// namespace
