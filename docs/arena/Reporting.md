# Arena Reporting and Comparison Guide

Complete guide to Arena's reporting, comparison, and analysis features.

---

## Overview

The Arena provides powerful reporting features to help you track engine strength improvements, compare versions, and analyze results. All reports are displayed directly in the terminal with color-coded output for quick visual analysis.

**Report Types:**
- **Baseline Reports** - Show all engines side-by-side
- **Comparison Reports** - Compare target engine against baselines
- **Filtered Reports** - Focus on test suites OR matches only

---

## Quick Command Reference

```bash
# View all results (test suites + matches)
FrankyCPP_Arena --report

# View only test suite results
FrankyCPP_Arena --report --testsuites-only

# View only match results
FrankyCPP_Arena --report --matches-only

# Compare target vs baselines
FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev

# Compare with specific baseline
FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev --baseline FrankyCPP-v1.1

# Compare only matches
FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev --matches-only

# List available engines
FrankyCPP_Arena --engines
```

---

## Baseline Reports (`--report`)

### Purpose

Shows all engines side-by-side for easy comparison across test suites and matches. Useful for:
- Getting a quick overview of all results
- Comparing multiple engine versions at once
- Identifying strongest/weakest performers

### Test Suite Baseline Report

**Command:**
```bash
FrankyCPP_Arena --report
# or
FrankyCPP_Arena --report --testsuites-only
```

**Example Output:**
```
================================================================================
BASELINE RESULTS - All Engines
================================================================================
Generated: 2026-02-06T15:30:00Z
================================================================================

TEST SUITE: WAC (300 positions)
--------------------------------------------------------------------------------
Engine                          Solved      Rate        Avg Time    Avg Nodes
--------------------------------------------------------------------------------
FrankyCPP v1.2-dev              288/300     96.0%       280ms       145K
FrankyCPP v1.1                  285/300     95.0%       285ms       150K
FrankyCPP v0.5                  250/300     83.3%       320ms       180K
FrankyGo v1.0.3                 275/300     91.7%       295ms       160K

TEST SUITE: STS1-STS15_LAN (1500 positions)
--------------------------------------------------------------------------------
Engine                          Solved      Rate        Avg Time    Avg Nodes
--------------------------------------------------------------------------------
FrankyCPP v1.2-dev              952/1500    63.5%       1.2s        250K
FrankyCPP v1.1                  945/1500    63.0%       1.2s        255K
FrankyCPP v0.5                  820/1500    54.7%       1.3s        280K

================================================================================
```

**Columns:**
- **Engine** - Engine name and version
- **Solved** - Number of positions solved / total positions
- **Rate** - Solve rate percentage
- **Avg Time** - Average time per position
- **Avg Nodes** - Average nodes searched per position

### Match Baseline Report

**Command:**
```bash
FrankyCPP_Arena --report --matches-only
```

**Example Output:**
```
================================================================================
MATCH RESULTS - All Engine Pairs
================================================================================
--------------------------------------------------------------------------------
Engine Pair                              Games   Score      W/D/L       ELO Diff
--------------------------------------------------------------------------------
FrankyCPP v1.1 vs FrankyGo v1.0.3        100     56.5%      32/49/19    +45
FrankyCPP v1.1 vs FrankyCPP v0.5         100     65.0%      42/46/12    +110
FrankyGo v1.0.3 vs FrankyCPP v0.5        100     58.0%      35/46/19    +56
================================================================================
```

**Columns:**
- **Engine Pair** - Two engines that played against each other
- **Games** - Total games played
- **Score** - Win percentage for first engine
- **W/D/L** - Wins / Draws / Losses (first engine perspective)
- **ELO Diff** - ELO rating difference (positive = first engine stronger)

---

## Comparison Reports (`--cmp`)

### Purpose

Compares a target engine against one or more baseline engines with delta analysis. Useful for:
- Detecting strength improvements/regressions
- Validating code changes
- Tracking progress over time

### Test Suite Comparison Report

**Command:**
```bash
FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev --baseline FrankyCPP-v1.1
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
crafty_test          45/100         42/100           +3          [+] BETTER

SUMMARY
--------------------------------------------------------------------------------
Total improvement: +13 positions solved
Status: ✅ IMPROVEMENT over baseline
================================================================================
```

