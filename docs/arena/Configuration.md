# Engine Arena Configuration Reference

Complete reference for `config/arena.yaml` configuration file.

---

## File Location

**Path:** `config/arena.yaml` (relative to project root)

**Format:** YAML (Yet Another Markup Language)

**Encoding:** UTF-8

---

## Configuration Structure

```yaml
version: "v1.1"                    # Engine version identifier
resultsDir: "./results"            # Results output directory
cutechessPath: "..."               # Path to cutechess-cli executable
debugMode: false                   # Enable UCI debug output (optional)

testSuites: [...]                  # List of EPD test suites
matches: [...]                     # List of engine matches
```

---

## Global Settings

### `version` (Required)

**Type:** String

**Purpose:** Identifier for the engine version being tested

**Usage:** This version tag appears in all result files and is used for comparison matching

**Examples:**
```yaml
version: "v1.1"           # Standard version
version: "v1.1-dev"       # Development build
version: "v1.1_ttfix"     # Version with specific fix
```

**Best Practices:**
- Use consistent naming scheme across versions
- Include semantic version numbers
- Add descriptive suffixes for experimental builds
- Avoid spaces and special characters

---

### `resultsDir` (Required)

**Type:** String (path)

**Purpose:** Directory where all results are saved

**Default:** `"./results"`

**Structure Created:**
```
results/
├── testsuites/    # JSON files for EPD test results
├── matches/       # JSON + PGN files for match results
└── comparisons/   # Text reports for version comparisons
```

**Path Format:**
- Relative to project root: `"./results"`
- Absolute path: `"D:/FrankyCPP/arena_results"`
- Forward slashes work on Windows: `"C:/Users/Frank/results"`

**Permissions:** Directory must be writable; will be created if it doesn't exist

---

### `cutechessPath` (Required for Matches)

**Type:** String (path)

**Purpose:** Location of cutechess-cli executable

**Required For:** Match execution only (not needed for test suites)

**Examples:**
```yaml
# Windows
cutechessPath: "D:/Games/CuteChess/cutechess-cli.exe"
cutechessPath: "C:/Program Files/CuteChess/cutechess-cli.exe"

# Linux
cutechessPath: "/usr/local/bin/cutechess-cli"
cutechessPath: "/opt/cutechess/cutechess-cli"
```

**Installation:**
- Download from: https://github.com/cutechess/cutechess/releases
- Ensure executable has proper permissions (Linux: `chmod +x`)

**Validation:** Arena checks if file exists before running matches

---

### `debugMode` (Optional)

**Type:** Boolean

**Purpose:** Enable detailed UCI communication logging in matches

**Default:** `false`

**Effect:** Passes `-debug all` to cutechess-cli, printing all engine I/O

**Examples:**
```yaml
debugMode: false    # Normal operation (quiet)
debugMode: true     # Verbose UCI logging
```

**Use Cases:**
- Debugging engine UCI implementation issues
- Investigating engine crashes or timeouts
- Understanding search behavior in specific positions

**Warning:** Produces large amounts of console output

---

## Test Suite Configuration

### TestSuiteConfig Structure

```yaml
testSuites:
  - name: "WAC"                              # Suite identifier
    epdPath: "test/testsets/wac.epd"        # Path to EPD file
    timePerMove: 5000                        # Milliseconds per position
    maxDepth: 30                             # Maximum search depth
```

### Fields

#### `name` (Required)

**Type:** String

**Purpose:** Unique identifier for the test suite

**Usage:** Appears in result files and comparison reports

**Examples:**
```yaml
name: "WAC"              # Win At Chess tactical suite
name: "STS"              # Strategic Test Suite
name: "mate_test"        # Mate-in-N problems
name: "franky_tests"     # Custom test suite
```

**Best Practices:**
- Use short, descriptive names
- Avoid spaces (use underscores instead)
- Match the EPD file name for clarity

---

#### `epdPath` (Required)

**Type:** String (path)

**Purpose:** Location of EPD (Extended Position Description) test file

