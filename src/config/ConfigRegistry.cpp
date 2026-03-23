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

using namespace config;
using namespace engine;
using namespace common;

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
  static_assert(sizeof(SearchConfigData) == 640,
                "SearchConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
  static_assert(sizeof(EvalConfigData) == 640,
                "EvalConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
#else
  static_assert(sizeof(SearchConfigData) == 608,
                "SearchConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
  static_assert(sizeof(EvalConfigData) == 632,
                "EvalConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
#endif
#elif defined(__GNUC__) || defined(__clang__)
// Linux GCC/Clang builds (including WSL)
#ifdef NDEBUG
  // Release build
  static_assert(sizeof(SearchConfigData) == 608,
                "SearchConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
  static_assert(sizeof(EvalConfigData) == 632,
                "EvalConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
#else
  // Debug build
  static_assert(sizeof(SearchConfigData) == 608,
                "SearchConfigData size changed! Did you add/remove a member? "
                "Update registry entries in ConfigRegistry.cpp AND this sizeof value.");
  static_assert(sizeof(EvalConfigData) == 600,
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

std::vector<const ConfigDef*> ConfigRegistry::tunableOptions() const {
  std::vector<const ConfigDef*> result;
  for (const auto& definition : definitions_) {
    if (definition.exposure.tunable) {
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

  // Default instance for referencing default values - avoids duplicating defaults in two places.
  // The struct's member initializers are the single source of truth.
  static const SearchConfigData defaultSearch{};

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
    .defaultValue = configToString(defaultSearch.MOVE_OVERHEAD_MS),
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
    .defaultValue = configToString(defaultSearch.USE_BOOK),
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
    .defaultValue = defaultSearch.BOOK_PATH,
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
    .defaultValue = defaultSearch.BOOK_TYPE,
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
    .defaultValue = configToString(defaultSearch.USE_PONDER),
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
    .defaultValue = configToString(defaultSearch.USE_ALPHABETA),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_ALPHABETA), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_ALPHABETA; }),
    .setter = SEARCH_CONFIG_SETTER(USE_ALPHABETA, parseBool)
  });

  definitions_.push_back({
    .name = "USE_PVS",
    .uciName = "Use Pvs",
    .description = "Enable Principal Variation Search",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_PVS),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_PVS), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_PVS; }),
    .setter = SEARCH_CONFIG_SETTER(USE_PVS, parseBool)
  });

  definitions_.push_back({
    .name = "USE_ASP",
    .uciName = "Use Aspiration",
    .description = "Enable aspiration windows",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_ASP),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_ASP), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_ASP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_ASP, parseBool)
  });

  definitions_.push_back({
    .name = "ASP_INITIAL_DELTA",
    .uciName = "Aspiration Initial Delta",
    .description = "Initial aspiration half-window in centipawns",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.ASP_INITIAL_DELTA),
    .minValue = 1,
    .maxValue = 500,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, ASP_INITIAL_DELTA), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.ASP_INITIAL_DELTA; }),
    .setter = SEARCH_CONFIG_SETTER(ASP_INITIAL_DELTA, parseInt)
  });

  definitions_.push_back({
    .name = "ASP_DELTA_GROWTH_DIVISOR",
    .uciName = "Aspiration Delta Growth Divisor",
    .description = "Delta growth: delta += delta / divisor (3=x1.33, 2=x1.50, 1=x2.00)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.ASP_DELTA_GROWTH_DIVISOR),
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, ASP_DELTA_GROWTH_DIVISOR), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.ASP_DELTA_GROWTH_DIVISOR; }),
    .setter = SEARCH_CONFIG_SETTER(ASP_DELTA_GROWTH_DIVISOR, parseInt)
  });

  definitions_.push_back({
    .name = "USE_QUIESCENCE",
    .uciName = "Use Quiescence",
    .description = "Enable quiescence search",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_QUIESCENCE),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_QUIESCENCE), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_TT),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_TT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_TT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_TT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_TT_VALUE",
    .uciName = "Use Hash Value",
    .description = "Use TT values for cutoffs",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_TT_VALUE),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_TT_VALUE), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_TT_VALUE; }),
    .setter = SEARCH_CONFIG_SETTER(USE_TT_VALUE, parseBool)
  });

  definitions_.push_back({
    .name = "USE_EVAL_TT",
    .uciName = "Use Eval TT",
    .description = "Use TT for evaluation cache",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_EVAL_TT),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_EVAL_TT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_EVAL_TT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_EVAL_TT, parseBool)
  });

  definitions_.push_back({
    .name = "TT_SIZE_MB",
    .uciName = "Hash",
    .description = "Transposition table size in MB",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.TT_SIZE_MB),
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
    .name = "THREADS",
    .uciName = "Threads",
    .description = "Number of search threads (1 = single-threaded, no SMP overhead)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.THREADS),
    .minValue = 1,
    .maxValue = 256,
    .exposure = {.uci = true, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.THREADS; }),
    .setter = SEARCH_CONFIG_SETTER(THREADS, parseInt)
  });

  definitions_.push_back({
    .name = "SMP_HELPER_START_DEPTH",
    .uciName = "",  // Not exposed via UCI - internal tuning parameter
    .description = "Depth at which to launch helper threads (allows main thread to prime TT first)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.SMP_HELPER_START_DEPTH),
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = false, .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.SMP_HELPER_START_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(SMP_HELPER_START_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "USE_BEST_THREAD_SELECTION",
    .uciName = "Best Thread Selection",
    .description = "Select best result from any thread after SMP search (not just main thread)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_BEST_THREAD_SELECTION),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_BEST_THREAD_SELECTION), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_BEST_THREAD_SELECTION; }),
    .setter = SEARCH_CONFIG_SETTER(USE_BEST_THREAD_SELECTION, parseBool)
  });

  definitions_.push_back({
    .name = "BEST_THREAD_SCORE_MARGIN",
    .uciName = "Best Thread Score Margin",
    .description = "Score margin (centipawns) for best-thread depth vs score comparison",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.BEST_THREAD_SCORE_MARGIN),
    .minValue = 0,
    .maxValue = 500,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, BEST_THREAD_SCORE_MARGIN), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.BEST_THREAD_SCORE_MARGIN; }),
    .setter = SEARCH_CONFIG_SETTER(BEST_THREAD_SCORE_MARGIN, parseInt)
  });

  definitions_.push_back({
    .name = "USE_QS_TT",
    .uciName = "Use Hash Quiescence",
    .description = "Use TT in quiescence search",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_QS_TT),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_QS_TT), .yaml = true, .display = true},
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
    .defaultValue = defaultSearch.TB_PATH,
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
    .defaultValue = configToString(defaultSearch.USE_TB_PROBE_ROOT),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_TB_PROBE_ROOT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_TB_PROBE_ROOT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_TB_PROBE_ROOT, parseBool)
  });

  definitions_.push_back({
    .name = "TB_ROOT_IMMEDIATE",
    .uciName = "Syzygy Root Immediate",
    .description = "Return TB move immediately without searching (false = search for PV)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.TB_ROOT_IMMEDIATE),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, TB_ROOT_IMMEDIATE), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_TB_PROBE_SEARCH),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_TB_PROBE_SEARCH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_TB_PROBE_SEARCH; }),
    .setter = SEARCH_CONFIG_SETTER(USE_TB_PROBE_SEARCH, parseBool)
  });

  definitions_.push_back({
    .name = "USE_TB_PROBE_PV",
    .uciName = "",  // Not exposed via UCI - internal tuning option
    .description = "Probe tablebases on PV nodes (false = only non-PV nodes for cutoffs)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_TB_PROBE_PV),
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
    .defaultValue = configToString(defaultSearch.TB_PROBE_DEPTH),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, TB_PROBE_DEPTH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.TB_PROBE_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(TB_PROBE_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "TB_PROBE_LIMIT",
    .uciName = "Syzygy Probe Limit",
    .description = "Maximum pieces for search TB probing (3-7)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.TB_PROBE_LIMIT),
    .minValue = 3,
    .maxValue = 7,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, TB_PROBE_LIMIT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.TB_PROBE_LIMIT; }),
    .setter = SEARCH_CONFIG_SETTER(TB_PROBE_LIMIT, parseInt)
  });

  definitions_.push_back({
    .name = "TB_RULE50_THRESHOLD",
    .uciName = "Syzygy 50 Move Rule",
    .description = "HalfMoveClock threshold for DTZ accuracy check (>=100 disables)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.TB_RULE50_THRESHOLD),
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, TB_RULE50_THRESHOLD), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.TB_RULE50_THRESHOLD; }),
    .setter = SEARCH_CONFIG_SETTER(TB_RULE50_THRESHOLD, parseInt)
  });

  definitions_.push_back({
    .name = "TB_CACHE_PREWARM",
    .uciName = "",
    .description = "Pre-warm OS file cache at startup for faster first probes",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.TB_CACHE_PREWARM),
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
    .defaultValue = configToString(defaultSearch.TB_CACHE_PREWARM_PIECES),
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
    .defaultValue = configToString(defaultSearch.USE_TT_PV_MOVE_SORT),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_TT_PV_MOVE_SORT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_TT_PV_MOVE_SORT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_TT_PV_MOVE_SORT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_KILLER_MOVES",
    .uciName = "Use Killer Moves",
    .description = "Enable killer move heuristic",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_KILLER_MOVES),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_KILLER_MOVES), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_KILLER_MOVES; }),
    .setter = SEARCH_CONFIG_SETTER(USE_KILLER_MOVES, parseBool)
  });

  definitions_.push_back({
    .name = "USE_HISTORY_COUNTER",
    .uciName = "Use History Counter",
    .description = "Enable counter-move history",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_HISTORY_COUNTER),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_HISTORY_COUNTER), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_HISTORY_COUNTER; }),
    .setter = SEARCH_CONFIG_SETTER(USE_HISTORY_COUNTER, parseBool)
  });

  definitions_.push_back({
    .name = "USE_HISTORY_MOVES",
    .uciName = "Use History Moves",
    .description = "Enable history heuristic for move ordering",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_HISTORY_MOVES),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_HISTORY_MOVES), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_HISTORY_MOVES; }),
    .setter = SEARCH_CONFIG_SETTER(USE_HISTORY_MOVES, parseBool)
  });

  definitions_.push_back({
    .name = "USE_IID",
    .uciName = "Use Internal Iterative Deepening",
    .description = "Enable Internal Iterative Deepening (legacy - IIR is more effective)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_IID),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_IID), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_IID; }),
    .setter = SEARCH_CONFIG_SETTER(USE_IID, parseBool)
  });

  definitions_.push_back({
    .name = "IID_DEPTH",
    .uciName = "IID Move Depth",
    .description = "Minimum depth to trigger IID",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.IID_DEPTH),
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, IID_DEPTH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.IID_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(IID_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "IID_REDUCTION",
    .uciName = "IID Depth Reduction",
    .description = "Depth reduction for IID search",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.IID_REDUCTION),
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, IID_REDUCTION), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_IIR),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_IIR), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_IIR; }),
    .setter = SEARCH_CONFIG_SETTER(USE_IIR, parseBool)
  });

  definitions_.push_back({
    .name = "IIR_DEPTH",
    .uciName = "IIR Min Depth",
    .description = "Minimum depth to apply IIR reduction",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.IIR_DEPTH),
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, IIR_DEPTH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.IIR_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(IIR_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "IIR_REDUCTION",
    .uciName = "IIR Depth Reduction",
    .description = "How much to reduce depth when IIR triggers",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.IIR_REDUCTION),
    .minValue = 1,
    .maxValue = 5,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, IIR_REDUCTION), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.IIR_REDUCTION; }),
    .setter = SEARCH_CONFIG_SETTER(IIR_REDUCTION, parseInt)
  });

  definitions_.push_back({
    .name = "IIR_ALL_NODES",
    .uciName = "IIR All Nodes",
    .description = "Apply IIR to all node types (true) or PV only (false)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.IIR_ALL_NODES),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, IIR_ALL_NODES), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_MDP),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_MDP), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_MDP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_MDP, parseBool)
  });

  definitions_.push_back({
    .name = "USE_QS_STANDPAT_CUT",
    .uciName = "Use Quiescence Standpat",
    .description = "Enable stand-pat cutoff in quiescence",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_QS_STANDPAT_CUT),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_QS_STANDPAT_CUT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_QS_STANDPAT_CUT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_QS_STANDPAT_CUT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_QS_SEE",
    .uciName = "Use Quiescence SEE",
    .description = "Enable SEE pruning in quiescence",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_QS_SEE),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_QS_SEE), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_RAZORING),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_RAZORING), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_RAZORING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_RAZORING, parseBool)
  });

  definitions_.push_back({
    .name = "RAZOR_MARGIN",
    .uciName = "Razor Margin",
    .description = "Razoring margin in centipawns",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.RAZOR_MARGIN),
    .minValue = 0,
    .maxValue = 1000,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, RAZOR_MARGIN), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_RFP),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_RFP), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_RFP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_RFP, parseBool)
  });

  definitions_.push_back({
    .name = "RFP_MARGIN",
    .uciName = "",
    .description = "Reverse futility pruning margins by depth",
    .valueType = IntArray,
    .domain = Search,
    .defaultValue = arrayToString(defaultSearch.RFP_MARGIN),
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
    .defaultValue = configToString(defaultSearch.USE_RFP_IMPROVING),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_RFP_IMPROVING), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_RFP_IMPROVING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_RFP_IMPROVING, parseBool)
  });

  definitions_.push_back({
    .name = "RFP_IMPROVING_MARGIN",
    .uciName = "RFP Improving Margin",
    .description = "RFP margin reduction in centipawns when position is not improving",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.RFP_IMPROVING_MARGIN),
    .minValue = 0,
    .maxValue = 300,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, RFP_IMPROVING_MARGIN), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_NMP),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_NMP), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_NMP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_NMP, parseBool)
  });

  definitions_.push_back({
    .name = "NMP_DEPTH",
    .uciName = "Null Move Depth",
    .description = "Minimum depth for null move pruning",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.NMP_DEPTH),
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, NMP_DEPTH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.NMP_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "NMP_REDUCTION",
    .uciName = "Null Depth Reduction",
    .description = "Depth reduction for null move search",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.NMP_REDUCTION),
    .minValue = 1,
    .maxValue = 6,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, NMP_REDUCTION), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.NMP_REDUCTION; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_REDUCTION, parseInt)
  });

  definitions_.push_back({
    .name = "USE_NMP_VERIFY",
    .uciName = "Use Null Move Verification",
    .description = "Enable null move verification search",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_NMP_VERIFY),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_NMP_VERIFY), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_NMP_VERIFY; }),
    .setter = SEARCH_CONFIG_SETTER(USE_NMP_VERIFY, parseBool)
  });

  definitions_.push_back({
    .name = "NMP_VERIFY_MIN_DEPTH",
    .uciName = "Null Move Verify Min Depth",
    .description = "Minimum depth for null move verification",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.NMP_VERIFY_MIN_DEPTH),
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, NMP_VERIFY_MIN_DEPTH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.NMP_VERIFY_MIN_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_VERIFY_MIN_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "NMP_VERIFY_MARGIN",
    .uciName = "Null Move Verify Margin",
    .description = "Depth margin for null move verification",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.NMP_VERIFY_MARGIN),
    .minValue = 0,
    .maxValue = 10,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, NMP_VERIFY_MARGIN), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.NMP_VERIFY_MARGIN; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_VERIFY_MARGIN, parseInt)
  });

  definitions_.push_back({
    .name = "NMP_NEAR_MATE_MARGIN",
    .uciName = "Null Move Near Mate Margin",
    .description = "Margin for near-mate null move check",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.NMP_NEAR_MATE_MARGIN),
    .minValue = 0,
    .maxValue = 200,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, NMP_NEAR_MATE_MARGIN), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.NMP_NEAR_MATE_MARGIN; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_NEAR_MATE_MARGIN, parseInt)
  });

  definitions_.push_back({
    .name = "USE_NMP_ZUG_GUARD",
    .uciName = "Use Null Move Zugzwang Guard",
    .description = "Enable zugzwang guard for null move",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_NMP_ZUG_GUARD),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_NMP_ZUG_GUARD), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_NMP_ZUG_GUARD; }),
    .setter = SEARCH_CONFIG_SETTER(USE_NMP_ZUG_GUARD, parseBool)
  });

  definitions_.push_back({
    .name = "NMP_ZUG_NONPAWN_THRESHOLD",
    .uciName = "Null Move Zug NonPawn Threshold",
    .description = "Non-pawn piece threshold for zugzwang guard",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.NMP_ZUG_NONPAWN_THRESHOLD),
    .minValue = 0,
    .maxValue = 10,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, NMP_ZUG_NONPAWN_THRESHOLD), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.NMP_ZUG_NONPAWN_THRESHOLD; }),
    .setter = SEARCH_CONFIG_SETTER(NMP_ZUG_NONPAWN_THRESHOLD, parseInt)
  });

  definitions_.push_back({
    .name = "USE_NMP_IMPROVING",
    .uciName = "Null Move Improving",
    .description = "Use improving flag to increase NMP reduction when not improving",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_NMP_IMPROVING),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_NMP_IMPROVING), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_NMP_IMPROVING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_NMP_IMPROVING, parseBool)
  });

  definitions_.push_back({
    .name = "NMP_IMPROVING_REDUCTION",
    .uciName = "Null Move Improving Reduction",
    .description = "Extra NMP reduction depth when position is not improving",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.NMP_IMPROVING_REDUCTION),
    .minValue = 0,
    .maxValue = 3,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, NMP_IMPROVING_REDUCTION), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_FP),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_FP), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_FP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_FP, parseBool)
  });

  definitions_.push_back({
    .name = "USE_QFP",
    .uciName = "Use Quiescence Futility Pruning",
    .description = "Enable futility pruning in quiescence",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_QFP),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_QFP), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_QFP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_QFP, parseBool)
  });

  definitions_.push_back({
    .name = "FP_MARGIN",
    .uciName = "",
    .description = "Futility pruning margins by depth",
    .valueType = IntArray,
    .domain = Search,
    .defaultValue = arrayToString(defaultSearch.FP_MARGIN),
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
    .defaultValue = configToString(defaultSearch.USE_FP_IMPROVING),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_FP_IMPROVING), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_FP_IMPROVING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_FP_IMPROVING, parseBool)
  });

  definitions_.push_back({
    .name = "FP_IMPROVING_MARGIN",
    .uciName = "Futility Pruning Improving Margin",
    .description = "FP margin reduction in centipawns when position is not improving",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.FP_IMPROVING_MARGIN),
    .minValue = 0,
    .maxValue = 300,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, FP_IMPROVING_MARGIN), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_IMPROVING),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_IMPROVING), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_LMR),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_LMR), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_LMR; }),
    .setter = SEARCH_CONFIG_SETTER(USE_LMR, parseBool)
  });

  definitions_.push_back({
    .name = "LMR_MIN_DEPTH",
    .uciName = "LMR Min Depth",
    .description = "Minimum depth for LMR",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.LMR_MIN_DEPTH),
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, LMR_MIN_DEPTH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.LMR_MIN_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_MIN_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "LMR_MIN_MOVES",
    .uciName = "LMR Min Moves",
    .description = "Minimum moves searched before LMR",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.LMR_MIN_MOVES),
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, LMR_MIN_MOVES), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.LMR_MIN_MOVES; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_MIN_MOVES, parseInt)
  });

  definitions_.push_back({
    .name = "LMR_USE_LOG_FORMULA",
    .uciName = "LMR Use Log Formula",
    .description = "Use logarithmic formula instead of linear for LMR",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.LMR_USE_LOG_FORMULA),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, LMR_USE_LOG_FORMULA), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.LMR_USE_LOG_FORMULA; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_USE_LOG_FORMULA, parseBool)
  });

  definitions_.push_back({
    .name = "LMR_LOG_BASE_DIV",
    .uciName = "LMR Log Base Divisor Pct",
    .description = "Divisor for log formula: log(d)*log(m)/divisor",
    .valueType = Double,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.LMR_LOG_BASE_DIV),
    .minValue = 50,
    .maxValue = 500,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, LMR_LOG_BASE_DIV), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_LMR_IMPROVING),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_LMR_IMPROVING), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_LMR_IMPROVING; }),
    .setter = SEARCH_CONFIG_SETTER(USE_LMR_IMPROVING, parseBool)
  });

  definitions_.push_back({
    .name = "LMR_IMPROVING_REDUCTION",
    .uciName = "LMR Improving Reduction",
    .description = "Extra LMR reduction depth when position is not improving",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.LMR_IMPROVING_REDUCTION),
    .minValue = 0,
    .maxValue = 4,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, LMR_IMPROVING_REDUCTION), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.LMR_IMPROVING_REDUCTION; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_IMPROVING_REDUCTION, parseInt)
  });

  definitions_.push_back({
    .name = "USE_LMR_HISTORY",
    .uciName = "Use LMR History",
    .description = "Use history score to modulate LMR (less reduction for moves with good history)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_LMR_HISTORY),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_LMR_HISTORY), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_LMR_HISTORY; }),
    .setter = SEARCH_CONFIG_SETTER(USE_LMR_HISTORY, parseBool)
  });

  definitions_.push_back({
    .name = "LMR_HISTORY_DIVISOR",
    .uciName = "LMR History Divisor",
    .description = "Divisor for history score to reduction conversion (higher = less effect)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.LMR_HISTORY_DIVISOR),
    .minValue = 1024,
    .maxValue = 32768,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, LMR_HISTORY_DIVISOR), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.LMR_HISTORY_DIVISOR; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_HISTORY_DIVISOR, parseInt)
  });

  definitions_.push_back({
    .name = "USE_LMR_CUTNODE",
    .uciName = "Use LMR Cut Node",
    .description = "Extra reduction on expected cut nodes (nodes expected to fail high)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_LMR_CUTNODE),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_LMR_CUTNODE), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_LMR_CUTNODE; }),
    .setter = SEARCH_CONFIG_SETTER(USE_LMR_CUTNODE, parseBool)
  });

  definitions_.push_back({
    .name = "LMR_CUTNODE_REDUCTION",
    .uciName = "LMR Cut Node Reduction",
    .description = "Extra LMR reduction depth on cut nodes",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.LMR_CUTNODE_REDUCTION),
    .minValue = 0,
    .maxValue = 4,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, LMR_CUTNODE_REDUCTION), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.LMR_CUTNODE_REDUCTION; }),
    .setter = SEARCH_CONFIG_SETTER(LMR_CUTNODE_REDUCTION, parseInt)
  });

  definitions_.push_back({
    .name = "USE_LMP",
    .uciName = "Use Late Move Pruning",
    .description = "Enable Late Move Pruning",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_LMP),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_LMP), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_LMP; }),
    .setter = SEARCH_CONFIG_SETTER(USE_LMP, parseBool)
  });

  definitions_.push_back({
    .name = "LMP_MOVES",
    .uciName = "",
    .description = "Late move pruning move count thresholds by depth",
    .valueType = IntArray,
    .domain = Search,
    .defaultValue = arrayToString(defaultSearch.LMP_MOVES),
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
    .defaultValue = configToString(defaultSearch.USE_LMP_IMPROVING),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_LMP_IMPROVING), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_EXTENSIONS),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_EXTENSIONS), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_EXTENSIONS; }),
    .setter = SEARCH_CONFIG_SETTER(USE_EXTENSIONS, parseBool)
  });

  definitions_.push_back({
    .name = "USE_CHECK_EXT",
    .uciName = "Use Check Extension",
    .description = "Enable check extension",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_CHECK_EXT),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_CHECK_EXT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_CHECK_EXT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_CHECK_EXT, parseBool)
  });

  definitions_.push_back({
    .name = "CHECK_EXT_MIN_DEPTH",
    .uciName = "Check Ext Min Depth",
    .description = "Minimum depth to apply check extension",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.CHECK_EXT_MIN_DEPTH),
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, CHECK_EXT_MIN_DEPTH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.CHECK_EXT_MIN_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(CHECK_EXT_MIN_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "CHECK_EXT_EARLY_LIMIT",
    .uciName = "Check Ext Early Limit",
    .description = "Only extend checks in first N moves per node (99 = no limit)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.CHECK_EXT_EARLY_LIMIT),
    .minValue = 0,
    .maxValue = 99,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, CHECK_EXT_EARLY_LIMIT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.CHECK_EXT_EARLY_LIMIT; }),
    .setter = SEARCH_CONFIG_SETTER(CHECK_EXT_EARLY_LIMIT, parseInt)
  });

  definitions_.push_back({
    .name = "USE_CHECK_EXT_SEE",
    .uciName = "Check Ext SEE",
    .description = "Only extend checks with SEE >= 0 (non-losing)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_CHECK_EXT_SEE),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_CHECK_EXT_SEE), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_CHECK_EXT_SEE; }),
    .setter = SEARCH_CONFIG_SETTER(USE_CHECK_EXT_SEE, parseBool)
  });

  definitions_.push_back({
    .name = "USE_THREAT_EXT",
    .uciName = "Use Threat Extension",
    .description = "Enable threat extension",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_THREAT_EXT),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_THREAT_EXT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_THREAT_EXT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_THREAT_EXT, parseBool)
  });

  definitions_.push_back({
    .name = "THREAT_EXT_MATE_DEPTH",
    .uciName = "",  // Not exposed via UCI
    .description = "Mate depth threshold for threat extension (mate-in-N detection)",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.THREAT_EXT_MATE_DEPTH),
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
    .defaultValue = configToString(defaultSearch.USE_EXT_ADD_DEPTH),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_EXT_ADD_DEPTH), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.USE_SINGULAR_EXT),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_SINGULAR_EXT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_SINGULAR_EXT; }),
    .setter = SEARCH_CONFIG_SETTER(USE_SINGULAR_EXT, parseBool)
  });

  definitions_.push_back({
    .name = "USE_SINGULAR_TT_BOUND",
    .uciName = "Singular TT Bound",
    .description = "Require BETA/EXACT TT bound for singular (too restrictive in practice)",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_SINGULAR_TT_BOUND),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_SINGULAR_TT_BOUND), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_SINGULAR_TT_BOUND; }),
    .setter = SEARCH_CONFIG_SETTER(USE_SINGULAR_TT_BOUND, parseBool)
  });

  definitions_.push_back({
    .name = "SINGULAR_MARGIN",
    .uciName = "Singular Margin",
    .description = "Centipawns below TT value to consider singular",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.SINGULAR_MARGIN),
    .minValue = 0,
    .maxValue = 200,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, SINGULAR_MARGIN), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.SINGULAR_MARGIN; }),
    .setter = SEARCH_CONFIG_SETTER(SINGULAR_MARGIN, parseInt)
  });

  definitions_.push_back({
    .name = "SINGULAR_MIN_DEPTH",
    .uciName = "Singular Min Depth",
    .description = "Minimum depth to attempt singular extension",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.SINGULAR_MIN_DEPTH),
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, SINGULAR_MIN_DEPTH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.SINGULAR_MIN_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(SINGULAR_MIN_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "SINGULAR_REDUCTION",
    .uciName = "Singular Reduction",
    .description = "Depth reduction for verification search",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.SINGULAR_REDUCTION),
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, SINGULAR_REDUCTION), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.MOVES_LEFT_OPENING),
    .minValue = 5,
    .maxValue = 60,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, MOVES_LEFT_OPENING), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_OPENING; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_OPENING, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_MIDGAME",
    .uciName = "Moves Left Midgame",
    .description = "Estimated moves left in midgame phase",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.MOVES_LEFT_MIDGAME),
    .minValue = 5,
    .maxValue = 60,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, MOVES_LEFT_MIDGAME), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_MIDGAME; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_MIDGAME, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_ENDGAME",
    .uciName = "Moves Left Endgame",
    .description = "Estimated moves left in endgame phase",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.MOVES_LEFT_ENDGAME),
    .minValue = 5,
    .maxValue = 60,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, MOVES_LEFT_ENDGAME), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_ENDGAME; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_ENDGAME, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_LOW_MAT",
    .uciName = "Moves Left Low Material",
    .description = "Estimated moves left in low material endgame",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.MOVES_LEFT_LOW_MAT),
    .minValue = 1,
    .maxValue = 30,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, MOVES_LEFT_LOW_MAT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_LOW_MAT; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_LOW_MAT, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_QUEENLESS",
    .uciName = "Moves Left Queenless",
    .description = "Estimated moves left in queenless middlegame",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.MOVES_LEFT_QUEENLESS),
    .minValue = 5,
    .maxValue = 60,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, MOVES_LEFT_QUEENLESS), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.NPP_HEAVY_THRESHOLD),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, NPP_HEAVY_THRESHOLD), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.NPP_HEAVY_THRESHOLD; }),
    .setter = SEARCH_CONFIG_SETTER(NPP_HEAVY_THRESHOLD, parseInt)
  });

  definitions_.push_back({
    .name = "NPP_LIGHT_THRESHOLD",
    .uciName = "NPP Light Threshold",
    .description = "Non-pawn pieces threshold for light position",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.NPP_LIGHT_THRESHOLD),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, NPP_LIGHT_THRESHOLD), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.REPETITION_HMC_HIGH),
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, REPETITION_HMC_HIGH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.REPETITION_HMC_HIGH; }),
    .setter = SEARCH_CONFIG_SETTER(REPETITION_HMC_HIGH, parseInt)
  });

  definitions_.push_back({
    .name = "REPETITION_RISK_PENALTY",
    .uciName = "Repetition Risk Penalty",
    .description = "Moves-left penalty when repetition risk is high",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.REPETITION_RISK_PENALTY),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, REPETITION_RISK_PENALTY), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.REPETITION_RISK_PENALTY; }),
    .setter = SEARCH_CONFIG_SETTER(REPETITION_RISK_PENALTY, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_MIN_CLAMP",
    .uciName = "Moves Left Min Clamp",
    .description = "Minimum clamped value for moves-left estimate",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.MOVES_LEFT_MIN_CLAMP),
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, MOVES_LEFT_MIN_CLAMP), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_MIN_CLAMP; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_MIN_CLAMP, parseInt)
  });

  definitions_.push_back({
    .name = "MOVES_LEFT_MAX_CLAMP",
    .uciName = "Moves Left Max Clamp",
    .description = "Maximum clamped value for moves-left estimate",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.MOVES_LEFT_MAX_CLAMP),
    .minValue = 10,
    .maxValue = 100,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, MOVES_LEFT_MAX_CLAMP), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.MOVES_LEFT_MAX_CLAMP; }),
    .setter = SEARCH_CONFIG_SETTER(MOVES_LEFT_MAX_CLAMP, parseInt)
  });

  //===========================================================================
  // TIME MANAGEMENT - EVAL VOLATILITY
  //===========================================================================
  definitions_.push_back({
    .name = "USE_EVAL_VOLATILITY",
    .uciName = "Use Eval Volatility",
    .description = "Enable eval volatility tracking for time management",
    .valueType = Bool,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.USE_EVAL_VOLATILITY),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_EVAL_VOLATILITY), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_EVAL_VOLATILITY; }),
    .setter = SEARCH_CONFIG_SETTER(USE_EVAL_VOLATILITY, parseBool)
  });

  definitions_.push_back({
    .name = "VOLATILITY_MIN_DEPTH",
    .uciName = "Volatility Min Depth",
    .description = "Minimum depth to start eval volatility tracking",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.VOLATILITY_MIN_DEPTH),
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, VOLATILITY_MIN_DEPTH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.VOLATILITY_MIN_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(VOLATILITY_MIN_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "VOLATILITY_THRESHOLD",
    .uciName = "Volatility Threshold",
    .description = "Eval swing threshold in centipawns to trigger extra time",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.VOLATILITY_THRESHOLD),
    .minValue = 50,
    .maxValue = 500,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, VOLATILITY_THRESHOLD), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.VOLATILITY_THRESHOLD; }),
    .setter = SEARCH_CONFIG_SETTER(VOLATILITY_THRESHOLD, parseInt)
  });

  definitions_.push_back({
    .name = "VOLATILITY_FACTOR",
    .uciName = "Volatility Factor Pct",
    .description = "Multiply remaining time by this when volatile (> 1.0)",
    .valueType = Double,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.VOLATILITY_FACTOR),
    .minValue = 100,
    .maxValue = 200,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, VOLATILITY_FACTOR), .yaml = true, .display = true},
    .getter = [](const SearchConfigData& s, const EvalConfigData&) {
      return configToString(s.VOLATILITY_FACTOR);
    },
    .setter = SEARCH_CONFIG_SETTER(VOLATILITY_FACTOR, parseDouble)
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
    .defaultValue = configToString(defaultSearch.USE_BESTMOVE_INSTABILITY),
    .exposure = {.uci = IS_MUTABLE(defaultSearch, USE_BESTMOVE_INSTABILITY), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.USE_BESTMOVE_INSTABILITY; }),
    .setter = SEARCH_CONFIG_SETTER(USE_BESTMOVE_INSTABILITY, parseBool)
  });

  definitions_.push_back({
    .name = "INSTABILITY_MIN_DEPTH",
    .uciName = "Instability Min Depth",
    .description = "Minimum depth to start instability tracking",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.INSTABILITY_MIN_DEPTH),
    .minValue = 1,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, INSTABILITY_MIN_DEPTH), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.INSTABILITY_MIN_DEPTH; }),
    .setter = SEARCH_CONFIG_SETTER(INSTABILITY_MIN_DEPTH, parseInt)
  });

  definitions_.push_back({
    .name = "INSTABILITY_STABLE_COUNT",
    .uciName = "Instability Stable Count",
    .description = "Consecutive stable iterations to trigger time reduction",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.INSTABILITY_STABLE_COUNT),
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, INSTABILITY_STABLE_COUNT), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.INSTABILITY_STABLE_COUNT; }),
    .setter = SEARCH_CONFIG_SETTER(INSTABILITY_STABLE_COUNT, parseInt)
  });

  definitions_.push_back({
    .name = "INSTABILITY_CHANGE_THRESHOLD",
    .uciName = "Instability Change Threshold",
    .description = "Number of best-move changes to trigger time extension",
    .valueType = Int,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.INSTABILITY_CHANGE_THRESHOLD),
    .minValue = 1,
    .maxValue = 10,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, INSTABILITY_CHANGE_THRESHOLD), .yaml = true, .display = true},
    .getter = searchGetter([](const auto& s){ return s.INSTABILITY_CHANGE_THRESHOLD; }),
    .setter = SEARCH_CONFIG_SETTER(INSTABILITY_CHANGE_THRESHOLD, parseInt)
  });

  definitions_.push_back({
    .name = "INSTABILITY_STABLE_FACTOR",
    .uciName = "Instability Stable Factor Pct",
    .description = "Multiply remaining time by this when stable (< 1.0)",
    .valueType = Double,
    .domain = Search,
    .defaultValue = configToString(defaultSearch.INSTABILITY_STABLE_FACTOR),
    .minValue = 50,
    .maxValue = 100,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, INSTABILITY_STABLE_FACTOR), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.INSTABILITY_EXTEND_FACTOR),
    .minValue = 100,
    .maxValue = 200,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, INSTABILITY_EXTEND_FACTOR), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultSearch.MAX_EXTRA_TIME_FACTOR),
    .minValue = 50,
    .maxValue = 500,
    .exposure = {.uci = IS_MUTABLE(defaultSearch, MAX_EXTRA_TIME_FACTOR), .yaml = true, .display = true},
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

  // Default instance for referencing default values - avoids duplicating defaults in two places.
  // The struct's member initializers are the single source of truth.
  static const EvalConfigData defaultEval{};

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
    .defaultValue = configToString(defaultEval.USE_MATERIAL),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_MATERIAL), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_MATERIAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_MATERIAL, parseBool)
  });

  definitions_.push_back({
    .name = "USE_POSITIONAL",
    .uciName = "Use Positional",
    .description = "Enable positional evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_POSITIONAL),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_POSITIONAL), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultEval.USE_TEMPO),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_TEMPO), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_TEMPO; }),
    .setter = EVAL_CONFIG_SETTER(USE_TEMPO, parseBool)
  });

  definitions_.push_back({
    .name = "TEMPO",
    .uciName = "Tempo Bonus",
    .description = "Tempo bonus in centipawns",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.TEMPO),
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = IS_MUTABLE(defaultEval, TEMPO), .yaml = true, .display = true, .tunable = true},
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
    .defaultValue = configToString(defaultEval.USE_LAZY_EVAL),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_LAZY_EVAL), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_LAZY_EVAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_LAZY_EVAL, parseBool)
  });

  definitions_.push_back({
    .name = "LAZY_THRESHOLD",
    .uciName = "Lazy Threshold",
    .description = "Lazy evaluation threshold in centipawns",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.LAZY_THRESHOLD),
    .minValue = 0,
    .maxValue = 2000,
    .exposure = {.uci = IS_MUTABLE(defaultEval, LAZY_THRESHOLD), .yaml = true, .display = true, .tunable = true},
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
    .defaultValue = configToString(defaultEval.USE_PAWN_EVAL),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_PAWN_EVAL), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_PAWN_EVAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_PAWN_EVAL, parseBool)
  });

  definitions_.push_back({
    .name = "USE_PAWN_TT",
    .uciName = "Use Pawn Hash",
    .description = "Enable pawn hash table",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_PAWN_TT),
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
    .defaultValue = configToString(defaultEval.PAWN_TT_SIZE_MB),
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
    .defaultValue = configToString(defaultEval.ISOLATED_PAWN_MID_WEIGHT),
    .minValue = -100,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ISOLATED_PAWN_MID_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ISOLATED_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(ISOLATED_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "ISOLATED_PAWN_END_WEIGHT",
    .uciName = "Isolated Pawn End",
    .description = "Isolated pawn penalty in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ISOLATED_PAWN_END_WEIGHT),
    .minValue = -100,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ISOLATED_PAWN_END_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ISOLATED_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(ISOLATED_PAWN_END_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "DOUBLED_PAWN_MID_WEIGHT",
    .uciName = "Doubled Pawn Mid",
    .description = "Doubled pawn penalty in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.DOUBLED_PAWN_MID_WEIGHT),
    .minValue = -100,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, DOUBLED_PAWN_MID_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.DOUBLED_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(DOUBLED_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "DOUBLED_PAWN_END_WEIGHT",
    .uciName = "Doubled Pawn End",
    .description = "Doubled pawn penalty in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.DOUBLED_PAWN_END_WEIGHT),
    .minValue = -100,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, DOUBLED_PAWN_END_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.DOUBLED_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(DOUBLED_PAWN_END_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "PASSED_PAWN_MID_WEIGHT",
    .uciName = "Passed Pawn Mid",
    .description = "Passed pawn bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.PASSED_PAWN_MID_WEIGHT),
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = IS_MUTABLE(defaultEval, PASSED_PAWN_MID_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.PASSED_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(PASSED_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "PASSED_PAWN_END_WEIGHT",
    .uciName = "Passed Pawn End",
    .description = "Passed pawn bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.PASSED_PAWN_END_WEIGHT),
    .minValue = 0,
    .maxValue = 200,
    .exposure = {.uci = IS_MUTABLE(defaultEval, PASSED_PAWN_END_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.PASSED_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(PASSED_PAWN_END_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "USE_PASSED_PAWN_RANK_BONUS",
    .uciName = "Passed Pawn Rank Bonus",
    .description = "Add rank-based bonus on top of flat passed pawn weight",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_PASSED_PAWN_RANK_BONUS),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_PASSED_PAWN_RANK_BONUS), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_PASSED_PAWN_RANK_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(USE_PASSED_PAWN_RANK_BONUS, parseBool)
  });

  definitions_.push_back({
    .name = "PASSED_PAWN_RANK_MID_BONUS",
    .uciName = "",
    .description = "Passed pawn midgame rank bonus by relative rank 2..7",
    .valueType = IntArray,
    .domain = Eval,
    .defaultValue = arrayToString(defaultEval.PASSED_PAWN_RANK_MID_BONUS),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = [](const SearchConfigData&, const EvalConfigData& e) {
      return arrayToString(e.PASSED_PAWN_RANK_MID_BONUS);
    },
    .setter = EVAL_CONFIG_ARRAY_SETTER(PASSED_PAWN_RANK_MID_BONUS)
  });

  definitions_.push_back({
    .name = "PASSED_PAWN_RANK_END_BONUS",
    .uciName = "",
    .description = "Passed pawn endgame rank bonus by relative rank 2..7",
    .valueType = IntArray,
    .domain = Eval,
    .defaultValue = arrayToString(defaultEval.PASSED_PAWN_RANK_END_BONUS),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = [](const SearchConfigData&, const EvalConfigData& e) {
      return arrayToString(e.PASSED_PAWN_RANK_END_BONUS);
    },
    .setter = EVAL_CONFIG_ARRAY_SETTER(PASSED_PAWN_RANK_END_BONUS)
  });

  definitions_.push_back({
    .name = "BLOCKED_PAWN_MID_WEIGHT",
    .uciName = "Blocked Pawn Mid",
    .description = "Blocked pawn penalty in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.BLOCKED_PAWN_MID_WEIGHT),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, BLOCKED_PAWN_MID_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.BLOCKED_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(BLOCKED_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "BLOCKED_PAWN_END_WEIGHT",
    .uciName = "Blocked Pawn End",
    .description = "Blocked pawn penalty in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.BLOCKED_PAWN_END_WEIGHT),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, BLOCKED_PAWN_END_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.BLOCKED_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(BLOCKED_PAWN_END_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "PHALANX_PAWN_MID_WEIGHT",
    .uciName = "Phalanx Pawn Mid",
    .description = "Phalanx pawn bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.PHALANX_PAWN_MID_WEIGHT),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, PHALANX_PAWN_MID_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.PHALANX_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(PHALANX_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "PHALANX_PAWN_END_WEIGHT",
    .uciName = "Phalanx Pawn End",
    .description = "Phalanx pawn bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.PHALANX_PAWN_END_WEIGHT),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, PHALANX_PAWN_END_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.PHALANX_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(PHALANX_PAWN_END_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "SUPPORTED_PAWN_MID_WEIGHT",
    .uciName = "Supported Pawn Mid",
    .description = "Supported pawn bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.SUPPORTED_PAWN_MID_WEIGHT),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, SUPPORTED_PAWN_MID_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.SUPPORTED_PAWN_MID_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(SUPPORTED_PAWN_MID_WEIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "SUPPORTED_PAWN_END_WEIGHT",
    .uciName = "Supported Pawn End",
    .description = "Supported pawn bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.SUPPORTED_PAWN_END_WEIGHT),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, SUPPORTED_PAWN_END_WEIGHT), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.SUPPORTED_PAWN_END_WEIGHT; }),
    .setter = EVAL_CONFIG_SETTER(SUPPORTED_PAWN_END_WEIGHT, parseInt)
  });

  //===========================================================================
  // PAWN ADVANCEMENT BONUS
  //===========================================================================
  definitions_.push_back({
    .name = "USE_PAWN_ADVANCE_BONUS",
    .uciName = "Use Pawn Advance Bonus",
    .description = "Enable bonus for advanced non-passed pawns",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_PAWN_ADVANCE_BONUS),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_PAWN_ADVANCE_BONUS), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_PAWN_ADVANCE_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(USE_PAWN_ADVANCE_BONUS, parseBool)
  });

  definitions_.push_back({
    .name = "PAWN_ADVANCE_MID_BONUS",
    .uciName = "",
    .description = "Pawn advancement midgame bonus by rank (rank4..rank7)",
    .valueType = IntArray,
    .domain = Eval,
    .defaultValue = arrayToString(defaultEval.PAWN_ADVANCE_MID_BONUS),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = [](const SearchConfigData&, const EvalConfigData& e) {
      return arrayToString(e.PAWN_ADVANCE_MID_BONUS);
    },
    .setter = EVAL_CONFIG_ARRAY_SETTER(PAWN_ADVANCE_MID_BONUS)
  });

  definitions_.push_back({
    .name = "PAWN_ADVANCE_END_BONUS",
    .uciName = "",
    .description = "Pawn advancement endgame bonus by rank (rank4..rank7)",
    .valueType = IntArray,
    .domain = Eval,
    .defaultValue = arrayToString(defaultEval.PAWN_ADVANCE_END_BONUS),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = [](const SearchConfigData&, const EvalConfigData& e) {
      return arrayToString(e.PAWN_ADVANCE_END_BONUS);
    },
    .setter = EVAL_CONFIG_ARRAY_SETTER(PAWN_ADVANCE_END_BONUS)
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
    .defaultValue = configToString(defaultEval.USE_PIECE_EVAL),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_PIECE_EVAL), .yaml = true, .display = true},
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
    .defaultValue = configToString(defaultEval.USE_BISHOP_PAIR_BONUS),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_BISHOP_PAIR_BONUS), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_BISHOP_PAIR_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(USE_BISHOP_PAIR_BONUS, parseBool)
  });

  definitions_.push_back({
    .name = "BISHOP_PAIR_MID_BONUS",
    .uciName = "Bishop Pair Mid Bonus",
    .description = "Bishop pair bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.BISHOP_PAIR_MID_BONUS),
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = IS_MUTABLE(defaultEval, BISHOP_PAIR_MID_BONUS), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.BISHOP_PAIR_MID_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(BISHOP_PAIR_MID_BONUS, parseInt)
  });

  definitions_.push_back({
    .name = "BISHOP_PAIR_END_BONUS",
    .uciName = "Bishop Pair End Bonus",
    .description = "Bishop pair bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.BISHOP_PAIR_END_BONUS),
    .minValue = 0,
    .maxValue = 100,
    .exposure = {.uci = IS_MUTABLE(defaultEval, BISHOP_PAIR_END_BONUS), .yaml = true, .display = true, .tunable = true},
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
    .defaultValue = configToString(defaultEval.USE_KNIGHT_MOBILITY),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_KNIGHT_MOBILITY), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_KNIGHT_MOBILITY; }),
    .setter = EVAL_CONFIG_SETTER(USE_KNIGHT_MOBILITY, parseBool)
  });

  definitions_.push_back({
    .name = "KNIGHT_MOBILITY_MID_PER_MOVE",
    .uciName = "Knight Mobility Mid",
    .description = "Knight mobility bonus per move in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KNIGHT_MOBILITY_MID_PER_MOVE),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KNIGHT_MOBILITY_MID_PER_MOVE), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_MOBILITY_MID_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_MOBILITY_MID_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_MOBILITY_END_PER_MOVE",
    .uciName = "Knight Mobility End",
    .description = "Knight mobility bonus per move in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KNIGHT_MOBILITY_END_PER_MOVE),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KNIGHT_MOBILITY_END_PER_MOVE), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_MOBILITY_END_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_MOBILITY_END_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_LOW_MOBILITY_LEQ1_MID",
    .uciName = "Knight Low Mob LEQ1 Mid",
    .description = "Knight penalty for <=1 moves in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KNIGHT_LOW_MOBILITY_LEQ1_MID),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KNIGHT_LOW_MOBILITY_LEQ1_MID), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_LOW_MOBILITY_LEQ1_MID; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_LOW_MOBILITY_LEQ1_MID, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_LOW_MOBILITY_LEQ1_END",
    .uciName = "Knight Low Mob LEQ1 End",
    .description = "Knight penalty for <=1 moves in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KNIGHT_LOW_MOBILITY_LEQ1_END),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KNIGHT_LOW_MOBILITY_LEQ1_END), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_LOW_MOBILITY_LEQ1_END; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_LOW_MOBILITY_LEQ1_END, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_LOW_MOBILITY_LEQ2_MID",
    .uciName = "Knight Low Mob LEQ2 Mid",
    .description = "Knight penalty for <=2 moves in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KNIGHT_LOW_MOBILITY_LEQ2_MID),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KNIGHT_LOW_MOBILITY_LEQ2_MID), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_LOW_MOBILITY_LEQ2_MID; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_LOW_MOBILITY_LEQ2_MID, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_LOW_MOBILITY_LEQ2_END",
    .uciName = "Knight Low Mob LEQ2 End",
    .description = "Knight penalty for <=2 moves in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KNIGHT_LOW_MOBILITY_LEQ2_END),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KNIGHT_LOW_MOBILITY_LEQ2_END), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_LOW_MOBILITY_LEQ2_END; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_LOW_MOBILITY_LEQ2_END, parseInt)
  });

  //===========================================================================
  // KNIGHT OUTPOST
  //===========================================================================
  definitions_.push_back({
    .name = "USE_KNIGHT_OUTPOST",
    .uciName = "Use Knight Outpost",
    .description = "Enable knight outpost evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_KNIGHT_OUTPOST),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_KNIGHT_OUTPOST), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_KNIGHT_OUTPOST; }),
    .setter = EVAL_CONFIG_SETTER(USE_KNIGHT_OUTPOST, parseBool)
  });

  definitions_.push_back({
    .name = "KNIGHT_OUTPOST_SUPPORTED_MID",
    .uciName = "Knight Outpost Supp Mid",
    .description = "Knight outpost bonus (pawn-supported) in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KNIGHT_OUTPOST_SUPPORTED_MID),
    .minValue = 0,
    .maxValue = 60,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KNIGHT_OUTPOST_SUPPORTED_MID), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_OUTPOST_SUPPORTED_MID; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_OUTPOST_SUPPORTED_MID, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_OUTPOST_SUPPORTED_END",
    .uciName = "Knight Outpost Supp End",
    .description = "Knight outpost bonus (pawn-supported) in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KNIGHT_OUTPOST_SUPPORTED_END),
    .minValue = 0,
    .maxValue = 60,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KNIGHT_OUTPOST_SUPPORTED_END), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_OUTPOST_SUPPORTED_END; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_OUTPOST_SUPPORTED_END, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_OUTPOST_UNSUPPORTED_MID",
    .uciName = "Knight Outpost Unsupp Mid",
    .description = "Knight outpost bonus (unsupported) in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KNIGHT_OUTPOST_UNSUPPORTED_MID),
    .minValue = 0,
    .maxValue = 40,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KNIGHT_OUTPOST_UNSUPPORTED_MID), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_OUTPOST_UNSUPPORTED_MID; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_OUTPOST_UNSUPPORTED_MID, parseInt)
  });

  definitions_.push_back({
    .name = "KNIGHT_OUTPOST_UNSUPPORTED_END",
    .uciName = "Knight Outpost Unsupp End",
    .description = "Knight outpost bonus (unsupported) in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KNIGHT_OUTPOST_UNSUPPORTED_END),
    .minValue = 0,
    .maxValue = 40,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KNIGHT_OUTPOST_UNSUPPORTED_END), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KNIGHT_OUTPOST_UNSUPPORTED_END; }),
    .setter = EVAL_CONFIG_SETTER(KNIGHT_OUTPOST_UNSUPPORTED_END, parseInt)
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
    .defaultValue = configToString(defaultEval.USE_BISHOP_MOBILITY),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_BISHOP_MOBILITY), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_BISHOP_MOBILITY; }),
    .setter = EVAL_CONFIG_SETTER(USE_BISHOP_MOBILITY, parseBool)
  });

  definitions_.push_back({
    .name = "BISHOP_MOBILITY_MID_PER_MOVE",
    .uciName = "Bishop Mobility Mid",
    .description = "Bishop mobility bonus per move in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.BISHOP_MOBILITY_MID_PER_MOVE),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, BISHOP_MOBILITY_MID_PER_MOVE), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.BISHOP_MOBILITY_MID_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(BISHOP_MOBILITY_MID_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "BISHOP_MOBILITY_END_PER_MOVE",
    .uciName = "Bishop Mobility End",
    .description = "Bishop mobility bonus per move in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.BISHOP_MOBILITY_END_PER_MOVE),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, BISHOP_MOBILITY_END_PER_MOVE), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.BISHOP_MOBILITY_END_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(BISHOP_MOBILITY_END_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "BISHOP_LOW_MOBILITY_LEQ3_MID",
    .uciName = "Bishop Low Mob LEQ3 Mid",
    .description = "Bishop penalty for <=3 moves in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.BISHOP_LOW_MOBILITY_LEQ3_MID),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, BISHOP_LOW_MOBILITY_LEQ3_MID), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.BISHOP_LOW_MOBILITY_LEQ3_MID; }),
    .setter = EVAL_CONFIG_SETTER(BISHOP_LOW_MOBILITY_LEQ3_MID, parseInt)
  });

  definitions_.push_back({
    .name = "BISHOP_LOW_MOBILITY_LEQ3_END",
    .uciName = "Bishop Low Mob LEQ3 End",
    .description = "Bishop penalty for <=3 moves in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.BISHOP_LOW_MOBILITY_LEQ3_END),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, BISHOP_LOW_MOBILITY_LEQ3_END), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.BISHOP_LOW_MOBILITY_LEQ3_END; }),
    .setter = EVAL_CONFIG_SETTER(BISHOP_LOW_MOBILITY_LEQ3_END, parseInt)
  });

  //===========================================================================
  // BAD BISHOP
  //===========================================================================
  definitions_.push_back({
    .name = "USE_BAD_BISHOP",
    .uciName = "Use Bad Bishop",
    .description = "Enable bad bishop penalty (pawns on bishop color)",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_BAD_BISHOP),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_BAD_BISHOP), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_BAD_BISHOP; }),
    .setter = EVAL_CONFIG_SETTER(USE_BAD_BISHOP, parseBool)
  });

  definitions_.push_back({
    .name = "BAD_BISHOP_PER_PAWN_MID",
    .uciName = "Bad Bishop Per Pawn Mid",
    .description = "Bad bishop penalty per own pawn on bishop color in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.BAD_BISHOP_PER_PAWN_MID),
    .minValue = -20,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, BAD_BISHOP_PER_PAWN_MID), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.BAD_BISHOP_PER_PAWN_MID; }),
    .setter = EVAL_CONFIG_SETTER(BAD_BISHOP_PER_PAWN_MID, parseInt)
  });

  definitions_.push_back({
    .name = "BAD_BISHOP_PER_PAWN_END",
    .uciName = "Bad Bishop Per Pawn End",
    .description = "Bad bishop penalty per own pawn on bishop color in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.BAD_BISHOP_PER_PAWN_END),
    .minValue = -20,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, BAD_BISHOP_PER_PAWN_END), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.BAD_BISHOP_PER_PAWN_END; }),
    .setter = EVAL_CONFIG_SETTER(BAD_BISHOP_PER_PAWN_END, parseInt)
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
    .defaultValue = configToString(defaultEval.USE_ROOK_MOBILITY),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_ROOK_MOBILITY), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_ROOK_MOBILITY; }),
    .setter = EVAL_CONFIG_SETTER(USE_ROOK_MOBILITY, parseBool)
  });

  definitions_.push_back({
    .name = "ROOK_MOBILITY_MID_PER_MOVE",
    .uciName = "Rook Mobility Mid",
    .description = "Rook mobility bonus per move in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_MOBILITY_MID_PER_MOVE),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_MOBILITY_MID_PER_MOVE), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_MOBILITY_MID_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_MOBILITY_MID_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_MOBILITY_END_PER_MOVE",
    .uciName = "Rook Mobility End",
    .description = "Rook mobility bonus per move in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_MOBILITY_END_PER_MOVE),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_MOBILITY_END_PER_MOVE), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_MOBILITY_END_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_MOBILITY_END_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_LOW_MOBILITY_LEQ3_MID",
    .uciName = "Rook Low Mob LEQ3 Mid",
    .description = "Rook penalty for <=3 moves in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_LOW_MOBILITY_LEQ3_MID),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_LOW_MOBILITY_LEQ3_MID), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_LOW_MOBILITY_LEQ3_MID; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_LOW_MOBILITY_LEQ3_MID, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_LOW_MOBILITY_LEQ3_END",
    .uciName = "Rook Low Mob LEQ3 End",
    .description = "Rook penalty for <=3 moves in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_LOW_MOBILITY_LEQ3_END),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_LOW_MOBILITY_LEQ3_END), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_LOW_MOBILITY_LEQ3_END; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_LOW_MOBILITY_LEQ3_END, parseInt)
  });

  definitions_.push_back({
    .name = "USE_ROOK_OPEN_FILE_BONUS",
    .uciName = "Use Rook Open File Bonus",
    .description = "Enable rook on open file bonus",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_ROOK_OPEN_FILE_BONUS),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_ROOK_OPEN_FILE_BONUS), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_ROOK_OPEN_FILE_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(USE_ROOK_OPEN_FILE_BONUS, parseBool)
  });

  definitions_.push_back({
    .name = "ROOK_OPEN_FILE_MID_BONUS",
    .uciName = "Rook Open File Mid",
    .description = "Rook on open file bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_OPEN_FILE_MID_BONUS),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_OPEN_FILE_MID_BONUS), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_OPEN_FILE_MID_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_OPEN_FILE_MID_BONUS, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_OPEN_FILE_END_BONUS",
    .uciName = "Rook Open File End",
    .description = "Rook on open file bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_OPEN_FILE_END_BONUS),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_OPEN_FILE_END_BONUS), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_OPEN_FILE_END_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_OPEN_FILE_END_BONUS, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_SEMIOPEN_FILE_MID_BONUS",
    .uciName = "Rook Semiopen File Mid",
    .description = "Rook on semi-open file bonus in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_SEMIOPEN_FILE_MID_BONUS),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_SEMIOPEN_FILE_MID_BONUS), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_SEMIOPEN_FILE_MID_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_SEMIOPEN_FILE_MID_BONUS, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_SEMIOPEN_FILE_END_BONUS",
    .uciName = "Rook Semiopen File End",
    .description = "Rook on semi-open file bonus in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_SEMIOPEN_FILE_END_BONUS),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_SEMIOPEN_FILE_END_BONUS), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_SEMIOPEN_FILE_END_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_SEMIOPEN_FILE_END_BONUS, parseInt)
  });

  definitions_.push_back({
    .name = "USE_ROOK_7TH_RANK_BONUS",
    .uciName = "Rook 7th Rank Bonus",
    .description = "Bonus for rook on 7th rank (relative to its color)",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_ROOK_7TH_RANK_BONUS),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_ROOK_7TH_RANK_BONUS), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_ROOK_7TH_RANK_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(USE_ROOK_7TH_RANK_BONUS, parseBool)
  });

  definitions_.push_back({
    .name = "ROOK_7TH_RANK_MID_BONUS",
    .uciName = "Rook 7th Mid",
    .description = "Rook on 7th rank midgame bonus",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_7TH_RANK_MID_BONUS),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_7TH_RANK_MID_BONUS), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_7TH_RANK_MID_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_7TH_RANK_MID_BONUS, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_7TH_RANK_END_BONUS",
    .uciName = "Rook 7th End",
    .description = "Rook on 7th rank endgame bonus",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_7TH_RANK_END_BONUS),
    .minValue = 0,
    .maxValue = 80,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_7TH_RANK_END_BONUS), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_7TH_RANK_END_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_7TH_RANK_END_BONUS, parseInt)
  });

  //===========================================================================
  // ROOK BEHIND PASSED PAWN
  //===========================================================================
  definitions_.push_back({
    .name = "USE_ROOK_BEHIND_PASSER",
    .uciName = "Use Rook Behind Passer",
    .description = "Enable rook behind passed pawn bonus",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_ROOK_BEHIND_PASSER),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_ROOK_BEHIND_PASSER), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_ROOK_BEHIND_PASSER; }),
    .setter = EVAL_CONFIG_SETTER(USE_ROOK_BEHIND_PASSER, parseBool)
  });

  definitions_.push_back({
    .name = "ROOK_BEHIND_PASSER_OWN_MID",
    .uciName = "Rook Behind Own Passer Mid",
    .description = "Rook behind own passed pawn midgame bonus",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_BEHIND_PASSER_OWN_MID),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_BEHIND_PASSER_OWN_MID), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_BEHIND_PASSER_OWN_MID; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_BEHIND_PASSER_OWN_MID, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_BEHIND_PASSER_OWN_END",
    .uciName = "Rook Behind Own Passer End",
    .description = "Rook behind own passed pawn endgame bonus",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_BEHIND_PASSER_OWN_END),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_BEHIND_PASSER_OWN_END), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_BEHIND_PASSER_OWN_END; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_BEHIND_PASSER_OWN_END, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_BEHIND_PASSER_OPP_MID",
    .uciName = "Rook Behind Opp Passer Mid",
    .description = "Rook behind enemy passed pawn midgame bonus",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_BEHIND_PASSER_OPP_MID),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_BEHIND_PASSER_OPP_MID), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_BEHIND_PASSER_OPP_MID; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_BEHIND_PASSER_OPP_MID, parseInt)
  });

  definitions_.push_back({
    .name = "ROOK_BEHIND_PASSER_OPP_END",
    .uciName = "Rook Behind Opp Passer End",
    .description = "Rook behind enemy passed pawn endgame bonus",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.ROOK_BEHIND_PASSER_OPP_END),
    .minValue = 0,
    .maxValue = 50,
    .exposure = {.uci = IS_MUTABLE(defaultEval, ROOK_BEHIND_PASSER_OPP_END), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.ROOK_BEHIND_PASSER_OPP_END; }),
    .setter = EVAL_CONFIG_SETTER(ROOK_BEHIND_PASSER_OPP_END, parseInt)
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
    .defaultValue = configToString(defaultEval.USE_QUEEN_MOBILITY),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_QUEEN_MOBILITY), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_QUEEN_MOBILITY; }),
    .setter = EVAL_CONFIG_SETTER(USE_QUEEN_MOBILITY, parseBool)
  });

  definitions_.push_back({
    .name = "QUEEN_MOBILITY_MID_PER_MOVE",
    .uciName = "Queen Mobility Mid",
    .description = "Queen mobility bonus per move in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.QUEEN_MOBILITY_MID_PER_MOVE),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, QUEEN_MOBILITY_MID_PER_MOVE), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.QUEEN_MOBILITY_MID_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(QUEEN_MOBILITY_MID_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "QUEEN_MOBILITY_END_PER_MOVE",
    .uciName = "Queen Mobility End",
    .description = "Queen mobility bonus per move in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.QUEEN_MOBILITY_END_PER_MOVE),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, QUEEN_MOBILITY_END_PER_MOVE), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.QUEEN_MOBILITY_END_PER_MOVE; }),
    .setter = EVAL_CONFIG_SETTER(QUEEN_MOBILITY_END_PER_MOVE, parseInt)
  });

  definitions_.push_back({
    .name = "USE_QUEEN_TROPISM",
    .uciName = "Use Queen Tropism",
    .description = "Enable queen tropism (king proximity bonus)",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_QUEEN_TROPISM),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_QUEEN_TROPISM), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_QUEEN_TROPISM; }),
    .setter = EVAL_CONFIG_SETTER(USE_QUEEN_TROPISM, parseBool)
  });

  definitions_.push_back({
    .name = "QUEEN_TROPISM_MID_PER_STEP",
    .uciName = "Queen Tropism Mid",
    .description = "Queen tropism bonus per step closer in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.QUEEN_TROPISM_MID_PER_STEP),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, QUEEN_TROPISM_MID_PER_STEP), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.QUEEN_TROPISM_MID_PER_STEP; }),
    .setter = EVAL_CONFIG_SETTER(QUEEN_TROPISM_MID_PER_STEP, parseInt)
  });

  definitions_.push_back({
    .name = "QUEEN_TROPISM_END_PER_STEP",
    .uciName = "Queen Tropism End",
    .description = "Queen tropism bonus per step closer in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.QUEEN_TROPISM_END_PER_STEP),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, QUEEN_TROPISM_END_PER_STEP), .yaml = true, .display = true, .tunable = true},
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
    .defaultValue = configToString(defaultEval.USE_KING_EVAL),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_KING_EVAL), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_KING_EVAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_KING_EVAL, parseBool)
  });

  definitions_.push_back({
    .name = "USE_KING_SAFETY_SHIELD",
    .uciName = "Use King Safety Shield",
    .description = "Enable king pawn shield bonus",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_KING_SAFETY_SHIELD),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_KING_SAFETY_SHIELD), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_KING_SAFETY_SHIELD; }),
    .setter = EVAL_CONFIG_SETTER(USE_KING_SAFETY_SHIELD, parseBool)
  });

  definitions_.push_back({
    .name = "KING_SHIELD_MID_PER_PAWN",
    .uciName = "King Shield Mid",
    .description = "King pawn shield bonus per pawn in middlegame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KING_SHIELD_MID_PER_PAWN),
    .minValue = 0,
    .maxValue = 30,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KING_SHIELD_MID_PER_PAWN), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KING_SHIELD_MID_PER_PAWN; }),
    .setter = EVAL_CONFIG_SETTER(KING_SHIELD_MID_PER_PAWN, parseInt)
  });

  definitions_.push_back({
    .name = "KING_SHIELD_END_PER_PAWN",
    .uciName = "King Shield End",
    .description = "King pawn shield bonus per pawn in endgame",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KING_SHIELD_END_PER_PAWN),
    .minValue = 0,
    .maxValue = 30,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KING_SHIELD_END_PER_PAWN), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KING_SHIELD_END_PER_PAWN; }),
    .setter = EVAL_CONFIG_SETTER(KING_SHIELD_END_PER_PAWN, parseInt)
  });

  definitions_.push_back({
    .name = "USE_KING_PAWN_PROXIMITY",
    .uciName = "King Pawn Proximity",
    .description = "Endgame bonus for king proximity to passed pawns",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_KING_PAWN_PROXIMITY),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_KING_PAWN_PROXIMITY), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_KING_PAWN_PROXIMITY; }),
    .setter = EVAL_CONFIG_SETTER(USE_KING_PAWN_PROXIMITY, parseBool)
  });

  definitions_.push_back({
    .name = "KING_OWN_PASSED_PROXIMITY_END",
    .uciName = "King Own Passer Prox",
    .description = "Endgame bonus per step of closeness to own passed pawns",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KING_OWN_PASSED_PROXIMITY_END),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KING_OWN_PASSED_PROXIMITY_END), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KING_OWN_PASSED_PROXIMITY_END; }),
    .setter = EVAL_CONFIG_SETTER(KING_OWN_PASSED_PROXIMITY_END, parseInt)
  });

  definitions_.push_back({
    .name = "KING_OPP_PASSED_PROXIMITY_END",
    .uciName = "King Opp Passer Prox",
    .description = "Endgame bonus per step of closeness to enemy passed pawns (defending)",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KING_OPP_PASSED_PROXIMITY_END),
    .minValue = 0,
    .maxValue = 20,
    .exposure = {.uci = IS_MUTABLE(defaultEval, KING_OPP_PASSED_PROXIMITY_END), .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KING_OPP_PASSED_PROXIMITY_END; }),
    .setter = EVAL_CONFIG_SETTER(KING_OPP_PASSED_PROXIMITY_END, parseInt)
  });

  definitions_.push_back({
    .name = "USE_KING_SAFETY_ATTACK",
    .uciName = "King Safety Attack",
    .description = "Enable king zone attack evaluation",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_KING_SAFETY_ATTACK),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_KING_SAFETY_ATTACK), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_KING_SAFETY_ATTACK; }),
    .setter = EVAL_CONFIG_SETTER(USE_KING_SAFETY_ATTACK, parseBool)
  });

  definitions_.push_back({
    .name = "KING_ATTACK_WEIGHT_KNIGHT",
    .uciName = "",
    .description = "Attack weight for knight attacking king zone",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KING_ATTACK_WEIGHT_KNIGHT),
    .minValue = 0,
    .maxValue = 10,
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KING_ATTACK_WEIGHT_KNIGHT; }),
    .setter = EVAL_CONFIG_SETTER(KING_ATTACK_WEIGHT_KNIGHT, parseInt)
  });

  definitions_.push_back({
    .name = "KING_ATTACK_WEIGHT_BISHOP",
    .uciName = "",
    .description = "Attack weight for bishop attacking king zone",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KING_ATTACK_WEIGHT_BISHOP),
    .minValue = 0,
    .maxValue = 10,
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KING_ATTACK_WEIGHT_BISHOP; }),
    .setter = EVAL_CONFIG_SETTER(KING_ATTACK_WEIGHT_BISHOP, parseInt)
  });

  definitions_.push_back({
    .name = "KING_ATTACK_WEIGHT_ROOK",
    .uciName = "",
    .description = "Attack weight for rook attacking king zone",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KING_ATTACK_WEIGHT_ROOK),
    .minValue = 0,
    .maxValue = 10,
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KING_ATTACK_WEIGHT_ROOK; }),
    .setter = EVAL_CONFIG_SETTER(KING_ATTACK_WEIGHT_ROOK, parseInt)
  });

  definitions_.push_back({
    .name = "KING_ATTACK_WEIGHT_QUEEN",
    .uciName = "",
    .description = "Attack weight for queen attacking king zone",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KING_ATTACK_WEIGHT_QUEEN),
    .minValue = 0,
    .maxValue = 10,
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KING_ATTACK_WEIGHT_QUEEN; }),
    .setter = EVAL_CONFIG_SETTER(KING_ATTACK_WEIGHT_QUEEN, parseInt)
  });

  definitions_.push_back({
    .name = "KING_SAFETY_TABLE",
    .uciName = "",
    .description = "Non-linear king safety penalty by total attack weight (0..15)",
    .valueType = IntArray,
    .domain = Eval,
    .defaultValue = arrayToString(defaultEval.KING_SAFETY_TABLE),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = [](const SearchConfigData&, const EvalConfigData& e) {
      return arrayToString(e.KING_SAFETY_TABLE);
    },
    .setter = EVAL_CONFIG_ARRAY_SETTER(KING_SAFETY_TABLE)
  });

  //===========================================================================
  // PAWN STORM
  //===========================================================================
  definitions_.push_back({
    .name = "USE_PAWN_STORM",
    .uciName = "Use Pawn Storm",
    .description = "Enable pawn storm detection (penalty for opponent pawns advancing toward king)",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_PAWN_STORM),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_PAWN_STORM), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_PAWN_STORM; }),
    .setter = EVAL_CONFIG_SETTER(USE_PAWN_STORM, parseBool)
  });

  definitions_.push_back({
    .name = "PAWN_STORM_MID_PENALTY",
    .uciName = "",
    .description = "Pawn storm midgame penalty by rank advancement (rank4..rank7)",
    .valueType = IntArray,
    .domain = Eval,
    .defaultValue = arrayToString(defaultEval.PAWN_STORM_MID_PENALTY),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = [](const SearchConfigData&, const EvalConfigData& e) {
      return arrayToString(e.PAWN_STORM_MID_PENALTY);
    },
    .setter = EVAL_CONFIG_ARRAY_SETTER(PAWN_STORM_MID_PENALTY)
  });

  //===========================================================================
  // KING OPEN FILE
  //===========================================================================
  definitions_.push_back({
    .name = "USE_KING_OPEN_FILE",
    .uciName = "Use King Open File",
    .description = "Enable penalty for open/semi-open files near king",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_KING_OPEN_FILE),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_KING_OPEN_FILE), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_KING_OPEN_FILE; }),
    .setter = EVAL_CONFIG_SETTER(USE_KING_OPEN_FILE, parseBool)
  });

  definitions_.push_back({
    .name = "KING_OPEN_FILE_MID_PENALTY",
    .uciName = "",
    .description = "Midgame penalty for fully open file near king",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KING_OPEN_FILE_MID_PENALTY),
    .minValue = -100,
    .maxValue = 0,
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KING_OPEN_FILE_MID_PENALTY; }),
    .setter = EVAL_CONFIG_SETTER(KING_OPEN_FILE_MID_PENALTY, parseInt)
  });

  definitions_.push_back({
    .name = "KING_SEMIOPEN_FILE_MID_PENALTY",
    .uciName = "",
    .description = "Midgame penalty for semi-open file near king (no own pawn, enemy pawn present)",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.KING_SEMIOPEN_FILE_MID_PENALTY),
    .minValue = -100,
    .maxValue = 0,
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.KING_SEMIOPEN_FILE_MID_PENALTY; }),
    .setter = EVAL_CONFIG_SETTER(KING_SEMIOPEN_FILE_MID_PENALTY, parseInt)
  });

  //===========================================================================
  // SAFE CHECK SQUARES
  //===========================================================================
  definitions_.push_back({
    .name = "USE_SAFE_CHECK",
    .uciName = "Use Safe Check",
    .description = "Enable penalty for safe check squares around king",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_SAFE_CHECK),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_SAFE_CHECK), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_SAFE_CHECK; }),
    .setter = EVAL_CONFIG_SETTER(USE_SAFE_CHECK, parseBool)
  });

  definitions_.push_back({
    .name = "SAFE_CHECK_KNIGHT_MID",
    .uciName = "",
    .description = "Midgame penalty per safe knight check square",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.SAFE_CHECK_KNIGHT_MID),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.SAFE_CHECK_KNIGHT_MID; }),
    .setter = EVAL_CONFIG_SETTER(SAFE_CHECK_KNIGHT_MID, parseInt)
  });

  definitions_.push_back({
    .name = "SAFE_CHECK_BISHOP_MID",
    .uciName = "",
    .description = "Midgame penalty per safe bishop check square",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.SAFE_CHECK_BISHOP_MID),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.SAFE_CHECK_BISHOP_MID; }),
    .setter = EVAL_CONFIG_SETTER(SAFE_CHECK_BISHOP_MID, parseInt)
  });

  definitions_.push_back({
    .name = "SAFE_CHECK_ROOK_MID",
    .uciName = "",
    .description = "Midgame penalty per safe rook check square",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.SAFE_CHECK_ROOK_MID),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.SAFE_CHECK_ROOK_MID; }),
    .setter = EVAL_CONFIG_SETTER(SAFE_CHECK_ROOK_MID, parseInt)
  });

  definitions_.push_back({
    .name = "SAFE_CHECK_QUEEN_MID",
    .uciName = "",
    .description = "Midgame penalty per safe queen check square",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.SAFE_CHECK_QUEEN_MID),
    .minValue = -50,
    .maxValue = 0,
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.SAFE_CHECK_QUEEN_MID; }),
    .setter = EVAL_CONFIG_SETTER(SAFE_CHECK_QUEEN_MID, parseInt)
  });

  //===========================================================================
  // THREAT EVALUATION
  //===========================================================================
  definitions_.push_back({
    .name = "USE_THREAT_EVAL",
    .uciName = "Use Threat Eval",
    .description = "Enable threat evaluation (pawn/minor attacks on pieces, hanging pieces)",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_THREAT_EVAL),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_THREAT_EVAL), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_THREAT_EVAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_THREAT_EVAL, parseBool)
  });
  definitions_.push_back({
    .name = "THREAT_BY_PAWN_MINOR_MID",
    .uciName = "",
    .description = "Threat bonus (mid): pawn attacks minor piece",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_BY_PAWN_MINOR_MID),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_BY_PAWN_MINOR_MID; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_BY_PAWN_MINOR_MID, parseInt)
  });
  definitions_.push_back({
    .name = "THREAT_BY_PAWN_MINOR_END",
    .uciName = "",
    .description = "Threat bonus (end): pawn attacks minor piece",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_BY_PAWN_MINOR_END),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_BY_PAWN_MINOR_END; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_BY_PAWN_MINOR_END, parseInt)
  });
  definitions_.push_back({
    .name = "THREAT_BY_PAWN_ROOK_MID",
    .uciName = "",
    .description = "Threat bonus (mid): pawn attacks rook",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_BY_PAWN_ROOK_MID),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_BY_PAWN_ROOK_MID; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_BY_PAWN_ROOK_MID, parseInt)
  });
  definitions_.push_back({
    .name = "THREAT_BY_PAWN_ROOK_END",
    .uciName = "",
    .description = "Threat bonus (end): pawn attacks rook",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_BY_PAWN_ROOK_END),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_BY_PAWN_ROOK_END; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_BY_PAWN_ROOK_END, parseInt)
  });
  definitions_.push_back({
    .name = "THREAT_BY_PAWN_QUEEN_MID",
    .uciName = "",
    .description = "Threat bonus (mid): pawn attacks queen",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_BY_PAWN_QUEEN_MID),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_BY_PAWN_QUEEN_MID; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_BY_PAWN_QUEEN_MID, parseInt)
  });
  definitions_.push_back({
    .name = "THREAT_BY_PAWN_QUEEN_END",
    .uciName = "",
    .description = "Threat bonus (end): pawn attacks queen",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_BY_PAWN_QUEEN_END),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_BY_PAWN_QUEEN_END; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_BY_PAWN_QUEEN_END, parseInt)
  });
  definitions_.push_back({
    .name = "THREAT_BY_MINOR_ROOK_MID",
    .uciName = "",
    .description = "Threat bonus (mid): minor piece attacks rook",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_BY_MINOR_ROOK_MID),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_BY_MINOR_ROOK_MID; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_BY_MINOR_ROOK_MID, parseInt)
  });
  definitions_.push_back({
    .name = "THREAT_BY_MINOR_ROOK_END",
    .uciName = "",
    .description = "Threat bonus (end): minor piece attacks rook",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_BY_MINOR_ROOK_END),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_BY_MINOR_ROOK_END; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_BY_MINOR_ROOK_END, parseInt)
  });
  definitions_.push_back({
    .name = "THREAT_BY_MINOR_QUEEN_MID",
    .uciName = "",
    .description = "Threat bonus (mid): minor piece attacks queen",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_BY_MINOR_QUEEN_MID),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_BY_MINOR_QUEEN_MID; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_BY_MINOR_QUEEN_MID, parseInt)
  });
  definitions_.push_back({
    .name = "THREAT_BY_MINOR_QUEEN_END",
    .uciName = "",
    .description = "Threat bonus (end): minor piece attacks queen",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_BY_MINOR_QUEEN_END),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_BY_MINOR_QUEEN_END; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_BY_MINOR_QUEEN_END, parseInt)
  });
  definitions_.push_back({
    .name = "THREAT_HANGING_MID",
    .uciName = "",
    .description = "Threat bonus (mid): per hanging enemy piece",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_HANGING_MID),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_HANGING_MID; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_HANGING_MID, parseInt)
  });
  definitions_.push_back({
    .name = "THREAT_HANGING_END",
    .uciName = "",
    .description = "Threat bonus (end): per hanging enemy piece",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.THREAT_HANGING_END),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.THREAT_HANGING_END; }),
    .setter = EVAL_CONFIG_SETTER(THREAT_HANGING_END, parseInt)
  });

  //===========================================================================
  // SPACE EVALUATION
  //===========================================================================
  definitions_.push_back({
    .name = "USE_SPACE_EVAL",
    .uciName = "Use Space Eval",
    .description = "Enable space evaluation (safe squares behind pawn chain)",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_SPACE_EVAL),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_SPACE_EVAL), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_SPACE_EVAL; }),
    .setter = EVAL_CONFIG_SETTER(USE_SPACE_EVAL, parseBool)
  });
  definitions_.push_back({
    .name = "SPACE_BONUS_MID",
    .uciName = "",
    .description = "Space bonus per safe square (midgame)",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.SPACE_BONUS_MID),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.SPACE_BONUS_MID; }),
    .setter = EVAL_CONFIG_SETTER(SPACE_BONUS_MID, parseInt)
  });
  definitions_.push_back({
    .name = "SPACE_BONUS_END",
    .uciName = "",
    .description = "Space bonus per safe square (endgame)",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.SPACE_BONUS_END),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.SPACE_BONUS_END; }),
    .setter = EVAL_CONFIG_SETTER(SPACE_BONUS_END, parseInt)
  });

  //===========================================================================
  // PIECE COORDINATION
  //===========================================================================
  definitions_.push_back({
    .name = "USE_CONNECTED_ROOKS",
    .uciName = "Use Connected Rooks",
    .description = "Bonus for connected rooks (same rank/file, no pieces between)",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_CONNECTED_ROOKS),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_CONNECTED_ROOKS), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_CONNECTED_ROOKS; }),
    .setter = EVAL_CONFIG_SETTER(USE_CONNECTED_ROOKS, parseBool)
  });
  definitions_.push_back({
    .name = "CONNECTED_ROOKS_MID_BONUS",
    .uciName = "",
    .description = "Connected rooks bonus (midgame)",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.CONNECTED_ROOKS_MID_BONUS),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.CONNECTED_ROOKS_MID_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(CONNECTED_ROOKS_MID_BONUS, parseInt)
  });
  definitions_.push_back({
    .name = "CONNECTED_ROOKS_END_BONUS",
    .uciName = "",
    .description = "Connected rooks bonus (endgame)",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.CONNECTED_ROOKS_END_BONUS),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.CONNECTED_ROOKS_END_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(CONNECTED_ROOKS_END_BONUS, parseInt)
  });
  definitions_.push_back({
    .name = "USE_MINOR_CONNECTIVITY",
    .uciName = "Use Minor Connectivity",
    .description = "Bonus for minor pieces defended by another minor piece",
    .valueType = Bool,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.USE_MINOR_CONNECTIVITY),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_MINOR_CONNECTIVITY), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_MINOR_CONNECTIVITY; }),
    .setter = EVAL_CONFIG_SETTER(USE_MINOR_CONNECTIVITY, parseBool)
  });
  definitions_.push_back({
    .name = "MINOR_CONNECTIVITY_MID_BONUS",
    .uciName = "",
    .description = "Minor connectivity bonus per connection (midgame)",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.MINOR_CONNECTIVITY_MID_BONUS),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.MINOR_CONNECTIVITY_MID_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(MINOR_CONNECTIVITY_MID_BONUS, parseInt)
  });
  definitions_.push_back({
    .name = "MINOR_CONNECTIVITY_END_BONUS",
    .uciName = "",
    .description = "Minor connectivity bonus per connection (endgame)",
    .valueType = Int,
    .domain = Eval,
    .defaultValue = configToString(defaultEval.MINOR_CONNECTIVITY_END_BONUS),
    .exposure = {.uci = false, .yaml = true, .display = true, .tunable = true},
    .getter = evalGetter([](const auto& e){ return e.MINOR_CONNECTIVITY_END_BONUS; }),
    .setter = EVAL_CONFIG_SETTER(MINOR_CONNECTIVITY_END_BONUS, parseInt)
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
    .defaultValue = configToString(defaultEval.USE_GAMEPHASE_VALUE),
    .exposure = {.uci = IS_MUTABLE(defaultEval, USE_GAMEPHASE_VALUE), .yaml = true, .display = true},
    .getter = evalGetter([](const auto& e){ return e.USE_GAMEPHASE_VALUE; }),
    .setter = EVAL_CONFIG_SETTER(USE_GAMEPHASE_VALUE, parseBool)
  });

  // clang-format on
}
