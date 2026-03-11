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
version: "v1.6"                    # Arena version identifier
resultsDir: "./results"            # Results output directory
cutechessPath: "..."               # Path to cutechess-cli executable
debugMode: false                   # Enable UCI debug output (optional)

testSuiteRuns: [...]               # List of grouped test suite runs (NEW)
matches: [...]                     # List of engine matches
benchmarks: [...]                  # List of benchmark configurations
```

---

## Global Settings

### `version` (Required)

**Type:** String

**Purpose:** Identifier for the Arena software version (used for result tracking)

**Usage:** This version tag appears in all result files in the `arenaVersion` field

**Examples:**
```yaml
version: "v1.6"           # Standard version
version: "v1.6-dev"       # Development build
```

**Note:** This is the Arena version, not the engine version. Each engine's version is specified separately in the test suite run configuration.

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
└── benchmarks/    # Consolidated JSON for benchmark results
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

### TestSuiteRunConfig Structure (NEW - Unified Format)

The new `testSuiteRuns` format eliminates duplication by defining shared settings for multiple EPD test suites. Each run configuration specifies engine settings once, with a list of suite EPD files to test.

```yaml
testSuiteRuns:
  - engine: "FrankyCPP v1.6"             # Display name for the engine
    engineVersion: "v1.6"                 # Version string for results grouping
    tag: "QuietSee"                       # Feature tag for tracking (NEW)
    enginePath: "Release/.../engine.exe"  # Path to external UCI engine
    timePerMove: 5000                     # Default milliseconds per position
    maxDepth: 99                          # Default maximum search depth
    isolatePositions: true                # Clear state between positions
    debugMode: false                      # Enable UCI debug output
    commandLineArgs: "--nobook"           # Engine startup arguments
    uciOptions: ""                        # UCI setoption commands
    parallelWorkers: 2                    # Parallel engine instances
    suites:                               # List of EPD files to test
      - "test/testsets/wac.epd"                           # Simple path
      - "test/testsets/franky_tests.epd"                  # Simple path
      - path: "test/testsets/mate_test_suite.epd"         # Override object
        timePerMove: 15000                                 # Custom time for this suite
