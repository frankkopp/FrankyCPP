# Engine Arena Development Guide

Guide for extending and customizing the Engine Arena framework.

---

## Architecture Overview

### Component Hierarchy

```
engine_arena_main.cpp
    ↓
ArenaRunner (Orchestrator)
    ↓
    ├─→ TestSuiteRunner → UCIEngine (external UCI engine via subprocess)
    ├─→ MatchRunner → cutechess-cli subprocess
    └─→ ResultWriter → JSON files
```

### Design Principles

1. **Separation of Concerns**
   - Each class has a single, well-defined responsibility
   - Arena code is isolated in `src/engine_arena/`
   - No modifications to core engine code

2. **Configurability**
   - All parameters in YAML configuration
   - No hardcoded paths or values
   - Easy to add new test suites or matches

3. **Extensibility**
   - Clear interfaces for adding new result types
   - Result format is JSON (easy to parse and extend)
   - Pluggable comparison logic

4. **Testability**
   - Pure functions where possible
   - Dependency injection for testing
   - Validation separated from execution

---

## Class Responsibilities

### ArenaRunner

**Purpose:** Main orchestrator that coordinates all arena operations

**File:** `src/engine_arena/ArenaRunner.h/cpp`

**Key Methods:**
- `runAll()` - Run all configured tests and matches
- `runTestSuitesOnly()` - Run only EPD test suites
- `runMatchesOnly()` - Run only engine matches
- `compareVersions(v1, v2)` - Generate comparison report

**Dependencies:**
- `TestSuiteRunner` - For running EPD tests
- `MatchRunner` - For running engine matches
- `ResultWriter` - For saving results

**Extension Points:**
- Add new comparison metrics
- Add new report formats (HTML, CSV)
- Add statistical analysis (confidence intervals, SPRT)

---

### TestSuiteRunner

**Purpose:** Execute EPD test suites against external UCI engines and capture detailed results

**File:** `src/engine_arena/TestSuiteRunner.h/cpp`

**Key Methods:**
- `runTestSuite(config)` - Run a single test suite
- `runAllTestSuites()` - Run all configured test suites
- `runTestSuiteParallel(config, numWorkers)` - Run with parallel workers

**Dependencies:**
- `UCIEngine` - External engine communication via UCI protocol
- `EpdParser` - Parse EPD test files

**Features:**
- Tests any UCI-compatible engine (FrankyCPP, Stockfish, etc.)
- Parallel execution with configurable worker count
- Position isolation via `ucinewgame` between tests
- Configurable UCI options and command-line arguments

**Extension Points:**
- Add custom test operations beyond bm/am/dm
- Add timing breakdowns (move generation, evaluation, etc.)
- Capture search statistics (hash hits, pruning counts)


---

### MatchRunner

**Purpose:** Execute engine matches via cutechess-cli and parse results

**File:** `src/engine_arena/MatchRunner.h/cpp`

**Key Methods:**
- `runMatch(config)` - Run a single match (with batch-based resumption)
- `runAllMatches()` - Run all configured matches
- `parseOutput(output, config)` - Parse cutechess-cli output
- `calculateEloDifference(score, games)` - ELO calculation
- `getStateFilePath(config)` - Get path to state file for a match
- `loadMatchState(path, state)` - Load saved match state
- `saveMatchState(path, state)` - Save current match state
- `deleteMatchState(path)` - Delete state file (on completion)

**State Persistence:**
- Matches run in batches of `batchSize` games (configurable, default: auto)
- State saved to `results/matches/.state/<match>.state.json` after each batch
- On resume, loads state and continues from last completed batch
- State file auto-deleted when match completes

**Data Structures:**
```cpp
struct MatchState {
  std::string matchName;
  int totalRounds;
  int completedRounds;
  int engine1Wins;
  int engine2Wins;
  int draws;
  std::string engine1Name;
  std::string engine2Name;
  std::string timestamp;
};
```

**Dependencies:**
- cutechess-cli executable (external)
- Engine executables (external or built-in)

**Extension Points:**
- Add support for other match managers (Fast Chess, bayeselo)
- Add opening suite support (not just PGN books)
- Add adjudication rules (draw by repetition, tablebase adjudication)
- Parse additional cutechess output (per-game times, opening statistics)
- Add SPRT (Sequential Probability Ratio Test) for early termination

---

### ResultWriter

**Purpose:** Persist results to JSON files

**File:** `src/engine_arena/ResultWriter.h/cpp`

**Key Methods:**
- `writeTestSuiteResult(result)` - Write test suite JSON
- `writeMatchResult(result)` - Write match JSON
- `generateFilename(prefix, name, version)` - Create timestamped filename

**Current Format:** Manually constructed JSON strings

