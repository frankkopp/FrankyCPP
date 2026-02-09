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
#include "engine/config/ConfigManager.h"

void UciOptions::initOptions() {

  // SearchConfig order alignment
  // 1) Time management overhead
  optionVector.emplace_back(
    "Move Overhead", SearchConfig.MOVE_OVERHEAD_MS, 0, 5000,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.MOVE_OVERHEAD_MS = getInt(getOption("Move Overhead")->currentValue); /* Search tweak */);
    });

  // 2) Opening book
  optionVector.emplace_back(
    "OwnBook", SearchConfig.USE_BOOK,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.USE_BOOK = getOption("OwnBook")->currentValue == "true";);
    });
  optionVector.emplace_back(
    "Book Path", SearchConfig.BOOK_PATH.c_str(),
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.BOOK_PATH = getOption("Book Path")->currentValue;);
    });
  // Book Format as COMBO with allowed values
  optionVector.emplace_back(
    "Book Format", SearchConfig.BOOK_TYPE.c_str(),
      [&](UciHandler*) {
      CONFIG_OVERRIDE(s.BOOK_TYPE = getOption("Book Format")->currentValue;);
    });

  // 3) Ponder
  optionVector.emplace_back(
    "Ponder", SearchConfig.USE_PONDER,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.USE_PONDER = getOption("Ponder")->currentValue == "true";);
    });

  // 4) Basic search strategies and features
  optionVector.emplace_back(
    "Use AlphaBeta", SearchConfig.USE_ALPHABETA,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.USE_ALPHABETA = getOption("Use AlphaBeta")->currentValue == "true";);
    });
  optionVector.emplace_back(
    "Use Pvs", SearchConfig.USE_PVS,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.USE_PVS = getOption("Use Pvs")->currentValue == "true";);
    });
  optionVector.emplace_back(
    "Use Aspiration", SearchConfig.USE_ASP,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.USE_ASP = getOption("Use Aspiration")->currentValue == "true";);
    });

  // 5) Quiescence search
  optionVector.emplace_back(
    "Use Quiescence", SearchConfig.USE_QUIESCENCE,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.USE_QUIESCENCE = getOption("Use Quiescence")->currentValue == "true";);
    });

  // 6) Transposition Table
  optionVector.emplace_back(
    "Use Hash", SearchConfig.USE_TT,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_TT = getOption("Use Hash")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use Hash Value", SearchConfig.USE_TT_VALUE,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_TT_VALUE = getOption("Use Hash Value")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use Hash Eval", SearchConfig.USE_EVAL_TT,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_EVAL_TT = getOption("Use Hash Eval")->currentValue == "true";); });
  optionVector.emplace_back(
    "Hash", SearchConfig.TT_SIZE_MB, 0, 4096,
    [&](const UciHandler* uciHandler) {
      CONFIG_OVERRIDE(s.TT_SIZE_MB = getInt(getOption("Hash")->currentValue); uciHandler->getSearchPtr()->resizeTT(););
    });
  optionVector.emplace_back(
    "Use Hash Quiescence", SearchConfig.USE_QS_TT,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_QS_TT = getOption("Use Hash Quiescence")->currentValue == "true";); });

  // 7) Move Sorting Features
  optionVector.emplace_back(
    "Use Hash PvMove", SearchConfig.USE_TT_PV_MOVE_SORT,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_TT_PV_MOVE_SORT = getOption("Use Hash PvMove")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use Killer Moves", SearchConfig.USE_KILLER_MOVES,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_KILLER_MOVES = getOption("Use Killer Moves")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use History Counter", SearchConfig.USE_HISTORY_COUNTER,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_HISTORY_COUNTER = getOption("Use History Counter")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use History Moves", SearchConfig.USE_HISTORY_MOVES,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_HISTORY_MOVES = getOption("Use History Moves")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use Internal Iterative Deepening", SearchConfig.USE_IID,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_IID = getOption("Use Internal Iterative Deepening")->currentValue == "true";); });
  optionVector.emplace_back(
    "IID Move Depth", SearchConfig.IID_DEPTH, 0, DEPTH_MAX,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.IID_DEPTH = static_cast<Depth>(getInt(getOption("IID Move Depth")->currentValue)));
    });
  optionVector.emplace_back(
    "IID Depth Reduction", SearchConfig.IID_REDUCTION, 0, DEPTH_MAX,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.IID_REDUCTION = static_cast<Depth>(getInt(getOption("IID Depth Reduction")->currentValue)););
    });

  // 8) Pruning features
  optionVector.emplace_back(
    "Use Mate Distance Pruning", SearchConfig.USE_MDP,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_MDP = getOption("Use Mate Distance Pruning")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use Quiescence Standpat", SearchConfig.USE_QS_STANDPAT_CUT,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_QS_STANDPAT_CUT = getOption("Use Quiescence Standpat")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use Quiescence SEE", SearchConfig.USE_QS_SEE,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_QS_SEE = getOption("Use Quiescence SEE")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use Razoring", SearchConfig.USE_RAZORING,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_RAZORING = getOption("Use Razoring")->currentValue == "true";); });
  optionVector.emplace_back(
    "Razor Margin", SearchConfig.RAZOR_MARGIN, VALUE_MIN, VALUE_MAX,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.RAZOR_MARGIN = static_cast<Value>(getInt(getOption("Razor Margin")->currentValue)););
    });
  optionVector.emplace_back(
    "Use Reverse Futility Pruning", SearchConfig.USE_RFP,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_RFP = getOption("Use Reverse Futility Pruning")->currentValue == "true";); });

  // 9) Null Move Pruning + verification
  optionVector.emplace_back(
    "Use Null Move Pruning", SearchConfig.USE_NMP,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_NMP = getOption("Use Null Move Pruning")->currentValue == "true";); });
  optionVector.emplace_back(
    "Null Move Depth", SearchConfig.NMP_DEPTH, 0, DEPTH_MAX,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.NMP_DEPTH = static_cast<Depth>(getInt(getOption("Null Move Depth")->currentValue)););
    });
  optionVector.emplace_back(
    "Null Depth Reduction", SearchConfig.NMP_REDUCTION, 0, DEPTH_MAX,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.NMP_REDUCTION = static_cast<Depth>(getInt(getOption("Null Depth Reduction")->currentValue)););
    });
  optionVector.emplace_back(
    "Use Null Move Verification", SearchConfig.USE_NMP_VERIFY,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_NMP_VERIFY = getOption("Use Null Move Verification")->currentValue == "true";); });
  optionVector.emplace_back(
    "Null Move Verify Min Depth", SearchConfig.NMP_VERIFY_MIN_DEPTH, 0, DEPTH_MAX,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.NMP_VERIFY_MIN_DEPTH = static_cast<Depth>(getInt(getOption("Null Move Verify Min Depth")->currentValue)););
    });
  optionVector.emplace_back(
    "Null Move Verify Margin", SearchConfig.NMP_VERIFY_MARGIN, 0, DEPTH_MAX,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.NMP_VERIFY_MARGIN = static_cast<Depth>(getInt(getOption("Null Move Verify Margin")->currentValue)););
    });
  optionVector.emplace_back(
    "Null Move Near Mate Margin", SearchConfig.NMP_NEAR_MATE_MARGIN, VALUE_MIN, VALUE_MAX,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.NMP_NEAR_MATE_MARGIN = static_cast<Value>(getInt(getOption("Null Move Near Mate Margin")->currentValue)););
    });
  optionVector.emplace_back(
    "Use Null Move Zugzwang Guard", SearchConfig.USE_NMP_ZUG_GUARD,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_NMP_ZUG_GUARD = getOption("Use Null Move Zugzwang Guard")->currentValue == "true";); });
  optionVector.emplace_back(
    "Null Move Zug NonPawn Threshold", SearchConfig.NMP_ZUG_NONPAWN_THRESHOLD, 0, 32,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.NMP_ZUG_NONPAWN_THRESHOLD = getInt(getOption("Null Move Zug NonPawn Threshold")->currentValue););
    });

  // 10) Futility pruning
  optionVector.emplace_back(
    "Use Futility Pruning", SearchConfig.USE_FP,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_FP = getOption("Use Futility Pruning")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use Quiescence Futility Pruning", SearchConfig.USE_QFP,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_QFP = getOption("Use Quiescence Futility Pruning")->currentValue == "true";); });

  // 11) LMR
  optionVector.emplace_back(
    "Use Late Move Reduction", SearchConfig.USE_LMR,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_LMR = getOption("Use Late Move Reduction")->currentValue == "true";); });
  optionVector.emplace_back(
    "LMR Min Depth", SearchConfig.LMR_MIN_DEPTH, 0, DEPTH_MAX,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.LMR_MIN_DEPTH = static_cast<Depth>(getInt(getOption("LMR Min Depth")->currentValue)););
    });
  optionVector.emplace_back(
    "LMR Min Moves", SearchConfig.LMR_MIN_MOVES, 0, 64,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.LMR_MIN_MOVES = getInt(getOption("LMR Min Moves")->currentValue););
    });

  // 12) LMP
  optionVector.emplace_back(
    "Use Late Move Pruning", SearchConfig.USE_LMP,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_LMP = getOption("Use Late Move Pruning")->currentValue == "true";); });

  // 13) Extensions
  optionVector.emplace_back(
    "Use Extensions", SearchConfig.USE_EXTENSIONS,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_EXTENSIONS = getOption("Use Extensions")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use Check Extension", SearchConfig.USE_CHECK_EXT,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_CHECK_EXT = getOption("Use Check Extension")->currentValue == "true";); });
  optionVector.emplace_back(
    "Check Ext Early Limit", SearchConfig.CHECK_EXT_EARLY_LIMIT, 1, 10,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.CHECK_EXT_EARLY_LIMIT = std::stoi(getOption("Check Ext Early Limit")->currentValue););
    });
  optionVector.emplace_back(
    "Use Threat Extension", SearchConfig.USE_THREAT_EXT,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_THREAT_EXT = getOption("Use Threat Extension")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use Extension Add", SearchConfig.USE_EXT_ADD_DEPTH,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_EXT_ADD_DEPTH = getOption("Use Extension Add")->currentValue == "true";); });
  optionVector.emplace_back(
    "Use Singular Extension", SearchConfig.USE_SINGULAR_EXT,
    [&](UciHandler*) { CONFIG_OVERRIDE(s.USE_SINGULAR_EXT = getOption("Use Singular Extension")->currentValue == "true";); });
  optionVector.emplace_back(
    "Singular Margin", SearchConfig.SINGULAR_MARGIN, 0, 500,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.SINGULAR_MARGIN = getInt(getOption("Singular Margin")->currentValue););
    });
  optionVector.emplace_back(
    "Singular Min Depth", SearchConfig.SINGULAR_MIN_DEPTH, 1, 20,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.SINGULAR_MIN_DEPTH = getInt(getOption("Singular Min Depth")->currentValue););
    });
  optionVector.emplace_back(
    "Singular Reduction", SearchConfig.SINGULAR_REDUCTION, 1, 10,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.SINGULAR_REDUCTION = getInt(getOption("Singular Reduction")->currentValue););
    });

  // 14) Moves-left model and thresholds/clamps
  optionVector.emplace_back(
    "Moves Left Opening", SearchConfig.MOVES_LEFT_OPENING, 0, 100,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.MOVES_LEFT_OPENING = getInt(getOption("Moves Left Opening")->currentValue););
    });
  optionVector.emplace_back(
    "Moves Left Midgame", SearchConfig.MOVES_LEFT_MIDGAME, 0, 100,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.MOVES_LEFT_MIDGAME = getInt(getOption("Moves Left Midgame")->currentValue););
    });
  optionVector.emplace_back(
    "Moves Left Endgame", SearchConfig.MOVES_LEFT_ENDGAME, 0, 100,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.MOVES_LEFT_ENDGAME = getInt(getOption("Moves Left Endgame")->currentValue););
    });
  optionVector.emplace_back(
    "Moves Left Low Material", SearchConfig.MOVES_LEFT_LOW_MAT, 0, 100,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.MOVES_LEFT_LOW_MAT = getInt(getOption("Moves Left Low Material")->currentValue););
    });
  optionVector.emplace_back(
    "Moves Left Queenless", SearchConfig.MOVES_LEFT_QUEENLESS, 0, 100,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.MOVES_LEFT_QUEENLESS = getInt(getOption("Moves Left Queenless")->currentValue););
    });
  optionVector.emplace_back(
    "NPP Heavy Threshold", SearchConfig.NPP_HEAVY_THRESHOLD, 0, 32,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.NPP_HEAVY_THRESHOLD = getInt(getOption("NPP Heavy Threshold")->currentValue););
    });
  optionVector.emplace_back(
    "NPP Light Threshold", SearchConfig.NPP_LIGHT_THRESHOLD, 0, 32,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.NPP_LIGHT_THRESHOLD = getInt(getOption("NPP Light Threshold")->currentValue););
    });
  optionVector.emplace_back(
    "Repetition HMC High", SearchConfig.REPETITION_HMC_HIGH, 0, 200,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.REPETITION_HMC_HIGH = getInt(getOption("Repetition HMC High")->currentValue););
    });
  optionVector.emplace_back(
    "Repetition Risk Penalty", SearchConfig.REPETITION_RISK_PENALTY, 0, 20,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.REPETITION_RISK_PENALTY = getInt(getOption("Repetition Risk Penalty")->currentValue););
    });
  optionVector.emplace_back(
    "Moves Left Min Clamp", SearchConfig.MOVES_LEFT_MIN_CLAMP, 0, 100,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.MOVES_LEFT_MIN_CLAMP = getInt(getOption("Moves Left Min Clamp")->currentValue););
    });
  optionVector.emplace_back(
    "Moves Left Max Clamp", SearchConfig.MOVES_LEFT_MAX_CLAMP, 0, 100,
    [&](UciHandler*) {
      CONFIG_OVERRIDE(s.MOVES_LEFT_MAX_CLAMP = getInt(getOption("Moves Left Max Clamp")->currentValue););
    });

  // UciOptions extras (non-SearchConfig)
  optionVector.emplace_back(
    "Clear Hash",
    [&](const UciHandler* uciHandler) { uciHandler->getSearchPtr()->clearTT(); });
  optionVector.emplace_back(
    "Reset to Defaults",
    [&](UciHandler* uciHandler) { this->resetToDefaults(uciHandler); });

  // EvalConfig-related options
  optionVector.emplace_back(
    "Use Lazy Eval", EvalConfig.USE_LAZY_EVAL,
    [&](UciHandler*) {
      engine::config::ConfigManager::instance().applyOverrides([&](auto&, engine::config::EvalConfigData& e) {
        e.USE_LAZY_EVAL = getOption("Use Lazy Eval")->currentValue == "true";
      });
    });
  optionVector.emplace_back(
    "Use Pawn Eval", EvalConfig.USE_PAWN_EVAL,
    [&](UciHandler*) {
      engine::config::ConfigManager::instance().applyOverrides([&](auto&, engine::config::EvalConfigData& e) {
        e.USE_PAWN_EVAL = getOption("Use Pawn Eval")->currentValue == "true";
      });
    });
  optionVector.emplace_back(
    "Use Pawn Hash", EvalConfig.USE_PAWN_TT,
    [&](UciHandler*) {
      engine::config::ConfigManager::instance().applyOverrides([&](auto&, engine::config::EvalConfigData& e) {
        e.USE_PAWN_TT = getOption("Use Pawn Hash")->currentValue == "true";
      });
    });
  optionVector.emplace_back(
    "Pawn Hash Size", EvalConfig.PAWN_TT_SIZE_MB, 0, 1024,
    [&](UciHandler*) {
      engine::config::ConfigManager::instance().applyOverrides([&](auto&, engine::config::EvalConfigData& e) {
        e.PAWN_TT_SIZE_MB = static_cast<Depth>(getInt(getOption("Pawn Hash Size")->currentValue));
      });
    });

  // optionVector.emplace_back("***", [&](UciHandler* uciHandler) { });
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
  return false;
}

std::string UciOptions::str() const {
  std::string str;
  for (const auto& o : optionVector) {
    str += o.str() + "\n";
  }
  str = trimFast(str);// remove last newline
  return str;
}

std::string UciOptions::strWithCurrentValues() const {
  std::string str;
  for (const auto& o : optionVector) {
    str += o.strWithCurrentValue() + "\n";
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
  // The "current" field is non-standard UCI but used by FrankyCPP to report the actual current value
  // Needs to be reviewed for other engines - may not be supported everywhere
  // if (type != BUTTON && !currentValue.empty()) {
  //   str += " current " + currentValue;
  // }
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
  } catch (...) {
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
