# VTune Analysis Summary

**Date:** 2026-03-06 01:54:49  
**Executable:** FrankyCPP v1.5  
**Configuration:** 8 threads, TT Buckets + alignas(64) implemented  
**Build:** Release

---

## Key Performance Metrics

### TT Performance (Primary Focus)

| Metric           | TT::probe         | TT::put           | TT::prefetch (combined) |
|------------------|-------------------|-------------------|-------------------------|
| CPU Time (s)     | 17.30             | 17.61             | 0.31 (inlined)          |
| Memory Bound (%) | 64.5% P / 70.7% E | 64.7% P / 45.3% E | 62.9-64.2% P            |
| CPI Rate         | 4.25              | 4.50              | 3.94-4.05               |
| LLC Misses       | 2,200,924         | 0                 | 0                       |

**Note:** TT::prefetch time appears significantly reduced due to inlining into caller functions. The actual prefetch work is distributed across Search::qsearch and Search::search call sites.

### Comparison Baselines

| Metric           | Attacks::sliderLookup | Evaluator::evaluate | Search::search    |
|------------------|-----------------------|---------------------|-------------------|
| CPU Time (s)     | 6.05                  | 0.89                | 1.95              |
| Memory Bound (%) | 9.7% P / 13.2% E      | 42.0% P / 78.6% E   | 24.0% P / 27.7% E |
| CPI Rate         | 0.36                  | 0.39                | 0.42              |

### Threading Efficiency

| Metric               | Value                      |
|----------------------|----------------------------|
| Total Effective Time | ~100% (all threads active) |
| Spin Time            | 0.0s (0.0%)                |
| Wait Time            | 0.0s (0.0%)                |

### Overall Assessment

| Category          | Status | Notes                                             |
|-------------------|--------|---------------------------------------------------|
| TT Memory Bound   | 🟡     | 64-71% still high but improved from 74% baseline  |
| Thread Contention | 🟢     | Zero spin/wait time - atomics efficient           |
| Slider Tables     | 🟢     | CPI 0.36, only 9.7% memory bound - excellent      |
| Evaluator         | 🟢     | CPI 0.39, well optimized                          |
| TT Buckets        | 🟡     | Implemented but CPI still 4.25-4.50 (target <2.0) |

---

## Changes Since Baseline (2026-03-03)

| Metric                 | Baseline (03-03) | Current (03-06) | Change     |
|------------------------|------------------|-----------------|------------|
| TT::probe CPU Time     | 22.76s           | 17.30s          | **-24%** ✅ |
| TT::probe CPI          | 9.10             | 4.25            | **-53%** ✅ |
| TT::probe Mem Bound    | 62.6%            | 64.5%           | +3%        |
| TT::put CPU Time       | 24.32s           | 17.61s          | **-28%** ✅ |
| TT::put CPI            | 6.26             | 4.50            | **-28%** ✅ |
| LLC Misses (TT::probe) | 1,100,462        | 2,200,924       | +100% 🔴   |

**Key Changes:**
1. TT Buckets implementation (4×16B entries = 64B cache line)
2. `alignas(64)` for guaranteed cache-line alignment
3. Depth-preferred + age tiebreak replacement policy

---

## Detailed Analysis

### TT Buckets Impact

The TT Buckets optimization shows **significant CPI improvement**:
- TT::probe CPI dropped from 9.10 → 4.25 (**53% reduction**)
- TT::put CPI dropped from 6.26 → 4.50 (**28% reduction**)
- Total TT CPU time reduced from ~47s → ~35s (**25% improvement**)

However, LLC misses doubled (1.1M → 2.2M), suggesting:
- Better L3 utilization leads to more actual DRAM accesses being measured
- Previous measurements may have been masked by extreme L3 latency
- The working set may be exceeding L3 cache capacity

### Memory Bound Analysis

TT functions remain highly memory-bound (64-71%) but this is expected for hash table operations. Key observations:

1. **L3 Bound dominates** (63-73% of memory bound time)
   - Indicates L3 → DRAM latency is primary bottleneck
   - DRAM Bound relatively low (0.1-6.9%) - TT fits mostly in L3

2. **Store Bound minimal** (0.2-0.5%)
   - Write operations are efficient
   - No write-back contention issues

3. **E-core vs P-core** differences:
   - E-cores show 70.7% memory bound vs 64.5% for P-cores
   - E-cores have smaller L3 access, explains higher bound

### Top CPU Consumers

| Rank | Function         | CPU Time | % of Total |
|------|------------------|----------|------------|
| 1    | TT::put          | 17.61s   | 16.5%      |
| 2    | TT::probe        | 17.30s   | 16.2%      |
| 3    | sliderLookup     | 6.05s    | 5.7%       |
| 4    | stopConditions   | 5.89s    | 5.5%       |
| 5    | Attacks::attacks | 4.28s    | 4.0%       |

---

## Recommendations

### Priority 1: Address LLC Miss Increase
- **Action:** Investigate why LLC misses doubled with TT Buckets
- **Possible causes:** 
  - Bucket search accessing more entries per probe
  - Memory allocation pattern changes
- **Test:** Compare with smaller TT size to confirm L3 fit

### Priority 2: Reduce TT Access Frequency
- **Action:** Skip TT probe in late move reductions (LMR)
- **Expected impact:** 10-15% reduction in TT operations
- **Risk:** Minimal - LMR nodes have low hit rate anyway

### Priority 3: Optimize stopConditions
- **Action:** Review stopConditions implementation (5.89s / 5.5%)
- **Observation:** 15.5% microarchitecture usage, CPI 1.28
- **Possible optimization:** Reduce polling frequency or inline critical checks

---

## Progress Toward Targets

| Metric                 | Target | Current | Status               |
|------------------------|--------|---------|----------------------|
| TT::probe CPU Time     | < 30s  | 17.30s  | ✅ Met                |
| TT::probe CPI          | < 2.0  | 4.25    | 🟡 Improved, not met |
| TT::probe Memory Bound | < 50%  | 64.5%   | 🟡 Needs work        |
| LLC Misses (TT)        | < 1.0M | 2.2M    | 🔴 Regressed         |

---

*Analysis generated from VTune 2026-03-06_01-54-49 run*
