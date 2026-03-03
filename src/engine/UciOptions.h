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

#ifndef FRANKYCPP_UCIOPTIONS_H
#define FRANKYCPP_UCIOPTIONS_H

//=============================================================================
// UciOptions.h - UCI Protocol Option Handling
//=============================================================================
//
// Manages UCI options that can be changed by the GUI via "setoption" command.
// Options are listed in response to the "uci" command.
// Depends on: ConfigManager.h
//
// UCI Option Types:
//   CHECK  - Boolean option (true/false)
//   SPIN   - Integer with min/max bounds
//   COMBO  - Selection from predefined values
//   BUTTON - Trigger action (no value stored)
//   STRING - Free-form text
//
// Architecture:
//   - UciOption: Single option definition with type, bounds, and handler
//   - UciOptions: Singleton managing all available options
//   - Each option has a handler function called when the value changes
//
// Standard Options:
//   - Hash: Transposition table size in MB
//   - Threads: Number of search threads
//   - Ponder: Enable pondering
//   - UCI_AnalyseMode: Analysis mode flag
//   - OwnBook: Use internal opening book
//   - Book File: Path to opening book file
//   - Clear Hash: Button to clear TT
//
// Usage:
//   // Get option value
//   const UciOption* opt = UciOptions::getInstance()->getOption("Hash");
//   int hashSize = UciOptions::getInt(opt->currentValue);
//
//   // Set option (typically called by UciHandler)
//   UciOptions::getInstance()->setOption(handler, "Hash", "256");
//
//=============================================================================

