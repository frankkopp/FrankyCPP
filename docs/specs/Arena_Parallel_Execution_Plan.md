# Engine Arena - Parallel Execution Plan

**Status:** Phase 2 Complete  
**Created:** 2026-02-04  
**Last Updated:** 2026-02-05

## Overview

This document outlines the plan to add parallel execution capabilities to the Engine Arena framework. The implementation focuses on two key improvements:

1. **Immediate result persistence** - Save test suite results as soon as each suite completes
2. **Parallel position execution** - Run multiple engine instances to test positions in parallel within a single suite

**Goals:**
1. Save test suite results immediately after completion (crash resilience)
2. Enable parallel execution of positions within a single test suite (speed)
3. Make parallelism configurable per test suite

---

## Phase 1: Callback for Immediate Result Saving

### Problem Statement

Currently, test suite results are saved **after all suites complete**:

```cpp
// ArenaRunner.cpp - Current flow
void ArenaRunner::runTestSuitesOnly() {
    auto results = testSuiteRunner.runAllTestSuites();  // Runs ALL suites
    
    // Results saved only after everything completes
    for (const auto& result : results) {
        resultWriter.writeTestSuiteResult(result);  // Too late if crash occurred
    }
}
```

**Issues:**
- If program crashes mid-run, all completed results are lost
- No progress feedback during long test runs
- Memory holds all results until end

### Solution: Result Callback

Add a callback mechanism to `TestSuiteRunner` that is invoked after each test suite completes.

### Changes Required

#### 1. TestSuiteRunner.h

```cpp
#include <functional>

namespace arena {

/// Callback invoked when a test suite completes
/// @param result The completed test suite result
using SuiteResultCallback = std::function<void(const TestSuiteResult&)>;

class TestSuiteRunner {
public:
    explicit TestSuiteRunner(const ArenaConfig& config);

    /// Runs a single test suite and returns detailed results
    TestSuiteResult runTestSuite(const TestSuiteConfig& suiteConfig) const;

    /// Runs all configured test suites sequentially
    /// @param onSuiteComplete Optional callback invoked after each suite completes
    /// @return Vector of TestSuiteResult, one per configured suite
    std::vector<TestSuiteResult> runAllTestSuites(
        const SuiteResultCallback& onSuiteComplete = nullptr) const;

private:
    const ArenaConfig& arenaConfig;
    static std::string getCurrentTimestamp();
};

} // namespace arena
```

#### 2. TestSuiteRunner.cpp

```cpp
std::vector<TestSuiteResult> TestSuiteRunner::runAllTestSuites(
    const SuiteResultCallback& onSuiteComplete) const {
    
    std::vector<TestSuiteResult> results;
    results.reserve(arenaConfig.testSuites.size());

    // ... existing header output ...

    int suiteNumber = 0;
    for (const auto& suiteConfig : arenaConfig.testSuites) {
        suiteNumber++;
        std::cout << "\n[" << suiteNumber << "/" << arenaConfig.testSuites.size() << "] ";

        try {
            TestSuiteResult result = runTestSuite(suiteConfig);
            
            // NEW: Invoke callback immediately after suite completes
            if (onSuiteComplete) {
                onSuiteComplete(result);
            }
            
            results.push_back(std::move(result));
        } catch (const std::exception& e) {
            std::cerr << "\nERROR: Failed to run test suite '" << suiteConfig.name 
                      << "': " << e.what() << std::endl;
            throw;
        }
    }

    // ... existing summary output ...

    return results;
}
```

#### 3. ArenaRunner.cpp