**Extension Points:**
- Add HTML report generation
- Add CSV export for spreadsheet analysis
- Add database persistence (SQLite)
- Add result compression for large test suites
- Use JSON library for cleaner code (currently manual strings)

---

### ArenaConfig

**Purpose:** Load and validate YAML configuration

**File:** `src/engine_arena/ArenaConfig.h/cpp`

**Key Methods:**
- `loadFromYaml(path)` - Parse YAML file
- `validate()` - Check all paths exist and values are valid

**Dependencies:**
- yaml-cpp library

**Extension Points:**
- Add configuration profiles (quick, standard, thorough)
- Add environment variable expansion
- Add configuration inheritance/includes
- Add validation rules (e.g., minimum rounds for statistical significance)

---

## Adding New Features

### Add a New Test Suite Type

**Example:** Add performance benchmark tests

1. **Define result structure** in `ArenaResults.h`:
```cpp
struct BenchmarkResult {
  std::string version;
  std::string benchmarkName;
  std::string timestamp;
  int64_t nodesPerSecond;
  int64_t totalNodes;
  int64_t timeMs;
};
```

2. **Add configuration** in `ArenaConfig.h`:
```cpp
struct BenchmarkConfig {
  std::string name;
  int depth;
  int positions;
};
```

3. **Create runner** in `BenchmarkRunner.h/cpp`:
```cpp
class BenchmarkRunner {
public:
  BenchmarkResult runBenchmark(const BenchmarkConfig& config);
};
```

4. **Integrate** into `ArenaRunner`:
```cpp
void ArenaRunner::runAll() {
  // ...existing test suites...
  // ...existing matches...
  
  if (!arenaConfig.benchmarks.empty()) {
    BenchmarkRunner benchRunner(arenaConfig);
    auto results = benchRunner.runAllBenchmarks();
    // Save results...
  }
}
```

5. **Update YAML** schema:
```yaml
benchmarks:
  - name: "standard_bench"
    depth: 20
    positions: 100
```

---

### Add a New Comparison Metric

**Example:** Add average node count comparison

1. **Update comparison report generation** in `ArenaRunner.cpp`:
```cpp
std::string ArenaRunner::generateComparisonReport(...) {
  // ...existing code...
  
  // Add node efficiency comparison
  if (!suites1.empty() && !suites2.empty()) {
    report << "\nNODE EFFICIENCY COMPARISON:\n";
    report << "-------------------------------------------------------------------\n";
    
    for (const auto& [name, suite1] : suites1) {
      auto it2 = suites2.find(name);
      if (it2 != suites2.end()) {
        const auto& suite2 = it2->second;
        
        double avgNodes1 = suite1.totalNodes / static_cast<double>(suite1.totalTests);
        double avgNodes2 = suite2.totalNodes / static_cast<double>(suite2.totalTests);
        double nodeDelta = avgNodes1 - avgNodes2;
        
        report << name << ":\n";
        report << "  " << version2 << ": " << avgNodes2 << " nodes/test\n";
        report << "  " << version1 << ": " << avgNodes1 << " nodes/test\n";
        report << "  Delta: " << (nodeDelta > 0 ? "+" : "") << nodeDelta << " nodes\n";
      }
    }
  }
  
  return report.str();
}
```

---

### Add HTML Report Generation

**Example:** Create HTML version of comparison report

1. **Add method** in `ResultWriter`:
```cpp
class ResultWriter {
public:
  // ...existing methods...
  
  std::string writeComparisonHtml(
      const std::string& version1,
      const std::string& version2,
      const std::string& report);
};
```

2. **Implement** in `ResultWriter.cpp`:
```cpp
std::string ResultWriter::writeComparisonHtml(...) {
  std::ostringstream html;
  
  html << "<!DOCTYPE html>\n";
  html << "<html><head><title>Comparison: " << version1 << " vs " << version2 << "</title>\n";
  html << "<style>body { font-family: monospace; }</style>\n";
  html << "</head><body>\n";
  html << "<h1>Version Comparison: " << version1 << " vs " << version2 << "</h1>\n";
  
  // Convert text report to HTML (add styling, tables, charts)
  // ...
  
  html << "</body></html>\n";
  
  // Save to file
  std::filesystem::path htmlPath = comparisonsDir / (filename + ".html");
  std::ofstream file(htmlPath);
  file << html.str();
  file.close();
  
  return htmlPath.string();
}
```

3. **Call** from `ArenaRunner::compareVersions()`:
```cpp
// After generating text report
std::string htmlPath = resultWriter.writeComparisonHtml(version1, version2, report);
std::cout << "HTML report saved to: " << htmlPath << std::endl;
```

---

### Add External Engine Test Suite Support

**See:** `docs/specs/External_Engine_TestSuite_Support_Outline.md` for complete design