```

### Benefits of New Format

**Before (Old `testSuites` format):** 7 suites × 9 duplicated fields = 63 lines of configuration

**After (New `testSuiteRuns` format):** 1 block with 7-item list = ~20 lines

**Advantages:**
- Single place to update settings
- `tag` captures what feature is being tested
- Per-suite overrides for `timePerMove` and `maxDepth`
- Suite names derived automatically from EPD filename

---

### Fields

#### `engine` (Required)

**Type:** String

**Purpose:** Display name for the engine

**Usage:** Appears in reports and console output

**Examples:**
```yaml
engine: "FrankyCPP v1.6"
engine: "FrankyCPP v1.6-dev"
engine: "Stockfish 16"
```

---

#### `engineVersion` (Required)

**Type:** String

**Purpose:** Version identifier for grouping and comparison

**Usage:** Used in result filenames and report grouping

**Examples:**
```yaml
engineVersion: "v1.6"
engineVersion: "v1.6-dev"
engineVersion: "v1.4"
```

---

#### `tag` (Optional but Recommended)

**Type:** String

**Purpose:** Feature tag for tracking development progress

**Usage:** Groups results by feature/development phase

**Examples:**
```yaml
tag: "QuietSee"        # Testing Quiescent Search improvements
tag: "TTbuckets"       # Testing TT bucket optimization
tag: "baseline"        # Baseline measurement
tag: ""                # No tag (triggers warning)
```

**Benefits:**
- Track which feature each test run was measuring
- Compare results across development phases
- Historical progression analysis with `--summary --history`

**Warning:** Empty tag triggers a validation warning:
```
WARNING: Test suite run for 'FrankyCPP v1.6' has empty tag - results won't be grouped by feature
```

---

#### `enginePath` (Required)

**Type:** String (path)

**Purpose:** Path to external UCI chess engine executable

**Usage:** All test suites run with external UCI engines for consistent testing

**Examples:**
```yaml
enginePath: "Release/FrankyCPP_v1.6/FrankyCPP_v1.6.exe"
enginePath: "cmake-build-win-release/src/FrankyCPP_v1.6.exe"
enginePath: "D:/Games/Engines/stockfish.exe"
```

---

#### `suites` (Required)

**Type:** List of strings or override objects

**Purpose:** EPD files to test with this engine configuration

**Formats:**

1. **Simple string:** Just the path to the EPD file
   ```yaml
   suites:
     - "test/testsets/wac.epd"
     - "test/testsets/franky_tests.epd"
   ```

2. **Override object:** Path with per-suite settings
   ```yaml
   suites:
     - path: "test/testsets/mate_test_suite.epd"
       timePerMove: 15000    # Override default time
     - path: "test/testsets/deep_tactics.epd"
       maxDepth: 50          # Override default depth
   ```

**Suite Name Derivation:**
- Suite name is automatically derived from the EPD filename
- `test/testsets/wac.epd` → suite name: `wac`
- `test/testsets/STS1-STS15_LAN.EPD` → suite name: `sts1-sts15_lan`

---

#### Other Fields

The following fields work the same as before:

- `timePerMove` - Default time limit per position (milliseconds)
- `maxDepth` - Default maximum search depth
- `isolatePositions` - Clear engine state between positions (default: `true`)
- `debugMode` - Enable UCI communication logging (default: `false`)
- `commandLineArgs` - Command-line arguments for engine startup
- `uciOptions` - UCI setoption commands (semicolon-separated)
- `parallelWorkers` - Number of parallel engine instances (default: `1`)

See detailed documentation for each field below.

---

### Complete Test Suite Run Example

```yaml
testSuiteRuns:
  # FrankyCPP v1.6 - current development
  - engine: "FrankyCPP v1.6"
    engineVersion: "v1.6"
    tag: "QuietSee"
    enginePath: "Release/FrankyCPP_v1.6/FrankyCPP_v1.6.exe"
    timePerMove: 5000
    maxDepth: 99
    isolatePositions: true
    debugMode: false
    commandLineArgs: "--nobook"
    uciOptions: ""
    parallelWorkers: 2
    suites:
      - "test/testsets/franky_tests.epd"
      - path: "test/testsets/mate_test_suite.epd"
        timePerMove: 15000    # Mate tests need more time
      - "test/testsets/wac.epd"
      - "test/testsets/STS1-STS15_LAN.EPD"
      - "test/testsets/crafty_test.epd"
      - "test/testsets/ecm98.epd"
      - "test/testsets/kaufman.epd"

  # FrankyCPP v1.4 - baseline for comparison
  - engine: "FrankyCPP v1.4"
    engineVersion: "v1.4"
    tag: "baseline"
    enginePath: "Release/FrankyCPP_v1.4/FrankyCPP_v1.4.exe"
    timePerMove: 5000
    maxDepth: 99
    isolatePositions: true
    debugMode: false
    commandLineArgs: "--nobook"
    uciOptions: ""
    parallelWorkers: 2
    suites:
      - "test/testsets/franky_tests.epd"
      - path: "test/testsets/mate_test_suite.epd"
        timePerMove: 15000
      - "test/testsets/wac.epd"
      - "test/testsets/STS1-STS15_LAN.EPD"
      - "test/testsets/crafty_test.epd"
      - "test/testsets/ecm98.epd"
      - "test/testsets/kaufman.epd"
```

---

## Individual Field Reference

The following fields are used in `testSuiteRuns` and work as documented below:

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

#### `enginePath` (Required)

**Type:** String (path)

**Purpose:** Path to external UCI chess engine executable

**Usage:** All test suites now run with external UCI engines for consistent testing

**Format:** Same as `cutechessPath` - forward slashes work on all platforms

**Examples:**
```yaml
# Current version (built by CMake)
enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"

# Previous release version
enginePath: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"

