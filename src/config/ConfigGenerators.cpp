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

#include "config/ConfigGenerators.h"
#include "config/ConfigRegistry.h"
#include "config/EvalConfigData.h"
#include "config/SearchConfigData.h"

#include "common/Logging.h"

#include <set>
#include <sstream>
#include <vector>

std::string generateConfigString(
  const SearchConfigData& search,
  const EvalConfigData& eval,
  const bool showAll,
  const std::optional<ConfigDomain> domainFilter) {

  const auto& registry = ConfigRegistry::instance();
  std::ostringstream oss;

  // Collect entries to display
  std::vector<const ConfigDef*> entries;
  if (showAll) {
    for (const auto& def : registry.all()) {
      if (!domainFilter.has_value() || def.domain == domainFilter.value()) {
        entries.push_back(&def);
      }
    }
  }
  else {
    for (const auto* def : registry.displayOptions()) {
      if (!domainFilter.has_value() || def->domain == domainFilter.value()) {
        entries.push_back(def);
      }
    }
  }

  // Group by domain for organized output
  // Order: General, Search, Eval, Tuning, Debug
  const std::vector domainOrder = {
    ConfigDomain::General,
    ConfigDomain::Search,
    ConfigDomain::Eval,
    ConfigDomain::Tuning,
    ConfigDomain::Debug};

  for (const auto domain : domainOrder) {
    // Skip if filtering to a different domain
    if (domainFilter.has_value() && domain != domainFilter.value()) {
      continue;
    }

    // Collect entries for this domain
    std::vector<const ConfigDef*> domainEntries;
    for (const auto* def : entries) {
      if (def->domain == domain) {
        domainEntries.push_back(def);
      }
    }

    // Skip empty domains
    if (domainEntries.empty()) {
      continue;
    }

    // Domain header
    oss << "=== " << domainToString(domain) << " ===\n";

    // Output each entry
    for (const auto* def : domainEntries) {
      const std::string value = def->getter(search, eval);
      oss << def->name << ": " << value << "\n";
    }

    oss << "\n";
  }

  return oss.str();
}

std::string generateConfigStringForDomain(
  const SearchConfigData& search,
  const EvalConfigData& eval,
  const ConfigDomain domain) {
  return generateConfigString(search, eval, false, domain);
}

//=============================================================================
// SearchConfigData::str() implementation
//=============================================================================

std::string SearchConfigData::str() const {
  // Use a default EvalConfigData - we only output General and Search domains
  const EvalConfigData defaultEval;
  std::ostringstream oss;

  // Output General domain
  oss << generateConfigStringForDomain(*this, defaultEval, ConfigDomain::General);

  // Output Search domain
  oss << generateConfigStringForDomain(*this, defaultEval, ConfigDomain::Search);

  return oss.str();
}

//=============================================================================
// EvalConfigData::str() implementation
//=============================================================================

std::string EvalConfigData::str() const {
  // Use a default SearchConfigData - we only output Eval domain
  const SearchConfigData defaultSearch;
  return generateConfigStringForDomain(defaultSearch, *this, ConfigDomain::Eval);
}

//=============================================================================
// YAML Parsing Functions
//=============================================================================

namespace {

  /// Helper to convert YAML sequence to comma-separated string for array parsing
  std::string yamlSequenceToString(const YAML::Node& node) {
    if (!node.IsSequence()) {
      return node.as<std::string>();
    }
    std::ostringstream oss;
    for (std::size_t i = 0; i < node.size(); ++i) {
      if (i > 0) oss << ",";
      oss << node[i].as<std::string>();
    }
    return oss.str();
  }

  /// Helper to warn about unknown keys
  void logUnknownKey(const std::string& key, const std::string& context) {
    LOG__WARN(Logger::get().SEARCH_LOG, "Unknown key in {} config: {}", context, key);
  }

}// namespace