**Format:** Standard EPD format with test operations (bm, am, dm)

**Path:** Relative to project root

**Examples:**
```yaml
epdPath: "test/testsets/wac.epd"              # Relative path
epdPath: "test/testsets/STS1-STS15_LAN.EPD"   # Multi-file suite
```

**EPD Format Example:**
```
r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - bm Bxc6; id "WAC.001";
```

**Supported Operations:**
- `bm` - Best Move (move must match one of the expected moves)
- `am` - Avoid Move (move must NOT match any listed move)
- `dm` - Direct Mate (must find mate in N moves)

---

#### `timePerMove` (Required)

**Type:** Integer (milliseconds)

**Purpose:** Time limit per test position

**Range:** 1-3600000 (1ms to 1 hour)

**Examples:**
```yaml
timePerMove: 1000     # 1 second (fast, for quick checks)
timePerMove: 5000     # 5 seconds (standard tactical tests)
timePerMove: 15000    # 15 seconds (difficult mate problems)
timePerMove: 60000    # 1 minute (deep strategic positions)
```

**Recommendations:**
- WAC, Bratko-Kopec: 5000-10000ms
- STS (Strategic): 10000-15000ms
- Mate problems: 15000-30000ms
- Quick regression: 1000-2000ms

**Trade-offs:**
- **Shorter:** Faster execution, may miss difficult tactics
- **Longer:** Better solve rate, takes more time

---

#### `maxDepth` (Required)

**Type:** Integer (ply)

**Purpose:** Maximum search depth limit

**Range:** 1-100 (practical range: 10-50)

**Examples:**
```yaml
maxDepth: 20     # Shallow search
maxDepth: 30     # Standard depth
maxDepth: 50     # Deep search
maxDepth: 100    # Effectively unlimited (time controls first)
```

**Interaction with Time:**
- Engine stops when **either** time OR depth limit is reached
- Usually time limit triggers first
- Depth limit prevents runaway searches in forced positions

**Recommendations:**
- Set higher than typical time-based search depth
- Use 30-50 for most test suites
- Use 100 to effectively disable depth limit

---

### Complete Test Suite Example

```yaml
testSuites:
  # Quick regression test
  - name: "franky_tests"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 2000
    maxDepth: 30

  # Standard tactical test
  - name: "WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30

  # Strategic test suite
  - name: "STS"
    epdPath: "test/testsets/STS1-STS15_LAN.EPD"
    timePerMove: 10000
    maxDepth: 40

  # Mate problems
  - name: "mate_test"
    epdPath: "test/testsets/mate_test_suite.epd"
    timePerMove: 15000
    maxDepth: 50
```

---

## Match Configuration

### MatchConfig Structure

```yaml
matches:
  - name: "v1.1_vs_v1.0_blitz"                          # Match identifier
    engine1Path: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"  # First engine
    engine2Path: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"       # Second engine
    openingBook: "books/8moves_GM_LB.pgn"               # Opening book PGN
    timeControl: "60+0.6"                               # Time control
    rounds: 100                                         # Number of games
    concurrency: 4                                      # Parallel games
    outputPgn: "results/matches/v1.1_vs_v1.0_blitz.pgn" # PGN output
```

### Fields

#### `name` (Required)

**Type:** String

**Purpose:** Unique identifier for the match

**Usage:** Appears in result files and comparison reports

**Examples:**
```yaml
name: "v1.1_vs_v1.0_blitz"      # Version + time control
name: "v1.1_vs_v1.0_rapid"      # Different time control
name: "v1.1_vs_stockfish_test"  # Testing against external engine
```

---

#### `engine1Path` (Required)

**Type:** String (path)

**Purpose:** Path to first engine executable (tested version)

**Path:** Relative to project root or absolute

**Examples:**
```yaml
# Relative paths
engine1Path: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
engine1Path: "build/FrankyCPP_v1.1"  # Linux

# Absolute paths
engine1Path: "D:/FrankyCPP/Release/FrankyCPP_v1.1.exe"
engine1Path: "/opt/engines/frankycpp/v1.1/FrankyCPP"
```

