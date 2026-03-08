# FrankyCPP Arena: Unified Config with Tags and Engine Summary

**Document Version:** 1.1  
**Created:** 2026-03-08  
**Last Updated:** 2026-03-08  
**Status:** 🚧 IN PROGRESS  
**Target:** FrankyCPP Arena v1.6+  
**Priority:** Medium (Development workflow improvement)

---

## Executive Summary

Restructure the Arena configuration to use a single run config for multiple test suites, add a `tag` field across all result types (test suites, matches, benchmarks) to track feature development progression, and implement `--summary` command for viewing engine results.

**Problem:** Currently, each test suite requires a separate config block with duplicated settings (enginePath, timePerMove, etc.). Additionally, there's no way to:
1. Tag runs with feature names (e.g., "QuietSee", "TTbuckets") to track progression
2. View a comprehensive summary of all test results for a specific engine version
3. See historical progression of results during version development

**Solution:** 
- New `testSuiteRuns:` config format with shared settings and suite list
- `tag` field on all result types (test suites, matches, benchmarks)
- `--summary <engine>` command showing aggregated results
- `--history` flag showing progression by tag

---

## Current vs. New Configuration Format

### Current Format (Repetitive)

```yaml
testSuites:
  - name: "franky_tests_v1.5"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000
    maxDepth: 99
    enginePath: "Release/FrankyCPP_v1.5/FrankyCPP_v1.5.exe"
    engineVersion: "v1.5"
    isolatePositions: true
    debugMode: false
    commandLineArgs: "--nobook"
    uciOptions: ""
    parallelWorkers: 2

  - name: "WAC_v1.5"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000                                        # duplicated!
    maxDepth: 99                                             # duplicated!
    enginePath: "Release/FrankyCPP_v1.5/FrankyCPP_v1.5.exe"  # duplicated!
    engineVersion: "v1.5"                                    # duplicated!
    isolatePositions: true                                   # duplicated!
    debugMode: false                                         # duplicated!
    commandLineArgs: "--nobook"                              # duplicated!
    uciOptions: ""                                           # duplicated!
    parallelWorkers: 2                                       # duplicated!
  
  # ... 5 more suites with identical settings ...
```

### New Format (Unified)

```yaml
testSuiteRuns:
  - engine: "FrankyCPP v1.5"
    engineVersion: "v1.5"
    tag: "QuietSee"
    enginePath: "Release/FrankyCPP_v1.5/FrankyCPP_v1.5.exe"
    timePerMove: 5000
    maxDepth: 99
    isolatePositions: true
    debugMode: false
    commandLineArgs: "--nobook"
    uciOptions: ""
    parallelWorkers: 2
    suites:
      - "test/testsets/franky_tests.epd"
      - "test/testsets/mate_test_suite.epd"
      - "test/testsets/wac.epd"
      - "test/testsets/STS1-STS15_LAN.EPD"
      - "test/testsets/crafty_test.epd"
      - "test/testsets/ecm98.epd"
      - "test/testsets/kaufman.epd"
```

**Benefits:**
- 7 suites × 9 duplicated fields = 63 lines → 1 block with 7-item list
- Single place to update settings
- `tag` captures what feature is being tested
- Suite names derived from EPD filename (e.g., `wac.epd` → `wac`)

---

## Tag Field Across All Result Types

### Test Suite Results
```json
// In JSON result file
{
  "tag": "QuietSee",
  "testSuite": { "name": "wac", ... },
  "engine": { "name": "FrankyCPP v1.5", "version": "v1.5", ... },
  ...
}
```

### Match Results
```yaml
# In arena.yaml
matches:
  - name: "v1.5_vs_v1.4_300s"
    tag: "QuietSee"                    # NEW FIELD
    engine1Path: "Release/FrankyCPP_v1.5/FrankyCPP_v1.5.exe"
    engine1Version: "v1.5"
    ...
```

### Benchmark Results
```yaml
# In arena.yaml (rename notes → tag)
benchmarks:
  - name: "v1.5 Internal"
    tag: "QuietSee"                    # RENAMED FROM notes
    engineVersion: "v1.5"
    depth: 12
    ...
```

---

## New CLI Commands

### `--summary <engine>` — Show Latest Results