```cpp
void ArenaRunner::runTestSuitesOnly() {
    std::cout << "\n===================================================================" << std::endl;
    std::cout << "Running Test Suites Only" << std::endl;
    std::cout << "===================================================================" << std::endl;
    std::cout << "Number of suites: " << arenaConfig.testSuites.size() << std::endl;
    std::cout << "===================================================================" << std::endl;

    // NEW: Pass callback that saves results immediately
    auto results = testSuiteRunner.runAllTestSuites(
        [this](const TestSuiteResult& result) {
            std::string jsonPath = resultWriter.writeTestSuiteResult(result);
            std::cout << "  -> Saved: " << jsonPath << std::endl;
        });

    // Results already saved via callback, just print summary
    std::cout << "\n===================================================================" << std::endl;
    std::cout << "Test Suites Complete - " << results.size() << " results saved" << std::endl;
    std::cout << "===================================================================" << std::endl;
}
```

### Benefits

| Aspect | Before | After |
|--------|--------|-------|
| **Crash resilience** | All results lost | Completed suites preserved |
| **Progress feedback** | None until end | Real-time save confirmation |
| **Memory usage** | Holds all results | Can process/discard immediately |
| **Future parallelism** | Not supported | Callback enables thread-safe saving |

### Validation

1. Run arena with multiple test suites
2. Verify JSON files appear after each suite completes (not at end)
3. Kill process mid-run, verify completed suite results are saved
4. Check no duplicate saves occur

### Status
- [x] Implementation complete
- [x] Tested with multiple suites
- [x] Crash resilience verified

### Estimated Time: 1-2 hours

---

## Phase 2: Parallel Position Execution (Single Suite)

### Problem Statement

A test suite with 300 positions at 5s/position takes 25 minutes sequentially. With 4 parallel engine instances, this could be ~6 minutes.

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    TestSuiteRunner                          │
├─────────────────────────────────────────────────────────────┤
│  Work Queue (thread-safe)                                   │
│  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐        │
│  │Pos1 │Pos2 │Pos3 │Pos4 │Pos5 │Pos6 │ ... │PosN │        │
│  └──┬──┴──┬──┴──┬──┴──┬──┴─────┴─────┴─────┴─────┘        │
│     │     │     │     │                                     │
│     ▼     ▼     ▼     ▼                                     │
│  ┌──────┬──────┬──────┬──────┐                             │
│  │Worker│Worker│Worker│Worker│  (each with own UCIEngine)  │
│  │  1   │  2   │  3   │  4   │                             │
│  └──┬───┴──┬───┴──┬───┴──┬───┘                             │
│     │      │      │      │                                  │
│     ▼      ▼      ▼      ▼                                  │
│  ┌─────────────────────────────────────────────────────┐   │
│  │         Results Vector (mutex-protected)             │   │
│  └─────────────────────────────────────────────────────┘   │
│                          │                                  │
│                          ▼                                  │
│              TestSuiteResult (aggregated)                   │
└─────────────────────────────────────────────────────────────┘
```

### Key Design Decisions

#### 1. Engine Pool
Each worker thread has its **own UCI engine process**:
- No shared state between engines
- Each engine maintains its own TT
- Clean isolation eliminates thread-safety concerns in engine code

#### 2. Work Distribution: Queue-Based
Using a thread-safe queue for better load balancing:
- Positions have variable solve times
- Queue auto-balances work across workers
- Lock contention is minimal (quick push/pop operations)

#### 3. Per-Suite Thread Configuration
Each test suite can specify its own worker count:
- Different suites may benefit from different parallelism levels
- Some suites may need sequential execution (e.g., debugging)
- Resource management per suite

### Configuration Extension

```yaml
testSuites:
  - name: "WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V1.1/FrankyCPP_v1.1.exe"
    isolatePositions: true
    # NEW: Parallel execution setting
    parallelWorkers: 4       # 0 or 1 = sequential, N>1 = use N workers

  - name: "STS"
    epdPath: "test/testsets/STS1-STS15_LAN.EPD"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V1.1/FrankyCPP_v1.1.exe"
    parallelWorkers: 8       # More workers for this large suite

  - name: "debug_suite"
    epdPath: "test/testsets/debug.epd"
    timePerMove: 10000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V1.1/FrankyCPP_v1.1.exe"
    parallelWorkers: 1       # Sequential for debugging