**Requirements:**
- Must be UCI-compatible chess engine
- Must have execute permissions (Linux)
- Path must exist (validated before match)

---

#### `engine2Path` (Required)

**Type:** String (path)

**Purpose:** Path to second engine executable (baseline version)

**Same requirements as `engine1Path`**

**Typical Usage:**
```yaml
# Compare current build with released version
engine2Path: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"

# Compare with older development build
engine2Path: "Release/FrankyCPP_v0.5/FrankyCPP_v0.5.exe"

# Test against external engine
engine2Path: "D:/Engines/Stockfish/stockfish-windows.exe"
```

---

#### `openingBook` (Required)

**Type:** String (path)

**Purpose:** PGN file containing opening positions

**Format:** Must be PGN format (not Polyglot .bin)

**Path:** Relative to project root

**Examples:**
```yaml
openingBook: "books/8moves_GM_LB.pgn"     # 8-move GM openings
openingBook: "books/superbook.pgn"        # Large opening book
openingBook: "books/test_openings.pgn"    # Custom test positions
```

**Purpose:**
- Ensures games start from diverse positions
- Prevents repetitive opening phase
- Tests middle-game and endgame strength

**Book Selection:**
- `8moves_GM_LB.pgn` - Good default (GM games, 8 moves deep)
- Larger books = more variety, but slower startup
- Custom books for specific opening testing

---

#### `timeControl` (Required)

**Type:** String

**Format:** `"base+increment"` where:
- `base` = initial time in seconds
- `increment` = time added per move in seconds

**Examples:**
```yaml
timeControl: "10+0.1"     # Blitz: 10s + 0.1s increment
timeControl: "60+0.6"     # Rapid: 1min + 0.6s increment
timeControl: "180+2"      # Classical: 3min + 2s increment
timeControl: "5+0.05"     # Bullet: 5s + 0.05s increment
```

**Typical Profiles:**
- **Bullet:** 5+0.05 (very fast, ~5 min/game)
- **Blitz:** 10+0.1 to 30+0.3 (fast, ~10-30 min/game)
- **Rapid:** 60+0.6 to 300+3 (medium, ~30-90 min/game)
- **Classical:** 600+5 to 1800+10 (slow, hours per game)

**Recommendations:**
- Start with blitz (10+0.1) for quick testing
- Use rapid (60+0.6) for more accurate ELO
- Classical for final strength validation

---

#### `rounds` (Required)

**Type:** Integer

**Purpose:** Number of games to play in the match

**Range:** 1-10000 (practical: 50-1000)

**Examples:**
```yaml
rounds: 10      # Quick test (20 games total with alternating colors)
rounds: 100     # Standard match (200 games)
rounds: 500     # Thorough match (1000 games)
```

**Note:** Each "round" = 2 games (alternating colors)
- rounds: 100 → 200 total games (100 white + 100 black each)

**Statistical Significance:**
- 50 rounds (100 games) - minimum for trends
- 100 rounds (200 games) - good confidence
- 500 rounds (1000 games) - high confidence
- More rounds = more accurate ELO, but longer time

**Time Estimation:**
```
Total Time ≈ rounds × 2 × (base_time × 40 moves / 60)

Example: 100 rounds × 2 × (10s × 40 / 60) ≈ 2.2 hours
```

---

#### `concurrency` (Optional)

**Type:** Integer

**Purpose:** Number of games to run in parallel

**Default:** 1 (sequential)

**Range:** 1-16 (practical: 1-8)

**Examples:**
```yaml
concurrency: 1     # Sequential (deterministic, slower)
concurrency: 2     # 2 games at once
concurrency: 4     # 4 games at once (recommended)
concurrency: 8     # 8 games at once (if CPU allows)
```

