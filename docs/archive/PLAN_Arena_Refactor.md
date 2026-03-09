# FrankyCPP Arena Refactoring Plan

**Document Version:** 1.3  
**Created:** 2026-03-08  
**Last Updated:** 2026-03-09  
**Status:** ✅ COMPLETE — All 13 stages done  
**Target:** FrankyCPP Arena (same version — no feature changes)  
**Priority:** High (Code quality and maintainability)

---

## Goal

Refactor the Arena codebase for **modularity, clarity, and maintainability** while preserving
**100% identical functionality**. No features are added or removed. Existing JSON result files,
YAML config format, and CLI interface remain fully compatible.

### Principles

1. **No regressions** — every CLI command produces the same output as before
2. **Commit after each stage** — safe rollback points
3. **User tests after each stage** — rebuild & run before proceeding
4. **No feature changes** unless explicitly discussed and approved

---

## Current File Inventory (for reference)

| File                    | Lines      | Role                                                                 |
|-------------------------|------------|----------------------------------------------------------------------|
| `ArenaConfig.h/cpp`     | 199 / 499  | YAML loading, validation, `expandTestSuiteRuns()`                    |
| `ArenaResults.h`        | 515        | All result structs + `ReportData` with query logic                   |
| `ArenaRunner.h/cpp`     | 195 / 1441 | Orchestrator + result loading + report generation + formatting utils |
| `ResultWriter.h/cpp`    | 153 / 428  | JSON writing (manual) + benchmark reading (hand-rolled parser)       |
| `TestSuiteRunner.h/cpp` | 152 / 732  | EPD test execution (sequential + parallel)                           |
| `MatchRunner.h/cpp`     | 229 / 699  | cutechess-cli integration, match state persistence                   |
| `BenchmarkRunner.h/cpp` | 81 / 239   | Benchmark execution (internal + external)                            |
| `UCIEngine.h/cpp`       | 233 / 691  | External UCI engine subprocess (Boost.Asio)                          |
| `ConsoleColors.h`       | 129        | ANSI color utilities                                                 |
| `engine_arena_main.cpp` | 356        | CLI entry point                                                      |
| **Total**               | **~5,672** |                                                                      |

---

## Issues to Address

### Architectural

| #  | Issue                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               | Impact                                      |
|----|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------|
| A1 | **ArenaRunner is a god object** (1441 lines) — orchestration + result loading + 5 report generators + formatting utils + engine config query                                                                                                                                                                                                                                                                                                                                                        | Hard to navigate, hard to test              |
| A2 | **Dual JSON approach** — manual `<<` writing in ResultWriter, nlohmann/json reading in ArenaRunner, hand-rolled parser in `readBenchmarkResults()`                                                                                                                                                                                                                                                                                                                                                  | Inconsistent, fragile, maintenance overhead |
| A3 | **ReportData mixes data + queries** — 6 query methods with complex matching logic baked into a data struct                                                                                                                                                                                                                                                                                                                                                                                          | Violates SRP                                |
| A4 | **Engine name matching scattered in 3 places** with 3 subtly different algorithms                                                                                                                                                                                                                                                                                                                                                                                                                   | Bug-prone, confusing                        |
| A5 | **`TestSuiteRunConfig → TestSuiteConfig` expansion** called 2-3× per run; both types coexist as public API                                                                                                                                                                                                                                                                                                                                                                                          | Unnecessary complexity                      |
| A6 | **Engine names inconsistent across result sources** — benchmarks store `"FrankyCPP"`, test suites store `"FrankyCPP v1.3"` (from UCI `id name`), matches store `"FrankyGo"` vs test suites `"FrankyGo v1.0.3 (4.6.2021)"`. All share a single `data.engines` set → duplicate rows (identical scores via flexible matching) and phantom `"External"` entries with N/A in reports. Current workaround: report collects engines from `suiteResults` keys only; benchmarks don't add to `data.engines`. | Confusing reports, fragile workarounds      |

### Code Quality

