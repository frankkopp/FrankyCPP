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

#ifndef FRANKYCPP_CONFIGGENERATORS_H
#define FRANKYCPP_CONFIGGENERATORS_H

//=============================================================================
// ConfigGenerators.h - Runtime Functions Using ConfigRegistry
//=============================================================================
//
// Provides functions that iterate over ConfigRegistry to produce output.
// These are runtime functions - no build-time code generation involved.
//
// Functions:
//   generateConfigString() - Generate str() output for display
//   (Future: parseYamlFromRegistry(), initUciOptionsFromRegistry())
//
// Usage:
//   SearchConfigData search;
//   EvalConfigData eval;
//   std::string output = generateConfigString(search, eval);
//
//=============================================================================

#include "config/ConfigDef.h"

#include <optional>
#include <string>

// Forward declarations
struct SearchConfigData;
struct EvalConfigData;

/// Generate configuration string for display.
/// Iterates over ConfigRegistry::displayOptions() and formats each value.
///
/// @param search     SearchConfigData instance to read values from
/// @param eval       EvalConfigData instance to read values from
/// @param showAll    If true, include entries with display=false (default: false)
/// @param domainFilter If set, only include entries from specified domain
/// @return Formatted string with all config values
[[nodiscard]] std::string generateConfigString(
    const SearchConfigData& search,
    const EvalConfigData& eval,
    bool showAll = false,
    std::optional<ConfigDomain> domainFilter = std::nullopt);

/// Generate configuration string for a single domain.
/// Convenience wrapper around generateConfigString with domain filter.
///
/// @param search  SearchConfigData instance
/// @param eval    EvalConfigData instance
/// @param domain  Domain to filter by
/// @return Formatted string with config values for specified domain
[[nodiscard]] std::string generateConfigStringForDomain(
    const SearchConfigData& search,
    const EvalConfigData& eval,
    ConfigDomain domain);

#endif  // FRANKYCPP_CONFIGGENERATORS_H
