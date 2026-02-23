# Constexpr Config Approach (Recommended Plan)

**Status:** Recommended  
**Created:** 2026-02-23  
**Last Updated:** 2026-02-23  
**Author:** Frank Kopp

---

## Project Metadata

| Field                | Value                                       |
|----------------------|---------------------------------------------|
| **Branch**           | `feature/compile-time-stripping` ✅ exists   |
| **Risk Level**       | 🟢 Low - Incremental changes, easy rollback |
| **Estimated Effort** | 2-3 sessions                                |
| **Rollback**         | Remove macros, revert to plain members      |

### Phase Status Tracker

| Phase                               | Status        | Notes                             |
|-------------------------------------|---------------|-----------------------------------|
| Phase 1: Infrastructure             | ⬜ Not Started | Create ConfigMode.h, CMake option |
| Phase 2: SearchConfigData Migration | ⬜ Not Started | Add CONFIG_CONST to members       |
| Phase 3: ConfigManager Conditional  | ⬜ Not Started | Return constexpr in production    |
| Phase 4: ConfigRegistry Conditional | ⬜ Not Started | Hide non-essential in production  |
| Phase 5: Statistics Macros          | ⬜ Not Started | STAT_INC etc.                     |
| Phase 6: Verification               | ⬜ Not Started | Benchmark, test both builds       |

**Status Legend:** ⬜ Not Started | 🔄 In Progress | ✅ Complete | ❌ Blocked

---

## Overview

This document describes the **recommended approach** for compile-time feature stripping. Instead of complex X-macro metaprogramming, we use the **STL pattern** of conditional `constexpr` via simple macros.

**Key Insight:** The MSVC STL uses macros like `_CONSTEXPR20` to conditionally make code `constexpr` based on C++ version. We use the same pattern to make config values `constexpr` in production builds while keeping them mutable in development.

**Why This Approach Over X-Macros:**
- **90% of the benefit with 20% of the complexity**
- Search.cpp code doesn't change (no `FEATURE_ENABLED()` wrappers)
- Normal C++ struct syntax - IDE understands it
- Incremental migration - can add one config at a time
- Easy rollback - just remove the macros

---

## The STL Pattern (from yvals_core.h)

```cpp
// MSVC STL conditional constexpr pattern
#ifdef __cpp_lib_constexpr_vector
#define _CONSTEXPR20 constexpr
#else
#define _CONSTEXPR20 inline
#endif

// Usage in vector:
_CONSTEXPR20 void push_back(_Ty&& _Val) { ... }
```

The same pattern is used throughout MSVC STL:
- `_CONSTEXPR17`, `_CONSTEXPR20`, `_CONSTEXPR23`
- `_NODISCARD`, `_NOEXCEPT_COND`
- Enables gradual adoption of language features

---

## Proposed Design

### Core Macro: `CONFIG_CONST`

```cpp
// src/config/ConfigMode.h

#ifndef FRANKYCPP_CONFIG_MODE_H
#define FRANKYCPP_CONFIG_MODE_H

//=============================================================================
// ConfigMode.h - Conditional constexpr for configuration values
//=============================================================================
//
// In PRODUCTION builds (-DFRANKYCPP_PRODUCTION):
//   - Config values become constexpr (compile-time constants)
//   - Compiler eliminates dead branches, inlines values
//   - No runtime config changes possible
//   - UCI/YAML config loading disabled for frozen options
//
// In DEVELOPMENT builds (default):
//   - Config values are mutable (runtime changeable)
//   - Full UCI/YAML support
//   - All debugging/tuning features enabled
//
// Usage:
//   struct SearchConfigData {
//       CONFIG_CONST bool USE_LMR = true;
//       CONFIG_CONST int LMR_MIN_DEPTH = 2;
//   };
//
//=============================================================================

#ifdef FRANKYCPP_PRODUCTION

  // Production build: config values are compile-time constants
  #define CONFIG_CONST constexpr
  #define CONFIG_MUTABLE constexpr  // Even "mutable" ones are frozen
  
  // Marker for code that should only exist in development
  #define DEV_ONLY(code)
  #define PROD_ONLY(code) code
  
  // Config is frozen in production
  #define IS_CONFIG_FROZEN true

#else

  // Development build: config values are mutable
  #define CONFIG_CONST
  #define CONFIG_MUTABLE mutable
  
  // Development-only code
  #define DEV_ONLY(code) code
  #define PROD_ONLY(code)
  
  // Config is mutable in development
  #define IS_CONFIG_FROZEN false

#endif

// Essential configs - ALWAYS mutable even in production (TT size, paths, etc.)
#define CONFIG_ESSENTIAL

#endif // FRANKYCPP_CONFIG_MODE_H
```

