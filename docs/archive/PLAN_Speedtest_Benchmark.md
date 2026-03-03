# FrankyCPP Speedtest Benchmark Plan

**Document Version:** 1.5    
**Created:** 2026-02-08  
**Last Updated:** 2026-02-08  
**Status:** Phase 1 Complete ✅  
**Target:** FrankyCPP v1.2 (Phase 1), v1.3+ (Phase 2 optional)  
**Priority:** HIGH (needed to measure v1.2 performance improvements)

---

## Executive Summary

Implement standardized benchmark features for FrankyCPP to provide comparable NPS (nodes per second) metrics between versions, measuring the performance impact of code changes.

**Phased Approach:**
1. **Phase 1: `bench` command** - Simple, fixed-depth benchmark with full Arena integration
2. **Phase 2: `speedtest` command** (optional) - Comprehensive game-based benchmark (only if needed)

This plan starts with the simpler `bench` command, which provides immediate value with less complexity. After `bench` is fully integrated (including Arena), we can evaluate whether the more complex `speedtest` is needed.

**Note:** Stockfish has both commands:
- `bench` - Simple benchmark with fixed depth/time per position
- `speedtest` - Comprehensive benchmark simulating realistic game play

We implement `bench` first as it covers most use cases. `speedtest` remains an optional future enhancement.

---

## Phase 1 Implementation Summary ✅ COMPLETE

**Completed: 2026-02-08**

### What Was Implemented

#### UCI `bench` Command
- **Command:** `bench [depth] [hash] [threads]` (default: `bench 10 128 1`)
- **Positions:** 50 curated positions from WAC, Kaufman, Eigenmann, and standard positions
- **Files:** `src/engine/Benchmark.h`, `Benchmark.cpp`, `BenchmarkPositions.h`
- **Output:** Total nodes, time, NPS to stderr (UCI-compatible)

#### Command-Line Interface
- **Command:** `FrankyCPP --bench [--benchDepth N] [--benchHash MB]`
- **Shortcut:** `-b` equivalent to `--bench`

#### Arena Integration
- **Command:** `FrankyCPP_Arena --bench` or `-b`
- **Report:** `FrankyCPP_Arena --bench-report`
- **Config:** YAML-based configuration in `arena.yaml`
- **Files:** `BenchmarkRunner.h`, `BenchmarkRunner.cpp`
- **Storage:** `results/benchmarks/benchmarks.json`

#### Key Design Decisions
1. **TT Clearing:** Clears TT before each position for fair, independent measurement
2. **Timing:** Measures only search time (excludes TT clearing overhead)
3. **External Engines:** Supports any UCI engine via standard protocol
4. **Results Table:** Shows timestamp, version, depth, hash, nodes, NPS, time

#### Results Comparison (Internal vs External)
```
Internal: 223,073,620 nodes in 59.35s = 3,758,675 NPS
External: 223,073,620 nodes in 59.94s = 3,721,801 NPS
```
- Identical node counts confirm TT clearing consistency
- ~1% time difference due to minimal UCI overhead

---

## Motivation

### Current State
- No standardized way to compare search speed between versions
- Google Benchmark exists for micro-benchmarks (move gen, do/undo move)
- Arena framework measures tactical test accuracy but not raw NPS
- Cannot easily quantify performance impact of refactoring

### Goals
- **Reproducible NPS measurements** across versions
- **Quick performance regression detection** after changes
- **Version-to-version comparison** (e.g., "v1.2 is 5% faster than v1.1")
- **CI/CD integration** for automated performance tracking

---

## Reference: Stockfish Benchmark Commands

Stockfish provides two benchmark commands with different purposes:

### `bench` - Simple Benchmark
A quick benchmark with fixed depth or time per position.

**Command Format:**
```
bench [ttSize] [threads] [limit] [fenFile] [limitType] [evalType]
```

**Features:**
- Fixed set of ~50 diverse positions (Defaults array)
- Same depth/time limit for each position
- Quick execution (~30 seconds typical)
- Simple output: total nodes, time, NPS

**Use Case:** Quick smoke test, basic performance comparison

---

### `speedtest` - Comprehensive Benchmark (PRIMARY REFERENCE)

A realistic benchmark that simulates actual game play.

**Command Format:**
```
speedtest [threads] [ttSize] [duration_seconds]
```

