# Arena Results Redesign Specification

**Status:** Draft - Discussion Phase  
**Created:** 2026-02-05  
**Last Updated:** 2026-02-05

---

## 1. What Do We Want to Measure and Understand?

### 1.1 Primary Goals

The arena system exists to answer these fundamental questions:

#### A. Engine Progress Tracking (PRIMARY)
- **"Is the engine improving over time?"**
  - Core question: **"Did my latest changes make the engine stronger or weaker?"**
  - Track performance across versions (v0.5 → v1.0 → v1.1 → v1.2-dev)
  - Identify regressions early
  - Validate that changes improve (or don't hurt) playing strength

#### B. Benchmark Against External Engines (SECONDARY)
- **"How does FrankyCPP compare to other engines?"**
  - Compare against FrankyGo, Stockfish, etc.
  - Establish relative strength in the engine ecosystem
  - Useful context, but not the primary focus

#### ~~C. Arbitrary Engine Strength Comparison~~ (NOT A GOAL)
- ~~"Is engine X stronger than engine Y?"~~
- This is NOT a general-purpose engine comparison tool
- We only care about comparing FrankyCPP versions against each other and selected baselines

### 1.2 Specific Metrics We Want

#### For Test Suites (EPD/tactical tests):
| Metric                     | Description                                            | Why It Matters                              |
|----------------------------|--------------------------------------------------------|---------------------------------------------|
| **Solve Rate**             | % of positions where engine finds the expected move    | Core measure of tactical/positional ability |
| **Solve Rate by Category** | Breakdown by test suite theme (tactics, endgame, etc.) | Identifies specific strengths/weaknesses    |
| **Time to Solve**          | Average/median time to find correct move               | Speed of tactical recognition               |
| **Nodes to Solve**         | Average/median nodes searched                          | Search efficiency                           |
| **Depth Reached**          | Average search depth within time limit                 | Search speed indicator                      |

#### For Matches (head-to-head games):
| Metric | Description | Why It Matters |
|--------|-------------|----------------|
| **Win/Draw/Loss** | Game outcomes | Direct strength comparison |
| **ELO Difference** | Calculated rating difference | Standardized strength measure |
| **Score %** | Points scored (win=1, draw=0.5, loss=0) | Alternative to ELO |
| **Game Length** | Average moves per game | Playing style indicator |
| **Time Usage** | Time management patterns | Efficiency under time pressure |

### 1.3 Comparison Dimensions

We want to compare across these dimensions:

```
                    ┌─────────────────────────────────────────┐
                    │           COMPARISON MATRIX             │
                    ├─────────────────────────────────────────┤
                    │                                         │
   Test Suite ──────┼──► WAC, STS, Crafty, ECM98, mate_test  │
        │           │                                         │
        ▼           │                                         │
   Engine ──────────┼──► v0.5, v1.0, v1.1, FrankyGo          │
        │           │                                         │
        ▼           │                                         │
   Time ────────────┼──► 2026-02-01, 2026-02-05, ...         │
                    │                                         │
                    └─────────────────────────────────────────┘
```

**Key comparisons we want to make:**

1. **Same test suite, different engines**: "On WAC, how does v1.1 compare to v0.5?"
2. **Same engine, different test suites**: "Where does v1.1 excel? Where does it struggle?"
3. **Same engine, over time**: "Has v1.1's WAC score improved since last week?"
4. **Cross-engine head-to-head**: "In direct matches, how does v1.1 fare against FrankyGo?"

### 1.4 Questions the Current System Cannot Answer

Based on the output you showed, the current system fails to answer:

1. ❌ "Compare v1.1 vs v0.5 on the same test suite" 
   - Shows "v0.5: NOT TESTED" even though data exists
   
2. ❌ "Show me all engines' results for WAC side by side"
   - Results are grouped by suite name which includes engine version
   
3. ❌ "Track v1.1's progress over multiple test runs"
   - No clear temporal comparison

### 1.5 Design Decisions ✅

1. **What is the primary use case?**
   - ✅ **Primary: Detect strength changes (regression/improvement)**
   - Core question: **"Did my latest changes make the engine stronger or weaker?"**
   
   For Test Suites:
   - "v1.1 scored 52% on Crafty. Does v1.2-dev do better or worse?"
   - "v1.1 solved 96% of WAC. Did my LMR changes break anything?"
   
   For Matches:
   - "v1.1 beat FrankyGo 55% of the time. Does v1.2-dev beat it more often?"
   - "v1.1 was +20 ELO vs v0.5. Is v1.2-dev even stronger?"
   
   - Benchmark engines: v0.5, v1.1, FrankyGo (stable reference points)
   - Development engine: v1.2-dev / current build (constantly changing)
   - Need to detect regressions quickly after each change

2. **How important is historical tracking?**
   - ✅ **Keep aggregated results (test suite summaries, match summaries)**
   - ✅ **Version results in git** to track progress over time
   - Individual test case details and game PGNs: keep temporarily, can be pruned
   - Key insight: We care about "v1.2 scores 95% on WAC", not every individual position

3. **What about match results vs test suite results?**
   - ✅ **Separate but related** - different sections in reports
   - Test Suites measure: "Can the engine find the right move?" (tactical/positional ability)
   - Matches measure: "Can the engine win games?" (overall playing strength)
   - These don't directly compare (95% solve rate ≠ 95% win rate)
   - Reports should support: TestSuites only, Matches only, or Both

### 1.6 Conceptual Model

Based on these decisions, here's the conceptual model:

```
┌─────────────────────────────────────────────────────────────────┐
│                        BENCHMARK ENGINES                        │
│         (Reference points - relatively stable)                  │
├─────────────────────────────────────────────────────────────────┤
│  • FrankyCPP v0.5    - Historical baseline                      │
│  • FrankyCPP v1.1    - Current stable release                   │
│  • FrankyGo          - External reference (Go-based engine)     │
│  • (future engines)  - Stockfish, Ruffian, etc.                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      DEVELOPMENT ENGINE                         │
│           (Actively changing - needs constant testing)          │
├─────────────────────────────────────────────────────────────────┤
│  • FrankyCPP v1.2-dev (or current build)                        │
│  • After each significant change → run tests → compare          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                         RESULT TYPES                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  TEST SUITE RESULTS              MATCH RESULTS                  │
│  ─────────────────               ─────────────                  │
│  • Per engine                    • Per engine pair              │
│  • Per test suite                • Time control                 │
│  • Solve rate %                  • W/D/L record                 │
│  • Avg time/nodes                • ELO difference               │
│                                                                 │
│         ▼                                  ▼                    │
│  "v1.2 solves 96% of WAC"      "v1.2 beats v1.1 +15 ELO"       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      VERSIONED IN GIT                           │
├─────────────────────────────────────────────────────────────────┤
│  results/                                                       │
│  ├── testsuites/     ← Aggregated results (committed)           │
│  ├── matches/        ← Match summaries (committed)              │
│  └── temp/           ← Raw data, PGNs (gitignored, prunable)    │
└─────────────────────────────────────────────────────────────────┘
```

### 1.7 Key Insight: Engine vs Engine Version

The current confusion stems from mixing:
- **Test Suite Name** (WAC, STS, Crafty) - the test being run
- **Engine** (FrankyCPP, FrankyGo) - the program
- **Engine Version** (v0.5, v1.1, v1.2-dev) - specific build

We need clean separation:
- `WAC` is a test suite
- `FrankyCPP v1.1` is an engine+version
- A result is: **Engine+Version** tested on **TestSuite** at **Timestamp**

---

## 2. Do We Collect This Data Already? ✅ IMPLEMENTED

Let me analyze what the current system captures:

### 2.1 Current JSON Structure (Test Suite Results) - OLD FORMAT

```json
{
  "version": "v1.1",           // ❌ This is ARENA version, not engine version
  "suiteName": "Crafty_v0.5",  // ❌ Engine version embedded in suite name
  "timestamp": "2026-02-05T00:36:45Z",
  "engine": {
    "name": "FrankyCPP v0.5",  // ✅ Engine name with version (combined)
    "path": "Release/FrankyCPP_V0.5/FrankyCPP_v0.5.exe"
  },
  "summary": {
    "totalTests": 345,         // ✅ 
    "passed": 181,             // ✅
    "failed": 164,             // ✅
    "skipped": 0,              // ✅
    "successRate": 52.46,      // ✅
    "totalNodes": 4904088008,  // ✅
    "totalTimeMs": 1363461     // ✅
  },
  "details": [...]             // ✅ Individual test cases (can be pruned)
}
```

### 2.2 What's Missing or Wrong - FIXED

| Field | Issue | Fix |
|-------|-------|-----|
| `version` | Stores arena version, not engine version | ✅ Renamed to `arenaVersion` |
| `suiteName` | Contains engine version (`Crafty_v0.5`) | ✅ Now `testSuite.name` = `Crafty` |
| `engine.version` | Missing! | ✅ Added explicit `engine.version` field |
| `testSuite` | Missing! | ✅ Added clean test suite name |

### 2.3 NEW JSON Structure ✅ IMPLEMENTED

```json
{
  "arenaVersion": "v1.1",
  "timestamp": "2026-02-05T00:36:45Z",
  
  "testSuite": {
    "name": "Crafty",
    "epdPath": "test/testsets/crafty_test.epd"
  },
  
  "engine": {
    "name": "FrankyCPP",
    "version": "v0.5",
    "path": "Release/FrankyCPP_V0.5/FrankyCPP_v0.5.exe"
  },
  
  "summary": {
    "totalTests": 345,
    "passed": 181,
    "failed": 164,
    "skipped": 0,
    "successRate": 52.46,
    "totalNodes": 4904088008,
    "totalTimeMs": 1363461,
    "avgTimeMs": 3952,
    "avgNodes": 14214748
  },
  
  "details": [...]  // Optional, can be in separate file or pruned
}
```

**Note:** Engine version is now explicitly configured in `arena.yaml` via `engineVersion` field,
not parsed from UCI `id name` response (which has no standard format for version).
```

### 2.4 NEW File Naming ✅ IMPLEMENTED

Old: `v1.1_Crafty_v0.5_20260205_013646.json`

**New:** `{TestSuite}_{EngineName}-{EngineVersion}_{Timestamp}.json`

Example: `Crafty_FrankyCPP-v0.5_20260205_013646.json`

---

## 3. How Do We Want to Report This Data?

### 3.1 Report Types

Based on the primary question "Did my changes make the engine stronger or weaker?", we need these report types:

| Report Type | Purpose | When to Use |
|-------------|---------|-------------|
| **Comparison Report** | Compare dev version against baseline(s) | After changes, before commit |
| **Baseline Report** | Show current state of all benchmark engines | Establish reference points |
| **Regression Report** | Quick pass/fail check against thresholds | CI/CD, quick sanity check |

### 3.2 Comparison Report (Primary)

This is the main report answering: "How does Engine A compare to Engine B (and C, D...)?"

#### Example Output:

```
================================================================================
ENGINE COMPARISON REPORT
================================================================================
Generated: 2026-02-05 14:30:00
Comparing: FrankyCPP v1.2-dev vs baselines

BASELINES: FrankyCPP v0.5, FrankyCPP v1.1, FrankyGo
================================================================================

TEST SUITE RESULTS
--------------------------------------------------------------------------------
                        v1.2-dev    v1.1        v0.5        FrankyGo
                        --------    ----        ----        --------
WAC (201 positions)
  Solved:               195         194         188         197
  Rate:                 97.0%       96.5%       93.5%       98.0%
  vs v1.1:              +1 (+0.5%)  baseline    -6 (-3.0%)  +3 (+1.5%)
  Avg Time:             1.2s        1.4s        1.8s        0.9s

Crafty (345 positions)
  Solved:               184         176         181         165
  Rate:                 53.3%       51.0%       52.5%       47.8%
  vs v1.1:              +8 (+2.3%)  baseline    +5 (+1.5%)  -11 (-3.2%)
  Avg Time:             4.1s        4.0s        3.9s        3.8s

STS (1500 positions)
  Solved:               780         755         757         685
  Rate:                 52.0%       50.3%       50.5%       45.7%
  vs v1.1:              +25 (+1.7%) baseline    +2 (+0.2%)  -70 (-4.6%)
  Avg Time:             2.8s        3.0s        3.2s        2.5s

ECM98 (769 positions)
  Solved:               530         521         535         526
  Rate:                 68.9%       67.8%       69.6%       68.4%
  vs v1.1:              +9 (+1.1%)  baseline    +14 (+1.8%) +5 (+0.6%)
  Avg Time:             3.5s        3.6s        3.4s        3.2s

mate_test (20 positions)
  Solved:               16          14          15          18
  Rate:                 80.0%       70.0%       75.0%       90.0%
  vs v1.1:              +2 (+10.0%) baseline    +1 (+5.0%)  +4 (+20.0%)
  Avg Time:             8.2s        9.1s        10.5s       6.3s

--------------------------------------------------------------------------------
TEST SUITE SUMMARY (v1.2-dev vs v1.1)
--------------------------------------------------------------------------------
  Total positions:      2835
  Improvement:          +45 positions (+1.6%)
  Regressions:          0 test suites worse
  Status:               ✅ IMPROVEMENT
--------------------------------------------------------------------------------

MATCH RESULTS
--------------------------------------------------------------------------------
v1.2-dev vs v1.1 (100 games, 60+0.6):
  Score:                +56.5 - 43.5 (56.5%)
  W/D/L:                32 / 49 / 19
  ELO diff:             +45 ± 30
  Status:               ✅ STRONGER

v1.2-dev vs v0.5 (100 games, 60+0.6):
  Score:                +71.0 - 29.0 (71.0%)
  W/D/L:                52 / 38 / 10
  ELO diff:             +156 ± 35
  Status:               ✅ STRONGER

v1.2-dev vs FrankyGo (100 games, 60+0.6):
  Score:                +48.5 - 51.5 (48.5%)
  W/D/L:                25 / 47 / 28
  ELO diff:             -10 ± 30
  Status:               ⚖️  EQUAL (within margin)

--------------------------------------------------------------------------------
MATCH SUMMARY (v1.2-dev)
--------------------------------------------------------------------------------
  vs v1.1:              +45 ELO (stronger)
  vs v0.5:              +156 ELO (much stronger)
  vs FrankyGo:          -10 ELO (equal)
  Status:               ✅ OVERALL IMPROVEMENT
--------------------------------------------------------------------------------

OVERALL VERDICT
================================================================================
  Test Suites:          ✅ +45 positions (+1.6%)
  Matches vs v1.1:      ✅ +45 ELO
  
  RECOMMENDATION:       ✅ Changes improve engine strength
================================================================================
```

### 3.3 Key Design Principles for Reports

1. **Delta-focused**: Always show the change from baseline, not just absolute numbers
2. **Configurable primary baseline**: Default to latest stable (e.g., v1.1), but configurable via CLI or config file
3. **Quick verdict**: ✅/❌/⚖️ symbols for instant assessment
4. **Grouped by test suite**: Easy to see which areas improved/regressed
5. **Summary at end**: Overall recommendation without reading details

### 3.4 Baseline Report

Shows current state of all benchmark engines (no comparison, just facts):

```
================================================================================
BASELINE RESULTS - All Engines
================================================================================
Generated: 2026-02-05
================================================================================

TEST SUITE: WAC (201 positions, 5s/move)
--------------------------------------------------------------------------------
Engine              Solved      Rate        Avg Time    Avg Nodes
------              ------      ----        --------    ---------
FrankyGo            197         98.0%       0.9s        12.4M
FrankyCPP v1.1      194         96.5%       1.4s        18.2M
FrankyCPP v0.5      188         93.5%       1.8s        22.1M

TEST SUITE: Crafty (345 positions, 5s/move)
--------------------------------------------------------------------------------
Engine              Solved      Rate        Avg Time    Avg Nodes
------              ------      ----        --------    ---------
FrankyCPP v0.5      181         52.5%       3.9s        48.3M
FrankyCPP v1.1      176         51.0%       4.0s        45.1M
FrankyGo            165         47.8%       3.8s        42.0M

... (more test suites)

MATCHES: Head-to-Head Records
--------------------------------------------------------------------------------
                    v1.1        v0.5        FrankyGo
                    ----        ----        --------
v1.1                -           +110 ELO    +15 ELO
v0.5                -110 ELO    -           -95 ELO
FrankyGo            -15 ELO     +95 ELO     -
================================================================================
```

### 3.5 Regression Report (Quick Check)

For CI/CD or quick sanity checks - just pass/fail:

```
================================================================================
REGRESSION CHECK: v1.2-dev vs v1.1
================================================================================
Test Suites:    ✅ PASS  (+45 positions, no suite regressed >2%)
Matches:        ✅ PASS  (+45 ELO vs v1.1)

OVERALL:        ✅ PASS - No regressions detected
================================================================================
```

Or if there's a problem:

```
================================================================================
REGRESSION CHECK: v1.2-dev vs v1.1
================================================================================
Test Suites:    ❌ FAIL  WAC regressed -8 positions (-4.0%)
Matches:        ⚠️ WARN  -20 ELO vs v1.1 (within noise margin)

OVERALL:        ❌ FAIL - Regression detected in WAC
================================================================================
```

### 3.6 Report Commands

Proposed CLI interface:

```bash
# Compare dev build against all baselines (primary use case)
FrankyCPP_Arena --compare v1.2-dev

# Compare against specific baseline (override default)
FrankyCPP_Arena --compare v1.2-dev --baseline v1.1

# Compare against multiple specific baselines
FrankyCPP_Arena --compare v1.2-dev --baseline v1.1 --baseline v0.5

# Show baseline report (all engines, no comparison)
FrankyCPP_Arena --baselines

# Quick regression check (for CI) - uses configured primary baseline
FrankyCPP_Arena --regression-check v1.2-dev
# Exit code: 0 = pass, 1 = fail

# Quick regression check against specific baseline
FrankyCPP_Arena --regression-check v1.2-dev --baseline v1.1

# Filter by result type
FrankyCPP_Arena --compare v1.2-dev --testsuites-only
FrankyCPP_Arena --compare v1.2-dev --matches-only
```

### 3.7 Configuration (arena.yaml)

The primary baseline and other defaults should be configurable:

```yaml
# arena.yaml
version: "v1.1"
resultsDir: "./results"

# Comparison settings
comparison:
  primaryBaseline: "FrankyCPP-v1.1"    # Default baseline for comparisons
  baselines:                           # All available baselines
    - "FrankyCPP-v0.5"
    - "FrankyCPP-v1.1"
    - "FrankyGo"
  
  # Regression thresholds
  regressionThresholds:
    testSuiteDropPercent: 2.0          # Fail if any suite drops >2%
    matchEloDropMargin: 30             # Warn if ELO drops >30
```

### 3.8 Open Questions

1. **What thresholds define "regression"?**
   - Test suites: >2% drop on any suite? >1% drop overall?
   - Matches: >30 ELO drop? Statistical significance?

2. **How to handle missing data?**
   - If v1.2-dev hasn't run WAC yet, show "NOT RUN" vs "NOT TESTED"
   - Should comparison fail if data is incomplete?

3. **Output formats?**
   - Console (default) - colored output with symbols
   - Plain text - for logs/files
   - JSON - for tooling/automation
   - Markdown - for documentation/reports

---

## 4. Implementation Plan

*(To be filled in after discussing Section 3)*

---

## Revision History

| Date | Changes |
|------|---------|
| 2026-02-05 | Initial draft - Section 1 discussion |
| 2026-02-05 | Added Section 2 (data collection) and Section 3 (reporting) |
| 2026-02-05 | ✅ Implemented Section 2: New JSON structure, file naming, backward compat |
