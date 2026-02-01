# Engine Arena Result Files and Analysis

Complete guide to Arena result file formats and how to analyze them.

---

## Result Directory Structure

```
results/
├── testsuites/          # EPD tactical test results
│   ├── v1.1_franky_tests_20260201_143022.json
│   ├── v1.1_wac_20260201_144530.json
│   └── v1.0_wac_20260115_091245.json
│
├── matches/             # Engine-vs-engine match results
│   ├── v1.1_vs_v1.0_blitz_20260201_150033.json
│   ├── v1.1_vs_v1.0_blitz_20260201_150033.pgn
│   └── ...
│
└── comparisons/         # Version comparison reports
    ├── v1.1_vs_v1.0_20260201_153000.txt
    └── ...
```

---

## File Naming Convention

**Format:** `{version}_{name}_{timestamp}.{ext}`

**Components:**
- `version` - Engine version identifier (from config)
- `name` - Test suite or match name
- `timestamp` - `YYYYMMDD_HHMMSS` in local time
- `ext` - File extension (json, pgn, txt)

**Examples:**
```
v1.1_WAC_20260201_143530.json              # Test suite result
v1.1_vs_v1.0_blitz_20260201_150033.json    # Match result (JSON)
v1.1_vs_v1.0_blitz_20260201_150033.pgn     # Match games (PGN)
v1.1_vs_v1.0_20260201_153000.txt           # Comparison report
```

**Sorting:** Files sort chronologically by timestamp

---

## Test Suite Result Format

### JSON Structure

```json
{
  "version": "v1.1",
  "suiteName": "WAC",
  "timestamp": "2026-02-01T14:30:22Z",
  "summary": {
    "totalTests": 300,
    "passed": 285,
    "failed": 15,
    "skipped": 0,
    "successRate": 95.0,
    "totalNodes": 45000000,
    "totalTimeMs": 85000
  },
  "details": [
    {
      "testId": "WAC.001",
      "fen": "2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - -",
      "expected": "Qg6",
      "actual": "Qg6",
      "passed": true,
      "nodes": 150000,
      "timeMs": 285
    },
    {
      "testId": "WAC.002",
      "fen": "8/7p/5k2/5p2/p1p2P2/Pr1pPK2/1P1R3P/8 b - -",
      "expected": "Rxb2",
      "actual": "Rb1",
      "passed": false,
      "nodes": 180000,
      "timeMs": 312
    }
    // ... more test details
  ]
}
```

### Field Descriptions

#### Top-Level Fields

| Field | Type | Description |
|-------|------|-------------|
| `version` | String | Engine version identifier |
| `suiteName` | String | Test suite name |
| `timestamp` | String | ISO 8601 timestamp (UTC) |
| `summary` | Object | Aggregate statistics |
| `details` | Array | Per-test case results |

#### Summary Object

| Field | Type | Description |
|-------|------|-------------|
| `totalTests` | Integer | Total number of test positions |
| `passed` | Integer | Number of tests passed |
| `failed` | Integer | Number of tests failed |
| `skipped` | Integer | Number of tests skipped (usually 0) |
| `successRate` | Float | Pass percentage (0-100) |
| `totalNodes` | Integer | Sum of nodes searched across all tests |
| `totalTimeMs` | Integer | Sum of time spent in milliseconds |

**Derived Metrics:**
- **Average nodes per test:** `totalNodes / totalTests`
- **Average time per test:** `totalTimeMs / totalTests`
- **Nodes per second:** `(totalNodes / totalTimeMs) * 1000`

#### Test Detail Object

| Field | Type | Description |
|-------|------|-------------|
| `testId` | String | Unique test identifier (e.g., "WAC.001") |
| `fen` | String | Position FEN string |
| `expected` | String | Expected move(s) in SAN or coordinate notation |
| `actual` | String | Move chosen by engine |
| `passed` | Boolean | Whether test passed |
| `nodes` | Integer | Nodes searched for this position |
| `timeMs` | Integer | Time spent in milliseconds |

---

## Match Result Format

### JSON Structure

```json
{
  "version": "v1.1",
  "matchName": "v1.1_vs_v1.0_blitz",
  "timestamp": "2026-02-01T15:00:33Z",
  "engines": {
    "engine1": "FrankyCPP_v1.1",
    "engine2": "FrankyCPP_v1.0"
  },
  "results": {
    "engine1Wins": 65,
    "engine2Wins": 15,
    "draws": 20,
    "engine1Score": 75.0,
    "engine2Score": 25.0,
    "eloDifference": 174.0
  },
  "pgnPath": "results/matches/v1.1_vs_v1.0_blitz.pgn",
  "durationMs": 7234567
}
```

### Field Descriptions