**How It Works:**
1. **Game-Based Positions:** Uses 5 complete games (~60 moves each) from real play
2. **Time-Scaled Searches:** Earlier moves get more time (simulating real game thinking):
   - Formula: `time_ms = 50000 / (ply + 15)`
   - Move 1: ~3125ms, Move 10: ~2000ms, Move 30: ~1111ms
3. **Duration Scaling:** Positions scaled to fit target duration (default 150 seconds)
4. **Warmup Phase:** First 3 positions warm up caches/JIT
5. **Isolated Games:** `ucinewgame` between games clears TT/history

**Output:**
```
===========================
Version                    : Stockfish 17 (commit hash)
Compiled by                : g++ (Ubuntu 13.2.0) 13.2.0
Large pages                : yes
User invocation            : speedtest 8 1024 150
Filled invocation          : speedtest 8 1024 150
Available processors       : 8
Thread count               : 8
Thread binding             : none
TT size [MiB]              : 1024
Hash max, avg [per mille]  :
    single search          : 234, 156
    single game            : 567, 423
Total nodes searched       : 2,345,678,901
Total search time [s]      : 150.2
Nodes/second               : 15,612,345
===========================
```

**Key Advantages over `bench`:**
- **Realistic time distribution** (like actual games)
- **Hash table behavior** measured across games
- **Thread scaling** properly exercised
- **Longer duration** for more stable measurements
- **Game continuity** tests incremental hash updates

**Use Case:** Comprehensive performance comparison between versions

---

## Phase 1: `bench` Command with Arena Integration

### 1.1 UCI `bench` Command (in-engine)

#### 1.1.1 Benchmark Positions

Use a curated mix from existing public domain test sets in `test/testsets/`:

| Source             | Count  | Purpose                      |
|--------------------|--------|------------------------------|
| WAC (Win at Chess) | 20     | Tactical positions           |
| Kaufman            | 10     | Balanced tactical/positional |
| Eigenmann Rapid    | 15     | Modern diverse positions     |
| Standard positions | 5      | Start pos, Kiwi, endgames    |
| **Total**          | **50** |                              |

**Advantages of using existing testsets:**
- **No licensing concerns** - well-known public domain positions
- **Already validated** - known to work with FrankyCPP
- **Diverse coverage** - tactical, positional, endgame
- **Documented provenance** - traceable sources

Store in: `src/engine/BenchmarkPositions.h`

```cpp
namespace benchmark {

// Curated benchmark positions from public domain test sets
// Sources: WAC, Kaufman, Eigenmann Rapid, standard positions
constexpr std::array<std::string_view, 50> BENCH_FENS = {
  // Standard positions
  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  // Start pos
  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",  // Kiwi
  "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",  // Endgame
  "8/PPP4k/8/8/8/8/4Kppp/8 w - - 0 1",  // Promotion race
  "8/8/8/8/8/k7/8/K1R5 w - - 0 1",  // Simple endgame
  
  // WAC positions (20) - tactical
  "5rk1/1ppb3p/p1pb4/6q1/3P1p1r/2P1R2P/PP1BQ1P1/5RKN w - - 0 1",  // WAC.003
  "r1bq2rk/pp3pbp/2p1p1pQ/7P/3P4/2PB1N2/PP3PPR/2KR4 w - - 0 1",  // WAC.004
  // ... (selected from wac.epd)
  
  // Kaufman positions (10) - balanced
  "1rbq1rk1/p1b1nppp/1p2p3/8/1B1pN3/P2B4/1P3PPP/2RQ1R1K w - - 0 1",  // Kaufman 1
  "3r2k1/p2r1p1p/1p2p1p1/q4n2/3P4/PQ5P/1P1RNPP1/3R2K1 b - - 0 1",  // Kaufman 2
  // ... (selected from kaufman.epd)
  
  // Eigenmann positions (15) - diverse modern
  "r1bqk1r1/1p1p1n2/p1n2pN1/2p1b2Q/2P1Pp2/1PN5/PB4PP/R4RK1 w q - 0 1",  // ERET 001
  "r1n2N1k/2n2K1p/3pp3/5Pp1/b5R1/8/1PPP4/8 w - - 0 1",  // ERET 002 002
  // ... (selected from eigenmann-rapid-engine.epd)
};

} // namespace benchmark
```

**Position Selection Criteria:**
- Mix of opening, middlegame, and endgame
- Variety of piece configurations
- Different branching factors (tactical vs quiet)
- Avoid positions that are too quick to solve (< depth 5)
- Avoid positions that are too slow (> 30s at depth 13)

#### 1.1.2 Benchmark Class

