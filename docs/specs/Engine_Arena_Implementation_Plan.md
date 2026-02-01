# FrankyCPP Engine Arena - Implementation Plan

## Overview

A standalone framework for testing, benchmarking, and tracking chess engine strength across versions. Located in `./src/engine_arena/` to keep it separated from core engine code while leveraging existing logic.

**Goal:** Enable objective measurement of engine strength changes between versions through automated test suites and cutechess-cli matches.

---

## Architecture

### Directory Structure

```
src/
├── engine_arena/           # NEW: Standalone testing framework
│   ├── ArenaRunner.h       # Main orchestrator
│   ├── ArenaRunner.cpp
│   ├── TestSuiteRunner.h   # EPD test suite execution
│   ├── TestSuiteRunner.cpp
│   ├── MatchRunner.h       # cutechess-cli wrapper
│   ├── MatchRunner.cpp
│   ├── ResultWriter.h      # JSON/YAML result persistence
│   ├── ResultWriter.cpp
│   └── ArenaConfig.h       # Configuration structures
│
├── engine_arena_main.cpp   # NEW: Standalone executable entry point

config/
├── arena.yaml             # NEW: Arena configuration

docs/
├── arena/                 # NEW: Arena-specific documentation folder
│   ├── README.md          # Quick start and overview
│   ├── Configuration.md   # Detailed config reference
│   ├── Results.md         # Result format documentation
│   └── Development.md     # Extension and development guide

results/                   # NEW: Result output directory
├── testsuites/
│   └── v1.1_wac_20260201_143022.json
└── matches/
    └── v1.1_vs_v1.0_20260201_150033.json
```

**Documentation Guidelines:**
- **All arena-specific documentation** goes in `docs/arena/` folder
- **Do NOT** scatter arena docs directly in `docs/` root - keep them organized
- Split documentation logically by topic (README, Configuration, Results, Development)
- Keep docs consolidated - avoid creating dozens of small files

### Executables

1. **FrankyCPP_v1.1** - Main UCI engine (unchanged)
2. **FrankyCPP_v1.1_Test** - Unit tests (unchanged)
3. **FrankyCPP_v1.1_Arena** - NEW: Arena test runner

---

## Component Design

### 1. ArenaConfig (Configuration)

**File:** `src/engine_arena/ArenaConfig.h`

```cpp
struct TestSuiteConfig {
    std::string name;           // "WAC", "STS", etc.
    std::string epdPath;        // Path to EPD file
    milliseconds timePerMove;   // e.g., 5000ms
    Depth maxDepth;            // e.g., 30
};

struct MatchConfig {
    std::string name;                  // "v1.1 vs v1.0"
    std::string engine1Path;           // "./FrankyCPP_v1.1.exe"
    std::string engine2Path;           // "./FrankyCPP_v1.0.exe"
    std::string cutechessPath;         // Path to cutechess-cli
    std::string openingBook;           // PGN opening book
    std::string timeControl;           // "10+0.1"
    int rounds;                        // 1000
    std::string outputPgn;             // Where to save games
};

struct ArenaConfig {
    std::string version;               // Engine version "v1.1"
    std::string resultsDir;            // "./results/"
    std::vector<TestSuiteConfig> testSuites;
    std::vector<MatchConfig> matches;
};
```

**Loading:** Via YAML file (`config/arena.yaml`) using yaml-cpp (already in dependencies)

---

### 2. TestSuiteRunner (EPD Test Suite Execution)

**File:** `src/engine_arena/TestSuiteRunner.h`

**Purpose:** Wrapper around existing `TestSuite` class from `src/enginetest/TestSuite.h`

