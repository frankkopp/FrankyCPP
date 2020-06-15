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

#include <misc.h>
#include <sstream>
#include <vector>

enum UciOptionType {
  CHECK,
  SPIN,
  COMBO,
  BUTTON,
  STRING
};

struct UciOption {
  const std::string nameID;
  const UciOptionType type;
  const std::string defaultValue;
  const std::string minValue;
  const std::string maxValue;
  const std::string varValue;
  std::string currentValue;
  std::function<void()> pHandler;

  friend std::ostream& operator<<(std::ostream& os, const UciOption& option);

  explicit UciOption(const char* name, std::function<void()> handler)
      : nameID(name), type(BUTTON), defaultValue(boolStr(false)), pHandler(handler) {}

  UciOption(const char* name, bool value, std::function<void()> handler)
      : nameID(name), type(CHECK), defaultValue(boolStr(value)), currentValue(boolStr(value)), pHandler(handler) {}

  UciOption(const char* name, int def, int min, int max, std::function<void()> handler)
      : nameID(name), type(SPIN), defaultValue(std::to_string(def)), minValue(std::to_string(min)),
        maxValue(std::to_string(max)), currentValue(std::to_string(def)), pHandler(handler) {}

  UciOption(const char* name, const char* str, std::function<void()> handler)
      : nameID(name), type(STRING), defaultValue(str), currentValue(str), pHandler(handler){}

  UciOption(const char* name, const char* val, const char* def, std::function<void()> handler)
      : nameID(name), type(STRING), defaultValue(val), currentValue(def), pHandler(handler) {}

  UciOption(const UciOption& o) = default;

  // String for uciOption will return a representation of the uci option as required by
  // the UCI protocol during the initialization phase of the UCI protocol
  std::string str() const;
};

inline std::ostream& operator<<(std::ostream& os, const UciOption& option) {
  // TODO
  return os;
}

// Singleton class for options
class UciOptions {

  UciOptions() { initOptions(); }// private constructor
  void initOptions();
  friend std::ostream& operator<<(std::ostream& os, const UciOptions& options);
  std::vector<UciOption> optionVector{};

public:
  UciOptions(UciOptions const&)  = delete;           // copy
  UciOptions(UciOptions const&&) = delete;           // move
  UciOptions& operator=(const UciOptions&) = delete; // copy assignment
  UciOptions& operator=(const UciOptions&&) = delete;// move assignment

  // returns a pointer to the uci option or nullptr if option is not found
  const UciOption* getOption(std::string name) const;

  // finds and stores the value in the given options. Returns true if
  // the options was found and the value was set. It calls the option's
  // handler function in this case.
  // Otherwise, if option was not found it returns false.
  bool setOption(std::string name, std::string value);

  // get the singleton instance of the class
  static UciOptions* getInstance() {
    static UciOptions instance;
    return &instance;
  }

  // String for uciOption will return a representation of the uci option as required by
  // the UCI protocol during the initialization phase of the UCI protocol
  std::string str() const;

  int getInt(const std::string& value);
};

inline std::ostream& operator<<(std::ostream& os, const UciOptions& options) {
  // TODO
  return os;
}

#endif//FRANKYCPP_UCIOPTIONS_H
