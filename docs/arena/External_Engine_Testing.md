# External Engine Testing Guide

## Overview

The FrankyCPP Arena test suite framework exclusively uses **external UCI engines** for all EPD tactical testing. This design ensures:

- ✅ **Consistent UCI testing** - Tests the engine exactly as users will interact with it
- ✅ **Production-like environment** - No special internal API access
- ✅ **Version comparison** - Compare different engine builds fairly
- ✅ **Cross-engine testing** - Test any UCI-compliant engine (Stockfish, etc.)

---

## Architecture

### External-Only Design

**Previous approach (v1.0):** Test suites used internal engine API directly  
**Current approach (v1.1+):** All test suites use external UCI engines

**Benefits:**
- Single code path (simpler, more maintainable)
- Better UCI protocol coverage (dogfooding)
- Matches real-world usage (GUIs, tournaments)
- Easier to compare with external engines

### Engine Lifecycle

```
┌──────────────────────────────────────────────────────┐
│ Test Suite Begins                                    │
└────────────────┬─────────────────────────────────────┘
                 ▼
        ┌────────────────────┐
        │ Start UCI Engine   │ ← Pass commandLineArgs
        │ (one per suite)    │
        └────────┬───────────┘
                 ▼
        ┌────────────────────┐
        │ Send "uci"         │
        │ Wait for "uciok"   │
        └────────┬───────────┘
                 ▼
        ┌────────────────────┐
        │ Send UCI options   │ ← Apply uciOptions
        │ (setoption...)     │
        └────────┬───────────┘
                 ▼
        ┌────────────────────────────────────┐
        │ For each position in EPD file:     │
        │                                    │
        │  1. Send "ucinewgame" (optional)   │ ← if isolatePositions=true
        │  2. Send "position fen ..."        │
        │  3. Send "go movetime X depth Y"   │
        │  4. Parse "info" lines             │
        │  5. Extract "bestmove"             │
        │  6. Compare with expected move     │
        │  7. Record result (pass/fail)      │
        └────────┬───────────────────────────┘
                 ▼
        ┌────────────────────┐
        │ Send "quit"        │
        │ Close engine       │
        └────────┬───────────┘
                 ▼
┌────────────────────────────────────────────────────────┐
│ Suite Complete - Save JSON results                     │
└────────────────────────────────────────────────────────┘
```

**Key Points:**
- **One engine process per suite** (not per position)
- Engine is reused across all positions
- `ucinewgame` clears state between positions (when `isolatePositions=true`)
- Matches real UCI GUI behavior

---

## Configuration

### Required Fields

```yaml
testSuites:
  - name: "test_name"              # Unique identifier
    epdPath: "path/to/test.epd"    # EPD file with test positions
    timePerMove: 5000              # Milliseconds per position
    maxDepth: 30                   # Maximum search depth
    enginePath: "path/to/engine"   # UCI engine executable (REQUIRED)
```

### Optional Fields

```yaml
    isolatePositions: true         # Clear state between positions (default: true)
    commandLineArgs: ""            # Engine startup arguments (default: "")
    uciOptions: ""                 # UCI setoption commands (default: "")
    debugMode: false               # Enable UCI debug output (default: false)
```

---

## Position Isolation

### What is Position Isolation?

Position isolation controls whether the engine's internal state (transposition table, history heuristics) is cleared between test positions.

### isolatePositions: true (Default - Recommended)

**Effect:** Sends `ucinewgame` before each position

**What gets cleared:**
- Transposition table (TT)
- History heuristics
- Killer moves
- Pawn hash table
- All search state

**Pros:**
- ✅ Fair position-by-position comparison
- ✅ Each position evaluated independently
- ✅ No cross-contamination from previous positions
- ✅ Consistent results across runs

**Cons:**
- ❌ Slightly slower (state reset overhead)
- ❌ Not representative of real games (where TT persists)

**Use case:** Version comparison, tactical testing, regression testing

### isolatePositions: false

**Effect:** Engine state persists across positions

**What persists:**
- Transposition table entries from previous positions
- History heuristic counters
- All accumulated search knowledge

**Pros:**
- ✅ Faster execution (no reset overhead)
- ✅ More realistic (simulates continuous play)

