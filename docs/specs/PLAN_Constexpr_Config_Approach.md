# Constexpr Config Approach (Recommended Plan)

**Status:** Recommended  
**Created:** 2026-02-23  
**Last Updated:** 2026-02-24  
**Author:** Frank Kopp

---

## Project Metadata

| Field                | Value                                         |
|----------------------|-----------------------------------------------|
| **Branch**           | `feature/compile-time-stripping` ✅ exists     |
| **Risk Level**       | 🟡 Medium - Struct changes, phased rollout    |
| **Estimated Effort** | 2-3 sessions                                  |
| **Rollback**         | Remove macros, revert to plain members        |

### Phase Status Tracker

| Phase                               | Status        | Notes                                 |
|-------------------------------------|---------------|---------------------------------------|
| Phase 1: Infrastructure             | ⬜ Not Started | ConfigMode.h, CMake option, presets   |
| Phase 2: SearchConfigData Migration | ⬜ Not Started | CONFIG_CONST added to struct members  |
| Phase 3: ConfigManager Conditional  | ⬜ Not Started | Conditional accessors                 |
| Phase 4: ConfigRegistry Conditional | ⬜ Not Started | essentialSetter vs frozenSetter       |
| Phase 5: Statistics Macros          | ⬜ Not Started | STAT_INC etc.                         |
| Phase 6: Verification               | ⬜ Not Started | Benchmark, test both builds           |
| Phase 7: CLI Tools & Unit Tests     | ⬜ Not Started | Production guards, test handling      |

**Status Legend:** ⬜ Not Started | 🔄 In Progress | ✅ Complete | ❌ Blocked

---

## Requirements Summary

### Goal
Provide a compile-time mechanism to strip non-essential configuration options and statistics from production builds, yielding smaller binaries and faster execution while maintaining full functionality in development builds.

### Core Requirements

**1. Compile-Time Build Mode Flag**
- A single CMake flag (`FRANKYCPP_PRODUCTION`) controls the build mode
- Development builds (default): Full functionality, all configs mutable, all stats collected
- Production builds: Only essential configs/stats remain, non-essential code completely eliminated by compiler

**2. Essential vs Non-Essential Classification**

| Category    | Essential                                                                        | Non-Essential                                                                       |
|-------------|----------------------------------------------------------------------------------|-------------------------------------------------------------------------------------|
| **Configs** | Must be mutable at runtime (TT size, book path, pondering, tablebase path, etc.) | Tuning/debugging parameters (LMR coefficients, pruning thresholds, feature toggles) |
| **Stats**   | Required for UCI output (nodes, depth, seldepth, NPS, time, pv, score)           | Detailed debugging stats (beta cut distribution, TT hit rates, extension counts)    |

**3. Production Build Behavior**

- **Non-essential configs:**
  - Code paths using them are eliminated by compiler (dead code elimination via `constexpr`)
  - UCI: Non-essential options are **not registered** (invisible to GUIs)
  - YAML: Non-essential settings are **ignored** with a single summary log message on startup (e.g., "Production build: ignoring N non-essential config options from YAML")
  - `--show-config`: Non-essential configs shown as "frozen" or "n/a"

- **Non-essential stats:**
  - Increment/update operations compile to no-ops
  - Output functions (`str()`, `operator<<`) omit non-essential fields entirely

**4. Design Principles**

- **Simplicity:** Adding a new config requires changes in at most 2 places (struct member + registry entry)
- **Low redundancy:** Default values defined once in the struct, not duplicated
- **Easy reclassification:** Moving a config between essential/non-essential should require only:
  - Changing the macro prefix (`CONFIG_ESSENTIAL` ↔ `CONFIG_CONST`)
  - Moving the registry entry between `#ifdef` blocks (or changing setter type)
- **Easy maintenance:** Clear visual grouping of essential vs non-essential in struct

### Essential Stats (for UCI `info` output)
- `nodesVisited` — nodes searched
- `currentSearchDepth` / `currentExtraSearchDepth` — depth/seldepth
- Time tracking — for NPS calculation
- `currentBestRootMove` / `currentBestRootMoveValue` — bestmove/score
- `currentVariation` — principal variation (pv)

*(This list can be adjusted as needed)*

---

## Review Findings: Shortcomings and Implementation Risks

