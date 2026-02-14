# FrankyCPP Configuration System Refactoring Plan

**Document Version:** 1.9  
**Created:** 2026-02-13  
**Last Updated:** 2026-02-14  
**Status:** ✅ COMPLETE (All Phases 0-6)  
**Target:** FrankyCPP v1.3+  
**Priority:** Medium (Maintenance / Technical Debt Reduction)

---

## Coding Style Notes

> **⚠️ DO NOT USE NAMESPACES** - This project does not use C++ namespaces for its own code.
> All new code should be at global scope (or within classes/structs) to stay consistent
> with the existing codebase. This applies to all files created as part of this refactor.

---

## Executive Summary

Refactor FrankyCPP's configuration system to use a **single source of truth** for all configuration settings. Currently, configuration data is duplicated across multiple locations:
- `SearchConfigData.h` - struct member declarations + defaults
- `EvalConfigData.h` - struct member declarations + defaults
- YAML `encode()`/`decode()` functions - manual field mappings
- `UciOptions.cpp` - manual UCI option registration
- `str()` methods - incomplete manual string generation

This duplication leads to:
- **Missing configs in output** (the immediate trigger for this refactor)
- **Maintenance burden** - adding a new option requires changes in 4-6 places
- **Risk of inconsistency** - easy to add a field but forget UCI option or YAML support
- **No compile-time validation** - only runtime discovery of mismatches

### Goals

1. **Single source of truth** - Define each setting once with all metadata
2. **Generic runtime functions** - `str()`, YAML encode/decode, UCI options derived from metadata at runtime (NOT build-time code generation - just functions iterating over registry)
3. **Type-safe access** - Current `SEARCH_CONFIG.USE_NMP` syntax must continue to work
4. **Extensible domains** - Easy to add new configuration categories (e.g., "tuning", "debug")
5. **Backward compatibility** - Existing YAML files and UCI commands continue to work

---

## Current Architecture Analysis

### Problem 1: Multiple Sources of Truth

Adding `USE_SINGULAR_EXT` currently requires:

1. **SearchConfigData.h** - Add member with default:
   ```cpp
   bool USE_SINGULAR_EXT = true;
   ```

2. **SearchConfigData.h** - Add to `str()` method (often forgotten):
   ```cpp
   os << "USE_SINGULAR_EXT: " << USE_SINGULAR_EXT << '\n';
   ```

3. **SearchConfigData.h** - Add to YAML `encode()`:
   ```cpp
   n["USE_SINGULAR_EXT"] = c.USE_SINGULAR_EXT;
   ```

4. **SearchConfigData.h** - Add to YAML `decode()`:
   ```cpp
   set_if_present(n, "USE_SINGULAR_EXT", c.USE_SINGULAR_EXT, seen);
   ```

5. **UciOptions.cpp** - Add UCI option (if exposed via UCI):
   ```cpp
   optionVector.emplace_back(
     "Use Singular Extension", SearchConfig.USE_SINGULAR_EXT,
     [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_SINGULAR_EXT = ...); });
   ```

6. **config/search.yaml** - Add to config file (optional, uses default otherwise):
   ```yaml
   USE_SINGULAR_EXT: true
   ```

**Observation:** Steps 2-5 are mechanical and could be auto-generated.

### Problem 2: Incomplete `str()` Methods

Current `SearchConfigData::str()` only outputs ~12 of ~70+ fields:
```cpp
std::string str() const {
  std::ostringstream os;
  os << "MOVE_OVERHEAD_MS: " << MOVE_OVERHEAD_MS << '\n'
     << "USE_BOOK: " << USE_BOOK << '\n'
     // ... ~10 more fields
     << "USE_QS_CHECKS: " << USE_QS_CHECKS << '\n';
  return os.str();
}
```

Missing important fields like:
- All NMP settings
- All LMR/LMP settings
- All extension settings
- All instability settings
- Most evaluation settings

### Problem 3: UCI Options vs YAML Mismatch

Some settings are:
- **UCI only** - `Clear Hash` (button, no YAML equivalent)
- **YAML only** - `CONFIG_SOURCE` (internal tracking)
- **Both UCI and YAML** - Most search/eval parameters

No formal way to express this relationship.

---

## Proposed Solution: ConfigDef Metadata System

### Core Concept

Define each configuration setting as a **metadata descriptor** that includes:
- Name and type
- Default value
- Domain/category
- Exposure flags (UCI, YAML, internal-only)
- For numeric types: min/max bounds
- For strings: allowed values (combo)
- Human-readable description

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                     ConfigRegistry (Singleton)                      │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │  ConfigDef entries[]  (single source of truth + defaults)    │   │
│  │  ┌────────────────────────────────────────────────────────┐  │   │
│  │  │ ConfigDef { name, type, domain, default, uci, yaml, …} │  │   │
│  │  │ ConfigDef { name, type, domain, default, uci, yaml, …} │  │   │
│  │  │ ...                                                    │  │   │
│  │  └────────────────────────────────────────────────────────┘  │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────────┐     │
│  │ generateStr()  │  │  parseYAML()   │  │ generateUciOpts()  │     │
│  │ (for display)  │  │ (read config)  │  │ (UciOptions init)  │     │
│  └────────────────┘  └────────────────┘  └────────────────────┘     │
└─────────────────────────────────────────────────────────────────────┘
           │                    │                     │
           │                    │                     │
           ▼                    ▼                     ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     ConfigManager (Existing)                        │
│  ┌──────────────────┐  ┌──────────────────┐                         │
│  │ SearchConfigData │  │  EvalConfigData  │  (runtime value storage)│
│  │ (defaults from   │  │ (defaults from   │                         │
│  │  registry, then  │  │  registry, then  │                         │
│  │  YAML overrides) │  │  YAML overrides) │                         │
│  └──────────────────┘  └──────────────────┘                         │
└─────────────────────────────────────────────────────────────────────┘
           │                                          │
           │         ┌──────────────────┐             │
           └────────►│  YAML Files      │◄────────────┘
                     │  (optional)      │
                     │  Only overrides  │
                     └──────────────────┘
```

---

## Detailed Design

### Phase 1: ConfigDef Class

```cpp
// ConfigDef.h - Configuration Definition Metadata