| Field | Type | Description |
|-------|------|-------------|
| `version` | String | Version being tested (engine1) |
| `matchName` | String | Match identifier |
| `timestamp` | String | ISO 8601 timestamp (UTC) |
| `engines.engine1` | String | First engine name |
| `engines.engine2` | String | Second engine name |
| `results.engine1Wins` | Integer | Wins by engine 1 |
| `results.engine2Wins` | Integer | Wins by engine 2 |
| `results.draws` | Integer | Number of drawn games |
| `results.engine1Score` | Float | Total points (win=1, draw=0.5) |
| `results.engine2Score` | Float | Total points for engine 2 |
| `results.eloDifference` | Float | ELO rating difference |
| `pgnPath` | String | Path to PGN file with games |
| `durationMs` | Integer | Match duration in milliseconds |

**Game Count:** `engine1Wins + engine2Wins + draws` = total games played

**ELO Calculation:** Uses standard formula:
```
score = engine1Score / totalGames
eloDiff = -400 * log10(1/score - 1)
```

**Interpretation:**
- `eloDifference > 0` → engine1 is stronger
- `eloDifference < 0` → engine2 is stronger
- `|eloDifference| < 10` → approximately equal strength

---

## PGN Files

Match PGN files contain all games in standard PGN format.

### Example Game

```pgn
[Event "?"]
[Site "?"]
[Date "2026.02.01"]
[Round "1"]
[White "FrankyCPP_v1.1"]
[Black "FrankyCPP_v1.0"]
[Result "1-0"]
[TimeControl "60+0.6"]
[Opening "Sicilian Defense"]

1. e4 c5 2. Nf3 d6 3. d4 cxd4 4. Nxd4 Nf6 5. Nc3 a6
... (moves continue)
1-0
```

### PGN Headers

| Header | Description |
|--------|-------------|
| `Event` | Match name |
| `Site` | Location (usually "?") |
| `Date` | Game date |
| `Round` | Game number in match |
| `White` | White engine name |
| `Black` | Black engine name |
| `Result` | Game result (1-0, 0-1, 1/2-1/2) |
| `TimeControl` | Time control used |
| `Opening` | Opening name (if available) |

### Analyzing PGN Files

**Chess GUIs:**
- ChessBase
- Arena Chess GUI
- Scid vs. PC
- Lichess (upload for analysis)

**Command-line tools:**
- pgn-extract
- python-chess library

---

## Comparison Report Format

Comparison reports are plain text files generated by `--compare` command.

### Example Report

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
  Avg time: 283.3ms → 280.5ms (-2.8ms)

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
  Duration: 7234.6 seconds


SUMMARY:
-------------------------------------------------------------------
v1.1 is approximately +174 ELO stronger than v1.0
Test suite improvement: +37 positions solved
===================================================================
```

### Report Sections

#### Header
- Version 1 and Version 2 identifiers
- Report generation timestamp

#### Test Suite Comparison
For each test suite found in both versions:
- Pass counts and percentages
- Improvement/regression delta
- Timing comparison (if significant difference)

**Indicators:**
- `Improvement: +N positions` - v1 solved more
- `Regression: -N positions` - v1 solved fewer
- `No change` - identical pass rates

#### Match Comparison
For each match:
- Win/draw/loss breakdown for both engines
- Total scores
- ELO difference
- Match duration

#### Summary
- Overall ELO estimate (average from matches)
- Total tactical improvement across all test suites
- General strength assessment

---

## Analysis Workflows

### 1. Checking Tactical Improvement

**After implementing a new feature:**

```powershell
# Run test suites
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --testsuites

# Compare with baseline
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --compare v1.1_new v1.1_baseline
```

**Look for:**
- ✓ Positive delta in tactical test suites (WAC, STS)
- ✓ No significant timing regression
- ✗ Failed tests that previously passed (regressions)

### 2. Analyzing Match Results

**After running a match:**

1. **Check ELO difference** in JSON file
   - `eloDifference > 20` → significant improvement
   - `eloDifference < -20` → regression
   - `-20 < eloDifference < 20` → approximately equal

2. **Check game distribution:**
   - High win rate + low draws → engine is clearly stronger
   - Many draws → engines are close in strength
   - Lopsided results with few games → may need more games

3. **Analyze critical games:**
   - Open PGN file in chess GUI
   - Look at lost games for bugs
   - Look at drawn games for missed wins

### 3. Finding Regressions

**Compare failed test details:**

```python
# Python script example
import json

with open('results/testsuites/v1.1_new_wac.json') as f:
    new = json.load(f)
with open('results/testsuites/v1.1_old_wac.json') as f:
    old = json.load(f)