| #  | Issue                                                                                                                | Impact                               |
|----|----------------------------------------------------------------------------------------------------------------------|--------------------------------------|
| C1 | `fixedWidth` lambda duplicated 4× across report methods                                                              | DRY violation                        |
| C2 | Timestamp generation duplicated 3× (`ArenaRunner`, `TestSuiteRunner`, `ResultWriter`) with subtle format differences | Maintenance burden                   |
| C3 | Sequential test runner duplicates the eval logic already extracted in `runSinglePosition`                            | Unnecessary duplication (~130 lines) |
| C4 | UCI option parsing duplicated between `setUciOptions()` and `sendPendingOptions()` in UCIEngine                      | Same code twice                      |
| C5 | `writeComparison()` in ResultWriter is a dead placeholder                                                            | Dead code                            |
| C6 | ConsoleColors.h: non-thread-safe `isTerminal()` init; raw constants bypass TTY check                                 | Minor correctness issue              |

---

## ✅ Design Decisions (Resolved)

### D1: `TestSuiteConfig` vs `TestSuiteRunConfig` — Keep Both ✅

**Decision:** Option **(a)** — keep both structs.

- **`TestSuiteRunConfig`** is the YAML-level grouping format. It represents one "run" block that
  configures a single engine (path, version, time, depth, options, etc.) and lists **multiple EPD
  suites** to run against. The `suites` field is a vector of paths (or `SuiteOverride` objects with
  per-suite time/depth overrides). Think of it as: *"run these N EPD suites against engine v1.5
  with these shared settings."*
- **`TestSuiteConfig`** is the fully-expanded, flat, per-suite config. One `TestSuiteRunConfig`
  with 5 suites becomes 5 `TestSuiteConfig` objects, each carrying the full set of settings
  (engine path, time, depth, etc.) plus the specific EPD path. Per-suite overrides from
  `SuiteOverride` are resolved here.
- **The difference:** `TestSuiteRunConfig` avoids YAML repetition (shared settings for N suites),
  while `TestSuiteConfig` is the denormalized form the runners actually consume.
  `expandTestSuiteRuns()` converts one → many by cross-joining settings with each suite entry.

**Implementation:** Expand once during `loadFromYaml()` and store the flat list inside
`ArenaConfig::testSuites`. Remove `expandTestSuiteRuns()` as a public method. This keeps the
YAML schema readable and the runtime simple.

### D2: `queryEngineConfig()` — Move to `UCIEngine` ✅

**Decision:** Option **(a)** — move to `UCIEngine` as a static method.

It naturally belongs in `UCIEngine` since it already creates a temporary `UCIEngine` internally.
Note: this only works for FrankyCPP engines (uses the non-standard `getoptions` command).
For non-FrankyCPP engines it prints a "not supported" message. This is acceptable — no change
to that behavior.

### D3: `loadAllResults()` — Separate Load Methods ✅

**Decision:** Option **(a)** — separate `loadTestSuiteResults()`, `loadMatchResults()`,
`loadBenchmarkResults()` as independent calls. Callers compose. Also provide a convenience
`loadAllResults()` that calls all three. Fixes the redundant double-load in
`generateEngineSummary()`.

### D4: Report Generation — All Static ✅

**Decision:** Option **(a)** — all report methods are static pure functions taking `ReportData`
as input. Caller loads data first, then passes it to the report generator.

---

## Refactoring Stages

### Stage 1: Extract `FormatUtils` — Deduplicate Formatting

**Scope:** Pure extraction — no behavior changes.

**What:**
1. Create `FormatUtils.h` (header-only) in `engine_arena/`
2. Move from `ArenaRunner`:
   - `formatNumber(int64_t)` → `FormatUtils::formatNumber()`
   - `formatNodes(double)` → `FormatUtils::formatNodes()`
   - `formatTime(double)` → `FormatUtils::formatTime()`
   - `formatDelta(int)` → `FormatUtils::formatDelta()`
   - `formatDeltaPercent(double)` → `FormatUtils::formatDeltaPercent()`
   - `fixedWidth(string, size_t)` — the lambda duplicated 4× becomes a free function
3. Create `TimeUtils.h` (header-only) in `engine_arena/`
   - Unify the 3 timestamp functions into:
     - `TimeUtils::isoTimestamp()` — `"2026-03-08T14:30:22Z"` (UTC, for result files)
     - `TimeUtils::fileTimestamp()` — `"20260308_143022"` (for filenames)
     - `TimeUtils::displayTimestamp()` — `"2026-03-08 14:30:22"` (for reports)
4. Update all callers in `ArenaRunner.cpp`, `TestSuiteRunner.cpp`, `ResultWriter.cpp`, `MatchRunner.cpp`
5. Remove the old private methods and lambdas

