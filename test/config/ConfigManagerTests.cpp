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
// ConfigManager tests: precedence, file I/O, overrides, and diagnostics

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include "config/ConfigManager.h"

using namespace config;


namespace {

  std::filesystem::path testBaseDir() {
    static std::filesystem::path base = std::filesystem::temp_directory_path() / "frankycpp_cfg_tests";
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    return base;
  }

  std::filesystem::path uniquePath(const std::string& stem) {
    const auto ts = std::chrono::steady_clock::now().time_since_epoch().count();
    return testBaseDir() / (stem + std::to_string(ts) + ".yaml");
  }

  std::filesystem::path writeYamlToFile(const YAML::Node& n, const std::string& stem) {
    YAML::Emitter out;
    out << n;
    const auto p = uniquePath(stem);
    std::ofstream ofs(p);
    ofs << std::string(out.c_str());
    return p;
  }

  std::filesystem::path writeInvalidYamlToFile(const std::string& stem) {
    const auto p = uniquePath(stem);
    std::ofstream ofs(p);
    ofs << "not: [valid"; // intentionally malformed YAML
    return p;
  }

  // Ensures missing files are non-fatal: manager keeps defaults and returns true.
  TEST(ConfigManagerTests, LoadMissingFilesUsesDefaults) {
    auto& mgr = ConfigManager::instance();
    mgr.resetToDefaults();

    // Point to paths that do not exist
    const auto missingSearch = testBaseDir() / "search_missing.yaml";
    const auto missingEval   = testBaseDir() / "eval_missing.yaml";
    if (std::filesystem::exists(missingSearch)) std::filesystem::remove(missingSearch);
    if (std::filesystem::exists(missingEval)) std::filesystem::remove(missingEval);

    const bool ok = mgr.loadFromFiles(missingSearch, missingEval);
    EXPECT_TRUE(ok);
    EXPECT_EQ(mgr.search().TT_SIZE_MB, SearchConfigData{}.TT_SIZE_MB);
    // TEMPO is CONFIG_CONST — accessible as static member in production, instance member in dev.
    // EvalConfigData{}.TEMPO works in both modes (static member access through instance is valid C++).
    EXPECT_EQ(mgr.eval().TEMPO, EvalConfigData{}.TEMPO);
  }

  // Verifies search-only YAML overrides are applied; eval remains defaults.
  TEST(ConfigManagerTests, LoadSearchYamlOverridesOnly) {
    auto& mgr = ConfigManager::instance();
    mgr.resetToDefaults();

    YAML::Node s;
    s["MOVE_OVERHEAD_MS"] = 42;
#ifndef FRANKYCPP_PRODUCTION
    // USE_TT is CONFIG_CONST in production — YAML setter is a no-op, cannot verify mutation.
    s["USE_TT"] = false;
#endif
    const auto sPath = writeYamlToFile(s, "search_override_");

    // eval path missing
    const auto ePath = testBaseDir() / "eval_not_present.yaml";
    if (std::filesystem::exists(ePath)) std::filesystem::remove(ePath);

    ASSERT_TRUE(mgr.loadFromFiles(sPath, ePath));
    EXPECT_EQ(mgr.search().MOVE_OVERHEAD_MS, 42);
#ifndef FRANKYCPP_PRODUCTION
    EXPECT_FALSE(mgr.search().USE_TT);
#endif
    // eval unchanged — TEMPO is CONFIG_CONST, EvalConfigData{}.TEMPO works in both modes.
    EXPECT_EQ(mgr.eval().TEMPO, EvalConfigData{}.TEMPO);
  }

