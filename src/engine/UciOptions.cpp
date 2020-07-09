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

#include "UciOptions.h"
#include "Search.h"
#include "SearchConfig.h"
#include "UciHandler.h"

#include <boost/algorithm/string/trim.hpp>

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

  optionVector.emplace_back("Use Hash Eval", SearchConfig::USE_EVAL_TT,
                            [&](UciHandler*) { SearchConfig::USE_EVAL_TT = getOption("Use Hash Eval")->currentValue == "true"; });

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


  // optionVector.emplace_back("***", [&](UciHandler* uciHandler) { });

  //  MAP("Use_RFP",          UCI_Option("Use_RFP",          SearchConfig::USE_RFP));
  //  MAP("RFP_Margin",       UCI_Option("RFP_Margin",       SearchConfig::RFP_MARGIN, 0, VALUE_MAX));
  //  MAP("Use_NMP",          UCI_Option("Use_NMP",          SearchConfig::USE_NMP));
  //  MAP("NMP_Depth",        UCI_Option("NMP_Depth",        SearchConfig::NMP_DEPTH, 0, DEPTH_MAX));
  //  MAP("NMP_Reduction",    UCI_Option("NMP_Reduction",    SearchConfig::NMP_REDUCTION, 0, DEPTH_MAX));
  //  MAP("Use_NMPVer",       UCI_Option("Use_NMPVer",       SearchConfig::NMP_VERIFICATION));
  //  MAP("NMPV_Reduction",   UCI_Option("NMPV_Reduction",   SearchConfig::NMP_V_REDUCTION, 0, DEPTH_MAX));
  //  MAP("Use_EXT",          UCI_Option("Use_EXT",          SearchConfig::USE_EXTENSIONS));
  //  MAP("Use_FP",           UCI_Option("Use_FP",           SearchConfig::USE_FP));
  //  MAP("FP_Margin",        UCI_Option("FP_Margin",        SearchConfig::FP_MARGIN, 0, VALUE_MAX));
  //  MAP("Use_EFP",          UCI_Option("Use_EFP",          SearchConfig::USE_EFP));
  //  MAP("EFP_Margin",       UCI_Option("EFP_Margin",       SearchConfig::EFP_MARGIN, 0, VALUE_MAX));
  //  MAP("Use_LMR",          UCI_Option("Use_LMR",          SearchConfig::USE_LMR));
  //  MAP("LMR_Min_Depth",    UCI_Option("LMR_Min_Depth",    SearchConfig::LMR_MIN_DEPTH, 0, DEPTH_MAX));
  //  MAP("LMR_Min_Moves",    UCI_Option("LMR_Min_Moves",    SearchConfig::LMR_MIN_MOVES, 0, DEPTH_MAX));
  //  MAP("LMR_Reduction",    UCI_Option("LMR_Reduction",    SearchConfig::LMR_REDUCTION, 0, DEPTH_MAX));
  // @formatter:on
}

const UciOption* UciOptions::getOption(const std::string& name) const {
  // find option entry
  const auto optionIterator =
    std::find_if(optionVector.begin(), optionVector.end(),
                 [&](UciOption p) {
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
  boost::trim(str);// remove last newline
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
  int intValue = 0;
  try {
    intValue = stoi(value);
    return intValue;
  } catch (...) {
    return 0;
  }
}