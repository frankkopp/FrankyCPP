# Engine Arena Result Files and Analysis

Complete guide to Arena result file formats and how to analyze them.

---

## Result Directory Structure

```
results/
├── testsuites/          # EPD tactical test results
│   ├── franky_tests_FrankyCPP-v1.1_20260201_143022.json
│   ├── WAC_FrankyCPP-v1.1_20260201_144530.json
│   └── WAC_FrankyCPP-v1.0_20260115_091245.json
│
├── matches/             # Engine-vs-engine match results
│   ├── FrankyCPP-v1.1_vs_FrankyGo-v1.0.3_60_0.6_20260201_150033.json
│   ├── FrankyCPP-v1.1_vs_FrankyGo-v1.0.3_60_0.6_20260201_150033.pgn
│   └── ...
│
└── comparisons/         # Version comparison reports
    ├── FrankyCPP-v1.1_vs_FrankyCPP-v1.0_20260201_153000.txt
    └── ...
```

---

## File Naming Convention

### Test Suite Results

**Format:** `{TestSuite}_{EngineName-Version}_{Timestamp}.json`

**Components:**
- `TestSuite` - Test suite name (e.g., "WAC", "STS1-STS15_LAN")
- `EngineName-Version` - Engine identifier (e.g., "FrankyCPP-v1.1")
- `Timestamp` - `YYYYMMDD_HHMMSS` in local time

**Examples:**
```
WAC_FrankyCPP-v1.1_20260201_143530.json
STS1-STS15_LAN_FrankyGo-v1.0.3_20260201_144530.json
mate_test_Stockfish-dev_20260201_145000.json
```

### Match Results

**Format:** `{Engine1-Version}_vs_{Engine2-Version}_{TimeControl}_{Timestamp}.json`

**Components:**
- `Engine1-Version` - First engine identifier (e.g., "FrankyCPP-v1.1")
- `Engine2-Version` - Second engine identifier (e.g., "FrankyGo-v1.0.3")
- `TimeControl` - Time control with sanitized characters (e.g., "60_0.6" from "60+0.6")
- `Timestamp` - `YYYYMMDD_HHMMSS` in local time

**Examples:**
```
FrankyCPP-v1.1_vs_FrankyGo-v1.0.3_60_0.6_20260201_150033.json  # Match result (JSON)
FrankyCPP-v1.1_vs_FrankyGo-v1.0.3_60_0.6_20260201_150033.pgn   # Match games (PGN)
FrankyCPP-v1.1_vs_FrankyCPP-v0.5_60_0.6_20260201_160000.json   # Self-play match
```

**Sorting:** Files sort chronologically by timestamp

---

## Test Suite Result Format

### JSON Structure

All string fields are JSON-escaped in the output.