1. **`ConfigManager` production sketch is internally inconsistent**  
   The current snippet returns full `SearchConfigData`/`EvalConfigData` by value as `constexpr` while also introducing separately mutable `EssentialConfig`. This creates two sources of truth and an unclear read path for essential options.
   
   ✅ **Resolution:** Essential configs stay in `SearchConfigData`/`EvalConfigData` as instance members (marked `CONFIG_ESSENTIAL`). No separate `EssentialConfig` struct. Single source of truth.

2. **`CONFIG_OVERRIDE` behavior is contradictory in this document**  
   One section says non-essential overrides in production "silently compile but have no effect", another says they should fail to compile. This must be decided explicitly before implementation.
   
   ✅ **Resolution:** Production builds **fail to compile** if code tries to override non-essential configs. This is the correct behavior - you cannot assign to `static constexpr` members. This catches mistakes at compile time.

3. **Default-value extraction (`defaultFrom`) is fragile with `CONFIG_CONST`**  
   The pointer-based helper assumes instance members. If a setting is moved between essential/non-essential, this helper can break or require type-specific handling.
   
   ✅ **Resolution:** Use direct value access instead of pointer-to-member. Since defaults are compile-time constants, just use the value directly: `defaultFrom(SearchConfigData::USE_LMR)` or inline the default.

4. **Risk level is likely underestimated**  
   Classification mistakes (essential vs non-essential), registry gating, and test/CLI behavior changes can cause regressions in UCI compatibility and tuning workflows. This is closer to medium risk.
   
   ✅ **Resolution:** Risk level updated to Medium. Phased rollout with verification at each step mitigates this.

5. **No explicit migration guardrails for accidental freezing**  
   The plan lacks a concrete validation checklist that prevents accidentally freezing options that are still required at runtime (e.g., GUI-controlled tournament settings).
   
   ✅ **Resolution:** Add `static_assert` guards in production builds to ensure essential configs remain non-static (mutable). Compile fails if essential config is accidentally marked `CONFIG_CONST`.

6. **Tooling/test impact is broader than currently represented**  
   Production-only and development-only behavior split needs a clear CI matrix and explicit pass/fail expectations per target; otherwise breakages may go unnoticed.
   
   ⏳ **Resolution:** Deferred to future task. Focus on local build first. CI matrix to be added after local implementation is verified working.

**Recommendation:** ~~Resolve the `ConfigManager`/`EssentialConfig` model first, define one authoritative `CONFIG_OVERRIDE` rule, and add a short "must-stay-mutable" allowlist before coding.~~ ✅ All resolved above.

---

## Overview

This document describes the **recommended approach** for compile-time feature stripping. Instead of complex X-macro metaprogramming, we use conditional `static constexpr` via simple macros.

**Key Insight:** C++ requires `constexpr` data members to be `static`. In production builds, non-essential config members become `static constexpr`, making them compile-time constants that enable dead code elimination. In development builds, they remain non-static instance members for runtime mutability.

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
// ConfigMode.h - Conditional static constexpr for configuration values
//=============================================================================
//
// In PRODUCTION builds (-DFRANKYCPP_PRODUCTION):
//   - Config values become static constexpr (compile-time constants)
//   - Compiler eliminates dead branches, inlines values
//   - No runtime config changes possible
//   - UCI/YAML config loading disabled for frozen options
//
// In DEVELOPMENT builds (default):
//   - Config values are non-static mutable members (runtime changeable)
//   - Full UCI/YAML support
//   - All debugging/tuning features enabled
//
// NOTE: C++ requires constexpr data members to be static. This macro
// expands to "static constexpr" in production, empty in development.
//
// Usage:
//   struct SearchConfigData {
//       CONFIG_CONST bool USE_LMR = true;
//       CONFIG_CONST int LMR_MIN_DEPTH = 2;
//   };
//
//=============================================================================

#ifdef FRANKYCPP_PRODUCTION

  // Production build: config values are static compile-time constants
  #define CONFIG_CONST static constexpr
  
  // Marker for code that should only exist in development
  #define DEV_ONLY(code)
  #define PROD_ONLY(code) code
  
  // Config is frozen in production
  #define IS_CONFIG_FROZEN true

#else

  // Development build: config values are non-static mutable members
  #define CONFIG_CONST
  
  // Development-only code
  #define DEV_ONLY(code) code
  #define PROD_ONLY(code)
  
  // Config is mutable in development
  #define IS_CONFIG_FROZEN false

#endif