```

### Changes Required

#### 1. ArenaConfig.h - Add parallelWorkers field

```cpp
struct TestSuiteConfig {
    std::string name;
    std::string epdPath;
    int timePerMove = 5000;
    int maxDepth = 30;
    std::string enginePath;
    bool isolatePositions = true;
    bool debugMode = false;
    std::string commandLineArgs;
    std::string uciOptions;
    int parallelWorkers = 1;    // NEW: 1 = sequential, N>1 = parallel
};
```

#### 2. ArenaConfig.cpp - Parse parallelWorkers

```cpp
// In YAML parsing section
if (suiteNode["parallelWorkers"]) {
    suite.parallelWorkers = suiteNode["parallelWorkers"].as<int>();
    if (suite.parallelWorkers < 1) {
        suite.parallelWorkers = 1;  // Minimum 1 worker
    }
}
```

#### 3. TestSuiteRunner.h - Add parallel execution method

```cpp
class TestSuiteRunner {
public:
    explicit TestSuiteRunner(const ArenaConfig& config);

    /// Runs a single test suite (auto-selects sequential or parallel)
    TestSuiteResult runTestSuite(const TestSuiteConfig& suiteConfig) const;

    /// Runs all configured test suites sequentially
    std::vector<TestSuiteResult> runAllTestSuites(
        const SuiteResultCallback& onSuiteComplete = nullptr) const;

private:
    const ArenaConfig& arenaConfig;
    
    /// Runs test suite sequentially (original implementation)
    TestSuiteResult runTestSuiteSequential(const TestSuiteConfig& suiteConfig) const;
    
    /// Runs test suite with parallel position execution
    TestSuiteResult runTestSuiteParallel(
        const TestSuiteConfig& suiteConfig,
        int numWorkers) const;
    
    /// Worker thread function for parallel execution
    void workerThread(
        const TestSuiteConfig& config,
        std::queue<EpdEntry>& workQueue,
        std::mutex& queueMutex,
        std::vector<TestCaseResult>& results,
        std::mutex& resultsMutex,
        std::atomic<int>& completedCount,
        int totalPositions) const;
    
    static std::string getCurrentTimestamp();
};
```

#### 4. TestSuiteRunner.cpp - Implementation

```cpp
TestSuiteResult TestSuiteRunner::runTestSuite(const TestSuiteConfig& suiteConfig) const {
    // Auto-select based on parallelWorkers config
    if (suiteConfig.parallelWorkers <= 1) {
        return runTestSuiteSequential(suiteConfig);
    } else {
        return runTestSuiteParallel(suiteConfig, suiteConfig.parallelWorkers);
    }
}

