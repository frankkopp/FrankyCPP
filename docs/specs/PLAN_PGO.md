# FrankyCPP — Profile-Guided Optimization (PGO) for PROD Builds

**Document Version:** 1.0  
**Created:** 2026-04-02  
**Status:** 📋 PLANNING  
**Parent:** PLAN_v1.8.md → D2  
**Scope:** Windows PROD build only (WSL deferred)

---

## Goal

Add Profile-Guided Optimization to the **Win Release (PROD)** build pipeline. PGO uses
runtime profiling data from a `--bench` run to let the compiler make better inlining, branch
prediction, and code layout decisions. Expected result: **+3–8% NPS**, translating to
**+5–15 ELO** with zero algorithmic changes.

---

## Background

PGO is a three-phase compiler optimization:

1. **Instrument** — Build with profiling counters inserted into every branch/call.
2. **Profile** — Run a representative workload (engine `--bench`). Generates `.pgd`/`.pgc`
   files (MSVC) recording hot paths, branch ratios, and loop trip counts.
3. **Optimize** — Rebuild using profile data. The compiler:
   - Inlines hot functions more aggressively
   - Inserts branch prediction hints from actual taken/not-taken ratios
   - Optimizes code layout (hot paths contiguous for better I-cache)
   - Tunes loop unrolling to actual trip counts
   - Moves cold code out of the hot path

### Why it works well for chess engines

Chess engines have very predictable hot paths — `search()`, `quiescence()`, `makeMove()`,
`evaluate()`, move generation loops. A small fraction of code accounts for most execution
time, and branch behavior is consistent across runs. The `--bench` workload closely mirrors
real play (fixed-depth multi-position search).

---

## Design

### Overview

- Two new CMake options: `FRANKYCPP_PGO_GENERATE` and `FRANKYCPP_PGO_USE` (both `OFF` by default).
- Two new CMake presets that inherit from `win-release-production` and share its build directory.
- A custom CMake target `pgo-bench` for IDE users to run the profiling workload.
- A new `production` mode in `build_windows.ps1` that automates the full three-phase pipeline.
- The existing `win-release-production` preset stays unchanged — non-PGO PROD still works.
- Normal `release` / `debug` / `relwithdebinfo` builds are completely unaffected.

### Build Matrix After Implementation

| Build                    | PGO? | Use case                                 |
|--------------------------|------|------------------------------------------|
| **Win Debug**            | ❌    | Daily development                        |
| **Win Release**          | ❌    | Daily development, testing, gauntlets    |
| **Win RelWithDebInfo**   | ❌    | Profiling, debugging                     |
| **Win Release (PROD)**   | ❌    | Production build without PGO (still works)|
| **PROD PGO Generate**    | 🔧    | Phase 1: instrumented build              |
| **PROD PGO Use**         | ✅    | Phase 3: final optimized build           |
| `build_windows.ps1 production` | ✅ | Automated full PGO pipeline          |

---

## Implementation Steps

### Step 1: CMake Options & Compiler/Linker Flags

**File:** `CMakeLists.txt` (root, ~line 37)

Add two new options near `FRANKYCPP_PRODUCTION`:

```cmake
option(FRANKYCPP_PGO_GENERATE "PGO instrumented build (phase 1: generate profile data)" OFF)
option(FRANKYCPP_PGO_USE "PGO optimized build (phase 3: use profile data)" OFF)
```

Add mutual-exclusion guard:

```cmake
if(FRANKYCPP_PGO_GENERATE AND FRANKYCPP_PGO_USE)
    message(FATAL_ERROR "FRANKYCPP_PGO_GENERATE and FRANKYCPP_PGO_USE are mutually exclusive.")
endif()
```

These are **linker flags only** on MSVC — the existing `/GL` from IPO/LTCG handles the
compile side. No compile flag changes needed.

Add PGO to the configuration summary (~line 462):

```cmake
message(STATUS "  PGO Generate:     ${FRANKYCPP_PGO_GENERATE}")
message(STATUS "  PGO Use:          ${FRANKYCPP_PGO_USE}")
```

### Step 2: PGO Linker Flags on Engine Exe Target

**File:** `src/CMakeLists.txt` (~after line 124, after engine exe `target_link_libraries`)

Apply PGO linker flags **only to the engine exe target** (not the static library or tests):

```cmake
# ============================================================================
# Profile-Guided Optimization (PGO) — linker flags
# Phase 1 (generate): instrument binary, run bench to collect .pgc/.pgd data
# Phase 3 (use):      rebuild using profile data for optimized code layout
# ============================================================================
if(FRANKYCPP_PGO_GENERATE)
    if(MSVC)
        target_link_options(${exeName} PRIVATE /GENPROFILE)
        message(STATUS "PGO: Instrumented build (MSVC /GENPROFILE)")
        message(STATUS "PGO: After building, run: $<TARGET_FILE:${exeName}> --bench")
        message(STATUS "PGO: Then reconfigure with FRANKYCPP_PGO_USE=ON and rebuild")
    else()
        message(WARNING "PGO generate not yet implemented for ${CMAKE_CXX_COMPILER_ID}")
    endif()
elseif(FRANKYCPP_PGO_USE)
    if(MSVC)
        target_link_options(${exeName} PRIVATE /USEPROFILE)
        message(STATUS "PGO: Optimized build (MSVC /USEPROFILE)")
    else()
        message(WARNING "PGO use not yet implemented for ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()
```