#ifndef FRANKYCPP_CONFIGDEF_H
#define FRANKYCPP_CONFIGDEF_H

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace engine::config {

// Forward declarations
struct SearchConfigData;
struct EvalConfigData;

/// Configuration domain/category
enum class ConfigDomain {
  General,     // General engine settings (time overhead, book, etc.)
  Search,      // Search algorithm parameters
  Eval,        // Evaluation parameters
  Tuning,      // Parameters exposed for automated tuning (future)
  Debug        // Debug/development settings (not for production)
};

/// Value type for configuration settings
enum class ConfigValueType {
  Bool,        // Boolean (true/false)
  Int,         // Integer with optional min/max bounds
  Double,      // Floating point
  String,      // Free-form string
  Combo,       // Selection from predefined values
  IntArray     // Array of integers (e.g., margin tables) - special handling
};

/// Exposure flags - where this config can be set from
struct ConfigExposure {
  bool uci     = false;   // Exposed as UCI option
  bool yaml    = true;    // Loaded from YAML config file
  bool display = true;    // Shown in str() output
  bool tunable = false;   // Exposed to automated tuning (future)
};

/// Type-erased getter function: reads value from config structs, returns as string
using ConfigGetter = std::function<std::string(const SearchConfigData&, const EvalConfigData&)>;

/// Type-erased setter function: parses string value and writes to config structs
using ConfigSetter = std::function<void(SearchConfigData&, EvalConfigData&, const std::string&)>;

/// Configuration definition - single source of truth for one setting
struct ConfigDef {
  // Identity
  std::string name;           // Internal name (e.g., "USE_NMP")
  std::string uciName;        // UCI display name (e.g., "Use Null Move Pruning")
  std::string description;    // Human-readable description
  
  // Type information
  ConfigValueType valueType = ConfigValueType::Bool;
  ConfigDomain domain = ConfigDomain::General;
  
  // Default value (as string for simplicity)
  std::string defaultValue;
  
  // Bounds (for Int/Double types)
  std::optional<int> minValue;
  std::optional<int> maxValue;
  std::vector<std::string> comboVars;  // For Combo type
  
  // Exposure configuration
  ConfigExposure exposure;
  
  // Type-safe member access via getter/setter lambdas
  // These reference the actual struct members, providing compile-time validation
  ConfigGetter getter;  // Returns current value as string
  ConfigSetter setter;  // Parses string and sets value
  
  // Optional custom UCI handler (for special cases like "Clear Hash")
  std::optional<std::function<void(class UciHandler*)>> customUciHandler;
};

// Helper macros for concise getter/setter definition
#define CONFIG_GETTER_SEARCH(member) \
  [](const SearchConfigData& s, const EvalConfigData&) { return configToString(s.member); }
#define CONFIG_SETTER_SEARCH(member, parser) \
  [](SearchConfigData& s, EvalConfigData&, const std::string& v) { s.member = parser(v); }

#define CONFIG_GETTER_EVAL(member) \
  [](const SearchConfigData&, const EvalConfigData& e) { return configToString(e.member); }
#define CONFIG_SETTER_EVAL(member, parser) \
  [](SearchConfigData&, EvalConfigData& e, const std::string& v) { e.member = parser(v); }

// Value conversion helpers
inline std::string configToString(bool v) { return v ? "true" : "false"; }
inline std::string configToString(int v) { return std::to_string(v); }
inline std::string configToString(double v) { return std::to_string(v); }
inline std::string configToString(const std::string& v) { return v; }

inline bool parseBool(const std::string& v) { return v == "true" || v == "1"; }
inline int parseInt(const std::string& v) { return std::stoi(v); }
inline double parseDouble(const std::string& v) { return std::stod(v); }
inline std::string parseString(const std::string& v) { return v; }

} // namespace engine::config

#endif // FRANKYCPP_CONFIGDEF_H
```

### Phase 2: ConfigRegistry

```cpp
// ConfigRegistry.h - Central Registry of All Configuration Definitions

#ifndef FRANKYCPP_CONFIGREGISTRY_H
#define FRANKYCPP_CONFIGREGISTRY_H

#include "ConfigDef.h"
#include <span>
#include <string>

namespace engine::config {

/// Central registry of all configuration definitions.
/// This is THE single source of truth for which configs exist.
class ConfigRegistry {
public:
  static ConfigRegistry& instance();
  
  /// Get all config definitions
  [[nodiscard]] std::span<const ConfigDef> all() const;
  
  /// Get configs for a specific domain
  [[nodiscard]] std::vector<const ConfigDef*> byDomain(ConfigDomain domain) const;
  
  /// Get configs exposed via UCI
  [[nodiscard]] std::vector<const ConfigDef*> uciOptions() const;
  
  /// Get configs loaded from YAML
  [[nodiscard]] std::vector<const ConfigDef*> yamlOptions() const;
  
  /// Find config by internal name
  [[nodiscard]] const ConfigDef* find(const std::string& name) const;
  
  /// Find config by UCI name
  [[nodiscard]] const ConfigDef* findByUciName(const std::string& uciName) const;

private:
  ConfigRegistry();
  void initializeDefinitions();
  
  std::vector<ConfigDef> definitions_;
};

} // namespace engine::config

#endif // FRANKYCPP_CONFIGREGISTRY_H
```

### Phase 3: Definition Initialization (The Single Source of Truth)

Registry entries are defined **in the same file or adjacent to the struct** for visual proximity.
Each entry includes getter/setter lambdas that reference the actual struct member - this provides
**compile-time validation** (misspelled member = compilation error).

```cpp
// ConfigRegistry.cpp (partial - definition initialization)
// NOTE: This file should #include SearchConfigData.h and EvalConfigData.h
//       so developers see both struct and registry together

