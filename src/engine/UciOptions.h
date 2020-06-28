/*
 * MIT License
 *
 * Copyright (c) 2018-2020 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef FRANKYCPP_UCIOPTIONS_H
#define FRANKYCPP_UCIOPTIONS_H

#include <ostream>
#include <sstream>
#include <utility>
#include <vector>
#include <functional>

#include "common/misc.h"

class UciHandler;

// Uci option types
enum UciOptionType {
  CHECK,
  SPIN,
  COMBO,
  BUTTON,
  STRING
};

// UCI provides the ability to change parameters of an engine via
// UCI options. This is done by the setoption command and the each engine
// might define its own set of options which will be listed when the GUI
// sends the 'uci' command.
// An instance of this struct defines one available option. It defines
// a type, default value, min, max or var (possible values) and also
// a pointer to a handler function mostly defined as a lambda function.
struct UciOption {
  const std::string nameID;
  const UciOptionType type;
  const std::string defaultValue;
  const std::string minValue;
  const std::string maxValue;
  const std::string varValue;
  std::string currentValue;
  std::function<void(UciHandler*)> pHandler;

    explicit UciOption(const char* name, std::function<void(UciHandler*)> handler)
      : nameID(name), type(BUTTON), defaultValue(boolStr(false)), pHandler(std::move(handler)) {}

  UciOption(const char* name, bool value, std::function<void(UciHandler*)> handler)
      : nameID(name), type(CHECK), defaultValue(boolStr(value)), currentValue(boolStr(value)), pHandler(std::move(handler)) {}

  UciOption(const char* name, int def, int min, int max, std::function<void(UciHandler*)> handler)
      : nameID(name), type(SPIN), defaultValue(std::to_string(def)), minValue(std::to_string(min)),
        maxValue(std::to_string(max)), currentValue(std::to_string(def)), pHandler(std::move(handler)) {}

  UciOption(const char* name, const char* str, std::function<void(UciHandler*)> handler)
      : nameID(name), type(STRING), defaultValue(str), currentValue(str), pHandler(std::move(handler)){}

  UciOption(const char* name, const char* val, const char* def, std::function<void(UciHandler*)> handler)
      : nameID(name), type(STRING), defaultValue(val), currentValue(def), pHandler(std::move(handler)) {}

  UciOption(const UciOption& o) = default;

  // String for uciOption will return a representation of the uci option as required by
  // the UCI protocol during the initialization phase of the UCI protocol
  std::string str() const;

  friend std::ostream& operator<<(std::ostream& os, const UciOption& option) {
    os << option.str();
    return os;
  }
};


// Singleton class to store for all available options
// This singleton instance can be used through the engine to access options.
// The str() call returns a list of all options as required by the "uci" command.
class UciOptions {
private:
  std::vector<UciOption> optionVector{};

  UciOptions() { initOptions(); }// private constructor

  void initOptions();

  friend std::ostream& operator<<(std::ostream& os, const UciOptions& options);

public:
  UciOptions(UciOptions const&)  = delete;           // copy
  UciOptions(UciOptions const&&) = delete;           // move
  UciOptions& operator=(const UciOptions&) = delete; // copy assignment
  UciOptions& operator=(const UciOptions&&) = delete;// move assignment

  // get the singleton instance of the class
  static UciOptions* getInstance() {
    static UciOptions instance;
    return &instance;
  }

  // returns a pointer to the uci option or nullptr if option is not found
  const UciOption* getOption(const std::string& name) const;

  // finds and stores the value in the given options. Returns true if
  // the options was found and the value was set. It calls the option's
  // handler function in this case.
  // Otherwise, if option was not found it returns false.
  bool setOption(UciHandler* uciHandler, const std::string& name, const std::string& value);

  // String for uciOption will return a representation of the uci option as required by
  // the UCI protocol during the initialization phase of the UCI protocol
  std::string str() const;

  // helper for converting a string option to an int
  int getInt(const std::string& value);
};

inline std::ostream& operator<<(std::ostream& os, const UciOptions& options) {
  os << options.str();
  return os;
}

#endif//FRANKYCPP_UCIOPTIONS_H