**Status Indicators:**
- `[+] BETTER` - Target solved more positions (shown in green)
- `[-] WORSE` - Target solved fewer positions (shown in red)
- `[=] EQUAL` - Same solve rate (shown in default color)
- `[!] NEW` - Baseline has no results for this suite

### Match Comparison Report

**Command:**
```bash
FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev --matches-only
```

**Example Output:**
```
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
vs v0.5:    +165 ELO  ✅ STRONGER
vs FrankyGo: +10 ELO  ⚖️  EQUAL
Status:      ✅ IMPROVEMENT over baseline
================================================================================
```

**Columns:**
- **Opponent** - Engine matched against
- **Games** - Total games played
- **Score** - Win percentage (target engine perspective)
- **W/D/L** - Wins/Draws/Losses (target engine perspective)
- **ELO** - ELO rating difference (target vs opponent)
- **vs v1.1** - ELO delta compared to baseline

**Delta Colors:**
- Green: Positive delta (stronger than baseline)
- Red: Negative delta (weaker than baseline)
- Default: Same as baseline

---

## Listing Engines (`--engines`)

**Command:**
```bash
FrankyCPP_Arena --engines
```

**Example Output:**
```
Available engines from results:
--------------------------------------------------------------------------------
  FrankyCPP-v0.5
  FrankyCPP-v1.0
  FrankyCPP-v1.1
  FrankyCPP-v1.2-dev
  FrankyGo-v1.0.3
  Stockfish-dev
--------------------------------------------------------------------------------
```

**Usage:**
- See which engines have stored results
- Get exact engine IDs for use in `--cmp` command
- Verify engine naming consistency

---

## Filtering Options

### Test Suites Only

**Command:**
```bash
FrankyCPP_Arena --report --testsuites-only
FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev --testsuites-only
```

**Purpose:**
- Focus on tactical/positional strength
- Faster than including matches
- Good for quick regression checks

### Matches Only

**Command:**
```bash
FrankyCPP_Arena --report --matches-only
FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev --matches-only
```

**Purpose:**
- Focus on head-to-head playing strength
- See ELO differences clearly
- Analyze match results without test suite noise

---

## Common Workflows

### 1. Quick Regression Check

After making code changes:

```bash
# Run test suites (fast)
FrankyCPP_Arena --testsuites

# Compare with baseline
FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev --baseline FrankyCPP-v1.1 --testsuites-only
```

Look for:
- ✅ Green `[+] BETTER` indicators (improvements)
- ⚠️ Yellow `[!] NEW` indicators (baseline missing data)
- ❌ Red `[-] WORSE` indicators (regressions - investigate!)

### 2. Comprehensive Strength Comparison

For major releases:

```bash
# Run everything
FrankyCPP_Arena

# View all results
FrankyCPP_Arena --report

# Compare against multiple baselines
FrankyCPP_Arena --cmp FrankyCPP-v1.2 --baseline FrankyCPP-v1.1 --baseline FrankyCPP-v1.0
```

### 3. Match-Focused Analysis

When you care primarily about playing strength:

```bash
# Run matches
FrankyCPP_Arena --matches

# View match results only
FrankyCPP_Arena --report --matches-only

# Compare matches against baseline
FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev --matches-only
```

### 4. Historical Progress Tracking

Track improvement over time:

```bash
# Compare each version against previous
FrankyCPP_Arena --cmp FrankyCPP-v1.2 --baseline FrankyCPP-v1.1
FrankyCPP_Arena --cmp FrankyCPP-v1.1 --baseline FrankyCPP-v1.0
FrankyCPP_Arena --cmp FrankyCPP-v1.0 --baseline FrankyCPP-v0.5

# Or view all versions side-by-side
FrankyCPP_Arena --report
```

### 5. External Engine Benchmarking

Compare against other engines:

```bash
# After running matches against external engines
FrankyCPP_Arena --cmp FrankyCPP-v1.2 --matches-only

# Will show results vs Stockfish, FrankyGo, etc.
```

---

## Report Interpretation

### Test Suite Pass Rates

**Excellent:** 95%+
- Very strong tactical/positional play
- Comparable to top engines

**Good:** 85-95%
- Solid tactical understanding
- Room for improvement in complex positions