New files: `src/engine/Benchmark.h` / `src/engine/Benchmark.cpp`

```cpp
namespace engine {

struct BenchConfig {
  int hashSizeMB = 128;        // TT size
  int threads = 1;             // Number of threads (future SMP)
  Depth depth = 13;            // Search depth limit
  milliseconds timeLimit{0};   // Time limit per position (0 = use depth)
};

struct BenchResult {
  uint64_t totalNodes = 0;
  milliseconds totalTime{0};
  double nps = 0.0;
  int positionsRun = 0;
  std::string version;
};

class Benchmark {
public:
  /// Run the standard benchmark with default positions
  static BenchResult runBench(const BenchConfig& config = {});
  
  /// Run benchmark with custom positions (for testing)
  static BenchResult runBench(const std::vector<std::string>& fens,
                              const BenchConfig& config = {});
  
  /// Print results to stderr (UCI-compatible, like Stockfish)
  static void printBenchResults(const BenchResult& result);
};

} // namespace engine
```

#### 1.1.3 UCI Integration

Add to `UciHandler.cpp`:

```cpp
void UciHandler::handleBench(std::istringstream& args) {
  BenchConfig config;
  
  // Parse: bench [depth] [hash] [threads]
  int depth, hash, threads;
  if (args >> depth) config.depth = depth;
  if (args >> hash) config.hashSizeMB = hash;
  if (args >> threads) config.threads = threads;
  
  auto result = Benchmark::runBench(config);
  Benchmark::printBenchResults(result);
}
```

#### 1.1.4 Output Format

```
===========================
FrankyCPP Bench Results
===========================
Version            : FrankyCPP v1.2
Hash size [MB]     : 128
Threads            : 1
Depth limit        : 13
Positions          : 50
---------------------------
Total nodes        : 45,123,456
Total time [s]     : 28.5
Nodes/second       : 1,583,279
===========================
```

---

### 1.2 Arena Framework Integration for `bench`

#### 1.2.1 New Config Structure

Add to `ArenaConfig.h`:

```cpp
/// Configuration for bench benchmark
struct BenchConfig {
  std::string name;             ///< Bench name (e.g., "bench_v1.2")
  std::string enginePath;       ///< Path to engine executable
  std::string engineVersion;    ///< Engine version for results
  int hashSizeMB = 128;         ///< TT size for benchmark
  Depth depth = 13;             ///< Search depth limit
  int runs = 3;                 ///< Number of runs (for averaging/stddev)
  std::string commandLineArgs;  ///< Command-line args (e.g., "--nobook")
  std::string uciOptions;       ///< UCI options to set before bench
};
```

Add to `ArenaConfig`:
```cpp
struct ArenaConfig {
  // ... existing fields ...
  std::vector<BenchConfig> benchmarks;  ///< Benchmark configurations
};
```

#### 1.2.2 BenchRunner

New file: `src/engine_arena/BenchRunner.h` / `.cpp`

```cpp
namespace arena {

struct BenchResult {
  std::string suiteName;
  std::string engineVersion;
  std::string enginePath;
  std::chrono::system_clock::time_point timestamp;
  
  // Benchmark metrics
  uint64_t totalNodes = 0;
  double totalTimeSeconds = 0.0;
  double nps = 0.0;
  int positionsRun = 0;
  
  // Multi-run statistics (if runs > 1)
  double npsMin = 0.0;
  double npsMax = 0.0;
  double npsAvg = 0.0;
  double npsStdDev = 0.0;
  std::vector<double> npsRuns;  // Individual run NPS values
};

class BenchRunner {
public:
  explicit BenchRunner(const BenchConfig& config);
  
  /// Run the benchmark and return results
  BenchResult run() const;
  
private:
  BenchConfig config_;
  
  /// Execute single benchmark run via UCI `bench` command
  double runSingleBenchmark() const;
  
  /// Parse NPS from engine's bench output
  double parseNpsFromOutput(const std::string& output) const;
};

} // namespace arena
```

#### 1.2.3 YAML Configuration

Example `arena.yaml` addition:

```yaml
# Benchmark configurations
benchmarks:
  - name: "bench_v1.2"
    enginePath: "Release/FrankyCPP_v1.2/FrankyCPP_v1.2.exe"
    engineVersion: "v1.2"
    hashSizeMB: 128
    depth: 13
    runs: 3
    commandLineArgs: "--nobook"
    uciOptions: ""

  - name: "bench_v1.1"
    enginePath: "Release/FrankyCPP_v1.1/FrankyCPP_v1.1.exe"
    engineVersion: "v1.1"
    hashSizeMB: 128
    depth: 13
    runs: 3
    commandLineArgs: "--nobook"
    uciOptions: ""
```

