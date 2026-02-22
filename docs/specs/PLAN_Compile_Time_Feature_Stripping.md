# Compile-Time Feature Stripping Plan

**Status:** Planning  
**Created:** 2026-02-22  
**Author:** Frank Kopp

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

| Category | Flags | Count |
|----------|-------|-------|
| Core Search | `USE_ALPHABETA`, `USE_PVS`, `USE_ASP` | 3 |
| Quiescence | `USE_QUIESCENCE`, `USE_QS_TT`, `USE_QS_STANDPAT_CUT`, `USE_QS_SEE` | 4 |
| Transposition Table | `USE_TT`, `USE_TT_VALUE`, `USE_EVAL_TT`, `USE_TT_PV_MOVE_SORT` | 4 |
| Tablebase | `USE_TB_PROBE_ROOT`, `USE_TB_PROBE_SEARCH`, `USE_TB_PROBE_PV` | 3 |
| Move Ordering | `USE_KILLER_MOVES`, `USE_HISTORY_COUNTER`, `USE_HISTORY_MOVES` | 3 |
| IID/IIR | `USE_IID`, `USE_IIR` | 2 |
| Pruning | `USE_MDP`, `USE_RAZORING`, `USE_RFP`, `USE_NMP`, `USE_FP`, `USE_QFP`, `USE_LMP` | 7 |
| Extensions | `USE_EXTENSIONS`, `USE_CHECK_EXT`, `USE_THREAT_EXT`, `USE_SINGULAR_EXT`, `USE_EXT_ADD_DEPTH` | 5 |
| LMR | `USE_LMR`, `USE_LMR_IMPROVING`, `USE_LMR_HISTORY`, `USE_LMR_CUTNODE` | 4 |
| Improving | `USE_IMPROVING`, `USE_RFP_IMPROVING`, `USE_NMP_IMPROVING`, `USE_FP_IMPROVING`, `USE_LMP_IMPROVING` | 5 |
| Time Mgmt | `USE_BESTMOVE_INSTABILITY` | 1 |
| Book/Ponder | `USE_BOOK`, `USE_PONDER` | 2 |

**Total: ~43 feature flags**

### Statistics (from SearchStats.h + Search.cpp)

Based on grep of `statistics.` usage in Search.cpp:

| Category | Statistics Fields | Approx Usage Count |
|----------|-------------------|---------------------|
| Node counts | `pvNodes`, `nonPvNodes`, `searchNodes`, `qsearchNodes` | 5 |
| Terminal | `checkmates`, `stalemates`, `leafPositionsEvaluated`, `evaluations` | 6 |
| Pruning | `mdp`, `standpatCuts`, `razorings`, `rfp_cuts`, `nullMoveCuts`, `fpPrunings`, `qfpPrunings`, `lmpCuts` | 10 |
| Beta cuts | `betaCuts`, `betaCutsByIndex[]` | 6 |
| TT | `ttHit`, `ttMiss`, `TtCuts`, `TtNoCuts`, `evalFromTT`, `NoTtMove`, `TtMoveUsed` | 10 |
| LMR | `lmrReductions`, `lmrResearches`, `lmrHistoryLessReduction`, `lmrHistoryDepthSaved`, `lmrCutNodeReductions` | 6 |
| Extensions | `checkExtension`, `threatExtension`, `singularSearches`, `singularFilteredByBound`, `singularExtension` | 6 |
| IID/IIR | `iidSearches`, `iidMoves`, `iirReductions` | 4 |
| Improving | `improvingTrue`, `improvingFalse` | 2 |
| Tablebase | `tbRootHits`, `tbSearchProbes`, `tbSearchHits`, `tbSearchMisses`, `tbSearchCutoffs` | 6 |
| Re-search | `rootPvsResearches`, `pvsResearches`, `aspirationResearches`, `bestMoveChange` | 5 |
| Current state | `currentIterationDepth`, `currentSearchDepth`, `currentExtraSearchDepth`, `currentBestRootMove`, `currentBestRootMoveValue`, `currentVariation`, `currentRootMoveIndex`, `currentRootMove` | 15+ |
| Null-move | `nullMoveVerifications` | 2 |
| Perft | `perftNodeCount` | 2 |