**Cons:**
- ❌ Position order affects results
- ❌ Unfair comparison (later positions benefit from earlier TT entries)
- ❌ Results not reproducible (TT state dependent)

**Use case:** Quick smoke tests, performance benchmarks

### Recommendation

**Always use `isolatePositions: true` for:**
- Version comparisons
- Regression testing
- Tactical test suites (WAC, STS, etc.)
- Any test where solve rate matters

**Only use `isolatePositions: false` for:**
- Quick sanity checks
- Performance testing (where TT persistence is feature)
- Explicitly testing TT effectiveness

---

## Engine Options

### Command-Line Arguments

**Field:** `commandLineArgs`  
**Timing:** Passed when starting engine process (BEFORE UCI initialization)  
**Format:** Exactly as you would type in shell  

**Examples:**
```yaml
# FrankyCPP
commandLineArgs: ""              # Use defaults
commandLineArgs: "--nobook"      # Disable opening book

# Hypothetical engine with custom syntax
commandLineArgs: "-hash 256 -threads 4"
commandLineArgs: "/NoBook /LogFile debug.log"
```

**Use Cases:**
- Engine-specific startup flags
- Options not available via UCI
- Debug/logging control

**Warning:** Engine-specific syntax - not portable across engines

### UCI Options

**Field:** `uciOptions`  
**Timing:** Sent AFTER UCI initialization (after `uciok`)  
**Format:** Semicolon-separated `name=value` pairs  

**Examples:**
```yaml
# Single option
uciOptions: "OwnBook=false"

# Multiple options (recommended format)
uciOptions: "OwnBook=false; Hash=256"

# Full configuration
uciOptions: "Hash=512; Threads=4; Ponder=false; MultiPV=1"
```

**UCI Standard Options:**
| Option | Type | Example | Description |
|--------|------|---------|-------------|
| `OwnBook` | Boolean | `true`/`false` | Use engine's opening book |
| `Hash` | Integer | `128`, `256`, `512` | Hash table size (MB) |
| `Threads` | Integer | `1`, `4`, `8` | Search threads |
| `Ponder` | Boolean | `true`/`false` | Think on opponent's time |
| `MultiPV` | Integer | `1`, `3`, `5` | Number of variations |

**FrankyCPP Options:**
- All UCI standard options above
- Additional options documented in `src/engine/UciOptions.cpp`

**Format Rules:**
- Use semicolon (`;`) separator (required for reliability)
- Whitespace around `=` and `;` is trimmed
- Option names can contain spaces (per UCI spec)
- No quotes around values

**Parser Behavior:**
- Sends: `setoption name <name> value <value>`
- Invalid format → Warning logged, continue processing
- Empty names/values → Skipped

### commandLineArgs vs uciOptions

| Feature | commandLineArgs | uciOptions |
|---------|----------------|------------|
| **Timing** | Before UCI init | After UCI init |
| **Format** | Engine-specific | Standard UCI |
| **Portability** | Engine-specific | All UCI engines |
| **Recommended** | Avoid if possible | **Preferred** |

**Best Practice:** Use `uciOptions` for all standard UCI options. Only use `commandLineArgs` for engine-specific startup flags not available via UCI protocol.

---

## Common Configurations

### Test Current Build

```yaml
testSuites:
  - name: "franky_tests_v1.1"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    isolatePositions: true
    commandLineArgs: ""
    uciOptions: "OwnBook=false"
    debugMode: false
```

### Compare Two Versions

```yaml
testSuites:
  # Current version
  - name: "franky_tests_v1.1"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    isolatePositions: true
    uciOptions: "OwnBook=false; Hash=128"

  # Previous version (same settings for fair comparison)
  - name: "franky_tests_v1.0"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
    isolatePositions: true
    uciOptions: "OwnBook=false; Hash=128"
```

**Then compare:**
```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --compare v1.1 v1.0
```

### Test External Engine

```yaml
testSuites:
  - name: "stockfish_WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "D:/Engines/stockfish.exe"
    isolatePositions: true
    commandLineArgs: ""                    # Stockfish has no command-line args
    uciOptions: "Hash=512; Threads=4"      # Standard UCI options
    debugMode: false
```

### Debug Engine Behavior

```yaml
testSuites:
  - name: "debug_test"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    isolatePositions: true
    commandLineArgs: ""
    uciOptions: "OwnBook=false"
    debugMode: true                        # Print all UCI communication
```

