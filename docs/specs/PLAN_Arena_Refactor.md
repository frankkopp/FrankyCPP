# FrankyCPP Arena Refactoring Plan

**Document Version:** 1.1  
**Created:** 2026-03-08  
**Status:** ✅ APPROVED — Ready for Implementation  
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

| #  | Issue                                                                                                                                              | Impact                                      |
|----|----------------------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------|
| A1 | **ArenaRunner is a god object** (1441 lines) — orchestration + result loading + 5 report generators + formatting utils + engine config query       | Hard to navigate, hard to test              |
| A2 | **Dual JSON approach** — manual `<<` writing in ResultWriter, nlohmann/json reading in ArenaRunner, hand-rolled parser in `readBenchmarkResults()` | Inconsistent, fragile, maintenance overhead |
| A3 | **ReportData mixes data + queries** — 6 query methods with complex matching logic baked into a data struct                                         | Violates SRP                                |
| A4 | **Engine name matching scattered in 3 places** with 3 subtly different algorithms                                                                  | Bug-prone, confusing                        |
| A5 | **`TestSuiteRunConfig → TestSuiteConfig` expansion** called 2-3× per run; both types coexist as public API                                         | Unnecessary complexity                      |

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

### Stage 3: Deduplicate `UCIEngine` Option Parsing

**Scope:** Internal refactor of UCIEngine.cpp — no API change.

**What:**
1. Extract the common option-string-to-pairs parsing into a private method:
   ```cpp
   static std::vector<std::pair<std::string, std::string>> parseOptionPairs(const std::string& options);
   ```
2. `setUciOptions()` calls `parseOptionPairs()` then calls `setOption()` for each
3. `sendPendingOptions()` calls `parseOptionPairs()` then sends raw commands
4. Move `queryEngineConfig()` from `ArenaRunner` to `UCIEngine` as a static method (per D2 decision)

**Files touched:** `UCIEngine.h/cpp`, `ArenaRunner.h/cpp`

**Verify:** Test suites still run correctly with `uciOptions` configured. Run: `--testsuites` with a small config.

---

### Stage 4: Deduplicate TestSuiteRunner Evaluation Logic

**Scope:** Internal refactor of TestSuiteRunner.cpp — no API change.

**What:**
1. `runTestSuiteSequential()` currently has inline eval logic (~130 lines) that duplicates `runSinglePosition()`
2. Refactor `runTestSuiteSequential()` to call `runSinglePosition()` for each test (just like parallel does)
3. Keep the sequential-specific console output (`[N/M] TestId: PASS/FAIL (move, nodes, time)`) by printing after `runSinglePosition()` returns

**Files touched:** `TestSuiteRunner.cpp`

**Verify:** Run `--testsuites` with `parallelWorkers: 1` — results must be identical. Compare JSON output files before/after.

---

### Stage 5: Unify JSON Handling — Use nlohmann/json Everywhere

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

**Files touched:** `ResultWriter.cpp`, `MatchRunner.cpp`  
**Build change:** `ResultWriter.cpp` and `MatchRunner.cpp` now `#include <nlohmann/json.hpp>` (already a vcpkg dependency)

**Verify:** 
- Run `--testsuites` and `--matches` — JSON files are valid and loadable
- Run `--report` — report loads from JSON correctly
- Run `--bench` — benchmark results append correctly
- Compare JSON structure with old output (field names, nesting must match)

---

### Stage 6: Extract `ResultStore` — Centralize Result I/O

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
2. Delete `ResultWriter.h/cpp` — its functionality is fully absorbed by `ResultStore`
3. Update `ArenaRunner` to use `ResultStore` instead of `ResultWriter` and inline loading code
4. Update `engine_arena_main.cpp` to use `ResultStore` where it currently uses `ResultWriter` directly

**Files touched:** New: `ResultStore.h/cpp`. Deleted: `ResultWriter.h/cpp`. Modified: `ArenaRunner.h/cpp`, `engine_arena_main.cpp`, `CMakeLists.txt` (both src and test)

