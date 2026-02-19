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

#include "config/ConfigRegistry.h"
#include "config/EvalConfigData.h"
#include "config/SearchConfigData.h"
#include "engine/Search.h"
#include "engine/UciHandler.h"

#include <algorithm>

ConfigRegistry& ConfigRegistry::instance() {
  static ConfigRegistry instance;
  return instance;
}

ConfigRegistry::ConfigRegistry() {
  // Static assertions to catch struct size changes.
  // If you add/remove a member from SearchConfigData or EvalConfigData,
  // update these values AND add/remove the corresponding registry entry.
  // Note: Debug builds may have larger sizes due to different alignment/padding.
  // Uncomment fprintln and comment out static_assert to debug size changes.

  // fprintln("Size of SearchConfigData: {}", sizeof(SearchConfigData));
  // fprintln("Size of EvalConfigData: {}", sizeof(EvalConfigData));

#ifdef _MSC_VER
// Windows MSVC builds
#ifdef _DEBUG
  static_assert(sizeof(SearchConfigData) == 504,
                "SearchConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
  static_assert(sizeof(EvalConfigData) == 256,
                "EvalConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
#else
  static_assert(sizeof(SearchConfigData) == 472,
                "SearchConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
  static_assert(sizeof(EvalConfigData) == 248,
                "EvalConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
#endif
#elif defined(__GNUC__) || defined(__clang__)
// Linux GCC/Clang builds (including WSL)
#ifdef NDEBUG
  // Release build
  static_assert(sizeof(SearchConfigData) == 472,
                "SearchConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
  static_assert(sizeof(EvalConfigData) == 248,
                "EvalConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
#else
  // Debug build
  static_assert(sizeof(SearchConfigData) == 472,
                "SearchConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
  static_assert(sizeof(EvalConfigData) == 248,
                "EvalConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
#endif
#endif

  initializeSearchDefinitions();
  initializeEvalDefinitions();
}

std::span<const ConfigDef> ConfigRegistry::all() const {
  return definitions_;
}

std::vector<const ConfigDef*> ConfigRegistry::byDomain(const ConfigDomain domain) const {
  std::vector<const ConfigDef*> result;
  for (const auto& definition : definitions_) {
    if (definition.domain == domain) {
      result.push_back(&definition);
    }
  }
  return result;
}

std::vector<const ConfigDef*> ConfigRegistry::uciOptions() const {
  std::vector<const ConfigDef*> result;
  for (const auto& definition : definitions_) {
    if (definition.exposure.uci) {
      result.push_back(&definition);
    }
  }
  return result;
}

std::vector<const ConfigDef*> ConfigRegistry::yamlOptions() const {
  std::vector<const ConfigDef*> result;
  for (const auto& definition : definitions_) {
    if (definition.exposure.yaml) {
      result.push_back(&definition);
    }
  }
  return result;
}

std::vector<const ConfigDef*> ConfigRegistry::displayOptions() const {
  std::vector<const ConfigDef*> result;
  for (const auto& definition : definitions_) {
    if (definition.exposure.display) {
      result.push_back(&definition);
    }
  }
  return result;
}

const ConfigDef* ConfigRegistry::find(const std::string& name) const {
  for (const auto& definition : definitions_) {
    if (definition.name == name) {
      return &definition;
    }
  }
  return nullptr;
}

const ConfigDef* ConfigRegistry::findByUciName(const std::string& uciName) const {
  // Case-insensitive comparison
  auto toLower = [](const std::string& s) {
    std::string result = s;
    std::ranges::transform(result, result.begin(),
                           [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
  };

  const std::string lowerName = toLower(uciName);
  for (const auto& definition : definitions_) {
    if (toLower(definition.uciName) == lowerName) {
      return &definition;
    }
  }
  return nullptr;
}

std::size_t ConfigRegistry::searchConfigCount() const {
  std::size_t count = 0;
  for (const auto& definition : definitions_) {
    if (definition.domain == ConfigDomain::General || definition.domain == ConfigDomain::Search) {
      ++count;
    }
  }
  return count;
}

std::size_t ConfigRegistry::evalConfigCount() const {
  std::size_t count = 0;
  for (const auto& definition : definitions_) {
    if (definition.domain == ConfigDomain::Eval) {
      ++count;
    }
  }
  return count;
}

//=============================================================================
// Search Config Definitions
//=============================================================================

void ConfigRegistry::initializeSearchDefinitions() {
  using enum ConfigValueType;
  using enum ConfigDomain;

  // Helper lambdas for Search configs
  auto searchGetter = [](auto member) {
    return [member](const SearchConfigData& s, const EvalConfigData&) {
      return configToString(s.*member);
    };
  };

  auto searchSetter = [](auto member, auto parser) {
    return [member, parser](SearchConfigData& s, EvalConfigData&, const std::string& v) {
      s.*member = parser(v);
    };
  };

  // clang-format off

  //===========================================================================
  // DEBUG / INTERNAL
  //===========================================================================
  definitions_.push_back({
    .name = "CONFIG_SOURCE",
    .uciName = "",
    .description = "Source of configuration (internal tracking)",
    .valueType = String,
    .domain = Debug,
    .defaultValue = "fallback",
    .exposure = {.uci = false, .yaml = true, .display = false},
    .getter = searchGetter(&SearchConfigData::CONFIG_SOURCE),
    .setter = searchSetter(&SearchConfigData::CONFIG_SOURCE, parseString)
  });

  //===========================================================================
  // GENERAL SETTINGS
  //===========================================================================
  definitions_.push_back({
    .name = "MOVE_OVERHEAD_MS",
    .uciName = "Move Overhead",
    .description = "Safety margin for time management (ms)",
    .valueType = Int,
    .domain = General,
    .defaultValue = "10",
    .minValue = 0,
    .maxValue = 5000,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::MOVE_OVERHEAD_MS),
    .setter = searchSetter(&SearchConfigData::MOVE_OVERHEAD_MS, parseInt)
  });

  definitions_.push_back({
    .name = "USE_BOOK",
    .uciName = "OwnBook",
    .description = "Use internal opening book",
    .valueType = Bool,
    .domain = General,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_BOOK),
    .setter = searchSetter(&SearchConfigData::USE_BOOK, parseBool)
  });

  definitions_.push_back({
    .name = "BOOK_PATH",
    .uciName = "Book Path",
    .description = "Path to opening book file",
    .valueType = String,
    .domain = General,
    .defaultValue = "./books/book.txt",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::BOOK_PATH),
    .setter = searchSetter(&SearchConfigData::BOOK_PATH, parseString)
  });

  definitions_.push_back({
    .name = "BOOK_TYPE",
    .uciName = "Book Format",
    .description = "Opening book format (SIMPLE, PGN, etc.)",
    .valueType = String,
    .domain = General,
    .defaultValue = "SIMPLE",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::BOOK_TYPE),
    .setter = searchSetter(&SearchConfigData::BOOK_TYPE, parseString)
  });

  definitions_.push_back({
    .name = "USE_PONDER",
    .uciName = "Ponder",
    .description = "Enable pondering (thinking on opponent's time)",
    .valueType = Bool,
    .domain = General,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_PONDER),
    .setter = searchSetter(&SearchConfigData::USE_PONDER, parseBool)
  });

  //===========================================================================
  // CORE SEARCH FLAGS
  //===========================================================================
  definitions_.push_back({
    .name = "USE_ALPHABETA",
    .uciName = "Use AlphaBeta",
    .description = "Enable alpha-beta pruning",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_ALPHABETA),
    .setter = searchSetter(&SearchConfigData::USE_ALPHABETA, parseBool)
  });

  definitions_.push_back({
    .name = "USE_PVS",
    .uciName = "Use Pvs",
    .description = "Enable Principal Variation Search",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_PVS),
    .setter = searchSetter(&SearchConfigData::USE_PVS, parseBool)
  });

  definitions_.push_back({
    .name = "USE_ASP",
    .uciName = "Use Aspiration",
    .description = "Enable aspiration windows",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_ASP),
    .setter = searchSetter(&SearchConfigData::USE_ASP, parseBool)
  });

  definitions_.push_back({
    .name = "USE_QUIESCENCE",
    .uciName = "Use Quiescence",
    .description = "Enable quiescence search",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_QUIESCENCE),
    .setter = searchSetter(&SearchConfigData::USE_QUIESCENCE, parseBool)
  });

  //===========================================================================
  // TRANSPOSITION TABLE
  //===========================================================================
  definitions_.push_back({
    .name = "USE_TT",
    .uciName = "Use Hash",
    .description = "Enable transposition table",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_TT),
    .setter = searchSetter(&SearchConfigData::USE_TT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_TT_VALUE",
    .uciName = "Use Hash Value",
    .description = "Use TT values for cutoffs",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_TT_VALUE),
    .setter = searchSetter(&SearchConfigData::USE_TT_VALUE, parseBool)
  });

  definitions_.push_back({
    .name = "USE_EVAL_TT",
    .uciName = "Use Eval TT",
    .description = "Use TT for evaluation cache",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_EVAL_TT),
    .setter = searchSetter(&SearchConfigData::USE_EVAL_TT, parseBool)
  });

  definitions_.push_back({
    .name = "TT_SIZE_MB",
    .uciName = "Hash",
    .description = "Transposition table size in MB",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "64",
    .minValue = 0,
    .maxValue = 4096,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::TT_SIZE_MB),
    .setter = searchSetter(&SearchConfigData::TT_SIZE_MB, parseInt),
    // Custom handler: resize TT after changing size
    .customUciHandler = [](const UciHandler* h) {
      if (h && h->getSearchPtr()) {
        h->getSearchPtr()->resizeTT();
      }
    }
  });

  definitions_.push_back({
    .name = "USE_QS_TT",
    .uciName = "Use Hash Quiescence",
    .description = "Use TT in quiescence search",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_QS_TT),
    .setter = searchSetter(&SearchConfigData::USE_QS_TT, parseBool)
  });

  //===========================================================================
  // SYZYGY TABLEBASE SETTINGS
  //===========================================================================
  definitions_.push_back({
    .name = "TB_PATH",
    .uciName = "SyzygyPath",
    .description = "Path to Syzygy tablebase files (empty = disabled)",
    .valueType = String,
    .domain = General,
    .defaultValue = "",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::TB_PATH),
    .setter = searchSetter(&SearchConfigData::TB_PATH, parseString)
  });

  // Root probing settings (once per search, for best move selection)
  definitions_.push_back({
    .name = "USE_TB_PROBE_ROOT",
    .uciName = "Use Syzygy Probe Root",
    .description = "Probe tablebases at root for best move selection",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_TB_PROBE_ROOT),
    .setter = searchSetter(&SearchConfigData::USE_TB_PROBE_ROOT, parseBool)
  });

  definitions_.push_back({
    .name = "TB_ROOT_IMMEDIATE",
    .uciName = "Syzygy Root Immediate",
    .description = "Return TB move immediately without searching (false = search for PV)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "false",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::TB_ROOT_IMMEDIATE),
    .setter = searchSetter(&SearchConfigData::TB_ROOT_IMMEDIATE, parseBool)
  });

  // Search probing settings (during tree search, for cutoffs)
  definitions_.push_back({
    .name = "USE_TB_PROBE_SEARCH",
    .uciName = "Use Syzygy Probe Search",
    .description = "Probe tablebases during search for cutoffs",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_TB_PROBE_SEARCH),
    .setter = searchSetter(&SearchConfigData::USE_TB_PROBE_SEARCH, parseBool)
  });

  definitions_.push_back({
    .name = "USE_TB_PROBE_PV",
    .uciName = "",  // Not exposed via UCI - internal tuning option
    .description = "Probe tablebases on PV nodes (false = only non-PV nodes for cutoffs)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = false, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_TB_PROBE_PV),
    .setter = searchSetter(&SearchConfigData::USE_TB_PROBE_PV, parseBool)
  });

  definitions_.push_back({
    .name = "TB_PROBE_DEPTH",
    .uciName = "Syzygy Probe Depth",
    .description = "Minimum remaining depth to probe WDL during search (0 = always)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "1",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::TB_PROBE_DEPTH),
    .setter = searchSetter(&SearchConfigData::TB_PROBE_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "TB_PROBE_LIMIT",
    .uciName = "Syzygy Probe Limit",
    .description = "Maximum pieces for search TB probing (3-7)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "6",
    .minValue = 3,
    .maxValue = 7,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::TB_PROBE_LIMIT),
    .setter = searchSetter(&SearchConfigData::TB_PROBE_LIMIT, parseInt)
  });

  definitions_.push_back({
    .name = "TB_RULE50_THRESHOLD",
    .uciName = "Syzygy 50 Move Rule",
    .description = "HalfMoveClock threshold for DTZ accuracy check (>=100 disables)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "80",
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::TB_RULE50_THRESHOLD),
    .setter = searchSetter(&SearchConfigData::TB_RULE50_THRESHOLD, parseInt)
  });


  //===========================================================================
  // MOVE SORTING
  //===========================================================================
  definitions_.push_back({
    .name = "USE_TT_PV_MOVE_SORT",
    .uciName = "Use TT Move as PvMove",
    .description = "Prioritize TT/PV moves in move ordering",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_TT_PV_MOVE_SORT),
    .setter = searchSetter(&SearchConfigData::USE_TT_PV_MOVE_SORT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_KILLER_MOVES",
    .uciName = "Use Killer Moves",
    .description = "Enable killer move heuristic",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_KILLER_MOVES),
    .setter = searchSetter(&SearchConfigData::USE_KILLER_MOVES, parseBool)
  });

  definitions_.push_back({
    .name = "USE_HISTORY_COUNTER",
    .uciName = "Use History Counter",
    .description = "Enable counter-move history",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_HISTORY_COUNTER),
    .setter = searchSetter(&SearchConfigData::USE_HISTORY_COUNTER, parseBool)
  });

  definitions_.push_back({
    .name = "USE_HISTORY_MOVES",
    .uciName = "Use History Moves",
    .description = "Enable history heuristic for move ordering",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_HISTORY_MOVES),
    .setter = searchSetter(&SearchConfigData::USE_HISTORY_MOVES, parseBool)
  });

  definitions_.push_back({
    .name = "USE_IID",
    .uciName = "Use Internal Iterative Deepening",
    .description = "Enable Internal Iterative Deepening",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_IID),
    .setter = searchSetter(&SearchConfigData::USE_IID, parseBool)
  });

  definitions_.push_back({
    .name = "IID_DEPTH",
    .uciName = "IID Move Depth",
    .description = "Minimum depth to trigger IID",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "6",
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::IID_DEPTH),
    .setter = searchSetter(&SearchConfigData::IID_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "IID_REDUCTION",
    .uciName = "IID Depth Reduction",
    .description = "Depth reduction for IID search",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "2",
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::IID_REDUCTION),
    .setter = searchSetter(&SearchConfigData::IID_REDUCTION, parseInt)
  });

  //===========================================================================
  // PRUNING - GENERAL
  //===========================================================================
  definitions_.push_back({
    .name = "USE_MDP",
    .uciName = "Use Mate Distance Pruning",
    .description = "Enable mate distance pruning",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_MDP),
    .setter = searchSetter(&SearchConfigData::USE_MDP, parseBool)
  });

  definitions_.push_back({
    .name = "USE_QS_STANDPAT_CUT",
    .uciName = "Use Quiescence Standpat",
    .description = "Enable stand-pat cutoff in quiescence",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_QS_STANDPAT_CUT),
    .setter = searchSetter(&SearchConfigData::USE_QS_STANDPAT_CUT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_QS_SEE",
    .uciName = "Use Quiescence SEE",
    .description = "Enable SEE pruning in quiescence",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_QS_SEE),
    .setter = searchSetter(&SearchConfigData::USE_QS_SEE, parseBool)
  });

  //===========================================================================
  // RAZORING
  //===========================================================================
  definitions_.push_back({
    .name = "USE_RAZORING",
    .uciName = "Use Razoring",
    .description = "Enable razoring pruning",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_RAZORING),
    .setter = searchSetter(&SearchConfigData::USE_RAZORING, parseBool)
  });

  definitions_.push_back({
    .name = "RAZOR_MARGIN",
    .uciName = "Razor Margin",
    .description = "Razoring margin in centipawns",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "531",
    .minValue = 0,
    .maxValue = 1000,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::RAZOR_MARGIN),
    .setter = searchSetter(&SearchConfigData::RAZOR_MARGIN, parseInt)
  });

  //===========================================================================
  // REVERSE FUTILITY PRUNING
  //===========================================================================
  definitions_.push_back({
    .name = "USE_RFP",
    .uciName = "Use Reverse Futility Pruning",
    .description = "Enable reverse futility pruning",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_RFP),
    .setter = searchSetter(&SearchConfigData::USE_RFP, parseBool)
  });

  definitions_.push_back({
    .name = "RFP_MARGIN",
    .uciName = "",
    .description = "Reverse futility pruning margins by depth",
    .valueType = IntArray,
    .domain = Search,
    .defaultValue = "0,200,400,800",
    .exposure = {.uci = false, .yaml = true, .display = true},
    .getter = [](const SearchConfigData& s, const EvalConfigData&) {
      return arrayToString(s.RFP_MARGIN);
    },
    .setter = [](SearchConfigData& s, EvalConfigData&, const std::string& v) {
      parseArray(v, s.RFP_MARGIN);
    }
  });

  //===========================================================================
  // NULL MOVE PRUNING
  //===========================================================================
  definitions_.push_back({
    .name = "USE_NMP",
    .uciName = "Use Null Move Pruning",
    .description = "Enable null move pruning",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_NMP),
    .setter = searchSetter(&SearchConfigData::USE_NMP, parseBool)
  });

  definitions_.push_back({
    .name = "NMP_DEPTH",
    .uciName = "Null Move Depth",
    .description = "Minimum depth for null move pruning",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "3",
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::NMP_DEPTH),
    .setter = searchSetter(&SearchConfigData::NMP_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "NMP_REDUCTION",
    .uciName = "Null Depth Reduction",
    .description = "Depth reduction for null move search",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "2",
    .minValue = 1,
    .maxValue = 6,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::NMP_REDUCTION),
    .setter = searchSetter(&SearchConfigData::NMP_REDUCTION, parseInt)
  });

  definitions_.push_back({
    .name = "USE_NMP_VERIFY",
    .uciName = "Use Null Move Verification",
    .description = "Enable null move verification search",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_NMP_VERIFY),
    .setter = searchSetter(&SearchConfigData::USE_NMP_VERIFY, parseBool)
  });

  definitions_.push_back({
    .name = "NMP_VERIFY_MIN_DEPTH",
    .uciName = "Null Move Verify Min Depth",
    .description = "Minimum depth for null move verification",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "6",
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::NMP_VERIFY_MIN_DEPTH),
    .setter = searchSetter(&SearchConfigData::NMP_VERIFY_MIN_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "NMP_VERIFY_MARGIN",
    .uciName = "Null Move Verify Margin",
    .description = "Depth margin for null move verification",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "2",
    .minValue = 0,
    .maxValue = 10,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::NMP_VERIFY_MARGIN),
    .setter = searchSetter(&SearchConfigData::NMP_VERIFY_MARGIN, parseInt)
  });

  definitions_.push_back({
    .name = "NMP_NEAR_MATE_MARGIN",
    .uciName = "Null Move Near Mate Margin",
    .description = "Margin for near-mate null move check",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "64",
    .minValue = 0,
    .maxValue = 200,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::NMP_NEAR_MATE_MARGIN),
    .setter = searchSetter(&SearchConfigData::NMP_NEAR_MATE_MARGIN, parseInt)
  });

  definitions_.push_back({
    .name = "USE_NMP_ZUG_GUARD",
    .uciName = "Use Null Move Zugzwang Guard",
    .description = "Enable zugzwang guard for null move",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_NMP_ZUG_GUARD),
    .setter = searchSetter(&SearchConfigData::USE_NMP_ZUG_GUARD, parseBool)
  });

  definitions_.push_back({
    .name = "NMP_ZUG_NONPAWN_THRESHOLD",
    .uciName = "Null Move Zug NonPawn Threshold",
    .description = "Non-pawn piece threshold for zugzwang guard",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "0",
    .minValue = 0,
    .maxValue = 10,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::NMP_ZUG_NONPAWN_THRESHOLD),
    .setter = searchSetter(&SearchConfigData::NMP_ZUG_NONPAWN_THRESHOLD, parseInt)
  });

  //===========================================================================
  // FUTILITY PRUNING
  //===========================================================================
  definitions_.push_back({
    .name = "USE_FP",
    .uciName = "Use Futility Pruning",
    .description = "Enable futility pruning",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_FP),
    .setter = searchSetter(&SearchConfigData::USE_FP, parseBool)
  });

  definitions_.push_back({
    .name = "USE_QFP",
    .uciName = "Use Quiescence Futility Pruning",
    .description = "Enable futility pruning in quiescence",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_QFP),
    .setter = searchSetter(&SearchConfigData::USE_QFP, parseBool)
  });

  definitions_.push_back({
    .name = "FP_MARGIN",
    .uciName = "",
    .description = "Futility pruning margins by depth",
    .valueType = IntArray,
    .domain = Search,
    .defaultValue = "0,100,200,300,500,900,1200",
    .exposure = {.uci = false, .yaml = true, .display = true},
    .getter = [](const SearchConfigData& s, const EvalConfigData&) {
      return arrayToString(s.FP_MARGIN);
    },
    .setter = [](SearchConfigData& s, EvalConfigData&, const std::string& v) {
      parseArray(v, s.FP_MARGIN);
    }
  });

  //===========================================================================
  // LMR / LMP
  //===========================================================================
  definitions_.push_back({
    .name = "USE_LMR",
    .uciName = "Use Late Move Reduction",
    .description = "Enable Late Move Reductions",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_LMR),
    .setter = searchSetter(&SearchConfigData::USE_LMR, parseBool)
  });

  definitions_.push_back({
    .name = "LMR_MIN_DEPTH",
    .uciName = "LMR Min Depth",
    .description = "Minimum depth for LMR",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "2",
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::LMR_MIN_DEPTH),
    .setter = searchSetter(&SearchConfigData::LMR_MIN_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "LMR_MIN_MOVES",
    .uciName = "LMR Min Moves",
    .description = "Minimum moves searched before LMR",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "2",
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::LMR_MIN_MOVES),
    .setter = searchSetter(&SearchConfigData::LMR_MIN_MOVES, parseInt)
  });

  definitions_.push_back({
    .name = "LMR_USE_LOG_FORMULA",
    .uciName = "LMR Use Log Formula",
    .description = "Use logarithmic formula instead of linear for LMR",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::LMR_USE_LOG_FORMULA),
    .setter = searchSetter(&SearchConfigData::LMR_USE_LOG_FORMULA, parseBool)
  });

  definitions_.push_back({
    .name = "LMR_LOG_BASE_DIV",
    .uciName = "LMR Log Base Divisor Pct",
    .description = "Divisor for log formula: log(d)*log(m)/divisor",
    .valueType = Double,
    .domain = Search,
    .defaultValue = "1.50",
    .minValue = 50,
    .maxValue = 500,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = [](const SearchConfigData& s, const EvalConfigData&) {
      return configToString(s.LMR_LOG_BASE_DIV);
    },
    .setter = [](SearchConfigData& s, EvalConfigData&, const std::string& v) {
      s.LMR_LOG_BASE_DIV = parseDouble(v);
    }
  });

  definitions_.push_back({
    .name = "USE_LMP",
    .uciName = "Use Late Move Pruning",
    .description = "Enable Late Move Pruning",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_LMP),
    .setter = searchSetter(&SearchConfigData::USE_LMP, parseBool)
  });

  definitions_.push_back({
    .name = "LMP_MOVES",
    .uciName = "",
    .description = "Late move pruning move count thresholds by depth",
    .valueType = IntArray,
    .domain = Search,
    .defaultValue = "0,7,9,11,13,15,17,19,22,24,27,29,32,35,38,41",
    .exposure = {.uci = false, .yaml = true, .display = true},
    .getter = [](const SearchConfigData& s, const EvalConfigData&) {
      return arrayToString(s.LMP_MOVES);
    },
    .setter = [](SearchConfigData& s, EvalConfigData&, const std::string& v) {
      parseArray(v, s.LMP_MOVES);
    }
  });

  //===========================================================================
  // EXTENSIONS
  //===========================================================================
  definitions_.push_back({
    .name = "USE_EXTENSIONS",
    .uciName = "Use Extensions",
    .description = "Enable search extensions",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_EXTENSIONS),
    .setter = searchSetter(&SearchConfigData::USE_EXTENSIONS, parseBool)
  });

  definitions_.push_back({
    .name = "USE_CHECK_EXT",
    .uciName = "Use Check Extension",
    .description = "Enable check extension",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_CHECK_EXT),
    .setter = searchSetter(&SearchConfigData::USE_CHECK_EXT, parseBool)
  });

  definitions_.push_back({
    .name = "CHECK_EXT_EARLY_LIMIT",
    .uciName = "Check Ext Early Limit",
    .description = "Only extend checks in first N moves per node",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "3",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::CHECK_EXT_EARLY_LIMIT),
    .setter = searchSetter(&SearchConfigData::CHECK_EXT_EARLY_LIMIT, parseInt)
  });

  definitions_.push_back({
    .name = "USE_THREAT_EXT",
    .uciName = "Use Threat Extension",
    .description = "Enable threat extension",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "false",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_THREAT_EXT),
    .setter = searchSetter(&SearchConfigData::USE_THREAT_EXT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_EXT_ADD_DEPTH",
    .uciName = "Use Extension Add",
    .description = "Add depth for extensions",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_EXT_ADD_DEPTH),
    .setter = searchSetter(&SearchConfigData::USE_EXT_ADD_DEPTH, parseBool)
  });

  //===========================================================================
  // SINGULAR EXTENSIONS
  //===========================================================================
  definitions_.push_back({
    .name = "USE_SINGULAR_EXT",
    .uciName = "Use Singular Extension",
    .description = "Enable singular extension",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_SINGULAR_EXT),
    .setter = searchSetter(&SearchConfigData::USE_SINGULAR_EXT, parseBool)
  });

  definitions_.push_back({
    .name = "SINGULAR_MARGIN",
    .uciName = "Singular Margin",
    .description = "Centipawns below TT value to consider singular",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "64",
    .minValue = 0,
    .maxValue = 200,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::SINGULAR_MARGIN),
    .setter = searchSetter(&SearchConfigData::SINGULAR_MARGIN, parseInt)
  });

  definitions_.push_back({
    .name = "SINGULAR_MIN_DEPTH",
    .uciName = "Singular Min Depth",
    .description = "Minimum depth to attempt singular extension",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "8",
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::SINGULAR_MIN_DEPTH),
    .setter = searchSetter(&SearchConfigData::SINGULAR_MIN_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "SINGULAR_REDUCTION",
    .uciName = "Singular Reduction",
    .description = "Depth reduction for verification search",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "4",
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::SINGULAR_REDUCTION),
    .setter = searchSetter(&SearchConfigData::SINGULAR_REDUCTION, parseInt)
  });

  //===========================================================================
  // TIME MANAGEMENT - MOVES LEFT MODEL
  //===========================================================================
  definitions_.push_back({
    .name = "MOVES_LEFT_OPENING",
    .uciName = "Moves Left Opening",
    .description = "Estimated moves left in opening phase",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "36",
    .minValue = 5,
    .maxValue = 60,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::MOVES_LEFT_OPENING),
    .setter = searchSetter(&SearchConfigData::MOVES_LEFT_OPENING, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_MIDGAME",
    .uciName = "Moves Left Midgame",
    .description = "Estimated moves left in midgame phase",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "28",
    .minValue = 5,
    .maxValue = 60,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::MOVES_LEFT_MIDGAME),
    .setter = searchSetter(&SearchConfigData::MOVES_LEFT_MIDGAME, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_ENDGAME",
    .uciName = "Moves Left Endgame",
    .description = "Estimated moves left in endgame phase",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "16",
    .minValue = 5,
    .maxValue = 60,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::MOVES_LEFT_ENDGAME),
    .setter = searchSetter(&SearchConfigData::MOVES_LEFT_ENDGAME, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_LOW_MAT",
    .uciName = "Moves Left Low Material",
    .description = "Estimated moves left in low material endgame",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "10",
    .minValue = 1,
    .maxValue = 30,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::MOVES_LEFT_LOW_MAT),
    .setter = searchSetter(&SearchConfigData::MOVES_LEFT_LOW_MAT, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_QUEENLESS",
    .uciName = "Moves Left Queenless",
    .description = "Estimated moves left in queenless middlegame",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "22",
    .minValue = 5,
    .maxValue = 60,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::MOVES_LEFT_QUEENLESS),
    .setter = searchSetter(&SearchConfigData::MOVES_LEFT_QUEENLESS, parseInt)
  });

  //===========================================================================
  // TIME MANAGEMENT - THRESHOLDS
  //===========================================================================
  definitions_.push_back({
    .name = "NPP_HEAVY_THRESHOLD",
    .uciName = "NPP Heavy Threshold",
    .description = "Non-pawn pieces threshold for heavy position",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "10",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::NPP_HEAVY_THRESHOLD),
    .setter = searchSetter(&SearchConfigData::NPP_HEAVY_THRESHOLD, parseInt)
  });

  definitions_.push_back({
    .name = "NPP_LIGHT_THRESHOLD",
    .uciName = "NPP Light Threshold",
    .description = "Non-pawn pieces threshold for light position",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "4",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::NPP_LIGHT_THRESHOLD),
    .setter = searchSetter(&SearchConfigData::NPP_LIGHT_THRESHOLD, parseInt)
  });

  //===========================================================================
  // TIME MANAGEMENT - REPETITION & CLAMPS
  //===========================================================================
  definitions_.push_back({
    .name = "REPETITION_HMC_HIGH",
    .uciName = "Repetition HMC High",
    .description = "Half-move clock threshold for repetition risk",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "80",
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::REPETITION_HMC_HIGH),
    .setter = searchSetter(&SearchConfigData::REPETITION_HMC_HIGH, parseInt)
  });

  definitions_.push_back({
    .name = "REPETITION_RISK_PENALTY",
    .uciName = "Repetition Risk Penalty",
    .description = "Moves-left penalty when repetition risk is high",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "6",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::REPETITION_RISK_PENALTY),
    .setter = searchSetter(&SearchConfigData::REPETITION_RISK_PENALTY, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_MIN_CLAMP",
    .uciName = "Moves Left Min Clamp",
    .description = "Minimum clamped value for moves-left estimate",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "6",
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::MOVES_LEFT_MIN_CLAMP),
    .setter = searchSetter(&SearchConfigData::MOVES_LEFT_MIN_CLAMP, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_MAX_CLAMP",
    .uciName = "Moves Left Max Clamp",
    .description = "Maximum clamped value for moves-left estimate",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "50",
    .minValue = 10,
    .maxValue = 100,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::MOVES_LEFT_MAX_CLAMP),
    .setter = searchSetter(&SearchConfigData::MOVES_LEFT_MAX_CLAMP, parseInt)
  });

  //===========================================================================
  // TIME MANAGEMENT - BESTMOVE INSTABILITY
  //===========================================================================
  definitions_.push_back({
    .name = "USE_BESTMOVE_INSTABILITY",
    .uciName = "Use BestMove Instability",
    .description = "Enable best-move instability tracking for time management",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::USE_BESTMOVE_INSTABILITY),
    .setter = searchSetter(&SearchConfigData::USE_BESTMOVE_INSTABILITY, parseBool)
  });

  definitions_.push_back({
    .name = "INSTABILITY_MIN_DEPTH",
    .uciName = "Instability Min Depth",
    .description = "Minimum depth to start instability tracking",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "5",
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::INSTABILITY_MIN_DEPTH),
    .setter = searchSetter(&SearchConfigData::INSTABILITY_MIN_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "INSTABILITY_STABLE_COUNT",
    .uciName = "Instability Stable Count",
    .description = "Consecutive stable iterations to trigger time reduction",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "3",
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::INSTABILITY_STABLE_COUNT),
    .setter = searchSetter(&SearchConfigData::INSTABILITY_STABLE_COUNT, parseInt)
  });

  definitions_.push_back({
    .name = "INSTABILITY_CHANGE_THRESHOLD",
    .uciName = "Instability Change Threshold",
    .description = "Number of best-move changes to trigger time extension",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "2",
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter(&SearchConfigData::INSTABILITY_CHANGE_THRESHOLD),
    .setter = searchSetter(&SearchConfigData::INSTABILITY_CHANGE_THRESHOLD, parseInt)
  });

  definitions_.push_back({
    .name = "INSTABILITY_STABLE_FACTOR",
    .uciName = "Instability Stable Factor Pct",
    .description = "Multiply remaining time by this when stable (< 1.0)",
    .valueType = Double,
    .domain = Search,
    .defaultValue = "0.80",
    .minValue = 50,
    .maxValue = 100,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = [](const SearchConfigData& s, const EvalConfigData&) {
      return configToString(s.INSTABILITY_STABLE_FACTOR);
    },
    .setter = [](SearchConfigData& s, EvalConfigData&, const std::string& v) {
      s.INSTABILITY_STABLE_FACTOR = parseDouble(v);
    }
  });

  definitions_.push_back({
    .name = "INSTABILITY_EXTEND_FACTOR",
    .uciName = "Instability Extend Factor Pct",
    .description = "Multiply remaining time by this when unstable (> 1.0)",
    .valueType = Double,
    .domain = Search,
    .defaultValue = "1.25",
    .minValue = 100,
    .maxValue = 200,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = [](const SearchConfigData& s, const EvalConfigData&) {
      return configToString(s.INSTABILITY_EXTEND_FACTOR);
    },
    .setter = [](SearchConfigData& s, EvalConfigData&, const std::string& v) {
      s.INSTABILITY_EXTEND_FACTOR = parseDouble(v);
    }
  });

  // clang-format on
}