void ConfigRegistry::initializeDefinitions() {
  using enum ConfigValueType;
  using enum ConfigDomain;
  
  definitions_ = {
    //=========================================================================
    // GENERAL SETTINGS
    //=========================================================================
    {
      .name = "MOVE_OVERHEAD_MS",
      .uciName = "Move Overhead",
      .description = "Safety margin for time management (ms)",
      .valueType = Int,
      .domain = General,
      .defaultValue = "10",
      .minValue = 0,
      .maxValue = 5000,
      .exposure = {.uci = true, .yaml = true, .display = true},
      // Compile-time validated: if MOVE_OVERHEAD_MS doesn't exist, this fails to compile
      .getter = [](const auto& s, const auto&) { return std::to_string(s.MOVE_OVERHEAD_MS); },
      .setter = [](auto& s, auto&, const std::string& v) { s.MOVE_OVERHEAD_MS = std::stoi(v); }
    },
    {
      .name = "USE_BOOK",
      .uciName = "OwnBook",
      .description = "Use internal opening book",
      .valueType = Bool,
      .domain = General,
      .defaultValue = "true",
      .exposure = {.uci = true, .yaml = true, .display = true},
      .getter = [](const auto& s, const auto&) { return s.USE_BOOK ? "true" : "false"; },
      .setter = [](auto& s, auto&, const std::string& v) { s.USE_BOOK = (v == "true"); }
    },
    {
      .name = "BOOK_PATH",
      .uciName = "Book Path",
      .description = "Path to opening book file",
      .valueType = String,
      .domain = General,
      .defaultValue = "./books/book.txt",
      .exposure = {.uci = true, .yaml = true, .display = true},
      .getter = [](const auto& s, const auto&) { return s.BOOK_PATH; },
      .setter = [](auto& s, auto&, const std::string& v) { s.BOOK_PATH = v; }
    },
    
    //=========================================================================
    // SEARCH SETTINGS - Transposition Table
    //=========================================================================
    {
      .name = "USE_TT",
      .uciName = "Use Hash",
      .description = "Enable transposition table",
      .valueType = Bool,
      .domain = Search,
      .defaultValue = "true",
      .exposure = {.uci = true, .yaml = true, .display = true},
      .getter = [](const auto& s, const auto&) { return s.USE_TT ? "true" : "false"; },
      .setter = [](auto& s, auto&, const std::string& v) { s.USE_TT = (v == "true"); }
    },
    {
      .name = "TT_SIZE_MB",
      .uciName = "Hash",
      .description = "Transposition table size in MB",
      .valueType = Int,
      .domain = Search,
      .defaultValue = "64",
      .minValue = 0,
      .maxValue = 4096,
      .exposure = {.uci = true, .yaml = true, .display = true},
      .getter = [](const auto& s, const auto&) { return std::to_string(s.TT_SIZE_MB); },
      .setter = [](auto& s, auto&, const std::string& v) { s.TT_SIZE_MB = std::stoi(v); },
      // Custom UCI handler: need to resize TT after changing size
      .customUciHandler = [](UciHandler* h) { h->getSearchPtr()->resizeTT(); }
    },
    
    //=========================================================================
    // SEARCH SETTINGS - Null Move Pruning
    //=========================================================================
    {
      .name = "USE_NMP",
      .uciName = "Use Null Move Pruning",
      .description = "Enable null move pruning",
      .valueType = Bool,
      .domain = Search,
      .defaultValue = "true",
      .exposure = {.uci = true, .yaml = true, .display = true},
      .getter = [](const auto& s, const auto&) { return s.USE_NMP ? "true" : "false"; },
      .setter = [](auto& s, auto&, const std::string& v) { s.USE_NMP = (v == "true"); }
    },
    {
      .name = "NMP_DEPTH",
      .uciName = "Null Move Depth",
      .description = "Minimum depth for null move pruning",
      .valueType = Int,
      .domain = Search,
      .defaultValue = "3",
      .minValue = 0,
      .maxValue = 127,
      .exposure = {.uci = true, .yaml = true, .display = true},
      .getter = [](const auto& s, const auto&) { return std::to_string(s.NMP_DEPTH); },
      .setter = [](auto& s, auto&, const std::string& v) { s.NMP_DEPTH = std::stoi(v); }
    },
    // ... continue for all ~100+ settings
    
    //=========================================================================
    // EVAL SETTINGS (note: these use the EvalConfigData struct)
    //=========================================================================
    {
      .name = "USE_LAZY_EVAL",
      .uciName = "Use Lazy Eval",
      .description = "Enable lazy evaluation cutoff",
      .valueType = Bool,
      .domain = Eval,
      .defaultValue = "true",
      .exposure = {.uci = true, .yaml = true, .display = true},
      // Note: Eval settings use the second parameter (EvalConfigData)
      .getter = [](const auto&, const auto& e) { return e.USE_LAZY_EVAL ? "true" : "false"; },
      .setter = [](auto&, auto& e, const std::string& v) { e.USE_LAZY_EVAL = (v == "true"); }
    },
    {
      .name = "LAZY_THRESHOLD",
      .uciName = "Lazy Threshold",
      .description = "Lazy evaluation threshold in centipawns",
      .valueType = Int,
      .domain = Eval,
      .defaultValue = "700",
      .minValue = 0,
      .maxValue = 2000,
      .exposure = {.uci = true, .yaml = true, .display = true},
      .getter = [](const auto&, const auto& e) { return std::to_string(e.LAZY_THRESHOLD); },
      .setter = [](auto&, auto& e, const std::string& v) { e.LAZY_THRESHOLD = std::stoi(v); }
    },
    
    //=========================================================================
    // UCI-ONLY OPTIONS (no YAML, no struct member - action triggers)
    //=========================================================================
    {
      .name = "CLEAR_HASH",
      .uciName = "Clear Hash",
      .description = "Clear transposition table",
      .valueType = Bool,  // Button type
      .domain = General,
      .defaultValue = "false",
      .exposure = {.uci = true, .yaml = false, .display = false},
      .getter = [](const auto&, const auto&) { return ""; },  // No value
      .setter = [](auto&, auto&, const std::string&) { },     // No-op
      .customUciHandler = [](UciHandler* h) { h->getSearchPtr()->clearTT(); }
    },
    {
      .name = "RESET_DEFAULTS",
      .uciName = "Reset to Defaults",
      .description = "Reset all options to defaults",
      .valueType = Bool,  // Button type
      .domain = General,
      .defaultValue = "false",
      .exposure = {.uci = true, .yaml = false, .display = false},
      .getter = [](const auto&, const auto&) { return ""; },
      .setter = [](auto&, auto&, const std::string&) { },
      .customUciHandler = [](UciHandler* h) { /* reset logic */ }
    }
  };
}
```

**Key points:**
- Each getter/setter lambda references the actual struct member (e.g., `s.USE_NMP`)
- **Compile-time validation:** Typo in member name → compilation error
- Search configs use first param (`s`), Eval configs use second param (`e`)
- UCI-only buttons have no-op getter/setter but custom handler

### Phase 4: Generic Runtime Functions

These are **runtime functions** that iterate over the registry to produce output – no build-time code generation involved.

```cpp
// ConfigGenerators.h - Runtime functions that use registry to produce output

namespace engine::config {

/// Generate str() output for display (used by ConfigManager::strCurrent)
/// Iterates over ConfigRegistry::all() and formats each value
std::string generateConfigString(const SearchConfigData& search, 
                                  const EvalConfigData& eval,
                                  bool showAll = false);

/// Parse YAML node into config (returns false on error)
/// Uses registry to know which keys to expect and their types
std::set<std::string> parseYamlConfig(const YAML::Node& node, SearchConfigData& search);
std::set<std::string> parseYamlConfig(const YAML::Node& node, EvalConfigData& eval);

/// Initialize UCI options from registry (called by UciOptions::initOptions)
/// Creates UciOption objects by iterating registry entries with exposure.uci = true
void initUciOptionsFromRegistry(std::vector<UciOption>& optionVector);

} // namespace engine::config
```

---

## Implementation Phases

### Version Control Strategy
Each phase will be committed separately to enable easy rollback:
- **Pre-refactor baseline:** Clean git state before starting (current state)
- **Phase commits:** Each phase gets its own commit upon completion
- **Rollback:** `git reset --hard <commit>` to return to any phase boundary

---

### Phase 0: Directory Reorganization (Preparatory) ✅ COMPLETE
**Goal:** Move all config-related files to a dedicated `src/config/` folder before starting the refactor.

**Rationale:**
- Configuration is a cross-cutting concern, not specific to the engine module
- All config-related files in one place for easier navigation
- Natural home for new files (`ConfigDef.h`, `ConfigRegistry.h/cpp`, `ConfigGenerators.h/cpp`)
- Cleaner structure before adding new code

| Task                                                      | Effort | Notes                                      |
|-----------------------------------------------------------|--------|--------------------------------------------|
| Create `src/config/` directory                            | 0.1h   | New top-level source folder                |
| Move `src/engine/config/ConfigManager.h/cpp`              | 0.1h   | → `src/config/ConfigManager.h/cpp`         |
| Move `src/engine/config/SearchConfigData.h`               | 0.1h   | → `src/config/SearchConfigData.h`          |
| Move `src/engine/config/EvalConfigData.h`                 | 0.1h   | → `src/config/EvalConfigData.h`            |
| Update `#include` paths in all affected files             | 1h     | ~20-30 files reference these headers       |
| Update CMakeLists.txt if needed                           | 0.1h   | Glob patterns should auto-discover         |
| Verify build succeeds                                     | 0.5h   | Windows + WSL builds                       |