# Absolute path
enginePath: "D:/Games/Engines/stockfish.exe"

# Linux
enginePath: "/usr/local/bin/gnuchess"
```

**Requirements:**
- Engine must support UCI protocol
- Must respond to: `uci`, `uciok`, `isready`, `readyok`, `position`, `go`, `bestmove`
- Engine process is started once per suite, reused across positions

**Validation:** Arena checks if file exists before running suite

---

#### `isolatePositions` (Optional)

**Type:** Boolean

**Purpose:** Control whether engine state is cleared between test positions

**Default:** `true` (recommended)

**Values:**
```yaml
isolatePositions: true    # Send "ucinewgame" between positions (default)
isolatePositions: false   # Reuse engine state (TT + history)
```

**Effect:**
- `true`: Sends `ucinewgame` before each position
  - Clears transposition table
  - Clears history heuristics
  - Ensures fair position-by-position comparison
  - Slightly slower but more accurate
- `false`: Keeps engine state across positions
  - Faster execution (no state reset)
  - TT/history from previous positions may affect results
  - Only use for speed or if you understand implications

**When to use `false`:**
- Quick regression testing (speed over precision)
- Testing with engines that have expensive state reset
- You explicitly want to test TT/history carryover effects

**Recommendation:** Leave as `true` for accurate version comparisons

---

#### `commandLineArgs` (Optional)

**Type:** String

**Purpose:** Command-line arguments passed when starting the engine

**Default:** `""` (no arguments)

**Timing:** Passed BEFORE UCI initialization

**Format:** Exactly as you would type in a shell

**Examples:**
```yaml
commandLineArgs: ""                        # No arguments (default)
commandLineArgs: "--nobook"                # Single argument
commandLineArgs: "--nobook --threads 4"    # Multiple arguments
commandLineArgs: "-hash 256 -book off"     # Engine-specific syntax
```

**FrankyCPP Arguments:**
```yaml
commandLineArgs: "--nobook"                # Disable opening book
commandLineArgs: "--help"                  # Show help (testing)
```

**Use Cases:**
- Engine-specific options not available via UCI
- Legacy engines with non-standard option syntax
- Debug flags or logging control

**Warning:** Engine-specific syntax - not portable across engines

**Recommendation:** Use `uciOptions` instead when possible (standard UCI protocol)

---

#### `uciOptions` (Optional)

**Type:** String (semicolon-separated key=value pairs)

**Purpose:** UCI setoption commands sent after engine initialization

**Default:** `""` (no options, use engine defaults)

**Timing:** Sent AFTER UCI initialization (after `uciok`)

**Format:** `"name1=value1; name2=value2; name3=value3"`

**Examples:**
```yaml
# Disable opening book
uciOptions: "OwnBook=false"

# Multiple options
uciOptions: "OwnBook=false; Hash=256"