std::set<std::string> parseYamlConfig(
  const YAML::Node& node,
  SearchConfigData& search,
  EvalConfigData& eval,
  const bool warnUnknown) {

  std::set<std::string> parsedKeys;

  if (!node || !node.IsMap()) {
    return parsedKeys;
  }

  const auto& registry = ConfigRegistry::instance();

  // Iterate all YAML-exposed config entries
  for (const auto* def : registry.yamlOptions()) {
    if (node[def->name]) {
      try {
        std::string value;

        // Handle array types specially - convert YAML sequence to comma-separated string
        if (def->valueType == ConfigValueType::IntArray) {
          value = yamlSequenceToString(node[def->name]);
        }
        else if (def->valueType == ConfigValueType::Double) {
          // For doubles, use as<double>() then convert to string to handle numeric YAML nodes
          value = std::to_string(node[def->name].as<double>());
        }
        else if (def->valueType == ConfigValueType::Int) {
          // For ints, use as<int>() then convert to string
          value = std::to_string(node[def->name].as<int>());
        }
        else if (def->valueType == ConfigValueType::Bool) {
          // For bools, use as<bool>() then convert to string
          value = node[def->name].as<bool>() ? "true" : "false";
        }
        else {
          // String and other types - get as string directly
          value = node[def->name].as<std::string>();
        }

        // Apply the setter
        def->setter(search, eval, value);
        parsedKeys.insert(def->name);
      } catch (const std::exception& e) {
        LOG__WARN(Logger::get().SEARCH_LOG,
                  "Failed to parse config '{}': {}", def->name, e.what());
      }
    }
  }

  // Warn about unknown keys if requested
  if (warnUnknown) {
    for (const auto& kv : node) {
      const auto key = kv.first.as<std::string>("");
      if (!key.empty() && !parsedKeys.contains(key)) {
        logUnknownKey(key, "YAML");
      }
    }
  }

  return parsedKeys;
}

std::set<std::string> parseYamlConfig(
  const YAML::Node& node,
  SearchConfigData& search,
  const bool warnUnknown) {
  EvalConfigData dummyEval;
  return parseYamlConfig(node, search, dummyEval, warnUnknown);
}

std::set<std::string> parseYamlConfig(
  const YAML::Node& node,
  EvalConfigData& eval,
  const bool warnUnknown) {
  SearchConfigData dummySearch;
  return parseYamlConfig(node, dummySearch, eval, warnUnknown);
}

//=============================================================================
// UCI Options Generation
//=============================================================================

// We need to include UciOptions.h here (not in header to avoid circular deps)
#include "engine/Search.h"
#include "engine/UciHandler.h"
#include "engine/UciOptions.h"

