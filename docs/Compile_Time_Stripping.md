# Compile-Time Configuration Stripping

**Status:** ✅ Implemented in v1.4
**Created:** 2026-02-24
**Related:** `docs/specs/PLAN_Constexpr_Config_Approach.md`

---

## Overview

FrankyCPP v1.4 introduces a **Production Build Mode** that strips non-essential configuration options and statistics at compile time. This ensures maximum performance by eliminating:
1. Runtime branches for feature toggles (e.g. `if (SearchConfig.USE_LMR)`)
2. Atomic/counter overhead for statistics (e.g. detailed beta-cutoff stats)

This is achieved using C++20 `static constexpr` members and macro magic, allowing the compiler's dead code elimination to remove unused code paths completely.

### Performance Impact
- **NPS Increase:** ~2-5% gain in Nodes Per Second
- **Binary Size:** Smaller binary due to stripped code
- **Functionality:** Identical search behavior (deterministic), but reduced runtime configurability

---

## Build Modes

The build mode is controlled by the CMake option `FRANKYCPP_PRODUCTION`.

### 1. Development Build (Default)
- **Flag:** `-DFRANKYCPP_PRODUCTION=OFF`
- **Behavior:**
    - All config options are **mutable instance members**
    - Full UCI/YAML support for all settings
    - All statistics are collected
    - `CONFIG_OVERRIDE` macros work for all options
- **Use Case:** Debugging, tuning, feature development, running test suites

### 2. Production Build
- **Flag:** `-DFRANKYCPP_PRODUCTION=ON`
- **Behavior:**
    - Non-essential configs become **`static constexpr`** (compile-time constants)
    - Code paths guarded by disabled features are eliminated
    - Non-essential statistics compile to **no-ops**
    - Runtime configuration (UCI/YAML) is **ignored** for non-essential options
    - Attempting to set a frozen option via code fails to compile (safety feature)
- **Use Case:** Releases, rating lists, competitive play

---

## Developer Guide

### 1. Defining Configuration Options

All config structs (`SearchConfigData`, `EvalConfigData`) use macros to define members. You must decide if a member is **Essential** or **Strippable**.

#### A. Essential Configs (`CONFIG_ESSENTIAL`)
Must remain mutable at runtime in **all** builds.
- **Examples:** Hash size, Threads, Book path, Syzygy path, Ponder
- **Macro:** `CONFIG_ESSENTIAL` (expands to nothing, just a marker)
- **Usage:**
  ```cpp
  struct SearchConfigData {
      // Always mutable (std::string, large buffers, system settings)
      CONFIG_ESSENTIAL int TT_SIZE_MB = 64;
      CONFIG_ESSENTIAL std::string BOOK_PATH = "book.txt";
  };
  ```

#### B. Strippable Configs (`CONFIG_CONST`)
Can be frozen to extensive constants in Production.
- **Examples:** Search parameters, LMR weights, Feature toggles (`USE_NULL_MOVE`)
- **Macro:** `CONFIG_CONST`
    - Dev: Expands to nothing (mutable)
    - Prod: Expands to `static constexpr` (immutable constant)
- **Usage:**
  ```cpp
  struct SearchConfigData {
      // Mutable in Dev, Constant in Prod
      CONFIG_CONST bool USE_LMR = true;
      CONFIG_CONST int LMR_DEPTH = 3;
  };
  ```

### 2. Statistics (`STAT_` Macros)

Use the provided macros in implementation files (`Search.cpp`) to ensure zero overhead in production.

| Macro                   | Description          | Dev Build | Prod Build |
|-------------------------|----------------------|-----------|------------|
| `STAT_INC(x)`           | Increment counter    | `++x`     | `(void)0`  |
| `STAT_ADD(x, v)`        | Add value            | `x += v`  | `(void)0`  |
| `ESSENTIAL_STAT_INC(x)` | **Always** increment | `++x`     | `++x`      |

**Example:**
```cpp
// Essential for UCI output
ESSENTIAL_STAT_INC(nodesVisited);

// Removed in Production
STAT_INC(stats.currentBetaCutoffs);
```

### 3. Safety Mechanisms

If you accidentally try to assign to a non-essential config in Production code, the compiler will catch it:

```cpp
// Search.cpp
SearchConfig.USE_LMR = false; // ERROR in Prod: cannot assign to static constexpr
```

**Registry Magic:**
The `ConfigRegistry` uses a special template helper `assignIfMutable<T>` that discards assignments to `static constexpr` members at compile-time. This allows the same registry code to work in both builds without `#ifdef` guards.

---

## Migration Checklist

When adding a new configuration option:

1. **Add to Struct:**
   Decide if it's `CONFIG_ESSENTIAL` or `CONFIG_CONST`.
   ```cpp
   CONFIG_CONST int MY_NEW_PARAM = 100;
   ```

2. **Add to Registry (`ConfigRegistry.cpp`):**
   Use the `SEARCH_CONFIG_SETTER` macro.
   ```cpp
   .setter = SEARCH_CONFIG_SETTER(MY_NEW_PARAM, parseInt)
   ```

3. **Use in Code:**
   Access normally via `SearchConfig.MY_NEW_PARAM`.
   **Do NOT** use `#ifdef` in the engine code. Rely on the compiler to eliminate dead code.

---

## Troubleshooting

**Q: My config change via UCI isn't working!**
A: Are you running a Production build? Non-essential configs are frozen. Check `FrankyCPP --show-config` or the startup logs.

**Q: I get a compile error "assignment of read-only variable" in my test.**
A: You are trying to modify a `CONFIG_CONST` member in a test compiled with `-DFRANKYCPP_PRODUCTION`.
   - **Fix 1:** Wrap the test code in `#ifndef FRANKYCPP_PRODUCTION`.
   - **Fix 2:** Change the config to `CONFIG_ESSENTIAL` if it really needs to be mutable.

**Q: How do I verify feature stripping works?**
A: 
1. Build Release Production: `cmake -B build -DFRANKYCPP_PRODUCTION=ON`
2. Check binary size (should be smaller).
3. Check `FrankyCPP --show-config` (frozen options marked).