# Full configuration
uciOptions: "Hash=512; Threads=4; Ponder=false; MultiPV=1"
```

**UCI Standard Options:**
| Option | Type | Description |
|--------|------|-------------|
| `Hash` | Integer | Hash table size in MB |
| `Threads` | Integer | Number of search threads |
| `Ponder` | Boolean | Think on opponent's time |
| `MultiPV` | Integer | Number of principal variations |
| `OwnBook` | Boolean | Use engine's opening book |

**FrankyCPP Supported Options:**
- All UCI standard options above
- Additional engine-specific options (see `src/engine/UciOptions.cpp`)

**Format Rules:**
- Use semicolon (`;`) to separate options (required for multi-word names)
- Whitespace around `=` and `;` is trimmed automatically
- Option names CAN contain spaces per UCI spec (though FrankyCPP uses single-word names)
- No quotes around values (even if they contain spaces)

**Parser Behavior:**
- Invalid format warnings logged but continue processing
- Empty names or values are skipped
- Sends: `setoption name <name> value <value>` to engine

**commandLineArgs vs uciOptions:**
| Feature | commandLineArgs | uciOptions |
|---------|----------------|------------|
| **Timing** | Before UCI init | After UCI init |
| **Format** | Engine-specific | Standard UCI protocol |
| **Portability** | Engine-specific | Works across all UCI engines |
| **Use case** | Non-UCI options | Standard configuration |
| **Recommendation** | Avoid if possible | Preferred method |

**Best Practice:** Use `uciOptions` for all standard UCI options, reserve `commandLineArgs` only for engine-specific startup flags not available via UCI.

---

#### `debugMode` (Optional)

**Type:** Boolean

**Purpose:** Enable detailed UCI communication logging for test suite

**Default:** `false`

**Effect:** Prints all UCI commands sent to/received from the engine

**Examples:**
```yaml
debugMode: false    # Normal operation (clean output)
debugMode: true     # Verbose UCI protocol logging
```

**Output Example (debugMode: true):**
```
[UCI] -> uci
[UCI] <- id name FrankyCPP v1.1
[UCI] <- id author Frank Kopp
[UCI] <- uciok
[UCI] -> isready
[UCI] <- readyok
[UCI] -> position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
[UCI] -> go movetime 5000 depth 30
[UCI] <- info depth 1 score cp 25 nodes 123 time 5
[UCI] <- bestmove e2e4
```

**Use Cases:**
- Debugging engine UCI implementation issues
- Investigating unexpected move selection
- Verifying engine receives correct FEN strings
- Understanding search behavior

**Warning:** Very verbose - produces large amounts of output

---

#### `parallelWorkers` (Optional)

**Type:** Integer

**Purpose:** Number of parallel engine instances for concurrent position testing

**Default:** `1` (sequential execution)

**Range:** 1-16 (practical range: 1-8)

**Examples:**
```yaml
parallelWorkers: 1     # Sequential execution (default)
parallelWorkers: 4     # 4 parallel engine instances
parallelWorkers: 8     # 8 parallel engines (for large suites)
```

**Effect:**
- `1`: Positions tested one at a time (original behavior)
- `N>1`: N engine processes run concurrently, each testing different positions

**Architecture:**
- Uses ThreadPool from `common/ThreadPool.h`
- Each worker thread manages its own UCIEngine instance
- Thread-local engine storage ensures proper isolation
- Results collected in original position order via futures

**Performance:**

| Workers | Engine Processes | Memory (est.) | Speedup |
|---------|------------------|---------------|---------|
| 1       | 1                | ~200 MB       | 1x      |
| 2       | 2                | ~400 MB       | ~1.9x   |
| 4       | 4                | ~800 MB       | ~3.5x   |
| 8       | 8                | ~1.6 GB       | ~6x     |

**Output:**
- Progress display: `Progress: 150/300 (50%) [120 passed, 30 failed]`
- Summary shows both engine time (sum) and wall time (actual elapsed)
- Speedup factor calculated: `Engine Time / Wall Time`

**Example Output:**
```
Test Suite Complete: WAC (parallel)
  Total Tests:  300
  Passed:       285 (95%)
  Failed:       15
  Skipped:      0
  Total Nodes:  123456789
  Engine Time:  1500000ms (sum of all positions)
  Wall Time:    450000ms (actual elapsed)
  Speedup:      3.33x
