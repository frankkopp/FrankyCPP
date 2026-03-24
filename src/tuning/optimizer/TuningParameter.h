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

#ifndef FRANKYCPP_TUNINGPARAMETER_H
#define FRANKYCPP_TUNINGPARAMETER_H

//=============================================================================
// TuningParameter.h - Parameter Mapping for Texel Tuning
//=============================================================================
//
// Maps ConfigRegistry tunable entries to a flat parameter vector suitable
// for coordinate descent optimization. Each scalar Int config becomes one
// TuningParameter; each IntArray element becomes a separate TuningParameter
// with an arrayIndex.
//
// Key Design Decisions:
//   - Uses ConfigDef getter/setter lambdas (no raw pointers into config structs).
//     The tuner reads/writes values through the same type-safe interface used
//     by YAML parsing and UCI, ensuring consistency.
//   - Monotonicity constraints are assigned by name to known array parameters.
//   - Parameter groups map eval categories for incremental MSE optimization.
//
// Usage:
//   auto& search = ConfigManager::instance().search();   // (unused, but needed by getter/setter)
//   auto& eval   = ConfigManager::instance().eval();
//   auto params = TuningParameter::buildFromRegistry(search, eval);
//   for (auto& p : params) {
//     std::cout << p.name << " = " << p.currentValue << "\n";
//   }
//
// Thread Safety:
//   TuningParameter objects are NOT thread-safe. The tuner must ensure
//   exclusive write access during parameter modification.
//
//=============================================================================

#include <string>
#include <vector>

namespace config {
  struct ConfigDef;
  struct SearchConfigData;
  struct EvalConfigData;
} // namespace config

namespace tuning {

  /// Monotonicity constraint for array parameters.
  /// Enforced after each coordinate descent step to maintain sensible ordering.
  enum class MonotonicityConstraint {
    NONE,           ///< No constraint
    NON_DECREASING, ///< array[i] >= array[i-1]
    NON_INCREASING  ///< array[i] <= array[i-1]
  };

  /// A single tunable parameter in the flat parameter vector.
  ///
  /// For scalar Int config entries, there is one TuningParameter per ConfigDef.
  /// For IntArray entries, there is one TuningParameter per array element.
  struct TuningParameter {
    std::string name;          ///< Display name (e.g., "ISOLATED_PAWN_MID_WEIGHT" or "KING_SAFETY_TABLE[3]")
    std::string configName;    ///< ConfigRegistry name (e.g., "KING_SAFETY_TABLE" — same for all elements)

    int originalValue = 0;     ///< Value at tuning start (for comparison / reset)
    int currentValue  = 0;     ///< Current optimized value

    int minValue = -1000;      ///< Lower bound for coordinate descent
    int maxValue = 1000;       ///< Upper bound for coordinate descent
    int delta    = 1;          ///< Step size for coordinate descent

    int paramGroup = 0;        ///< Group index for activation bitset (incremental MSE)
    int arrayIndex = -1;       ///< -1 for scalars, 0..N-1 for array elements
    int arraySize  = 0;        ///< Total array size (0 for scalars)

    MonotonicityConstraint monotonicity = MonotonicityConstraint::NONE;

    /// Applies the current value back to the config structs via the ConfigDef setter.
    /// For array parameters, only the element at arrayIndex is modified.
    void applyToConfig(config::SearchConfigData& search, config::EvalConfigData& eval) const;

    /// Reads the current value from the config structs via the ConfigDef getter.
    /// For array parameters, extracts the element at arrayIndex.
    void readFromConfig(const config::SearchConfigData& search, const config::EvalConfigData& eval);

    /// Build the flat parameter vector from all tunable ConfigRegistry entries.
    /// Scalar Int entries produce one parameter each. IntArray entries produce one
    /// parameter per element. Values are read from the provided config structs.
    ///
    /// @param search  Current search config (needed by getter/setter lambdas)
    /// @param eval    Current eval config (values are read from here)
    /// @return Vector of TuningParameter, one per tunable scalar/element
    [[nodiscard]] static std::vector<TuningParameter> buildFromRegistry(
      const config::SearchConfigData& search,
      const config::EvalConfigData& eval);

    /// Returns the total number of tunable scalar values (expanding arrays).
    /// Equivalent to buildFromRegistry(...).size() but without allocating.
    [[nodiscard]] static std::size_t countTunableValues();

  private:
    /// Pointer to the ConfigDef this parameter was built from (non-owning).
    /// Used by applyToConfig/readFromConfig for type-safe access.
    const config::ConfigDef* configDef_ = nullptr;

    /// Assigns monotonicity constraints to known array parameters by name.
    static void assignMonotonicity(TuningParameter& param);

    /// Assigns parameter group index based on the config name.
    /// Groups map to eval categories for activation-flag optimization.
    [[nodiscard]] static int assignParamGroup(const std::string& configName);
  };

} // namespace tuning

#endif // FRANKYCPP_TUNINGPARAMETER_H