**Fair:** 70-85%
- Basic tactical ability
- Significant gaps in advanced positions

**Poor:** <70%
- Weak tactical recognition
- Major improvement needed

### ELO Differences

**Significant Improvement:** +50 ELO or more
- Clear strength gain
- Changes are working

**Moderate Improvement:** +20 to +50 ELO
- Noticeable but not dramatic
- Continue in this direction

**Marginal:** -20 to +20 ELO
- Approximately equal strength
- May need more games for clarity

**Regression:** -50 ELO or worse
- Strength decreased
- Investigate changes immediately

### Status Symbols

- ✅ **BETTER** / **STRONGER** - Improvement detected
- ❌ **WORSE** / **WEAKER** - Regression detected
- ⚖️ **EQUAL** - No significant change
- ⚠️ **NEW** / **N/A** - Missing baseline data

---

## Tips and Best Practices

💡 **Use `--report` first** to get an overview before diving into detailed comparisons

💡 **Filter by type** (`--testsuites-only`, `--matches-only`) to focus your analysis

💡 **Run test suites after every significant change** - they're fast and catch regressions

💡 **Use specific baseline** with `--baseline` for targeted comparisons

💡 **List engines with `--engines`** to see exact naming for comparison commands

💡 **Track historical progress** by comparing each version against the previous one

💡 **Combine test suites and matches** for comprehensive strength assessment

💡 **Watch for regressions** (red indicators) - investigate immediately

💡 **Run matches overnight** - 100+ games can take hours

💡 **Save reports** by redirecting output: `FrankyCPP_Arena --report > results.txt`

---

## Troubleshooting

### "No results found"

**Problem:** Report shows no data

**Solutions:**
1. Run test suites first: `FrankyCPP_Arena --testsuites`
2. Run matches first: `FrankyCPP_Arena --matches`
3. Check `results/testsuites/` and `results/matches/` directories

### "Engine not found"

**Problem:** `--cmp` can't find specified engine

**Solutions:**
1. List available engines: `FrankyCPP_Arena --engines`
2. Check engine ID format: `EngineName-version` (e.g., `FrankyCPP-v1.1`)
3. Ensure results exist for that engine

### Missing baseline data

**Problem:** Comparison shows `[!] NEW` or `N/A`

**Cause:** Baseline engine hasn't been tested on that suite/match

**Solution:** Run tests for baseline engine to generate comparison data

### Reports look wrong

**Problem:** Formatting issues or missing colors

**Possible Causes:**
1. Terminal doesn't support ANSI colors
2. Output redirected to file (colors disabled)
3. Console width too narrow

**Solutions:**
1. Use modern terminal (Windows Terminal, PowerShell 7+, iTerm2, etc.)
2. View reports directly in terminal, not redirected
3. Maximize console window for best formatting

---

## Advanced Usage

### Multiple Baselines

Compare against several previous versions:

```bash
FrankyCPP_Arena --cmp FrankyCPP-v1.2 \
  --baseline FrankyCPP-v1.1 \
  --baseline FrankyCPP-v1.0 \
  --baseline FrankyCPP-v0.5
```

Shows deltas against each baseline separately.

### Saving Reports

Redirect output to save reports as text files:

```bash
# Save baseline report
FrankyCPP_Arena --report > baseline_report_$(date +%Y%m%d).txt

# Save comparison report
FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev > comparison_$(date +%Y%m%d).txt
```

**Note:** ANSI color codes will be included in saved files.

### Scripting

Use in CI/CD pipelines:

```bash
#!/bin/bash
# Run tests
FrankyCPP_Arena --testsuites

# Compare and check for regressions
FrankyCPP_Arena --cmp FrankyCPP-dev --baseline FrankyCPP-latest > report.txt

# Check for "WORSE" indicators
if grep -q "WORSE" report.txt; then
  echo "REGRESSION DETECTED!"
  exit 1
fi
```

---

## Related Documentation

- **[README.md](README.md)** - Quick start guide
- **[Results.md](Results.md)** - Result file formats
- **[Configuration.md](Configuration.md)** - Configuration reference
- **[External_Engine_Testing.md](External_Engine_Testing.md)** - UCI engine testing guide

---

*Last updated: 2026-02-06*
