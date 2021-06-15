// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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
#include "EvalConfig.h"
#include "Search.h"
#include "SearchConfig.h"
#include "UciHandler.h"
#include "common/stringutil.h"

void UciOptions::initOptions() {

  optionVector.emplace_back("OwnBook", SearchConfig::USE_BOOK,
                            [&](UciHandler*) { SearchConfig::USE_BOOK = getOption("OwnBook")->currentValue == "true"; });

  optionVector.emplace_back("Ponder", SearchConfig::USE_PONDER,
                            [&](UciHandler*) { SearchConfig::USE_PONDER = getOption("Ponder")->currentValue == "true"; });

  optionVector.emplace_back("Use AlphaBeta", SearchConfig::USE_ALPHABETA,
                            [&](UciHandler*) { SearchConfig::USE_ALPHABETA = getOption("Use AlphaBeta")->currentValue == "true"; });

  optionVector.emplace_back("Use Pvs", SearchConfig::USE_PVS,
                            [&](UciHandler*) { SearchConfig::USE_PVS = getOption("Use Pvs")->currentValue == "true"; });

  optionVector.emplace_back("Use Aspiration", SearchConfig::USE_ASP,
                            [&](UciHandler*) { SearchConfig::USE_ASP = getOption("Use Aspiration")->currentValue == "true"; });

  optionVector.emplace_back("Use Hash", SearchConfig::USE_TT,
                            [&](UciHandler*) { SearchConfig::USE_TT = getOption("Use Hash")->currentValue == "true"; });

  optionVector.emplace_back("Hash", SearchConfig::TT_SIZE_MB, 0, 4096,
                            [&](UciHandler* uciHandler) { SearchConfig::TT_SIZE_MB = getInt(getOption("Hash")->currentValue); uciHandler->getSearchPtr()->resizeTT(); });

  optionVector.emplace_back("Use Hash Value", SearchConfig::USE_TT_VALUE,
                            [&](UciHandler*) { SearchConfig::USE_TT_VALUE = getOption("Use Hash Value")->currentValue == "true"; });

  optionVector.emplace_back("Use Hash PvMove", SearchConfig::USE_TT_PV_MOVE_SORT,
                            [&](UciHandler*) { SearchConfig::USE_TT_PV_MOVE_SORT = getOption("Use Hash PvMove")->currentValue == "true"; });

  optionVector.emplace_back("Use Hash Quiescence", SearchConfig::USE_QS_TT,
                            [&](UciHandler*) { SearchConfig::USE_QS_TT = getOption("Use Hash Quiescence")->currentValue == "true"; });

  optionVector.emplace_back("Clear Hash",
                            [&](UciHandler* uciHandler) { uciHandler->getSearchPtr()->clearTT(); });

  optionVector.emplace_back("Use Killer Moves", SearchConfig::USE_KILLER_MOVES,
                            [&](UciHandler*) { SearchConfig::USE_KILLER_MOVES = getOption("Use Killer Moves")->currentValue == "true"; });

  optionVector.emplace_back("Use History Moves", SearchConfig::USE_HISTORY_MOVES,
                            [&](UciHandler*) { SearchConfig::USE_HISTORY_MOVES = getOption("Use History Moves")->currentValue == "true"; });

  optionVector.emplace_back("Use History Counter", SearchConfig::USE_HISTORY_COUNTER,
                            [&](UciHandler*) { SearchConfig::USE_HISTORY_COUNTER = getOption("Use History Counter")->currentValue == "true"; });

  optionVector.emplace_back("Use Mate Distance Pruning", SearchConfig::USE_MDP,
                            [&](UciHandler*) { SearchConfig::USE_MDP = getOption("Use Mate Distance Pruning")->currentValue == "true"; });

  optionVector.emplace_back("Use Quiescence", SearchConfig::USE_QUIESCENCE,
                            [&](UciHandler*) { SearchConfig::USE_QUIESCENCE = getOption("Use Quiescence")->currentValue == "true"; });

  optionVector.emplace_back("Use Quiescence Standpat", SearchConfig::USE_QS_STANDPAT_CUT,
                            [&](UciHandler*) { SearchConfig::USE_QS_STANDPAT_CUT = getOption("Use Quiescence Standpat")->currentValue == "true"; });

  optionVector.emplace_back("Use Quiescence SEE", SearchConfig::USE_QS_SEE,
                            [&](UciHandler*) { SearchConfig::USE_QS_SEE = getOption("Use Quiescence SEE")->currentValue == "true"; });

  optionVector.emplace_back("Use Razoring", SearchConfig::USE_RAZORING,
                            [&](UciHandler*) { SearchConfig::USE_RAZORING = getOption("Use Razoring")->currentValue == "true"; });

  optionVector.emplace_back("Razor Margin", SearchConfig::RAZOR_MARGIN, VALUE_MIN, VALUE_MAX,
                            [&](UciHandler*) { SearchConfig::RAZOR_MARGIN = static_cast<Value>(getInt(getOption("Razor Margin")->currentValue)); });

  optionVector.emplace_back("Use Reverse Futility Pruning", SearchConfig::USE_RFP,
                            [&](UciHandler*) { SearchConfig::USE_RFP = getOption("Use Reverse Futility Pruning")->currentValue == "true"; });

  optionVector.emplace_back("Use Null Move Pruning", SearchConfig::USE_NMP,
                            [&](UciHandler*) { SearchConfig::USE_NMP = getOption("Use Null Move Pruning")->currentValue == "true"; });

  optionVector.emplace_back("Null Move Depth", SearchConfig::NMP_DEPTH, 0, DEPTH_MAX,
                            [&](UciHandler*) { SearchConfig::NMP_DEPTH = static_cast<Depth>(getInt(getOption("Null Move Depth")->currentValue)); });

  optionVector.emplace_back("Null Depth Reduction", SearchConfig::NMP_REDUCTION, 0, DEPTH_MAX,
                            [&](UciHandler*) { SearchConfig::NMP_REDUCTION = static_cast<Depth>(getInt(getOption("Null Depth Reduction")->currentValue)); });

  optionVector.emplace_back("Use Internal Iterative Deepening", SearchConfig::USE_IID,
                            [&](UciHandler*) { SearchConfig::USE_IID = getOption("Use Internal Iterative Deepening")->currentValue == "true"; });

  optionVector.emplace_back("IID Move Depth", SearchConfig::IID_DEPTH, 0, DEPTH_MAX,
                            [&](UciHandler*) { SearchConfig::IID_DEPTH = static_cast<Depth>(getInt(getOption("IID Move Depth")->currentValue)); });

  optionVector.emplace_back("IID Depth Reduction", SearchConfig::IID_REDUCTION, 0, DEPTH_MAX,
                            [&](UciHandler*) { SearchConfig::IID_REDUCTION = static_cast<Depth>(getInt(getOption("IID Depth Reduction")->currentValue)); });

  optionVector.emplace_back("Use Futility Pruning", SearchConfig::USE_FP,
                            [&](UciHandler*) { SearchConfig::USE_FP = getOption("Use Futility Pruning")->currentValue == "true"; });

  optionVector.emplace_back("Use Quiescence Futility Pruning", SearchConfig::USE_QFP,
                            [&](UciHandler*) { SearchConfig::USE_QFP = getOption("Use Quiescence Futility Pruning")->currentValue == "true"; });

  optionVector.emplace_back("Use Late Move Reduction", SearchConfig::USE_LMR,
                            [&](UciHandler*) { SearchConfig::USE_LMR = getOption("Use Late Move Reduction")->currentValue == "true"; });

  optionVector.emplace_back("Use Late Move Pruning", SearchConfig::USE_LMP,
                            [&](UciHandler*) { SearchConfig::USE_LMP = getOption("Use Late Move Pruning")->currentValue == "true"; });

  optionVector.emplace_back("Use Extensions", SearchConfig::USE_EXTENSIONS,
                            [&](UciHandler*) { SearchConfig::USE_EXTENSIONS = getOption("Use Extensions")->currentValue == "true"; });

  optionVector.emplace_back("Use Check Extension", SearchConfig::USE_CHECK_EXT,
                            [&](UciHandler*) { SearchConfig::USE_CHECK_EXT = getOption("Use Check Extension")->currentValue == "true"; });

  optionVector.emplace_back("Use Threat Extension", SearchConfig::USE_THREAT_EXT,
                            [&](UciHandler*) { SearchConfig::USE_THREAT_EXT = getOption("Use Threat Extension")->currentValue == "true"; });

  optionVector.emplace_back("Use Extension Add", SearchConfig::USE_EXT_ADD_DEPTH,
                            [&](UciHandler*) { SearchConfig::USE_EXT_ADD_DEPTH = getOption("Use Extension Add")->currentValue == "true"; });

  optionVector.emplace_back("Use Hash Eval", SearchConfig::USE_EVAL_TT,
                            [&](UciHandler*) { SearchConfig::USE_EVAL_TT = getOption("Use Hash Eval")->currentValue == "true"; });

  optionVector.emplace_back("Use Lazy Eval", EvalConfig::USE_LAZY_EVAL,
                            [&](UciHandler*) { EvalConfig::USE_LAZY_EVAL = getOption("Use Lazy Eval")->currentValue == "true"; });

  optionVector.emplace_back("Use Pawn Eval", EvalConfig::USE_PAWN_EVAL,
                            [&](UciHandler*) { EvalConfig::USE_PAWN_EVAL = getOption("Use Pawn Eval")->currentValue == "true"; });

  optionVector.emplace_back("Use Pawn Hash", EvalConfig::USE_PAWN_TT,
                            [&](UciHandler*) { EvalConfig::USE_PAWN_TT = getOption("Use Pawn Hash")->currentValue == "true"; });

  optionVector.emplace_back("Pawn Hash Size", EvalConfig::PAWN_TT_SIZE_MB, 0, 1024,
                            [&](UciHandler*) { EvalConfig::PAWN_TT_SIZE_MB = static_cast<Depth>(getInt(getOption("Pawn Hash Size")->currentValue)); });

  // optionVector.emplace_back("***", [&](UciHandler* uciHandler) { });
}

const UciOption* UciOptions::getOption(const std::string& name) const {
  // find option entry
  const auto optionIterator = std::find_if(optionVector.begin(), optionVector.end(),
                                           [&](const UciOption& p) {
                                             return name == p.nameID;
                                           });
  if (optionIterator != optionVector.end()) {
    return &*optionIterator;
  }
  return nullptr;
}

bool UciOptions::setOption(UciHandler* uciHandler, const std::string& name, const std::string& value) {
  auto o = const_cast<UciOption*>(getOption(name));
  if (o) {
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
      str += "combo default " + defaultValue + " var " + varValue;
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

int UciOptions::getInt(const std::string& value) {
  try {
    int intValue = stoi(value);
    return intValue;
  } catch (...) {
    return 0;
  }
}