**Files to move:**
```
src/engine/config/ConfigManager.h    → src/config/ConfigManager.h
src/engine/config/ConfigManager.cpp  → src/config/ConfigManager.cpp
src/engine/config/SearchConfigData.h → src/config/SearchConfigData.h
src/engine/config/EvalConfigData.h   → src/config/EvalConfigData.h
```

**Namespace decision:** Keep `engine::config` namespace for now to minimize changes. Can revisit later.

**Note:** `UciOptions.h/cpp` stays permanently in `src/engine/`. Rationale:
- UCI options are engine-specific (part of UCI protocol handling)
- After Phase 4 refactoring, `UciOptions.cpp` will be minimal (~50-100 lines) - just calling `initUciOptionsFromRegistry()` and handling UCI-specific logic
- The config registry provides the data; UciOptions provides the UCI-specific presentation
- No need to move what will become a thin UCI adapter layer

**Deliverable:** Config files consolidated in `src/config/`. Build succeeds. No behavioral changes.

---

### Phase 1: Foundation (Low Risk) ✅ COMPLETE
**Goal:** Create ConfigDef and ConfigRegistry without changing existing behavior.

| Task                                          | Effort | Files                                       |
|-----------------------------------------------|--------|---------------------------------------------|
| Create `ConfigDef.h` with metadata structures | 1h     | `src/engine/config/ConfigDef.h`             |
| Create `ConfigRegistry.h/cpp` skeleton        | 1h     | `src/engine/config/ConfigRegistry.h/cpp`    |
| Populate definitions for Search configs       | 3h     | `ConfigRegistry.cpp`                        |
| Populate definitions for Eval configs         | 2h     | `ConfigRegistry.cpp`                        |
| Unit tests for registry                       | 1h     | `test/engine/config/ConfigRegistryTest.cpp` |

**Deliverable:** Registry exists but nothing uses it yet. No behavioral changes.

### Phase 2: str() Auto-Generation ✅ COMPLETE
**Goal:** Fix the immediate issue - complete `str()` output.

| Task                                               | Effort | Files                      |
|----------------------------------------------------|--------|----------------------------|
| Create `generateConfigString()` function           | 2h     | `ConfigGenerators.cpp`     |
| Replace `SearchConfigData::str()` to use generator | 0.5h   | `SearchConfigData.h`       |
| Replace `EvalConfigData::str()` to use generator   | 0.5h   | `EvalConfigData.h`         |
| Unit tests for ConfigGenerators                    | 1h     | `ConfigGeneratorsTest.cpp` |
| Verify output shows all configs                    | 0.5h   | Manual test                |

**Deliverable:** `SearchConfigData::str()` and `EvalConfigData::str()` output all ~117 settings grouped by domain.

### Phase 3: YAML Parsing Using Registry Metadata ✅ COMPLETE
**Goal:** Eliminate duplicate YAML decode code. (Note: We only *read* YAML, not write it.)

| Task                                                             | Effort | Files                      |
|------------------------------------------------------------------|--------|----------------------------|
| Create `parseYamlConfig()` function                              | 2h     | `ConfigGenerators.h/cpp`   |
| Update `YAML::convert<SearchConfigData>::decode` to use registry | 1h     | `SearchConfigData.h`       |
| Update `YAML::convert<EvalConfigData>::decode` to use registry   | 1h     | `EvalConfigData.h`         |
| Remove or simplify unused `encode()` functions                   | 0.5h   | `*ConfigData.h`            |
| Add unit tests for `parseYamlConfig()`                           | 1h     | `ConfigGeneratorsTest.cpp` |
| Verify existing YAML files still load correctly                  | 0.5h   | Manual test                |

**Deliverable:** YAML parsing derived from registry. Adding new field = one place only.

#### Phase 3 Implementation Details

##### 3.1 Create `parseYamlConfig()` Function

Add to `ConfigGenerators.h`:
```cpp
/// Parse YAML node into config structs using registry metadata.
/// Iterates registry entries with exposure.yaml=true and applies setters.
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
std::set<std::string> parseYamlConfig(
    const YAML::Node& node,
    SearchConfigData& search,
    bool warnUnknown = true);

/// Overload for Eval-only parsing (uses default SearchConfigData internally)
std::set<std::string> parseYamlConfig(
    const YAML::Node& node,
    EvalConfigData& eval,
    bool warnUnknown = true);
```

Implementation in `ConfigGenerators.cpp`:
```cpp
std::set<std::string> parseYamlConfig(
    const YAML::Node& node,
    SearchConfigData& search,
    EvalConfigData& eval,
    bool warnUnknown) {
    
  std::set<std::string> parsedKeys;
  const auto& registry = ConfigRegistry::instance();
  
  // Iterate all YAML-exposed config entries
  for (const auto* def : registry.yamlOptions()) {
    if (node[def->name]) {
      try {
        // Handle array types specially
        if (def->valueType == ConfigValueType::IntArray) {
          // Arrays stored as YAML sequences - convert to comma-separated string
          std::string value;
          if (node[def->name].IsSequence()) {
            for (std::size_t i = 0; i < node[def->name].size(); ++i) {
              if (i > 0) value += ",";
              value += node[def->name][i].as<std::string>();
            }
          } else {
            value = node[def->name].as<std::string>();
          }
          def->setter(search, eval, value);
        } else {
          // Scalar types
          const std::string value = node[def->name].as<std::string>();
          def->setter(search, eval, value);
        }
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
      const std::string key = kv.first.as<std::string>();
      if (parsedKeys.find(key) == parsedKeys.end()) {
        warnUnknownKey(key, "config");
      }
    }
  }
  
  return parsedKeys;
}
```

##### 3.2 Update YAML::convert Specializations

**Before (SearchConfigData.h)** - ~70 lines of manual field mapping:
```cpp
static bool decode(const Node& n, SearchConfigData& c) {
  std::set<std::string> seen;
  set_if_present(n, "MOVE_OVERHEAD_MS", c.MOVE_OVERHEAD_MS, seen);
  set_if_present(n, "USE_BOOK", c.USE_BOOK, seen);
  // ... 70+ more set_if_present calls
  warnUnseenKeys(n, seen, "Search");
  return true;
}
```