**Verify:** All commands still work: `--testsuites`, `--matches`, `--bench`, `--bench-report`, `--report`, `--summary`.

---

### Stage 7: Extract `ReportGenerator` — Separate Report Formatting

**Scope:** Move all report generation out of ArenaRunner into a dedicated class.

**What:**
1. Create `ReportGenerator.h/cpp` with all static methods:
   - `generateBaselineReport(const ReportData&) → string`
   - `generateComparisonReport(const ReportData&, const EngineId&, const vector<EngineId>&) → string`
   - `generateMatchBaselineReport(const ReportData&) → string`
   - `generateMatchComparisonReport(const ReportData&, const EngineId&, const vector<EngineId>&) → string`
   - `generateEngineSummary(const ReportData&, const EngineId&, bool showHistory) → string`
2. Move from `ArenaRunner.cpp` — these are large methods (total ~800 lines)
3. `ArenaRunner` report methods become thin wrappers:
   ```cpp
   // In ArenaRunner — calls ResultStore then ReportGenerator
   std::string generateBaselineReport() const {
     auto data = resultStore.loadAllResults();
     return ReportGenerator::generateBaselineReport(data);
   }
   ```
   Or callers in `engine_arena_main.cpp` can call `ResultStore` + `ReportGenerator` directly.
4. `generateEngineSummary()` currently calls `loadAllResults()` + `loadMatchResults()` (double-load) — fix this by calling `loadAllResults()` once.

**Files touched:** New: `ReportGenerator.h/cpp`. Modified: `ArenaRunner.h/cpp`, `engine_arena_main.cpp`, `CMakeLists.txt`

**Verify:** All report commands produce identical output: `--report`, `--cmp`, `--summary`, `--summary --history`, `--report --testsuites-only`, `--report --matches-only`.

---

### Stage 8: Simplify `ArenaResults.h` — Separate Data from Queries

**Scope:** Move query methods from `ReportData` into `ResultStore`.

**What:**
1. `ReportData` becomes a pure data struct (no methods except `hasResults()` etc.)
2. Move to `ResultStore`:
   - `findEngine(const EngineId&)` 
   - `getResult(suite, engine)` 
   - `getMatch(engine1, engine2)`
   - `getMatchesForEngine(engine)`
   - `getBenchmarksForEngine(engine)`
   - `enginesMatchFlexibly()` (the private helper)
3. Update `ReportGenerator` to call `ResultStore` query methods (or take the already-extracted data)
4. `EngineId` stays in `ArenaResults.h` — it's a core value type

**Files touched:** `ArenaResults.h`, `ResultStore.h/cpp`, `ReportGenerator.cpp`

**Verify:** Same as Stage 7 — all reports identical.

---

### Stage 9: Clean Up `ArenaConfig` — Eager Expansion

**Scope:** Simplify the config expansion flow.

**What:**
1. Call `expandTestSuiteRuns()` once inside `loadFromYaml()` and store the result in `ArenaConfig::testSuites` (a `vector<TestSuiteConfig>`)
2. Keep `testSuiteRuns` as the raw parsed YAML data (private or removed)
3. Remove the public `expandTestSuiteRuns()` method
4. Update all callers to use `config.testSuites` directly
5. `TestSuiteRunConfig` and `SuiteOverride` remain for YAML parsing but are not part of the public API

**Files touched:** `ArenaConfig.h/cpp`, `ArenaRunner.cpp`, `TestSuiteRunner.cpp`, `engine_arena_main.cpp`

**Verify:** Run `--testsuites` — same results. Check that `config.testSuites.size()` matches `config.expandTestSuiteRuns().size()` in the old code.

---

### Stage 10: Clean Up Dead Code and Polish

**Scope:** Remove dead code, fix minor issues.

**What:**
1. Remove `writeComparison()` placeholder from `ResultStore` (was in old `ResultWriter`)
2. Remove any unused includes
3. Ensure all new files have proper copyright headers and header guards
4. Ensure `CMakeLists.txt` lists all new files correctly
5. Verify all tests compile and pass
6. Review and fix any `const`-correctness issues introduced during refactoring