**Summary:**
1. Create `UCIEngine` class for UCI communication
2. Create `EPDParser` for standalone EPD parsing
3. Add `useExternalEngine` flag to `TestSuiteConfig`
4. Implement `TestSuiteRunner::runTestSuiteExternal()`

**Effort:** ~10-14 hours, ~1250 lines of code

---

## Testing Arena Code

### Unit Testing

**Location:** `test/engine_arena/`

**Framework:** Google Test

**Example Test:**
```cpp
#include <gtest/gtest.h>
#include "engine_arena/ArenaConfig.h"

TEST(ArenaConfigTest, LoadValidConfig) {
  ArenaConfig config = ArenaConfig::loadFromYaml("test/fixtures/valid_arena.yaml");
  
  EXPECT_EQ(config.version, "v1.1");
  EXPECT_EQ(config.testSuites.size(), 2);
  EXPECT_EQ(config.matches.size(), 1);
}

TEST(ArenaConfigTest, ValidateFailsWithMissingFile) {
  ArenaConfig config;
  config.testSuites.push_back({
    "test", "nonexistent.epd", std::chrono::milliseconds(5000), 30
  });
  
  EXPECT_FALSE(config.validate());
}
```

### Integration Testing

**Approach:** Create minimal test configurations

**Example:** `test/fixtures/arena_integration.yaml`
```yaml
version: "test"
resultsDir: "./test/temp_results"

testSuites:
  - name: "mini_test"
    epdPath: "test/fixtures/mini.epd"  # 5 positions
    timePerMove: 100
    maxDepth: 10
```

**Test:**
```cpp
TEST(ArenaIntegrationTest, RunMiniTestSuite) {
  ArenaConfig config = ArenaConfig::loadFromYaml("test/fixtures/arena_integration.yaml");
  TestSuiteRunner runner(config);
  
  auto result = runner.runTestSuite(config.testSuites[0]);
  
  EXPECT_EQ(result.totalTests, 5);
  EXPECT_GE(result.passed, 3);  // At least 3 should pass
}
```

---

## Code Style Guidelines

### Naming Conventions

**Classes:** `PascalCase`
```cpp
class ArenaRunner { };
class TestSuiteRunner { };
```

**Methods:** `camelCase`
```cpp
void runAll();
TestSuiteResult runTestSuite();
```

**Variables:** `camelCase`
```cpp
int totalTests = 0;
std::string version = "v1.1";
```

**Constants:** `UPPER_SNAKE_CASE`
```cpp
const int MAX_DEPTH = 100;
const milliseconds DEFAULT_TIME = milliseconds(5000);
```

### Header Guards

Use `FRANKYCPP_ENGINE_ARENA_*` pattern:
```cpp
#ifndef FRANKYCPP_ENGINE_ARENA_ARENARUNNER_H
#define FRANKYCPP_ENGINE_ARENA_ARENARUNNER_H
// ...
#endif // FRANKYCPP_ENGINE_ARENA_ARENARUNNER_H
```

### Documentation

**Class Documentation:**
```cpp
/// Executes EPD test suites and captures results with metadata
///
/// TestSuiteRunner wraps the existing TestSuite class to execute EPD test
/// suites and capture results with metadata for version comparison.
///
/// Example:
/// @code
///   TestSuiteRunner runner(config);
///   TestSuiteResult result = runner.runTestSuite(config.testSuites[0]);
///   std::cout << "Passed: " << result.passed << std::endl;
/// @endcode
class TestSuiteRunner {
  // ...
};
```

**Method Documentation:**
```cpp
/// Runs a single test suite and returns detailed results
///
/// @param suiteConfig Test suite configuration
/// @return TestSuiteResult with full metadata and per-test details
/// @throws std::runtime_error if EPD file not found or execution fails
TestSuiteResult runTestSuite(const TestSuiteConfig& suiteConfig);
```

### Error Handling

**Use exceptions for configuration errors:**
```cpp
if (!std::filesystem::exists(epdPath)) {
  throw std::runtime_error("EPD file not found: " + epdPath);
}
```

**Use return codes for execution failures:**
```cpp
bool executeCutechess(const std::string& command, std::string& output) {
  // ... attempt execution ...
  return success;  // true on success, false on failure
}
```

**Provide detailed error messages:**
```cpp
// ❌ Bad
throw std::runtime_error("Config error");

// ✅ Good
throw std::runtime_error(
  "Configuration validation failed: EPD file not found at '" + 
  epdPath + "'. Ensure you are running from the project root directory."
);
```

---

## Performance Considerations

### Test Suite Execution

**Current:** Sequential execution (one test at a time)

**Optimization Opportunities:**
- Parallel test execution (ThreadPool)
- Batch position loading
- Shared transposition table

