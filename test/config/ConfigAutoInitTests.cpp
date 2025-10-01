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

// FrankyCPP
// Auto-init and default-path loading tests for ConfigManager

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <chrono>

#include "engine/config/ConfigManager.h"
#include "engine/config/ConfigPaths.h"

using engine::config::ConfigManager;
using engine::config::SearchConfigData;
using engine::config::EvalConfigData;

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
    const auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
    auto base = std::filesystem::temp_directory_path() / ("frankycpp_auto_init_" + name + "_" + std::to_string(ts));
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    return base;
}

TEST(ConfigAutoInitTests, AutoLoadFlagIsSetOnFirstUse) {
    // Accessing the singleton should have triggered auto-load in its constructor
    auto& mgr = ConfigManager::instance();
    EXPECT_TRUE(mgr.wasAutoLoaded());
}

TEST(ConfigAutoInitTests, DefaultPathsLoadWhenPresent) {
    // Use the repository-provided config copied next to the test executable
    auto& mgr = ConfigManager::instance();

    // Make sure we are in a directory where ./config exists (the test runner's cwd)
    ASSERT_TRUE(std::filesystem::exists(ConfigPaths::SearchYaml()));
    ASSERT_TRUE(std::filesystem::exists(ConfigPaths::EvalYaml()));

    mgr.resetToDefaults();
    ASSERT_TRUE(mgr.loadFromFiles());

    // Validate a couple of representative values from repo YAMLs
    EXPECT_EQ(mgr.search().TT_SIZE_MB, 64); // from config/search.yaml
    EXPECT_EQ(mgr.eval().TEMPO, 34);        // from config/eval.yaml
}

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
        ofs << "not: [valid"; // malformed
    }
    {
        std::ofstream ofs(cfgDir / "eval.yaml");
        ofs << "invalid: true: :"; // malformed
    }

    auto& mgr = ConfigManager::instance();
    mgr.resetToDefaults();

    // Establish a non-default state to detect rollback
    mgr.applyOverrides([](SearchConfigData& s, EvalConfigData& e){
        s.MOVE_OVERHEAD_MS = 77;
        e.TEMPO = 66;
    });

    const bool ok = mgr.loadFromFiles();
    EXPECT_FALSE(ok);
    EXPECT_EQ(mgr.search().MOVE_OVERHEAD_MS, 77);
    EXPECT_EQ(mgr.eval().TEMPO, 66);
}

} // namespace