//=============================================================================
// Eval Config Definitions
//=============================================================================

void ConfigRegistry::initializeEvalDefinitions() {
  using enum ConfigValueType;
  using enum ConfigDomain;

  // Helper lambdas for Eval configs
  auto evalGetter = [](auto member) {
    return [member](const SearchConfigData&, const EvalConfigData& e) {
      return configToString(e.*member);
    };
  };

  auto evalSetter = [](auto member, auto parser) {
    return [member, parser](SearchConfigData&, EvalConfigData& e, const std::string& v) {
      e.*member = parser(v);
    };
  };

  // clang-format off

  //===========================================================================
  // DEBUG / INTERNAL
  //===========================================================================
  definitions_.push_back({
    .name = "EVAL_CONFIG_SOURCE",
    .uciName = "",
    .description = "Source of eval configuration (internal tracking)",
    .valueType = String,
    .domain = Debug,
    .defaultValue = "fallback",
    .exposure = {.uci = false, .yaml = true, .display = false},
    .getter = evalGetter(&EvalConfigData::EVAL_CONFIG_SOURCE),
    .setter = evalSetter(&EvalConfigData::EVAL_CONFIG_SOURCE, parseString)
  });

  //===========================================================================
  // MASTER TOGGLES
  //===========================================================================
  definitions_.push_back({
    .name = "USE_MATERIAL",
    .uciName = "Use Material",
    .description = "Enable material evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_MATERIAL),
    .setter = evalSetter(&EvalConfigData::USE_MATERIAL, parseBool)
  });

  definitions_.push_back({
    .name = "USE_POSITIONAL",
    .uciName = "Use Positional",
    .description = "Enable positional evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_POSITIONAL),
    .setter = evalSetter(&EvalConfigData::USE_POSITIONAL, parseBool)
  });

  //===========================================================================
  // TEMPO
  //===========================================================================
  definitions_.push_back({
    .name = "USE_TEMPO",
    .uciName = "Use Tempo",
    .description = "Enable tempo bonus",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_TEMPO),
    .setter = evalSetter(&EvalConfigData::USE_TEMPO, parseBool)
  });

  definitions_.push_back({
    .name = "TEMPO",
    .uciName = "Tempo Bonus",
    .description = "Tempo bonus in centipawns",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "34",
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::TEMPO),
    .setter = evalSetter(&EvalConfigData::TEMPO, parseInt)
  });

  //===========================================================================
  // LAZY EVAL
  //===========================================================================
  definitions_.push_back({
    .name = "USE_LAZY_EVAL",
    .uciName = "Use Lazy Eval",
    .description = "Enable lazy evaluation cutoff",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_LAZY_EVAL),
    .setter = evalSetter(&EvalConfigData::USE_LAZY_EVAL, parseBool)
  });

  definitions_.push_back({
    .name = "LAZY_THRESHOLD",
    .uciName = "Lazy Threshold",
    .description = "Lazy evaluation threshold in centipawns",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "700",
    .minValue = 0,
    .maxValue = 2000,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::LAZY_THRESHOLD),
    .setter = evalSetter(&EvalConfigData::LAZY_THRESHOLD, parseInt)
  });

  //===========================================================================
  // PAWN EVAL
  //===========================================================================
  definitions_.push_back({
    .name = "USE_PAWN_EVAL",
    .uciName = "Use Pawn Eval",
    .description = "Enable pawn structure evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_PAWN_EVAL),
    .setter = evalSetter(&EvalConfigData::USE_PAWN_EVAL, parseBool)
  });

  definitions_.push_back({
    .name = "USE_PAWN_TT",
    .uciName = "Use Pawn Hash",
    .description = "Enable pawn hash table",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_PAWN_TT),
    .setter = evalSetter(&EvalConfigData::USE_PAWN_TT, parseBool)
  });

  definitions_.push_back({
    .name = "PAWN_TT_SIZE_MB",
    .uciName = "Pawn Hash Size",
    .description = "Pawn hash table size in MB",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "64",
    .minValue = 1,
    .maxValue = 1024,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::PAWN_TT_SIZE_MB),
    .setter = evalSetter(&EvalConfigData::PAWN_TT_SIZE_MB, parseInt)
  });

  //===========================================================================
  // PAWN STRUCTURE WEIGHTS
  //===========================================================================
  definitions_.push_back({
    .name = "ISOLATED_PAWN_MID_WEIGHT",
    .uciName = "Isolated Pawn Mid",
    .description = "Isolated pawn penalty in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-10",
    .minValue = -100,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::ISOLATED_PAWN_MID_WEIGHT),
    .setter = evalSetter(&EvalConfigData::ISOLATED_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "ISOLATED_PAWN_END_WEIGHT",
    .uciName = "Isolated Pawn End",
    .description = "Isolated pawn penalty in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-20",
    .minValue = -100,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::ISOLATED_PAWN_END_WEIGHT),
    .setter = evalSetter(&EvalConfigData::ISOLATED_PAWN_END_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "DOUBLED_PAWN_MID_WEIGHT",
    .uciName = "Doubled Pawn Mid",
    .description = "Doubled pawn penalty in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-10",
    .minValue = -100,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::DOUBLED_PAWN_MID_WEIGHT),
    .setter = evalSetter(&EvalConfigData::DOUBLED_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "DOUBLED_PAWN_END_WEIGHT",
    .uciName = "Doubled Pawn End",
    .description = "Doubled pawn penalty in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-30",
    .minValue = -100,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::DOUBLED_PAWN_END_WEIGHT),
    .setter = evalSetter(&EvalConfigData::DOUBLED_PAWN_END_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "PASSED_PAWN_MID_WEIGHT",
    .uciName = "Passed Pawn Mid",
    .description = "Passed pawn bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "20",
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::PASSED_PAWN_MID_WEIGHT),
    .setter = evalSetter(&EvalConfigData::PASSED_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "PASSED_PAWN_END_WEIGHT",
    .uciName = "Passed Pawn End",
    .description = "Passed pawn bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "40",
    .minValue = 0,
    .maxValue = 200,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::PASSED_PAWN_END_WEIGHT),
    .setter = evalSetter(&EvalConfigData::PASSED_PAWN_END_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "BLOCKED_PAWN_MID_WEIGHT",
    .uciName = "Blocked Pawn Mid",
    .description = "Blocked pawn penalty in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-2",
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::BLOCKED_PAWN_MID_WEIGHT),
    .setter = evalSetter(&EvalConfigData::BLOCKED_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "BLOCKED_PAWN_END_WEIGHT",
    .uciName = "Blocked Pawn End",
    .description = "Blocked pawn penalty in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-20",
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::BLOCKED_PAWN_END_WEIGHT),
    .setter = evalSetter(&EvalConfigData::BLOCKED_PAWN_END_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "PHALANX_PAWN_MID_WEIGHT",
    .uciName = "Phalanx Pawn Mid",
    .description = "Phalanx pawn bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "4",
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::PHALANX_PAWN_MID_WEIGHT),
    .setter = evalSetter(&EvalConfigData::PHALANX_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "PHALANX_PAWN_END_WEIGHT",
    .uciName = "Phalanx Pawn End",
    .description = "Phalanx pawn bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "4",
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::PHALANX_PAWN_END_WEIGHT),
    .setter = evalSetter(&EvalConfigData::PHALANX_PAWN_END_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "SUPPORTED_PAWN_MID_WEIGHT",
    .uciName = "Supported Pawn Mid",
    .description = "Supported pawn bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "10",
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::SUPPORTED_PAWN_MID_WEIGHT),
    .setter = evalSetter(&EvalConfigData::SUPPORTED_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "SUPPORTED_PAWN_END_WEIGHT",
    .uciName = "Supported Pawn End",
    .description = "Supported pawn bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "15",
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::SUPPORTED_PAWN_END_WEIGHT),
    .setter = evalSetter(&EvalConfigData::SUPPORTED_PAWN_END_WEIGHT, parseInt)
  });

  //===========================================================================
  // PIECE EVAL
  //===========================================================================
  definitions_.push_back({
    .name = "USE_PIECE_EVAL",
    .uciName = "Use Piece Eval",
    .description = "Enable piece-specific evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_PIECE_EVAL),
    .setter = evalSetter(&EvalConfigData::USE_PIECE_EVAL, parseBool)
  });

  //===========================================================================
  // BISHOP PAIR
  //===========================================================================
  definitions_.push_back({
    .name = "USE_BISHOP_PAIR_BONUS",
    .uciName = "Use Bishop Pair Bonus",
    .description = "Enable bishop pair bonus",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_BISHOP_PAIR_BONUS),
    .setter = evalSetter(&EvalConfigData::USE_BISHOP_PAIR_BONUS, parseBool)
  });

  definitions_.push_back({
    .name = "BISHOP_PAIR_MID_BONUS",
    .uciName = "Bishop Pair Mid Bonus",
    .description = "Bishop pair bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "20",
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::BISHOP_PAIR_MID_BONUS),
    .setter = evalSetter(&EvalConfigData::BISHOP_PAIR_MID_BONUS, parseInt)
  });

  definitions_.push_back({
    .name = "BISHOP_PAIR_END_BONUS",
    .uciName = "Bishop Pair End Bonus",
    .description = "Bishop pair bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "20",
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::BISHOP_PAIR_END_BONUS),
    .setter = evalSetter(&EvalConfigData::BISHOP_PAIR_END_BONUS, parseInt)
  });

  //===========================================================================
  // KNIGHT MOBILITY
  //===========================================================================
  definitions_.push_back({
    .name = "USE_KNIGHT_MOBILITY",
    .uciName = "Use Knight Mobility",
    .description = "Enable knight mobility evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_KNIGHT_MOBILITY),
    .setter = evalSetter(&EvalConfigData::USE_KNIGHT_MOBILITY, parseBool)
  });

  definitions_.push_back({
    .name = "KNIGHT_MOBILITY_MID_PER_MOVE",
    .uciName = "Knight Mobility Mid",
    .description = "Knight mobility bonus per move in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "3",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::KNIGHT_MOBILITY_MID_PER_MOVE),
    .setter = evalSetter(&EvalConfigData::KNIGHT_MOBILITY_MID_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_MOBILITY_END_PER_MOVE",
    .uciName = "Knight Mobility End",
    .description = "Knight mobility bonus per move in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "2",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::KNIGHT_MOBILITY_END_PER_MOVE),
    .setter = evalSetter(&EvalConfigData::KNIGHT_MOBILITY_END_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_LOW_MOBILITY_LEQ1_MID",
    .uciName = "Knight Low Mob LEQ1 Mid",
    .description = "Knight penalty for <=1 moves in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-6",
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::KNIGHT_LOW_MOBILITY_LEQ1_MID),
    .setter = evalSetter(&EvalConfigData::KNIGHT_LOW_MOBILITY_LEQ1_MID, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_LOW_MOBILITY_LEQ1_END",
    .uciName = "Knight Low Mob LEQ1 End",
    .description = "Knight penalty for <=1 moves in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-6",
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::KNIGHT_LOW_MOBILITY_LEQ1_END),
    .setter = evalSetter(&EvalConfigData::KNIGHT_LOW_MOBILITY_LEQ1_END, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_LOW_MOBILITY_LEQ2_MID",
    .uciName = "Knight Low Mob LEQ2 Mid",
    .description = "Knight penalty for <=2 moves in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-3",
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::KNIGHT_LOW_MOBILITY_LEQ2_MID),
    .setter = evalSetter(&EvalConfigData::KNIGHT_LOW_MOBILITY_LEQ2_MID, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_LOW_MOBILITY_LEQ2_END",
    .uciName = "Knight Low Mob LEQ2 End",
    .description = "Knight penalty for <=2 moves in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-3",
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::KNIGHT_LOW_MOBILITY_LEQ2_END),
    .setter = evalSetter(&EvalConfigData::KNIGHT_LOW_MOBILITY_LEQ2_END, parseInt)
  });

  //===========================================================================
  // BISHOP MOBILITY
  //===========================================================================
  definitions_.push_back({
    .name = "USE_BISHOP_MOBILITY",
    .uciName = "Use Bishop Mobility",
    .description = "Enable bishop mobility evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_BISHOP_MOBILITY),
    .setter = evalSetter(&EvalConfigData::USE_BISHOP_MOBILITY, parseBool)
  });

  definitions_.push_back({
    .name = "BISHOP_MOBILITY_MID_PER_MOVE",
    .uciName = "Bishop Mobility Mid",
    .description = "Bishop mobility bonus per move in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "2",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::BISHOP_MOBILITY_MID_PER_MOVE),
    .setter = evalSetter(&EvalConfigData::BISHOP_MOBILITY_MID_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "BISHOP_MOBILITY_END_PER_MOVE",
    .uciName = "Bishop Mobility End",
    .description = "Bishop mobility bonus per move in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "3",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::BISHOP_MOBILITY_END_PER_MOVE),
    .setter = evalSetter(&EvalConfigData::BISHOP_MOBILITY_END_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "BISHOP_LOW_MOBILITY_LEQ3_MID",
    .uciName = "Bishop Low Mob LEQ3 Mid",
    .description = "Bishop penalty for <=3 moves in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-4",
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::BISHOP_LOW_MOBILITY_LEQ3_MID),
    .setter = evalSetter(&EvalConfigData::BISHOP_LOW_MOBILITY_LEQ3_MID, parseInt)
  });

  definitions_.push_back({
    .name = "BISHOP_LOW_MOBILITY_LEQ3_END",
    .uciName = "Bishop Low Mob LEQ3 End",
    .description = "Bishop penalty for <=3 moves in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-2",
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::BISHOP_LOW_MOBILITY_LEQ3_END),
    .setter = evalSetter(&EvalConfigData::BISHOP_LOW_MOBILITY_LEQ3_END, parseInt)
  });

  //===========================================================================
  // ROOK MOBILITY AND FILES
  //===========================================================================
  definitions_.push_back({
    .name = "USE_ROOK_MOBILITY",
    .uciName = "Use Rook Mobility",
    .description = "Enable rook mobility evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_ROOK_MOBILITY),
    .setter = evalSetter(&EvalConfigData::USE_ROOK_MOBILITY, parseBool)
  });

  definitions_.push_back({
    .name = "ROOK_MOBILITY_MID_PER_MOVE",
    .uciName = "Rook Mobility Mid",
    .description = "Rook mobility bonus per move in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "2",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::ROOK_MOBILITY_MID_PER_MOVE),
    .setter = evalSetter(&EvalConfigData::ROOK_MOBILITY_MID_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_MOBILITY_END_PER_MOVE",
    .uciName = "Rook Mobility End",
    .description = "Rook mobility bonus per move in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "2",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::ROOK_MOBILITY_END_PER_MOVE),
    .setter = evalSetter(&EvalConfigData::ROOK_MOBILITY_END_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_LOW_MOBILITY_LEQ3_MID",
    .uciName = "Rook Low Mob LEQ3 Mid",
    .description = "Rook penalty for <=3 moves in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-3",
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::ROOK_LOW_MOBILITY_LEQ3_MID),
    .setter = evalSetter(&EvalConfigData::ROOK_LOW_MOBILITY_LEQ3_MID, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_LOW_MOBILITY_LEQ3_END",
    .uciName = "Rook Low Mob LEQ3 End",
    .description = "Rook penalty for <=3 moves in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "-3",
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::ROOK_LOW_MOBILITY_LEQ3_END),
    .setter = evalSetter(&EvalConfigData::ROOK_LOW_MOBILITY_LEQ3_END, parseInt)
  });

  definitions_.push_back({
    .name = "USE_ROOK_OPEN_FILE_BONUS",
    .uciName = "Use Rook Open File Bonus",
    .description = "Enable rook on open file bonus",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_ROOK_OPEN_FILE_BONUS),
    .setter = evalSetter(&EvalConfigData::USE_ROOK_OPEN_FILE_BONUS, parseBool)
  });

  definitions_.push_back({
    .name = "ROOK_OPEN_FILE_MID_BONUS",
    .uciName = "Rook Open File Mid",
    .description = "Rook on open file bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "10",
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::ROOK_OPEN_FILE_MID_BONUS),
    .setter = evalSetter(&EvalConfigData::ROOK_OPEN_FILE_MID_BONUS, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_OPEN_FILE_END_BONUS",
    .uciName = "Rook Open File End",
    .description = "Rook on open file bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "8",
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::ROOK_OPEN_FILE_END_BONUS),
    .setter = evalSetter(&EvalConfigData::ROOK_OPEN_FILE_END_BONUS, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_SEMIOPEN_FILE_MID_BONUS",
    .uciName = "Rook Semiopen File Mid",
    .description = "Rook on semi-open file bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "5",
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::ROOK_SEMIOPEN_FILE_MID_BONUS),
    .setter = evalSetter(&EvalConfigData::ROOK_SEMIOPEN_FILE_MID_BONUS, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_SEMIOPEN_FILE_END_BONUS",
    .uciName = "Rook Semiopen File End",
    .description = "Rook on semi-open file bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "4",
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::ROOK_SEMIOPEN_FILE_END_BONUS),
    .setter = evalSetter(&EvalConfigData::ROOK_SEMIOPEN_FILE_END_BONUS, parseInt)
  });

  //===========================================================================
  // QUEEN
  //===========================================================================
  definitions_.push_back({
    .name = "USE_QUEEN_MOBILITY",
    .uciName = "Use Queen Mobility",
    .description = "Enable queen mobility evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_QUEEN_MOBILITY),
    .setter = evalSetter(&EvalConfigData::USE_QUEEN_MOBILITY, parseBool)
  });

  definitions_.push_back({
    .name = "QUEEN_MOBILITY_MID_PER_MOVE",
    .uciName = "Queen Mobility Mid",
    .description = "Queen mobility bonus per move in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "1",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::QUEEN_MOBILITY_MID_PER_MOVE),
    .setter = evalSetter(&EvalConfigData::QUEEN_MOBILITY_MID_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "QUEEN_MOBILITY_END_PER_MOVE",
    .uciName = "Queen Mobility End",
    .description = "Queen mobility bonus per move in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "1",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::QUEEN_MOBILITY_END_PER_MOVE),
    .setter = evalSetter(&EvalConfigData::QUEEN_MOBILITY_END_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "USE_QUEEN_TROPISM",
    .uciName = "Use Queen Tropism",
    .description = "Enable queen tropism (king proximity bonus)",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_QUEEN_TROPISM),
    .setter = evalSetter(&EvalConfigData::USE_QUEEN_TROPISM, parseBool)
  });

  definitions_.push_back({
    .name = "QUEEN_TROPISM_MID_PER_STEP",
    .uciName = "Queen Tropism Mid",
    .description = "Queen tropism bonus per step closer in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "0",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::QUEEN_TROPISM_MID_PER_STEP),
    .setter = evalSetter(&EvalConfigData::QUEEN_TROPISM_MID_PER_STEP, parseInt)
  });

  definitions_.push_back({
    .name = "QUEEN_TROPISM_END_PER_STEP",
    .uciName = "Queen Tropism End",
    .description = "Queen tropism bonus per step closer in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "1",
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::QUEEN_TROPISM_END_PER_STEP),
    .setter = evalSetter(&EvalConfigData::QUEEN_TROPISM_END_PER_STEP, parseInt)
  });

  //===========================================================================
  // KING
  //===========================================================================
  definitions_.push_back({
    .name = "USE_KING_EVAL",
    .uciName = "Use King Eval",
    .description = "Enable king safety evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_KING_EVAL),
    .setter = evalSetter(&EvalConfigData::USE_KING_EVAL, parseBool)
  });

  definitions_.push_back({
    .name = "USE_KING_SAFETY_SHIELD",
    .uciName = "Use King Safety Shield",
    .description = "Enable king pawn shield bonus",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_KING_SAFETY_SHIELD),
    .setter = evalSetter(&EvalConfigData::USE_KING_SAFETY_SHIELD, parseBool)
  });

  definitions_.push_back({
    .name = "KING_SHIELD_MID_PER_PAWN",
    .uciName = "King Shield Mid",
    .description = "King pawn shield bonus per pawn in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "5",
    .minValue = 0,
    .maxValue = 30,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::KING_SHIELD_MID_PER_PAWN),
    .setter = evalSetter(&EvalConfigData::KING_SHIELD_MID_PER_PAWN, parseInt)
  });

  definitions_.push_back({
    .name = "KING_SHIELD_END_PER_PAWN",
    .uciName = "King Shield End",
    .description = "King pawn shield bonus per pawn in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = "0",
    .minValue = 0,
    .maxValue = 30,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::KING_SHIELD_END_PER_PAWN),
    .setter = evalSetter(&EvalConfigData::KING_SHIELD_END_PER_PAWN, parseInt)
  });

  //===========================================================================
  // GAME PHASE
  //===========================================================================
  definitions_.push_back({
    .name = "USE_GAMEPHASE_VALUE",
    .uciName = "Use Game Phase Value",
    .description = "Enable game phase-based evaluation blending",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter(&EvalConfigData::USE_GAMEPHASE_VALUE),
    .setter = evalSetter(&EvalConfigData::USE_GAMEPHASE_VALUE, parseBool)
  });

  // clang-format on
}