**After** - single function call:
```cpp
static bool decode(const Node& n, SearchConfigData& c) {
  parseYamlConfig(n, c);
  return true;
}
```

Same transformation applies to `EvalConfigData`.

##### 3.3 Simplify encode() Functions

The `encode()` functions are not used (we don't write YAML files programmatically).
Replace with minimal stub:

```cpp
static Node encode(const SearchConfigData&) {
  // YAML encoding not used - config files are manually maintained
  // Use generateConfigString() for human-readable output
  return Node();
}
```

##### 3.4 Special Cases

1. **Array types** (FP_MARGIN, LMP_MOVES, RFP_MARGIN, etc.)
   - YAML stores as sequences: `[0, 100, 200, 300]`
   - Registry setter expects comma-separated string: `"0,100,200,300"`
   - `parseYamlConfig()` handles conversion (see implementation above)

2. **Type validation**
   - Int values: Validate against minValue/maxValue if present
   - Bool values: Accept "true"/"false" and "1"/"0"
   - String values: No validation needed

3. **Unknown keys** 
   - Logged as warnings (existing behavior preserved)
   - Does not fail parsing (allows forward compatibility)

##### 3.5 Test Cases for `parseYamlConfig()`

```cpp
TEST_F(ConfigGeneratorsTest, ParseYamlConfigBasic) {
  YAML::Node node;
  node["USE_NMP"] = true;
  node["TT_SIZE_MB"] = 256;
  
  SearchConfigData search;
  auto parsed = parseYamlConfig(node, search);
  
  EXPECT_TRUE(search.USE_NMP);
  EXPECT_EQ(search.TT_SIZE_MB, 256);
  EXPECT_EQ(parsed.size(), 2);
}

TEST_F(ConfigGeneratorsTest, ParseYamlConfigUnknownKeyWarns) {
  YAML::Node node;
  node["UNKNOWN_KEY"] = "value";
  
  SearchConfigData search;
  // Should log warning but not fail
  auto parsed = parseYamlConfig(node, search);
  EXPECT_EQ(parsed.size(), 0);
}

TEST_F(ConfigGeneratorsTest, ParseYamlConfigArrayType) {
  YAML::Node node;
  node["FP_MARGIN"] = std::vector<int>{0, 150, 250, 350, 550, 950, 1250};
  
  SearchConfigData search;
  parseYamlConfig(node, search);
  
  EXPECT_EQ(search.FP_MARGIN[0], 0);
  EXPECT_EQ(search.FP_MARGIN[1], 150);
  // ... etc
}

TEST_F(ConfigGeneratorsTest, ParseYamlConfigMissingKeysUseDefaults) {
  YAML::Node node;  // Empty node
  
  SearchConfigData search;  // Has default values
  parseYamlConfig(node, search);
  
  // Defaults should be unchanged
  EXPECT_EQ(search.TT_SIZE_MB, 64);  // Default value
  EXPECT_TRUE(search.USE_NMP);       // Default value
}
```

##### 3.6 Migration Notes

- Existing `config/search.yaml` and `config/eval.yaml` files require **no changes**
- YAML key names match registry `name` field (already aligned)
- Unknown keys in user YAML files will warn but continue to work
- After migration: ~140 lines of `set_if_present()` calls removed

### Phase 4: UCI Auto-Generation ✅ COMPLETE
**Goal:** UCI options derived from registry.

| Task                                           | Effort | Files                  |
|------------------------------------------------|--------|------------------------|
| Create `initUciOptionsFromRegistry()` function | 3h     | `ConfigGenerators.cpp` |
| Create handler factory for common patterns     | 2h     | `ConfigGenerators.cpp` |
| Replace `UciOptions::initOptions()`            | 2h     | `UciOptions.cpp`       |
| Verify all UCI options still work              | 1h     | Manual test + UCITests |

**Implementation Notes:**
- Added `initUciOptionsFromRegistry()` and `addUciOnlyButtons()` to `ConfigGenerators.cpp`
- Replaced ~350 lines of manual option registration in `UciOptions.cpp` with ~15 lines
- All UCI-exposed config entries automatically generate UCI options based on type:
  - Bool → CHECK option
  - Int → SPIN option with min/max
  - Double → SPIN option (value × 100 as percentage)
  - String → STRING option
  - Combo → STRING option with comboVars
- Custom handlers (e.g., `resizeTT()` after Hash change) preserved via `customUciHandler`
- UCI-only buttons (Clear Hash, Reset to Defaults) added via `addUciOnlyButtons()`
- Fixed ~20 UCI name mismatches to maintain backward compatibility with existing GUIs
- Added `customUciHandler` to `TT_SIZE_MB` registry entry for `resizeTT()` call

**Deliverable:** UCI options derived from registry. ~350 lines of boilerplate removed.

### Phase 5: Cleanup & Documentation ✅ COMPLETE
**Goal:** Remove legacy code, update documentation.

| Task                                              | Effort | Files                             |
|---------------------------------------------------|--------|-----------------------------------|
| Remove manual field lists from ConfigData headers | 1h     | `*.h`                             |
| Update copilot-instructions.md with new pattern   | 0.5h   | `.github/copilot-instructions.md` |
| Update architecture documentation                 | 1h     | `docs/Architecture.md`            |
| Add developer guide for adding new configs        | 1h     | `docs/arena/Development.md`       |

**Implementation Notes:**
- **Phase 5.1:** Deleted `src/config/YamlHelpers.h` (unused after Phase 3)
- **Phase 5.2:** Consolidated string parsing into `stringutil.h`:
  - Added throwing variants: `parseInt()`, `parseDouble()`, `parseBool()`, `parseString()`
  - Added safe variants with logging: `parseIntOr()`, `parseDoubleOr()`
  - Added optional variants: `tryParseInt()`, `tryParseDouble()`
  - Removed duplicate functions from `ConfigDef.h`
  - Removed `UciOptions::getInt()` - replaced by `parseIntOr()`
  - Added 17 unit tests in `StringUtilsTest.cpp`
- **Phase 5.3:** Updated documentation:
  - `.github/copilot-instructions.md` - Updated "Modifying Configuration" section
  - `docs/Architecture.md` - Updated directory structure to show `src/config/`

**Deliverable:** Clean codebase with no unused code and up-to-date documentation.

### Phase 6: Enhanced Configuration Discovery (Optional) ✅ COMPLETE
**Goal:** Help users discover available settings without documentation.

| Task                                                        | Effort | Files                    | Status |
|-------------------------------------------------------------|--------|--------------------------|--------|
| Implement `--show-config` CLI command                       | 2h     | `main.cpp`               | ✅ Done |
| Add table formatter for config output                       | 1h     | `ConfigGenerators.cpp`   | ✅ Done |
| Add YAML template generator (commented, for reference)      | 1h     | `ConfigGenerators.cpp`   | ✅ Done |
| Add `--format` option (table, yaml, json)                   | 1h     | `main.cpp`               | ✅ Done |
| Add JSON output format (using nlohmann::json)               | 1h     | `ConfigGenerators.cpp`   | ✅ Done |
| Add `--domain` filter option                                | 0.5h   | `main.cpp`               | ✅ Done |
| Extend UCI with `extendedoptions` command                   | 1h     | `UciHandler.cpp`         | ✅ Done |
| Add `strExtended()` to UciOptions                           | 0.5h   | `UciOptions.cpp`         | ✅ Done |
| Move `truncate()` to stringutil.h as template               | 0.5h   | `stringutil.h`           | ✅ Done |
| Add CLI integration tests                                   | 1h     | `CliIntegrationTest.cpp` | ✅ Done |
| Add CMake dependency for test→main exe                      | 0.1h   | `test/CMakeLists.txt`    | ✅ Done |

**Implementation Notes:**
- Added `generateConfigTable()` - formats all settings in aligned columns
- Added `generateYamlTemplate()` - generates commented YAML for users to copy
- Added `generateConfigJson()` - machine-readable JSON using nlohmann::json library
- Added `parseDomainName()` - case-insensitive domain string parsing
- CLI options: `--show-config`, `--format <table|yaml|json>`, `--domain <name|all>`
- New UCI command: `extendedoptions` - shows options with default, current, min/max, and domain
- Added `UciOptions::strExtended()` for extended UCI output
- Moved `truncate()` to `stringutil.h` as template supporting string and string_view
- Added CLI integration tests (`test/cli/CliIntegrationTest.cpp`) covering:
  - `--help`, `-?`, `--version`, `-v`
  - `--show-config` with all format/domain combinations
  - `--ucioptions`, `-u`
  - `--perft` with depth options
  - `--bench` with benchDepth/benchHash options
  - Invalid option error handling
- Added CMake dependency so test executable automatically builds main executable
- Updated `TestEnginePath.h` with additional search paths for CI compatibility

**Usage examples:**
```bash
# Show all settings in table format (default)
FrankyCPP --show-config

# Show search settings only
FrankyCPP --show-config --domain search

# Generate YAML template for all settings
FrankyCPP --show-config --format yaml

# Generate JSON for tooling
FrankyCPP --show-config --format json --domain eval
```

**UCI examples:**
```
> extendedoptions
option name Book Path type string default ./books/book.txt current ./books/book.txt domain General
option name Clear Hash type button domain unknown
option name Hash type spin default 64 current 64 min 0 max 4096 domain Search
...
optionsok
```

**Deliverable:** Users can run `FrankyCPP --show-config` to see all available settings with defaults, bounds, and descriptions. Can generate YAML template on demand. CLI integration tests validate all CLI options.

---

## Technical Considerations

### Type-Safe Access (Critical Requirement)

**Priority order:**
1. **Minimal performance impact** - Direct struct member access is fastest (no maps/lookups in hot path)
2. **Ideally preserve syntax** - `SEARCH_CONFIG.USE_NMP` should continue to work
3. **Central maintenance** - Adding new config should be straightforward

Current code uses direct member access:
```cpp
if (SEARCH_CONFIG.USE_NMP && depth >= SEARCH_CONFIG.NMP_DEPTH) {
  // ...
}
```

This compiles to a simple memory load - **extremely fast**. Any registry lookup would be 10-100x slower.

**Solution: Mandatory Member Pointer in ConfigDef**

Each registry entry contains a **pointer-to-member** linking it to the actual struct field:

```cpp
struct ConfigDef {
  // ... other fields ...
  
  // Type-erased getter/setter using the actual struct member
  std::function<std::string(const SearchConfigData&, const EvalConfigData&)> getter;
  std::function<void(SearchConfigData&, EvalConfigData&, const std::string&)> setter;
};

// Usage in registry definition:
{
  .name = "USE_NMP",
  .uciName = "Use Null Move Pruning",
  .valueType = Bool,
  .domain = Search,
  .defaultValue = true,
  .getter = [](const auto& s, const auto&) { return s.USE_NMP ? "true" : "false"; },
  .setter = [](auto& s, auto&, const std::string& v) { s.USE_NMP = (v == "true"); },
  // ...
}
```

**Compile-time validation (Registry → Struct):**
- If `s.USE_NMP` doesn't exist or is misspelled, **compilation fails**
- The lambda captures a direct reference to the struct member
- No runtime overhead for hot path - registry is only used in cold paths (str(), YAML, UCI init)

**What this doesn't catch:**
- Struct member exists but not in registry (developer adds field, forgets registry entry)

**Mitigations for Struct → Registry direction:**

1. **Co-location:** Define registry entries in the same file as the struct (or adjacent), so when adding a struct member, the registry is visually nearby

2. **Exact sizeof() static_assert (primary safeguard):**
   ```cpp
   // In ConfigRegistry.cpp (same file as registry definitions)
   // These MUST match current struct sizes - update when adding/removing struct members!
   static_assert(sizeof(SearchConfigData) == 248, 
     "SearchConfigData size changed! Did you add/remove a member? "
     "Update registry entries AND this sizeof value.");
   static_assert(sizeof(EvalConfigData) == 192,
     "EvalConfigData size changed! Did you add/remove a member? "
     "Update registry entries AND this sizeof value.");
   ```
   
   **How it works:**
   - Developer adds new struct member (e.g., `bool USE_NEW_FEATURE`)
   - `sizeof(SearchConfigData)` changes (adding members increases size)
   - **Compilation fails** with reminder message
   - Developer updates sizeof value AND adds registry entry (since registry is in same file)
   - Compilation succeeds
   
   **Why sizeof(), not registry count:**
   - Registry count doesn't change when struct changes - no failure triggered
   - sizeof() directly monitors the struct, catching the actual problem
   - Even with padding, adding members almost always changes sizeof()
   
   **Platform differences:**
   - sizeof() may vary slightly across platforms due to alignment
   - Can be mitigated with `#ifdef` if needed:
     ```cpp
     #ifdef _MSC_VER
     static_assert(sizeof(SearchConfigData) == 248, "...");
     #elif defined(__GNUC__)
     static_assert(sizeof(SearchConfigData) == 240, "...");
     #endif
     ```
   - In practice, differences are rare for simple structs with standard types

3. **Unit test as backup:**
   ```cpp
   TEST(ConfigRegistry, AllStructMembersHaveRegistryEntries) {
     // Verify registry entry count is reasonable (catches major drift)
     EXPECT_GE(ConfigRegistry::instance().searchConfigCount(), 60);
     EXPECT_GE(ConfigRegistry::instance().evalConfigCount(), 50);
   }
   ```

**Limitation acknowledged:** C++ cannot count struct members automatically (no reflection until C++26). The sizeof() approach is the best available proxy - it catches most struct changes and creates a checkpoint that reminds developers to update the registry.

**Benefits of this approach:**
- ✅ Zero performance impact on hot path (direct `struct.member` access unchanged)
- ✅ Compile-time validation (registry → struct direction)
- ✅ Standard C++ (no macros, no UB, IDE-friendly)
- ✅ Generic str()/YAML/UCI without manual field lists
- ✅ Syntax unchanged: `SEARCH_CONFIG.USE_NMP` still works

**Trade-off:**
- ⚠️ Still two places to maintain (struct + registry), but now **linked via member reference**
- ⚠️ Struct → registry direction relies on developer discipline + visual proximity + tests

### Handler Functions for UCI

Current UCI handlers have custom logic for some options:
```cpp
// Hash option needs to call resizeTT()
[&](const UciHandler* uciHandler) {
  CONFIG_OVERRIDE(s.TT_SIZE_MB = getInt(...); uciHandler->getSearchPtr()->resizeTT(););
}
```

Solution: Registry includes optional handler override:
```cpp
struct ConfigDef {
  // ...
  std::optional<std::function<void(UciHandler*)>> customHandler;
};
```

Most options use default handler; only special cases override.

### Array Types (FP_MARGIN, LMP_MOVES, etc.)

Current arrays like `std::array<int, 7> FP_MARGIN` are included with simple handling:
- **YAML:** Read/write as sequences (existing behavior preserved)
- **UCI:** Not exposed (too complex for `setoption` syntax)
- **str():** Display as comma-separated list

**Implementation:** Use `ConfigValueType::IntArray` with specialized getter/setter:

```cpp
{
  .name = "FP_MARGIN",
  .uciName = "",  // Not exposed via UCI
  .description = "Futility pruning margins by depth",
  .valueType = IntArray,
  .domain = Search,
  .defaultValue = "0,100,200,300,500,900,1200",
  .exposure = {.uci = false, .yaml = true, .display = true},
  .getter = [](const auto& s, const auto&) { 
    return arrayToString(s.FP_MARGIN);  // "0,100,200,300,500,900,1200"
  },
  .setter = [](auto& s, auto&, const std::string& v) { 
    parseArray(v, s.FP_MARGIN);  // Parse comma-separated values
  }
}
```

Helper functions:
```cpp
template<std::size_t N>
std::string arrayToString(const std::array<int, N>& arr) {
  std::ostringstream oss;
  for (std::size_t i = 0; i < N; ++i) {
    if (i > 0) oss << ",";
    oss << arr[i];
  }
  return oss.str();
}

template<std::size_t N>
void parseArray(const std::string& str, std::array<int, N>& arr) {
  std::istringstream iss(str);
  std::string token;
  std::size_t i = 0;
  while (std::getline(iss, token, ',') && i < N) {
    arr[i++] = std::stoi(token);
  }
}
```

### Migration Strategy

1. **Phase 1-2 are additive** - No breaking changes
2. **Phase 3-4 are replacements** - Gradual migration possible
3. **Each phase independently testable**
4. **Rollback possible** - Keep old code commented until confident

---

## Alternative Approaches Considered

### A: Macro-Based Definition (Rejected)

```cpp
#define CONFIG_DEF(type, name, default, ...) \
  static constexpr ConfigDef name##_DEF = {...}; \
  type name = default;
```

**Pros:** Single declaration
**Cons:** Macro complexity, harder to debug, IDE unfriendly

### B: External Code Generator (Rejected)

Generate C++ from YAML/JSON definition file.

**Pros:** Clean separation
**Cons:** Build complexity, extra tooling, harder to maintain

### C: Full Reflection (Future C++26)

Wait for C++ reflection proposal.

**Pros:** Language-native solution
**Cons:** Years away, uncertain timeline

### D: Template Metaprogramming (Considered)

Use `std::tuple` of typed values with compile-time iteration.

**Pros:** Type-safe at compile time
**Cons:** Complex template code, slower compilation

**Recommendation:** Keep it simple with runtime registry (Option A from current proposal).

---

## Success Criteria

### Functional
- [ ] `ConfigManager::strCurrent()` outputs ALL configuration values
- [ ] Adding new config requires changes in TWO linked places: struct member + registry entry (with compile-time validation ensuring they match)
- [ ] Existing YAML config files load without changes
- [ ] All UCI options continue to work
- [ ] Existing code using `SEARCH_CONFIG.X` unchanged
- [ ] Misspelled member in registry → compilation error

### Code Quality
- [ ] Net reduction in lines of code (~500+ lines removed from str(), YAML encode/decode, UCI init)
- [ ] No raw string duplication of config names (each name defined once in registry)
- [ ] Clear documentation for adding new configs
- [ ] Registry and struct co-located for visual proximity

### Testing
- [ ] Unit tests for ConfigRegistry
- [ ] Exact sizeof() static_assert for SearchConfigData and EvalConfigData (compilation fails if struct changes)
- [ ] Unit test validating minimum registry entry count (catches major drift)
- [ ] Integration tests for YAML round-trip
- [ ] UCI option tests pass
- [ ] Arena bench shows complete config output

---

## Risks and Mitigations

| Risk                                               | Impact | Mitigation                               |
|----------------------------------------------------|--------|------------------------------------------|
| Subtle behavioral changes in YAML parsing          | Medium | Extensive before/after comparison        |
| UCI option handler logic lost in translation       | High   | Keep custom handlers, verify each option |
| Compile-time errors from struct offset computation | Low    | Use simpler approach if needed           |
| Performance overhead from indirection              | Low    | Profile; registry is not hot path        |

---

## Open Questions for Discussion

### Resolved

1. **Should array types (FP_MARGIN, etc.) be exposed via UCI?**
   - **Decision: Include as-is.** Array types (e.g., `FP_MARGIN`, `LMP_MOVES`, `RFP_MARGIN`) will be included in the registry with their current simple handling:
     - YAML: Read/write as sequences (existing behavior)
     - UCI: Not exposed (too complex for `setoption` syntax)
     - str(): Display as comma-separated list
   - Special getter/setter lambdas will handle the array ↔ string conversion.

2. **Should we support YAML comments in generated output?**
   - **Decision: Defer.** Can add later if useful.

3. **Do we want compile-time validation?**
   - **Decision: Defer.** Can add static_asserts later if needed.

4. **Should domain categorization affect YAML file structure?**
   - **Decision: Yes.** Domains should map to YAML structure. Details TBD during implementation.

5. **Export format for automated tuning (SPSA, etc.)?**
   - **Decision: Defer.** Future feature, not in scope for initial refactor.

### Clarification: YAML Generation

**Q: Do we actually generate YAML output?**

**A: No.** Currently we only *read* YAML config files, we don't *write* them. The "generateYamlNode" terminology in Phase 3 is misleading.

What we actually need:
- **YAML decode (read):** Parse YAML file → populate ConfigData struct ✅ (needed)
- **YAML encode (write):** ConfigData struct → YAML file ❌ (not used currently)

**Updated Phase 3 scope:** Focus only on `parseYamlConfig()` (decoding). The `encode()` functions in the current YAML converters are unused and can be removed or left as-is.

---

### Design Decision: YAML is Optional and Additive

**Principle:** YAML files are purely optional overrides, not required configuration.

**How it works:**
1. **Registry holds all defaults** - Every setting has a default value in ConfigDef
2. **Missing YAML = all defaults** - Engine works perfectly with no YAML file at all
3. **Empty YAML = all defaults** - Same as missing file
4. **Partial YAML = selective overrides** - Only settings present in YAML override defaults
5. **No "master template" needed** - Users don't need a complete example YAML to maintain

**Example: Minimal user YAML**
```yaml
# config/search.yaml - User only changes what they want
TT_SIZE_MB: 256
USE_SINGULAR_EXT: false
```
All other ~100 settings use registry defaults.

**Benefits:**
- No template YAML to maintain (avoids sync issues)
- Users add only what they care about
- Upgrading engine versions "just works" (new settings get defaults)
- Clear separation: registry = source of truth, YAML = user overrides

---

### Design Decision: Enhanced Configuration Discovery

**Problem:** How do users discover available settings without a template YAML?

**Solution:** Enhance the CLI/UCI to provide complete configuration information.

**Proposed CLI command:** `--show-config` or `--options`

```
FrankyCPP --show-config [--domain <domain>] [--format <format>]

Options:
  --domain    Filter by domain: general, search, eval, all (default: all)
  --format    Output format: table, yaml, json (default: table)
```

**Example output (table format):**
```
Domain   Name                   Type   Default  Current  Min   Max   UCI Name
──────── ────────────────────── ────── ──────── ──────── ───── ───── ─────────────────────────
General  MOVE_OVERHEAD_MS       int    10       10       0     5000  Move Overhead
General  USE_BOOK               bool   true     true     -     -     OwnBook
General  BOOK_PATH              string ./books/ ./books/ -     -     Book Path
Search   USE_TT                 bool   true     true     -     -     Use Hash
Search   TT_SIZE_MB             int    64       256      0     4096  Hash
Search   USE_NMP                bool   true     true     -     -     Use Null Move Pruning
Search   NMP_DEPTH              int    3        3        0     127   Null Move Depth
...
```

**Example output (yaml format):** Generates a complete YAML template
```yaml
# FrankyCPP Configuration - All Available Settings
# Generated from ConfigRegistry - copy settings you want to override

# === General ===
# MOVE_OVERHEAD_MS: 10      # Safety margin for time management (ms) [0-5000]
# USE_BOOK: true            # Use internal opening book
# BOOK_PATH: ./books/book.txt  # Path to opening book file

# === Search ===
# USE_TT: true              # Enable transposition table
# TT_SIZE_MB: 64            # Transposition table size in MB [0-4096]
...
```

**UCI enhancement:** Extend `getoptions` or add new command
```
> getoptions
option name Move Overhead type spin default 10 current 10 min 0 max 5000 domain general
option name OwnBook type check default true current true domain general
option name Hash type spin default 64 current 256 min 0 max 4096 domain search
...
```

**Benefits:**
- Users can discover ALL settings without documentation
- Can generate a YAML template on demand (but don't need to maintain one)
- Shows current vs default to identify what's been changed
- Domain filtering helps users find relevant settings

---

## Design Decisions

### CLI is NOT a ConfigDomain (Decided: Keep Separate)

**Decision:** Command-line arguments are intentionally excluded from the ConfigRegistry.

**Rationale:**
1. **Different lifecycle** - CLI args are per-invocation; config settings persist across runs
2. **Different purpose** - CLI controls "what mode to run" (bench, testsuites, matches); config controls "how the engine behaves" (search params, eval weights)
3. **No overlap** - CLI args like `--help`, `--config`, `--testsuites` have no UCI/YAML equivalent
4. **Already solved** - Boost.Program_Options handles CLI parsing well; no duplication issue

**One-off CLI flags are still allowed:**
- Convenience flags like `--nobook` that map to a specific config value are fine
- These are hardcoded shortcuts, not a systematic CLI-config integration
- Example: `--nobook` → `CONFIG_OVERRIDE(s.USE_BOOK = false;)`

**Future consideration:** If we ever want systematic `--option=value` CLI overrides for all config values, we could add a `ConfigExposure.cli` flag. Not needed for current goals.

---

## Appendix: Current Config Count

| Category            | Count    | Notes                     |
|---------------------|----------|---------------------------|
| Search general      | 6        | book, ponder, etc.        |
| Search TT           | 6        | hash size, use flags      |
| Search move sorting | 7        | killers, history, IID     |
| Search pruning      | 16       | NMP, FP, RFP, razoring    |
| Search LMR/LMP      | 5        | reduction/pruning params  |
| Search extensions   | 8        | check, singular, etc.     |
| Search time mgmt    | 14       | moves-left, instability   |
| Eval general        | 6        | lazy, tempo, etc.         |
| Eval pawn           | 14       | weights for pawn features |
| Eval pieces         | 35       | mobility, bonuses, etc.   |
| **Total**           | **~117** |                           |

---

## Completion Summary

**All phases completed on 2026-02-14.**

### Files Created
- `src/config/ConfigDef.h` - Configuration metadata structures
- `src/config/ConfigRegistry.h/.cpp` - Central registry singleton
- `src/config/ConfigGenerators.h/.cpp` - Auto-generated str(), YAML, UCI, table, JSON
- `test/cli/CliIntegrationTest.cpp` - CLI integration tests

### Files Modified
- `src/config/SearchConfigData.h` - Uses registry for str()
- `src/config/EvalConfigData.h` - Uses registry for str()
- `src/config/ConfigManager.h/.cpp` - Uses parseYamlConfig()
- `src/engine/UciOptions.cpp` - Uses initUciOptionsFromRegistry()
- `src/engine/UciHandler.h/.cpp` - Added `extendedoptions` command
- `src/common/stringutil.h` - Added `truncate()` template, parsing functions
- `src/main.cpp` - Added `--show-config`, `--format`, `--domain` options
- `test/CMakeLists.txt` - Added CLI tests and exe dependency
- `test/TestEnginePath.h` - Added CI-compatible search paths
- `.github/copilot-instructions.md` - Updated configuration docs

### Files Deleted
- `src/config/YamlHelpers.h` - Replaced by ConfigGenerators

### Key Achievements
1. **Single source of truth** - All configs defined once in ConfigRegistry
2. **Compile-time validation** - Misspelled members cause compilation errors
3. **Auto-generated output** - str(), YAML parsing, UCI options from registry
4. **~500 lines removed** - Eliminated duplicate field mappings
5. **Configuration discovery** - `--show-config` CLI with table/yaml/json formats
6. **CLI integration tests** - Automated testing of all CLI options

---

*Last updated: 2026-02-14*