### SearchConfigData.h (Simplified)

```cpp
#ifndef FRANKYCPP_SEARCHCONFIGDATA_H
#define FRANKYCPP_SEARCHCONFIGDATA_H

#include "config/ConfigMode.h"
#include <array>
#include <string>

struct SearchConfigData {
    //=========================================================================
    // ESSENTIAL - Always mutable (user must be able to change these)
    //=========================================================================
    CONFIG_ESSENTIAL int TT_SIZE_MB = 64;
    CONFIG_ESSENTIAL int MOVE_OVERHEAD_MS = 10;
    CONFIG_ESSENTIAL bool USE_PONDER = true;
    CONFIG_ESSENTIAL std::string BOOK_PATH = "./books/book.txt";
    CONFIG_ESSENTIAL std::string BOOK_TYPE = "SIMPLE";
    CONFIG_ESSENTIAL std::string TB_PATH = "";
    CONFIG_ESSENTIAL bool USE_PAWN_TT = true;
    CONFIG_ESSENTIAL int PAWN_TT_SIZE_MB = 4;

    //=========================================================================
    // CORE SEARCH - Frozen in production
    //=========================================================================
    CONFIG_CONST bool USE_ALPHABETA = true;
    CONFIG_CONST bool USE_PVS = true;
    CONFIG_CONST bool USE_ASP = true;
    CONFIG_CONST bool USE_QUIESCENCE = true;

    //=========================================================================
    // TRANSPOSITION TABLE - Frozen in production
    //=========================================================================
    CONFIG_CONST bool USE_TT = true;
    CONFIG_CONST bool USE_TT_VALUE = true;
    CONFIG_CONST bool USE_EVAL_TT = true;
    CONFIG_CONST bool USE_QS_TT = true;

    //=========================================================================
    // LMR - Late Move Reductions - Frozen in production
    //=========================================================================
    CONFIG_CONST bool USE_LMR = true;
    CONFIG_CONST int LMR_MIN_DEPTH = 2;
    CONFIG_CONST int LMR_MIN_MOVES = 2;
    CONFIG_CONST bool LMR_USE_LOG_FORMULA = true;
    CONFIG_CONST double LMR_LOG_BASE_DIV = 1.25;
    CONFIG_CONST bool USE_LMR_IMPROVING = true;
    CONFIG_CONST int LMR_IMPROVING_REDUCTION = 1;
    CONFIG_CONST bool USE_LMR_HISTORY = true;
    CONFIG_CONST int LMR_HISTORY_DIVISOR = 8192;
    CONFIG_CONST bool USE_LMR_CUTNODE = true;
    CONFIG_CONST int LMR_CUTNODE_REDUCTION = 2;

    //=========================================================================
    // NULL MOVE PRUNING - Frozen in production
    //=========================================================================
    CONFIG_CONST bool USE_NMP = true;
    CONFIG_CONST int NMP_DEPTH = 3;
    CONFIG_CONST int NMP_REDUCTION = 2;
    CONFIG_CONST bool USE_NMP_VERIFY = true;
    CONFIG_CONST bool USE_NMP_IMPROVING = true;
    CONFIG_CONST int NMP_IMPROVING_REDUCTION = 1;

    //=========================================================================
    // FUTILITY PRUNING - Frozen in production
    //=========================================================================
    CONFIG_CONST bool USE_FP = true;
    CONFIG_CONST bool USE_QFP = true;
    CONFIG_CONST bool USE_FP_IMPROVING = true;
    CONFIG_CONST int FP_IMPROVING_MARGIN = 80;
    CONFIG_CONST std::array<int, 7> FP_MARGIN = {0, 100, 200, 300, 500, 900, 1200};

    //=========================================================================
    // OTHER PRUNING - Frozen in production
    //=========================================================================
    CONFIG_CONST bool USE_RAZORING = true;
    CONFIG_CONST int RAZOR_MARGIN = 531;
    CONFIG_CONST bool USE_RFP = true;
    CONFIG_CONST bool USE_RFP_IMPROVING = true;
    CONFIG_CONST int RFP_IMPROVING_MARGIN = 40;
    CONFIG_CONST std::array<int, 4> RFP_MARGIN = {0, 200, 400, 800};
    CONFIG_CONST bool USE_MDP = true;
    CONFIG_CONST bool USE_LMP = true;
    CONFIG_CONST bool USE_LMP_IMPROVING = true;
    CONFIG_CONST std::array<int, 16> LMP_MOVES = {0, 7, 9, 11, 13, 15, 17, 19, 22, 24, 27, 29, 32, 35, 38, 41};

    //=========================================================================
    // EXTENSIONS - Frozen in production
    //=========================================================================
    CONFIG_CONST bool USE_EXTENSIONS = true;
    CONFIG_CONST bool USE_CHECK_EXT = true;
    CONFIG_CONST bool USE_THREAT_EXT = true;
    CONFIG_CONST bool USE_SINGULAR_EXT = true;

    //=========================================================================
    // IID/IIR - Frozen in production
    //=========================================================================
    CONFIG_CONST bool USE_IID = false;
    CONFIG_CONST bool USE_IIR = true;
    CONFIG_CONST int IIR_DEPTH = 4;
    CONFIG_CONST int IIR_REDUCTION = 2;

    // ... more configs following same pattern ...
};

#endif // FRANKYCPP_SEARCHCONFIGDATA_H
```