```cpp
struct TestSuiteResult {
    std::string version;        // "v1.1"
    std::string suiteName;      // "WAC"
    std::string timestamp;      // ISO 8601 format
    int totalTests;
    int passed;
    int failed;
    int skipped;
    uint64_t totalNodes;
    nanoseconds totalTime;
    std::vector<TestCaseDetail> details; // Per-test breakdown
};

class TestSuiteRunner {
public:
    TestSuiteRunner(const ArenaConfig& config);
    
    // Run a single test suite and return results
    TestSuiteResult runTestSuite(const TestSuiteConfig& config);
    
    // Run all configured test suites
    std::vector<TestSuiteResult> runAllTestSuites();
    
private:
    const ArenaConfig& arenaConfig;
};
```

**Key Features:**
- Reuses existing `TestSuite` class (no duplication)
- Captures detailed per-test results for comparison
- Adds metadata (version, timestamp, system info)

---

### 3. MatchRunner (cutechess-cli Integration)

**File:** `src/engine_arena/MatchRunner.h`

**Purpose:** Execute engine-vs-engine matches via cutechess-cli and parse results

```cpp
struct MatchResult {
    std::string version;              // "v1.1"
    std::string matchName;            // "v1.1 vs v1.0"
    std::string timestamp;            // ISO 8601
    std::string engine1Name;
    std::string engine2Name;
    int engine1Wins;
    int engine2Wins;
    int draws;
    double engine1Score;              // Points (win=1, draw=0.5)
    double engine2Score;
    double eloDifference;             // Calculated from score
    std::string pgnPath;              // Path to saved games
    nanoseconds duration;
};

class MatchRunner {
public:
    MatchRunner(const ArenaConfig& config);
    
    // Run a single match and return results
    MatchResult runMatch(const MatchConfig& config);
    
    // Run all configured matches
    std::vector<MatchResult> runAllMatches();
    
private:
    const ArenaConfig& arenaConfig;
    
    // Execute cutechess-cli command
    bool executeCutechess(const MatchConfig& config, std::string& output);
    
    // Parse cutechess output for results
    MatchResult parseOutput(const std::string& output, const MatchConfig& config);
    
    // Calculate ELO difference from score
    double calculateEloDifference(double score, int games);
};
```

**Implementation Notes:**
- Use PowerShell `Start-Process` or C++ `std::system()` to invoke cutechess-cli
- Parse cutechess output (stdout/stderr) to extract W/D/L counts
- Calculate ELO using standard formula: ELO_diff = -400 * log10(1/score - 1)
- Validate cutechess-cli exists at configured path before running

---

### 4. ResultWriter (Persistence)

**File:** `src/engine_arena/ResultWriter.h`

**Purpose:** Write structured results to JSON files for version comparison

```cpp
class ResultWriter {
public:
    ResultWriter(const std::string& resultsDir);
    
    // Write test suite results to JSON
    void writeTestSuiteResult(const TestSuiteResult& result);
    
    // Write match results to JSON
    void writeMatchResult(const MatchResult& result);
    
    // Compare two test suite results
    void writeComparison(const TestSuiteResult& v1, const TestSuiteResult& v2);
    
private:
    std::string resultsDir;
    
    // Generate filename with timestamp
    std::string generateFilename(const std::string& prefix, const std::string& version);
};
```

**Output Format (JSON):**

```json
{
  "version": "v1.1",
  "suiteName": "WAC",
  "timestamp": "2026-02-01T14:30:22Z",
  "systemInfo": {
    "os": "Windows 11",
    "compiler": "MSVC 2022",
    "buildType": "Release"
  },
  "summary": {
    "totalTests": 300,
    "passed": 285,
    "failed": 15,
    "successRate": 95.0,
    "totalNodes": 45000000,
    "totalTimeMs": 85000
  },
  "details": [
    {
      "testId": "WAC.001",
      "fen": "...",
      "expected": "Qg6",
      "actual": "Qg6",
      "passed": true,
      "nodes": 150000,
      "timeMs": 285
    }
  ]
}
```

**Why JSON?**
- Easy to parse for automated comparison scripts
- Human-readable for manual inspection
- Wide tool support (Python, JavaScript, etc.)
- Can generate CSV/HTML reports later

