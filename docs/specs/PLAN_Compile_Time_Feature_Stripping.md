# Compile-Time Feature Stripping Plan

**Status:** Planning  
**Created:** 2026-02-22  
**Last Updated:** 2026-02-23  
**Author:** Frank Kopp

---

## Project Metadata

| Field                | Value                                        |
|----------------------|----------------------------------------------|
| **Branch**           | `feature/compile-time-stripping` ✅ exists    |
| **Risk Level**       | 🔴 High - Major refactoring of config system |
| **Estimated Effort** | 3-5 sessions                                 |
| **Rollback**         | Git branch - abandon and return to main      |

### Phase Status Tracker

| Phase                                   | Status        | Notes                                        |
|-----------------------------------------|---------------|----------------------------------------------|
| Phase 1: X-Macro Infrastructure         | ⬜ Not Started | Major refactoring - create definitions files |
| Phase 1.5: Config Framework Integration | ⬜ Not Started | Add frozen field, verify essential configs   |
| Phase 2: Statistics Migration           | ⬜ Not Started | Convert ~100+ stat accesses to macros        |
| Phase 3: Feature Guard Migration        | ⬜ Not Started | Convert feature guards in Search.cpp         |
| Phase 4: Testing & Validation           | ⬜ Not Started | Verify both build modes work correctly       |
| Phase 5: Documentation                  | ⬜ Not Started | Update copilot-instructions, README          |

**Status Legend:** ⬜ Not Started | 🔄 In Progress | ✅ Complete | ❌ Blocked | ⏸️ Paused

### Session Log

| Date       | Session | Phase(s) | Outcome | Notes                                       |
|------------|---------|----------|---------|---------------------------------------------|
| 2026-02-22 | 1       | Planning | ✅       | Initial plan created                        |
| 2026-02-23 | 2       | Planning | ✅       | X-macro approach finalized, feature-grouped |
|            |         |          |         |                                             |

---

## Quick Start for New Session

1. **Verify on feature branch:** `git branch --show-current` → should show `feature/compile-time-stripping`
2. **Check phase status** in table above
3. **Find current phase section** below for detailed tasks
4. **After work:** Update phase status and session log above

---

## Overview

Implement compile-time macros to completely eliminate runtime overhead from:
1. **Feature guards** - `if (SearchConfig.USE_XXX)` checks
2. **Statistics collection** - `statistics.xxx++` increments

The goal is **true zero-cost removal** - no memory access, no branch prediction cost, no cache impact. The compiler should completely eliminate dead code in tournament/release builds.

---

## Motivation

FrankyCPP has numerous runtime feature toggles and extensive statistics collection. While invaluable for development and tuning, they add measurable overhead:

1. **Feature guards**: Each `if (SearchConfig.USE_LMR)` requires:
   - Memory load from config struct
   - Branch instruction
   - Branch prediction pressure

2. **Statistics increments**: Each `statistics.lmrReductions++` requires:
   - Memory load
   - Increment
   - Memory store
   - Cache line access

For tournament play, these features are always-on with fixed parameters. Removing them entirely at compile-time provides:
- Cleaner instruction cache
- Better branch prediction for remaining branches
- Reduced memory bandwidth
- Smaller binary size

---

## Current State Analysis

### Feature Guards (from SearchConfigData.h)

Major categories with `USE_*` flags:

| Category            | Flags                                                                                              | Count |
|---------------------|----------------------------------------------------------------------------------------------------|-------|
| Core Search         | `USE_ALPHABETA`, `USE_PVS`, `USE_ASP`                                                              | 3     |
| Quiescence          | `USE_QUIESCENCE`, `USE_QS_TT`, `USE_QS_STANDPAT_CUT`, `USE_QS_SEE`                                 | 4     |
| Transposition Table | `USE_TT`, `USE_TT_VALUE`, `USE_EVAL_TT`, `USE_TT_PV_MOVE_SORT`                                     | 4     |
| Tablebase           | `USE_TB_PROBE_ROOT`, `USE_TB_PROBE_SEARCH`, `USE_TB_PROBE_PV`                                      | 3     |
| Move Ordering       | `USE_KILLER_MOVES`, `USE_HISTORY_COUNTER`, `USE_HISTORY_MOVES`                                     | 3     |
| IID/IIR             | `USE_IID`, `USE_IIR`                                                                               | 2     |
| Pruning             | `USE_MDP`, `USE_RAZORING`, `USE_RFP`, `USE_NMP`, `USE_FP`, `USE_QFP`, `USE_LMP`                    | 7     |
| Extensions          | `USE_EXTENSIONS`, `USE_CHECK_EXT`, `USE_THREAT_EXT`, `USE_SINGULAR_EXT`, `USE_EXT_ADD_DEPTH`       | 5     |
| LMR                 | `USE_LMR`, `USE_LMR_IMPROVING`, `USE_LMR_HISTORY`, `USE_LMR_CUTNODE`                               | 4     |
| Improving           | `USE_IMPROVING`, `USE_RFP_IMPROVING`, `USE_NMP_IMPROVING`, `USE_FP_IMPROVING`, `USE_LMP_IMPROVING` | 5     |
| Time Mgmt           | `USE_BESTMOVE_INSTABILITY`                                                                         | 1     |
| Book/Ponder         | `USE_BOOK`, `USE_PONDER`                                                                           | 2     |

**Total: ~43 feature flags**

### Statistics (from SearchStats.h + Search.cpp)

Based on grep of `statistics.` usage in Search.cpp:

| Category      | Statistics Fields                                                                                                                                                                          | Approx Usage Count |
|---------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------|
| Node counts   | `pvNodes`, `nonPvNodes`, `searchNodes`, `qsearchNodes`                                                                                                                                     | 5                  |
| Terminal      | `checkmates`, `stalemates`, `leafPositionsEvaluated`, `evaluations`                                                                                                                        | 6                  |
| Pruning       | `mdp`, `standpatCuts`, `razorings`, `rfp_cuts`, `nullMoveCuts`, `fpPrunings`, `qfpPrunings`, `lmpCuts`                                                                                     | 10                 |
| Beta cuts     | `betaCuts`, `betaCutsByIndex[]`                                                                                                                                                            | 6                  |
| TT            | `ttHit`, `ttMiss`, `TtCuts`, `TtNoCuts`, `evalFromTT`, `NoTtMove`, `TtMoveUsed`                                                                                                            | 10                 |
| LMR           | `lmrReductions`, `lmrResearches`, `lmrHistoryLessReduction`, `lmrHistoryDepthSaved`, `lmrCutNodeReductions`                                                                                | 6                  |
| Extensions    | `checkExtension`, `threatExtension`, `singularSearches`, `singularFilteredByBound`, `singularExtension`                                                                                    | 6                  |
| IID/IIR       | `iidSearches`, `iidMoves`, `iirReductions`                                                                                                                                                 | 4                  |
| Improving     | `improvingTrue`, `improvingFalse`                                                                                                                                                          | 2                  |
| Tablebase     | `tbRootHits`, `tbSearchProbes`, `tbSearchHits`, `tbSearchMisses`, `tbSearchCutoffs`                                                                                                        | 6                  |
| Re-search     | `rootPvsResearches`, `pvsResearches`, `aspirationResearches`, `bestMoveChange`                                                                                                             | 5                  |
| Current state | `currentIterationDepth`, `currentSearchDepth`, `currentExtraSearchDepth`, `currentBestRootMove`, `currentBestRootMoveValue`, `currentVariation`, `currentRootMoveIndex`, `currentRootMove` | 15+                |
| Null-move     | `nullMoveVerifications`                                                                                                                                                                    | 2                  |
| Perft         | `perftNodeCount`                                                                                                                                                                           | 2                  |

**Total: ~100+ statistics field accesses**

---

## Design

### Industry Patterns Research

The problem of "development flexibility vs production performance" is common across many domains.
Here's how major projects handle similar challenges:

#### Pattern 1: Compile-Time Feature Flags (`#ifdef`)
**Used by:** Stockfish, Godot Engine, LLVM, Abseil

```cpp
// Stockfish types.h
#ifdef USE_POPCNT
constexpr bool HasPopCnt = true;
#else
constexpr bool HasPopCnt = false;
#endif

// Godot engine.h - complete function stripping
#ifdef TOOLS_ENABLED
_FORCE_INLINE_ bool is_editor_hint() const { return editor_hint; }
#else
_FORCE_INLINE_ bool is_editor_hint() const { return false; }  // Always false, compiler eliminates
#endif
```

**Pros:** Zero overhead, simple, widely understood  
**Cons:** Binary incompatibility, requires recompilation, no runtime toggle

#### Pattern 2: Metrics/Stats Macros
**Used by:** Chromium (UMA histograms), Facebook Velox, Google Benchmark

```cpp
// Chromium histogram_macros.h
#define UMA_HISTOGRAM_ENUMERATION(name, ...) \
  INTERNAL_UMA_HISTOGRAM_ENUMERATION_GET_MACRO(__VA_ARGS__, ...)

// In production builds, these expand to actual collection
// In tests or disabled builds, can be compiled out entirely
```

**Pros:** Centralized stats control, can be stripped in release  
**Cons:** Still requires macro discipline in code

#### Pattern 3: Flag Definition Macros (Similar to X-macros)
**Used by:** PyTorch/Caffe2 (C10_DEFINE_*), gflags

```cpp
// PyTorch c10/util/Flags.h
#define C10_DEFINE_bool(name, default_value, help_str) \
  C10_DEFINE_typed_var(bool, name, default_value, help_str)

// Usage - defines flag, default, help in ONE place
C10_DEFINE_bool(c10_use_mkldnn, true, "Use MKL-DNN for computations");
```

**Pros:** Single definition point, auto-generates registration  
**Cons:** Runtime overhead (flags checked at runtime, not compile-time)

#### Pattern 4: Constexpr Configuration with Templates
**Used by:** Abseil, modern game engines

```cpp
// Abseil-style feature detection
namespace absl {
#ifdef ABSL_HAVE_FEATURE_X
constexpr bool kHasFeatureX = true;
#else
constexpr bool kHasFeatureX = false;
#endif
}

// Usage - compiler eliminates dead branches
if constexpr (absl::kHasFeatureX) { ... }
```

**Pros:** Type-safe, optimizer-friendly, no macros in usage code  
**Cons:** Requires C++17, still needs `#ifdef` for definition

#### Pattern 5: Build Profiles with Separate Structs
**Used by:** LevelDB, RocksDB, game engines