**Total: ~100+ statistics field accesses**

---

## Design

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
│  │  CONFIG_VALUE     │  │  STAT_TIMER_*     │  │ DEBUG_LOG    │ │
│  └───────────────────┘  └───────────────────┘  └──────────────┘ │
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
//   - FEATURE_ENABLED/DISABLED: Runtime feature toggle guards
//   - CONFIG_VALUE: Config parameter access with compile-time default
//   - STAT_*: Optional statistics collection
//   - ESSENTIAL_STAT_*: Always-collected stats (nodes, time, depth)
//   - DEBUG_*: Debug-only code and logging
//   - EXPOSURE_MINIMAL/ESSENTIAL: Config registry exposure control
//
//=============================================================================

#include <algorithm>
#include <chrono>

// ============================================================================
// FEATURE GUARDS - Control runtime feature toggles
// ============================================================================

#ifdef FRANKYCPP_MINIMAL_BUILD

  // Feature guards become compile-time constants - compiler eliminates branches
  #define FEATURE_ENABLED(condition) (true)
  #define FEATURE_DISABLED(condition) (false)
  
  // Config values become compile-time constants
  #define CONFIG_VALUE(value, default_val) (default_val)

#else

  #define FEATURE_ENABLED(condition) (condition)
  #define FEATURE_DISABLED(condition) (!(condition))
  #define CONFIG_VALUE(value, default_val) (value)

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

### Phase 1: Infrastructure Setup
- [ ] Create `src/config/FeatureFlags.h` with macro definitions
- [ ] Add CMake option and configuration
- [ ] Add CMake preset for tournament builds
- [ ] Test compilation in both modes

### Phase 1.5: Config Framework Integration
- [ ] Add `frozen` field to `ConfigDef` struct
- [ ] Add `FROZEN_IN_MINIMAL`, `EXPOSURE_MINIMAL`, `EXPOSURE_ESSENTIAL` macros to `FeatureFlags.h`
- [ ] Update all feature flag entries in ConfigRegistry to use `EXPOSURE_MINIMAL`
- [ ] Update all feature parameter entries to use `EXPOSURE_MINIMAL`
- [ ] Identify and mark essential configs with `EXPOSURE_ESSENTIAL` (never frozen):
  - `TT_SIZE_MB` / Hash
  - `TB_PATH` / SyzygyPath
  - `BOOK_PATH`
  - `USE_BOOK` (user preference, not search feature)
  - `USE_PONDER` / Ponder
  - `MOVE_OVERHEAD_MS` / Move Overhead
- [ ] Update `--show-config` to display `[FROZEN]` marker
- [ ] Update engine name to indicate minimal build: `FrankyCPP v1.x (minimal)`
- [ ] Test: YAML loading ignores frozen options in minimal builds
- [ ] Test: UCI options list excludes frozen options in minimal builds

### Phase 2: Statistics Migration (Search.cpp)
Migrate statistics collection to use macros. Group by category:

#### Essential Stats (keep using ESSENTIAL_STAT_*)
- `nodesVisited` (main node counter for NPS)
- `currentIterationDepth`, `currentSearchDepth`, `currentExtraSearchDepth`
- `currentBestRootMove`, `currentBestRootMoveValue`
- `currentVariation`, `currentRootMoveIndex`, `currentRootMove`

#### Optional Stats (convert to STAT_*)
- [ ] Node type counts: `pvNodes`, `nonPvNodes`, `searchNodes`, `qsearchNodes`
- [ ] Terminal counts: `checkmates`, `stalemates`, `leafPositionsEvaluated`, `evaluations`
- [ ] Pruning stats: `mdp`, `standpatCuts`, `razorings`, `rfp_cuts`, `nullMoveCuts`, etc.
- [ ] Beta cut stats: `betaCuts`, `betaCutsByIndex[]`
- [ ] TT stats: `ttHit`, `ttMiss`, `TtCuts`, etc.
- [ ] LMR stats: `lmrReductions`, `lmrResearches`, etc.
- [ ] Extension stats: `checkExtension`, `threatExtension`, etc.
- [ ] Re-search stats: `rootPvsResearches`, `pvsResearches`, etc.
- [ ] Tablebase stats: `tbSearchProbes`, `tbSearchHits`, etc.
- [ ] Improving stats: `improvingTrue`, `improvingFalse`