void initUciOptionsFromRegistry(std::vector<UciOption>& optionVector, void* uciOptionsPtr) {
  // ReSharper disable once CppDFAUnusedValue
  // ReSharper disable once CppDFAUnreadVariable
  auto* uciOptions     = static_cast<UciOptions*>(uciOptionsPtr);
  const auto& registry = ConfigRegistry::instance();

  // Iterate all UCI-exposed config entries
  for (const auto* def : registry.uciOptions()) {
    // Skip entries with empty UCI name (internal-only)
    if (def->uciName.empty()) {
      continue;
    }

    // Skip IntArray types - they're not exposed via UCI
    if (def->valueType == ConfigValueType::IntArray) {
      continue;
    }

    // Capture the config name for use in lambdas (names are stable in registry)
    const std::string configName = def->name;
    const std::string uciName    = def->uciName;

    // Check if this option has a custom handler
    const bool hasCustomHandler = def->customUciHandler.has_value();

    // Create appropriate option type based on valueType
    switch (def->valueType) {
      case ConfigValueType::Bool: {
        const bool defaultVal = parseBool(def->defaultValue);

        optionVector.emplace_back(uciName.c_str(), defaultVal,
                                  [configName, uciName, uciOptions, hasCustomHandler](UciHandler* uciHandler) {
                                    const UciOption* opt = uciOptions->getOption(uciName);
                                    if (!opt) return;
                                    const bool value = opt->currentValue == "true";

                                    // Find the registry entry and use its setter
                                    const ConfigDef* regDef = ConfigRegistry::instance().find(configName);
                                    if (!regDef) return;

                                    ConfigManager::instance().applyOverrides(
                                      [regDef, value](SearchConfigData& s, EvalConfigData& e) {
                                        regDef->setter(s, e, value ? "true" : "false");
                                      });

                                    // Call custom handler if present
                                    if (hasCustomHandler && regDef->customUciHandler) {
                                      (*regDef->customUciHandler)(uciHandler);
                                    }
                                  });
        break;
      }

      case ConfigValueType::Int: {
        const int defaultVal = parseInt(def->defaultValue);
        const int minVal     = def->minValue.value_or(INT_MIN);
        const int maxVal     = def->maxValue.value_or(INT_MAX);

        optionVector.emplace_back(uciName.c_str(), defaultVal, minVal, maxVal,
                                  [configName, uciName, uciOptions, hasCustomHandler](UciHandler* uciHandler) {
                                    const UciOption* opt = uciOptions->getOption(uciName);
                                    if (!opt) return;
                                    const std::string valueStr = opt->currentValue;

                                    const ConfigDef* regDef = ConfigRegistry::instance().find(configName);
                                    if (!regDef) return;

                                    ConfigManager::instance().applyOverrides(
                                      [regDef, valueStr](SearchConfigData& s, EvalConfigData& e) {
                                        regDef->setter(s, e, valueStr);
                                      });

                                    // Call custom handler if present
                                    if (hasCustomHandler && regDef->customUciHandler) {
                                      (*regDef->customUciHandler)(uciHandler);
                                    }
                                  });
        break;
      }

      case ConfigValueType::Double: {
        // Double values exposed as percentage integers in UCI (multiply by 100)
        const int defaultPct = static_cast<int>(parseDouble(def->defaultValue) * 100);
        const int minPct     = def->minValue.value_or(0);
        const int maxPct     = def->maxValue.value_or(200);

        optionVector.emplace_back(uciName.c_str(), defaultPct, minPct, maxPct,
                                  [configName, uciName, uciOptions, hasCustomHandler](UciHandler* uciHandler) {
                                    const UciOption* opt = uciOptions->getOption(uciName);
                                    if (!opt) return;
                                    const double value = UciOptions::getInt(opt->currentValue) / 100.0;

                                    const ConfigDef* regDef = ConfigRegistry::instance().find(configName);
                                    if (!regDef) return;

                                    ConfigManager::instance().applyOverrides(
                                      [regDef, value](SearchConfigData& s, EvalConfigData& e) {
                                        regDef->setter(s, e, std::to_string(value));
                                      });

                                    // Call custom handler if present
                                    if (hasCustomHandler && regDef->customUciHandler) {
                                      (*regDef->customUciHandler)(uciHandler);
                                    }
                                  });
        break;
      }

      case ConfigValueType::String: {
        optionVector.emplace_back(uciName.c_str(), def->defaultValue.c_str(),
                                  [configName, uciName, uciOptions, hasCustomHandler](UciHandler* uciHandler) {
                                    const UciOption* opt = uciOptions->getOption(uciName);
                                    if (!opt) return;
                                    const std::string valueStr = opt->currentValue;

                                    const ConfigDef* regDef = ConfigRegistry::instance().find(configName);
                                    if (!regDef) return;

                                    ConfigManager::instance().applyOverrides(
                                      [regDef, valueStr](SearchConfigData& s, EvalConfigData& e) {
                                        regDef->setter(s, e, valueStr);
                                      });

                                    // Call custom handler if present
                                    if (hasCustomHandler && regDef->customUciHandler) {
                                      (*regDef->customUciHandler)(uciHandler);
                                    }
                                  });
        break;
      }

      case ConfigValueType::Combo: {
        // Build a STRING option (UciOption doesn't have a vector-based COMBO constructor)
        // But we need to manually set comboVars after creation
        UciOption opt(uciName.c_str(), def->defaultValue.c_str(),
                      [configName, uciName, uciOptions, hasCustomHandler](UciHandler* uciHandler) {
                        const UciOption* o = uciOptions->getOption(uciName);
                        if (!o) return;
                        const std::string valueStr = o->currentValue;

                        const ConfigDef* regDef = ConfigRegistry::instance().find(configName);
                        if (!regDef) return;

                        ConfigManager::instance().applyOverrides(
                          [regDef, valueStr](SearchConfigData& s, EvalConfigData& e) {
                            regDef->setter(s, e, valueStr);
                          });

                        // Call custom handler if present
                        if (hasCustomHandler && regDef->customUciHandler) {
                          (*regDef->customUciHandler)(uciHandler);
                        }
                      });
        opt.comboVars = def->comboVars;
        optionVector.push_back(std::move(opt));
        break;
      }

      case ConfigValueType::IntArray:
        // Already skipped above, but handle for completeness
        break;
    }
  }
}

void addUciOnlyButtons(std::vector<UciOption>& optionVector, void* uciOptionsPtr) {
  // Clear Hash button - clears the transposition table
  optionVector.emplace_back("Clear Hash",
                            [](const UciHandler* uciHandler) {
                              if (uciHandler && uciHandler->getSearchPtr()) {
                                uciHandler->getSearchPtr()->clearTT();
                              }
                            });

  // Reset to Defaults button - resets all options to their default values
  auto* uciOptions = static_cast<UciOptions*>(uciOptionsPtr);
  optionVector.emplace_back("Reset to Defaults",
                            [uciOptions](UciHandler* uciHandler) {
                              uciOptions->resetToDefaults(uciHandler);
                            });
}