**Trade-offs:**
- **Higher:** Faster execution (linear speedup up to CPU cores)
- **Higher:** Non-deterministic (thread scheduling affects results slightly)
- **Lower:** Deterministic, but much slower

**Recommendations:**
- Development testing: concurrency: 4 (fast feedback)
- Final validation: concurrency: 1 (reproducible results)
- CPU with 8+ cores: concurrency: 4-8

**Warning:** High concurrency may affect search quality due to CPU contention

---

#### `outputPgn` (Required)

**Type:** String (path)

**Purpose:** Where to save PGN file with all games

**Path:** Relative to project root

**Examples:**
```yaml
outputPgn: "results/matches/v1.1_vs_v1.0_blitz.pgn"
outputPgn: "results/matches/regression_test.pgn"
```

**File Contents:**
- All games from the match in PGN format
- Headers with engine names, time control, opening, result
- Full move notation
- Viewable in chess GUIs (ChessBase, Arena, etc.)

**Usage:**
- Analyze critical games manually
- Study openings where engine struggled
- Share specific game examples

---

### Complete Match Example

```yaml
matches:
  # Quick debug match
  - name: "v1.1_vs_v1.0_debug"
    engine1Path: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    engine2Path: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    openingBook: "books/test_openings.pgn"
    timeControl: "10+0.1"
    rounds: 10
    concurrency: 1
    outputPgn: "results/matches/debug_match.pgn"

  # Standard blitz match
  - name: "v1.1_vs_v1.0_blitz"
    engine1Path: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    engine2Path: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    openingBook: "books/8moves_GM_LB.pgn"
    timeControl: "60+0.6"
    rounds: 100
    concurrency: 4
    outputPgn: "results/matches/v1.1_vs_v1.0_blitz.pgn"

  # Thorough rapid match
  - name: "v1.1_vs_v1.0_rapid"
    engine1Path: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    engine2Path: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    openingBook: "books/8moves_GM_LB.pgn"
    timeControl: "180+2"
    rounds: 500
    concurrency: 2
    outputPgn: "results/matches/v1.1_vs_v1.0_rapid.pgn"
```

---

## Complete Configuration Example

```yaml
version: "v1.1"
resultsDir: "./results"
cutechessPath: "D:/Games/CuteChess/cutechess-cli.exe"
debugMode: false

testSuites:
  - name: "franky_tests"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000
    maxDepth: 30
  
  - name: "WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30

matches:
  - name: "v1.1_vs_v1.0_blitz"
    engine1Path: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    engine2Path: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    openingBook: "books/8moves_GM_LB.pgn"
    timeControl: "60+0.6"
    rounds: 100
    concurrency: 4
    outputPgn: "results/matches/v1.1_vs_v1.0_blitz.pgn"
```

---

## Validation

The Arena executable validates configuration on startup:

**Checks:**
- ✓ Required fields present
- ✓ EPD files exist
- ✓ Engine executables exist
- ✓ Opening books exist (for matches)
- ✓ cutechess-cli exists (for matches)
- ✓ Numeric values in valid ranges
- ✓ Results directory is writable

**Error Handling:**
- Prints detailed error message indicating the problem
- Shows which file/path is missing
- Exits with error code 1

**Example Error:**
```
ERROR: Configuration validation failed!
EPD file not found: test/testsets/missing.epd

IMPORTANT: Make sure you are running from the project root directory.
Example: cd D:\_DEV\FrankyCPP && .\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe
```

---

## Tips

💡 **Comment out unused sections** with `#` to temporarily disable:
```yaml
# - name: "slow_match"
#   engine1Path: "..."
#   ...
```

💡 **Create multiple config files** for different scenarios:
- `arena_quick.yaml` - Fast test suites only
- `arena_full.yaml` - All suites + matches
- `arena_regression.yaml` - Minimal smoke test

💡 **Use environment variables** in paths (not currently supported, but can be added)

💡 **Test configuration** with a minimal match (rounds: 1) before long runs

💡 **Keep configs in version control** to track testing changes over time

---

*Last updated: 2026-02-01*