**Files touched:** Various — cleanup pass.

**Verify:** Full rebuild. Run all tests. Run each CLI mode once.

---

### Stage 11: Update Documentation

**Scope:** Bring documentation in sync with the refactored code.

**What:**
1. Update the header comment blocks in all new/modified `.h` files to match the project style (banner + overview)
2. Update `PLAN_Arena_Tags_and_Summary.md` status to ✅ COMPLETE (the feature it tracks is done)
3. Mark this plan `PLAN_Arena_Refactor.md` as ✅ COMPLETE
4. Update the workspace section of `CLAUDE.md` if the Arena file list changed
5. Update `.github/copilot-instructions.md` if the Architecture section mentions Arena files

**Files touched:** Various `.h` files, `CLAUDE.md`, `copilot-instructions.md`, this document.

---

### Stage 12: Self-Critical Review of Full Arena Codebase

**Scope:** Read every Arena source file end-to-end and produce a detailed review.

**What:**
1. Read all `engine_arena/*.h`, `engine_arena/*.cpp`, and `engine_arena_main.cpp` in full
2. Review against the original issue list (A1–A5, C1–C6) — verify each issue is fully resolved
3. Check for **new problems introduced by the refactoring**:
   - Inconsistent error handling between old and new code paths
   - Missing `const`-correctness in new/modified code
   - Broken or stale comments that reference old structure
   - Include hygiene (unused includes, missing includes that compile only by accident)
   - Naming consistency across old and new files
   - Any code duplication re-introduced during extraction
4. Check **API consistency**:
   - Are `ResultStore` and `ReportGenerator` APIs clean and orthogonal?
   - Does `ArenaRunner` still have responsibilities that belong elsewhere?
   - Are there unnecessary coupling points (e.g., `ReportGenerator` reaching into internals)?
5. Check **edge cases**:
   - Empty result directories, missing files, malformed JSON
   - Engine name matching: verify the single canonical implementation is used everywhere
   - Config with zero suites, zero matches, zero benchmarks
6. Verify all `[[nodiscard]]`, header guards, copyright headers follow project conventions
7. Present findings to user as a prioritized list — **do not make changes in this stage**

**Files touched:** None (read-only analysis).

**Output:** Prioritized list of findings, categorized as: 🔴 Must Fix, 🟡 Should Fix, 🟢 Nice to Have.

---

### Stage 13: Fix Issues from Review

**Scope:** Apply fixes identified in Stage 12.

**What:**
1. Address all 🔴 Must Fix items from Stage 12
2. Address 🟡 Should Fix items (unless user defers specific ones)
3. Apply 🟢 Nice to Have items at user's discretion
4. Each fix should be a clear, minimal change — no scope creep
5. User tests after all fixes are applied

**Files touched:** Depends on Stage 12 findings.

**Verify:** Full rebuild. Run all tests. Run each CLI mode once. Confirm all Stage 12 findings are resolved.

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
| 6     | Extract `ResultStore` (replaces `ResultWriter`)                 | Medium   | A1 (part)     | 3 modified, 2 new, 2 deleted |
| 7     | Extract `ReportGenerator`                                       | Medium   | A1 (major)    | 3 modified, 2 new            |
| 8     | Simplify `ArenaResults.h` (data-only)                           | Low      | A3, A4        | 3 modified                   |
| 9     | Eager expansion in `ArenaConfig`                                | Low      | A5            | 4 modified                   |
| 10    | Dead code cleanup & polish                                      | Very Low | C5            | Various                      |
| 11    | Update documentation                                            | None     | —             | Docs only                    |
| 12    | Self-critical review of full Arena codebase                     | None     | All           | None (read-only)             |
| 13    | Fix issues from review                                          | Low      | From Stage 12 | Depends on findings          |

---

*Last updated: 2026-03-08*
