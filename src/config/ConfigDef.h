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

#ifndef FRANKYCPP_CONFIGDEF_H
#define FRANKYCPP_CONFIGDEF_H

//=============================================================================
// ConfigDef.h - Configuration Definition Metadata
//=============================================================================
//
// Defines the metadata structures for configuration settings, providing
// a single source of truth for all engine configuration parameters.
//
// Enums:
//   ConfigDomain    - Category of setting (General, Search, Eval, etc.)
//   ConfigValueType - Data type (Bool, Int, Double, String, IntArray)
//
// Structs:
//   ConfigExposure  - Visibility flags (UCI, YAML, display)
//   ConfigDef       - Complete definition of one config setting
//
// Type Aliases:
//   ConfigGetter    - Function to read a config value as string
//   ConfigSetter    - Function to parse string and set config value
//   UciHandlerFunc  - Custom UCI handler for special options
//
// Helper Functions:
//   configToString()  - Convert typed value to string
//   parseBool/Int/Double/String() - Parse string to typed value
//   arrayToString()   - Convert int array to comma-separated string
//   parseArray()      - Parse comma-separated string to int array
//
// Usage:
//   ConfigDef def = {
//     .name = "USE_NMP",
//     .uciName = "Use Null Move Pruning",
//     .valueType = ConfigValueType::Bool,
//     .domain = ConfigDomain::Search,
//     .defaultValue = "true",
//     .exposure = {.uci = true, .yaml = true, .display = true},
//     .getter = [](const auto& s, const auto&) { return configToString(s.USE_NMP); },
//     .setter = [](auto& s, auto&, const std::string& v) { s.USE_NMP = parseBool(v); }
//   };
//
//=============================================================================

#include <array>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "common/stringutil.h"

// Forward declarations
namespace engine {
  class UciHandler;
}

namespace config {

  struct SearchConfigData;
  struct EvalConfigData;

  /// Configuration domain/category
  enum class ConfigDomain {
    General, // General engine settings (time overhead, book, etc.)
    Search,  // Search algorithm parameters
    Eval,    // Evaluation parameters
    Tuning,  // Parameters exposed for automated tuning (future)
    Debug    // Debug/development settings (not for production)
  };

  /// Convert ConfigDomain to string for display
  [[nodiscard]] inline std::string domainToString(const ConfigDomain domain) {
    switch (domain) {
      case ConfigDomain::General:
        return "General";
      case ConfigDomain::Search:
        return "Search";
      case ConfigDomain::Eval:
        return "Eval";
      case ConfigDomain::Tuning:
        return "Tuning";
      case ConfigDomain::Debug:
        return "Debug";
    }
    return "Unknown";
  }

  /// Value type for configuration settings
  enum class ConfigValueType {
    Bool,    // Boolean (true/false)
    Int,     // Integer with optional min/max bounds
    Double,  // Floating point
    String,  // Free-form string
    Combo,   // Selection from predefined values
    IntArray // Array of integers (e.g., margin tables) - special handling
  };

  /// Convert ConfigValueType to string for display
  [[nodiscard]] inline std::string valueTypeToString(const ConfigValueType type) {
    switch (type) {
      case ConfigValueType::Bool:
        return "bool";
      case ConfigValueType::Int:
        return "int";
      case ConfigValueType::Double:
        return "double";
      case ConfigValueType::String:
        return "string";
      case ConfigValueType::Combo:
        return "combo";
      case ConfigValueType::IntArray:
        return "int[]";
    }
    return "unknown";
  }

  /// Exposure flags - where this config can be set from
  struct ConfigExposure {
    bool uci     = false; // Exposed as UCI option
    bool yaml    = true;  // Loaded from YAML config file
    bool display = true;  // Shown in str() output
    bool tunable = false; // Exposed to automated tuning (future)
  };

  /// Type-erased getter function: reads value from config structs, returns as string
  using ConfigGetter = std::function<std::string(const SearchConfigData&, const EvalConfigData&)>;

  /// Type-erased setter function: parses string value and writes to config structs
  using ConfigSetter = std::function<void(SearchConfigData&, EvalConfigData&, const std::string&)>;

  /// Custom UCI handler function type
  using UciHandlerFunc = std::function<void(engine::UciHandler*)>;

  /// Configuration definition - single source of truth for one setting
  struct ConfigDef {
    // Identity
    std::string name;        // Internal name (e.g., "USE_NMP")
    std::string uciName;     // UCI display name (e.g., "Use Null Move Pruning")
    std::string description; // Human-readable description

    // Type information
    ConfigValueType valueType = ConfigValueType::Bool;
    ConfigDomain domain       = ConfigDomain::General;

    // Default value (as string for simplicity)
    std::string defaultValue;

    // Bounds (for Int/Double types)
    std::optional<int> minValue;
    std::optional<int> maxValue;
    std::vector<std::string> comboVars; // For Combo type

    // Exposure configuration
    ConfigExposure exposure;

    // Type-safe member access via getter/setter lambdas
    // These reference the actual struct members, providing compile-time validation
    ConfigGetter getter; // Returns current value as string
    ConfigSetter setter; // Parses string and sets value

    // Optional custom UCI handler (for special cases like "Clear Hash")
    std::optional<UciHandlerFunc> customUciHandler;
  };

  //=============================================================================
  // Value conversion helpers
  //=============================================================================

  /// Convert bool to string
  [[nodiscard]] inline std::string configToString(const bool v) {
    return v ? "true" : "false";
  }

  /// Convert int to string
  [[nodiscard]] inline std::string configToString(const int v) {
    return std::to_string(v);
  }

  /// Convert double to string
  [[nodiscard]] inline std::string configToString(const double v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
  }

  /// Convert string to string (identity)
  [[nodiscard]] inline std::string configToString(const std::string& v) {
    return v;
  }

  /// Convert array to comma-separated string
  template<std::size_t N>
  [[nodiscard]] std::string arrayToString(const std::array<int, N>& arr) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < N; ++i) {
      if (i > 0) oss << ",";
      oss << arr[i];
    }
    return oss.str();
  }

  /// Parse comma-separated string to array
  template<std::size_t N>
  void parseArray(const std::string& str, std::array<int, N>& arr) {
    std::istringstream iss(str);
    std::string token;
    std::size_t i = 0;
    while (std::getline(iss, token, ',') && i < N) {
      const std::string trimmed = common::trimFast(token);
      if (!trimmed.empty()) {
        arr[i++] = std::stoi(trimmed);
      }
    }
  }

} // namespace config

#endif // FRANKYCPP_CONFIGDEF_H
