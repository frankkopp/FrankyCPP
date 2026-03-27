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

#include "tuning/optimizer/TuningParameter.h"

#include "config/ConfigDef.h"
#include "config/ConfigRegistry.h"
#include "config/EvalConfigData.h"
#include "config/SearchConfigData.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace tuning {

  // =========================================================================
  // Parameter group assignment
  // =========================================================================

  // Maps config name prefixes to parameter group indices.
  // Groups correspond to eval categories for activation-flag optimization (Phase 6.5).
  // The group index maps to a bit in TuningEntry::activeParamGroups.
  //
  // Group assignment:
  //   0  = Tempo / lazy eval / misc
  //   1  = Pawn structure (isolated, doubled, blocked, phalanx, supported)
  //   2  = Passed pawns (flat weight + rank bonus)
  //   3  = Pawn advancement bonus
  //   4  = Bishop pair
  //   5  = Knight (mobility, low mobility, outpost)
  //   6  = Bishop (mobility, low mobility, bad bishop)
  //   7  = Rook (mobility, open file, 7th rank, behind passer)
  //   8  = Queen (mobility, tropism)
  //   9  = King safety (shield, proximity, attack weights, safety table, pawn storm, open file, safe check)
  //   10 = Threats (by pawn, by minor, hanging)
  //   11 = Space
  //   12 = Coordination (connected rooks, minor connectivity)
  //   13–15 = Reserved for future use

  int TuningParameter::assignParamGroup(const std::string& configName) {
    // Use a prefix-matching approach. Order matters: check longer/more-specific prefixes first.
    // clang-format off
    static const std::vector<std::pair<std::string, int>> prefixMap = {
      // Group 0: Tempo / lazy / misc
      {"TEMPO",                       0},
      {"LAZY_THRESHOLD",              0},
      // Group 1: Pawn structure
      {"ISOLATED_PAWN",               1},
      {"DOUBLED_PAWN",                1},
      {"BLOCKED_PAWN",                1},
      {"PHALANX_PAWN",                1},
      {"SUPPORTED_PAWN",              1},
      // Group 2: Passed pawns
      {"PASSED_PAWN",                 2},
      // Group 3: Pawn advancement
      {"PAWN_ADVANCE",                3},
      // Group 4: Bishop pair
      {"BISHOP_PAIR",                 4},
      // Group 5: Knight
      {"KNIGHT",                      5},
      // Group 6: Bishop (must come after BISHOP_PAIR check)
      {"BISHOP",                      6},
      {"BAD_BISHOP",                  6},
      // Group 7: Rook
      {"ROOK",                        7},
      // Group 8: Queen
      {"QUEEN",                       8},
      // Group 9: King safety (all king-related params)
      {"KING",                        9},
      {"PAWN_STORM",                  9},
      {"SAFE_CHECK",                  9},
      // Group 10: Threats
      {"THREAT",                      10},
      // Group 11: Space
      {"SPACE",                       11},
      // Group 12: Coordination
      {"CONNECTED_ROOKS",             12},
      {"MINOR_CONNECTIVITY",          12},
    };
    // clang-format on

    for (const auto& [prefix, group] : prefixMap) {
      if (configName.starts_with(prefix)) {
        return group;
      }
    }

    // Fallback: unmapped parameters go to group 0 (misc)
    return 0;
  }

  // =========================================================================
  // Monotonicity constraint assignment
  // =========================================================================

  // Known array parameters that require monotonicity constraints.
  // All currently known arrays should be non-decreasing (higher index = more value/danger).
  void TuningParameter::assignMonotonicity(TuningParameter& param) {
    if (param.arrayIndex < 0) return; // scalars have no constraint

    static const std::unordered_set<std::string> nonDecreasing = {
      "KING_SAFETY_TABLE",
      "PASSED_PAWN_RANK_MID_BONUS",
      "PASSED_PAWN_RANK_END_BONUS",
      "PAWN_ADVANCE_MID_BONUS",
      "PAWN_ADVANCE_END_BONUS",
      "PAWN_STORM_MID_PENALTY",
    };

    if (nonDecreasing.contains(param.configName)) {
      param.monotonicity = MonotonicityConstraint::NON_DECREASING;
    }
  }

  // =========================================================================
  // Parse helpers for array values
  // =========================================================================

  namespace {

    /// Parses a comma-separated string of integers into a vector.
    std::vector<int> parseIntArray(const std::string& str) {
      std::vector<int> values;
      std::istringstream iss(str);
      std::string token;
      while (std::getline(iss, token, ',')) {
        // Trim whitespace
        const auto start = token.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        const auto end     = token.find_last_not_of(" \t");
        const auto trimmed = token.substr(start, end - start + 1);
        if (!trimmed.empty()) {
          values.push_back(std::stoi(trimmed));
        }
      }
      return values;
    }

    /// Serializes an int vector back to a comma-separated string.
    std::string intArrayToString(const std::vector<int>& values) {
      std::ostringstream oss;
      for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) oss << ",";
        oss << values[i];
      }
      return oss.str();
    }

  } // anonymous namespace

  // =========================================================================
  // applyToConfig / readFromConfig
  // =========================================================================

  void TuningParameter::applyToConfig(config::SearchConfigData& search, config::EvalConfigData& eval) const {
    if (!configDef_) {
      throw std::logic_error("TuningParameter::applyToConfig: configDef_ is null for " + name);
    }

    if (arrayIndex < 0) {
      // Scalar: just set the value directly
      configDef_->setter(search, eval, std::to_string(currentValue));
    }
    else {
      // Array element: read the full array, modify one element, write back
      const std::string arrayStr = configDef_->getter(search, eval);
      auto values                = parseIntArray(arrayStr);
      if (static_cast<std::size_t>(arrayIndex) < values.size()) {
        values[arrayIndex] = currentValue;
      }
      configDef_->setter(search, eval, intArrayToString(values));
    }
  }

  void TuningParameter::readFromConfig(const config::SearchConfigData& search,
                                        const config::EvalConfigData& eval) {
    if (!configDef_) {
      throw std::logic_error("TuningParameter::readFromConfig: configDef_ is null for " + name);
    }

    const std::string valueStr = configDef_->getter(search, eval);

    if (arrayIndex < 0) {
      // Scalar
      currentValue = std::stoi(valueStr);
    }
    else {
      // Array element
      const auto values = parseIntArray(valueStr);
      if (static_cast<std::size_t>(arrayIndex) < values.size()) {
        currentValue = values[arrayIndex];
      }
    }
  }

  // =========================================================================
  // buildFromRegistry
  // =========================================================================

  std::vector<TuningParameter> TuningParameter::buildFromRegistry(
    const config::SearchConfigData& search,
    const config::EvalConfigData& eval) {

    const auto& registry   = config::ConfigRegistry::instance();
    const auto tunableDefs = registry.tunableOptions();

    std::vector<TuningParameter> params;
    params.reserve(tunableDefs.size() * 2); // rough estimate (arrays expand)

    for (const auto* def : tunableDefs) {
      if (def->valueType == config::ConfigValueType::Int) {
        // Scalar integer parameter → one TuningParameter
        TuningParameter param;
        param.name       = def->name;
        param.configName = def->name;
        param.configDef_ = def;

        // Read current value from config
        const std::string valueStr = def->getter(search, eval);
        param.currentValue  = std::stoi(valueStr);
        param.originalValue = param.currentValue;

        // Bounds from ConfigDef (if specified)
        param.minValue = def->minValue.value_or(-1000);
        param.maxValue = def->maxValue.value_or(1000);

        // Ensure current value is within stated bounds (defensive)
        param.minValue = std::min(param.minValue, param.currentValue);
        param.maxValue = std::max(param.maxValue, param.currentValue);

        param.paramGroup = assignParamGroup(def->name);

        params.push_back(std::move(param));
      }
      else if (def->valueType == config::ConfigValueType::IntArray) {
        // Array parameter → one TuningParameter per element
        const std::string arrayStr = def->getter(search, eval);
        const auto values          = parseIntArray(arrayStr);
        const auto arrSize         = static_cast<int>(values.size());
        const int group            = assignParamGroup(def->name);

        for (int i = 0; i < arrSize; ++i) {
          TuningParameter param;
          param.name       = def->name + "[" + std::to_string(i) + "]";
          param.configName = def->name;
          param.configDef_ = def;

          param.currentValue  = values[i];
          param.originalValue = values[i];
          param.arrayIndex    = i;
          param.arraySize     = arrSize;
          param.paramGroup    = group;

          // Bounds for array elements: use a generous range around the default.
          // No explicit min/max in ConfigDef for arrays, so use heuristics:
          //   - Min: min(0, currentValue - abs(currentValue) - 50)
          //   - Max: max(currentValue + abs(currentValue) + 50, 1000)
          // This gives enough room for the tuner to explore.
          const int absVal   = std::abs(values[i]);
          param.minValue     = std::min(0, values[i] - absVal - 50);
          param.maxValue     = std::max(values[i] + absVal + 50, 1000);

          assignMonotonicity(param);

          params.push_back(std::move(param));
        }
      }
      // Skip other types (Bool, String, etc.) — they shouldn't be marked tunable,
      // but be defensive.
    }

    return params;
  }

  // =========================================================================
  // countTunableValues
  // =========================================================================

  std::size_t TuningParameter::countTunableValues() {
    const auto& registry   = config::ConfigRegistry::instance();
    const auto tunableDefs = registry.tunableOptions();

    std::size_t count = 0;
    // We need a temporary config to read array sizes via getter.
    // Use default-constructed configs for counting purposes.
    // ReSharper disable once CppTooWideScope
    const config::SearchConfigData defaultSearch;
    // ReSharper disable once CppTooWideScope
    const config::EvalConfigData defaultEval;

    for (const auto* def : tunableDefs) {
      if (def->valueType == config::ConfigValueType::Int) {
        count += 1;
      }
      else if (def->valueType == config::ConfigValueType::IntArray) {
        const std::string arrayStr = def->getter(defaultSearch, defaultEval);
        const auto values          = parseIntArray(arrayStr);
        count += values.size();
      }
    }
    return count;
  }

} // namespace tuning