```
$ FrankyCPP_Arena --summary FrankyCPP-v1.5

===================================================================
Engine Summary: FrankyCPP v1.5 [QuietSee]
===================================================================
Test Suites (2026-03-08 01:53):
  franky_tests:    12/13   (92.31%)
  mate_test_suite: 16/20   (80.00%)
  wac:             193/201 (96.02%)
  STS1-STS15_LAN:  772/1500 (51.47%)
  crafty_test:     178/346 (51.45%)
  ecm98:           535/769 (69.57%)
  kaufman:         21/25   (84.00%)
-------------------------------------------------------------------
  TOTAL:           1727/2874 (60.09%)
  Total Nodes:     89,838,967,086
  Total Time:      3h 18m 45s
===================================================================

Matches:
  vs v1.4 (300+0) [TTbuckets]:  63-41 (60.6%)  W:42 D:42 L:20  +75 ELO
  vs v1.3 (300+0) [QuietSee]:   81-23 (77.9%)  W:69 D:24 L:11  +219 ELO
===================================================================
```

**Engine name matching:** Accepts ` `, `_`, `-` as equivalent separators:
- `FrankyCPP-v1.5`
- `FrankyCPP_v1.5`
- `FrankyCPP v1.5`

All match the same engine.

### `--summary <engine> --history` — Show Progression by Tag

> **Note:** `--history` only works in combination with `--summary`. It has no effect when used standalone.

```
$ FrankyCPP_Arena --summary FrankyCPP-v1.5 --history

===================================================================
Engine History: FrankyCPP v1.5
===================================================================

--- [TTbuckets] 2026-03-06 ---
  TOTAL: 1722/2874 (59.92%)  Nodes: 88.2B  Time: 3h 15m
  Matches:
    vs v1.4 (300+0):  63-41 (60.6%)  +75 ELO

--- [QuietSee] 2026-03-08 ---
  TOTAL: 1727/2874 (60.09%)  Nodes: 89.8B  Time: 3h 19m
  Delta vs [TTbuckets]: +5 positions (+0.17%)
  Matches:
    vs v1.4 (300+0):  65-39 (62.5%)  +88 ELO
    vs v1.3 (300+0):  81-23 (77.9%)  +219 ELO
===================================================================
```

**Grouping:** Results are grouped by `tag`, not by timestamp. This shows the progression of features during version development.

---

## Implementation Steps

### Step 1: Add `TestSuiteRunConfig` struct
**File:** `src/engine_arena/ArenaConfig.h`

```cpp
/// Per-suite override settings (optional)
struct SuiteOverride {
  std::string path;                       ///< EPD file path (required)
  std::optional<milliseconds> timePerMove;///< Override time limit
  std::optional<Depth> maxDepth;          ///< Override depth limit
};

/// Configuration for a grouped test suite run
struct TestSuiteRunConfig {
  std::string engine;             ///< Display name: "FrankyCPP v1.5"
  std::string engineVersion;      ///< Version for grouping: "v1.5"
  std::string tag;                ///< Feature tag: "QuietSee"
  std::string enginePath;         ///< Path to executable
  milliseconds timePerMove;       ///< Time limit per move
  Depth maxDepth;                 ///< Maximum search depth
  bool isolatePositions = true;   ///< Clear state between positions
  bool debugMode = false;         ///< Print UCI communication
  std::string commandLineArgs;    ///< Command-line arguments
  std::string uciOptions;         ///< UCI options
  int parallelWorkers = 1;        ///< Parallel workers
  std::vector<std::variant<std::string, SuiteOverride>> suites; ///< EPD paths or overrides
};
```

**YAML format supports both simple paths and override objects:**
```yaml
suites:
  - "test/testsets/wac.epd"                    # Simple path
  - "test/testsets/crafty_test.epd"            # Simple path
  - path: "test/testsets/mate_test_suite.epd"  # Override object
    timePerMove: 15000                         # Custom time for this suite
  - path: "test/testsets/STS1-STS15_LAN.EPD"
    maxDepth: 20                               # Custom depth for this suite
```

Update `ArenaConfig`:
```cpp
struct ArenaConfig {
  // ...existing fields...
  std::vector<TestSuiteRunConfig> testSuiteRuns;  ///< NEW: Grouped test suite runs
  // Remove: std::vector<TestSuiteConfig> testSuites;
};
```

### Step 2: Add `tag` field to existing configs
**File:** `src/engine_arena/ArenaConfig.h`

```cpp
struct MatchConfig {
  // ...existing fields...
  std::string tag;  ///< Feature tag: "QuietSee"
};

struct BenchmarkConfig {
  // ...existing fields...
  std::string tag;  ///< Feature tag (renamed from notes)
  // Remove: std::string notes;
};
```

### Step 3: Update YAML parsing
**File:** `src/engine_arena/ArenaConfig.cpp`

- Parse `testSuiteRuns:` section
- Handle both simple string paths and override objects in `suites:` list
- Expand each run into individual `TestSuiteConfig` entries internally
- Derive suite name from EPD filename: `test/testsets/wac.epd` → `wac`
- Parse `tag` for matches and benchmarks