**Output:**
```
[UCI] -> uci
[UCI] <- id name FrankyCPP v1.1
[UCI] <- id author Frank Kopp
[UCI] <- option name OwnBook type check default true
[UCI] <- uciok
[UCI] -> setoption name OwnBook value false
[UCI] -> isready
[UCI] <- readyok
[UCI] -> position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
[UCI] -> go movetime 5000 depth 30
[UCI] <- info depth 1 score cp 25 nodes 123 time 5
[UCI] <- info depth 2 score cp 28 nodes 456 time 12
...
[UCI] <- bestmove e2e4
```

---

## EPD Format

### Supported Operations

The Arena supports three EPD test operations:

#### bm (Best Move)
Position passes if engine finds any of the expected moves.

```
r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - bm Bxc6; id "WAC.001";
```

Multiple acceptable moves:
```
rnbqkb1r/pp3ppp/2p5/3P4/8/8/PPP2PPP/RNBQKBNR w KQkq - bm d6 Nf3 Nc3; id "multi.001";
```

#### am (Avoid Move)
Position passes if engine does NOT find any of the listed moves.

```
r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - am Nxe5; id "avoid.001";
```

#### dm (Direct Mate)
Position passes if engine finds mate in N moves or fewer.

```
r1b1kb1r/pppp1ppp/5q2/4n3/3KP3/2N3PN/PPP4P/R1BQ1B1R b kq - dm 2; id "mate.001";
```

### Format Notes

- Semicolon separates fields
- `id` field is optional but recommended
- Move notation: UCI long algebraic (e2e4, g1f3, e7e8q)
- SAN notation also supported (Nf3, Bxc6, O-O)

---

## Result Files

### Test Suite Results

**Location:** `results/testsuites/`  
**Format:** JSON  
**Naming:** `{version}_{suite_name}_{timestamp}.json`

**Example:** `v1.1_franky_tests_20260201_143022.json`

**Content:**
```json
{
  "version": "v1.1",
  "suiteName": "franky_tests",
  "engineName": "FrankyCPP v1.1",
  "enginePath": "cmake-build-win-release/src/FrankyCPP_v1.1.exe",
  "timestamp": "20260201_143022",
  "totalTests": 50,
  "passed": 48,
  "failed": 2,
  "skipped": 0,
  "totalNodes": 1234567,
  "totalTime": 250000,
  "results": [
    {
      "id": "test.001",
      "fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      "bestMove": "e2e4",
      "expectedMoves": ["e2e4", "d2d4"],
      "testType": "bm",
      "passed": true,
      "nodes": 24567,
      "time": 5000,
      "depth": 12
    }
  ]
}
```

### Version Comparison

**Command:**
```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --compare v1.1 v1.0
```

**Output Location:** `results/comparisons/`  
**Format:** Plain text report  
**Naming:** `{version1}_vs_{version2}_{timestamp}.txt`

**Report Sections:**
1. **Header** - Versions compared, timestamp
2. **Test Suite Comparison** - Solve rates, improvement/regression
3. **Position Details** - Which positions improved/regressed
4. **Match Comparison** - ELO difference (if matches exist)
5. **Summary** - Overall strength change

---

## Troubleshooting

### Engine Not Found

**Error:** `Failed to start UCI engine: file not found`

**Causes:**
- Engine path incorrect
- Engine executable doesn't exist
- Wrong path separators (use `/` on all platforms)

**Solutions:**
```yaml
# Use forward slashes (works on Windows and Linux)
enginePath: "D:/Games/engine.exe"          # Windows
enginePath: "/usr/local/bin/engine"        # Linux

# Or use relative paths from project root
enginePath: "cmake-build-win-release/src/FrankyCPP_v1.1.exe"
enginePath: "Release/FrankyCPP_V1.0/FrankyCPP_v1.0.exe"
```

### Engine Doesn't Respond

**Error:** `UCI initialization timeout`

**Causes:**
- Engine doesn't support UCI protocol
- Engine crashed on startup
- Command-line arguments invalid

**Solutions:**
1. Test engine manually:
   ```powershell
   echo "uci" | .\path\to\engine.exe
   ```
   Should print `uciok` within a few seconds