```cpp
// LevelDB options.h - all config in one struct
struct Options {
  bool paranoid_checks = false;  // Debug only in production
  size_t write_buffer_size = 4 * 1024 * 1024;
  // ...
};
```

**Pros:** Clean API, all defaults in one place  
**Cons:** Runtime overhead (values always checked), no compile-time elimination

### Comparison: FrankyCPP X-Macro Approach vs Industry Patterns

| Aspect | FrankyCPP X-Macro | `#ifdef` Guards | Flag Macros | constexpr |
|--------|-------------------|-----------------|-------------|-----------|
| Single source of truth | ✅ One file | ❌ Multiple | ✅ One call | ❌ Multiple |
| Zero runtime overhead | ✅ Yes | ✅ Yes | ❌ No | ✅ Yes |
| Registry auto-generation | ✅ Yes | ❌ Manual | ✅ Yes | ❌ Manual |
| IDE autocomplete | ⚠️ Limited | ✅ Good | ✅ Good | ✅ Good |
| Error messages | ⚠️ Cryptic | ✅ Clear | ✅ Clear | ✅ Clear |
| Requires recompilation | ✅ Yes | ✅ Yes | ❌ No | ✅ Yes |

### Conclusion: X-Macros Are Viable

The X-macro approach is **not exotic** - it's a well-established C/C++ pattern used when:
1. You need a single source of truth for many similar definitions
2. The same data must generate different code in different contexts
3. Compile-time code elimination is required for performance

**Similar usage in industry:**
- Linux kernel: Uses X-macros extensively for syscall tables, error codes
- Chromium: Uses X-macros for IPC message definitions
- LLVM: Uses `.def` files (a variant of X-macros) for instruction definitions
- SQLite: Uses X-macros for opcode definitions

The tradeoff (macro complexity vs DRY/performance) is acceptable because:
1. The complexity is **isolated** to definition files
2. **Consuming code** uses clean macros like `FEATURE_ENABLED()`
3. The **alternative** (3 places to edit) is worse for maintenance
4. **C++26 reflection** will eventually allow cleaner syntax

---

### X-Macro Approach: Single Source of Truth

All configuration is defined **once** in `SearchConfigDefinitions.h` (and `EvalConfigDefinitions.h`).
From these definitions, we auto-generate:
- Struct members (`SearchConfigData.h`)
- Constexpr defaults (`ConfigDefaults.h`)
- Registry entries (`ConfigRegistry.cpp`)

**Tradeoffs:**

| Pros                    | Cons                                |
|-------------------------|-------------------------------------|
| Single source of truth  | X-macros are "ugly" (macro soup)    |
| Add config in ONE place | IDE autocomplete/navigation suffers |
| Zero runtime overhead   | Cryptic compiler error messages     |
| No mismatch risk        | Harder to debug macro expansions    |
| Feature-grouped layout  | Learning curve for contributors     |

The ugliness is isolated to the definition files. Consuming code (`Search.cpp`) uses clean,
readable macros like `FEATURE_ENABLED()` and `CONFIG_PARAM()`.

### Three-Tier Macro System

```
┌─────────────────────────────────────────────────────────────────┐
│                     FRANKYCPP_MINIMAL_BUILD                      │
│  (Master switch - CMake option for tournament builds)            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌───────────────────┐  ┌───────────────────┐  ┌──────────────┐ │
│  │  Feature Guards   │  │  Optional Stats   │  │ Debug Code   │ │
│  │  FEATURE_ENABLED  │  │  STAT_INC/ADD/MAX │  │ DEBUG_ONLY   │ │
│  │  CONFIG_PARAM     │  │  STAT_TIMER_*     │  │ DEBUG_LOG    │ │
│  └───────────────────┘  └───────────────────┘  └──────────────┘ │
│                                                                  │
│  Defaults from X-macros in SearchConfigDefinitions.h            │
│                                                                  │
├─────────────────────────────────────────────────────────────────┤
│                    ESSENTIAL (Always Active)                     │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  ESSENTIAL_STAT_INC/ADD - nodesVisited, time, depth       │  │
│  │  (Required for UCI info, NPS, time management)            │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### Header File: `src/config/FeatureFlags.h`

```cpp
#ifndef FRANKYCPP_FEATURE_FLAGS_H
#define FRANKYCPP_FEATURE_FLAGS_H

//=============================================================================
// FeatureFlags.h - Compile-Time Feature and Statistics Control
//=============================================================================
//
// Define FRANKYCPP_MINIMAL_BUILD via CMake for tournament/release builds.
// This completely eliminates feature guards and non-essential statistics
// at compile time - zero runtime overhead.
//
// Usage:
//   cmake -DFRANKYCPP_MINIMAL_BUILD=ON ...
//
// Categories:
//   - FEATURE_ENABLED/DISABLED: Boolean USE_* flag guards (compile-time true in minimal)
//   - CONFIG_PARAM: Feature parameters (compile-time from ConfigDefaults.h in minimal)
//   - STAT_*: Optional statistics collection (disabled in minimal)
//   - ESSENTIAL_STAT_*: Always-collected stats (nodes, time, depth)
//   - DEBUG_*: Debug-only code and logging
//   - EXPOSURE_MINIMAL/ESSENTIAL: Config registry exposure control
//
//=============================================================================

#include <algorithm>
#include <chrono>

// ============================================================================
// FEATURE GUARDS - Control runtime feature toggles (boolean USE_* flags only)
// ============================================================================

#ifdef FRANKYCPP_MINIMAL_BUILD

  // Boolean feature guards become compile-time true - compiler eliminates branches
  #define FEATURE_ENABLED(condition) (true)
  #define FEATURE_DISABLED(condition) (false)

#else

  #define FEATURE_ENABLED(condition) (condition)
  #define FEATURE_DISABLED(condition) (!(condition))

#endif

// ============================================================================
// FEATURE PARAMETERS - Numeric config values (compile-time in minimal builds)
// ============================================================================
//
// In minimal builds, feature parameters become compile-time constants.
// The default values are defined ONCE in ConfigDefaults.h (generated or manual),
// which is the single source of truth used by both:
//   - ConfigRegistry (for runtime initialization in normal builds)
//   - CONFIG_PARAM macro (for compile-time constants in minimal builds)
//
// Usage in Search.cpp:
//   depth >= CONFIG_PARAM(LMR_MIN_DEPTH)
//
// Expands to:
//   Normal build:  depth >= SearchConfig.LMR_MIN_DEPTH  (runtime)
//   Minimal build: depth >= config::defaults::LMR_MIN_DEPTH  (compile-time constexpr)

#ifdef FRANKYCPP_MINIMAL_BUILD
  // Minimal build: use compile-time constant from ConfigDefaults.h
  #define CONFIG_PARAM(name) (config::defaults::name)
#else
  // Normal build: use runtime value from SearchConfig
  #define CONFIG_PARAM(name) (SearchConfig.name)
#endif

// ============================================================================
// STATISTICS COLLECTION - Non-essential statistics (disabled in minimal)
// ============================================================================

#ifdef FRANKYCPP_MINIMAL_BUILD

  #define STAT_INC(counter) ((void)0)
  #define STAT_ADD(counter, val) ((void)0)
  #define STAT_MAX(counter, val) ((void)0)
  #define STAT_SET(counter, val) ((void)0)
  #define STAT_TIMER_START(var) ((void)0)
  #define STAT_TIMER_END(counter, var) ((void)0)

#else

  #define STAT_INC(counter) (++(counter))
  #define STAT_ADD(counter, val) ((counter) += (val))
  #define STAT_MAX(counter, val) ((counter) = std::max((counter), (val)))
  #define STAT_SET(counter, val) ((counter) = (val))
  #define STAT_TIMER_START(var) \
    const auto var = std::chrono::high_resolution_clock::now()
  #define STAT_TIMER_END(counter, var) \
    ((counter) += std::chrono::duration_cast<std::chrono::nanoseconds>( \
      std::chrono::high_resolution_clock::now() - (var)).count())

#endif

// ============================================================================
// ESSENTIAL STATS - Always collected (nodes, time, depth for UCI/NPS)
// ============================================================================

#define ESSENTIAL_STAT_INC(counter) (++(counter))
#define ESSENTIAL_STAT_ADD(counter, val) ((counter) += (val))
#define ESSENTIAL_STAT_SET(counter, val) ((counter) = (val))
#define ESSENTIAL_STAT_MAX(counter, val) ((counter) = std::max((counter), (val)))

// ============================================================================
// DEBUG-ONLY CODE - Completely removed in minimal builds
// ============================================================================

#ifdef FRANKYCPP_MINIMAL_BUILD

  #define DEBUG_ONLY(code) ((void)0)
  #define DEBUG_LOG(...) ((void)0)

#else

  #define DEBUG_ONLY(code) do { code; } while(0)
  // Note: DEBUG_LOG requires Logging.h to be included before use
  #define DEBUG_LOG(...) LOG__DEBUG(__VA_ARGS__)

#endif

// ============================================================================
// CONFIG REGISTRY HELPERS - Control option exposure in minimal builds
// ============================================================================
// Use these macros in ConfigRegistry.cpp to control which options are
// exposed via UCI and YAML in minimal builds.
//
// EXPOSURE_MINIMAL: Option is frozen in minimal builds (not exposed)
// EXPOSURE_ESSENTIAL: Option is always exposed (never frozen)
// FROZEN_IN_MINIMAL: Boolean flag for ConfigDef.frozen field

#ifdef FRANKYCPP_MINIMAL_BUILD
  // In minimal builds, feature options are frozen (compile-time constants)
  #define FROZEN_IN_MINIMAL true
  // Frozen options: uci=false, yaml=false, display still shows with [FROZEN] marker
  #define EXPOSURE_MINIMAL(uci, yaml, disp) {.uci = false, .yaml = false, .display = (disp)}
#else
  #define FROZEN_IN_MINIMAL false
  // Normal builds: use the specified exposure settings
  #define EXPOSURE_MINIMAL(uci, yaml, disp) {.uci = (uci), .yaml = (yaml), .display = (disp)}
#endif

// Essential options - never frozen, always configurable at runtime
#define EXPOSURE_ESSENTIAL(uci, yaml, disp) {.uci = (uci), .yaml = (yaml), .display = (disp)}