### Phase 3: Feature Guard Migration (Search.cpp)
Convert feature guards to macros, starting with most-used:

#### High-Impact (frequently checked in hot path)
- [ ] `USE_TT` / `USE_TT_VALUE` / `USE_EVAL_TT` (TT access)
- [ ] `USE_LMR` (late move reductions)
- [ ] `USE_NMP` (null move pruning)
- [ ] `USE_FP` / `USE_QFP` (futility pruning)
- [ ] `USE_PVS` (principal variation search)
- [ ] `USE_IMPROVING` and variants

#### Medium-Impact
- [ ] `USE_RAZORING`, `USE_RFP`, `USE_MDP`
- [ ] `USE_CHECK_EXT`, `USE_THREAT_EXT`, `USE_SINGULAR_EXT`
- [ ] `USE_LMP`
- [ ] `USE_IID` / `USE_IIR`

#### Low-Impact (checked once per search)
- [ ] `USE_BOOK`
- [ ] `USE_ASP`
- [ ] `USE_TB_PROBE_ROOT`, `USE_TB_PROBE_SEARCH`
- [ ] `USE_BESTMOVE_INSTABILITY`

### Phase 4: Testing & Validation
- [ ] Verify identical behavior in both build modes
- [ ] Run full test suite in both modes
- [ ] Benchmark NPS difference (expect 1-3% improvement)
- [ ] Ensure UCI output still works correctly
- [ ] Verify time management functions (uses nodes/nps)

### Phase 5: Documentation
- [ ] Update `copilot-instructions.md` with new macros
- [ ] Update `README.md` build instructions
- [ ] Document tournament build in release process

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
if (FEATURE_ENABLED(SearchConfig.USE_LMR) 
    && depth >= CONFIG_VALUE(SearchConfig.LMR_MIN_DEPTH, 2) 
    && moveCount >= CONFIG_VALUE(SearchConfig.LMR_MIN_MOVES, 2)) {
  // LMR logic
}
```

In minimal builds, compiler sees:
```cpp
if (true && depth >= 2 && moveCount >= 2) {
  // LMR logic - always executed when conditions met
}
```

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

| Aspect | Normal Build | Minimal Build |
|--------|--------------|---------------|
| UCI `setoption USE_LMR` | Works, changes behavior | Not visible (not sent) |
| UCI `setoption Hash` | Works | Works ✓ |
| YAML `USE_LMR: false` | Loaded, applied | Ignored (not parsed) |
| YAML `TT_SIZE_MB: 128` | Loaded, applied | Loaded, applied ✓ |
| `--show-config` | Shows all | Shows all, marks [FROZEN] |

### Essential Configs (Never Frozen)

These use `EXPOSURE_ESSENTIAL` and `frozen = false`:

- `TT_SIZE_MB` / `Hash` - TT size must be configurable
- `TB_PATH` / `SyzygyPath` - Tablebase path
- `BOOK_PATH` - Opening book path  
- `USE_BOOK` - Whether to use book (user choice, not a search feature)
- `USE_PONDER` / `Ponder` - Pondering on/off
- `MOVE_OVERHEAD_MS` / `Move Overhead` - Time management
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

2. **Config values in minimal builds:**
   - Should `CONFIG_VALUE` use hardcoded defaults or current SearchConfig defaults?
   - **Recommendation:** Use SearchConfigData defaults (consistent, maintainable)

3. **Granular control:**
   - Should we allow `FRANKYCPP_DISABLE_STATS` without `FRANKYCPP_MINIMAL_BUILD`?
   - **Recommendation:** Start simple, add granularity if needed

4. **UCI currmove/currline output:**
   - These need `currentVariation` updated - mark as essential?
   - **Recommendation:** Mark as essential (used for UCI info during search)

5. ~~**Config framework in minimal builds:**~~ **RESOLVED**
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

- `src/config/SearchConfigData.h` - All feature flags
- `src/engine/SearchStats.h` - All statistics fields
- `src/engine/Search.cpp` - Primary migration target (~100+ stat accesses)
- Stockfish source - Similar approach with `#ifdef` guards