**Files touched:** New: `FormatUtils.h`, `TimeUtils.h`. Modified: `ArenaRunner.h/cpp`, `TestSuiteRunner.cpp`, `ResultWriter.cpp`, `MatchRunner.cpp`

**Verify:** All CLI commands produce identical output. Rebuild and run: `--report`, `--summary FrankyCPP-v1.5`, `--bench-report`.

---

### Stage 2: Fix `ConsoleColors.h` — Thread Safety & Correctness

**Scope:** Minimal change — fix `isTerminal()` and make raw constants safe.

**What:**
1. Replace the non-thread-safe `checked`/`isTTY` pattern with `static const bool`:
   ```cpp
   inline bool isTerminal() {
     static const bool isTTY = (isatty(fileno(stdout)) != 0);
     return isTTY;
   }
   ```
2. No other changes — the `color()` / `colorize()` functions already gate on `isTerminal()`.

**Files touched:** `ConsoleColors.h`

**Verify:** Reports still show colors on terminal, no colors when piped to file.

---

### Stage 3: Deduplicate `UCIEngine` Option Parsing ✅

**Scope:** Internal refactor of UCIEngine.cpp — no API change.

**What:**
1. Extract the common option-string-to-pairs parsing into a private method:
   ```cpp
   static std::vector<std::pair<std::string, std::string>> parseOptionPairs(const std::string& options);
   ```
2. `setUciOptions()` calls `parseOptionPairs()` then calls `setOption()` for each
3. `sendPendingOptions()` removed — inlined in `initializeUCI()` using `parseOptionPairs()`
4. Move `queryEngineConfig()` from `ArenaRunner` to `UCIEngine` as a static method (per D2 decision)

**Status:** ✅ Complete

**Files touched:** `UCIEngine.h/cpp`, `ArenaRunner.h/cpp`

**Verify:** Test suites still run correctly with `uciOptions` configured. Run: `--testsuites` with a small config.

---

### Stage 4: Deduplicate TestSuiteRunner Evaluation Logic ✅

**Scope:** Internal refactor of TestSuiteRunner.cpp — no API change.

**What:**
1. `runTestSuiteSequential()` currently has inline eval logic (~130 lines) that duplicates `runSinglePosition()`
2. Refactor `runTestSuiteSequential()` to call `runSinglePosition()` for each test (just like parallel does)
3. Keep the sequential-specific console output (`[N/M] TestId: PASS/FAIL (move, nodes, time)`) by printing after `runSinglePosition()` returns

**Status:** ✅ Complete

**Files touched:** `TestSuiteRunner.cpp`

**Verify:** Run `--testsuites` with `parallelWorkers: 1` — results must be identical. Compare JSON output files before/after.

---


### Stage 5: Unify JSON Handling — Use nlohmann/json Everywhere ✅
**Scope:** Replace manual JSON writing and the hand-rolled reader with nlohmann/json.

**What:**
1. `ResultWriter.cpp`: Replace manual `<<` JSON writing with `nlohmann::json` for:
   - `writeTestSuiteResult()` 
   - `writeMatchResult()`
   - `writeBenchmarkResult()`
2. Remove the manual `escapeJsonString()` helper
3. `ResultWriter.cpp`: Replace hand-rolled `readBenchmarkResults()` parser with `nlohmann::json`
4. `MatchRunner.cpp`: Replace regex-based `loadMatchState()` and manual `saveMatchState()` with nlohmann/json
5. Verify JSON output is byte-for-byte compatible (field order, formatting) or document any harmless whitespace differences

**Status:** ✅ Complete — nlohmann/json::dump(2) produces alphabetically-sorted keys (differs from hand-written order), but structure and values are identical. All existing JSON files remain readable.

**Files touched:** `ResultWriter.cpp`, `MatchRunner.cpp`  
**Build change:** `ResultWriter.cpp` and `MatchRunner.cpp` now `#include <nlohmann/json.hpp>` (already a vcpkg dependency)

**Verify:** 
- Run `--testsuites` and `--matches` — JSON files are valid and loadable
- Run `--report` — report loads from JSON correctly
- Run `--bench` — benchmark results append correctly
- Compare JSON structure with old output (field names, nesting must match)

---

### Stage 6: Extract `ResultStore` — Centralize Result I/O ✅

**Scope:** Move all result read/write logic into one class. ArenaRunner becomes orchestration-only for data loading.