#endif // FRANKYCPP_FEATURE_FLAGS_H
```

### Single Source of Truth: X-Macro Pattern

Instead of defining configs in 3 places (struct, registry, defaults), we use **X-macros** to define
each feature **once** and generate all required code from that single definition.

**Key Files:**
- `SearchConfigDefinitions.h` - Single source of truth for all search configs
- `EvalConfigDefinitions.h` - Single source of truth for all eval configs
- `SearchConfigData.h` - Struct generated from definitions
- `ConfigDefaults.h` - Constexpr defaults generated from definitions  
- `ConfigRegistry.cpp` - Registry entries generated from definitions

### SearchConfigDefinitions.h (Single Source of Truth)

```cpp
#ifndef FRANKYCPP_SEARCH_CONFIG_DEFINITIONS_H
#define FRANKYCPP_SEARCH_CONFIG_DEFINITIONS_H

//=============================================================================
// SearchConfigDefinitions.h - SINGLE SOURCE OF TRUTH for Search Configuration
//=============================================================================
//
// This file defines ALL search configuration using X-macros.
// Each feature is grouped with its flag and related parameters together.
//
// From this single file, we generate:
//   - SearchConfigData struct members (SearchConfigData.h)
//   - ConfigDefaults constexpr values (ConfigDefaults.h)
//   - ConfigRegistry entries (ConfigRegistry.cpp)
//
// Macro formats:
//   X_BOOL(name, default, description, uciName, uci, yaml, display, essential)
//   X_INT(name, default, description, uciName, min, max, uci, yaml, display, essential)
//   X_DOUBLE(name, default, description, uciName, min, max, uci, yaml, display, essential)
//   X_STRING(name, default, description, uciName, uci, yaml, display, essential)
//
// Parameters:
//   name        - C++ identifier (becomes struct member and config name)
//   default     - Default value
//   description - Human-readable description
//   uciName     - UCI option name (empty string = not exposed via UCI)
//   min/max     - Value range for numeric types
//   uci         - Expose via UCI protocol
//   yaml        - Load from YAML config file
//   display     - Show in --show-config output
//   essential   - true = never frozen (always runtime), false = frozen in minimal builds
//
//=============================================================================

//=============================================================================
// ESSENTIAL SETTINGS (never frozen, always runtime-configurable)
//=============================================================================
#define SEARCH_CONFIG_ESSENTIAL(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_INT(TT_SIZE_MB,           64,    "Transposition table size (MB)",     "Hash",          1, 65536, true, true, true, true) \
  X_INT(MOVE_OVERHEAD_MS,     10,    "Safety margin for time management", "Move Overhead", 0, 5000,  true, true, true, true) \
  X_BOOL(USE_PONDER,          true,  "Enable pondering",                  "Ponder",        true, true, true, true) \
  X_BOOL(USE_PAWN_TT,         true,  "Enable pawn hash table",            "Use Pawn TT",   true, true, true, true) \
  X_INT(PAWN_TT_SIZE_MB,      4,     "Pawn hash table size (MB)",         "Pawn TT Size",  1, 128,   true, true, true, true)

//=============================================================================
// BOOK SETTINGS (essential - user preference, not search algorithm)
//=============================================================================
#define SEARCH_CONFIG_BOOK(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_BOOK,            true,  "Use internal opening book",         "OwnBook",       true, true, true, true) \
  X_STRING(BOOK_PATH,         "./books/book.txt", "Path to opening book", "Book Path",     true, true, true, true) \
  X_STRING(BOOK_TYPE,         "SIMPLE", "Book format (SIMPLE or PGN)",    "Book Type",     true, true, true, true)

//=============================================================================
// TABLEBASE SETTINGS (essential - path is user config)
//=============================================================================
#define SEARCH_CONFIG_TABLEBASE(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_STRING(TB_PATH,           "",    "Path to Syzygy tablebase files",    "SyzygyPath",    true, true, true, true) \
  X_BOOL(USE_TB_PROBE_ROOT,   true,  "Probe TB at root for best move",    "Syzygy Probe Root",   true, true, true, false) \
  X_BOOL(TB_ROOT_IMMEDIATE,   false, "Return TB move without searching",  "Syzygy Root Immediate", true, true, true, false) \
  X_BOOL(USE_TB_PROBE_SEARCH, true,  "Probe TB during search for cutoffs","Syzygy Probe Search", true, true, true, false) \
  X_INT(TB_PROBE_DEPTH,       1,     "Min depth for TB probe in search",  "Syzygy Probe Depth",  0, 20, true, true, true, false) \
  X_INT(TB_PROBE_LIMIT,       6,     "Max pieces for search TB probing",  "Syzygy Probe Limit",  3, 7,  true, true, true, false)

//=============================================================================
// CORE SEARCH (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_CORE(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_ALPHABETA,       true,  "Use alpha-beta search",             "",              false, true, true, false) \
  X_BOOL(USE_PVS,             true,  "Use Principal Variation Search",    "Use PVS",       true, true, true, false) \
  X_BOOL(USE_ASP,             true,  "Use aspiration windows",            "Use Aspiration",true, true, true, false) \
  X_BOOL(USE_QUIESCENCE,      true,  "Use quiescence search",             "",              false, true, true, false)

//=============================================================================
// TRANSPOSITION TABLE (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_TT(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_TT,              true,  "Use transposition table",           "Use TT",        true, true, true, false) \
  X_BOOL(USE_TT_VALUE,        true,  "Use TT value for cutoffs",          "Use TT Value",  true, true, true, false) \
  X_BOOL(USE_EVAL_TT,         true,  "Use eval TT for static eval",       "Use Eval TT",   true, true, true, false) \
  X_BOOL(USE_QS_TT,           true,  "Use TT in quiescence search",       "Use QS TT",     true, true, true, false) \
  X_BOOL(USE_TT_PV_MOVE_SORT, true,  "Sort TT/PV move first",             "",              false, true, true, false)

//=============================================================================
// MOVE ORDERING (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_MOVE_ORDER(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_KILLER_MOVES,    true,  "Use killer move heuristic",         "Use Killers",   true, true, true, false) \
  X_BOOL(USE_HISTORY_COUNTER, true,  "Use counter-move history",          "Use Counter History", true, true, true, false) \
  X_BOOL(USE_HISTORY_MOVES,   true,  "Use history heuristic",             "Use History",   true, true, true, false)

//=============================================================================
// IID / IIR (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_IID_IIR(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_IID,             false, "Use Internal Iterative Deepening",  "Use IID",       true, true, true, false) \
  X_INT(IID_DEPTH,            6,     "IID minimum depth",                 "IID Depth",     1, 20, true, true, true, false) \
  X_INT(IID_REDUCTION,        2,     "IID depth reduction",               "IID Reduction", 1, 6,  true, true, true, false) \
  X_BOOL(USE_IIR,             true,  "Use Internal Iterative Reduction",  "Use IIR",       true, true, true, false) \
  X_INT(IIR_DEPTH,            4,     "IIR minimum depth",                 "IIR Depth",     1, 20, true, true, true, false) \
  X_INT(IIR_REDUCTION,        2,     "IIR depth reduction",               "IIR Reduction", 1, 6,  true, true, true, false) \
  X_BOOL(IIR_ALL_NODES,       true,  "Apply IIR to all node types",       "IIR All Nodes", true, true, true, false)

//=============================================================================
// LMR - Late Move Reductions (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_LMR(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_LMR,             true,  "Enable Late Move Reductions",       "Use LMR",       true, true, true, false) \
  X_INT(LMR_MIN_DEPTH,        2,     "LMR minimum depth",                 "LMR Min Depth", 1, 10, true, true, true, false) \
  X_INT(LMR_MIN_MOVES,        2,     "LMR minimum moves searched",        "LMR Min Moves", 1, 10, true, true, true, false) \
  X_BOOL(LMR_USE_LOG_FORMULA, true,  "Use log formula for LMR",           "LMR Log Formula", true, true, true, false) \
  X_DOUBLE(LMR_LOG_BASE_DIV,  1.25,  "LMR log formula divisor",           "LMR Log Divisor", 0.5, 5.0, true, true, true, false) \
  X_BOOL(USE_LMR_IMPROVING,   true,  "Extra LMR when not improving",      "LMR Improving", true, true, true, false) \
  X_INT(LMR_IMPROVING_REDUCTION, 1,  "Extra reduction when not improving","LMR Impr Reduction", 0, 4, true, true, true, false) \
  X_BOOL(USE_LMR_HISTORY,     true,  "Adjust LMR by history score",       "LMR History",   true, true, true, false) \
  X_INT(LMR_HISTORY_DIVISOR,  8192,  "History to reduction divisor",      "LMR Hist Divisor", 1024, 32768, true, true, true, false) \
  X_BOOL(USE_LMR_CUTNODE,     true,  "Extra LMR on cut nodes",            "LMR CutNode",   true, true, true, false) \
  X_INT(LMR_CUTNODE_REDUCTION, 2,    "Extra reduction on cut nodes",      "LMR CutNode Reduction", 0, 4, true, true, true, false)

//=============================================================================
// LMP - Late Move Pruning (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_LMP(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_LMP,             true,  "Enable Late Move Pruning",          "Use LMP",       true, true, true, false) \
  X_BOOL(USE_LMP_IMPROVING,   true,  "More moves when improving",         "LMP Improving", true, true, true, false)

//=============================================================================
// NULL MOVE PRUNING (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_NMP(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_NMP,             true,  "Enable Null Move Pruning",          "Use NMP",       true, true, true, false) \
  X_INT(NMP_DEPTH,            3,     "NMP minimum depth",                 "NMP Depth",     1, 10, true, true, true, false) \
  X_INT(NMP_REDUCTION,        2,     "NMP base reduction",                "NMP Reduction", 1, 6,  true, true, true, false) \
  X_BOOL(USE_NMP_VERIFY,      true,  "Verify null move with re-search",   "NMP Verify",    true, true, true, false) \
  X_INT(NMP_VERIFY_MIN_DEPTH, 6,     "Min depth for NMP verification",    "NMP Verify Depth", 1, 15, true, true, true, false) \
  X_INT(NMP_VERIFY_MARGIN,    2,     "NMP verification depth margin",     "NMP Verify Margin", 0, 6, true, true, true, false) \
  X_BOOL(USE_NMP_IMPROVING,   true,  "Extra NMP when not improving",      "NMP Improving", true, true, true, false) \
  X_INT(NMP_IMPROVING_REDUCTION, 1,  "Extra reduction when not improving","NMP Impr Reduction", 0, 4, true, true, true, false)

//=============================================================================
// FUTILITY PRUNING (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_FUTILITY(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_FP,              true,  "Enable Futility Pruning",           "Use FP",        true, true, true, false) \
  X_BOOL(USE_QFP,             true,  "Enable QS Futility Pruning",        "Use QFP",       true, true, true, false) \
  X_BOOL(USE_FP_IMPROVING,    true,  "Tighter FP when improving",         "FP Improving",  true, true, true, false) \
  X_INT(FP_IMPROVING_MARGIN,  80,    "FP margin increase when not impr",  "FP Impr Margin", 0, 200, true, true, true, false)