### Step 4: Add tag validation
**File:** `src/engine_arena/ArenaConfig.cpp` (in `validate()` method)

**Validation checks:**
1. Warn if `tag` is empty - results won't be grouped by feature
2. Warn if `tag` matches the last run's tag for same engine - likely forgot to update YAML

**Rationale:** Since the tag is set in the YAML config file, it's easy to forget to update it between runs. Warning helps catch this common mistake.

```cpp
// In ArenaConfig::validate()

// Check for empty tags
for (const auto& run : testSuiteRuns) {
  if (run.tag.empty()) {
    std::cerr << "WARNING: Test suite run for '" << run.engine 
              << "' has empty tag - results won't be grouped by feature\n";
  }
}
for (const auto& match : matches) {
  if (match.tag.empty()) {
    std::cerr << "WARNING: Match '" << match.name 
              << "' has empty tag - results won't be grouped by feature\n";
  }
}

// Check for duplicate tags (same as last run)
// This requires loading the last result for each engine to compare tags
// Implementation in ArenaRunner::validateTagsAgainstHistory()
```

**File:** `src/engine_arena/ArenaRunner.cpp` (new method)

```cpp
void ArenaRunner::validateTagsAgainstHistory(const ArenaConfig& config) const {
  const auto data = loadAllResults();
  
  for (const auto& run : config.testSuiteRuns) {
    if (run.tag.empty()) continue;  // Already warned about empty
    
    const EngineId engineId{run.engine, run.engineVersion};
    
    // Find latest result for this engine
    std::string lastTag;
    for (const auto& [suiteName, suiteResults] : data.suiteResults) {
      const auto it = suiteResults.find(engineId);
      if (it != suiteResults.end() && !it->second.tag.empty()) {
        lastTag = it->second.tag;
        break;  // Found one, they should all have same tag
      }
    }
    
    if (!lastTag.empty() && lastTag == run.tag) {
      std::cerr << "WARNING: Tag '" << run.tag << "' for engine '" << run.engine 
                << "' is the same as the last run - did you forget to update the tag?\n";
    }
  }
  
  // Similar check for matches
  for (const auto& match : config.matches) {
    if (match.tag.empty()) continue;
    
    // Find latest match result with same engine pair
    for (const auto& [matchKey, result] : data.matchResults) {
      if (!result.tag.empty() && result.tag == match.tag) {
        // Check if this is the same engine pair
        const EngineId e1{match.engine1Path, match.engine1Version};  // simplified
        if (result.getEngine1Id() == e1 || result.getEngine2Id() == e1) {
          std::cerr << "WARNING: Tag '" << match.tag << "' for match '" << match.name 
                    << "' is the same as a previous run - did you forget to update the tag?\n";
          break;
        }
      }
    }
  }
}
```

### Step 5: Add `tag` field to result structs
**File:** `src/engine_arena/ArenaResults.h`

```cpp
struct TestSuiteResult {
  // ...existing fields...
  std::string tag;  ///< Feature tag: "QuietSee"
};

struct MatchResult {
  // ...existing fields...
  std::string tag;  ///< Feature tag: "QuietSee"
};

struct BenchmarkResult {
  // ...existing fields...
  std::string tag;  ///< Feature tag (renamed from notes)
  // Remove: std::string notes;
};
```

### Step 6: Update JSON serialization
**File:** `src/engine_arena/ResultWriter.cpp`

- Write/read `tag` field in all result types
- Backwards compatibility: Old results without `tag` field load with empty string

### Step 7: Add `tag` to `TestSuiteConfig` and update `TestSuiteRunner`
**Files:** `src/engine_arena/ArenaConfig.h`, `src/engine_arena/TestSuiteRunner.cpp`

**Note:** `TestSuiteRunner` continues to accept `TestSuiteConfig` (not `TestSuiteRunConfig`). The expansion from `testSuiteRuns:` to individual `TestSuiteConfig` entries happens during YAML parsing in Step 3.

Add `tag` field to `TestSuiteConfig`:
```cpp
struct TestSuiteConfig {
  // ...existing fields...
  std::string tag;  ///< Feature tag: "QuietSee" (propagated from TestSuiteRunConfig)
};
```

Update `TestSuiteRunner::runTestSuite()` to propagate `tag` to result:
```cpp
TestSuiteResult result;
// ...existing code...
result.tag = suiteConfig.tag;  // NEW: propagate tag from config
```

### Step 8: Add `EngineId::matchesFlexible()` method
**File:** `src/engine_arena/ArenaResults.h`

