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

  // In production builds, CONFIG_CONST members become static constexpr and do not
  // contribute to sizeof(). The assertions below are only valid in development builds
  // where all members are non-static instance members.
#ifndef FRANKYCPP_PRODUCTION
#ifdef _MSC_VER
// Windows MSVC builds
#ifdef _DEBUG
  static_assert(sizeof(SearchConfigData) == 600,
                "SearchConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
  static_assert(sizeof(EvalConfigData) == 256,
                "EvalConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
#else
  static_assert(sizeof(SearchConfigData) == 568,
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
  static_assert(sizeof(SearchConfigData) == 568,
                "SearchConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
  static_assert(sizeof(EvalConfigData) == 248,
                "EvalConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
#else
  // Debug build
  static_assert(sizeof(SearchConfigData) == 568,
                "SearchConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
  static_assert(sizeof(EvalConfigData) == 248,
                "EvalConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
#endif
#endif
#endif // FRANKYCPP_PRODUCTION

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

  // searchGetter: accepts a getter lambda (const SearchConfigData&) -> value.
  // Lambda-based (not pointer-to-member) so it works for both instance members
  // and static constexpr members in production builds.
  auto searchGetter = [](auto getterFn) {
    return [getterFn](const SearchConfigData& s, const EvalConfigData&) {
      return configToString(getterFn(s));
    };
  };

  // searchSetter is no longer used — replaced by SEARCH_CONFIG_SETTER macro at each call site.

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
    .getter = searchGetter([](const auto& s){ return s.CONFIG_SOURCE; }),
    .setter = SEARCH_CONFIG_SETTER(CONFIG_SOURCE, parseString)
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
    .getter = searchGetter([](const auto& s){ return s.MOVE_OVERHEAD_MS; }),
    .setter = SEARCH_CONFIG_SETTER(MOVE_OVERHEAD_MS, parseInt)
  });

  definitions_.push_back({
    .name = "USE_BOOK",
    .uciName = "OwnBook",
    .description = "Use internal opening book",
    .valueType = Bool,
    .domain = General,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_BOOK; }),
    .setter = SEARCH_CONFIG_SETTER(USE_BOOK, parseBool)
  });

  definitions_.push_back({
    .name = "BOOK_PATH",
    .uciName = "Book Path",
    .description = "Path to opening book file",
    .valueType = String,
    .domain = General,
    .defaultValue = "./books/book.txt",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.BOOK_PATH; }),
    .setter = SEARCH_CONFIG_SETTER(BOOK_PATH, parseString)
  });

  definitions_.push_back({
    .name = "BOOK_TYPE",
    .uciName = "Book Format",
    .description = "Opening book format (SIMPLE, PGN, etc.)",
    .valueType = String,
    .domain = General,
    .defaultValue = "SIMPLE",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.BOOK_TYPE; }),
    .setter = SEARCH_CONFIG_SETTER(BOOK_TYPE, parseString)
  });

  definitions_.push_back({
    .name = "USE_PONDER",
    .uciName = "Ponder",
    .description = "Enable pondering (thinking on opponent's time)",
    .valueType = Bool,
    .domain = General,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_PONDER; }),
    .setter = SEARCH_CONFIG_SETTER(USE_PONDER, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.USE_ALPHABETA; }),
    .setter = SEARCH_CONFIG_SETTER(USE_ALPHABETA, parseBool)
  });

  definitions_.push_back({
    .name = "USE_PVS",
    .uciName = "Use Pvs",
    .description = "Enable Principal Variation Search",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_PVS; }),
    .setter = SEARCH_CONFIG_SETTER(USE_PVS, parseBool)
  });

  definitions_.push_back({
    .name = "USE_ASP",
    .uciName = "Use Aspiration",
    .description = "Enable aspiration windows",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_ASP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_ASP, parseBool)
  });

  definitions_.push_back({
    .name = "USE_QUIESCENCE",
    .uciName = "Use Quiescence",
    .description = "Enable quiescence search",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_QUIESCENCE; }),
    .setter = SEARCH_CONFIG_SETTER(USE_QUIESCENCE, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.USE_TT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_TT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_TT_VALUE",
    .uciName = "Use Hash Value",
    .description = "Use TT values for cutoffs",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_TT_VALUE; }),
    .setter = SEARCH_CONFIG_SETTER(USE_TT_VALUE, parseBool)
  });

  definitions_.push_back({
    .name = "USE_EVAL_TT",
    .uciName = "Use Eval TT",
    .description = "Use TT for evaluation cache",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_EVAL_TT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_EVAL_TT, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.TT_SIZE_MB; }),
    .setter = SEARCH_CONFIG_SETTER(TT_SIZE_MB, parseInt),
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
    .getter = searchGetter([](const auto& s){ return s.USE_QS_TT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_QS_TT, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.TB_PATH; }),
    .setter = SEARCH_CONFIG_SETTER(TB_PATH, parseString)
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
    .getter = searchGetter([](const auto& s){ return s.USE_TB_PROBE_ROOT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_TB_PROBE_ROOT, parseBool)
  });

  definitions_.push_back({
    .name = "TB_ROOT_IMMEDIATE",
    .uciName = "Syzygy Root Immediate",
    .description = "Return TB move immediately without searching (false = search for PV)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "false",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.TB_ROOT_IMMEDIATE; }),
    .setter = SEARCH_CONFIG_SETTER(TB_ROOT_IMMEDIATE, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.USE_TB_PROBE_SEARCH; }),
    .setter = SEARCH_CONFIG_SETTER(USE_TB_PROBE_SEARCH, parseBool)
  });

  definitions_.push_back({
    .name = "USE_TB_PROBE_PV",
    .uciName = "",  // Not exposed via UCI - internal tuning option
    .description = "Probe tablebases on PV nodes (false = only non-PV nodes for cutoffs)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = false, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_TB_PROBE_PV; }),
    .setter = SEARCH_CONFIG_SETTER(USE_TB_PROBE_PV, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.TB_PROBE_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(TB_PROBE_DEPTH, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.TB_PROBE_LIMIT; }),
    .setter = SEARCH_CONFIG_SETTER(TB_PROBE_LIMIT, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.TB_RULE50_THRESHOLD; }),
    .setter = SEARCH_CONFIG_SETTER(TB_RULE50_THRESHOLD, parseInt)
  });

  definitions_.push_back({
    .name = "TB_CACHE_PREWARM",
    .uciName = "",
    .description = "Pre-warm OS file cache at startup for faster first probes",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = false, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.TB_CACHE_PREWARM; }),
    .setter = SEARCH_CONFIG_SETTER(TB_CACHE_PREWARM, parseBool)
  });

  definitions_.push_back({
    .name = "TB_CACHE_PREWARM_PIECES",
    .uciName = "",
    .description = "Max pieces to pre-warm (3-6, higher = more startup time)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "6",
    .minValue = 3,
    .maxValue = 6,
    .exposure = {.uci = false, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.TB_CACHE_PREWARM_PIECES; }),
    .setter = SEARCH_CONFIG_SETTER(TB_CACHE_PREWARM_PIECES, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.USE_TT_PV_MOVE_SORT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_TT_PV_MOVE_SORT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_KILLER_MOVES",
    .uciName = "Use Killer Moves",
    .description = "Enable killer move heuristic",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_KILLER_MOVES; }),
    .setter = SEARCH_CONFIG_SETTER(USE_KILLER_MOVES, parseBool)
  });

  definitions_.push_back({
    .name = "USE_HISTORY_COUNTER",
    .uciName = "Use History Counter",
    .description = "Enable counter-move history",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_HISTORY_COUNTER; }),
    .setter = SEARCH_CONFIG_SETTER(USE_HISTORY_COUNTER, parseBool)
  });

  definitions_.push_back({
    .name = "USE_HISTORY_MOVES",
    .uciName = "Use History Moves",
    .description = "Enable history heuristic for move ordering",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_HISTORY_MOVES; }),
    .setter = SEARCH_CONFIG_SETTER(USE_HISTORY_MOVES, parseBool)
  });

  definitions_.push_back({
    .name = "USE_IID",
    .uciName = "Use Internal Iterative Deepening",
    .description = "Enable Internal Iterative Deepening (legacy - IIR is more effective)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "false",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_IID; }),
    .setter = SEARCH_CONFIG_SETTER(USE_IID, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.IID_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(IID_DEPTH, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.IID_REDUCTION; }),
    .setter = SEARCH_CONFIG_SETTER(IID_REDUCTION, parseInt)
  });

  // Internal Iterative Reduction (IIR) - modern alternative to IID
  definitions_.push_back({
    .name = "USE_IIR",
    .uciName = "Use Internal Iterative Reduction",
    .description = "Enable IIR - 36% node reduction vs IID in testing",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_IIR; }),
    .setter = SEARCH_CONFIG_SETTER(USE_IIR, parseBool)
  });

  definitions_.push_back({
    .name = "IIR_DEPTH",
    .uciName = "IIR Min Depth",
    .description = "Minimum depth to apply IIR reduction",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "4",
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.IIR_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(IIR_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "IIR_REDUCTION",
    .uciName = "IIR Depth Reduction",
    .description = "How much to reduce depth when IIR triggers",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "2",
    .minValue = 1,
    .maxValue = 5,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.IIR_REDUCTION; }),
    .setter = SEARCH_CONFIG_SETTER(IIR_REDUCTION, parseInt)
  });

  definitions_.push_back({
    .name = "IIR_ALL_NODES",
    .uciName = "IIR All Nodes",
    .description = "Apply IIR to all node types (true) or PV only (false)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.IIR_ALL_NODES; }),
    .setter = SEARCH_CONFIG_SETTER(IIR_ALL_NODES, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.USE_MDP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_MDP, parseBool)
  });

  definitions_.push_back({
    .name = "USE_QS_STANDPAT_CUT",
    .uciName = "Use Quiescence Standpat",
    .description = "Enable stand-pat cutoff in quiescence",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_QS_STANDPAT_CUT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_QS_STANDPAT_CUT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_QS_SEE",
    .uciName = "Use Quiescence SEE",
    .description = "Enable SEE pruning in quiescence",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_QS_SEE; }),
    .setter = SEARCH_CONFIG_SETTER(USE_QS_SEE, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.USE_RAZORING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_RAZORING, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.RAZOR_MARGIN; }),
    .setter = SEARCH_CONFIG_SETTER(RAZOR_MARGIN, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.USE_RFP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_RFP, parseBool)
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
    .setter = SEARCH_CONFIG_ARRAY_SETTER(RFP_MARGIN)
  });

  definitions_.push_back({
    .name = "USE_RFP_IMPROVING",
    .uciName = "RFP Improving",
    .description = "Use improving flag to reduce RFP margin when not improving",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_RFP_IMPROVING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_RFP_IMPROVING, parseBool)
  });

  definitions_.push_back({
    .name = "RFP_IMPROVING_MARGIN",
    .uciName = "RFP Improving Margin",
    .description = "RFP margin reduction in centipawns when position is not improving",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "40",
    .minValue = 0,
    .maxValue = 300,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.RFP_IMPROVING_MARGIN; }),
    .setter = SEARCH_CONFIG_SETTER(RFP_IMPROVING_MARGIN, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.USE_NMP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_NMP, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.NMP_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_DEPTH, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.NMP_REDUCTION; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_REDUCTION, parseInt)
  });

  definitions_.push_back({
    .name = "USE_NMP_VERIFY",
    .uciName = "Use Null Move Verification",
    .description = "Enable null move verification search",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_NMP_VERIFY; }),
    .setter = SEARCH_CONFIG_SETTER(USE_NMP_VERIFY, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.NMP_VERIFY_MIN_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_VERIFY_MIN_DEPTH, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.NMP_VERIFY_MARGIN; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_VERIFY_MARGIN, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.NMP_NEAR_MATE_MARGIN; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_NEAR_MATE_MARGIN, parseInt)
  });

  definitions_.push_back({
    .name = "USE_NMP_ZUG_GUARD",
    .uciName = "Use Null Move Zugzwang Guard",
    .description = "Enable zugzwang guard for null move",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_NMP_ZUG_GUARD; }),
    .setter = SEARCH_CONFIG_SETTER(USE_NMP_ZUG_GUARD, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.NMP_ZUG_NONPAWN_THRESHOLD; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_ZUG_NONPAWN_THRESHOLD, parseInt)
  });

  definitions_.push_back({
    .name = "USE_NMP_IMPROVING",
    .uciName = "Null Move Improving",
    .description = "Use improving flag to increase NMP reduction when not improving",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_NMP_IMPROVING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_NMP_IMPROVING, parseBool)
  });

  definitions_.push_back({
    .name = "NMP_IMPROVING_REDUCTION",
    .uciName = "Null Move Improving Reduction",
    .description = "Extra NMP reduction depth when position is not improving",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "1",
    .minValue = 0,
    .maxValue = 3,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.NMP_IMPROVING_REDUCTION; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_IMPROVING_REDUCTION, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.USE_FP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_FP, parseBool)
  });

  definitions_.push_back({
    .name = "USE_QFP",
    .uciName = "Use Quiescence Futility Pruning",
    .description = "Enable futility pruning in quiescence",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_QFP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_QFP, parseBool)
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
    .setter = SEARCH_CONFIG_ARRAY_SETTER(FP_MARGIN)
  });

  definitions_.push_back({
    .name = "USE_FP_IMPROVING",
    .uciName = "Futility Pruning Improving",
    .description = "Use improving flag to reduce FP margin when not improving",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_FP_IMPROVING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_FP_IMPROVING, parseBool)
  });

  definitions_.push_back({
    .name = "FP_IMPROVING_MARGIN",
    .uciName = "Futility Pruning Improving Margin",
    .description = "FP margin reduction in centipawns when position is not improving",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "80",
    .minValue = 0,
    .maxValue = 300,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.FP_IMPROVING_MARGIN; }),
    .setter = SEARCH_CONFIG_SETTER(FP_IMPROVING_MARGIN, parseInt)
  });

  //===========================================================================
  // IMPROVING FLAG
  //===========================================================================
  definitions_.push_back({
    .name = "USE_IMPROVING",
    .uciName = "Use Improving Flag",
    .description = "Track if eval is improving vs 2 plies ago for pruning modulation",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_IMPROVING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_IMPROVING, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.USE_LMR; }),
    .setter = SEARCH_CONFIG_SETTER(USE_LMR, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.LMR_MIN_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_MIN_DEPTH, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.LMR_MIN_MOVES; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_MIN_MOVES, parseInt)
  });

  definitions_.push_back({
    .name = "LMR_USE_LOG_FORMULA",
    .uciName = "LMR Use Log Formula",
    .description = "Use logarithmic formula instead of linear for LMR",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.LMR_USE_LOG_FORMULA; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_USE_LOG_FORMULA, parseBool)
  });

  definitions_.push_back({
    .name = "LMR_LOG_BASE_DIV",
    .uciName = "LMR Log Base Divisor Pct",
    .description = "Divisor for log formula: log(d)*log(m)/divisor",
    .valueType = Double,
    .domain = Search,
    .defaultValue = "1.25",
    .minValue = 50,
    .maxValue = 500,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = [](const SearchConfigData& s, const EvalConfigData&) {
      return configToString(s.LMR_LOG_BASE_DIV);
    },
    .setter = SEARCH_CONFIG_SETTER(LMR_LOG_BASE_DIV, parseDouble)
  });

  definitions_.push_back({
    .name = "USE_LMR_IMPROVING",
    .uciName = "Use LMR Improving",
    .description = "Use improving flag to modulate LMR (extra reduction when not improving)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_LMR_IMPROVING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_LMR_IMPROVING, parseBool)
  });

  definitions_.push_back({
    .name = "LMR_IMPROVING_REDUCTION",
    .uciName = "LMR Improving Reduction",
    .description = "Extra LMR reduction depth when position is not improving",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "1",
    .minValue = 0,
    .maxValue = 4,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.LMR_IMPROVING_REDUCTION; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_IMPROVING_REDUCTION, parseInt)
  });

  definitions_.push_back({
    .name = "USE_LMR_HISTORY",
    .uciName = "Use LMR History",
    .description = "Use history score to modulate LMR (less reduction for moves with good history)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_LMR_HISTORY; }),
    .setter = SEARCH_CONFIG_SETTER(USE_LMR_HISTORY, parseBool)
  });

  definitions_.push_back({
    .name = "LMR_HISTORY_DIVISOR",
    .uciName = "LMR History Divisor",
    .description = "Divisor for history score to reduction conversion (higher = less effect)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "8192",
    .minValue = 1024,
    .maxValue = 32768,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.LMR_HISTORY_DIVISOR; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_HISTORY_DIVISOR, parseInt)
  });

  definitions_.push_back({
    .name = "USE_LMR_CUTNODE",
    .uciName = "Use LMR Cut Node",
    .description = "Extra reduction on expected cut nodes (nodes expected to fail high)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_LMR_CUTNODE; }),
    .setter = SEARCH_CONFIG_SETTER(USE_LMR_CUTNODE, parseBool)
  });

  definitions_.push_back({
    .name = "LMR_CUTNODE_REDUCTION",
    .uciName = "LMR Cut Node Reduction",
    .description = "Extra LMR reduction depth on cut nodes",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "2",
    .minValue = 0,
    .maxValue = 4,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.LMR_CUTNODE_REDUCTION; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_CUTNODE_REDUCTION, parseInt)
  });

  definitions_.push_back({
    .name = "USE_LMP",
    .uciName = "Use Late Move Pruning",
    .description = "Enable Late Move Pruning",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_LMP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_LMP, parseBool)
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
    .setter = SEARCH_CONFIG_ARRAY_SETTER(LMP_MOVES)
  });

  definitions_.push_back({
    .name = "USE_LMP_IMPROVING",
    .uciName = "Use LMP Improving",
    .description = "Use improving flag to modulate LMP threshold (more moves when improving)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_LMP_IMPROVING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_LMP_IMPROVING, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.USE_EXTENSIONS; }),
    .setter = SEARCH_CONFIG_SETTER(USE_EXTENSIONS, parseBool)
  });

  definitions_.push_back({
    .name = "USE_CHECK_EXT",
    .uciName = "Use Check Extension",
    .description = "Enable check extension",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_CHECK_EXT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_CHECK_EXT, parseBool)
  });

  definitions_.push_back({
    .name = "CHECK_EXT_MIN_DEPTH",
    .uciName = "Check Ext Min Depth",
    .description = "Minimum depth to apply check extension",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "2",
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.CHECK_EXT_MIN_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(CHECK_EXT_MIN_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "CHECK_EXT_EARLY_LIMIT",
    .uciName = "Check Ext Early Limit",
    .description = "Only extend checks in first N moves per node (99 = no limit)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "99",
    .minValue = 0,
    .maxValue = 99,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.CHECK_EXT_EARLY_LIMIT; }),
    .setter = SEARCH_CONFIG_SETTER(CHECK_EXT_EARLY_LIMIT, parseInt)
  });

  definitions_.push_back({
    .name = "USE_CHECK_EXT_SEE",
    .uciName = "Check Ext SEE",
    .description = "Only extend checks with SEE >= 0 (non-losing)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_CHECK_EXT_SEE; }),
    .setter = SEARCH_CONFIG_SETTER(USE_CHECK_EXT_SEE, parseBool)
  });

  definitions_.push_back({
    .name = "USE_THREAT_EXT",
    .uciName = "Use Threat Extension",
    .description = "Enable threat extension",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_THREAT_EXT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_THREAT_EXT, parseBool)
  });

  definitions_.push_back({
    .name = "THREAT_EXT_MATE_DEPTH",
    .uciName = "",  // Not exposed via UCI
    .description = "Mate depth threshold for threat extension (mate-in-N detection)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = "4",
    .minValue = 2,
    .maxValue = 10,
    .exposure = {.uci = false, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.THREAT_EXT_MATE_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(THREAT_EXT_MATE_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "USE_EXT_ADD_DEPTH",
    .uciName = "Use Extension Add",
    .description = "Add depth for extensions",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_EXT_ADD_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(USE_EXT_ADD_DEPTH, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.USE_SINGULAR_EXT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_SINGULAR_EXT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_SINGULAR_TT_BOUND",
    .uciName = "Singular TT Bound",
    .description = "Require BETA/EXACT TT bound for singular (too restrictive in practice)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = "false",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_SINGULAR_TT_BOUND; }),
    .setter = SEARCH_CONFIG_SETTER(USE_SINGULAR_TT_BOUND, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.SINGULAR_MARGIN; }),
    .setter = SEARCH_CONFIG_SETTER(SINGULAR_MARGIN, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.SINGULAR_MIN_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(SINGULAR_MIN_DEPTH, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.SINGULAR_REDUCTION; }),
    .setter = SEARCH_CONFIG_SETTER(SINGULAR_REDUCTION, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_OPENING; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_OPENING, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_MIDGAME; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_MIDGAME, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_ENDGAME; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_ENDGAME, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_LOW_MAT; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_LOW_MAT, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_QUEENLESS; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_QUEENLESS, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.NPP_HEAVY_THRESHOLD; }),
    .setter = SEARCH_CONFIG_SETTER(NPP_HEAVY_THRESHOLD, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.NPP_LIGHT_THRESHOLD; }),
    .setter = SEARCH_CONFIG_SETTER(NPP_LIGHT_THRESHOLD, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.REPETITION_HMC_HIGH; }),
    .setter = SEARCH_CONFIG_SETTER(REPETITION_HMC_HIGH, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.REPETITION_RISK_PENALTY; }),
    .setter = SEARCH_CONFIG_SETTER(REPETITION_RISK_PENALTY, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_MIN_CLAMP; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_MIN_CLAMP, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_MAX_CLAMP; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_MAX_CLAMP, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.USE_BESTMOVE_INSTABILITY; }),
    .setter = SEARCH_CONFIG_SETTER(USE_BESTMOVE_INSTABILITY, parseBool)
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
    .getter = searchGetter([](const auto& s){ return s.INSTABILITY_MIN_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(INSTABILITY_MIN_DEPTH, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.INSTABILITY_STABLE_COUNT; }),
    .setter = SEARCH_CONFIG_SETTER(INSTABILITY_STABLE_COUNT, parseInt)
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
    .getter = searchGetter([](const auto& s){ return s.INSTABILITY_CHANGE_THRESHOLD; }),
    .setter = SEARCH_CONFIG_SETTER(INSTABILITY_CHANGE_THRESHOLD, parseInt)
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
    .setter = SEARCH_CONFIG_SETTER(INSTABILITY_STABLE_FACTOR, parseDouble)
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
    .setter = SEARCH_CONFIG_SETTER(INSTABILITY_EXTEND_FACTOR, parseDouble)
  });

  definitions_.push_back({
    .name = "MAX_EXTRA_TIME_FACTOR",
    .uciName = "Max Extra Time Factor Pct",
    .description = "Maximum extra time as multiple of base time (2.0 = max 3x total budget)",
    .valueType = Double,
    .domain = Search,
    .defaultValue = "2.0",
    .minValue = 50,
    .maxValue = 500,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = [](const SearchConfigData& s, const EvalConfigData&) {
      return configToString(s.MAX_EXTRA_TIME_FACTOR);
    },
    .setter = SEARCH_CONFIG_SETTER(MAX_EXTRA_TIME_FACTOR, parseDouble)
  });

  // clang-format on
}