**What:**
1. Create `ResultStore.h/cpp`:
   - Constructor: `ResultStore(const std::string& resultsDir)`
   - **Writing** (absorbed from `ResultWriter`):
     - `writeTestSuiteResult(const TestSuiteResult&) → std::string`
     - `writeMatchResult(const MatchResult&) → std::string`
     - `writeBenchmarkResult(const BenchmarkResult&) → std::string`
   - **Reading** (absorbed from `ArenaRunner` + `ResultWriter`):
     - `loadTestSuiteResults() → ReportData` (just test suites)
     - `loadMatchResults(ReportData&)` (adds matches to existing data)
     - `loadBenchmarkResults(ReportData&)` (adds benchmarks)
     - `loadAllResults() → ReportData` (convenience: calls all three)
     - `readBenchmarkResults() → vector<BenchmarkResult>`
2. **Fix A6 — Engine name inconsistency:** Each load method populates only its own
   domain-specific data in `ReportData`. In particular:
   - `loadTestSuiteResults()` populates `engines`, `testSuites`, `suiteResults`
   - `loadMatchResults()` populates `matchResults` only — does NOT add match engine
     IDs to `engines` (match engine names like `"FrankyGo"` differ from test suite
     names like `"FrankyGo v1.0.3 (4.6.2021)"` and cause phantom duplicates)
   - `loadBenchmarkResults()` populates `benchmarkResults` only — does NOT add
     benchmark engine IDs to `engines` (benchmark names like `"FrankyCPP"` or
     `"External"` differ from test suite names like `"FrankyCPP v1.3"`)
   - Report generators that need cross-domain engine lists use flexible matching
     queries (`getMatchesForEngine()`, `getBenchmarksForEngine()`) rather than
     relying on a shared `engines` set
3. Delete `ResultWriter.h/cpp` — its functionality is fully absorbed by `ResultStore`
4. Update `ArenaRunner` to use `ResultStore` instead of `ResultWriter` and inline loading code
5. Update `engine_arena_main.cpp` to use `ResultStore` where it currently uses `ResultWriter` directly

**Status:** ✅ Complete — also fixed double-load in generateEngineSummary() and removed ~155 lines of loading code from ArenaRunner.cpp

**Files touched:** New: `ResultStore.h/cpp`, `ResultStore_Test.cpp`. Deleted: `ResultWriter.h/cpp`, `ResultWriter_Test.cpp`. Modified: `ArenaRunner.h/cpp`, `engine_arena_main.cpp`

**Verify:** All commands still work: `--testsuites`, `--matches`, `--bench`, `--bench-report`, `--report`, `--summary`.

---

### Stage 7: Extract `ReportGenerator` — Separate Report Formatting ✅

**Scope:** Move all report generation out of ArenaRunner into a dedicated class.

**What:**
1. Create `ReportGenerator.h/cpp` with all static methods:
   - `generateBaselineReport(const ReportData&) → string`
   - `generateComparisonReport(const ReportData&, const EngineId&, const vector<EngineId>&) → string`
   - `generateMatchBaselineReport(const ReportData&) → string`
   - `generateMatchComparisonReport(const ReportData&, const EngineId&, const vector<EngineId>&) → string`
   - `generateEngineSummary(const ReportData&, const EngineId&, bool showHistory) → string`
2. Move from `ArenaRunner.cpp` — these are large methods (total ~800 lines)
3. `ArenaRunner` report methods removed entirely. Callers in `engine_arena_main.cpp`
   call `ResultStore` (via `ArenaRunner::loadAllResults()`) then `ReportGenerator` directly.
4. `generateEngineSummary()` no longer loads its own data — it takes `const ReportData&`
   as input like all other report methods.

**Status:** ✅ Complete — ArenaRunner.cpp reduced from 1087 to 138 lines. All ~930 lines of report formatting moved to ReportGenerator.cpp.

**Files touched:** New: `ReportGenerator.h/cpp`. Modified: `ArenaRunner.h/cpp`, `engine_arena_main.cpp`

**Verify:** All report commands produce identical output: `--report`, `--cmp`, `--summary`, `--summary --history`, `--report --testsuites-only`, `--report --matches-only`.

---

### Stage 8: Simplify `ArenaResults.h` — Separate Data from Queries ✅

**Scope:** Move query methods from `ReportData` into `ResultStore`.

