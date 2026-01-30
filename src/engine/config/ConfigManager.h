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

#ifndef FRANKYCPP_CONFIGMANAGER_H
#define FRANKYCPP_CONFIGMANAGER_H

#include <filesystem>
#include <optional>
#include <string>

#include "engine/config/EvalConfigData.h"
#include "engine/config/SearchConfigData.h"

namespace engine::config {

  class ConfigManager {
    ConfigManager();

    // Non-copyable, non-movable singleton
    ConfigManager(const ConfigManager&)            = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&)                 = delete;
    ConfigManager& operator=(ConfigManager&&)      = delete;

    // Hard-coded fallback values (constructed defaults). Only used if YAML is missing or invalid.
    SearchConfigData fallbackSearch_{};
    EvalConfigData fallbackEval_{};

    // Defaults captured from the initial autoload (YAML at startup, or fallback if not available)
    SearchConfigData defaultSearch_{};
    EvalConfigData defaultEval_{};

    // Current active configuration
    SearchConfigData currentSearch_{};
    EvalConfigData currentEval_{};

    // Auto-init flags
    bool autoLoadAttempted_{false};
    bool lastLoadOk_{false};

  public:
    static ConfigManager& instance();

    // Accessors (const refs) to current configuration
    const SearchConfigData& search() const noexcept { return currentSearch_; }
    const EvalConfigData& eval() const noexcept { return currentEval_; }

    // Reset current configs back to the initially loaded defaults (from YAML at startup, or fallback if YAML unavailable)
    void resetToDefaults();

    // Load from YAML files (paths optional). If a path is not provided, use default ConfigPaths.
    // Return true on success. Missing files are not considered fatal and fall back to hard-coded values.
    // Malformed YAML returns false and preserves last good configuration.
    bool loadFromFiles(const std::optional<std::filesystem::path>& searchPath = {},
                       const std::optional<std::filesystem::path>& evalPath   = {});

    /// Apply ad-hoc runtime overrides to the currently active configuration.
    ///
    /// Contract:
    /// - Invokes the provided callable with two non-const references: (SearchConfigData&, EvalConfigData&).
    /// - The callable may mutate both objects in-place; changes take effect immediately.
    /// - Overrides persist until you call resetToDefaults() or loadFromFiles() again.
    ///
    /// Notes:
    /// - Precedence: values changed here take the highest precedence at runtime. A subsequent loadFromFiles()
    ///   will replace current values, so if you want to combine YAML with overrides, call applyOverrides() after loading.
    /// - Thread-safety: ConfigManager has no internal locking. Call this during initialization or ensure
    ///   external synchronization so other threads do not read/modify config concurrently.
    ///
    /// Example:
    ///   engine::config::ConfigManager::instance().applyOverrides([](auto& s, auto& e) {
    ///       s.MOVE_OVERHEAD_MS = 25;   // Search tweak
    ///       s.USE_PVS = true;          // enable PVS
    ///       e.TEMPO = 40;              // Eval tweak
    ///   });
    template<typename F>
    void applyOverrides(F&& fn) { fn(currentSearch_, currentEval_); }

    // Diagnostics
    bool wasAutoLoaded() const noexcept { return autoLoadAttempted_; }
    bool lastLoadOk() const noexcept { return lastLoadOk_; }

    // Human-readable dumps
    std::string strCurrent() const;
    std::string strDefaults() const;// dumps the initially loaded defaults
  };

}// namespace engine::config

//
//
// using cm   = engine::config::ConfigManager;
// using scfg = engine::config::SearchConfigData;
//
// auto& cfg = cm::instance();
//
// cfg.applyOverrides([&](scfg& s, auto&) {
//

// Helper
#define SEARCH_CONFIG engine::config::ConfigManager::instance().search()

// Helper macro to simplify usage of applyOverrides in lambdas
// Use s for SearchConfigData and e for EvalConfigData
// Example: CONFIG_OVERRIDE(s.MOVE_OVERHEAD_MS = 25; s.USE_PVS = true;)
#define CONFIG_OVERRIDE(expr) engine::config::ConfigManager::instance().applyOverrides([&]([[maybe_unused]] engine::config::SearchConfigData& s, [[maybe_unused]]engine::config::EvalConfigData& e) { expr; })

// For multi-line overrides
// Use s for SearchConfigData and e for EvalConfigData
// Example:
// CONFIG_OVERRIDE_START()
//    s.MOVE_OVERHEAD_MS = 25;   // Search tweak
//    s.USE_PVS = true;          // enable PVS
// CONFIG_OVERRIDE_END();
#define CONFIG_OVERRIDE_START() engine::config::ConfigManager::instance().applyOverrides([&]([[maybe_unused]] engine::config::SearchConfigData& s, [[maybe_unused]] engine::config::EvalConfigData& e) {

// For multi-line overrides
// Use s for SearchConfigData and e for EvalConfigData
// Example:
// CONFIG_OVERRIDE_START()
//    s.MOVE_OVERHEAD_MS = 25;   // Search tweak
//    s.USE_PVS = true;          // enable PVS
// CONFIG_OVERRIDE_END();
#define CONFIG_OVERRIDE_END() })

#endif // FRANKYCPP_CONFIGMANAGER_H
