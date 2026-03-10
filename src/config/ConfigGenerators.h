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
// These are runtime functions - no build-time code generation is involved.
//
// Functions:
//   generateConfigString() - Generate str() output for display
//   parseYamlConfig() - Parse YAML config using registry metadata
//   initUciOptionsFromRegistry() - Initialize UCI options from registry
//
// Usage:
//   SearchConfigData search;
//   EvalConfigData eval;
//   std::string output = generateConfigString(search, eval);
//
//=============================================================================

#include "config/ConfigDef.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

// Forward declarations
namespace engine {
  struct UciOption;
  class UciHandler;
} // namespace engine

namespace config {

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
    bool showAll                             = false,
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

  //=============================================================================
  // YAML Parsing Functions
  //=============================================================================

  /// Parse YAML node into config structs using registry metadata.
  /// Iterates registry entries with exposure.yaml=true and applies setters.
  ///
  /// Keys starting with '_' (underscore) are reserved for internal/test use
  /// (e.g., _YAML_SMOKE_TEST_MARKER) and are silently skipped when warning
  /// about unknown keys.
  ///
  /// @param node        YAML node to parse
  /// @param search      SearchConfigData to populate
  /// @param eval        EvalConfigData to populate
  /// @param warnUnknown If true, log warning for unknown keys (default: true)
  /// @return Set of keys that were successfully parsed
  std::set<std::string> parseYamlConfig(
    const YAML::Node& node,
    SearchConfigData& search,
    EvalConfigData& eval,
    bool warnUnknown = true);

  /// Overload for Search-only parsing (uses default EvalConfigData internally)
  ///
  /// @param node        YAML node to parse
  /// @param search      SearchConfigData to populate
  /// @param warnUnknown If true, log warning for unknown keys (default: true)
  /// @return Set of keys that were successfully parsed
  std::set<std::string> parseYamlConfig(
    const YAML::Node& node,
    SearchConfigData& search,
    bool warnUnknown = true);

  /// Overload for Eval-only parsing (uses default SearchConfigData internally)
  ///
  /// @param node        YAML node to parse
  /// @param eval        EvalConfigData to populate
  /// @param warnUnknown If true, log warning for unknown keys (default: true)
  /// @return parseYamlConfig Set of keys that were successfully parsed
  std::set<std::string> parseYamlConfig(
    const YAML::Node& node,
    EvalConfigData& eval,
    bool warnUnknown = true);

  //=============================================================================
  // UCI Options Generation
  //=============================================================================

  /// Initialize UCI options from registry (called by UciOptions::initOptions).
  /// Creates UciOption objects by iterating registry entries with exposure.uci = true.
  ///
  /// This function generates UCI options automatically from ConfigRegistry metadata,
  /// eliminating the need for manual option registration in UciOptions.cpp.
  ///
  /// For each registry entry with exposure.uci = true:
  /// - Bool types become CHECK options
  /// - Int types become SPIN options with min/max bounds
  /// - String types become STRING options
  /// - Combo types become COMBO options with predefined values
  ///
  /// @param optionVector  Vector to populate with UciOption objects
  /// @param uciOptionsPtr Pointer to UciOptions instance (for handler callbacks)
  void initUciOptionsFromRegistry(std::vector<engine::UciOption>& optionVector, void* uciOptionsPtr);

  /// Add UCI-only buttons that don't have struct members (e.g., "Clear Hash").
  /// These are special options that trigger actions but don't store values.
  ///
  /// @param optionVector  Vector to add button options to
  /// @param uciOptionsPtr    Pointer to UciOptions instance (for handler callbacks)
  void addUciOnlyButtons(std::vector<engine::UciOption>& optionVector, void* uciOptionsPtr);

  //=============================================================================
  // Configuration Discovery
  //=============================================================================

  /// Output format for configuration display
  enum class ConfigOutputFormat {
    Table, // Human-readable table with columns
    Yaml,  // YAML template with comments
    Json   // Machine-readable JSON
  };

  /// Generate formatted table of all config settings.
  /// Shows name, type, default, current, min/max, UCI name in columns.
  ///
  /// @param search       SearchConfigData instance to read current values from
  /// @param eval         EvalConfigData instance to read current values from
  /// @param domainFilter If set, only include entries from specified domain
  /// @return Formatted table string
  [[nodiscard]] std::string generateConfigTable(
    const SearchConfigData& search,
    const EvalConfigData& eval,
    std::optional<ConfigDomain> domainFilter = std::nullopt);

  /// Generate YAML template with all settings as comments.
  /// Users can copy sections they want to override into their config files.
  ///
  /// @param domainFilter If set, only include entries from specified domain
  /// @return YAML template string with all settings commented out
  [[nodiscard]] std::string generateYamlTemplate(
    std::optional<ConfigDomain> domainFilter = std::nullopt);

  /// Generate JSON representation of all config settings.
  /// Useful for tooling and automated configuration.
  ///
  /// @param search       SearchConfigData instance to read current values from
  /// @param eval         EvalConfigData instance to read current values from
  /// @param domainFilter If set, only include entries from specified domain
  /// @return JSON string with config metadata
  [[nodiscard]] std::string generateConfigJson(
    const SearchConfigData& search,
    const EvalConfigData& eval,
    std::optional<ConfigDomain> domainFilter = std::nullopt);

  /// Parse domain name string to ConfigDomain enum.
  /// Case-insensitive comparison.
  ///
  /// @param name  Domain name (e.g., "search", "eval", "general")
  /// @return Optional ConfigDomain if name is valid, nullopt otherwise
  [[nodiscard]] std::optional<ConfigDomain> parseDomainName(const std::string& name);

} // namespace config

#endif // FRANKYCPP_CONFIGGENERATORS_H
