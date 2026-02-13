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

#ifndef FRANKYCPP_CONFIGREGISTRY_H
#define FRANKYCPP_CONFIGREGISTRY_H

//=============================================================================
// ConfigRegistry.h - Central Registry of All Configuration Definitions
//=============================================================================
//
// Singleton registry holding all config definitions (ConfigDef) for the engine.
// This is THE single source of truth for which configuration settings exist.
// Depends on: ConfigDef.h, SearchConfigData.h, EvalConfigData.h
//
// Purpose:
//   - Centralize config metadata (name, type, bounds, exposure flags)
//   - Provide compile-time validation via getter/setter lambdas
//   - Enable generic str(), YAML parsing, and UCI option generation
//
// Query Methods:
//   all()            - Get all config definitions
//   byDomain()       - Filter by ConfigDomain (General, Search, Eval, etc.)
//   uciOptions()     - Get configs exposed via UCI protocol
//   yamlOptions()    - Get configs loaded from YAML files
//   displayOptions() - Get configs shown in str() output
//   find()           - Lookup by internal name (case-sensitive)
//   findByUciName()  - Lookup by UCI name (case-insensitive)
//
// Validation:
//   searchConfigCount() - Count of Search/General configs
//   evalConfigCount()   - Count of Eval configs
//   totalCount()        - Total config count
//
// Usage:
//   auto& registry = ConfigRegistry::instance();
//   for (const auto& def : registry.all()) {
//     std::cout << def.name << " = " << def.getter(search, eval) << "\n";
//   }
//
//   const ConfigDef* nmp = registry.find("USE_NMP");
//   if (nmp) nmp->setter(search, eval, "false");
//
//=============================================================================

#include "config/ConfigDef.h"

#include <span>
#include <string>
#include <vector>

class ConfigRegistry {
public:
  /// Get the singleton instance
  [[nodiscard]] static ConfigRegistry& instance();

  // Non-copyable, non-movable
  ConfigRegistry(const ConfigRegistry&)            = delete;
  ConfigRegistry& operator=(const ConfigRegistry&) = delete;
  ConfigRegistry(ConfigRegistry&&)                 = delete;
  ConfigRegistry& operator=(ConfigRegistry&&)      = delete;

  /// Get all config definitions
  [[nodiscard]] std::span<const ConfigDef> all() const;

  /// Get configs for a specific domain
  [[nodiscard]] std::vector<const ConfigDef*> byDomain(ConfigDomain domain) const;

  /// Get configs exposed via UCI
  [[nodiscard]] std::vector<const ConfigDef*> uciOptions() const;

  /// Get configs loaded from YAML
  [[nodiscard]] std::vector<const ConfigDef*> yamlOptions() const;

  /// Get configs that should be shown in str() output
  [[nodiscard]] std::vector<const ConfigDef*> displayOptions() const;

  /// Find config by internal name (case-sensitive)
  [[nodiscard]] const ConfigDef* find(const std::string& name) const;

  /// Find config by UCI name (case-insensitive)
  [[nodiscard]] const ConfigDef* findByUciName(const std::string& uciName) const;

  /// Get count of Search config entries (for validation)
  [[nodiscard]] std::size_t searchConfigCount() const;

  /// Get count of Eval config entries (for validation)
  [[nodiscard]] std::size_t evalConfigCount() const;

  /// Get total count of all config entries
  [[nodiscard]] std::size_t totalCount() const { return definitions_.size(); }

private:
  ConfigRegistry();
  ~ConfigRegistry() = default;

  void initializeSearchDefinitions();
  void initializeEvalDefinitions();

  std::vector<ConfigDef> definitions_;
};

#endif  // FRANKYCPP_CONFIGREGISTRY_H