### Step 3: `pgo-bench` Custom Target

**File:** `src/CMakeLists.txt` (after PGO linker flags from Step 2)

Add a custom target that runs the engine bench to collect profile data. Only available during
the generate phase — gives IDE users a one-click way to do the profiling step.

```cmake
if(FRANKYCPP_PGO_GENERATE)
    add_custom_target(pgo-bench
        COMMAND $<TARGET_FILE:${exeName}> --bench --benchDepth 12 --benchHash 128
        DEPENDS ${exeName}
        WORKING_DIRECTORY $<TARGET_FILE_DIR:${exeName}>
        COMMENT "Running engine benchmark to collect PGO profile data..."
        VERBATIM
    )
    message(STATUS "PGO: Custom target 'pgo-bench' available — build it to collect profile data")
endif()
```

### Step 4: CMake Presets

**File:** `CMakePresets.json`

Add two new **configure presets** that inherit from `win-release-production`. Critical: both
must share the **same `binaryDir`** so the `.pgd`/`.pgc` files from the generate phase are
found during the use phase.

```json
{
  "name": "win-release-production-pgo-gen",
  "displayName": "Win Release PROD (PGO Generate)",
  "inherits": "win-release-production",
  "binaryDir": "${sourceDir}/cmake-build-win-release-production",
  "cacheVariables": {
    "FRANKYCPP_PGO_GENERATE": "ON"
  }
},
{
  "name": "win-release-production-pgo-use",
  "displayName": "Win Release PROD (PGO Use)",
  "inherits": "win-release-production",
  "binaryDir": "${sourceDir}/cmake-build-win-release-production",
  "cacheVariables": {
    "FRANKYCPP_PGO_USE": "ON"
  }
}
```

Add matching **build presets**:

```json
{
  "name": "win-release-production-pgo-gen",
  "displayName": "Win Release PROD (PGO Generate)",
  "configurePreset": "win-release-production-pgo-gen",
  "jobs": 30
},
{
  "name": "win-release-production-pgo-use",
  "displayName": "Win Release PROD (PGO Use)",
  "configurePreset": "win-release-production-pgo-use",
  "jobs": 30
}
```

Also add explicit `binaryDir` to existing `win-release-production` configure preset for
consistency (it should already resolve to `cmake-build-win-release-production` via the
`${presetName}` pattern, but making it explicit avoids ambiguity):

```json
"binaryDir": "${sourceDir}/cmake-build-win-release-production"
```

### Step 5: `build_windows.ps1` — Add `production` Mode

**File:** `build_windows.ps1`

#### 5a. Update `ValidateSet`

Change the parameter validation to add `production` and remove `minsizerel` (no matching
preset exists — it would fail today):

```powershell
[ValidateSet("debug", "release", "relwithdebinfo", "production")]
```

#### 5b. Add PGO Pipeline for Production Mode

When `$BuildMode -eq "production"`, run the three-phase pipeline instead of the normal
single-pass build:

```powershell
if ($BuildMode -eq "production") {
    $buildDir = "cmake-build-win-release-production"

    # ── Phase 1: Instrumented build ──
    Write-Host ""
    Write-Host "PGO Phase 1/3: Building instrumented binary..."
    Write-Host ""
    cmake --preset win-release-production-pgo-gen
    cmake --build $buildDir --parallel

    # ── Phase 2: Collect profile data ──
    Write-Host ""
    Write-Host "PGO Phase 2/3: Running benchmark to collect profile data..."
    Write-Host ""
    $engineExe = Get-ChildItem -Path ".\$buildDir\src" -Filter "FrankyCPP_v*.exe" `
                     -File -ErrorAction SilentlyContinue |
                 Where-Object { $_.Name -notmatch '_(Arena|Test|Extractor|Tuner)' } |
                 Select-Object -First 1 -ExpandProperty FullName
    if (-not $engineExe -or -not (Test-Path $engineExe)) {
        Write-Host "ERROR: Engine executable not found for PGO profiling" -ForegroundColor Red
        exit 1
    }
    Write-Host "Running: $engineExe --bench --benchDepth 12 --benchHash 128"
    & $engineExe --bench --benchDepth 12 --benchHash 128
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Benchmark run failed" -ForegroundColor Red
        exit $LASTEXITCODE
    }

    # ── Phase 3: Optimized rebuild ──
    Write-Host ""
    Write-Host "PGO Phase 3/3: Rebuilding with profile data..."
    Write-Host ""
    cmake --preset win-release-production-pgo-use
    cmake --build $buildDir --parallel

} else {
    # Normal (non-PGO) build flow — unchanged
    ...
}
```

**Important:** Phase 3 must NOT do `--clean-first` — the `.pgd`/`.pgc` files from Phase 2
must survive in the build directory.

#### 5c. Update Help Text

Add production mode to help output:

```
  BuildMode         Build configuration (default: release)
                    Valid values: debug, release, relwithdebinfo, production

  .\build_windows.ps1 production          # Production build with PGO (3-phase)