TestSuiteResult TestSuiteRunner::runTestSuiteParallel(
    const TestSuiteConfig& suiteConfig,
    int numWorkers) const {
    
    std::cout << "Running test suite: " << suiteConfig.name 
              << " (parallel: " << numWorkers << " workers)" << std::endl;
    
    // 1. Parse EPD file
    EpdParser parser;
    std::vector<EpdEntry> positions = parser.parseFile(suiteConfig.epdPath);
    const int totalPositions = static_cast<int>(positions.size());
    
    std::cout << "Loaded " << totalPositions << " positions" << std::endl;
    
    // 2. Create thread-safe work queue
    std::queue<EpdEntry> workQueue;
    for (auto& pos : positions) {
        workQueue.push(std::move(pos));
    }
    std::mutex queueMutex;
    
    // 3. Shared results storage
    std::vector<TestCaseResult> results;
    results.reserve(totalPositions);
    std::mutex resultsMutex;
    std::atomic<int> completedCount{0};
    
    // 4. Create and start worker threads
    std::vector<std::thread> workers;
    workers.reserve(numWorkers);
    
    auto startTime = std::chrono::steady_clock::now();
    
    for (int i = 0; i < numWorkers; ++i) {
        workers.emplace_back(&TestSuiteRunner::workerThread, this,
            std::cref(suiteConfig),
            std::ref(workQueue), std::ref(queueMutex),
            std::ref(results), std::ref(resultsMutex),
            std::ref(completedCount),
            totalPositions);
    }
    
    // 5. Wait for all workers to complete
    for (auto& worker : workers) {
        worker.join();
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto totalTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    // 6. Sort results by position ID for consistent ordering
    std::sort(results.begin(), results.end(), 
        [](const TestCaseResult& a, const TestCaseResult& b) {
            return a.testId < b.testId;
        });
    
    // 7. Aggregate and return results
    return aggregateResults(suiteConfig, results, totalTimeMs);
}

void TestSuiteRunner::workerThread(
    const TestSuiteConfig& config,
    std::queue<EpdEntry>& workQueue,
    std::mutex& queueMutex,
    std::vector<TestCaseResult>& results,
    std::mutex& resultsMutex,
    std::atomic<int>& completedCount,
    int totalPositions) const {
    
    // Each worker creates its own engine instance
    UCIEngine engine(config.enginePath, config.commandLineArgs);
    engine.initialize();
    
    // Apply UCI options
    if (!config.uciOptions.empty()) {
        engine.setOptions(config.uciOptions);
    }
    
    while (true) {
        // Grab next position from queue
        EpdEntry position;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (workQueue.empty()) {
                break;  // No more work
            }
            position = std::move(workQueue.front());
            workQueue.pop();
        }
        
        // Clear engine state if required
        if (config.isolatePositions) {
            engine.newGame();
        }
        
        // Run test (no lock needed - engine is thread-local)
        TestCaseResult result = runSinglePosition(engine, position, config);
        
        // Store result (thread-safe)
        {
            std::lock_guard<std::mutex> lock(resultsMutex);
            results.push_back(std::move(result));
        }
        
        // Progress update
        int completed = ++completedCount;
        if (completed % 10 == 0 || completed == totalPositions) {
            std::cout << "\r  Progress: " << completed << "/" << totalPositions 
                      << " (" << (completed * 100 / totalPositions) << "%)" 
                      << std::flush;
        }
    }
    
    // Engine destructor sends "quit" command
}
```

### Resource Considerations

| Workers | Engine Processes | Memory (est.) | Speedup |
|---------|------------------|---------------|---------|
| 1 | 1 | ~200 MB | 1x |
| 2 | 2 | ~400 MB | ~1.9x |
| 4 | 4 | ~800 MB | ~3.5x |
| 8 | 8 | ~1.6 GB | ~6x |

**Note:** Diminishing returns due to:
- Engine startup overhead (~1-2s each)
- Queue synchronization overhead
- CPU/memory contention at high worker counts

**Recommendation:** Default to 4 workers, allow override per suite.

### Error Handling

1. **Engine startup failure:** Log error, reduce worker count, continue with remaining workers
2. **Engine crash during test:** Mark position as failed, worker continues with next position
3. **All engines fail:** Fail the entire suite with error

```cpp
// In workerThread, wrap engine operations in try-catch
try {
    TestCaseResult result = runSinglePosition(engine, position, config);
    // ... store result ...
} catch (const std::exception& e) {
    // Log error, mark as failed, continue
    TestCaseResult failedResult;
    failedResult.testId = position.id;
    failedResult.passed = false;
    failedResult.error = e.what();
    
    std::lock_guard<std::mutex> lock(resultsMutex);
    results.push_back(std::move(failedResult));
}
```

### Validation

1. Run same suite with parallelWorkers=1 and parallelWorkers=4
2. Verify identical results (same pass/fail counts)
3. Verify ~3-4x speedup with 4 workers
4. Test engine crash recovery (kill one engine process mid-suite)
5. Verify memory usage scales as expected

### Status
- [x] Configuration parsing (parallelWorkers)
- [x] Sequential/parallel dispatch in runTestSuite()
- [x] Worker thread implementation
- [x] Result aggregation with sorting
- [x] Error handling
- [x] Progress display
- [ ] Testing and validation

### Estimated Time: 4-6 hours

---

## Implementation Order

| Phase | Description | Dependencies | Time |
|-------|-------------|--------------|------|
| **1** | Suite-level callback | None | 1-2h |
| **2** | Parallel positions | Phase 1 | 4-6h |

**Total estimated time:** 5-8 hours

---

## Configuration Summary

### arena.yaml Structure

```yaml
version: "v1.1"
resultsDir: "./results"
cutechessPath: "D:/Games/Cute Chess/cutechess-cli.exe"