```

**When to use:**
- Large test suites (100+ positions)
- Time-per-move >= 2 seconds
- Sufficient RAM for multiple engines
- CPU has multiple cores available

**When NOT to use:**
- Debugging (use sequential for clear output)
- Small test suites (<20 positions, startup overhead dominates)
- Limited RAM systems
- Comparing exact search behavior (parallel may affect timing)

**Recommendations:**
- Start with `parallelWorkers: 4` and adjust based on system
- Monitor memory usage with large worker counts
- Keep `isolatePositions: true` for accurate results
- For debugging, set `parallelWorkers: 1`

---

### Complete Test Suite Example

```yaml
testSuites:
  # Quick regression test with current build (sequential)
  - name: "franky_tests_v1.1"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 2000
    maxDepth: 30
    enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    isolatePositions: true
    commandLineArgs: ""
    uciOptions: "OwnBook=false"
    debugMode: false
    parallelWorkers: 1            # Sequential for small suite

  # Previous version for comparison
  - name: "franky_tests_v1.0"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 2000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    isolatePositions: true
    commandLineArgs: ""
    uciOptions: "OwnBook=false; Hash=128"
    debugMode: false
    parallelWorkers: 1

  # Standard tactical test (parallel)
  - name: "WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    isolatePositions: true
    commandLineArgs: ""
    uciOptions: "OwnBook=false"
    debugMode: false
    parallelWorkers: 4            # 4 parallel workers for ~3x speedup

  # Strategic test with longer time (parallel)
  - name: "STS"
    epdPath: "test/testsets/STS1-STS15_LAN.EPD"
    timePerMove: 10000
    maxDepth: 40
    enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    isolatePositions: true
    commandLineArgs: ""
    uciOptions: "OwnBook=false; Hash=256"
    parallelWorkers: 8            # More workers for large suite
    debugMode: false

  # Mate problems with deep search
  - name: "mate_test"
    epdPath: "test/testsets/mate_test_suite.epd"
    timePerMove: 15000
    maxDepth: 50
    enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    isolatePositions: true
    commandLineArgs: ""
    uciOptions: "OwnBook=false"
    debugMode: false

  # Test with external engine (Stockfish)
  - name: "stockfish_WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "D:/Engines/stockfish.exe"
    isolatePositions: true
    commandLineArgs: ""
    uciOptions: "Hash=512; Threads=4"
    debugMode: false
```

---

## Match Configuration

### MatchConfig Structure

```yaml
matches:
  - name: "v1.1_vs_FrankyGo_blitz_100"                 # Match identifier
    engine1Path: "Release/FrankyCPP_V1.1/FrankyCPP_v1.1.exe"  # First engine
    engine1Version: "v1.1"                             # Engine 1 version (required)
    engine1Options: "OwnBook=false"                    # UCI options for engine 1 (optional)
    engine2Path: "D:/Games/FrankyChess/FrankyGo/FrankyGo.exe"  # Second engine
    engine2Version: "v1.0.3"                           # Engine 2 version (required)
    engine2Options: "OwnBook=false"                    # UCI options for engine 2 (optional)
    openingBook: "books/8moves_GM_LB.pgn"              # Opening book PGN
    timeControl: "60+0.6"                              # Time control
    rounds: 100                                        # Number of games
    concurrency: 4                                     # Parallel games
    batchSize: 0                                       # Games per batch (0=auto)
    outputPgn: "results/matches/v1.1_vs_FrankyGo_blitz_100.pgn"  # PGN output
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

#### `engine1Version` (Required)

**Type:** String

**Purpose:** Explicit version identifier for engine 1

**Usage:** Used in result files and reporting for clear engine identification

**Format:** Free-form string, but recommend semantic versioning

**Examples:**
```yaml
engine1Version: "v1.1"          # Standard version
engine1Version: "v1.2-dev"      # Development version
engine1Version: "v1.1.2"        # Detailed version
engine1Version: "dev-20260206"  # Date-based version
```

**Best Practices:**
- Match the actual engine version being tested
- Use consistent naming across all configurations
- Include enough detail for unique identification

**Note:** If omitted, will be empty in results (not recommended)

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

#### `engine2Version` (Required)

**Type:** String

**Purpose:** Explicit version identifier for engine 2

**Same format and requirements as `engine1Version`**

**Examples:**
```yaml
engine2Version: "v1.0"       # Previous release
engine2Version: "v0.5"       # Older baseline
engine2Version: "v1.0.3"     # External engine version (e.g., FrankyGo)
engine2Version: "dev"        # Stockfish development build
```

**Note:** Especially important for external engines where version may not be obvious from path

---

#### `openingBook` (Required)

**Type:** String (path)

**Purpose:** PGN file containing opening positions

**Format:** Must be PGN format (not Polyglot .bin)

**Path:** Relative to project root