// Essential configs - ALWAYS non-static mutable even in production
// (TT size, paths, etc. that users must be able to change)
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

//=============================================================================
// SearchConfigData - Search configuration parameters
//=============================================================================
//
// CONFIG_CONST members:
//   - Development: Normal instance members (mutable at runtime)
//   - Production: static constexpr (compile-time constants, shared across instances)
//
// CONFIG_ESSENTIAL members:
//   - Always instance members (mutable at runtime in all builds)
//
// In production, accessing CONFIG_CONST members like `config.USE_LMR` works
// because C++ allows accessing static members through an instance. The compiler
// optimizes this to direct access of the static constant.
//=============================================================================

struct SearchConfigData {
    //=========================================================================
    // ESSENTIAL - Always mutable instance members
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

//=============================================================================
// ESSENTIAL CONFIG GUARDRAILS
//=============================================================================
// These static_asserts ensure essential configs are never accidentally frozen.
// If someone mistakenly uses CONFIG_CONST instead of CONFIG_ESSENTIAL,
// the build will fail with a clear error message.
//
// Only needed in production builds (in development, all members are instance members)
//=============================================================================

#ifdef FRANKYCPP_PRODUCTION

// Helper to check if a member is static (would indicate accidental freezing)
template<typename T, typename M>
constexpr bool is_instance_member(M T::*) { return true; }

// Search essential configs - must remain mutable
static_assert(is_instance_member(&SearchConfigData::TT_SIZE_MB),
              "TT_SIZE_MB must be CONFIG_ESSENTIAL (Hash size must be configurable)");
static_assert(is_instance_member(&SearchConfigData::MOVE_OVERHEAD_MS),
              "MOVE_OVERHEAD_MS must be CONFIG_ESSENTIAL (Time control requires this)");
static_assert(is_instance_member(&SearchConfigData::USE_PONDER),
              "USE_PONDER must be CONFIG_ESSENTIAL (UCI Ponder option)");
static_assert(is_instance_member(&SearchConfigData::BOOK_PATH),
              "BOOK_PATH must be CONFIG_ESSENTIAL (Opening book path must be configurable)");
static_assert(is_instance_member(&SearchConfigData::BOOK_TYPE),
              "BOOK_TYPE must be CONFIG_ESSENTIAL (Opening book type must be configurable)");
static_assert(is_instance_member(&SearchConfigData::TB_PATH),
              "TB_PATH must be CONFIG_ESSENTIAL (SyzygyPath must be configurable)");

// Eval essential configs - must remain mutable
static_assert(is_instance_member(&EvalConfigData::USE_PAWN_TT),
              "USE_PAWN_TT must be CONFIG_ESSENTIAL");
static_assert(is_instance_member(&EvalConfigData::PAWN_TT_SIZE_MB),
              "PAWN_TT_SIZE_MB must be CONFIG_ESSENTIAL");

#endif // FRANKYCPP_PRODUCTION

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
    // PRODUCTION: Return const reference to struct with mixed members
    //=========================================================================
    // - CONFIG_CONST members are static constexpr (accessed via instance syntax)
    // - CONFIG_ESSENTIAL members are normal instance members (mutable)
    //
    // Returning const ref prevents accidental modification attempts.
    // Essential configs are modified via dedicated setters or YAML loading.
    
    const SearchConfigData& search() const { return searchConfig_; }
    const EvalConfigData& eval() const { return evalConfig_; }
    
    // YAML loading only affects essential configs in production
    void loadFromYaml(const std::string& path);
    
#else
    //=========================================================================
    // DEVELOPMENT: Return mutable references
    //=========================================================================
    
    SearchConfigData& search() { return searchConfig_; }
    const SearchConfigData& search() const { return searchConfig_; }
    
    EvalConfigData& eval() { return evalConfig_; }
    const EvalConfigData& eval() const { return evalConfig_; }
    
    void loadFromYaml(const std::string& path);
    
#endif

    // applyOverrides only available in development
    // Production builds FAIL TO COMPILE if non-essential configs are modified
#ifndef FRANKYCPP_PRODUCTION
    template<typename F>
    void applyOverrides(F&& func) {
        func(searchConfig_, evalConfig_);
    }
#endif

private:
    SearchConfigData searchConfig_;
    EvalConfigData evalConfig_;
};
```

**Key Design Decision:** Essential configs remain in `SearchConfigData`/`EvalConfigData` as instance members (marked `CONFIG_ESSENTIAL`). No separate `EssentialConfig` struct. This keeps a single source of truth and maintains the existing access pattern: `ConfigManager::instance().search().TT_SIZE_MB`.

### ConfigRegistry Changes

The key insight: since `CONFIG_CONST` controls whether values are `constexpr`, we use the **same compile flag** to control registry visibility. Non-essential configs simply don't get registered in production builds.

```cpp
// src/config/ConfigRegistry.cpp