//=============================================================================
// RAZORING (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_RAZORING(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_RAZORING,        true,  "Enable Razoring",                   "Use Razoring",  true, true, true, false) \
  X_INT(RAZOR_MARGIN,         531,   "Razoring margin (centipawns)",      "Razor Margin",  100, 800, true, true, true, false)

//=============================================================================
// REVERSE FUTILITY PRUNING (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_RFP(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_RFP,             true,  "Enable Reverse Futility Pruning",   "Use RFP",       true, true, true, false) \
  X_BOOL(USE_RFP_IMPROVING,   true,  "Less RFP when improving",           "RFP Improving", true, true, true, false) \
  X_INT(RFP_IMPROVING_MARGIN, 40,    "RFP margin increase when not impr", "RFP Impr Margin", 0, 200, true, true, true, false)

//=============================================================================
// OTHER PRUNING (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_PRUNING(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_MDP,             true,  "Enable Mate Distance Pruning",      "Use MDP",       true, true, true, false) \
  X_BOOL(USE_QS_STANDPAT_CUT, true,  "Use stand-pat cutoff in QS",        "",              false, true, true, false) \
  X_BOOL(USE_QS_SEE,          true,  "Use SEE pruning in QS",             "",              false, true, true, false)

//=============================================================================
// IMPROVING FLAG (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_IMPROVING(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_IMPROVING,       true,  "Track improving eval flag",         "Use Improving", true, true, true, false)

//=============================================================================
// EXTENSIONS (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_EXTENSIONS(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_EXTENSIONS,      true,  "Enable search extensions",          "Use Extensions",true, true, true, false) \
  X_BOOL(USE_CHECK_EXT,       true,  "Extend when in check",              "Check Ext",     true, true, true, false) \
  X_INT(CHECK_EXT_MIN_DEPTH,  2,     "Min depth for check extension",     "Check Ext Depth", 0, 10, true, true, true, false) \
  X_INT(CHECK_EXT_EARLY_LIMIT,99,    "Early check ext limit",             "Check Ext Limit", 0, 99, true, true, true, false) \
  X_BOOL(USE_CHECK_EXT_SEE,   true,  "Only extend checks with SEE >= 0",  "Check Ext SEE", true, true, true, false) \
  X_BOOL(USE_THREAT_EXT,      true,  "Extend on mate threats",            "Threat Ext",    true, true, true, false) \
  X_INT(THREAT_EXT_MATE_DEPTH, 4,    "Mate depth for threat extension",   "Threat Ext Depth", 1, 10, true, true, true, false) \
  X_BOOL(USE_EXT_ADD_DEPTH,   true,  "Allow additive extensions",         "Ext Add Depth", true, true, true, false)

//=============================================================================
// SINGULAR EXTENSIONS (frozen in minimal builds)
//=============================================================================
#define SEARCH_CONFIG_SINGULAR(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_SINGULAR_EXT,    true,  "Enable singular extensions",        "Use Singular Ext", true, true, true, false) \
  X_BOOL(USE_SINGULAR_TT_BOUND, false, "Require beta/exact TT bound",     "Singular TT Bound", true, true, true, false) \
  X_INT(SINGULAR_MARGIN,      64,    "Margin below TT value (cp)",        "Singular Margin", 0, 200, true, true, true, false) \
  X_INT(SINGULAR_MIN_DEPTH,   8,     "Min depth for singular search",     "Singular Depth", 4, 15, true, true, true, false) \
  X_INT(SINGULAR_REDUCTION,   4,     "Verification search reduction",     "Singular Reduction", 1, 8, true, true, true, false)

//=============================================================================
// TIME MANAGEMENT (some frozen, some essential)
//=============================================================================
#define SEARCH_CONFIG_TIME(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  X_BOOL(USE_BESTMOVE_INSTABILITY, true, "Extend time on best move changes", "BM Instability", true, true, true, false) \
  X_INT(MOVES_LEFT_OPENING,   36,    "Expected moves in opening",         "",              false, true, true, false) \
  X_INT(MOVES_LEFT_MIDGAME,   28,    "Expected moves in midgame",         "",              false, true, true, false) \
  X_INT(MOVES_LEFT_ENDGAME,   16,    "Expected moves in endgame",         "",              false, true, true, false)

//=============================================================================
// MASTER MACRO - Expands ALL feature groups (order determines struct layout)
//=============================================================================
#define SEARCH_CONFIG_ALL(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_ESSENTIAL(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_BOOK(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_TABLEBASE(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_CORE(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_TT(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_MOVE_ORDER(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_IID_IIR(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_LMR(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_LMP(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_NMP(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_FUTILITY(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_RAZORING(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_RFP(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_PRUNING(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_IMPROVING(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_EXTENSIONS(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_SINGULAR(X_BOOL, X_INT, X_DOUBLE, X_STRING) \
  SEARCH_CONFIG_TIME(X_BOOL, X_INT, X_DOUBLE, X_STRING)

#endif // FRANKYCPP_SEARCH_CONFIG_DEFINITIONS_H
```

### SearchConfigData.h (Generated from Definitions)

```cpp
#ifndef FRANKYCPP_SEARCHCONFIGDATA_H
#define FRANKYCPP_SEARCHCONFIGDATA_H

#include "SearchConfigDefinitions.h"
#include <string>

struct SearchConfigData {
  // Debug / internal (not in X-macro, rarely changes)
  std::string CONFIG_SOURCE = "fallback";

  // =========================================================================
  // Auto-generated struct members from SearchConfigDefinitions.h
  // =========================================================================
  #define X_BOOL(name, def, desc, uciName, uci, yaml, disp, essential)   bool name = def;
  #define X_INT(name, def, desc, uciName, min, max, uci, yaml, disp, essential)   int name = def;
  #define X_DOUBLE(name, def, desc, uciName, min, max, uci, yaml, disp, essential) double name = def;
  #define X_STRING(name, def, desc, uciName, uci, yaml, disp, essential) std::string name = def;

  SEARCH_CONFIG_ALL(X_BOOL, X_INT, X_DOUBLE, X_STRING)

  #undef X_BOOL
  #undef X_INT
  #undef X_DOUBLE
  #undef X_STRING

  // =========================================================================
  // Non-generated members (arrays, special types)
  // =========================================================================
  std::array<int, 4> RFP_MARGIN{0, 200, 400, 800};
  std::array<int, 7> FP_MARGIN{0, 100, 200, 300, 500, 900, 1200};
  std::array<int, 16> LMP_MOVES{0, 7, 9, 11, 13, 15, 17, 19, 22, 24, 27, 29, 32, 35, 38, 41};
};

#endif // FRANKYCPP_SEARCHCONFIGDATA_H
```

### ConfigDefaults.h (Generated from Definitions)

```cpp
#ifndef FRANKYCPP_CONFIG_DEFAULTS_H
#define FRANKYCPP_CONFIG_DEFAULTS_H

#include "SearchConfigDefinitions.h"
// #include "EvalConfigDefinitions.h"  // Similar pattern for eval

#include <string_view>

namespace config::defaults {

// =========================================================================
// Auto-generated constexpr defaults from SearchConfigDefinitions.h
// Used by CONFIG_PARAM() macro in minimal builds
// =========================================================================
#define X_BOOL(name, def, desc, uciName, uci, yaml, disp, essential)   inline constexpr bool name = def;
#define X_INT(name, def, desc, uciName, min, max, uci, yaml, disp, essential)   inline constexpr int name = def;
#define X_DOUBLE(name, def, desc, uciName, min, max, uci, yaml, disp, essential) inline constexpr double name = def;
#define X_STRING(name, def, desc, uciName, uci, yaml, disp, essential) inline constexpr std::string_view name = def;

SEARCH_CONFIG_ALL(X_BOOL, X_INT, X_DOUBLE, X_STRING)

#undef X_BOOL
#undef X_INT
#undef X_DOUBLE
#undef X_STRING

}  // namespace config::defaults

#endif // FRANKYCPP_CONFIG_DEFAULTS_H
```

### ConfigRegistry.cpp (Generated from Definitions)

```cpp
#include "ConfigRegistry.h"
#include "SearchConfigDefinitions.h"
#include "FeatureFlags.h"

void ConfigRegistry::initializeSearchDefinitions() {
  using enum ConfigValueType;
  using enum ConfigDomain;

  // =========================================================================
  // Auto-generated registry entries from SearchConfigDefinitions.h
  // =========================================================================

  #define X_BOOL(name, def, desc, uciName, uci, yaml, disp, essential) \
    definitions_.push_back({ \
      .name = #name, \
      .uciName = uciName, \
      .description = desc, \
      .valueType = Bool, \
      .domain = Search, \
      .defaultValue = def ? "true" : "false", \
      .frozen = essential ? false : FROZEN_IN_MINIMAL, \
      .exposure = essential ? ConfigExposure{uci, yaml, disp} \
                            : EXPOSURE_MINIMAL(uci, yaml, disp), \
      .getter = [](const SearchConfigData& s, const EvalConfigData&) { \
        return configToString(s.name); \
      }, \
      .setter = [](SearchConfigData& s, EvalConfigData&, const std::string& v) { \
        s.name = parseBool(v); \
      } \
    });

  #define X_INT(name, def, desc, uciName, min, max, uci, yaml, disp, essential) \
    definitions_.push_back({ \
      .name = #name, \
      .uciName = uciName, \
      .description = desc, \
      .valueType = Int, \
      .domain = Search, \
      .defaultValue = std::to_string(def), \
      .minValue = min, \
      .maxValue = max, \
      .frozen = essential ? false : FROZEN_IN_MINIMAL, \
      .exposure = essential ? ConfigExposure{uci, yaml, disp} \
                            : EXPOSURE_MINIMAL(uci, yaml, disp), \
      .getter = [](const SearchConfigData& s, const EvalConfigData&) { \
        return configToString(s.name); \
      }, \
      .setter = [](SearchConfigData& s, EvalConfigData&, const std::string& v) { \
        s.name = parseInt(v); \
      } \
    });

  #define X_DOUBLE(name, def, desc, uciName, min, max, uci, yaml, disp, essential) \
    definitions_.push_back({ \
      .name = #name, \
      .uciName = uciName, \
      .description = desc, \
      .valueType = Double, \
      .domain = Search, \
      .defaultValue = std::to_string(def), \
      .minValue = static_cast<int>(min * 100), \
      .maxValue = static_cast<int>(max * 100), \
      .frozen = essential ? false : FROZEN_IN_MINIMAL, \
      .exposure = essential ? ConfigExposure{uci, yaml, disp} \
                            : EXPOSURE_MINIMAL(uci, yaml, disp), \
      .getter = [](const SearchConfigData& s, const EvalConfigData&) { \
        return configToString(s.name); \
      }, \
      .setter = [](SearchConfigData& s, EvalConfigData&, const std::string& v) { \
        s.name = parseDouble(v); \
      } \
    });

  #define X_STRING(name, def, desc, uciName, uci, yaml, disp, essential) \
    definitions_.push_back({ \
      .name = #name, \
      .uciName = uciName, \
      .description = desc, \
      .valueType = String, \
      .domain = Search, \
      .defaultValue = def, \
      .frozen = essential ? false : FROZEN_IN_MINIMAL, \
      .exposure = essential ? ConfigExposure{uci, yaml, disp} \
                            : EXPOSURE_MINIMAL(uci, yaml, disp), \
      .getter = [](const SearchConfigData& s, const EvalConfigData&) { \
        return s.name; \
      }, \
      .setter = [](SearchConfigData& s, EvalConfigData&, const std::string& v) { \
        s.name = v; \
      } \
    });

  SEARCH_CONFIG_ALL(X_BOOL, X_INT, X_DOUBLE, X_STRING)

  #undef X_BOOL
  #undef X_INT
  #undef X_DOUBLE
  #undef X_STRING

  // =========================================================================
  // Non-generated entries (arrays, special handling)
  // =========================================================================
  // ... RFP_MARGIN, FP_MARGIN, LMP_MOVES handled separately ...
}
```

### CMake Integration

Add to `CMakeLists.txt`:

```cmake
# ============================================================================
# Build Type Options
# ============================================================================