#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace engine {

  class UciHandler;

  /// UCI option types as defined by the UCI protocol.
  enum UciOptionType {
    CHECK, ///< Boolean option (true/false)
    SPIN,  ///< Integer with min/max bounds
    COMBO, ///< Selection from predefined values
    BUTTON,///< Trigger action (no stored value)
    STRING ///< Free-form text
  };

  /// Defines a single UCI option with type, default value, bounds, and handler.
  /// The handler function is called when the option value changes.
  struct UciOption {
    const std::string nameID;                 ///< Option name (case-sensitive in UCI)
    const UciOptionType type;                 ///< Option type
    const std::string defaultValue;           ///< Default value as string
    const std::string minValue;               ///< Minimum value (SPIN only)
    const std::string maxValue;               ///< Maximum value (SPIN only)
    const std::string varValue;               ///< Variable value (legacy, prefer comboVars)
    std::string currentValue;                 ///< Current value as string
    std::function<void(UciHandler*)> pHandler;///< Handler called on value change

    /// Allowed values for COMBO options.
    std::vector<std::string> comboVars{};

    /// Creates a BUTTON option (action trigger, no value).
    /// @param name     Option name
    /// @param handler  Function called when button is pressed
    explicit UciOption(const char* name, std::function<void(UciHandler*)> handler)
        : nameID(name), type(BUTTON), defaultValue(common::boolStr(false)), pHandler(std::move(handler)) {}

    /// Creates a CHECK option (boolean).
    /// @param name     Option name
    /// @param value    Default value
    /// @param handler  Function called on value change
    UciOption(const char* name, const bool value, std::function<void(UciHandler*)> handler)
        : nameID(name), type(CHECK), defaultValue(common::boolStr(value)), currentValue(common::boolStr(value)), pHandler(std::move(handler)) {}

    /// Creates a SPIN option (integer with bounds).
    /// @param name     Option name
    /// @param def      Default value
    /// @param min      Minimum allowed value
    /// @param max      Maximum allowed value
    /// @param handler  Function called on value change
    UciOption(const char* name, const int def, const int min, const int max, std::function<void(UciHandler*)> handler)
        : nameID(name), type(SPIN), defaultValue(std::to_string(def)), minValue(std::to_string(min)),
          maxValue(std::to_string(max)), currentValue(std::to_string(def)), pHandler(std::move(handler)) {}

    /// Creates a STRING option with same default and current value.
    /// @param name     Option name
    /// @param str      Default and current value
    /// @param handler  Function called on value change
    UciOption(const char* name, const char* str, std::function<void(UciHandler*)> handler)
        : nameID(name), type(STRING), defaultValue(str), currentValue(str), pHandler(std::move(handler)) {}

    /// Creates a STRING option with different default and current values.
    /// @param name     Option name
    /// @param val      Default value
    /// @param def      Current value
    /// @param handler  Function called on value change
    UciOption(const char* name, const char* val, const char* def, std::function<void(UciHandler*)> handler)
        : nameID(name), type(STRING), defaultValue(val), currentValue(def), pHandler(std::move(handler)) {}

    /// Creates a COMBO option with allowed values.
    /// @param name     Option name
    /// @param vars     List of allowed values
    /// @param def      Default value (must be in vars)
    /// @param handler  Function called on value change
    UciOption(const char* name, const std::initializer_list<const char*> vars, const char* def, std::function<void(UciHandler*)> handler)
        : nameID(name), type(COMBO), defaultValue(def), currentValue(def), pHandler(std::move(handler)) {
      comboVars.reserve(vars.size());
      for (auto v : vars) comboVars.emplace_back(v);
    }

    UciOption(const UciOption& o) = default;

    /// Returns UCI protocol representation for the "uci" command response.
    /// Format: "option name <name> type <type> default <default> [min <min>] [max <max>] [var <var>...]"
    /// @return  UCI-formatted option string
    [[nodiscard]] std::string str() const;

    /// Returns option information with current value.
    /// Extension to UCI protocol for testing purposes.
    /// Format: "option name <name> type <type> current <currentValue>"
    /// BUTTON options return only name and type.
    /// @return  Option string with current value
    [[nodiscard]] std::string strWithCurrentValue() const;

    friend std::ostream& operator<<(std::ostream& os, const UciOption& option) {
      os << option.str();
      return os;
    }
  };

  /// Singleton class managing all available UCI options.
  /// Access via getInstance(). Options are initialized from config and can be
  /// queried/modified through the UCI protocol.
  class UciOptions {
    std::vector<UciOption> optionVector{};

    /// Private constructor - use getInstance() to access.
    UciOptions() {
      initOptions();
    }

    /// Initializes all available UCI options with their defaults and handlers.
    void initOptions();

    friend std::ostream& operator<<(std::ostream& os, const UciOptions& options);


  public:
    // disallow copies and moves
    UciOptions(UciOptions const&)             = delete;
    UciOptions(UciOptions const&&)            = delete;
    UciOptions& operator=(const UciOptions&)  = delete;
    UciOptions& operator=(const UciOptions&&) = delete;

    /// Returns the singleton instance.
    /// @return  Pointer to the UciOptions singleton
    static UciOptions* getInstance() {
      static UciOptions instance;
      return &instance;
    }

    /// Finds an option by name.
    /// @param name  Option name (case-sensitive)
    /// @return      Pointer to the option, or nullptr if not found
    [[nodiscard]] UciOption* getOption(const std::string& name);

    /// Sets an option value and calls its handler.
    /// @param uciHandler  UCI handler for handler callback
    /// @param name        Option name
    /// @param value       New value as string
    /// @return            True if option was found and set, false otherwise
    bool setOption(UciHandler* uciHandler, const std::string& name, const std::string& value);

    /// Resets all options to their default values and applies handlers.
    /// BUTTON options are skipped.
    /// @param uciHandler  UCI handler for handler callbacks
    void resetToDefaults(UciHandler* uciHandler);

    /// Returns UCI protocol representation for all options.
    /// Used in response to the "uci" command.
    /// @return  Multi-line string with all option definitions
    [[nodiscard]] std::string str() const;

    /// Returns option information with current values.
    /// Extension to UCI protocol for testing. Used in response to the "getoptions" command.
    /// Format: "option name <name> type <type> current <currentValue>"
    /// @return  Multi-line string with all options and their current values
    [[nodiscard]] std::string strWithCurrentValues() const;

    /// Returns extended option information including domain and default.
    /// Non-standard extension that includes config domain from ConfigRegistry.
    /// Format: "option name <name> type <type> default <default> current <current> domain <domain>"
    /// @return  Multi-line string with extended option information
    [[nodiscard]] std::string strExtended() const;
  };

  /// Stream output operator for all options.
  inline std::ostream& operator<<(std::ostream& os, const UciOptions& options) {
    os << options.str();
    return os;
  }

}// namespace engine

#endif// FRANKYCPP_UCIOPTIONS_H
