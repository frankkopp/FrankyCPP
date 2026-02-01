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

### Compare Two Versions

```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --compare v1.1 v1.0
```

**Example Output:**
```
===================================================================
Engine Version Comparison Report
===================================================================
Version 1: v1.1
Version 2: v1.0
Generated: 20260201_153000
===================================================================

TEST SUITE COMPARISON:
-------------------------------------------------------------------

WAC:
  v1.0: 250/300 (83.3%)
  v1.1: 285/300 (95.0%)
  Improvement: +35 positions (+11.7%)

franky_tests:
  v1.0: 48/50 (96.0%)
  v1.1: 50/50 (100.0%)
  Improvement: +2 positions (+4.0%)


MATCH COMPARISON:
-------------------------------------------------------------------

v1.1_vs_v1.0_blitz:
  FrankyCPP_v1.1: 65 wins, 20 draws, 15 losses
  FrankyCPP_v1.0: 15 wins, 20 draws, 65 losses
  Score: 75.0 - 25.0
  ELO Difference: +174.0 ELO
  Duration: 1234.5 seconds


SUMMARY:
-------------------------------------------------------------------
v1.1 is approximately +174 ELO stronger than v1.0
Test suite improvement: +37 positions solved
===================================================================
```

---

## Common Workflows

### 1. Test Current Engine

```powershell
# Run all test suites to establish baseline
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --testsuites

# Results saved to:
# results/testsuites/v1.1_franky_tests_20260201_143022.json
# results/testsuites/v1.1_wac_20260201_143530.json
```

### 2. Compare with Previous Version

After making code changes:

```powershell
# Run new tests (after rebuilding)
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --testsuites

# Compare with baseline
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --compare v1.1 v1.1_old
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
│   ├── v1.1_franky_tests_20260201_143022.json
│   └── v1.1_wac_20260201_144530.json
│
├── matches/             # Engine match results
│   ├── v1.1_vs_v1.0_blitz_20260201_150033.json
│   └── v1.1_vs_v1.0_blitz_20260201_150033.pgn
│
└── comparisons/         # Version comparison reports
    └── v1.1_vs_v1.0_20260201_153000.txt
```

**File Naming:** `{version}_{name}_{timestamp}.{ext}`

**Timestamp Format:** `YYYYMMDD_HHMMSS` for sortability

---

## Configuration

Edit `config/arena.yaml` to customize:

- **Test suites:** EPD file paths, time limits, depth limits
- **Matches:** Engine paths, opening books, time controls, rounds
- **Paths:** Results directory, cutechess-cli location

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

**Problem:** `--compare` finds no results for specified version

**Solution:**
1. Run test suites/matches first to generate results
2. Check version string matches exactly (case-sensitive)
3. Look in `results/testsuites/` and `results/matches/` for existing files

---

## Command-Line Reference

```
FrankyCPP Arena - Engine Strength Testing

Options:
  -h, --help              Show this help message
  -c, --config PATH       Configuration file path (default: config/arena.yaml)
  -t, --testsuites        Run test suites only
  -m, --matches           Run matches only
  --compare V1 V2         Compare two versions: --compare v1.1 v1.0

Examples:
  Run all tests:       FrankyCPP_v1.1_Arena.exe
  Test suites only:    FrankyCPP_v1.1_Arena.exe --testsuites
  Matches only:        FrankyCPP_v1.1_Arena.exe --matches
  Compare versions:    FrankyCPP_v1.1_Arena.exe --compare v1.1 v1.0
  Custom config:       FrankyCPP_v1.1_Arena.exe --config my_arena.yaml
```

---

## Next Steps

- **[Configuration.md](Configuration.md)** - Detailed configuration reference
- **[Results.md](Results.md)** - Result file format and analysis
- **[Development.md](Development.md)** - Extending the arena framework

---

## Tips

💡 **Run test suites after every significant change** to track tactical strength improvements

💡 **Keep old results** for long-term historical comparison (results are small JSON files)

💡 **Use descriptive version names** like `v1.1_before_ttfix` and `v1.1_after_ttfix` for easier comparison

💡 **Run matches overnight** - 100+ rounds can take hours depending on time control

💡 **Compare with multiple versions** to see progression: `v1.1` → `v1.0` → `v0.5`

---

*Last updated: 2026-02-01*