### ConfigManager Changes

```cpp
// src/config/ConfigManager.h

class ConfigManager {
public:
    static ConfigManager& instance();

#ifdef FRANKYCPP_PRODUCTION
    //=========================================================================
    // PRODUCTION: Return constexpr struct (all values are compile-time)
    //=========================================================================
    
    // Returns a constexpr copy - optimizer will inline all accesses
    static constexpr SearchConfigData search() { 
        return SearchConfigData{}; 
    }
    
    static constexpr EvalConfigData eval() { 
        return EvalConfigData{}; 
    }
    
    // No-op in production (essential configs handled separately)
    void loadFromYaml(const std::string&) {}
    
#else
    //=========================================================================
    // DEVELOPMENT: Return mutable references
    //=========================================================================
    
    SearchConfigData& search() { return searchConfig_; }
    const SearchConfigData& search() const { return searchConfig_; }
    
    EvalConfigData& eval() { return evalConfig_; }
    const EvalConfigData& eval() const { return evalConfig_; }
    
    void loadFromYaml(const std::string& path);
    
private:
    SearchConfigData searchConfig_;
    EvalConfigData evalConfig_;
    
#endif

    //=========================================================================
    // ESSENTIAL configs - Always mutable, even in production
    //=========================================================================
    struct EssentialConfig {
        int ttSizeMB = 64;
        int moveOverheadMs = 10;
        bool usePonder = true;
        std::string bookPath = "./books/book.txt";
        std::string bookType = "SIMPLE";
        std::string tbPath = "";
        bool usePawnTT = true;
        int pawnTTSizeMB = 4;
    };
    
    EssentialConfig& essential() { return essentialConfig_; }
    const EssentialConfig& essential() const { return essentialConfig_; }
    
private:
    EssentialConfig essentialConfig_;
};
```

### ConfigRegistry Changes

The key insight: since `CONFIG_CONST` controls whether values are `constexpr`, we use the **same compile flag** to control registry visibility. Non-essential configs simply don't get registered in production builds.