```json
{
  "arenaVersion": "v1.1",
  "timestamp": "2026-02-01T14:30:22Z",

  "testSuite": {
    "name": "WAC",
    "epdPath": "test/testsets/wac.epd"
  },

  "engine": {
    "name": "FrankyCPP",
    "version": "v1.1",
    "path": "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
  },

  "summary": {
    "totalTests": 300,
    "passed": 285,
    "failed": 15,
    "skipped": 0,
    "successRate": 95.0,
    "totalNodes": 45000000,
    "totalTimeMs": 85000,
    "avgTimeMs": 283.3,
    "avgNodes": 150000
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
| `arenaVersion` | String | Arena version identifier (typically the engine version under test) |
| `timestamp` | String | ISO 8601 timestamp (UTC) |
| `testSuite` | Object | Test suite metadata |
| `engine` | Object | Engine identification |
| `summary` | Object | Aggregate statistics |
| `details` | Array | Per-test case results |

#### TestSuite Object

| Field | Type | Description |
|-------|------|-------------|
| `name` | String | Test suite name |
| `epdPath` | String | Path to EPD file |

#### Engine Object

| Field | Type | Description |
|-------|------|-------------|
| `name` | String | Engine name (e.g., "FrankyCPP") |
| `version` | String | Engine version (e.g., "v1.1") |
| `path` | String | Path to engine executable |

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
| `avgTimeMs` | Float | Average time per test in milliseconds |
| `avgNodes` | Float | Average nodes per test |

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

All string fields are JSON-escaped in the output.

```json
{
  "arenaVersion": "v1.1",
  "timestamp": "2026-02-06T10:00:00Z",

  "match": {
    "name": "v1.1_vs_FrankyGo_blitz_100",
    "timeControl": "60+0.6",
    "rounds": 100
  },

  "engine1": {
    "name": "FrankyCPP",
    "version": "v1.1",
    "path": "Release/FrankyCPP_V1.1/FrankyCPP_v1.1.exe"
  },

  "engine2": {
    "name": "FrankyGo",
    "version": "v1.0.3",
    "path": "D:/Games/FrankyChess/FrankyGo/FrankyGo.exe"
  },

  "results": {
    "engine1Wins": 32,
    "engine2Wins": 19,
    "draws": 49,
    "engine1Score": 56.5,
    "engine2Score": 43.5,
    "eloDifference": 45.2
  },

  "pgnPath": "results/matches/v1.1_vs_FrankyGo_blitz_100.pgn",
  "durationMs": 3600000
}
```

### Field Descriptions

#### Top-Level Fields

| Field | Type | Description |
|-------|------|-------------|
| `arenaVersion` | String | Arena version that ran this match |
| `timestamp` | String | ISO 8601 timestamp (UTC) when match started |
| `match` | Object | Match configuration details |
| `engine1` | Object | First engine identification |
| `engine2` | Object | Second engine identification |
| `results` | Object | Match outcome statistics |
| `pgnPath` | String | Path to PGN file with games |
| `durationMs` | Integer | Match duration in milliseconds |

#### Match Object

| Field | Type | Description |
|-------|------|-------------|
| `name` | String | Match identifier |
| `timeControl` | String | Time control (e.g., "60+0.6") |
| `rounds` | Integer | Number of games played |

#### Engine Objects (engine1, engine2)

| Field | Type | Description |
|-------|------|-------------|
| `name` | String | Engine name (e.g., "FrankyCPP") |
| `version` | String | Engine version (e.g., "v1.1") |
| `path` | String | Path to engine executable |

#### Results Object

| Field | Type | Description |
|-------|------|-------------|
| `engine1Wins` | Integer | Wins by engine 1 |
| `engine2Wins` | Integer | Wins by engine 2 |
| `draws` | Integer | Number of drawn games |
| `engine1Score` | Float | Total points (win=1, draw=0.5) |
| `engine2Score` | Float | Total points for engine 2 |
| `eloDifference` | Float | ELO rating difference (engine1 - engine2) |

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

## Reporting Formats

The Arena provides powerful reporting and comparison features. Reports are displayed directly in the terminal (not saved to files).

For complete reporting documentation, see **[Reporting.md](Reporting.md)**.

### Available Reports

**Baseline Reports** (`--report`):
- Shows all engines side-by-side for all test suites
- Shows all engine pairs for all matches
- Useful for quick overview of all results

**Comparison Reports** (`--cmp`):
- Compares target engine against baseline engine(s)
- Shows deltas with color-coded indicators (✅/❌/⚖️)
- Highlights improvements and regressions
- Works for both test suites and matches

**Filtering:**
- `--testsuites-only` - Show only test suite results
- `--matches-only` - Show only match results

### Quick Examples

```bash
# View all results
FrankyCPP_Arena --report

# Compare target vs baselines
FrankyCPP_Arena --cmp FrankyCPP-v1.2-dev --baseline FrankyCPP-v1.1

# Show only match results
FrankyCPP_Arena --report --matches-only

# List available engines
FrankyCPP_Arena --engines
```

See **[Reporting.md](Reporting.md)** for detailed report formats, examples, and workflows

---

## Analysis Workflows

### 1. Checking Tactical Improvement

**After implementing a new feature:**

```powershell
# Run test suites
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --testsuites

# Compare with baseline
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --cmp FrankyCPP-v1.1_new --baseline FrankyCPP-v1.1_baseline
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

with open('results/testsuites/WAC_FrankyCPP-v1.1_new_20260201_143022.json') as f:
    new = json.load(f)
with open('results/testsuites/WAC_FrankyCPP-v1.1_baseline_20260201_091245.json') as f:
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
    
    engine_id = f"{data['engine']['name']}-{data['engine']['version']}"
    results.append({
        'engine': engine_id,
        'suite': data['testSuite']['name'],
        'pass_rate': data['summary']['successRate'],
        'timestamp': data['timestamp']
    })

# Sort by timestamp
results.sort(key=lambda x: x['timestamp'])

# Print summary
for r in results:
    print(f"{r['timestamp']}: {r['engine']} - {r['suite']}: {r['pass_rate']:.1f}%")
```

### PowerShell Example: Compare Versions

```powershell
# Extract ELO differences from match results
Get-ChildItem "results/matches/*.json" | ForEach-Object {
    $data = Get-Content $_ | ConvertFrom-Json
    [PSCustomObject]@{
        Version = $data.arenaVersion
        Match = $data.match.name
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
with open('results/testsuites/WAC_FrankyCPP-v1.1_20260201_143530.json') as f:
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
with open('results/testsuites/WAC_FrankyCPP-v1.1_20260201_143530.json') as f:
    data = json.load(f)

for detail in data['details']:
    cursor.execute('''
    INSERT INTO test_results VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    ''', (
        data['engine']['version'],
        data['testSuite']['name'],
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

*Last updated: 2026-02-06*