```cpp
struct EngineId {
  // ...existing code...
  
  /// Matches engine IDs with flexible separators (space, underscore, hyphen)
  /// e.g., "FrankyCPP-v1.5" matches "FrankyCPP v1.5" and "FrankyCPP_v1.5"
  [[nodiscard]] bool matchesFlexible(const std::string& input) const;
  
  /// Parse from string with flexible separators
  static EngineId fromStringFlexible(const std::string& str);
};
```

### Step 9: Add `loadAllResultsWithHistory()` method
**File:** `src/engine_arena/ArenaRunner.cpp`

- Keep ALL runs per engine (not just latest)
- Sort by timestamp
- Group by `tag`

### Step 10: Add `generateEngineSummary()` method
**Files:** `src/engine_arena/ArenaRunner.h`, `ArenaRunner.cpp`

- Generate formatted summary output
- Support both latest-only and history modes
- Calculate deltas between tags in history mode

### Step 11: Add CLI options
**File:** `src/engine_arena_main.cpp`

```cpp
desc.add_options()
  // ...existing options...
  ("summary",  po::value<std::string>(), "Show summary for engine: --summary FrankyCPP-v1.5")
  ("history",  "Show all historical runs (requires --summary)");
```

**Validation:** If `--history` is used without `--summary`, print error and exit.

### Step 12: Migrate arena.yaml
**File:** `config/arena.yaml`

Convert to new format with `testSuiteRuns:` and `tag` fields.

---

## File Change Summary

| File                    | Changes                                                                                                                                                   |
|-------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------|
| `ArenaConfig.h`         | Add `TestSuiteRunConfig`, `SuiteOverride`; add `tag` to `TestSuiteConfig` and `MatchConfig`; rename `notes`→`tag` in `BenchmarkConfig`; add `testSuiteRuns` |
| `ArenaConfig.cpp`       | Parse `testSuiteRuns:` YAML format, expand to `TestSuiteConfig` entries; parse `tag` for matches/benchmarks                                               |
| `ArenaResults.h`        | Add `tag` to `TestSuiteResult`/`MatchResult`/`BenchmarkResult`; add `EngineId::matchesFlexible()`                                                         |
| `ResultWriter.cpp`      | Serialize/deserialize `tag` field (backwards-compatible read)                                                                                             |
| `TestSuiteRunner.cpp`   | Propagate `tag` from `TestSuiteConfig` to `TestSuiteResult`                                                                                               |
| `ArenaRunner.h/cpp`     | Add `loadAllResultsWithHistory()`, `generateEngineSummary()`, `validateTagsAgainstHistory()`                                                              |
| `engine_arena_main.cpp` | Add `--summary`, `--history` options with validation                                                                                                      |
| `arena.yaml`            | Migrate to `testSuiteRuns:` format with `tag` fields                                                                                                      |

---

## Backwards Compatibility

### Breaking Changes (Clean Break)
- Old `testSuites:` YAML format **no longer supported** — migration required
- `BenchmarkConfig::notes` renamed to `tag`
- **Rationale:** Test suite runs are short enough to re-run with older engine versions if needed. Clean break simplifies the codebase.

### Non-Breaking
- Old JSON result files without `tag` field load with empty tag
- Existing functionality unchanged
- Result file structure remains **flat** (tag stored in JSON content only, not in file paths)

---

## Validation Checklist

- [x] `FrankyCPP_Arena --testsuites` runs all suites from `testSuiteRuns` config
- [x] Per-suite overrides work (custom `timePerMove`, `maxDepth` per suite)
- [x] Empty tag triggers warning during validation
- [ ] Duplicate tag (same as last run) triggers warning during validation
- [x] Results JSON files include `tag` field
- [x] `FrankyCPP_Arena --summary FrankyCPP-v1.5` shows aggregated results
- [ ] `FrankyCPP_Arena --summary FrankyCPP-v1.5 --history` shows progression by tag
- [x] Flexible engine name matching works (`-`, `_`, ` ` equivalent)
- [x] Match results show with `tag` in summary
- [x] Benchmark results use `tag` instead of `notes`

---

## Future Enhancements

1. **Tag autocomplete / suggestions** — When `--summary` doesn't find an exact match:
   - Search for partial matches in engine name or version
   - Display list of similar/available engines with their tags
   - Example:
     ```
     $ FrankyCPP_Arena --summary v1.5
     
     Engine "v1.5" not found. Did you mean:
       FrankyCPP-v1.5  [QuietSee, TTbuckets]  (2 tags, 14 test runs, 3 matches)
       FrankyCPP-v1.5-dev  [WIP]  (1 tag, 7 test runs)
     
     Usage: --summary FrankyCPP-v1.5
     ```
   - Could also support `--summary --list` to show all available engines with tag counts

---

*Document created: 2026-03-08*