```

Add PGO explanation in Notes:

```
  - Production mode runs Profile-Guided Optimization (PGO):
    Phase 1: Build instrumented binary
    Phase 2: Run benchmark to collect profile data
    Phase 3: Rebuild with profile data for optimal performance
    This adds ~30-60 seconds but produces the fastest possible binary.
```

### Step 6: Update Documentation

**Files:** `CLAUDE.md`, `.github/copilot-instructions.md`

- Add `production` to the build command examples
- Briefly describe the PGO pipeline in the build environment section
- Document the IDE workflow:
  1. Select `Win Release PROD (PGO Generate)` preset → Build
  2. Build the `pgo-bench` target (runs engine bench)
  3. Switch to `Win Release PROD (PGO Use)` preset → Build
  4. Final binary is the PGO-optimized engine

---

## IDE Workflow (CLion)

For users who prefer to build in the IDE rather than via `build_windows.ps1`:

1. **Select preset** `Win Release PROD (PGO Generate)` in CLion's CMake profile dropdown
2. **Build** — produces instrumented engine binary
3. **Build target `pgo-bench`** — runs `--bench`, generates `.pgc` profile data files
4. **Switch preset** to `Win Release PROD (PGO Use)` in CMake profile dropdown
5. **Reload CMake** (CLion will prompt) — picks up the `USEPROFILE` linker flag
6. **Build** — produces the final PGO-optimized binary

The output binary is in `cmake-build-win-release-production/src/FrankyCPP_v1.8.exe`.

---

## Key Technical Details

### MSVC PGO Flags

| Phase    | Compile Flag | Linker Flag     | Profile Files         |
|----------|--------------|-----------------|-----------------------|
| Generate | (none — `/GL` from IPO) | `/GENPROFILE`  | `.pgd` created at link time |
| Profile  | N/A (run)    | N/A (run)       | `.pgc` created at runtime   |
| Use      | (none — `/GL` from IPO) | `/USEPROFILE`  | Reads `.pgd` + `.pgc`       |

- `/GL` (Whole Program Optimization) is already enabled via `CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON`.
- `/GENPROFILE` and `/USEPROFILE` work alongside `/LTCG` (Link-Time Code Generation).
- Profile files are placed next to the output `.exe` by default.
- Both presets share `binaryDir` = `cmake-build-win-release-production`, so profile files are
  automatically found in Phase 3.

### Shared Build Directory

Both `pgo-gen` and `pgo-use` presets MUST use the same `binaryDir`:
`${sourceDir}/cmake-build-win-release-production`

This ensures:
- Phase 1 link creates `FrankyCPP_v1.8.pgd` next to the exe
- Phase 2 bench run creates `FrankyCPP_v1.8!1.pgc` (etc.) in the same directory
- Phase 3 link finds and merges the `.pgc` data via the `.pgd`

### No `--clean-first` Between Phases

The `pgo-use` phase must NOT clean the build directory. The `.pgd`/`.pgc` files from the
generate+bench phases must persist. The `build_windows.ps1` script must not insert any clean
step between phases.

### `minsizerel` Removal

The current `ValidateSet` includes `minsizerel` but no CMake preset exists for it. It would
fail at `cmake --preset minsizerel`. Remove it from the `ValidateSet` as part of this change.

---

## Validation

After implementation:

1. **Run `build_windows.ps1 production`** end-to-end — verify all three phases complete
2. **Verify PGO files exist** — `.pgd` and `.pgc` in build dir after Phase 2
3. **Compare NPS** — Run `--bench` on PGO vs non-PGO PROD binary, expect +3–8% NPS
4. **Run tests** — All tests must pass on the PGO-optimized binary
5. **Bench signature** — Must match (PGO doesn't change search behavior, only speed)
6. **IDE workflow** — Verify the three-step CLion workflow works as documented

---

## Future Extensions (Deferred)

- **WSL/Linux PGO** — Add `wsl-release-production-pgo-gen`/`-pgo-use` presets and
  `build_wsl.sh production` mode. GCC uses `-fprofile-generate`/`-fprofile-use`;
  Clang uses `-fprofile-instr-generate`/`-fprofile-instr-use=<path>`.
- **CI/CD PGO** — GitHub Actions workflow that produces PGO-optimized release binaries.
- **PGO for Arena exe** — Currently only the main engine exe gets PGO. Arena exe could
  benefit too, but the profiling workload would differ.

---

*Created: 2026-04-02*
