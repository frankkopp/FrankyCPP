# VTune Analysis Summary

**Date:** 2026-03-03 13:28:59
**Executable:** FrankyCPP v1.4
**Configuration:** Multi-threaded search
**Build:** Release

## Key Performance Metrics

### TT Performance (Primary Focus)

| Metric           | TT::probe  | TT::put    | TT::prefetch (combined) |
|------------------|------------|------------|-------------------------|
| CPU Time (s)     | 22.76      | 24.32      | 20.41                   |
| Memory Bound (%) | 62.6-71.3% | 46.7-67.1% | 66.9-77.4%              |
| CPI Rate         | 9.10       | 6.26       | 3.77-4.11               |
| LLC Misses       | 1,100,462  | 0          | 0                       |

**Note:** TT::prefetch appears at two call sites (0x14020a501 and 0x1402098f9), metrics combined.

### Comparison Baselines

| Metric           | Attacks::sliderLookup | Evaluator::evaluate | Search::search |
|------------------|-----------------------|---------------------|----------------|
| CPU Time (s)     | 6.26                  | 1.02                | 2.47           |
| Memory Bound (%) | 13.7-14.3%            | 31.0-34.4%          | 29.8-34.8%     |
| CPI Rate         | 0.39                  | 0.39                | 0.50           |

### Threading Efficiency

| Metric                    | Value       |
|---------------------------|-------------|
| TT::put Effective Time    | 25.78s      |
| TT::probe Effective Time  | 23.83s      |
| Search::thread (combined) | 2.68s       |
| Spin Time                 | 0.0s (0.0%) |
| Overhead Time             | 0.0s (0.0%) |

**Note:** Zero spin time indicates good thread utilization - no lock contention detected.

### Overall Assessment

| Category          | Status | Notes                                                    |
|-------------------|--------|----------------------------------------------------------|
| TT Memory Bound   | 🔴     | 62-77% memory bound - L3 dominant bottleneck             |
| Thread Contention | 🟢     | Zero spin time, excellent thread efficiency              |
| Slider Tables     | 🟢     | Only 13-14% memory bound, CPI=0.39 excellent             |
| Evaluator         | 🟡     | 31-34% memory bound, acceptable but room for improvement |

## Top Hotspots Summary

| Rank | Function                                | CPU Time | % of Total |
|------|-----------------------------------------|----------|------------|
| 1    | TT::put                                 | 24.32s   | ~19%       |
| 2    | TT::probe                               | 22.76s   | ~18%       |
| 3    | std::_Atomic_storage::load (TT-related) | 16.12s   | ~13%       |
| 4    | TT::prefetch (combined)                 | 20.41s   | ~16%       |
| 5    | Attacks::sliderLookup                   | 6.26s    | ~5%        |
| 6    | Search::stopConditions                  | 6.56s    | ~5%        |

**TT operations account for ~66% of total CPU time!**

## Memory Analysis Deep Dive

### TT::probe Memory Breakdown (P-core)
- L1 Bound: 0.6%
- L2 Bound: 0.0%
- **L3 Bound: 59.9%** ← Primary bottleneck
- **DRAM Bound: 8.0%** ← Secondary bottleneck

### TT::put Memory Breakdown (P-core)
- L1 Bound: 1.7%
- L2 Bound: 0.0%
- **L3 Bound: 71.4%** ← Primary bottleneck
- DRAM Bound: 0.1%

### TT::prefetch Memory Breakdown
- **L3 Bound: 68.9-71.4%** ← Prefetch helping but L3 still limiting

## Changes Since Baseline

### Code Changes: Lazy SMP Refactor (commit ac215e6)

**Date:** 2026-03-03 12:54  
**Commit:** `ac215e6` - "Refactor Lazy SMP implementation to reuse `iterativeDeepening` for helper threads"

Key changes:
- Removed separate `helperRun()` function - now all threads use shared `iterativeDeepening()`
- Added `isMainThread()` guards for UCI output, time management, TB probing
- Moved `rootMoves` and `evaluator` to thread-local `SearchThreadData`
- Implemented **depth diversification** for helper threads
- Fixed concurrency issues with thread-safe TT and PawnTT

**Files changed:** Search.cpp (+491/-317 lines), Search.h, SearchThreadData.h

### Comparison with Previous Runs