option(FRANKYCPP_MINIMAL_BUILD 
  "Strip feature guards and non-essential stats for tournament builds" OFF)

if(FRANKYCPP_MINIMAL_BUILD)
  target_compile_definitions(FrankyCPP_lib PRIVATE FRANKYCPP_MINIMAL_BUILD)
  target_compile_definitions(FrankyCPP_exe PRIVATE FRANKYCPP_MINIMAL_BUILD)
  message(STATUS "")
  message(STATUS "╔══════════════════════════════════════════════════════════════╗")
  message(STATUS "║  MINIMAL BUILD: Feature guards and stats DISABLED            ║")
  message(STATUS "║  Only essential stats (nodes, depth, time) are collected     ║")
  message(STATUS "╚══════════════════════════════════════════════════════════════╝")
  message(STATUS "")
endif()
```

Add CMake preset for tournament builds:

```json
{
  "name": "win-release-tournament",
  "displayName": "Windows Release Tournament",
  "inherits": "win-release",
  "cacheVariables": {
    "FRANKYCPP_MINIMAL_BUILD": "ON"
  }
}
```

---

## Implementation Plan

> **Note:** Feature branch already exists. Rollback is always possible via git.

---

### Phase 1: X-Macro Infrastructure ✦ BREAKING - replaces config definition approach

**Status:** ⬜ Not Started  
**Estimated Effort:** 1-2 sessions  
**Risk:** 🔴 High - Core config system changes  

#### Prerequisites
- [ ] On branch `feature/compile-time-stripping`
- [ ] All tests passing on current main
- [ ] Clean working directory (`git status`)

#### Tasks

**1.1 Create SearchConfigDefinitions.h**
- [ ] Create `src/config/SearchConfigDefinitions.h`
- [ ] Define X-macro format: `X_BOOL(name, def, desc, uciName, uci, yaml, disp, essential)`
- [ ] Define X-macro format: `X_INT(name, def, desc, uciName, min, max, uci, yaml, disp, essential)`
- [ ] Define X-macro format: `X_DOUBLE(name, def, desc, uciName, min, max, uci, yaml, disp, essential)`
- [ ] Define X-macro format: `X_STRING(name, def, desc, uciName, uci, yaml, disp, essential)`
- [ ] Migrate all search configs from current `SearchConfigData.h`, grouped by feature:
  - [ ] SEARCH_CONFIG_ESSENTIAL (TT_SIZE_MB, MOVE_OVERHEAD_MS, USE_PONDER, etc.)
  - [ ] SEARCH_CONFIG_BOOK (USE_BOOK, BOOK_PATH, BOOK_TYPE)
  - [ ] SEARCH_CONFIG_TABLEBASE (TB_PATH, USE_TB_PROBE_*, etc.)
  - [ ] SEARCH_CONFIG_CORE (USE_ALPHABETA, USE_PVS, USE_ASP, etc.)
  - [ ] SEARCH_CONFIG_TT (USE_TT, USE_TT_VALUE, USE_EVAL_TT, etc.)
  - [ ] SEARCH_CONFIG_MOVE_ORDER (USE_KILLER_MOVES, USE_HISTORY_*, etc.)
  - [ ] SEARCH_CONFIG_IID_IIR (USE_IID, USE_IIR, IID_*, IIR_*)
  - [ ] SEARCH_CONFIG_LMR (USE_LMR, LMR_*, USE_LMR_*)
  - [ ] SEARCH_CONFIG_LMP (USE_LMP, USE_LMP_IMPROVING)
  - [ ] SEARCH_CONFIG_NMP (USE_NMP, NMP_*, USE_NMP_*)
  - [ ] SEARCH_CONFIG_FUTILITY (USE_FP, USE_QFP, FP_*, USE_FP_*)
  - [ ] SEARCH_CONFIG_RAZORING (USE_RAZORING, RAZOR_*)
  - [ ] SEARCH_CONFIG_RFP (USE_RFP, USE_RFP_IMPROVING, RFP_*)
  - [ ] SEARCH_CONFIG_PRUNING (USE_MDP, USE_QS_STANDPAT_CUT, USE_QS_SEE)
  - [ ] SEARCH_CONFIG_IMPROVING (USE_IMPROVING)
  - [ ] SEARCH_CONFIG_EXTENSIONS (USE_EXTENSIONS, USE_CHECK_EXT, etc.)
  - [ ] SEARCH_CONFIG_SINGULAR (USE_SINGULAR_EXT, SINGULAR_*)
  - [ ] SEARCH_CONFIG_TIME (USE_BESTMOVE_INSTABILITY, MOVES_LEFT_*)
- [ ] Define SEARCH_CONFIG_ALL master macro combining all groups

**1.2 Create EvalConfigDefinitions.h**
- [ ] Create `src/config/EvalConfigDefinitions.h`
- [ ] Define X-macro format: `X_BOOL(name, def, desc, uciName, uci, yaml, disp, essential)`
- [ ] Define X-macro format: `X_INT(name, def, desc, uciName, min, max, uci, yaml, disp, essential)`
- [ ] Define X-macro format: `X_DOUBLE(name, def, desc, uciName, min, max, uci, yaml, disp, essential)`
- [ ] Define X-macro format: `X_STRING(name, def, desc, uciName, uci, yaml, disp, essential)`
- [ ] Migrate all eval configs from current `EvalConfigData.h`
- [ ] Define EVAL_CONFIG_ALL master macro

**1.3 Create FeatureFlags.h**
- [ ] Create `src/config/FeatureFlags.h`
- [ ] Define FEATURE_ENABLED / FEATURE_DISABLED macros
- [ ] Define CONFIG_PARAM macro
- [ ] Define STAT_INC, STAT_ADD, STAT_MAX, STAT_SET macros
- [ ] Define ESSENTIAL_STAT_* macros
- [ ] Define STAT_TIMER_START, STAT_TIMER_END macros
- [ ] Define DEBUG_ONLY, DEBUG_LOG macros
- [ ] Define FROZEN_IN_MINIMAL, EXPOSURE_MINIMAL, EXPOSURE_ESSENTIAL macros

**1.4 Create ConfigDefaults.h**
- [ ] Create `src/config/ConfigDefaults.h`
- [ ] Include SearchConfigDefinitions.h and EvalConfigDefinitions.h
- [ ] Generate `namespace config::defaults` with constexpr values from X-macros

**1.5 Refactor SearchConfigData.h**
- [ ] Include SearchConfigDefinitions.h
- [ ] Replace manual member declarations with X-macro expansion
- [ ] Keep non-generated members (arrays like RFP_MARGIN, FP_MARGIN, LMP_MOVES)
- [ ] Verify struct compiles

**1.6 Refactor EvalConfigData.h**
- [ ] Include EvalConfigDefinitions.h
- [ ] Replace manual member declarations with X-macro expansion
- [ ] Verify struct compiles

**1.7 Refactor ConfigRegistry.cpp**
- [ ] Include definitions files and FeatureFlags.h
- [ ] Replace manual `definitions_.push_back()` calls with X-macro expansion
- [ ] Keep non-generated entries (arrays, special handling)
- [ ] Update static_assert size checks to new struct sizes
- [ ] Add `frozen` field to ConfigDef struct in ConfigRegistry.h

**1.8 CMake Integration**
- [ ] Add `option(FRANKYCPP_MINIMAL_BUILD ...)` to CMakeLists.txt
- [ ] Add compile definition when enabled
- [ ] Add CMake preset `win-release-tournament` to CMakePresets.json
- [ ] Add CMake preset `wsl-release-tournament` to CMakePresets.json

**1.9 Verification**
- [ ] Build in normal mode (no MINIMAL_BUILD) - must compile
- [ ] Build in minimal mode (MINIMAL_BUILD=ON) - must compile
- [ ] Run tests in normal mode - all must pass
- [ ] Verify `--show-config` output looks correct

#### Acceptance Criteria
- [ ] All existing tests pass in normal build mode
- [ ] Project compiles in both normal and minimal build modes
- [ ] `--show-config` displays all configs correctly
- [ ] UCI options work as before in normal build
- [ ] No runtime behavior change in normal build

#### Rollback
If phase fails: `git checkout -- src/config/` and `git clean -fd src/config/`

#### Commit
```powershell
git add src/config/ CMakeLists.txt CMakePresets.json
git commit -m "Phase 1: X-Macro infrastructure for config system