---

### 5. ArenaRunner (Main Orchestrator)

**File:** `src/engine_arena/ArenaRunner.h`

**Purpose:** Main entry point that coordinates all testing

```cpp
class ArenaRunner {
public:
    ArenaRunner(const std::string& configPath);
    
    // Run all configured tests
    void runAll();
    
    // Run only test suites
    void runTestSuitesOnly();
    
    // Run only matches
    void runMatchesOnly();
    
    // Generate comparison report between two versions
    void compareVersions(const std::string& version1, const std::string& version2);
    
private:
    ArenaConfig config;
    TestSuiteRunner testSuiteRunner;
    MatchRunner matchRunner;
    ResultWriter resultWriter;
};
```

---

### 6. Main Executable

**File:** `src/engine_arena_main.cpp`

```cpp
#include "engine_arena/ArenaRunner.h"
#include "common/Logging.h"
#include "init.h"
#include <boost/program_options.hpp>

int main(int argc, char* argv[]) {
    init::init();
    
    namespace po = boost::program_options;
    po::options_description desc("FrankyCPP Arena - Engine Strength Testing");
    desc.add_options()
        ("help,h", "Show help message")
        ("config,c", po::value<std::string>()->default_value("config/arena.yaml"), 
         "Configuration file path")
        ("testsuites", "Run test suites only")
        ("matches", "Run matches only")
        ("compare", po::value<std::vector<std::string>>()->multitoken(), 
         "Compare two versions: --compare v1.1 v1.0");
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }
    
    ArenaRunner arena(vm["config"].as<std::string>());
    
    if (vm.count("compare")) {
        auto versions = vm["compare"].as<std::vector<std::string>>();
        arena.compareVersions(versions[0], versions[1]);
    } else if (vm.count("testsuites")) {
        arena.runTestSuitesOnly();
    } else if (vm.count("matches")) {
        arena.runMatchesOnly();
    } else {
        arena.runAll();
    }
    
    return 0;
}
```

---

## Configuration File

**File:** `config/arena.yaml`

```yaml
version: "v1.1"
resultsDir: "./results"

# Cutechess-cli path (Windows)
cutechessPath: "D:/Games/Cute Chess/cutechess-cli.exe"

# Test suite configurations
testSuites:
  - name: "franky_tests"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 5000  # milliseconds
    maxDepth: 30
  
  - name: "WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
  
  - name: "STS"
    epdPath: "test/testsets/STS1-STS15_LAN.EPD"
    timePerMove: 5000
    maxDepth: 30
  
  - name: "mate_test"
    epdPath: "test/testsets/mate_test_suite.epd"
    timePerMove: 15000
    maxDepth: 30

# Match configurations
matches:
  - name: "v1.1_vs_v1.0_rapid"
    engine1Path: "./cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    engine2Path: "./Release/FrankyCPP_v1.0/FrankyCPP_v1.0.exe"
    openingBook: "books/8moves_GM_LB.pgn"
    timeControl: "10+0.1"  # 10s + 0.1s increment
    rounds: 100
    outputPgn: "results/matches/v1.1_vs_v1.0_rapid.pgn"
  
  - name: "v1.1_vs_v1.0_classical"
    engine1Path: "./cmake-build-win-release/src/FrankyCPP_v1.1.exe"
    engine2Path: "./Release/FrankyCPP_v1.0/FrankyCPP_v1.0.exe"
    openingBook: "books/8moves_GM_LB.pgn"
    timeControl: "60+0.6"  # 1 minute + 0.6s increment
    rounds: 100
    outputPgn: "results/matches/v1.1_vs_v1.0_classical.pgn"
```

---

## CMake Integration

**Changes to `src/CMakeLists.txt`:**