#### 1.2.4 Results Storage

Add to `ArenaResults.h`:

```cpp
/// Bench result for JSON storage
struct BenchResultData {
  std::string suiteName;
  std::string engineVersion;
  std::string timestamp;
  uint64_t totalNodes;
  double totalTimeSeconds;
  double nps;
  double npsMin;
  double npsMax;
  double npsAvg;
  double npsStdDev;
  std::vector<double> npsRuns;
};
```

JSON output format:
```json
{
  "suiteName": "bench_v1.2",
  "engineVersion": "v1.2",
  "timestamp": "2026-02-08T14:30:00",
  "totalNodes": 45123456,
  "totalTimeSeconds": 28.5,
  "nps": 1583279.0,
  "npsMin": 1550000.0,
  "npsMax": 1610000.0,
  "npsAvg": 1583279.0,
  "npsStdDev": 25000.0,
  "npsRuns": [1550000.0, 1590000.0, 1610000.0]
}
```

#### 1.2.5 ArenaRunner Integration

Add methods to `ArenaRunner`:

```cpp
class ArenaRunner {
public:
  // ... existing methods ...
  
  /// Runs only benchmarks
  void runBenchmarksOnly() const;
  
  /// Compare benchmark results between versions
  void compareBenchmarks(const std::string& version1,
                         const std::string& version2) const;
};
```

#### 1.2.6 Comparison Report