- Add SearchConfigDefinitions.h with feature-grouped X-macros
- Add EvalConfigDefinitions.h with X-macros  
- Add FeatureFlags.h with compile-time macros
- Add ConfigDefaults.h with constexpr defaults
- Refactor SearchConfigData.h to use X-macro generation
- Refactor EvalConfigData.h to use X-macro generation
- Refactor ConfigRegistry.cpp to use X-macro generation
- Add FRANKYCPP_MINIMAL_BUILD CMake option
- Add tournament build presets"
```

---

### Phase 1.5: Config Framework Integration ✦ Part of Phase 1 with X-macros

**Status:** ⬜ Not Started  
**Estimated Effort:** 0.5 session  
**Risk:** 🟡 Medium - Extends Phase 1  

> **Note:** With the X-macro approach, this phase is largely handled by Phase 1.
> The `essential` parameter in each X-macro definition controls frozen/exposure behavior.

#### Prerequisites
- [ ] Phase 1 complete and committed
- [ ] All tests passing

#### Tasks

**1.5.1 Verify Essential Configs**
- [ ] Verify `essential=true` in X-macro definitions for:
  - [ ] `TT_SIZE_MB` / Hash
  - [ ] `TB_PATH` / SyzygyPath
  - [ ] `BOOK_PATH`, `BOOK_TYPE`
  - [ ] `USE_BOOK`
  - [ ] `USE_PONDER` / Ponder
  - [ ] `MOVE_OVERHEAD_MS` / Move Overhead
  - [ ] `USE_PAWN_TT`, `PAWN_TT_SIZE_MB`

**1.5.2 Update Display**
- [ ] Update `--show-config` to display `[FROZEN]` marker for frozen configs
- [ ] Update engine name in minimal build: `FrankyCPP v1.x (minimal)`

**1.5.3 Verification**
- [ ] Test: YAML loading ignores frozen options in minimal builds
- [ ] Test: UCI options list excludes frozen options in minimal builds
- [ ] Test: Essential configs work in both build modes

#### Acceptance Criteria
- [ ] Essential configs (Hash, SyzygyPath, etc.) work in minimal builds
- [ ] Frozen configs show `[FROZEN]` in `--show-config`
- [ ] Engine name shows `(minimal)` suffix in minimal builds
- [ ] UCI `uci` command doesn't list frozen options in minimal builds

#### Commit
```powershell
git add src/
git commit -m "Phase 1.5: Config framework integration

- Verify essential configs marked correctly
- Add [FROZEN] display marker
- Update engine name for minimal builds"
```

---

### Phase 2: Statistics Migration (Search.cpp) ✦ BREAKING - modifies Search.cpp

**Status:** ⬜ Not Started  
**Estimated Effort:** 1 session  
**Risk:** 🟡 Medium - Many small changes, easy to miss some  

#### Prerequisites
- [ ] Phase 1 and 1.5 complete
- [ ] All tests passing

#### Tasks

**2.1 Identify Essential Stats (keep as ESSENTIAL_STAT_*)**
These must ALWAYS be collected for UCI info and time management:
- [ ] `nodesVisited` - required for NPS calculation
- [ ] `currentIterationDepth` - required for UCI info depth
- [ ] `currentSearchDepth` - required for UCI info seldepth  
- [ ] `currentExtraSearchDepth` - required for UCI info
- [ ] `currentBestRootMove` - required for UCI bestmove
- [ ] `currentBestRootMoveValue` - required for UCI info score
- [ ] `currentVariation` - required for UCI info pv
- [ ] `currentRootMoveIndex` - required for UCI info currmove
- [ ] `currentRootMove` - required for UCI info currmove

**2.2 Convert Optional Stats to STAT_* (by category)**

- [ ] **Node type counts:**
  - `pvNodes` → `STAT_INC(statistics.pvNodes)`
  - `nonPvNodes` → `STAT_INC(statistics.nonPvNodes)`
  - `searchNodes` → `STAT_INC(statistics.searchNodes)`
  - `qsearchNodes` → `STAT_INC(statistics.qsearchNodes)`

- [ ] **Terminal counts:**
  - `checkmates`, `stalemates`, `leafPositionsEvaluated`, `evaluations`

- [ ] **Pruning stats:**
  - `mdp`, `standpatCuts`, `razorings`, `rfp_cuts`, `nullMoveCuts`
  - `fpPrunings`, `qfpPrunings`, `lmpCuts`

- [ ] **Beta cut stats:**
  - `betaCuts`, `betaCutsByIndex[]`

- [ ] **TT stats:**
  - `ttHit`, `ttMiss`, `TtCuts`, `TtNoCuts`
  - `evalFromTT`, `NoTtMove`, `TtMoveUsed`

- [ ] **LMR stats:**
  - `lmrReductions`, `lmrResearches`
  - `lmrHistoryLessReduction`, `lmrHistoryDepthSaved`
  - `lmrCutNodeReductions`

- [ ] **Extension stats:**
  - `checkExtension`, `threatExtension`
  - `singularSearches`, `singularFilteredByBound`, `singularExtension`

- [ ] **IID/IIR stats:**
  - `iidSearches`, `iidMoves`, `iirReductions`

- [ ] **Re-search stats:**
  - `rootPvsResearches`, `pvsResearches`
  - `aspirationResearches`, `bestMoveChange`

- [ ] **Tablebase stats:**
  - `tbRootHits`, `tbSearchProbes`, `tbSearchHits`
  - `tbSearchMisses`, `tbSearchCutoffs`

- [ ] **Improving stats:**
  - `improvingTrue`, `improvingFalse`

- [ ] **Null-move stats:**
  - `nullMoveVerifications`

**2.3 Verification**
- [ ] Grep for `statistics.` in Search.cpp - all should use macros
- [ ] Build in normal mode - verify stats still collected
- [ ] Build in minimal mode - verify compiles (stats eliminated)
- [ ] Run tests - all must pass

#### Acceptance Criteria
- [ ] All `statistics.xxx` accesses use STAT_* or ESSENTIAL_STAT_* macros
- [ ] Normal build: stats collected as before
- [ ] Minimal build: compiles, stats eliminated
- [ ] All tests pass

#### Commit
```powershell
git add src/engine/Search.cpp
git commit -m "Phase 2: Statistics migration to STAT_* macros

- Convert ~100+ stat accesses to STAT_INC/ADD/SET/MAX macros
- Keep essential stats (nodes, depth, pv) as ESSENTIAL_STAT_*
- Stats eliminated in MINIMAL_BUILD mode"
```

---

### Phase 3: Feature Guard Migration (Search.cpp) ✦ BREAKING - modifies Search.cpp

**Status:** ⬜ Not Started  
**Estimated Effort:** 1 session  
**Risk:** 🟡 Medium - Many changes, but pattern is consistent  

#### Prerequisites
- [ ] Phase 2 complete
- [ ] All tests passing

#### Tasks

**3.1 High-Impact Guards (hot path)**
- [ ] `USE_TT` → `FEATURE_ENABLED(SearchConfig.USE_TT)`
- [ ] `USE_TT_VALUE` → `FEATURE_ENABLED(SearchConfig.USE_TT_VALUE)`
- [ ] `USE_EVAL_TT` → `FEATURE_ENABLED(SearchConfig.USE_EVAL_TT)`
- [ ] `USE_LMR` → `FEATURE_ENABLED(SearchConfig.USE_LMR)`
- [ ] `USE_NMP` → `FEATURE_ENABLED(SearchConfig.USE_NMP)`
- [ ] `USE_FP` → `FEATURE_ENABLED(SearchConfig.USE_FP)`
- [ ] `USE_QFP` → `FEATURE_ENABLED(SearchConfig.USE_QFP)`
- [ ] `USE_PVS` → `FEATURE_ENABLED(SearchConfig.USE_PVS)`
- [ ] `USE_IMPROVING` → `FEATURE_ENABLED(SearchConfig.USE_IMPROVING)`

**3.2 Medium-Impact Guards**
- [ ] `USE_RAZORING` → `FEATURE_ENABLED(SearchConfig.USE_RAZORING)`
- [ ] `USE_RFP` → `FEATURE_ENABLED(SearchConfig.USE_RFP)`
- [ ] `USE_MDP` → `FEATURE_ENABLED(SearchConfig.USE_MDP)`
- [ ] `USE_CHECK_EXT` → `FEATURE_ENABLED(SearchConfig.USE_CHECK_EXT)`
- [ ] `USE_THREAT_EXT` → `FEATURE_ENABLED(SearchConfig.USE_THREAT_EXT)`
- [ ] `USE_SINGULAR_EXT` → `FEATURE_ENABLED(SearchConfig.USE_SINGULAR_EXT)`
- [ ] `USE_LMP` → `FEATURE_ENABLED(SearchConfig.USE_LMP)`
- [ ] `USE_IID` → `FEATURE_ENABLED(SearchConfig.USE_IID)`
- [ ] `USE_IIR` → `FEATURE_ENABLED(SearchConfig.USE_IIR)`

**3.3 Low-Impact Guards**
- [ ] `USE_BOOK` → `FEATURE_ENABLED(SearchConfig.USE_BOOK)`
- [ ] `USE_ASP` → `FEATURE_ENABLED(SearchConfig.USE_ASP)`
- [ ] `USE_TB_PROBE_ROOT` → `FEATURE_ENABLED(SearchConfig.USE_TB_PROBE_ROOT)`
- [ ] `USE_TB_PROBE_SEARCH` → `FEATURE_ENABLED(SearchConfig.USE_TB_PROBE_SEARCH)`
- [ ] `USE_BESTMOVE_INSTABILITY` → `FEATURE_ENABLED(SearchConfig.USE_BESTMOVE_INSTABILITY)`

**3.4 Feature Parameters**
Convert numeric parameter accesses to CONFIG_PARAM:
- [ ] `SearchConfig.LMR_MIN_DEPTH` → `CONFIG_PARAM(LMR_MIN_DEPTH)`
- [ ] `SearchConfig.LMR_MIN_MOVES` → `CONFIG_PARAM(LMR_MIN_MOVES)`
- [ ] `SearchConfig.NMP_DEPTH` → `CONFIG_PARAM(NMP_DEPTH)`
- [ ] `SearchConfig.NMP_REDUCTION` → `CONFIG_PARAM(NMP_REDUCTION)`
- [ ] ... (all numeric params used in hot path)

**3.5 Verification**
- [ ] Grep for `SearchConfig.USE_` - all should use FEATURE_ENABLED
- [ ] Build in normal mode - behavior unchanged
- [ ] Build in minimal mode - compiles, guards eliminated
- [ ] Run tests - all must pass
- [ ] Run benchmark - verify NPS similar or better

#### Acceptance Criteria
- [ ] All feature guards use FEATURE_ENABLED macro
- [ ] All hot-path params use CONFIG_PARAM macro
- [ ] Normal build: behavior identical to before
- [ ] Minimal build: compiles, guards eliminated
- [ ] All tests pass

#### Commit
```powershell
git add src/engine/Search.cpp
git commit -m "Phase 3: Feature guard migration to macros

