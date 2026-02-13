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

#include "UciOptions.h"
#include "Search.h"
#include "UciHandler.h"
#include "common/stringutil.h"
#include "config/ConfigGenerators.h"
#include "config/ConfigManager.h"

void UciOptions::initOptions() {
  // Phase 4: UCI options are now auto-generated from ConfigRegistry
  // This replaces ~350 lines of manual option registration

  // Initialize all UCI options from the registry
  initUciOptionsFromRegistry(optionVector, this);

  // Add UCI-only buttons (Clear Hash, Reset to Defaults)
  addUciOnlyButtons(optionVector, this);
}

UciOption* UciOptions::getOption(const std::string& name) {
  // find option entry
  const auto optionIterator = std::ranges::find_if(optionVector,
                                                   [&](const UciOption& p) {
                                                     return name == p.nameID;
                                                   });
  if (optionIterator != optionVector.end()) {
    return &*optionIterator;
  }
  LOG__WARN(Logger::get().UCI_LOG, "Option '{}' not found", name);
  return nullptr;
}

bool UciOptions::setOption(UciHandler* uciHandler, const std::string& name, const std::string& value) {
  if (auto *const o = getOption(name)) {
    if (o->type == COMBO) {
      bool ok = false;
      for (const auto& v : o->comboVars) {
        if (v == value) {
          ok = true;
          break;
        }
      }
      if (!ok) return false;
    }
    o->currentValue = value;
    o->pHandler(uciHandler);
    return true;
  }
  LOG__ERROR(Logger::get().UCI_LOG, "Option '{}' not found", name);
  return false;
}

std::string UciOptions::str() const {
  // Collect option strings and sort alphabetically by option name
  std::vector<std::string> optionStrings;
  optionStrings.reserve(optionVector.size());
  for (const auto& o : optionVector) {
    optionStrings.push_back(o.str());
  }
  std::ranges::sort(optionStrings);

  std::string str;
  for (const auto& s : optionStrings) {
    str += s + "\n";
  }
  str = trimFast(str);// remove last newline
  return str;
}

std::string UciOptions::strWithCurrentValues() const {
  // Collect option strings and sort alphabetically by option name
  std::vector<std::string> optionStrings;
  optionStrings.reserve(optionVector.size());
  for (const auto& o : optionVector) {
    optionStrings.push_back(o.strWithCurrentValue());
  }
  std::ranges::sort(optionStrings);

  std::string str;
  for (const auto& s : optionStrings) {
    str += s + "\n";
  }
  str = trimFast(str);// remove last newline
  return str;
}

std::string UciOption::str() const {
  std::string str = "option name " + nameID + " type ";
  switch (type) {
    case CHECK:
      str += "check default " + defaultValue;
      break;
    case SPIN:
      str += "spin default " + defaultValue + " min " + minValue + " max " + maxValue;
      break;
    case COMBO:
      str += "combo default " + defaultValue;
      for (const auto& v : comboVars) {
        str += " var " + v;
      }
      break;
    case BUTTON:
      str += "button";
      break;
    case STRING:
      str += "string default " + defaultValue;
      break;
  }
  return str;
}

std::string UciOption::strWithCurrentValue() const {
  std::string str = "option name " + nameID + " type ";
  switch (type) {
    case CHECK:
      str += "check current " + currentValue;
      break;
    case SPIN:
      str += "spin current " + currentValue;
      break;
    case COMBO:
      str += "combo current " + currentValue;
      break;
    case BUTTON:
      str += "button";
      break;
    case STRING:
      str += "string current " + currentValue;
      break;
  }
  return str;
}

int UciOptions::getInt(const std::string& value) {
  try {
    const int intValue = stoi(value);
    return intValue;
  } catch (const std::exception& e) {
    LOG__ERROR(Logger::get().UCI_LOG,
               "Failed to parse integer from value '{}': {}", value, e.what());
    return 0;
  }
}

void UciOptions::resetToDefaults(UciHandler* uciHandler) {
  if (!uciHandler) return;
  // Reset every non-BUTTON option to its default by reusing setOption,
  // which also invokes the option's handler to propagate the change.
  LOG__INFO(Logger::get().UCI_LOG, "Resetting all options to their default values");
  for (const auto& o : optionVector) {
    if (o.type == BUTTON) continue;// buttons have no persistent value
    setOption(uciHandler, o.nameID, o.defaultValue);
    LOG__DEBUG(Logger::get().UCI_LOG, "  Option '{}' reset to default value '{}'", o.nameID, o.defaultValue);
  }
}