```cpp
// src/config/ConfigRegistry.cpp

void ConfigRegistry::initializeSearchDefinitions() {
    // Helper to get default value from struct - single source of truth!
    auto defaultFrom = [](auto memberPtr) {
        return configToString(SearchConfigData{}.*memberPtr);
    };

    //=========================================================================
    // ESSENTIAL configs - Always registered (both builds)
    //=========================================================================
    definitions_.push_back({
        .name = "TT_SIZE_MB",
        .uciName = "Hash",
        .description = "Transposition table size (MB)",
        .valueType = Int,
        .domain = Search,
        .defaultValue = defaultFrom(&SearchConfigData::TT_SIZE_MB),
        .minValue = 1,
        .maxValue = 65536,
        .exposure = {.uci = true, .yaml = true, .display = true},
        .getter = essentialGetter(&EssentialConfig::ttSizeMB),
        .setter = essentialSetter(&EssentialConfig::ttSizeMB, parseInt)
    });

    definitions_.push_back({
        .name = "TB_PATH",
        .uciName = "SyzygyPath",
        // ... essential config ...
    });

    definitions_.push_back({
        .name = "MOVE_OVERHEAD_MS",
        .uciName = "Move Overhead",
        // ... essential config ...
    });

    // ... more essential configs ...

#ifndef FRANKYCPP_PRODUCTION
    //=========================================================================
    // NON-ESSENTIAL configs - Development builds only
    // In production, these simply don't exist in the registry
    //=========================================================================
    
    // LMR
    definitions_.push_back({
        .name = "USE_LMR",
        .uciName = "Use LMR",
        .description = "Enable Late Move Reductions",
        .valueType = Bool,
        .domain = Search,
        .defaultValue = defaultFrom(&SearchConfigData::USE_LMR),
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
        .defaultValue = defaultFrom(&SearchConfigData::LMR_MIN_DEPTH),
        .minValue = 1,
        .maxValue = 10,
        .exposure = {.uci = true, .yaml = true, .display = true},
        .getter = searchGetter(&SearchConfigData::LMR_MIN_DEPTH),
        .setter = searchSetter(&SearchConfigData::LMR_MIN_DEPTH, parseInt)
    });

    // NMP
    definitions_.push_back({
        .name = "USE_NMP",
        .uciName = "Use NMP",
        // ... etc ...
    });

    // ... all other non-essential configs ...

#endif // FRANKYCPP_PRODUCTION
}
```

**Result:**
- **Development:** All ~50 configs visible in UCI, YAML, `--show-config`
- **Production:** Only ~8 essential configs visible (Hash, SyzygyPath, etc.)
- **No runtime checks** - configs simply don't exist in production registry
- **Default values** come from struct via `defaultFrom()` - single source of truth!
```

### Search.cpp Usage (Unchanged!)

The beauty of this approach is that **Search.cpp code doesn't change**:

```cpp
// This works in BOTH builds:
if (SearchConfig.USE_LMR 
    && depth >= SearchConfig.LMR_MIN_DEPTH
    && movesSearched >= SearchConfig.LMR_MIN_MOVES) {
    // LMR logic
}

// In DEVELOPMENT:
//   - SearchConfig.USE_LMR is a runtime bool
//   - Branch is evaluated at runtime
//
// In PRODUCTION:
//   - SearchConfig.USE_LMR is constexpr true
//   - Compiler sees: if (true && depth >= 2 && movesSearched >= 2)
//   - Dead branch elimination removes the check entirely
```

---

## How Compiler Optimization Works

### Development Build

```cpp
// SearchConfigData struct has normal members
struct SearchConfigData {
    bool USE_LMR = true;  // Runtime mutable
    int LMR_MIN_DEPTH = 2;
};

// ConfigManager returns mutable reference
SearchConfigData& config = ConfigManager::instance().search();

// Code checks runtime value
if (config.USE_LMR && depth >= config.LMR_MIN_DEPTH) {
    // Branch evaluated at runtime
}
```

### Production Build

```cpp
// SearchConfigData struct has constexpr members
struct SearchConfigData {
    constexpr bool USE_LMR = true;  // Compile-time constant
    constexpr int LMR_MIN_DEPTH = 2;
};

// ConfigManager returns constexpr copy
constexpr SearchConfigData config = ConfigManager::search();

// Compiler sees constant expression
if (true && depth >= 2) {  // Simplified by compiler
    // No branch needed for USE_LMR check
}
```

**GCC/Clang/MSVC all optimize this to:**
```asm
; No comparison for USE_LMR - it's always true
cmp depth, 2
jl .skip_lmr
; LMR code here
.skip_lmr:
```

---

## Statistics Macros (Still Needed)

For statistics, we still need macros because we want to **completely eliminate** the increment operation:

```cpp
// src/config/ConfigMode.h (additions)

#ifdef FRANKYCPP_PRODUCTION
  #define STAT_INC(counter) ((void)0)
  #define STAT_ADD(counter, val) ((void)0)
  #define STAT_SET(counter, val) ((void)0)
#else
  #define STAT_INC(counter) (++(counter))
  #define STAT_ADD(counter, val) ((counter) += (val))
  #define STAT_SET(counter, val) ((counter) = (val))
#endif