- Convert all USE_* guards to FEATURE_ENABLED() macro
- Convert hot-path params to CONFIG_PARAM() macro
- Guards become compile-time true in MINIMAL_BUILD mode"
```

---

### Phase 4: Testing & Validation ✦ Non-breaking, verification only

**Status:** ⬜ Not Started  
**Estimated Effort:** 0.5-1 session  
**Risk:** 🟢 Low - Verification only  

#### Prerequisites
- [ ] Phases 1-3 complete
- [ ] All tests passing in normal mode

#### Tasks

**4.1 Functional Verification**
- [ ] Run full test suite in normal build mode
- [ ] Run full test suite in minimal build mode
- [ ] Compare search results: same position should give same best move
- [ ] Verify UCI protocol works correctly in both modes

**4.2 Performance Benchmarking**
- [ ] Benchmark NPS in normal mode (baseline)
- [ ] Benchmark NPS in minimal mode
- [ ] Expected improvement: 1-3%
- [ ] Document results

**4.3 Integration Testing**
- [ ] Test with cutechess-cli against itself (normal vs minimal)
- [ ] Verify no crashes, hangs, or incorrect behavior
- [ ] Test time management works correctly

**4.4 Edge Cases**
- [ ] Test with various YAML configs
- [ ] Test UCI setoption for essential options
- [ ] Test `--show-config` output in both modes

#### Acceptance Criteria
- [ ] All tests pass in both build modes
- [ ] Same best move for same position in both modes
- [ ] NPS improvement measurable in minimal mode
- [ ] No regressions in functionality

#### Commit
```powershell
git add test/
git commit -m "Phase 4: Testing and validation complete

- All tests pass in both build modes
- NPS improvement: X% in minimal build
- Verified UCI protocol works correctly"
```

---

### Phase 5: Documentation ✦ Non-breaking, documentation only

**Status:** ⬜ Not Started  
**Estimated Effort:** 0.5 session  
**Risk:** 🟢 Low - Documentation only  

#### Tasks

- [ ] Update `.github/copilot-instructions.md`:
  - Document new X-macro config system
  - Document FEATURE_ENABLED, CONFIG_PARAM, STAT_* macros
  - Document how to add new configs (single file!)
  - Document FRANKYCPP_MINIMAL_BUILD option

- [ ] Update `README.md`:
  - Add tournament build instructions
  - Document minimal build mode

- [ ] Update `CLAUDE.md` if present

- [ ] Update this plan document:
  - Mark all phases complete
  - Add final session log entry
  - Document any lessons learned

#### Acceptance Criteria
- [ ] All documentation updated
- [ ] New contributor can understand X-macro system
- [ ] Build instructions include tournament mode

#### Commit
```powershell
git add .github/ README.md docs/
git commit -m "Phase 5: Documentation updates

- Update copilot-instructions with X-macro system
- Add tournament build instructions to README
- Document FRANKYCPP_MINIMAL_BUILD option"
```

---

---

## Code Transformation Examples

### Feature Guard Transformation

**Before:**
```cpp
if (SearchConfig.USE_LMR && depth >= SearchConfig.LMR_MIN_DEPTH 
    && moveCount >= SearchConfig.LMR_MIN_MOVES) {
  // LMR logic
}
```

**After:**
```cpp
// Boolean flags use FEATURE_ENABLED, numeric params use CONFIG_PARAM
if (FEATURE_ENABLED(SearchConfig.USE_LMR) 
    && depth >= CONFIG_PARAM(LMR_MIN_DEPTH) 
    && moveCount >= CONFIG_PARAM(LMR_MIN_MOVES)) {
  // LMR logic
}
```

In minimal builds, compiler sees:
```cpp
// FEATURE_ENABLED → true, CONFIG_PARAM → config::defaults::XXX (constexpr)
// Compiler optimizes to:
if (true && depth >= 2 && moveCount >= 2) {
  // LMR logic - all values are compile-time constants
}
```

**Key insight:** Default values come from ONE place - `SearchConfigDefinitions.h`.
The X-macro expansion generates:
- `SearchConfigData` struct members with defaults
- `config::defaults::` constexpr values (used by `CONFIG_PARAM()` in minimal builds)
- Registry entries with defaults

No duplication, no mismatch risk.

---

### Statistics Transformation

**Before:**
```cpp
statistics.lmrReductions++;
statistics.betaCuts++;
statistics.betaCutsByIndex[std::min(movesSearched - 1, 9)]++;
```

**After:**
```cpp
STAT_INC(statistics.lmrReductions);
STAT_INC(statistics.betaCuts);
STAT_INC(statistics.betaCutsByIndex[std::min(movesSearched - 1, 9)]);
```

In minimal builds:
```cpp
((void)0);  // Completely eliminated by compiler
((void)0);
((void)0);
```

### Essential Stats (Always Active)

```cpp
// These are ALWAYS collected for UCI info output
ESSENTIAL_STAT_INC(nodesVisited);
ESSENTIAL_STAT_SET(statistics.currentSearchDepth, iterationDepth);
ESSENTIAL_STAT_MAX(statistics.currentExtraSearchDepth, ply);
```

---

## Configuration Framework Compatibility

### The Challenge

In minimal builds, feature guards like `USE_LMR` become compile-time `true`. However:
- The ConfigRegistry still exists and registers all options
- UCI `setoption` commands still work (values stored in struct)
- YAML loading still works
- **But changing values at runtime has no effect** (the code ignores them)

### Design Decision: Registry-Controlled Freezing (RECOMMENDED)

The ConfigRegistry already has `exposure.uci` and `exposure.yaml` flags per option.
We can make these **compile-time conditional** using macros in the registry definitions.

**Key Insight:** Control everything from one place - the ConfigDef entries in ConfigRegistry.cpp.

### Implementation: Frozen Flag in ConfigDef

Add a `frozen` field to ConfigDef that is set via macro:

```cpp
// In ConfigDef.h
struct ConfigDef {
  std::string name;
  std::string uciName;
  std::string description;
  ConfigValueType valueType;
  ConfigDomain domain;
  std::string defaultValue;
  
  struct Exposure {
    bool uci;      // Expose via UCI setoption
    bool yaml;     // Load from YAML config
    bool display;  // Show in --show-config
  } exposure;
  
  bool frozen;     // NEW: True if compile-time constant (minimal build)
  
  // ... getter/setter lambdas
};
```

### Macro for Frozen Options

In `FeatureFlags.h`:

```cpp
// ============================================================================
// CONFIG REGISTRY HELPERS - Mark options as frozen in minimal builds
// ============================================================================

#ifdef FRANKYCPP_MINIMAL_BUILD
  // In minimal builds, feature options are frozen (compile-time constants)
  #define FROZEN_IN_MINIMAL true
  #define EXPOSURE_MINIMAL(uci, yaml, disp) {.uci = false, .yaml = false, .display = (disp)}
#else
  #define FROZEN_IN_MINIMAL false
  #define EXPOSURE_MINIMAL(uci, yaml, disp) {.uci = (uci), .yaml = (yaml), .display = (disp)}
#endif

// Essential options - never frozen, always configurable
#define EXPOSURE_ESSENTIAL(uci, yaml, disp) {.uci = (uci), .yaml = (yaml), .display = (disp)}
```

### Registry Definition Examples

In `ConfigRegistry.cpp`:

```cpp
// FROZEN in minimal builds - feature flag
{
  .name = "USE_LMR",
  .uciName = "Use LMR",
  .description = "Enable Late Move Reductions",
  .valueType = Bool,
  .domain = Search,
  .defaultValue = "true",
  .exposure = EXPOSURE_MINIMAL(true, true, true),  // Hidden in minimal
  .frozen = FROZEN_IN_MINIMAL,
  .getter = searchGetter(&SearchConfigData::USE_LMR),
  .setter = searchSetter(&SearchConfigData::USE_LMR, parseBool)
},

// FROZEN in minimal builds - feature parameter
{
  .name = "LMR_MIN_DEPTH",
  .uciName = "LMR Min Depth",
  .description = "Minimum depth for LMR",
  .valueType = Int,
  .domain = Search,
  .defaultValue = "2",
  .exposure = EXPOSURE_MINIMAL(false, true, true),  // Hidden in minimal
  .frozen = FROZEN_IN_MINIMAL,
  .getter = searchGetter(&SearchConfigData::LMR_MIN_DEPTH),
  .setter = searchSetter(&SearchConfigData::LMR_MIN_DEPTH, parseInt)
},

// ESSENTIAL - never frozen, always runtime configurable
{
  .name = "TT_SIZE_MB",
  .uciName = "Hash",
  .description = "Transposition table size in MB",
  .valueType = Int,
  .domain = Search,
  .defaultValue = "64",
  .exposure = EXPOSURE_ESSENTIAL(true, true, true),  // Always available
  .frozen = false,  // Never frozen
  .getter = searchGetter(&SearchConfigData::TT_SIZE_MB),
  .setter = searchSetter(&SearchConfigData::TT_SIZE_MB, parseInt)
},
```

### Registry Methods Use Frozen Flag

The existing filter methods naturally work:

```cpp
std::vector<const ConfigDef*> ConfigRegistry::uciOptions() const {
  std::vector<const ConfigDef*> result;
  for (const auto& def : definitions_) {
    if (def.exposure.uci) {  // Already false for frozen options in minimal builds!
      result.push_back(&def);
    }
  }
  return result;
}

