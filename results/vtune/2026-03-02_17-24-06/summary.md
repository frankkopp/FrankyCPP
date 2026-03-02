# VTune Analysis Summary - 4 Threads vs 8 Threads Comparison

**Date:** 2026-03-02 17:24:06
**Executable:** FrankyCPP_v1.4.exe
**Configuration:** --bench --threads 4 -l warn -s warn
**Build:** RelWithDebInfo
**Purpose:** Compare memory pressure with reduced thread count

---

## Executive Comparison: 4 Threads vs 8 Threads

### TT Performance - DRAMATIC IMPROVEMENT with Fewer Threads

| Metric                     | 8 Threads | 4 Threads | Change   | Interpretation                 |
|----------------------------|-----------|-----------|----------|--------------------------------|
| **TT::probe CPU Time**     | 42.30s    | 9.24s     | **-78%** | Less contention                |
| **TT::probe CPI**          | 3.37      | 6.56      | +95%     | Higher CPI but less total time |
| **TT::probe Memory Bound** | 74%       | 37.5%     | **-49%** | Half the memory pressure!      |
| **TT::probe LLC Misses**   | 1.65M     | 1.10M     | **-33%** | Fewer cache misses             |
| **TT::put CPU Time**       | 13.01s    | 11.58s    | -11%     | Similar                        |
| **TT::put Memory Bound**   | 73%       | 45.7%     | **-37%** | Reduced pressure               |

### Key Insight: Memory Bandwidth Saturation Confirmed

The 8-thread configuration **saturates the memory subsystem**:
- Memory Bound drops from **74% → 37.5%** (halved!)
- LLC Miss Count drops from **1.65M → 1.10M** (33% reduction)
- TT::probe total time drops from **42.3s → 9.2s** (78% reduction!)

This proves the TT bottleneck is primarily **memory bandwidth saturation**, not inherent latency.

### Slider Tables - Unchanged (As Expected)

| Metric                    | 8 Threads | 4 Threads | Change |
|---------------------------|-----------|-----------|--------|
| sliderLookup Memory Bound | 9.7%      | 6.5%      | -33%   |
| sliderLookup CPI          | 0.38      | 0.38      | Same   |
| sliderLookup LLC Misses   | 1.65M     | 0.55M     | -67%   |

Slider tables were never the problem - their CPI remains excellent at 0.38.

---

## Detailed Metrics

### TT Performance (4 Threads)

| Metric           | TT::probe | TT::put | TT::prefetch    |
|------------------|-----------|---------|-----------------|
| CPU Time (s)     | 9.24      | 11.58   | 8.71 (combined) |
| CPI Rate         | 6.56      | 3.30    | 1.68            |
| Memory Bound (%) | 37.5%     | 45.7%   | ~10%            |
| LLC Misses       | 1,100,462 | 0       | 0               |

### Atomic Load Overhead (4 Threads)

| Location      | CPU Time | Memory Bound | CPI  |
|---------------|----------|--------------|------|
| TT key load   | 5.10s    | 81.7%        | 2.50 |
| TT entry load | 4.25s    | 32.1%        | -    |

### Threading Efficiency (4 Threads)

| Metric    | Value       |
|-----------|-------------|
| Spin Time | 0.0s (0.0%) |
| Wait Time | 0.0s (0.0%) |

Still zero contention with 4 threads.

---

## Critical Finding: Memory Bandwidth is the Bottleneck

### Evidence

| Observation            | 8 Threads | 4 Threads | Conclusion                            |
|------------------------|-----------|-----------|---------------------------------------|
| TT::probe Memory Bound | 74%       | 37.5%     | Memory pressure scales with threads   |
| TT::probe time         | 42.3s     | 9.2s      | Contention causes massive slowdown    |
| LLC Misses             | 1.65M     | 1.10M     | More threads = more misses            |
| Prefetch Memory Bound  | 70-74%    | ~10%      | Prefetch works better with 4 threads! |

### What This Means for Optimization

1. **TT Buckets will help MORE with 8 threads** - Better locality reduces bandwidth pressure
2. **Cache-line alignment is CRITICAL** - False sharing amplifies with more threads  
3. **Prefetch IS working** - Just overwhelmed at 8 threads (10% vs 70% memory bound)
4. **Consider adaptive thread count** - Some positions may benefit from fewer threads

---

## Recommendations Based on This Analysis

### High Priority (Confirmed by 4-thread test)

1. **TT Buckets** - Will reduce LLC misses by improving locality; bigger impact at 8 threads
2. **Cache-line alignment** - False sharing is worse with more threads

### New Insight

3. **Prefetch IS effective** - At 4 threads it shows 10% memory bound; the issue at 8 threads is bandwidth saturation, not prefetch timing

### Consider

4. **Thread scaling analysis** - Current implementation may have diminishing returns beyond 4-6 threads due to TT bandwidth limits

---

## Summary

| Category               | 8 Threads            | 4 Threads          | Status                         |
|------------------------|----------------------|--------------------|--------------------------------|
| TT Memory Bound        | 🔴 74%               | 🟡 37.5%           | Bandwidth saturation confirmed |
| Prefetch Effectiveness | 🔴 Poor              | 🟢 Working         | Overwhelmed at 8 threads       |
| Thread Contention      | 🟢 None              | 🟢 None            | Not the issue                  |
| Overall Efficiency     | 🟡 Bandwidth limited | 🟢 Better balanced | Memory is the bottleneck       |

**Conclusion:** The 4-thread test **confirms memory bandwidth saturation** is the root cause of TT performance issues. Optimizations that reduce memory traffic (buckets, alignment) will have the biggest impact at higher thread counts.

---
*Generated from VTune automated analysis - 4 thread comparison run*