//=============================================================================
// Eval Config Definitions
//=============================================================================

void ConfigRegistry::initializeEvalDefinitions() {
  using enum ConfigValueType;
  using enum ConfigDomain;

  // evalGetter: same pattern as searchGetter but for EvalConfigData.
  auto evalGetter = [](auto getterFn) {
    return [getterFn](const SearchConfigData&, const EvalConfigData& e) {
      return configToString(getterFn(e));
    };
  };

  // evalSetter is no longer used — replaced by EVAL_CONFIG_SETTER macro at each call site.

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
    .getter = evalGetter([](const auto& e){ return e.EVAL_CONFIG_SOURCE; }),
    .setter = EVAL_CONFIG_SETTER(EVAL_CONFIG_SOURCE, parseString)
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
    .getter = evalGetter([](const auto& e){ return e.USE_MATERIAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_MATERIAL, parseBool)
  });

  definitions_.push_back({
    .name = "USE_POSITIONAL",
    .uciName = "Use Positional",
    .description = "Enable positional evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_POSITIONAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_POSITIONAL, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.USE_TEMPO; }),
    .setter = EVAL_CONFIG_SETTER(USE_TEMPO, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.TEMPO; }),
    .setter = EVAL_CONFIG_SETTER(TEMPO, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.USE_LAZY_EVAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_LAZY_EVAL, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.LAZY_THRESHOLD; }),
    .setter = EVAL_CONFIG_SETTER(LAZY_THRESHOLD, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.USE_PAWN_EVAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_PAWN_EVAL, parseBool)
  });

  definitions_.push_back({
    .name = "USE_PAWN_TT",
    .uciName = "Use Pawn Hash",
    .description = "Enable pawn hash table",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_PAWN_TT; }),
    .setter = EVAL_CONFIG_SETTER(USE_PAWN_TT, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.PAWN_TT_SIZE_MB; }),
    .setter = EVAL_CONFIG_SETTER(PAWN_TT_SIZE_MB, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.ISOLATED_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(ISOLATED_PAWN_MID_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.ISOLATED_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(ISOLATED_PAWN_END_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.DOUBLED_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(DOUBLED_PAWN_MID_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.DOUBLED_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(DOUBLED_PAWN_END_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.PASSED_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(PASSED_PAWN_MID_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.PASSED_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(PASSED_PAWN_END_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.BLOCKED_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(BLOCKED_PAWN_MID_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.BLOCKED_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(BLOCKED_PAWN_END_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.PHALANX_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(PHALANX_PAWN_MID_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.PHALANX_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(PHALANX_PAWN_END_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.SUPPORTED_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(SUPPORTED_PAWN_MID_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.SUPPORTED_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(SUPPORTED_PAWN_END_WEIGHT, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.USE_PIECE_EVAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_PIECE_EVAL, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.USE_BISHOP_PAIR_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(USE_BISHOP_PAIR_BONUS, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.BISHOP_PAIR_MID_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(BISHOP_PAIR_MID_BONUS, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.BISHOP_PAIR_END_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(BISHOP_PAIR_END_BONUS, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.USE_KNIGHT_MOBILITY; }),
    .setter = EVAL_CONFIG_SETTER(USE_KNIGHT_MOBILITY, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_MOBILITY_MID_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_MOBILITY_MID_PER_MOVE, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_MOBILITY_END_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_MOBILITY_END_PER_MOVE, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_LOW_MOBILITY_LEQ1_MID; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_LOW_MOBILITY_LEQ1_MID, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_LOW_MOBILITY_LEQ1_END; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_LOW_MOBILITY_LEQ1_END, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_LOW_MOBILITY_LEQ2_MID; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_LOW_MOBILITY_LEQ2_MID, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_LOW_MOBILITY_LEQ2_END; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_LOW_MOBILITY_LEQ2_END, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.USE_BISHOP_MOBILITY; }),
    .setter = EVAL_CONFIG_SETTER(USE_BISHOP_MOBILITY, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.BISHOP_MOBILITY_MID_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(BISHOP_MOBILITY_MID_PER_MOVE, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.BISHOP_MOBILITY_END_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(BISHOP_MOBILITY_END_PER_MOVE, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.BISHOP_LOW_MOBILITY_LEQ3_MID; }),
    .setter = EVAL_CONFIG_SETTER(BISHOP_LOW_MOBILITY_LEQ3_MID, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.BISHOP_LOW_MOBILITY_LEQ3_END; }),
    .setter = EVAL_CONFIG_SETTER(BISHOP_LOW_MOBILITY_LEQ3_END, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.USE_ROOK_MOBILITY; }),
    .setter = EVAL_CONFIG_SETTER(USE_ROOK_MOBILITY, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.ROOK_MOBILITY_MID_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_MOBILITY_MID_PER_MOVE, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.ROOK_MOBILITY_END_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_MOBILITY_END_PER_MOVE, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.ROOK_LOW_MOBILITY_LEQ3_MID; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_LOW_MOBILITY_LEQ3_MID, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.ROOK_LOW_MOBILITY_LEQ3_END; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_LOW_MOBILITY_LEQ3_END, parseInt)
  });

  definitions_.push_back({
    .name = "USE_ROOK_OPEN_FILE_BONUS",
    .uciName = "Use Rook Open File Bonus",
    .description = "Enable rook on open file bonus",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_ROOK_OPEN_FILE_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(USE_ROOK_OPEN_FILE_BONUS, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.ROOK_OPEN_FILE_MID_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_OPEN_FILE_MID_BONUS, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.ROOK_OPEN_FILE_END_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_OPEN_FILE_END_BONUS, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.ROOK_SEMIOPEN_FILE_MID_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_SEMIOPEN_FILE_MID_BONUS, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.ROOK_SEMIOPEN_FILE_END_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_SEMIOPEN_FILE_END_BONUS, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.USE_QUEEN_MOBILITY; }),
    .setter = EVAL_CONFIG_SETTER(USE_QUEEN_MOBILITY, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.QUEEN_MOBILITY_MID_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(QUEEN_MOBILITY_MID_PER_MOVE, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.QUEEN_MOBILITY_END_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(QUEEN_MOBILITY_END_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "USE_QUEEN_TROPISM",
    .uciName = "Use Queen Tropism",
    .description = "Enable queen tropism (king proximity bonus)",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_QUEEN_TROPISM; }),
    .setter = EVAL_CONFIG_SETTER(USE_QUEEN_TROPISM, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.QUEEN_TROPISM_MID_PER_STEP; }),
    .setter = EVAL_CONFIG_SETTER(QUEEN_TROPISM_MID_PER_STEP, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.QUEEN_TROPISM_END_PER_STEP; }),
    .setter = EVAL_CONFIG_SETTER(QUEEN_TROPISM_END_PER_STEP, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.USE_KING_EVAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_KING_EVAL, parseBool)
  });

  definitions_.push_back({
    .name = "USE_KING_SAFETY_SHIELD",
    .uciName = "Use King Safety Shield",
    .description = "Enable king pawn shield bonus",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = "true",
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_KING_SAFETY_SHIELD; }),
    .setter = EVAL_CONFIG_SETTER(USE_KING_SAFETY_SHIELD, parseBool)
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
    .getter = evalGetter([](const auto& e){ return e.KING_SHIELD_MID_PER_PAWN; }),
    .setter = EVAL_CONFIG_SETTER(KING_SHIELD_MID_PER_PAWN, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.KING_SHIELD_END_PER_PAWN; }),
    .setter = EVAL_CONFIG_SETTER(KING_SHIELD_END_PER_PAWN, parseInt)
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
    .getter = evalGetter([](const auto& e){ return e.USE_GAMEPHASE_VALUE; }),
    .setter = EVAL_CONFIG_SETTER(USE_GAMEPHASE_VALUE, parseBool)
  });

  // clang-format on
}