// Essential stats - always collected (for UCI info, NPS)
#define ESSENTIAL_STAT_INC(counter) (++(counter))
#define ESSENTIAL_STAT_ADD(counter, val) ((counter) += (val))
```

---

## Comparison: X-Macro vs Constexpr Approach

| Aspect                           | X-Macro Approach            | Constexpr Approach    |
|----------------------------------|-----------------------------|-----------------------|
| **Complexity**                   | High (meta-programming)     | Low (simple macros)   |
| **Files to edit for new config** | 1 (definitions file)        | 2 (struct + registry) |
| **Default value duplication**    | None (generated)            | Can be eliminated*    |
| **IDE support**                  | Poor                        | Good                  |
| **Error messages**               | Cryptic                     | Clear                 |
| **Risk level**                   | High (complete rewrite)     | Low (incremental)     |
| **Search.cpp changes**           | Yes (FEATURE_ENABLED macro) | No                    |
| **Struct readability**           | Generated (hard to read)    | Normal C++            |
| **Gradual migration**            | Difficult                   | Easy                  |
| **Rollback difficulty**          | High                        | Low                   |

*Default duplication eliminated using `defaultFrom(&SearchConfigData::MEMBER)` pattern

---

## Implementation Plan

### Phase 1: Infrastructure (Low Risk)

**Goal:** Create the macro header and CMake option without any behavior change.

- [ ] Create `src/config/ConfigMode.h`:
  ```cpp
  #ifdef FRANKYCPP_PRODUCTION
    #define CONFIG_CONST constexpr
    #define STAT_INC(counter) ((void)0)
    // ... etc
  #else
    #define CONFIG_CONST
    #define STAT_INC(counter) (++(counter))
    // ... etc
  #endif
  #define CONFIG_ESSENTIAL  // Always empty - just documentation
  ```
- [ ] Add to `CMakeLists.txt`:
  ```cmake
  option(FRANKYCPP_PRODUCTION "Production build with frozen configs" OFF)
  if(FRANKYCPP_PRODUCTION)
    target_compile_definitions(FrankyCPP_lib PRIVATE FRANKYCPP_PRODUCTION)
  endif()
  ```
- [ ] Add CMake presets: `win-release-production`, `wsl-release-production`

**Acceptance:** Project compiles unchanged (macros expand to nothing)

---

### Phase 2: Migrate SearchConfigData (Low Risk)

**Goal:** Add macros to struct members - no behavior change in development.

- [ ] Add `#include "config/ConfigMode.h"` to SearchConfigData.h
- [ ] Add `CONFIG_CONST` prefix to all non-essential members
- [ ] Add `CONFIG_ESSENTIAL` prefix to essential members (TT_SIZE, paths, etc.)
- [ ] Verify development build compiles and works as before
- [ ] Verify production build compiles (members become constexpr)

**Example:**
```cpp
// Before
bool USE_LMR = true;

// After  
CONFIG_CONST bool USE_LMR = true;
```

**Acceptance:** Both builds compile, all tests pass in development

---

### Phase 3: ConfigManager Conditional (Medium Risk)

**Goal:** In production, return constexpr struct; in development, return mutable reference.

- [ ] Add `#ifdef FRANKYCPP_PRODUCTION` block to ConfigManager
- [ ] Production path: `static constexpr SearchConfigData search() { return {}; }`
- [ ] Development path: Keep existing `SearchConfigData& search()` 
- [ ] Handle essential configs (always mutable) - keep in separate member or same struct

**Acceptance:** Both builds work correctly

---

### Phase 4: ConfigRegistry Conditional (Low Risk)

**Goal:** Non-essential configs invisible in production UCI/YAML.

- [ ] Add `defaultFrom()` helper to derive defaults from struct
- [ ] Wrap non-essential config registrations in `#ifndef FRANKYCPP_PRODUCTION`
- [ ] Essential configs always registered (Hash, SyzygyPath, etc.)

**Result:**
- Development: ~50 UCI options, full YAML support
- Production: ~8 UCI options (essentials only), minimal YAML

**Acceptance:** UCI `uci` command shows only essentials in production

---

### Phase 5: Statistics Macros (Low Risk)

**Goal:** Eliminate stat collection overhead in production.

- [ ] Ensure `STAT_INC`, `STAT_ADD`, etc. are in ConfigMode.h
- [ ] Migrate stats in Search.cpp: `statistics.xxx++` → `STAT_INC(statistics.xxx)`
- [ ] Keep essential stats (nodesVisited, depth) as `ESSENTIAL_STAT_INC`
- [ ] Verify stats still collected in development
- [ ] Verify stats eliminated in production

**Acceptance:** Production build has zero stat collection code

---

### Phase 6: Verification & Benchmarking

**Goal:** Confirm the approach works and measure improvement.