```cmake
# Engine Arena executable
add_executable(${PROJECT_NAME}_Arena
    engine_arena_main.cpp
    engine_arena/ArenaRunner.cpp
    engine_arena/TestSuiteRunner.cpp
    engine_arena/MatchRunner.cpp
    engine_arena/ResultWriter.cpp
)

target_link_libraries(${PROJECT_NAME}_Arena PRIVATE
    FrankyCPP_LIB
    yaml-cpp::yaml-cpp
    Boost::program_options
    spdlog::spdlog_header_only
)

target_include_directories(${PROJECT_NAME}_Arena PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

# Install arena executable
install(TARGETS ${PROJECT_NAME}_Arena
    RUNTIME DESTINATION bin
)
```

---

## Usage Examples

### 1. Run All Tests (from project root)

```powershell
# Build
cmake --build cmake-build-win-release --config Release

# Run arena
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe

# Or specify config
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --config config/arena.yaml
```

### 2. Run Test Suites Only

```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --testsuites
```

### 3. Run Matches Only

```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --matches
```

### 4. Compare Two Versions

```powershell
.\cmake-build-win-release\src\FrankyCPP_v1.1_Arena.exe --compare v1.1 v1.0
```

**Output:**
```
=================================================================
Test Suite Comparison: v1.1 vs v1.0
=================================================================
Suite: WAC
  v1.0: 250/300 (83.3%)
  v1.1: 285/300 (95.0%)
  Improvement: +35 positions (+11.7%)

Suite: STS
  v1.0: 900/1500 (60.0%)
  v1.1: 1125/1500 (75.0%)
  Improvement: +225 positions (+15.0%)

Match: v1.1 vs v1.0 (rapid)
  v1.1: 65 wins, 20 draws, 15 losses (75 points)
  v1.0: 15 wins, 20 draws, 65 losses (25 points)
  ELO Difference: +174 ELO
  
Overall: v1.1 is approximately +170 ELO stronger than v1.0
=================================================================
```

### 5. From CLion

- **Run Configuration:** Create "Arena" run config pointing to the executable
- **Arguments:** `--testsuites` or `--matches`
- **Working Directory:** Project root (for relative paths)

---

## Result File Organization

```
results/
├── testsuites/
│   ├── v1.1_franky_tests_20260201_143022.json
│   ├── v1.1_wac_20260201_144530.json
│   ├── v1.0_wac_20260115_091245.json
│   └── ...
├── matches/
│   ├── v1.1_vs_v1.0_rapid_20260201_150033.json
│   ├── v1.1_vs_v1.0_rapid_20260201_150033.pgn
│   └── ...
└── comparisons/
    └── v1.1_vs_v1.0_20260201_153000.txt
```

**Naming Convention:**
- `{version}_{suite/match_name}_{timestamp}.{ext}`
- Timestamp: `YYYYMMDD_HHMMSS` for sortability

---

## Implementation Phases

### Phase 1: Core Infrastructure (2-3 hours)
1. Create `src/engine_arena/` directory structure
2. Implement `ArenaConfig.h` with YAML loading
3. Implement `ResultWriter` with JSON output
4. Create basic `engine_arena_main.cpp` with arg parsing
5. Update CMakeLists.txt to build Arena executable

**Validation:** Arena executable builds and loads config

### Phase 2: Test Suite Integration (2-3 hours)
1. Implement `TestSuiteRunner` (wraps existing `TestSuite`)
2. Add metadata capture (version, timestamp, system info)
3. Integrate with `ResultWriter` for JSON output
4. Test with `franky_tests.epd`

**Validation:** Test suite results saved to JSON with full details

### Phase 3: Match Runner (3-4 hours)
1. Implement `MatchRunner` with cutechess-cli invocation
2. Parse cutechess output for W/D/L counts
3. Calculate ELO difference
4. Save results to JSON + PGN
5. Handle Windows path escaping

**Validation:** Successful match run and result parsing

### Phase 4: Comparison & Reporting (2-3 hours)
1. Implement version comparison logic in `ArenaRunner`
2. Generate text reports comparing results
3. Add summary statistics (ELO changes, position improvements)
4. Test with real v1.1 vs v1.0 data