**Trade-offs:**
- Parallel = faster but non-deterministic
- Sequential = slower but reproducible

### Match Execution

**Current:** Uses cutechess-cli's concurrency setting

**Optimization:**
- Already parallelized via cutechess-cli
- `concurrency: 4` runs 4 games simultaneously

### Result Writing

**Current:** Synchronous file I/O

**Optimization Opportunities:**
- Asynchronous writes
- Buffered I/O
- Compression for large result files

**Impact:** Minimal (I/O is fast compared to search time)

---

## Future Enhancements

### Short-term (1-2 hours each)

1. **Add CSV export**
   - Extend `ResultWriter` with `writeCsv()` methods
   - Useful for spreadsheet analysis

2. **Add progress bar**
   - Show progress during test suite execution
   - Use simple text-based progress (e.g., [====>    ] 50%)

3. **Add result filtering**
   - `--cmp FrankyCPP-v1.1 --baseline FrankyCPP-v1.0 --suite WAC` (compare specific suite)
   - `--cmp FrankyCPP-v1.1 --baseline FrankyCPP-v1.0 --matches-only` (skip test suites)

4. **Add configuration validation**
   - Warn if time control is too short
   - Warn if rounds < 100 (statistically weak)

### Medium-term (4-8 hours each)

1. **HTML report generation**
   - Visual comparison with charts
   - Clickable test details
   - Embedded PGN viewer

2. **Statistical significance testing**
   - Calculate confidence intervals for ELO
   - Implement SPRT for early match termination
   - Add Bayesian ELO estimation

3. **Result database**
   - SQLite storage for historical data
   - Query API for trending analysis
   - Automatic schema migrations

4. **CI/CD integration**
   - GitHub Actions workflow
   - Auto-run on PRs
   - Comment results on PR

### Long-term (10+ hours each)

1. **External engine test suite support**
   - See `External_Engine_TestSuite_Support_Outline.md`
   - ~10-14 hours, ~1250 lines

2. **Web dashboard**
   - Real-time progress monitoring
   - Historical trend visualization
   - Interactive game analysis

3. **Automated regression detection**
   - Git bisect integration
   - Automatic issue creation on regression
   - Blame analysis (which commit caused regression)

4. **Parameter tuning integration**
   - Feed arena results to SPSA/genetic tuner
   - Automated parameter optimization
   - A/B testing framework

---

## Contributing Guidelines

### Before Adding Features

1. **Check existing design docs**
   - `docs/specs/Engine_Arena_Implementation_Plan.md`
   - Feature may already be designed

2. **Discuss major changes**
   - Open GitHub issue for discussion
   - Get feedback on approach

3. **Write design doc**
   - For features > 4 hours effort
   - Include architecture, effort estimate, alternatives

### Pull Request Checklist

- [ ] Code follows project style guidelines
- [ ] All classes have proper documentation
- [ ] Unit tests added for new functionality
- [ ] Integration tests added (if applicable)
- [ ] Configuration examples updated
- [ ] Documentation updated (README, config guide, etc.)
- [ ] No changes to core engine code (unless necessary)
- [ ] Code compiles without warnings
- [ ] All tests pass

### Code Review Focus

**Reviewers should check:**
1. **Separation of concerns** - Is arena code isolated?
2. **Configuration** - Are values configurable (not hardcoded)?
3. **Error handling** - Are errors clear and actionable?
4. **Testing** - Is the code testable and tested?
5. **Documentation** - Can users understand how to use it?

---

## Getting Help

**Questions about architecture:**
- Read `docs/specs/Engine_Arena_Implementation_Plan.md`
- Check class documentation in header files

**Questions about implementation:**
- Look at existing code in `src/engine_arena/`
- Patterns are consistent across classes

**Questions about integration:**
- Check how `ArenaRunner` coordinates components
- Follow data flow from config → runner → results

**Questions about design decisions:**
- See "Design Principles" in implementation plan
- Check git commit messages for rationale

---

## Useful Resources

### Chess Programming

- **Chess Programming Wiki:** https://www.chessprogramming.org/
- **UCI Protocol:** https://www.chessprogramming.org/UCI
- **EPD Format:** https://www.chessprogramming.org/Extended_Position_Description

### Tools

- **cutechess-cli:** https://github.com/cutechess/cutechess
- **pgn-extract:** https://www.cs.kent.ac.uk/people/staff/djb/pgn-extract/
- **python-chess:** https://python-chess.readthedocs.io/

### Libraries

- **yaml-cpp:** https://github.com/jbeder/yaml-cpp
- **nlohmann/json:** https://github.com/nlohmann/json
- **spdlog:** https://github.com/gabime/spdlog

---

*Last updated: 2026-02-07*
