# FrankyCPP Engine Arena - Quick Start Guide

## Overview

The **Engine Arena** is a standalone testing framework for measuring and tracking chess engine strength across versions. It provides automated EPD tactical test suites and engine-vs-engine matches with detailed result tracking and version comparison.

**Key Features:**
- 🎯 **EPD Test Suites** - Run tactical puzzles (WAC, STS, etc.) and track solve rates
- ⚔️ **Engine Matches** - Run automated matches via cutechess-cli with ELO calculation
- 📊 **Version Comparison** - Compare tactical strength and match results between versions
- 💾 **Result Persistence** - JSON output for historical tracking and automation
- 📈 **Detailed Reports** - See exactly which positions improved/regressed

---

## Quick Start

### Prerequisites

1. **Build the Arena executable:**
   ```powershell
   cmake --build cmake-build-win-release --config Release
   ```

2. **Install cutechess-cli** (for matches only):
   - Download from: https://github.com/cutechess/cutechess
   - Update path in `config/arena.yaml`

### Run All Tests

From the project root:

```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe
```

This runs all configured test suites and matches, saving results to `results/`.

### Run Test Suites Only

```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --testsuites
```

**Output:**
```
===================================================================
Running All Test Suites
===================================================================
Engine Version: v1.1
Number of Test Suites: 2

[1/2] Running Test Suite: franky_tests
  Total Tests:  50
  Passed:       50 (100.0%)
  Failed:       0
  Total Time:   2500ms

[2/2] Running Test Suite: WAC
  Total Tests:  300
  Passed:       285 (95.0%)
  Failed:       15
  Total Time:   85000ms
===================================================================
```

### Run Matches Only

```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --matches
```

Runs engine-vs-engine matches defined in `config/arena.yaml`.

### View Baseline Report (All Engines)

```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --report
```

Shows all engines side-by-side for all test suites and matches.

### Compare Target Engine vs Baselines

```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --cmp FrankyCPP-v1.2-dev
```

**Example Output:**
```
================================================================================
TEST SUITE COMPARISON: FrankyCPP v1.2-dev vs Baselines
================================================================================
Baseline: FrankyCPP v1.1

--------------------------------------------------------------------------------
Test Suite           Target Pass    Baseline Pass    Delta       Status
--------------------------------------------------------------------------------
WAC                  288/300        285/300          +3          [+] BETTER
franky_tests         50/50          50/50            +0          [=] EQUAL
STS1-STS15_LAN       952/1500       945/1500         +7          [+] BETTER
================================================================================


================================================================================
MATCH COMPARISON: FrankyCPP v1.2-dev vs Baselines
================================================================================
--------------------------------------------------------------------------------
Opponent             Games   Score      W/D/L       ELO         vs v1.1
--------------------------------------------------------------------------------
FrankyCPP v1.1       100     58.0%      35/46/19    +56         [baseline]
FrankyCPP v0.5       100     72.0%      55/34/11    +165        +109 ELO
FrankyGo v1.0.3      100     51.5%      28/47/25    +10         -46 ELO

SUMMARY
--------------------------------------------------------------------------------
vs v1.1:    +56 ELO   ✅ STRONGER
Status:      ✅ IMPROVEMENT over baseline
================================================================================
```

### Filter Reports by Type

```powershell
# Show only test suite results
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --report --testsuites-only

# Show only match results
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --report --matches-only

# Compare only matches
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --cmp FrankyCPP-v1.2-dev --matches-only
```

### List Available Engines

```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --engines
```

Shows all engines found in stored results.

---

## Common Workflows

### 1. Test Current Engine

```powershell
# Run all test suites to establish baseline
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --testsuites

# Results saved to:
# results/testsuites/franky_tests_FrankyCPP-v1.1_20260201_143022.json
# results/testsuites/WAC_FrankyCPP-v1.1_20260201_143530.json
```

### 2. Compare with Previous Version

After making code changes:

```powershell
# Run new tests (after rebuilding)
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --testsuites

# Compare with baseline
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --cmp FrankyCPP-v1.2-dev --baseline FrankyCPP-v1.1

# Or view baseline report to see all engines
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --report
```

### 3. Run Engine Match

```powershell
# Requires cutechess-cli and older engine executable
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --matches

# Results include:
# - JSON summary with W/D/L and ELO
# - PGN file with all games
```

### 4. From CLion

**Setup Run Configuration:**
1. Edit Configurations → Add New → Application
2. **Name:** FrankyCPP Arena
3. **Target:** FrankyCPP_v1.1_Arena
4. **Program arguments:** `--testsuites` (or other mode)
5. **Working directory:** `$ProjectFileDir$`
6. Click OK

**Run:** Click the green play button or press Shift+F10

---

## Result Files

All results are saved to the `results/` directory:

```
results/
├── testsuites/          # EPD test suite results
│   ├── WAC_FrankyCPP-v1.1_20260201_143022.json
│   ├── WAC_FrankyGo-v1.0.3_20260201_144530.json
│   └── STS1-STS15_FrankyCPP-v0.5_20260201_145000.json
│
└── matches/             # Engine match results
    ├── FrankyCPP-v1.1_vs_FrankyGo-v1.0.3_60+0.6_20260201_150033.json
    ├── FrankyCPP-v1.1_vs_FrankyGo-v1.0.3_60+0.6_20260201_150033.pgn
    └── FrankyCPP-v1.1_vs_FrankyCPP-v0.5_60+0.6_20260201_160000.json
```

**Test Suite File Naming:** `{TestSuite}_{EngineName-Version}_{Timestamp}.json`

**Match File Naming:** `{Engine1-Version}_vs_{Engine2-Version}_{TimeControl}_{Timestamp}.json`

**Timestamp Format:** `YYYYMMDD_HHMMSS` for sortability

## Configuration

**Location:** `config/arena.yaml`

**Important:** All test suites now require an external UCI engine executable path. The Arena tests engines via UCI protocol for consistent, production-like testing.

**Key Configuration Fields:**

Edit `config/arena.yaml` to customize:

- **Test suites:** EPD file paths, time limits, depth limits, **engine paths** (required)
- **Matches:** Engine paths, opening books, time controls, rounds
- **Paths:** Results directory, cutechess-cli location
- **Engine options:** Command-line args, UCI options, position isolation

**Minimal Test Suite Example:**
```yaml
testSuites:
  - name: "franky_tests"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    isolatePositions: true
    commandLineArgs: ""
    uciOptions: "OwnBook=false"
```

See [Configuration.md](Configuration.md) for detailed reference.

---

## Troubleshooting

### "Configuration validation failed"

**Problem:** Arena can't find required files (EPD, engines, opening books)

**Solution:**
1. Ensure you're running from the **project root** directory
2. Check paths in `config/arena.yaml` are relative to project root
3. Verify files exist: `Test-Path "test/testsets/wac.epd"`

### "cutechess-cli not found"

**Problem:** Match runner can't find cutechess-cli executable

**Solution:**
1. Install cutechess-cli from https://github.com/cutechess/cutechess
2. Update `cutechessPath` in `config/arena.yaml`
3. Use absolute path: `D:/Games/CuteChess/cutechess-cli.exe`

### "Engine not found"

**Problem:** Match configuration references non-existent engine

**Solution:**
1. Build older engine version or copy from Release folder
2. Update `engine2Path` in match configuration
3. Ensure path is relative to project root

### No comparison results

**Problem:** `--cmp` finds no results for specified engine

**Solution:**
1. Run test suites/matches first to generate results
2. Check engine ID format: `EngineName-version` (e.g., `FrankyCPP-v1.1`)
3. Use `--engines` to list available engines
4. Look in `results/testsuites/` and `results/matches/` for existing files

### Match shows "RESUMING MATCH" or "ALREADY COMPLETE"