| Run         | Date             | Threads | TT::probe Time | TT::probe CPI | TT::probe Mem Bound |
|-------------|------------------|---------|----------------|---------------|---------------------|
| Baseline    | 2026-03-02 16:36 | 8       | 42.30s         | 3.37          | 74%                 |
| 4-Thread    | 2026-03-02 17:24 | 4       | 9.24s          | 6.56          | 37.5%               |
| 1-Thread    | 2026-03-02 18:23 | 1       | 0.05s          | 1.64          | 29%                 |
| **Current** | 2026-03-03 13:28 | 8       | 22.76s         | 9.10          | 62.6-71.3%          |

### Key Observations vs Baseline (8-thread)

| Metric               | Baseline (03-02) | Current (03-03) | Change     |
|----------------------|------------------|-----------------|------------|
| TT::probe CPU Time   | 42.30s           | 22.76s          | **-46%** ✅ |
| TT::put CPU Time     | 13.01s           | 24.32s          | +87%       |
| TT::probe Mem Bound  | 74%              | 62.6%           | **-15%** ✅ |
| sliderLookup CPI     | 0.38             | 0.39            | ~0%        |

### Analysis: Lazy SMP Refactor Impact

✅ **Positive impacts from the thread refactor:**
- TT::probe time dropped 46% (42.3s → 22.76s) - better thread coordination
- Memory bound reduced from 74% to 62.6% - less memory contention
- Zero spin time maintained - no lock contention issues

⚠️ **Trade-offs observed:**
- TT::put time increased (+87%) - more writes due to all threads using full iterativeDeepening
- CPI increased (3.37 → 9.10) - different memory access patterns, but total time still lower

**Conclusion:** The Lazy SMP refactor improved overall TT performance **without any TT-specific optimizations**. The depth diversification and thread-local data reduced memory contention significantly. TT optimization (buckets, alignment) would provide additional gains on top of these improvements.

### Historical Pattern Confirmed

The multi-run comparison confirms:
1. **Memory bandwidth saturation** scales with thread count
2. **TT operations** dominate CPU time in multi-threaded runs
3. **Slider tables** are not affected by thread count (PEXT tables fit in L3)
4. **Better thread management** reduces TT contention even without TT changes

## Recommendations

**Reference:** See `docs/specs/PLAN_TT_Performance_Optimization.md` for full analysis and implementation details.

### Priority 1: TT Buckets + alignas(64) ⬅️ NEXT STEP
- Pack 4×16B entries into 64-byte cache-aligned clusters
- Eliminates false sharing between threads
- Enables smarter replacement policy (depth vs recency)
- **Previous 20% slowdown was single-threaded** - SMP may show net benefit
- Stockfish uses 3-entry buckets specifically for Lazy SMP

### Priority 2: Reduce TT Access Frequency
- Skip TT probe in late move reductions
- Batch TT updates where possible
- Consider lazy TT clearing strategies

### Priority 3: Attack Caching Per-Position (Lower Priority)
- Cache attack bitboards once per position
- Slider tables already excellent (CPI 0.39) - diminishing returns
- Consider only if TT optimizations plateau

### NOT Recommended (Per Analysis)
- ❌ **alignas(64) alone** - Wastes 48B per entry (4× memory overhead)
- ❌ **Earlier prefetch** - Not possible, requires zobrist key from doMove()
- ❌ **PEXT slider tables** - Already implemented and working well
- ❌ **Thread synchronization** - Zero contention measured

### Target Metrics (from Plan)

| Metric                 | Baseline (03-02) | Current (03-03) | Target   |
|------------------------|------------------|-----------------|----------|
| TT::probe CPI          | 3.37             | 9.10            | < 2.0    |
| TT::probe Memory Bound | 74%              | 62.6%           | < 50%    |
| TT::probe CPU Time     | 42.3s            | 22.76s          | < 30s ✅ |
| LLC Misses (TT)        | 1.65M            | 1.10M           | < 1.0M   |

**Note:** TT::probe CPU Time target already met thanks to Lazy SMP refactor!

## Raw Metrics Reference

### CPU Time by Analysis Type
| Function     | hotspots | threading | memory-access | hpc-performance |
|--------------|----------|-----------|---------------|-----------------|
| TT::probe    | 22.76s   | 23.83s    | 19.87s        | 17.07s          |
| TT::put      | 24.32s   | 25.78s    | 21.17s        | 3.48s           |
| TT::prefetch | 20.41s   | 21.40s    | 17.95s        | 0.29s           |

**Note:** Variations reflect different sampling/attribution methods.