**Validation:** Meaningful comparison reports generated

### Phase 5: Documentation & Testing (1-2 hours)
1. Create `docs/arena/` folder with arena documentation:
   - `README.md` - Quick start and overview
   - `Configuration.md` - Detailed config reference  
   - `Results.md` - Result format and comparison
   - `Development.md` - Extension guide
2. Create example `arena.yaml` config
3. Test all command-line modes
4. Verify CLion integration
5. Update main project README.md and docs/INDEX.md with arena references

**Validation:** Complete documentation in `docs/arena/`, all modes tested

**Note:** Keep arena docs organized in `docs/arena/` - do not scatter files in `docs/` root.

**Total Estimated Time:** 10-15 hours

---

## Design Principles (Per Copilot Instructions)

### ✅ Keep It Simple
- **Single responsibility:** Each class does one thing
- **No over-engineering:** Direct file I/O, simple JSON structure
- **Reuse existing code:** Wraps `TestSuite`, doesn't reimplement

### ✅ Strictly Separated
- **Location:** `src/engine_arena/` is isolated
- **No mixing:** Core engine code unchanged
- **Clean interfaces:** Uses public APIs only

### ✅ Configurable
- **YAML config:** All parameters in one file
- **CLI args:** Override behavior without code changes
- **Flexible paths:** Works on different machines

### ✅ Simple to Run
- **Inside CLion:** Create run configuration, click play
- **Outside CLion:** One executable, clear arguments
- **Automated:** Can be scripted for CI/CD

### ✅ Adheres to Conventions
- **Naming:** `PascalCase` classes, `camelCase` methods
- **Headers:** `FRANKYCPP_ENGINE_ARENA_*` guards
- **Logging:** Uses existing `spdlog` infrastructure
- **Error handling:** Exceptions for config errors, return codes for execution

---

## Future Enhancements (Out of Scope for v1)

1. **Web Dashboard:** HTML report generation with charts
2. **Automated Regression Testing:** Git hook to run on commit
3. **Parameter Tuning Integration:** Feed arena results to tuner
4. **Multi-Engine Gauntlet:** Test against multiple external engines
5. **Result Database:** SQLite storage for long-term analysis
6. **Statistical Significance:** SPRT (Sequential Probability Ratio Test)
7. **Email Notifications:** Alert on completion of long matches

---

## Open Questions

1. **cutechess-cli availability:** Should we bundle it or require user install?
   - **Recommendation:** Document installation, detect at runtime, fail gracefully

2. **Result retention:** How many old results to keep?
   - **Recommendation:** Keep all, add cleanup script for users

3. **Parallel execution:** Run multiple test suites concurrently?
   - **Recommendation:** v1 = sequential, add threading in v2 if needed

4. **CI/CD integration:** Auto-run on every commit?
   - **Recommendation:** Manual trigger initially, automate later

---

## Success Criteria

After implementation, users should be able to:

1. ✅ Run `FrankyCPP_v1.1_Arena.exe` without arguments and execute all configured tests
2. ✅ Compare two versions with `--compare v1.1 v1.0` and see ELO difference
3. ✅ Find detailed JSON results in `results/` directory
4. ✅ Modify `config/arena.yaml` to add new test suites or matches
5. ✅ Run arena from CLion with a single click
6. ✅ Determine if a code change improved/degraded engine strength

---

## Summary

This framework provides a **simple, maintainable, and effective** solution for tracking engine strength across versions. It:

- **Reuses existing code** (TestSuite class)
- **Stays separated** (dedicated directory, no core changes)
- **Outputs structured data** (JSON for automation)
- **Integrates cutechess-cli** (industry-standard tool)
- **Scales gracefully** (easy to add new suites/matches)
- **Supports workflow** (works in IDE and command line)

**Estimated implementation time:** 10-15 hours for a fully functional v1.0

This approach balances simplicity with completeness, providing immediate value without over-engineering.