**What:**
1. `ReportData` becomes a pure data struct (no methods except `hasResults()` etc.)
2. Move to `ResultStore` as static methods (operate on `const ReportData&`):
   - `findEngine(const ReportData&, const EngineId&)` 
   - `getResult(const ReportData&, suite, engine)` 
   - `getMatch(const ReportData&, engine1, engine2)`
   - `getMatchesForEngine(const ReportData&, engine)`
   - `getBenchmarksForEngine(const ReportData&, engine)`
   - `enginesMatchFlexibly()` (private static helper)
3. Update `ReportGenerator` to call `ResultStore::findEngine(data, ...)` etc.
4. `EngineId` stays in `ArenaResults.h` — it's a core value type

**Status:** ✅ Complete — ReportData reduced from ~200 lines to ~55 lines (pure data bag). All query logic centralized in ResultStore with single canonical `enginesMatchFlexibly` implementation.

**Files touched:** `ArenaResults.h`, `ResultStore.h/cpp`, `ReportGenerator.cpp`

**Verify:** Same as Stage 7 — all reports identical.

---

### Stage 9: Clean Up `ArenaConfig` — Eager Expansion ✅

**Scope:** Simplify the config expansion flow.

**What:**
1. Call `expandTestSuiteRunsInternal()` once inside `loadFromYaml()` and store the result in `ArenaConfig::testSuites`
2. Keep `testSuiteRuns` for YAML validation; `expandTestSuiteRunsInternal()` is private
3. Remove the public `expandTestSuiteRuns()` method
4. Update all callers to use `config.testSuites` directly
5. `TestSuiteRunConfig` and `SuiteOverride` remain for YAML parsing but are not part of the public API

**Status:** ✅ Complete — 3 callers updated to use `config.testSuites` directly. No more redundant re-expansion on each call.

**Files touched:** `ArenaConfig.h/cpp`, `ArenaRunner.cpp`, `TestSuiteRunner.cpp`, `engine_arena_main.cpp`

**Verify:** Run `--testsuites` — same results. Check that `config.testSuites.size()` matches `config.expandTestSuiteRuns().size()` in the old code.

---

### Stage 10: Clean Up Dead Code and Polish ✅

**Scope:** Remove dead code, fix minor issues.

**What:**
1. Remove `writeComparison()` placeholder from `ResultStore` (was in old `ResultWriter`) ✅
2. Remove `comparisons/` directory creation from `ResultStore` constructor ✅
3. Verified all includes are needed — no unused includes found ✅
4. All new files have proper copyright headers and `FRANKYCPP_*` header guards ✅
5. CMakeLists.txt uses GLOB — new files auto-discovered ✅
6. Const-correctness verified across all modified files ✅

**Status:** ✅ Complete

**Files touched:** `ResultStore.h/cpp`

---

### Stage 11: Update Documentation ✅

**Scope:** Bring documentation in sync with the refactored code.

**What:**
1. Update `docs/arena/Development.md`: replaced `ResultWriter` with `ResultStore` + `ReportGenerator`,
   updated component hierarchy, fixed code examples ✅
2. Update `docs/arena/Results.md`: removed `comparisons/`, added `benchmarks/` ✅
3. Update `docs/arena/Configuration.md`: removed `comparisons/`, added `benchmarks/` ✅
4. Update `docs/arena/README.md`: added `benchmarks/` to directory structure ✅
5. Mark `PLAN_Arena_Tags_and_Summary.md` as ✅ COMPLETE ✅
6. Mark this plan as ✅ COMPLETE ✅
7. `CLAUDE.md` and `.github/copilot-instructions.md` — no Arena-specific references, no changes needed ✅

**Status:** ✅ Complete

**Files touched:** `Development.md`, `Results.md`, `Configuration.md`, `README.md`, `PLAN_Arena_Tags_and_Summary.md`, this document.

---

### Stage 12: Self-Critical Review of Full Arena Codebase ✅

**Scope:** Read every Arena source file end-to-end and produce a detailed review.

**What:**
1. Read all `engine_arena/*.h`, `engine_arena/*.cpp`, and `engine_arena_main.cpp` in full ✅
2. Review against the original issue list (A1–A5, C1–C6) — verify each issue is fully resolved ✅
3. Check for **new problems introduced by the refactoring** ✅
4. Check **API consistency** ✅
5. Check **edge cases** ✅
6. Verify all `[[nodiscard]]`, header guards, copyright headers follow project conventions ✅
7. Present findings to user as a prioritized list ✅