void ConfigRegistry::initializeSearchDefinitions() {
    // For default values, just use the value directly from the struct.
    // Works for both instance members (essential) and static constexpr (non-essential).
    // No pointer-to-member complexity needed.

    //=========================================================================
    // ESSENTIAL configs - Always registered (both builds)
    //=========================================================================
    definitions_.push_back({
        .name = "TT_SIZE_MB",
        .uciName = "Hash",
        .description = "Transposition table size (MB)",
        .valueType = Int,
        .domain = Search,
        .defaultValue = std::to_string(SearchConfigData{}.TT_SIZE_MB),  // Instance member
        .minValue = 1,
        .maxValue = 65536,
        .exposure = {.uci = true, .yaml = true, .display = true},
        .getter = [](const auto& s, const auto&) { return std::to_string(s.TT_SIZE_MB); },
        .setter = [](auto& s, auto&, const std::string& v) { s.TT_SIZE_MB = parseInt(v); }
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
        .defaultValue = std::to_string(SearchConfigData{}.USE_LMR),  // Instance member in dev
        .exposure = {.uci = true, .yaml = true, .display = true},
        .getter = [](const auto& s, const auto&) { return std::to_string(s.USE_LMR); },
        .setter = [](auto& s, auto&, const std::string& v) { s.USE_LMR = parseBool(v); }
    });

    definitions_.push_back({
        .name = "LMR_MIN_DEPTH",
        .uciName = "LMR Min Depth",
        .description = "Minimum depth for LMR",
        .valueType = Int,
        .domain = Search,
        .defaultValue = std::to_string(SearchConfigData{}.LMR_MIN_DEPTH),
        .minValue = 1,
        .maxValue = 10,
        .exposure = {.uci = true, .yaml = true, .display = true},
        .getter = [](const auto& s, const auto&) { return std::to_string(s.LMR_MIN_DEPTH); },
        .setter = [](auto& s, auto&, const std::string& v) { s.LMR_MIN_DEPTH = parseInt(v); }
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
//   - SearchConfig.USE_LMR is a runtime bool (instance member)
//   - Branch is evaluated at runtime
//
// In PRODUCTION:
//   - SearchConfig.USE_LMR is static constexpr true
//   - C++ allows accessing static members via instance syntax
//   - Compiler sees: if (true && depth >= 2 && movesSearched >= 2)
//   - Dead branch elimination removes the check entirely
```

---

## How Compiler Optimization Works

### Development Build

```cpp
// SearchConfigData struct has normal instance members
struct SearchConfigData {
    bool USE_LMR = true;  // Runtime mutable instance member
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
// SearchConfigData struct has static constexpr members
struct SearchConfigData {
    static constexpr bool USE_LMR = true;  // Compile-time constant
    static constexpr int LMR_MIN_DEPTH = 2;
};

// Access through instance still works (C++ allows static member access via instance)
SearchConfigData config{};  // Instance only holds ESSENTIAL members
if (config.USE_LMR && depth >= config.LMR_MIN_DEPTH) {
    // Compiler sees: if (true && depth >= 2)
}

// Or equivalently:
if (SearchConfigData::USE_LMR && depth >= SearchConfigData::LMR_MIN_DEPTH) {
    // Same result - compiler inlines the constants
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

### Why Static Works

C++ allows accessing static members through an instance: `obj.staticMember` is equivalent to `ClassName::staticMember`. This means existing code like `SearchConfig.USE_LMR` works unchanged - in development it's an instance member access, in production it's a static constexpr access through the instance syntax.

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

## CLI Tools Handling

Several CLI tools use `CONFIG_OVERRIDE` macros to modify search behavior. These need special consideration in production builds.

### Tools That Use CONFIG_OVERRIDE

| Tool          | Config Overrides   | Production Behavior               |
|---------------|--------------------|-----------------------------------|
| `--bench`     | `TT_SIZE_MB` only  | ✅ Works - TT_SIZE_MB is essential |
| `--testsuite` | `USE_BOOK = false` | ✅ Works - USE_BOOK is essential   |

**Note:** `SearchTreeSizeTest` (in `src/enginetest/`) uses many non-essential config overrides
but is not a CLI tool - it's part of the unit test infrastructure and handled in the Unit Test section.

### Solution: Tool-Specific Handling

**1. `--bench` (Benchmark.cpp)** - No changes needed
```cpp
// Already only modifies essential config
ConfigManager::instance().applyOverrides(
  [&](SearchConfigData& s, EvalConfigData&) {
    s.TT_SIZE_MB = config.hashSizeMB;  // Essential - always mutable
  });
```

**2. `--testsuite` (TestSuite.cpp)** - No changes needed
```cpp
// Already only modifies essential config
CONFIG_OVERRIDE(s.USE_BOOK = false;);  // Essential - always mutable
```

### CONFIG_OVERRIDE Macro Changes

In production builds, `applyOverrides()` is not available (see ConfigManager section above). This means:

1. **Code using CONFIG_OVERRIDE on non-essential configs will fail to compile in production**
2. This is **intentional** - it catches mistakes at compile time
3. Only code that modifies essential configs (like `--bench` and `--testsuite`) will compile

```cpp
// src/config/ConfigManager.h

#ifndef FRANKYCPP_PRODUCTION
// Development: Full CONFIG_OVERRIDE support
#define CONFIG_OVERRIDE_START() \
    ConfigManager::instance().applyOverrides( \
        [&]([[maybe_unused]] SearchConfigData& s, [[maybe_unused]] EvalConfigData& e) {
#define CONFIG_OVERRIDE_END() });
#define CONFIG_OVERRIDE(expr) \
    ConfigManager::instance().applyOverrides( \
        [&]([[maybe_unused]] SearchConfigData& s, [[maybe_unused]] EvalConfigData& e) { expr });
#else
// Production: CONFIG_OVERRIDE macros not defined
// Code that uses them will fail to compile - this is intentional!
// Only essential configs should be modified at runtime.
#endif
```

**Why compile error is the right choice:**
- Catches accidental use of CONFIG_OVERRIDE on non-essential configs
- No silent failures or unexpected behavior
- Forces developers to wrap non-essential overrides in `#ifndef FRANKYCPP_PRODUCTION`
- Clear error message points to the problem

---

## Unit Test Handling

Unit tests present a unique challenge: we want most tests to verify the engine works correctly in **both** development and production modes, but some tests are specifically about config modification which only makes sense in development.

### Test Categories

| Category               | Example Tests                                  | Production Behavior         |
|------------------------|------------------------------------------------|-----------------------------|
| **Core Functionality** | MoveGeneratorTest, PositionTest, EvaluatorTest | ✅ Must work in both modes   |
| **Search Behavior**    | SearchTest (basic search), TTTest              | ✅ Should work in both modes |
| **Config-Dependent**   | Tests that override USE_LMR, USE_NMP, etc.     | ❌ Development only          |
| **Config System**      | ConfigRegistryTest, ConfigManagerTest          | ❌ Development only          |
| **Statistics**         | Tests that check SearchStats counters          | ⚠️ May need adaptation      |

### Recommended Approach: Conditional Test Compilation

**1. Mark development-only tests with `#ifndef FRANKYCPP_PRODUCTION`**

```cpp
// test/engine/SearchTest.cpp

// These tests work in both modes
TEST_F(SearchTest, BasicSearchFindsGoodMove) {
  // ... test without config overrides ...
}

// These tests require config modification - development only
#ifndef FRANKYCPP_PRODUCTION

TEST_F(SearchTest, LMR_DisabledIncreasesNodes) {
  CONFIG_OVERRIDE(s.USE_LMR = false);
  // ... test that depends on disabling LMR ...
}

TEST_F(SearchTest, NMP_VerifyReducesZugzwangErrors) {
  CONFIG_OVERRIDE(s.USE_NMP_VERIFY = true);
  // ... test that depends on enabling NMP verify ...
}

#endif // FRANKYCPP_PRODUCTION
```

**2. Create separate test targets in CMake**

```cmake
# test/CMakeLists.txt

# Core tests - run in both development and production
set(CORE_TEST_SOURCES
    chesscore/MoveGeneratorTest.cpp
    chesscore/PositionTest.cpp
    chesscore/BitboardTest.cpp
    engine/TTTest.cpp
    engine/EvaluatorTest.cpp
    # ... etc
)

# Development-only tests - require config modification
set(DEV_ONLY_TEST_SOURCES
    config/ConfigRegistryTest.cpp
    config/ConfigManagerTest.cpp
    enginetest/SearchTreeSizeTest.cpp
    # ... etc
)

# Main test executable includes all tests
add_executable(FrankyCPP_Test
    ${CORE_TEST_SOURCES}
    ${DEV_ONLY_TEST_SOURCES}
)

# Optional: Separate executable for production-compatible tests only
if(FRANKYCPP_PRODUCTION)
    add_executable(FrankyCPP_Test_Production
        ${CORE_TEST_SOURCES}
    )
    target_compile_definitions(FrankyCPP_Test_Production PRIVATE FRANKYCPP_PRODUCTION)
endif()
```

### Test Files Requiring Modification

| Test File                | Issue                               | Solution                               |
|--------------------------|-------------------------------------|----------------------------------------|
| `SearchTreeSizeTest.cpp` | Overrides ~40 non-essential configs | Exclude from production                |
| `EngineSpeedTests.cpp`   | Uses CONFIG_OVERRIDE_START          | Wrap in `#ifndef FRANKYCPP_PRODUCTION` |
| `TablebaseTest.cpp`      | Overrides TB_PATH (essential)       | ✅ No changes needed                    |
| `TestSuite_Test.cpp`     | Overrides USE_BOOK (essential)      | ✅ No changes needed                    |
| `ConfigRegistryTest.cpp` | Tests config modification           | Exclude from production                |
| `ConfigManagerTest.cpp`  | Tests config modification           | Exclude from production                |

### Statistics Counter Tests

Tests that verify statistics counters need special handling:

```cpp
// Before
TEST_F(SearchTest, CheckStatisticsCounters) {
  search.startSearch(position, limits);
  search.waitWhileSearching();
  EXPECT_GT(search.getSearchStats().nodes, 0);
  EXPECT_GT(search.getSearchStats().betaCutoffs, 0);  // May be 0 in production!
}

// After - handle production mode
TEST_F(SearchTest, CheckStatisticsCounters) {
  search.startSearch(position, limits);
  search.waitWhileSearching();
  EXPECT_GT(search.getSearchStats().nodes, 0);  // Essential stat
#ifndef FRANKYCPP_PRODUCTION
  EXPECT_GT(search.getSearchStats().betaCutoffs, 0);  // Non-essential stat
#endif
}
```

### Verifying Both Modes Work

**TODO (Future Task):** Set up CI/CD to run tests in both modes. For now, focus on local build verification.

```yaml
# .github/workflows/test.yml (FUTURE - not implemented yet)
jobs:
  test-development:
    runs-on: ubuntu-latest
    steps:
      - name: Build (Development)
        run: cmake -B build -DFRANKYCPP_PRODUCTION=OFF
      - name: Test
        run: ctest --test-dir build

  test-production:
    runs-on: ubuntu-latest
    steps:
      - name: Build (Production)
        run: cmake -B build -DFRANKYCPP_PRODUCTION=ON
      - name: Test
        run: ctest --test-dir build
```

### Summary: Test Handling Rules

1. **Default:** Tests should work in both modes
2. **Config modification:** Wrap in `#ifndef FRANKYCPP_PRODUCTION`
3. **Non-essential stats:** Either skip check in production or use `ESSENTIAL_STAT` for important counters
4. **Development-only test files:** Exclude via CMake or `#ifndef` the entire file
5. **CI/CD:** *(Future task)* Run tests in both modes to ensure compatibility

---

## Comparison: X-Macro vs Static Constexpr Approach

| Aspect                           | X-Macro Approach            | Static Constexpr Approach |
|----------------------------------|-----------------------------|---------------------------|
| **Complexity**                   | High (meta-programming)     | Low (simple macros)       |
| **Files to edit for new config** | 1 (definitions file)        | 2 (struct + registry)     |
| **Default value duplication**    | None (generated)            | Can be eliminated*        |
| **IDE support**                  | Poor                        | Good                      |
| **Error messages**               | Cryptic                     | Clear                     |
| **Risk level**                   | High (complete rewrite)     | Low (incremental)         |
| **Search.cpp changes**           | Yes (FEATURE_ENABLED macro) | No                        |
| **Struct readability**           | Generated (hard to read)    | Normal C++                |
| **Gradual migration**            | Difficult                   | Easy                      |
| **Rollback difficulty**          | High                        | Low                       |
| **C++ requirement**              | N/A                         | Static for constexpr      |

*Default duplication eliminated using `defaultFrom(&SearchConfigData::MEMBER)` pattern

---

## Implementation Plan

### Phase 1: Infrastructure (Low Risk)

**Goal:** Create the macro header and CMake option without any behavior change.

- [ ] Create `src/config/ConfigMode.h`:
  ```cpp
  #ifdef FRANKYCPP_PRODUCTION
    #define CONFIG_CONST static constexpr
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
- [ ] Add static_assert guardrails for essential configs (see SearchConfigData example)
- [ ] Add `#include "config/ConfigMode.h"` to EvalConfigData.h
- [ ] Add `CONFIG_CONST`/`CONFIG_ESSENTIAL` to EvalConfigData members
- [ ] Add static_assert guardrails for essential eval configs
- [ ] Verify development build compiles and works as before
- [ ] ~~Verify production build compiles~~ (Expected to fail until Phase 3)

**Note:** Production build will NOT compile yet because ConfigManager still returns
mutable references. This is expected - Phase 3 fixes the ConfigManager.

**Example:**
```cpp
// Before
bool USE_LMR = true;

// After  
CONFIG_CONST bool USE_LMR = true;
```

**Acceptance:** Development build compiles, all tests pass

---

### Phase 3: ConfigManager Conditional (Medium Risk)

**Goal:** In production, return const reference (essential configs still mutable via setters); in development, return mutable reference.

- [ ] Add `#include "config/ConfigMode.h"` to ConfigManager.h
- [ ] Add `#ifdef FRANKYCPP_PRODUCTION` block to ConfigManager
- [ ] Production path: `const SearchConfigData& search() const` (const ref, essential members still mutable internally)
- [ ] Development path: Keep existing mutable `SearchConfigData& search()`
- [ ] Make `applyOverrides()` conditional (dev only - not defined in production)
- [ ] Make `CONFIG_OVERRIDE*` macros conditional (dev only - not defined in production)
- [ ] Verify development build still works
- [ ] ~~Verify production build works~~ (Expected to fail until Phase 4)

**Note:** Production build will NOT compile yet because ConfigRegistry setters try
to modify constexpr members. Phase 4 will make those setters conditional.

**Key Design:** Essential configs stay in `SearchConfigData`/`EvalConfigData` as instance members.
No separate `EssentialConfig` struct. Single source of truth.

**Acceptance:** Development build works correctly

---

### Phase 4: ConfigRegistry Conditional (Low Risk)

**Goal:** Non-essential config setters are no-ops in production.

- [ ] Add `#include "config/ConfigMode.h"` to ConfigRegistry.cpp
- [ ] Create `essentialSetter` and `frozenSetter` helpers for Search configs
- [ ] Create `essentialEvalSetter` and `frozenEvalSetter` helpers for Eval configs
- [ ] In production: `frozenSetter` is a no-op lambda
- [ ] In development: `frozenSetter` works same as `essentialSetter`
- [ ] Update essential configs to use `essentialSetter`:
    - Search: CONFIG_SOURCE, MOVE_OVERHEAD_MS, USE_BOOK, BOOK_PATH, BOOK_TYPE, USE_PONDER, TT_SIZE_MB, TB_PATH
    - Eval: EVAL_CONFIG_SOURCE, USE_PAWN_TT, PAWN_TT_SIZE_MB
- [ ] All other configs use `frozenSetter` (aliased as `searchSetter`/`evalSetter` for minimal code change)
- [ ] Verify development build still works
- [ ] Verify production build compiles

**Note:** Approach chosen was no-op setters rather than removing registry entries.
This preserves display/documentation while preventing runtime modification of frozen configs.

**Acceptance:** Both builds compile and work correctly

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

### Phase 7: CLI Tools and Unit Tests (Low Risk)

**Goal:** Ensure CLI tools work correctly and unit tests run in both modes.

#### CLI Tools

- [ ] Verify `--bench` works (only uses essential TT_SIZE_MB)
- [ ] Verify `--testsuite` works (only uses essential USE_BOOK)

#### Unit Tests - Wrap Development-Only Tests

- [ ] `test/engine/EngineSpeedTests.cpp` - wrap CONFIG_OVERRIDE usage in `#ifndef FRANKYCPP_PRODUCTION`
- [ ] `test/config/ConfigRegistryTest.cpp` - wrap entire file or individual tests
- [ ] `test/config/ConfigManagerTest.cpp` - wrap tests that modify configs

#### Unit Tests - Statistics Handling

- [ ] Identify tests that check non-essential statistics counters
- [ ] Wrap non-essential stat checks in `#ifndef FRANKYCPP_PRODUCTION`
- [ ] Keep essential stat checks (nodes, depth, time) without guards

#### CMake Test Configuration

- [ ] Ensure test executable compiles in both modes
- [ ] Document which tests are development-only

#### Future Task (Deferred)

- [ ] Add CI job for production build tests (after local implementation verified)

**Acceptance:** 
- `--bench` and `--testsuite` work in production builds
- All applicable tests pass in both build modes (local verification)

---

## Summary: What Changes Where

| File                     | Change                                        | Risk   |
|--------------------------|-----------------------------------------------|--------|
| `ConfigMode.h`           | **NEW** - defines CONFIG_CONST, STAT_* macros | Low    |
| `CMakeLists.txt`         | Add FRANKYCPP_PRODUCTION option               | Low    |
| `CMakePresets.json`      | Add production presets                        | Low    |
| `SearchConfigData.h`     | Add CONFIG_CONST prefix to members            | Low    |
| `EvalConfigData.h`       | Add CONFIG_CONST prefix to members            | Low    |
| `ConfigManager.h/cpp`    | Conditional return type                       | Medium |
| `ConfigRegistry.cpp`     | `#ifndef` around non-essentials               | Low    |
| `Search.cpp`             | `statistics.x++` → `STAT_INC(statistics.x)`   | Low    |
| `EngineSpeedTests.cpp`   | `#ifndef FRANKYCPP_PRODUCTION` guards         | Low    |
| `ConfigRegistryTest.cpp` | `#ifndef FRANKYCPP_PRODUCTION` guards         | Low    |
| `ConfigManagerTest.cpp`  | `#ifndef FRANKYCPP_PRODUCTION` guards         | Low    |

**Total:** ~11 files changed, most changes are mechanical/low-risk

---

## Advantages of This Approach

1. **Simplicity** - Just a few macros, no meta-programming
2. **Gradual migration** - Can add `CONFIG_CONST` one member at a time
3. **Low risk** - Easy to rollback (just remove macros)
4. **IDE friendly** - Struct is normal C++, IDE understands it
5. **Search.cpp unchanged** - No `FEATURE_ENABLED()` wrapper needed
6. **Proven pattern** - Same approach used by STL, Boost, game engines
7. **Clear struct** - Can read SearchConfigData.h and understand all configs
8. **Compiler does the work** - We just mark things `static constexpr`, optimizer handles the rest
9. **Single source of truth** - Defaults in struct, accessed directly
10. **Compile-time safety** - static_assert guards prevent accidental freezing of essential configs
11. **Fail-fast on mistakes** - CONFIG_OVERRIDE on non-essential configs fails to compile in production

---

## Disadvantages / Tradeoffs

1. **Two places for new config** - Still need struct member + registry entry
    - Mitigated by direct value access for defaults

2. **Manual essential marking** - Must remember `CONFIG_ESSENTIAL` vs `CONFIG_CONST`
    - Mitigated by clear grouping in struct
    - Mitigated by static_assert guardrails catching mistakes

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

| Aspect             | X-Macro  | Static Constexpr (This) |
|--------------------|----------|-------------------------|
| Complexity         | High     | **Low**                 |
| Risk               | High     | **Low**                 |
| Search.cpp changes | Required | **None**                |
| IDE support        | Poor     | **Good**                |
| Gradual migration  | Hard     | **Easy**                |
| Rollback           | Hard     | **Easy**                |

### Key Insight

The X-macro approach solves a problem we don't actually have. We wanted:
1. Single source of truth for defaults → **Solved by `defaultFrom()`**
2. Zero runtime overhead → **Solved by `static constexpr`**
3. Hide configs in production → **Solved by `#ifndef`**

We don't need meta-programming to achieve any of these goals.

### Bottom Line

**90% of the benefit with 20% of the complexity.**

The static constexpr approach:
- Uses proven STL patterns
- Requires minimal code changes
- Lets the compiler do the work
- Is easy to understand and maintain