**Expected behavior:** Matches are automatically resumable via state files. Matches run in batches of 2 games (one color swap pair) and save state after each batch.

**Console output when resuming:**
```
*** RESUMING MATCH ***
  State file:        results/matches/.state/v1.1_vs_v1.0.state.json
  Completed rounds:  48
  Remaining rounds:  52
  Current score:     25 - 15 - 8 (W-L-D)
```

**Console output when complete:**
```
*** MATCH ALREADY COMPLETE ***
  All 100 games have been played.
  Delete state file to restart: results/matches/.state/v1.1_vs_v1.0.state.json
```

**To restart a match from scratch:**
1. Delete the state file shown in the console output
2. Optionally delete the output PGN file
3. Re-run the match

**Note:** Rounds must be an even number (matches run in pairs for color fairness).

---

## Command-Line Reference

```
FrankyCPP Arena - Engine Strength Testing

Options:
  -h, --help                      Show this help message
  -c, --config PATH               Configuration file path (default: config/arena.yaml)

Execution:
  -t, --testsuites                Run test suites only
  -m, --matches                   Run matches only

Reporting:
  -r, --report, --baselines       Show baseline report (all engines, all test suites)
  --engines                       List all available engines from results
  --cmp <engine>                  Compare engine against baselines (e.g., --cmp FrankyCPP-v1.2-dev)
  -b, --baseline <engine>         Specify baseline(s) for comparison (can repeat)
  --testsuites-only               Show only test suite results (filter out matches)
  --matches-only                  Show only match results (filter out test suites)

Examples:
  Run all tests:          FrankyCPP_v1.1_Arena.exe
  Test suites only:       FrankyCPP_v1.1_Arena.exe --testsuites
  Matches only:           FrankyCPP_v1.1_Arena.exe --matches

  Baseline report:        FrankyCPP_v1.1_Arena.exe --report
  Only test suites:       FrankyCPP_v1.1_Arena.exe --report --testsuites-only
  Only matches:           FrankyCPP_v1.1_Arena.exe --report --matches-only

  Compare vs baseline:    FrankyCPP_v1.1_Arena.exe --cmp FrankyCPP-v1.2-dev
  Specify baseline:       FrankyCPP_v1.1_Arena.exe --cmp FrankyCPP-v1.2-dev --baseline FrankyCPP-v1.1
  Compare matches only:   FrankyCPP_v1.1_Arena.exe --cmp FrankyCPP-v1.2-dev --matches-only

  List engines:           FrankyCPP_v1.1_Arena.exe --engines
  Custom config:          FrankyCPP_v1.1_Arena.exe --config my_arena.yaml
```

---

## Next Steps

- **[Reporting.md](Reporting.md)** - Complete guide to reporting and comparison features (NEW)
- **[External_Engine_Testing.md](External_Engine_Testing.md)** - Complete guide to external UCI engine testing
- **[Configuration.md](Configuration.md)** - Detailed configuration reference
- **[Results.md](Results.md)** - Result file format and analysis
- **[Development.md](Development.md)** - Extending the arena framework

---

## Tips

💡 **Run test suites after every significant change** to track tactical strength improvements

💡 **Use `--report` to see all engines side-by-side** for quick comparison

💡 **Use `--cmp` for detailed comparison** against specific baselines with delta analysis

💡 **Keep old results** for long-term historical comparison (results are small JSON files)

💡 **Use descriptive engine versions** like `FrankyCPP-v1.1` for clear identification in reports

💡 **Run matches overnight** - 100+ rounds can take hours depending on time control

💡 **Matches are resumable** - matches run in batches of 2 games and save state after each batch. If interrupted, just run again to continue. State is saved to `results/matches/.state/`. Delete the state file to restart a match from scratch.

💡 **Use filtering options** (`--testsuites-only`, `--matches-only`) to focus on specific result types

---

*Last updated: 2026-02-07*
