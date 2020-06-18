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

#include "Search.h"
#include "SearchConfig.h"
#include "UciHandler.h"
#include "UciOptions.h"

#include <boost/algorithm/string/trim.hpp>

void UciOptions::initOptions() {
  optionVector.push_back(UciOption{"Clear Hash", [&](UciHandler* uciHandler) {
                                     uciHandler->getSearchPtr()->clearTT(); }});
  optionVector.push_back(UciOption{"Hash", SearchConfig::TT_SIZE_MB, 0, 4096, [&](UciHandler* uciHandler) {
                                     SearchConfig::TT_SIZE_MB = getInt(getOption("Hash")->currentValue);
                                     uciHandler->getSearchPtr()->resizeTT(); }});

  // @formatter:off
  //  MAP("Use_Hash",         UCI_Option("Use_Hash",         SearchConfig::USE_TT));
  //  MAP("Hash",             UCI_Option("Hash",             EngineConfig::hash, 0, TT::MAX_SIZE_MB));
  //  MAP("Ponder",           UCI_Option("Ponder",           EngineConfig::ponder));
  //  MAP("OwnBook",          UCI_Option("OwnBook",          SearchConfig::USE_BOOK));
  //  MAP("Use_AlphaBeta",    UCI_Option("Use_AlphaBeta",    SearchConfig::USE_ALPHABETA));
  //  MAP("Use_PVS",          UCI_Option("Use_PVS",          SearchConfig::USE_PVS));
  //  MAP("Use_Aspiration",   UCI_Option("Use_Aspiration",   SearchConfig::USE_ASPIRATION_WINDOW));
  //  MAP("Aspiration_Depth", UCI_Option("Aspiration_Depth", SearchConfig::ASPIRATION_START_DEPTH, 1, DEPTH_MAX));
  //  MAP("Use_Quiescence",   UCI_Option("Use_Quiescence",   SearchConfig::USE_QUIESCENCE));
  //  MAP("Max_Extra_Depth",  UCI_Option("Max_Extra_Depth",  SearchConfig::MAX_EXTRA_QDEPTH, 1, DEPTH_MAX));
  //  MAP("Use_QS_SEE",       UCI_Option("Use_QS_SEE",       SearchConfig::USE_QS_SEE));
  //  MAP("Use_KillerMoves",  UCI_Option("Use_KillerMoves",  SearchConfig::USE_KILLER_MOVES));
  //  MAP("No_Of_Killer",     UCI_Option("No_Of_Killer",     SearchConfig::NO_KILLER_MOVES, 1, 9));
  //  MAP("Use_PV_Sort",      UCI_Option("Use_PV_Sort",      SearchConfig::USE_PV_MOVE_SORT));
  //  MAP("Use_MDP",          UCI_Option("Use_MDP",          SearchConfig::USE_MDP));
  //  MAP("Use_MPP",          UCI_Option("Use_MPP",          SearchConfig::USE_MPP));
  //  MAP("Use_Standpat",     UCI_Option("Use_Standpat",     SearchConfig::USE_QS_STANDPAT_CUT));
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

const UciOption* UciOptions::getOption(std::string name) const {
  // find option entry
  const auto optionIterator =
    std::find_if(optionVector.begin(), optionVector.end(),
                 [&](UciOption p) {
                   return name == p.nameID;
                 });
  if (optionIterator != optionVector.end()) {
    return optionIterator.base();
  } else {
    return nullptr;
  }
}

bool UciOptions::setOption(UciHandler* uciHandler, const std::string name, const std::string value) {
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
  for (auto o : optionVector) {
    str += o.str() + "\n";
  }
  boost::trim(str); // remove last newline
  return str;
}

std::string UciOption::str() const {
  std::string str = "option name "+ nameID + " type ";
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

int UciOptions::getInt(const std::string &value) {
  int intValue = 0;
  try {
    intValue = stoi(value);
    return intValue;
  }
  catch (...) {
   return 0;
  }
}