std::vector<const ConfigDef*> ConfigRegistry::yamlOptions() const {
  std::vector<const ConfigDef*> result;
  for (const auto& def : definitions_) {
    if (def.exposure.yaml) {  // Already false for frozen options!
      result.push_back(&def);
    }
  }
  return result;
}
```

### Enhanced --show-config Display

```cpp
std::string ConfigRegistry::formatOption(const ConfigDef& def, ...) const {
  std::ostringstream os;
  os << def.name << " = " << value;
  if (def.frozen) {
    os << " [FROZEN]";  // Indicate compile-time constant
  }
  return os.str();
}
```

### What This Achieves

| Aspect                  | Normal Build            | Minimal Build             |
|-------------------------|-------------------------|---------------------------|
| UCI `setoption USE_LMR` | Works, changes behavior | Not visible (not sent)    |
| UCI `setoption Hash`    | Works                   | Works ✓                   |
| YAML `USE_LMR: false`   | Loaded, applied         | Ignored (not parsed)      |
| YAML `TT_SIZE_MB: 128`  | Loaded, applied         | Loaded, applied ✓         |
| `--show-config`         | Shows all               | Shows all, marks [FROZEN] |

### Essential Configs (Never Frozen)

These use `EXPOSURE_ESSENTIAL` and `frozen = false`:

- `TT_SIZE_MB` / `Hash` - TT size must be configurable
- `TB_PATH` / `SyzygyPath` - Tablebase path
- `BOOK_PATH` - Opening book path
- `BOOK_TYPE` - Book type (SimpleBook vs PGN)
- `USE_BOOK` - Whether to use book (user choice, not a search feature)
- `USE_PONDER` / `Ponder` - Pondering on/off
- `MOVE_OVERHEAD_MS` / `Move Overhead` - Time management
- `USE_PAWN_TT` - Enable pawn hash table
- `PAWN_TT_SIZE_MB` - Pawn hash table size
- `Threads` (future) - Thread count

### Benefits of This Approach

1. **Single source of truth** - All freeze logic in ConfigRegistry.cpp
2. **No runtime filtering** - `exposure` flags are compile-time constants
3. **Clean YAML parsing** - Frozen options simply not in `yamlOptions()` list
4. **Clean UCI output** - Frozen options not in `uciOptions()` list
5. **Self-documenting** - `frozen` field makes intent clear
6. **Easy to audit** - Grep for `FROZEN_IN_MINIMAL` to see all affected options

---

## Open Questions

1. **SearchStats struct in minimal builds:**
   - Option A: Keep struct but don't write to it (simpler, some memory waste)
   - Option B: Conditionally compile struct members (complex, less memory)
   - **Recommendation:** Option A - memory impact is negligible

2. ~~**Config values in minimal builds:**~~ **RESOLVED**
   - ~~Should defaults be hardcoded at call sites or from registry?~~
   - **Decision:** X-macro approach provides single source of truth.
     All configs defined ONCE in `SearchConfigDefinitions.h`.
     The X-macros generate:
     - Struct members with defaults
     - `config::defaults::` constexpr values
     - Registry entries with defaults
     `CONFIG_PARAM(name)` macro expands to:
     - Normal build: `SearchConfig.name` (runtime value)
     - Minimal build: `config::defaults::name` (compile-time constexpr)

3. ~~**3 places to define configs:**~~ **RESOLVED**
   - ~~Adding a config requires editing struct, registry, and defaults - too many places!~~
   - **Decision:** X-macro approach - define each config ONCE in the definitions file.
     Struct, defaults, and registry entries are all auto-generated.

4. **Granular control:**
   - Should we allow `FRANKYCPP_DISABLE_STATS` without `FRANKYCPP_MINIMAL_BUILD`?
   - **Recommendation:** Start simple, add granularity if needed

5. **UCI currmove/currline output:**
   - These need `currentVariation` updated - mark as essential?
   - **Recommendation:** Mark as essential (used for UCI info during search)

6. ~~**Config framework in minimal builds:**~~ **RESOLVED**
   - ~~Keep full framework working (values stored but ignored)?~~
   - ~~Or hide/disable non-functional options?~~
   - **Decision:** Registry-controlled freezing via `EXPOSURE_MINIMAL` macro.
     Frozen options are automatically excluded from UCI and YAML parsing.

---

## Success Criteria

1. **Zero-cost elimination:** Assembly inspection confirms no code generated for disabled macros
2. **Identical search results:** Same best move / evaluation in both build modes
3. **Tests pass:** Full test suite passes in both modes
4. **Measurable improvement:** NPS improvement of at least 1% in minimal builds
5. **Clean integration:** No disruption to normal development workflow
6. **Config framework functional:** 
   - YAML config files load without errors in both modes
   - UCI options work (essential ones have effect, others store value)
   - `--show-config` works in both modes
   - Engine name indicates minimal build mode

---

## References

### New Files (to be created)
- `src/config/SearchConfigDefinitions.h` - **SINGLE SOURCE OF TRUTH** for search configs (X-macros)
- `src/config/EvalConfigDefinitions.h` - **SINGLE SOURCE OF TRUTH** for eval configs (X-macros)
- `src/config/ConfigDefaults.h` - Auto-generated constexpr defaults from X-macros
- `src/config/FeatureFlags.h` - Compile-time macros (FEATURE_ENABLED, CONFIG_PARAM, STAT_*, etc.)

### Existing Files (to be refactored)
- `src/config/SearchConfigData.h` - Struct will be auto-generated from X-macros
- `src/config/EvalConfigData.h` - Struct will be auto-generated from X-macros
- `src/config/ConfigRegistry.cpp` - Registry entries will be auto-generated from X-macros
- `src/engine/Search.cpp` - Primary migration target (~100+ stat accesses, feature guards)
- `src/engine/SearchStats.h` - Statistics struct (keep as-is, use STAT_* macros)

### External References
- Stockfish source - Similar approach with `#ifdef` guards
- X-macro pattern - Common C/C++ technique for DRY code generation

---

## Troubleshooting

### Common Issues

#### X-Macro Expansion Errors
**Symptom:** Cryptic compiler errors mentioning macro names  
**Solution:** 
1. Check that all X-macro arguments match expected count
2. Ensure no trailing commas in macro definitions
3. Use `/E` flag to see preprocessor output: `cl /E src/config/SearchConfigData.h`

#### Struct Size Mismatch
**Symptom:** `static_assert` failure in ConfigRegistry.cpp  
**Solution:**
1. Temporarily comment out static_assert
2. Add `fprintln("Size: {}", sizeof(SearchConfigData));` to print actual size
3. Update static_assert with new size

#### Missing Config in Minimal Build
**Symptom:** Essential config not working in minimal build  
**Solution:**
1. Check X-macro definition has `essential=true` (last parameter)
2. Verify EXPOSURE_ESSENTIAL is used, not EXPOSURE_MINIMAL

#### Stat Not Being Collected
**Symptom:** Statistic always zero in normal build  
**Solution:**
1. Verify using `STAT_INC` not `ESSENTIAL_STAT_INC` (or vice versa)
2. Check macro is defined in FeatureFlags.h
3. Verify `#include "config/FeatureFlags.h"` is present

#### Feature Always Enabled in Minimal Build
**Symptom:** Feature guard not being eliminated  
**Solution:**
1. Verify using `FEATURE_ENABLED(SearchConfig.USE_LMR)` not just `SearchConfig.USE_LMR`
2. Check FRANKYCPP_MINIMAL_BUILD is defined in CMake

### Debug Commands

```powershell
# Show preprocessor output for a file (MSVC)
cl /E /I src src/config/SearchConfigData.h > preprocessed.txt

# Check if MINIMAL_BUILD is defined
cmake -B build -DFRANKYCPP_MINIMAL_BUILD=ON
Select-String -Path build/* -Pattern "FRANKYCPP_MINIMAL_BUILD" -Recurse

# Find all stat accesses not using macros
Select-String -Path src/engine/Search.cpp -Pattern "statistics\." | Where-Object { $_ -notmatch "STAT_" }

# Find all feature guards not using macros  
Select-String -Path src/engine/Search.cpp -Pattern "SearchConfig\.USE_" | Where-Object { $_ -notmatch "FEATURE_ENABLED" }
```

---

## Lessons Learned

> **Note:** This section will be filled in after implementation is complete.

### What Worked Well
- (To be filled after implementation)

### What Was Challenging
- (To be filled after implementation)

### Recommendations for Similar Projects
- (To be filled after implementation)

---

## Future Considerations

### C++26 Reflection
When C++26 reflection is available, the X-macro approach could potentially be replaced with:
- Compile-time struct member enumeration
- Automatic getter/setter generation
- Cleaner syntax without macro "ugliness"

**Revisit this plan when:** GCC/Clang/MSVC support C++26 reflection (estimated 2027-2028)

### Granular Control Options
If needed, could add separate flags:
- `FRANKYCPP_DISABLE_STATS` - Disable stats only, keep feature guards
- `FRANKYCPP_DISABLE_DEBUG` - Disable debug logging only
- `FRANKYCPP_FROZEN_FEATURES` - Freeze features but keep stats

**Currently:** Starting simple with single `FRANKYCPP_MINIMAL_BUILD` flag.

### NNUE Integration
If NNUE evaluation is added in the future:
- NNUE-related configs should be marked `essential=true`
- Network path must be runtime-configurable
- Consider separate `EvalConfigDefinitions.h` structure for NNUE

---

## Appendix: Quick Reference

### Macro Cheat Sheet

| Macro | Normal Build | Minimal Build | Use For |
|-------|--------------|---------------|---------|
| `FEATURE_ENABLED(x)` | `(x)` | `(true)` | Boolean feature guards |
| `CONFIG_PARAM(name)` | `SearchConfig.name` | `config::defaults::name` | Numeric parameters |
| `STAT_INC(x)` | `++(x)` | `((void)0)` | Optional stats |
| `STAT_ADD(x,v)` | `(x)+=(v)` | `((void)0)` | Optional stats |
| `ESSENTIAL_STAT_INC(x)` | `++(x)` | `++(x)` | Required stats (nodes, depth) |
| `FROZEN_IN_MINIMAL` | `false` | `true` | ConfigDef.frozen field |
| `EXPOSURE_MINIMAL(u,y,d)` | `{u,y,d}` | `{false,false,d}` | Non-essential config exposure |
| `EXPOSURE_ESSENTIAL(u,y,d)` | `{u,y,d}` | `{u,y,d}` | Essential config exposure |

### X-Macro Parameter Order

```cpp
X_BOOL(name, default, description, uciName, uci, yaml, display, essential)
//     0     1        2            3        4    5     6        7

X_INT(name, default, description, uciName, min, max, uci, yaml, display, essential)
//    0     1        2            3        4    5    6    7     8        9

X_DOUBLE(name, default, description, uciName, min, max, uci, yaml, display, essential)
//       0     1        2            3        4    5    6    7     8        9

X_STRING(name, default, description, uciName, uci, yaml, display, essential)
//       0     1        2            3        4    5     6        7
```

### File Location Reference

| Purpose | File |
|---------|------|
| Search config definitions (edit here!) | `src/config/SearchConfigDefinitions.h` |
| Eval config definitions (edit here!) | `src/config/EvalConfigDefinitions.h` |
| Compile-time macros | `src/config/FeatureFlags.h` |
| Generated constexpr defaults | `src/config/ConfigDefaults.h` |
| Generated struct (don't edit directly) | `src/config/SearchConfigData.h` |
| Generated registry (don't edit directly) | `src/config/ConfigRegistry.cpp` |
| Main usage site | `src/engine/Search.cpp` |