testSuites:
  - name: "WAC"
    epdPath: "test/testsets/wac.epd"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V1.1/FrankyCPP_v1.1.exe"
    isolatePositions: true
    parallelWorkers: 4         # NEW: 1=sequential, N>1=parallel

  - name: "STS"
    epdPath: "test/testsets/STS1-STS15_LAN.EPD"
    timePerMove: 5000
    maxDepth: 30
    enginePath: "Release/FrankyCPP_V1.1/FrankyCPP_v1.1.exe"
    parallelWorkers: 8         # More workers for large suite

  - name: "franky_tests_debug"
    epdPath: "test/testsets/franky_tests.epd"
    timePerMove: 10000
    maxDepth: 30
    enginePath: "cmake-build-win-debug/src/FrankyCPP_v1.1.exe"
    debugMode: true
    parallelWorkers: 1         # Sequential for debugging

matches:
  # ... unchanged ...
```

---

## Success Criteria

### Phase 1 (Callback)
- [x] Results saved immediately after each suite completes
- [x] Killing process mid-run preserves completed results
- [x] No change in result content or format

### Phase 2 (Parallel Positions)
- [ ] 4 workers achieve ~3x speedup on 300-position suite
- [ ] Results identical regardless of worker count (pass/fail counts match)
- [ ] Memory usage scales linearly with workers
- [ ] Graceful handling of engine crashes
- [ ] Progress display works correctly

---

## Open Questions

1. **Engine crash recovery:** If one engine crashes, should we restart it or continue with fewer workers?
   - **Recommendation:** Log error, mark position as failed, continue with remaining workers. Optionally restart engine for next position.

2. **Result ordering:** Parallel execution produces results in non-deterministic order.
   - **Decision:** Sort results by position ID before aggregating for consistent output.

3. **Default worker count:** What should the default be if not specified?
   - **Recommendation:** Default to 1 (sequential) for backward compatibility. User must explicitly enable parallelism.

---

## Future Enhancements

The following features are not planned for initial implementation but could be added later:

### Parallel Suite Execution
Run multiple test suites concurrently (each suite gets its own set of workers). This would require:
- Thread-safe ResultWriter (mutex on file writes)
- Progress display for multiple concurrent suites
- Resource management (total worker limit across all suites)

### Position-Level Result Streaming
Stream individual position results to a file as they complete (for very long suites). This would require:
- Position result callback
- Incremental file writing
- Recovery mechanism to resume interrupted suites

### Auto-Tuning
Automatically determine optimal worker count based on:
- Available CPU cores
- Available memory
- Engine memory footprint

---

## Summary

This plan focuses on two high-value improvements:

1. **Phase 1 (Callback):** Immediate crash resilience by saving results as each suite completes
2. **Phase 2 (Parallel positions):** Major speedup (3-6x) for large test suites via parallel engine instances

The callback pattern in Phase 1 provides the foundation for Phase 2's parallel execution, ensuring results are safely persisted even when running positions in parallel.

Per-suite `parallelWorkers` configuration allows fine-grained control over parallelism, with sequential execution remaining the default for backward compatibility.