# Find new failures
new_failed = {d['testId'] for d in new['details'] if not d['passed']}
old_failed = {d['testId'] for d in old['details'] if not d['passed']}

regressions = new_failed - old_failed
print(f"Regressions: {regressions}")
```

### 4. Tracking Historical Progress

**Create a spreadsheet:**

| Version | WAC Pass Rate | STS Pass Rate | vs v1.0 ELO | Date |
|---------|---------------|---------------|-------------|------|
| v1.0 | 83.3% | 60.0% | 0 | 2026-01-15 |
| v1.1 | 95.0% | 75.0% | +174 | 2026-02-01 |
| v1.2 | 96.7% | 78.0% | +210 | 2026-02-15 |

**Graph trends:**
- Pass rates over time
- ELO progression
- Node efficiency (nodes per test)

---

## Automated Analysis Scripts

### Python Example: Extract Pass Rates

```python
import json
import glob

results = []
for filepath in glob.glob('results/testsuites/*.json'):
    with open(filepath) as f:
        data = json.load(f)
    
    results.append({
        'version': data['version'],
        'suite': data['suiteName'],
        'pass_rate': data['summary']['successRate'],
        'timestamp': data['timestamp']
    })

# Sort by timestamp
results.sort(key=lambda x: x['timestamp'])

# Print summary
for r in results:
    print(f"{r['timestamp']}: {r['version']} - {r['suite']}: {r['pass_rate']:.1f}%")
```

### PowerShell Example: Compare Versions

```powershell
# Extract ELO differences from match results
Get-ChildItem "results/matches/*.json" | ForEach-Object {
    $data = Get-Content $_ | ConvertFrom-Json
    [PSCustomObject]@{
        Version = $data.version
        Match = $data.matchName
        ELO = $data.results.eloDifference
        Date = $data.timestamp
    }
} | Format-Table -AutoSize
```

---

## Tips for Result Management

💡 **Keep all result files** - They're small JSON files useful for historical tracking

💡 **Use version control** for comparison reports (track strength progression over time)

💡 **Back up PGN files** - They contain valuable game data for analysis

💡 **Automate collection** - Script extraction of key metrics for trending

💡 **Clean old results** periodically, but keep:
- Latest result for each version
- Results from official releases
- Baseline results for comparison

💡 **Name versions descriptively** when testing specific changes:
- `v1.1_before_ttfix` / `v1.1_after_ttfix`
- `v1.1_baseline` / `v1.1_experimental`

---

## Exporting Results

### To CSV

```python
import json
import csv

# Export test suite results to CSV
with open('results/testsuites/v1.1_wac.json') as f:
    data = json.load(f)

with open('wac_results.csv', 'w', newline='') as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(['TestID', 'Expected', 'Actual', 'Passed', 'Nodes', 'TimeMs'])
    
    for detail in data['details']:
        writer.writerow([
            detail['testId'],
            detail['expected'],
            detail['actual'],
            detail['passed'],
            detail['nodes'],
            detail['timeMs']
        ])
```

### To Database

```python
import json
import sqlite3

conn = sqlite3.connect('arena_results.db')
cursor = conn.cursor()

# Create table
cursor.execute('''
CREATE TABLE IF NOT EXISTS test_results (
    version TEXT,
    suite TEXT,
    test_id TEXT,
    expected TEXT,
    actual TEXT,
    passed BOOLEAN,
    nodes INTEGER,
    time_ms INTEGER,
    timestamp TEXT
)
''')

# Import JSON
with open('results/testsuites/v1.1_wac.json') as f:
    data = json.load(f)

for detail in data['details']:
    cursor.execute('''
    INSERT INTO test_results VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (
        data['version'],
        data['suiteName'],
        detail['testId'],
        detail['expected'],
        detail['actual'],
        detail['passed'],
        detail['nodes'],
        detail['timeMs'],
        data['timestamp']
    ))

conn.commit()
conn.close()
```

---

## Troubleshooting

### "No results found for version X"

**Problem:** Comparison can't find result files for specified version

**Solutions:**
1. Check exact version string (case-sensitive)
2. Run test suites/matches first to generate results
3. Look in `results/testsuites/` and `results/matches/` directories
4. Ensure version string in `arena.yaml` matches exactly

### Invalid JSON

**Problem:** Result file is corrupted or incomplete

**Causes:**
- Arena was interrupted during write
- Disk full
- Permissions issue

**Solutions:**
1. Delete corrupted file
2. Re-run test suite or match
3. Check disk space and permissions

### Missing test details

**Problem:** JSON file has empty `details` array

**Cause:** Test suite didn't complete or failed to load EPD

**Solutions:**
1. Check EPD file path in configuration
2. Verify EPD file format is valid
3. Look for error messages in console output

---

*Last updated: 2026-02-01*