2. Enable debug mode:
   ```yaml
   debugMode: true
   ```

3. Remove command-line arguments:
   ```yaml
   commandLineArgs: ""
   ```

### No Moves Generated

**Error:** `Engine returned empty move`

**Causes:**
- Invalid FEN string in EPD file
- Engine doesn't understand position command
- Engine crashes during search

**Solutions:**
1. Enable debug mode to see UCI communication
2. Test position manually:
   ```
   uci
   position fen [FEN_STRING]
   go movetime 1000
   ```
3. Check EPD file format

### Wrong Results

**Problem:** Engine solves positions differently than expected

**Not a bug if:**
- Different engine version (expected)
- Different time/depth limits
- `isolatePositions: false` (TT contamination)
- Different UCI options (Hash, Threads affect search)

**Debugging:**
1. Use same settings for both versions
2. Enable `isolatePositions: true`
3. Disable opening book (`OwnBook=false`)
4. Use consistent hash table size
5. Run multiple times to check consistency

---

## Implementation Details

### UCIEngine Class

**Location:** `src/engine_arena/UCIEngine.{h,cpp}`

**Features:**
- Cross-platform subprocess management (Boost.Process)
- UCI protocol implementation
- Timeout handling
- Error recovery

**Key Methods:**
- `UCIEngine(path, commandLineArgs)` - Start engine, initialize UCI
- `setPosition(fen)` - Set board position
- `search(time, depth)` - Run search, return result
- `newGame()` - Clear engine state (send `ucinewgame`)
- `setUciOptions(options)` - Send UCI setoption commands

### Move Comparison

**Location:** `src/chesscore/MoveUtils.{h,cpp}`

**Function:** `matchesExpectedMove(actual, expected, fen)`

**Strategy:**
1. Fast path: Direct normalized string comparison (UCI format)
2. Slow path: Parse via MoveGenerator (handles SAN notation)

**Supported Notations:**
- UCI long algebraic: `e2e4`, `g1f3`, `e7e8q`
- SAN: `Nf3`, `Bxc6`, `O-O`, `e8=Q`

### EPD Parser

**Location:** `src/common/EPDParser.{h,cpp}`

**Features:**
- Parses EPD test operations (bm, am, dm)
- Handles multiple expected moves
- Extracts FEN, test type, ID fields
- Robust error handling

---

## Testing

The external engine testing feature includes comprehensive automated tests:

### Integration Tests
**Location:** `test/engine_arena/TestSuiteRunner_IntegrationTest.cpp`  
**Count:** 11 tests

Tests cover:
- Single-position test execution
- Multiple test types (BM, AM, DM)
- Sequential suite execution
- Position isolation modes
- Result metadata validation
- Error handling (missing files, invalid FEN)
- Stress testing (10+ positions)

### Error Handling Tests
**Location:** `test/engine_arena/UCIEngine_ErrorHandlingTest.cpp`  
**Count:** 10 tests

Tests cover:
- Missing engine executable
- Invalid paths
- Invalid FEN strings
- Search timeouts
- Resource leak prevention
- Edge cases (zero time, long names)

### UCI Options Tests
**Location:** `test/engine_arena/UCIEngine_OptionsTest.cpp`  
**Count:** 20 tests

Tests cover:
- Single-word option names
- Multi-word option names
- Multiple options (semicolon/space separated)
- Boolean and numeric values
- Whitespace handling
- Invalid format recovery
- FrankyCPP-specific options

**Total:** 41 comprehensive automated tests

---

## Version History

- **v1.1 (2026-02-04):** External UCI engine support with comprehensive configuration
  - External-only design (removed internal engine path)
  - UCI options support (`uciOptions` field)
  - Command-line arguments support (`commandLineArgs` field)
  - Position isolation control (`isolatePositions` field)
  - Debug mode for UCI communication (`debugMode` field)
  - 41 automated tests covering all functionality

- **v1.0 (2026-01-30):** Initial Arena framework
  - Internal engine API testing only
  - Basic EPD test suite support

---

## See Also

- [Arena Quick Start](README.md) - Getting started guide
- [Configuration Reference](Configuration.md) - Complete YAML reference
- [Result Analysis](Results.md) - Understanding test results
- [Development Guide](Development.md) - Contributing to Arena

---

*Last updated: 2026-02-04*