**Examples:**
```yaml
openingBook: "books/8moves_GM_LB.pgn"     # 8-move GM openings
openingBook: "books/superbook.pgn"        # Large opening book (dev only, not in releases)
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

#### `batchSize` (Optional)

**Type:** Integer

**Purpose:** Number of games per batch for resumable match execution

**Default:** 0 (auto-calculate: `max(2, concurrency)` rounded up to even)

**Range:** Must be even, >= 2

**Examples:**
```yaml
batchSize: 0       # Auto-calculate (default, recommended)
batchSize: 2       # Minimum batch size (finest granularity)
batchSize: 4       # Match concurrency of 4
batchSize: 10      # Larger batches (less overhead, coarser resume points)
```

**How it works:**
- Matches run in batches of `batchSize` games
- State is saved after each batch to `results/matches/.state/`
- If interrupted (Ctrl+C), resume from last saved state
- On completion, state file is automatically deleted

**Auto-calculation (batchSize: 0):**
- Takes `max(2, concurrency)`
- Rounds up to next even number (for color fairness)
- Example: concurrency=3 → batchSize=4

**Constraints:**
- Must be even (each batch has equal games per color)
- `rounds` must be divisible by `batchSize`
- Should be >= `concurrency` to fully utilize parallel games

**Resumable matches:**
```
# If interrupted at batch 25 of 50:
*** RESUMING MATCH ***
  State file:        results/matches/.state/v1.1_vs_v1.0.state.json
  Completed rounds:  50
  Remaining rounds:  50
  Current score:     28 - 18 - 4 (W-L-D)
```

**To restart a match from scratch:**
1. Delete the state file in `results/matches/.state/`
2. Optionally delete the output PGN file
3. Re-run the match

**Recommendations:**
- Leave as 0 (auto) for most cases
- Set explicitly if you need specific resume granularity
- Larger batches = less overhead, but more games lost on interrupt

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
    engine1Version: "v1.1"
    engine1Options: "OwnBook=false"
    engine2Path: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    engine2Version: "v1.0"
    engine2Options: "OwnBook=false"
    openingBook: "books/test_openings.pgn"
    timeControl: "10+0.1"
    rounds: 10
    concurrency: 1
    batchSize: 2                  # Minimum batch size for fine-grained resume
    outputPgn: "results/matches/debug_match.pgn"

  # Standard blitz match (resumable)
  - name: "v1.1_vs_v1.0_blitz"
    engine1Path: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    engine1Version: "v1.1"
    engine1Options: "OwnBook=false"
    engine2Path: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    engine2Version: "v1.0"
    engine2Options: "OwnBook=false"
    openingBook: "books/8moves_GM_LB.pgn"
    timeControl: "60+0.6"
    rounds: 100
    concurrency: 4
    batchSize: 0                  # Auto: will use 4 (matches concurrency)
    outputPgn: "results/matches/v1.1_vs_v1.0_blitz.pgn"

  # Thorough rapid match
  - name: "v1.1_vs_v1.0_rapid"
    engine1Path: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    engine1Version: "v1.1"
    engine1Options: "OwnBook=false"
    engine2Path: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    engine2Version: "v1.0"
    engine2Options: "OwnBook=false"
    openingBook: "books/8moves_GM_LB.pgn"
    timeControl: "180+2"
    rounds: 500
    concurrency: 2
    batchSize: 10                 # Larger batches for less overhead
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
    enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    uciOptions: "OwnBook=false"
  
  - name: "WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    uciOptions: "OwnBook=false"
    parallelWorkers: 4

matches:
  - name: "v1.1_vs_v1.0_blitz"
    engine1Path: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    engine1Version: "v1.1"
    engine1Options: "OwnBook=false"
    engine2Path: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    engine2Version: "v1.0"
    engine2Options: "OwnBook=false"
    openingBook: "books/8moves_GM_LB.pgn"
    timeControl: "60+0.6"
    rounds: 100
    concurrency: 4
    batchSize: 0                  # Auto-calculate for resumable matches
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

*Last updated: 2026-02-07*