- [ ] Check assembly output (Godbolt or local) - verify dead code elimination
- [ ] Benchmark NPS: development vs production (expect 1-3% improvement)
- [ ] Run full test suite in both builds
- [ ] Verify identical search results (same bestmove for same position)
- [ ] Test UCI protocol in both builds
- [ ] Test YAML loading in both builds

**Acceptance:** All tests pass, measurable NPS improvement

---

## Summary: What Changes Where

| File | Change | Risk |
|------|--------|------|
| `ConfigMode.h` | **NEW** - defines CONFIG_CONST, STAT_* macros | Low |
| `CMakeLists.txt` | Add FRANKYCPP_PRODUCTION option | Low |
| `CMakePresets.json` | Add production presets | Low |
| `SearchConfigData.h` | Add CONFIG_CONST prefix to members | Low |
| `EvalConfigData.h` | Add CONFIG_CONST prefix to members | Low |
| `ConfigManager.h/cpp` | Conditional return type | Medium |
| `ConfigRegistry.cpp` | `#ifndef` around non-essentials | Low |
| `Search.cpp` | `statistics.x++` → `STAT_INC(statistics.x)` | Low |

**Total:** ~8 files changed, most changes are mechanical/low-risk

---

## Advantages of This Approach

1. **Simplicity** - Just a few macros, no meta-programming
2. **Gradual migration** - Can add `CONFIG_CONST` one member at a time
3. **Low risk** - Easy to rollback (just remove macros)
4. **IDE friendly** - Struct is normal C++, IDE understands it
5. **Search.cpp unchanged** - No `FEATURE_ENABLED()` wrapper needed
6. **Proven pattern** - Same approach used by STL, Boost, game engines
7. **Clear struct** - Can read SearchConfigData.h and understand all configs
8. **Compiler does the work** - We just mark things `constexpr`, optimizer handles the rest
9. **Single source of truth for defaults** - `defaultFrom()` reads from struct

---

## Disadvantages / Tradeoffs

1. **Two places for new config** - Still need struct member + registry entry
   - Mitigated by `defaultFrom()` pattern (default only in struct)
   
2. **Manual essential marking** - Must remember `CONFIG_ESSENTIAL` vs `CONFIG_CONST`
   - Mitigated by clear grouping in struct
   
3. **Registry still exists in production** - Some code bloat
   - Mitigated by `#ifdef` around non-essential registrations
   
4. **Stats still need macros** - Can't use constexpr for side effects
   - This is unavoidable with any approach

---

## Open Questions

1. **Naming:** `FRANKYCPP_PRODUCTION` vs `FRANKYCPP_RELEASE` vs `FRANKYCPP_TOURNAMENT`?
   - Recommendation: `FRANKYCPP_PRODUCTION` (clear intent)

2. **Essential config struct:** Separate struct or same struct with different macro?
   - Recommendation: Same struct, `CONFIG_ESSENTIAL` macro (simpler)

3. **constexpr arrays:** Do `std::array` members work with `constexpr`?
   - Yes, `std::array` is fully constexpr in C++20

4. **String members:** Can `std::string` be constexpr?
   - In C++20 yes, but only for compile-time operations
   - For paths (runtime strings), use `CONFIG_ESSENTIAL`

---

## References

- MSVC STL `yvals_core.h` - `_CONSTEXPR20` pattern
- Abseil `config.h` - Conditional feature macros
- Godot `engine.h` - `#ifdef TOOLS_ENABLED` pattern
- C++20 constexpr improvements - P1064R0, P1002R1

---

## Decision

**This is the RECOMMENDED approach**, replacing the X-macro plan.

### Why This Over X-Macros:

| Aspect | X-Macro | Constexpr (This) |
|--------|---------|------------------|
| Complexity | High | **Low** |
| Risk | High | **Low** |
| Search.cpp changes | Required | **None** |
| IDE support | Poor | **Good** |
| Gradual migration | Hard | **Easy** |
| Rollback | Hard | **Easy** |

### Key Insight

The X-macro approach solves a problem we don't actually have. We wanted:
1. Single source of truth for defaults → **Solved by `defaultFrom()`**
2. Zero runtime overhead → **Solved by `constexpr`**
3. Hide configs in production → **Solved by `#ifndef`**

We don't need meta-programming to achieve any of these goals.

### Bottom Line

**90% of the benefit with 20% of the complexity.**

The constexpr approach:
- Uses proven STL patterns
- Requires minimal code changes
- Lets the compiler do the work
- Is easy to understand and maintain