**Status:** ✅ Complete — Review identified issues addressed in Stage 13.

**Files touched:** None (read-only analysis).


---

### Stage 13: Fix Issues from Review ✅

**Scope:** Apply fixes identified in Stage 12.

**What:**
1. Address all 🔴 Must Fix items from Stage 12 ✅
2. Address 🟡 Should Fix items ✅
3. Apply 🟢 Nice to Have items ✅
4. Each fix was a clear, minimal change — no scope creep ✅
5. User tested after all fixes applied ✅

**Status:** ✅ Complete

**Files touched:** Various Arena source files per Stage 12 findings.

**Verify:** Full rebuild. All tests pass. All CLI modes verified.

---

## Post-Refactor File Structure

```
engine_arena/
  ArenaConfig.h/cpp        — YAML loading, validation, eager expansion (unchanged API)
  ArenaResults.h            — Pure data structs: EngineId, TestSuiteResult, MatchResult, 
                              BenchmarkResult, ReportData (data bag, no query logic)
  ArenaRunner.h/cpp         — Orchestration only: runAll, runTestSuites, runMatches
                              (~200 lines, down from 1441)
  ResultStore.h/cpp         — All result I/O: write JSON, load JSON, query methods
                              (replaces ResultWriter + loading from ArenaRunner)
  ReportGenerator.h/cpp     — All report formatting: baseline, comparison, summary, history
                              (~800 lines, pure functions of ReportData)
  FormatUtils.h             — formatNumber, formatNodes, formatTime, fixedWidth (header-only)
  TimeUtils.h               — isoTimestamp, fileTimestamp, displayTimestamp (header-only)
  ConsoleColors.h           — ANSI colors + symbols (fixed thread safety)
  TestSuiteRunner.h/cpp     — EPD test execution (deduplicated eval logic)
  MatchRunner.h/cpp         — cutechess-cli integration (nlohmann/json for state files)
  BenchmarkRunner.h/cpp     — Benchmark execution (unchanged)
  UCIEngine.h/cpp           — External UCI engine interface (deduplicated option parsing,
                              + queryEngineConfig static method)
engine_arena_main.cpp       — CLI entry point (unchanged interface)
```

**Files added:** `FormatUtils.h`, `TimeUtils.h`, `ResultStore.h/cpp`, `ReportGenerator.h/cpp`  
**Files removed:** `ResultWriter.h/cpp` (absorbed into `ResultStore`)  
**Net change:** +3 files (4 added, 1 removed)

---

## Stage Summary

| Stage | Description                                                     | Risk     | Addresses     | Files Changed                |
|-------|-----------------------------------------------------------------|----------|---------------|------------------------------|
| 1     | Extract `FormatUtils` + `TimeUtils`                             | Low      | C1, C2        | 6 modified, 2 new            |
| 2     | Fix `ConsoleColors.h`                                           | Very Low | C6            | 1 modified                   |
| 3     | Deduplicate UCIEngine option parsing + move `queryEngineConfig` | Low      | C4, A1 (part) | 2 modified                   |
| 4     | Deduplicate TestSuiteRunner eval logic                          | Low      | C3            | 1 modified                   |
| 5     | Unify JSON with nlohmann/json                                   | Medium   | A2            | 2 modified                   |
| 6     | Extract `ResultStore` (replaces `ResultWriter`)                 | Medium   | A1 (part), A6 | 3 modified, 2 new, 2 deleted |
| 7     | Extract `ReportGenerator` ✅                                     | Medium   | A1 (major)    | 3 modified, 2 new            |
| 8     | Simplify `ArenaResults.h` (data-only) ✅                         | Low      | A3, A4        | 4 modified                   |
| 9     | Eager expansion in `ArenaConfig` ✅                              | Low      | A5            | 5 modified                   |
| 10    | Dead code cleanup & polish ✅                                    | Very Low | C5            | 2 modified                   |
| 11    | Update documentation ✅                                          | Low      | —             | 6 docs modified              |
| 12    | Self-critical review of full Arena codebase ✅                   | None     | All           | None (read-only)             |
| 13    | Fix issues from review ✅                                        | Low      | From Stage 12 | Depends on findings          |

---

*Last updated: 2026-03-09*