  // Verifies eval-only YAML overrides are applied; search remains defaults.
  // In production, non-essential eval configs (TEMPO, USE_MATERIAL) are CONFIG_CONST
  // so YAML setters are no-ops; this test is skipped in production.
#ifndef FRANKYCPP_PRODUCTION
  TEST(ConfigManagerTests, LoadEvalYamlOverridesOnly) {
    auto& mgr = ConfigManager::instance();
    mgr.resetToDefaults();

    YAML::Node e;
    e["TEMPO"]        = 55;
    e["USE_MATERIAL"] = false;
    const auto ePath  = writeYamlToFile(e, "eval_override_");

    // search path missing
    const auto sPath = testBaseDir() / "search_not_present.yaml";
    if (std::filesystem::exists(sPath)) std::filesystem::remove(sPath);

    ASSERT_TRUE(mgr.loadFromFiles(sPath, ePath));
    EXPECT_EQ(mgr.eval().TEMPO, 55);
    EXPECT_FALSE(mgr.eval().USE_MATERIAL);
    // search unchanged
    EXPECT_EQ(mgr.search().TT_SIZE_MB, SearchConfigData{}.TT_SIZE_MB);
  }
#endif // FRANKYCPP_PRODUCTION

  // Ensures runtime overrides via applyOverrides() take effect with highest precedence.
  // In production, only essential configs (TT_SIZE_MB) can be overridden at runtime.
  TEST(ConfigManagerTests, ApplyRuntimeOverrides) {
    auto& mgr = ConfigManager::instance();
    mgr.resetToDefaults();

#ifndef FRANKYCPP_PRODUCTION
    mgr.applyOverrides([](SearchConfigData& s, EvalConfigData& e) {
      s.TT_SIZE_MB = 256;
      e.TEMPO      = 60;
    });
    EXPECT_EQ(mgr.search().TT_SIZE_MB, 256);
    EXPECT_EQ(mgr.eval().TEMPO, 60);
#else
    // Production: only essential configs can be overridden.
    mgr.applyOverrides([](SearchConfigData& s, EvalConfigData&) {
      s.TT_SIZE_MB = 256;
    });
    EXPECT_EQ(mgr.search().TT_SIZE_MB, 256);
#endif
  }

  // Malformed YAML should not change current values and should return false.
  // In production, only essential config mutations (MOVE_OVERHEAD_MS) can be verified.
  TEST(ConfigManagerTests, InvalidYamlRollsBackAndReturnsFalse) {
    auto& mgr = ConfigManager::instance();
    mgr.resetToDefaults();

    // establish a non-default current state to detect rollbacks
#ifndef FRANKYCPP_PRODUCTION
    mgr.applyOverrides([](SearchConfigData& s, EvalConfigData& e) {
      s.MOVE_OVERHEAD_MS = 77;
      e.TEMPO            = 66;
    });
#else
    mgr.applyOverrides([](SearchConfigData& s, EvalConfigData&) {
      s.MOVE_OVERHEAD_MS = 77;
    });
#endif

    const auto badSearch = writeInvalidYamlToFile("search_bad_");
    const auto badEval   = writeInvalidYamlToFile("eval_bad_");

    const bool ok = mgr.loadFromFiles(badSearch, badEval);
    EXPECT_FALSE(ok);
    // values should remain as before
    EXPECT_EQ(mgr.search().MOVE_OVERHEAD_MS, 77);
#ifndef FRANKYCPP_PRODUCTION
    EXPECT_EQ(mgr.eval().TEMPO, 66);
#endif
  }

  // str() should contain section headers and representative keys.
  TEST(ConfigManagerTests, StrOutputsContainSectionsAndKeys) {
    auto& mgr = ConfigManager::instance();
    mgr.resetToDefaults();

    const auto cur = mgr.strCurrent();
    const auto def = mgr.strDefaults();
    EXPECT_NE(cur.find("[Search]"), std::string::npos);
    EXPECT_NE(cur.find("TT_SIZE_MB:"), std::string::npos);
    EXPECT_NE(def.find("[Eval]"), std::string::npos);
    EXPECT_NE(def.find("TEMPO:"), std::string::npos);
  }

} // namespace