Example output (comparing future versions, since v1.1 doesn't have `bench`):
```
=================================================================
Benchmark Comparison: v1.3 vs v1.2
=================================================================
Version     | NPS (avg)    | NPS (std)   | Change
-----------------------------------------------------------------
v1.3        | 1,583,279    | ±25,000     | +5.2% faster
v1.2        | 1,505,123    | ±22,000     | (baseline)
-----------------------------------------------------------------
Improvement: +78,156 NPS (+5.2%)
=================================================================
```

---

### 1.3 CI/CD Integration for `bench`

#### 1.3.1 GitHub Actions Workflow

```yaml
name: Performance Benchmark

on:
  push:
    branches: [main, develop]
  pull_request:

jobs:
  benchmark:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build Release
        run: cmake --build cmake-build-release --config Release
      - name: Run Benchmark
        run: |
          .\cmake-build-release\src\FrankyCPP_v1.2.exe bench 13 128 1
      - name: Archive Results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-results
          path: results/benchmarks/
```

#### 1.3.2 Performance Regression Detection

- Store baseline NPS in repo (e.g., `benchmark_baseline.json`)
- CI compares current NPS against baseline
- Warn if NPS drops > 5%
- Fail if NPS drops > 10%

---

## Phase 2: `speedtest` Command (OPTIONAL - Future)

> **Note:** This phase is optional and should only be implemented if `bench` proves insufficient
> for detecting performance changes. The simpler `bench` command covers most use cases.

### 2.1 When to Consider `speedtest`

Consider implementing `speedtest` if:
- `bench` results are too variable (> 5% variance between runs)
- Need to measure hash table efficiency across game continuity
- Need to test multi-threaded scaling behavior
- Want more realistic time-based benchmarks (vs fixed depth)

### 2.2 `speedtest` Design Overview

If needed, `speedtest` would add:

**UCI Command:** `speedtest [threads] [hash] [duration]`

**Key Differences from `bench`:**
- Uses complete game sequences (~5 games, ~300 positions)
- Time-scaled searches: `time_ms = 50000 / (ply + 15)`
- Warmup phase (first 3 positions)
- Hash utilization tracking per search and per game
- Longer duration (default 150 seconds vs ~30 seconds for bench)

**Implementation would follow similar patterns:**
- `SpeedtestConfig` / `SpeedtestResult` structs
- `Benchmark::runSpeedtest()` method
- `SpeedtestRunner` for Arena integration
- Additional YAML config section

**Estimated Effort:** ~15-20 hours additional

---

## Implementation Tasks

### Phase 1: `bench` Command + Arena Integration
| Task | Effort | Files |
|------|--------|-------|
| **UCI Command** | | |
| Create BenchmarkPositions.h with ~50 positions | 2h | `src/engine/BenchmarkPositions.h` |
| Implement `Benchmark` class with `runBench()` | 3h | `src/engine/Benchmark.h`, `Benchmark.cpp` |
| Add UCI `bench` command handling | 1h | `src/engine/UciHandler.cpp` |
| Unit tests for Benchmark class | 2h | `test/engine/BenchmarkTest.cpp` |
| **Arena Integration** | | |
| Add BenchConfig to ArenaConfig | 1h | `ArenaConfig.h`, `ArenaConfig.cpp` |
| Implement BenchRunner | 3h | `BenchRunner.h`, `BenchRunner.cpp` |
| Add BenchResultData to ArenaResults | 1h | `ArenaResults.h` |
| Update ResultWriter for bench JSON | 1h | `ResultWriter.cpp` |
| Add runBenchmarksOnly to ArenaRunner | 1h | `ArenaRunner.cpp` |
| Implement benchmark comparison | 1h | `ArenaRunner.cpp` |
| Update arena.yaml schema | 0.5h | `config/arena.yaml` |
| Tests for BenchRunner | 1.5h | `test/engine_arena/BenchRunnerTest.cpp` |
| **CI/CD** | | |
| GitHub Actions workflow for bench | 1h | `.github/workflows/benchmark.yml` |
| Documentation | 1h | Update README |
| **Total Phase 1** | **~20h** | |

### Phase 2: `speedtest` Command (OPTIONAL)
| Task | Effort | Files |
|------|--------|-------|
| Create game sequences (~300 positions) | 3h | `src/engine/BenchmarkPositions.h` |
| Implement `runSpeedtest()` with time scaling | 4h | `src/engine/Benchmark.cpp` |
| Add hash utilization tracking | 2h | `src/engine/Benchmark.cpp` |
| Add UCI `speedtest` command | 0.5h | `src/engine/UciHandler.cpp` |
| Arena integration (SpeedtestRunner, etc.) | 5h | Multiple arena files |
| Tests | 2h | Test files |
| **Total Phase 2** | **~16.5h** | |

---

## Success Criteria

### Phase 1 (`bench`)
1. **Reproducibility:** Running `bench` twice yields < 3% NPS variance
2. **Comparability:** Can compare NPS across versions with confidence intervals
3. **Arena Integration:** Bench results stored in JSON, comparison reports work
4. **CI/CD:** GitHub Actions runs bench on every PR
5. **Documentation:** Clear instructions for running `bench` command

### Phase 2 (`speedtest` - if implemented)
1. **Stability:** < 2% NPS variance due to longer duration
2. **Hash Metrics:** Useful hash utilization statistics reported
3. **Game Realism:** Time scaling matches real game behavior

---

## Risks & Mitigations

| Risk                              | Impact | Mitigation                              |
|-----------------------------------|--------|-----------------------------------------|
| NPS variance due to system load   | Medium | Run multiple times, report avg/stddev   |
| Different CPUs give different NPS | Medium | Track relative change, not absolute NPS |
| Hash table effects                | Low    | Clear TT between runs, use ucinewgame   |

---

## Dependencies

- Phase 1 has no external dependencies (uses existing Search infrastructure)
- Phase 1 Arena integration depends on existing Arena framework
- CI/CD depends on GitHub Actions (already configured)
- Phase 2 (optional) depends on Phase 1 completion

---

## Related Documents

- `docs/specs/V1_ENGINE_ENHANCEMENT_PLAN.md` - Main roadmap
- `docs/specs/Engine_Arena_Implementation_Plan.md` - Arena framework
- `config/arena.yaml` - Arena configuration

---

## Revision History

| Version | Date       | Author  | Changes                                                                                                                                                 |
|---------|------------|---------|---------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1.0     | 2026-02-08 | Copilot | Initial plan                                                                                                                                            |
| 1.1     | 2026-02-08 | Copilot | Added comprehensive speedtest approach based on Stockfish                                                                                               |
| 1.2     | 2026-02-08 | Copilot | Restructured into two phases: `bench` first with full Arena integration, `speedtest` as optional future enhancement                                     |
| 1.3     | 2026-02-08 | Copilot | Moved Phase 1 (`bench`) to v1.2 to enable measuring StaticMoveList and other v1.2 performance improvements                                              |
| 1.4     | 2026-02-08 | Copilot | Changed benchmark positions to use existing public domain testsets (WAC, Kaufman, Eigenmann) instead of Stockfish positions to avoid licensing concerns |
| 1.5     | 2026-02-08 | Copilot | **Phase 1 Complete ✅** - UCI `bench` command and Arena integration fully implemented with TT clearing per position                                     |
