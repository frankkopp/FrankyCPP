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

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "engine/config/EvalConfigData.h"
#include "engine/config/SearchConfigData.h"

namespace engine::config {

class ConfigManager {
public:
    static ConfigManager& instance();

    // Accessors (const refs) to current configuration
    const SearchConfigData& search() const noexcept { return currentSearch_; }
    const EvalConfigData&   eval() const noexcept { return currentEval_; }

    // Reset current configs back to built-in defaults
    void resetToDefaults();

    // Load from YAML files (paths optional). If a path is not provided, use default ConfigPaths.
    // Return true on success. Missing files are not considered fatal and fall back to defaults.
    // Malformed YAML returns false and preserves last good configuration.
    bool loadFromFiles(std::optional<std::filesystem::path> searchPath = {},
                       std::optional<std::filesystem::path> evalPath   = {});

    // Apply runtime overrides with the highest precedence.
    // The provided functor receives (SearchConfigData&, EvalConfigData&) to mutate current values.
    template <typename F>
    void applyOverrides(F&& fn) {
        fn(currentSearch_, currentEval_);
    }

    // Human-readable dumps
    std::string strCurrent() const;
    std::string strDefaults() const;

private:
    ConfigManager();

    // Keep defaults as pristine baselines
    SearchConfigData defaultsSearch_{};
    EvalConfigData   defaultsEval_{};

    // Current active configuration (initialized from defaults)
    SearchConfigData currentSearch_{};
    EvalConfigData   currentEval_{};
};

} // namespace engine::config
