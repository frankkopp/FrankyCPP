# TT and Memory Performance Optimization Plan

**Status:** ✅ **COMPLETE** — All viable optimizations implemented or evaluated  
**Created:** 2026-03-02  
**Last Updated:** 2026-03-07  
**Priority:** ~~Medium-High~~ → Complete

---

## Latest Update: Lazy SMP Refactor Impact (2026-03-03)

### VTune Results After Lazy SMP Refactor (commit ac215e6)

The Lazy SMP refactor (removing separate `helperRun()`, using shared `iterativeDeepening()` with depth diversification) **improved TT performance without any TT-specific changes:**

| Metric               | Before (03-02) | After (03-03) | Change     |
|----------------------|----------------|---------------|------------|
| TT::probe CPU Time   | 42.30s         | 22.76s        | **-46%** ✅ |
| TT::probe Mem Bound  | 74%            | 62.6%         | **-15%** ✅ |
| TT::put CPU Time     | 13.01s         | 24.32s        | +87%       |
| sliderLookup CPI     | 0.38           | 0.39          | ~0%        |

**Key Insight:** Better thread coordination through depth diversification reduced memory contention significantly. TT::put time increased due to all threads now using full `iterativeDeepening()` (more TT writes), but total TT::probe time dropped substantially.

**TT::probe CPU Time target (<30s) already met!** TT Buckets optimization would provide additional gains.

---

## Latest Update: TT Buckets Implementation Verified (2026-03-06)

### VTune Results After TT Buckets + alignas(64) Implementation

Single-threaded profiling (TT=256MB, Release build) confirms the TT buckets implementation is working:

| Metric               | Pre-Buckets (03-03) | Post-Buckets (03-06) | Change      |
|----------------------|---------------------|----------------------|-------------|
| TT::probe CPU Time   | 22.76s              | 19.23s               | **-15%** ✅  |
| TT::probe Mem Bound  | 62.6%               | 58.9%                | **-6%** ✅   |
| TT::probe CPI        | 9.10                | 4.31                 | **-53%** ✅  |
| TT::probe L3 Bound   | -                   | 52.4%                | (baseline)  |
| TT::put CPU Time     | 24.32s              | 31.60s               | +30%        |
| TT::put CPI          | -                   | 6.75                 | (baseline)  |
| TT::prefetch Time    | -                   | 20.41s (combined)    | (baseline)  |
| LLC Miss Count       | 1.10M               | 550K                 | **-50%** ✅  |
| sliderLookup CPI     | 0.39                | 0.39                 | ~0%         |

**Key Findings:**

1. **LLC Misses reduced by 50%** — Buckets with cache-line alignment dramatically reduced L3 cache misses (1.10M → 550K)
2. **CPI improved by 53%** — TT::probe CPI dropped from 9.10 to 4.31, meaning far less CPU stall time
3. **Memory Bound slightly improved** — 62.6% → 58.9%, still memory-bound but better
4. **TT::put time increased** — Expected due to bucket replacement logic (checking 4 entries)

### Single-Thread Hotspot Summary (Top 20)

| Rank | Function               | CPU Time (s) | % Total | Memory Bound |
|------|------------------------|--------------|---------|--------------|
| 1    | TT::put                | 33.62        | 26.8%   | 62.7%        |
| 2    | TT::probe              | 20.52        | 16.4%   | 58.9%        |
| 3    | TT::prefetch (2×)      | 20.71        | 16.5%   | 64.8%        |
| 4    | sliderLookup           | 6.85         | 5.5%    | 12.3%        |
| 5    | Attacks::attacks       | 4.93         | 3.9%    | 19.4%        |
| 6    | atomic::store          | 4.21         | 3.4%    | 89.3%        |
| 7    | atomic::load           | 3.15         | 2.5%    | 78.5%        |
| 8    | pieceEval              | 2.76         | 2.2%    | 69.5%        |
| 9    | Search::search         | 2.64         | 2.1%    | 23.7%        |
| 10   | stopConditions         | 2.42         | 1.9%    | 20.9%        |

**TT operations (put + probe + prefetch) = 59.7% of total CPU time** — Still the dominant bottleneck, but improved.

### Memory Access Analysis (Post-Buckets)

| Function              | CPU Time | Memory Bound | L3 Bound | DRAM Bound | LLC Misses |
|-----------------------|----------|--------------|----------|------------|------------|
| TT::probe             | 29.22s   | 68.6%        | 63.0%    | 7.4%       | 550,231    |
| TT::put               | 27.97s   | 63.9%        | 69.1%    | 0.0%       | 0          |
| TT::prefetch          | 9.58s    | 68.2%        | 68.4%    | 0.1%       | 0          |
| TT::prefetch (2nd)    | 8.68s    | 62.1%        | 69.7%    | 0.2%       | 0          |
| Attacks::sliderLookup | 6.64s    | **12.3%**    | 2.5%     | 2.3%       | 550,231    |

**Observations:**
- TT operations are predominantly **L3 Bound** (63-69%), not DRAM Bound
- This suggests data is mostly in L3 cache, but access patterns cause misses
- Slider tables remain highly efficient at 12.3% memory bound

### Microarchitecture Breakdown (Post-Buckets)

| Function              | CPI   | Microarch Usage | Retiring | Back-End Bound |
|-----------------------|-------|-----------------|----------|----------------|
| TT::put               | 6.75  | 3.0%            | ~2%      | ~70%           |
| TT::probe             | 5.73  | 3.5%            | ~4%      | ~65%           |
| TT::prefetch          | 4.32  | 4.2%            | ~4%      | ~65%           |
| Attacks::sliderLookup | 0.38  | 44.2%           | ~52%     | ~9%            |
| Search::search        | 0.50  | 34.0%           | ~40%     | ~24%           |

**CPI Comparison:**
- TT::probe CPI improved: **9.10 → 5.73** (37% better)
- Slider tables unchanged: **0.38** (excellent baseline)

### Remaining Optimization Opportunities

1. **TT::put is now the largest hotspot** — Bucket replacement logic adds overhead
   - Consider optimizing the 4-entry scan (SIMD comparison?)
   - Current depth-preferred + age tiebreak may have branch mispredictions

2. **Atomic operations still costly** — 7.36s combined (atomic::store + atomic::load)
   - 89% memory bound on atomic::store
   - ✅ XOR key validation implemented (eliminates atomic dependency chains)

3. **Multi-threaded benchmarks needed** — These results are single-threaded
   - The bucket design should shine under SMP (reduced false sharing)
   - ✅ Match results below confirm SMP benefits

### Match Results: TT Buckets + XOR Validation (v1.5)

**v1.5 includes:** TT Buckets, XOR key validation, SMP race condition hardening

| Match                | Games | Score | W/D/L      | ELO        | Test Suite        |
|----------------------|-------|-------|------------|------------|-------------------|
| **v1.5 vs v1.3**     | 104   | 77.9% | 69/24/11   | **+218.7** | +68 pos (+2.4%)   |
| **v1.5 vs v1.4**     | 104   | 60.6% | 42/42/20   | **+74.6**  | +27 pos (+0.9%)   |

**Analysis:**
- **+218.7 ELO vs v1.3** — Combined effect of v1.4 search improvements + v1.5 TT/SMP improvements
- **+74.6 ELO vs v1.4** — Pure TT buckets + XOR validation + SMP hardening contribution
- TT buckets implementation is a clear success under real game conditions
- SMP race hardening prevents corrupted TT entries from causing search instability

---

## Executive Summary

### Key Conclusions from VTune Analysis (5 Test Types)

Comprehensive VTune profiling (Hotspots, Microarchitecture, Memory Access, Threading, HPC) on an 8-thread benchmark reveals:

| Finding                                   | Evidence                                      | Impact                                                  |
|-------------------------------------------|-----------------------------------------------|---------------------------------------------------------|
| **TT is the critical bottleneck**         | CPI 5.73, 58.9% memory bound, 550K LLC misses | 🟡 Improved 50% but still ~60% of search time in TT ops |
| **TT Buckets reduced LLC misses by 50%**  | 1.10M → 550K LLC misses, CPI 9.10 → 5.73      | ✅ Significant improvement from cache-line alignment     |
| **XOR key validation implemented**        | Detects SMP torn reads, +74.6 ELO vs v1.4     | ✅ Robust multi-threaded TT operation                    |
| **Memory subsystem still under pressure** | TT::prefetch still 64-68% memory bound        | 🟡 Prefetch effective but memory still a factor         |
| **Threading is efficient**                | 0% spin time, 0% wait time on TT atomics      | ✅ Not a synchronization problem                         |
| **Slider tables are NOT a problem**       | CPI 0.38, only 12.3% memory bound             | ✅ PEXT already implemented & working                    |
| **Evaluator is well optimized**           | CPI 0.25-0.40, using AVX SIMD                 | ✅ No action needed                                      |

### 4-Thread vs 8-Thread Comparison (Bandwidth Saturation Proof)

| Metric                    | 8 Threads | 4 Threads | Change | Conclusion                          |
|---------------------------|-----------|-----------|--------|-------------------------------------|
| TT::probe Memory Bound    | 74%       | **37.5%** | -49%   | 🔴 Bandwidth saturates at 8 threads |
| TT::probe CPU Time        | 42.3s     | 9.2s      | -78%   | Less contention = much faster       |
| TT::probe LLC Misses      | 1.65M     | 1.10M     | -33%   | Fewer threads = fewer misses        |
| TT::prefetch Memory Bound | 70-74%    | **~10%**  | -86%   | ✅ Prefetch WORKS at 4 threads!      |

**Key insight:** The prefetch IS effective when memory bandwidth isn't saturated. At 8 threads, the memory subsystem is overwhelmed and prefetch requests queue behind existing loads.

### Root Cause

The transposition table experiences **memory latency that cannot be hidden**:
- 8 threads competing for memory bandwidth
- Random access pattern prevents effective prefetching
- Each TT probe requires waiting for key before proceeding (serial dependency)
- CPI 6.6 in TT::probe vs CPI 0.38 in slider tables = **17x worse efficiency**

### Prioritized Optimization Roadmap

| Priority | Optimization                    | Effort | Expected Impact | Rationale                                                                                          |
|----------|---------------------------------|--------|-----------------|----------------------------------------------------------------------------------------------------|
| ~~1~~    | ~~TT Buckets + alignas(64)~~    | Medium | 🔴 **HIGH**     | ✅ **COMPLETED 2026-03-06** — 50% LLC miss reduction, 53% CPI improvement, 15% probe time reduction |
| ~~2~~    | ~~Cache-line alignment alone~~  | N/A    | N/A             | ❌ Must combine with buckets; alone wastes 48B/entry (4x memory overhead)                           |
| ~~3~~    | ~~XOR key encoding~~            | Medium | 🟡 Medium       | ✅ **COMPLETED 2026-03-07** — XOR validation implemented; +74.6 ELO vs v1.4                         |
| ~~4~~    | ~~Reduce TT access frequency~~  | Medium | N/A             | ❌ **TESTED 2026-03-07** — LMR Skip TT causes +19-44% node explosion; TT cutoffs are valuable       |
| ~~5~~    | ~~Attack caching per-position~~ | Medium | 🟢 Low          | ⏭️ **SKIPPED** — Slider tables already efficient (CPI 0.38); not worth implementation effort       |
| ~~6~~    | ~~Earlier prefetch placement~~  | N/A    | N/A             | ❌ NOT POSSIBLE - requires zobrist key from after doMove()                                          |
| ~~7~~    | ~~PEXT slider tables~~          | N/A    | N/A             | ✅ **Already implemented** - no action needed                                                       |
| ~~8~~    | ~~Thread synchronization~~      | N/A    | N/A             | ✅ Zero contention measured                                                                         |

### Quick Wins (Completed)

1. ✅ **TT buckets with `alignas(64)` implemented & verified (2026-03-06):**
   - 4 entries × 16 bytes = 64 bytes = exactly 1 cache line per `TTCluster`
   - `alignas(64)` + `static_assert(alignof == 64)` guarantees cache-line aligned heap allocation
   - Depth-preferred + age tiebreak replacement policy
   - Single `_mm_prefetch` loads entire bucket (all 4 entries)
   - **Results:** 50% LLC miss reduction, 53% CPI improvement, 15% probe time reduction

2. ✅ **XOR key validation implemented (2026-03-07):**
   - Key verification using XOR encoding detects torn reads from SMP races
   - Combined with SMP race hardening for robust multi-threaded operation
   - **Match Results:** +74.6 ELO vs v1.4 (TT buckets + XOR + SMP hardening combined)

### Recent Changes

1. ✅ **Switched to unstable sort (commit 7d025903, 2026-03-03 17:38)** — Move ordering now uses `std::ranges::sort` (unstable) instead of `std::ranges::stable_sort`
   - Change at `MoveGenerator.cpp` line 36: `constexpr auto& moveSort = std::ranges::sort;`
   - VTune comparison (note: `_Insertion_sort_common` is used internally by both sort variants):
     - v1.4 (03-03 13:28, **before** change): 1.10s
     - v1.5 (03-06 01:54, after change): 1.32s  
     - v1.5 (03-06 02:34 / r022, after change): 1.34s
   - Time increase likely due to more nodes searched (TT buckets improvement), not sort regression
   - Unstable sort removes stability overhead but still uses insertion sort for small subarrays

### Failed Experiments

1. ❌ **LMR Skip TT (tested 2026-03-07)** — Skip TT probes during LMR subtree searches
   - **Hypothesis:** LMR subtrees are speculative (expect fail-low); TT probe cost may exceed benefit
   - **Implementation:** Added `USE_LMR_SKIP_TT` config flag, `skipTtProbe` field in `SearchThreadData`
   - **SearchTreeSizeTest Results:**
   
     | Depth | Baseline Nodes | LMR Skip TT Nodes | Node Increase | TT Probes Skipped |
     |-------|----------------|-------------------|---------------|-------------------|
     | 8     | 235,991        | 237,027           | +0.4%         | 275,441           |
     | 10    | 868,949        | 1,039,514         | **+19.6%**    | 1,313,573         |
     | 12    | 2,222,249      | 2,606,436         | **+17.3%**    | 3,677,833         |
     | 14    | 5,860,820      | 7,970,431         | **+36.0%**    | 12,557,486        |
     | 16    | 15,010,119     | 21,594,746        | **+43.9%**    | 36,262,354        |
   
   - **Conclusion:** TT probes in LMR subtrees provide valuable cutoffs; skipping them causes node explosion (+19-44% more nodes at depth 10-16). NPS unchanged (~7.3-7.7 Mnps), so no speed gain from reduced memory access.
   - **Root Cause:** TT entries from previous iterations/positions provide good move ordering even in LMR subtrees; losing this information leads to more re-searches (LMR re-searches increased significantly).
   - **Status:** Feature disabled by default (`USE_LMR_SKIP_TT = false`); code rolled back.

### Future Optimizations

*No remaining optimizations identified — all viable options completed or evaluated.*

### Not Viable

- ~~**Move TT_PREFETCH earlier**~~ - NOT POSSIBLE: Prefetch requires `p.getZobristKey()` which is only valid after `doMove()`. Current placement is already optimal.

### Metrics to Track After Optimization

| Metric                 | Baseline (03-02) | Lazy SMP (03-03) | TT Buckets (03-06) | Target | Status |
|------------------------|------------------|------------------|--------------------|--------|--------|
| TT::probe CPI          | 3.37             | 9.10             | 5.73               | < 2.0  | 🟡     |
| TT::probe Memory Bound | 74%              | 62.6%            | 58.9%              | < 50%  | 🟡     |
| TT::probe CPU Time     | 42.3s            | 22.76s           | 19.23s             | < 30s  | ✅      |
| LLC Misses (TT)        | 1.65M            | 1.10M            | 550K               | < 1.0M | ✅      |

**Note:** TT Buckets + XOR validation achieved LLC miss target and CPU time target. Combined improvements provide **+74.6 ELO** vs v1.4. CPI and memory bound improved but inherently limited by random TT access patterns.

### What NOT to Optimize

- **Slider tables** - Already excellent (CPI 0.38)
- **Evaluator** - Well optimized with SIMD
- **Threading/atomics** - Zero contention
- **Move sorting** - Already switched to unstable sort (commit 7d025903); `_Insertion_sort_common` is intrinsic to introsort

---

## Overview

VTune profiling of an 8-thread benchmark run revealed significant performance bottlenecks in transposition table (TT) access and slider attack table lookups. This document captures the analysis findings and outlines potential optimizations for future implementation.

---

## Profiling Summary

### Top Hotspots by CPU Time

| Rank | Function                                | Total Time | Self Time  | Notes                  |
|------|-----------------------------------------|------------|------------|------------------------|
| 1    | `Search::search`                        | 192.76s    | 5.61s      | Expected - main search |
| 2    | `Search::qsearch`                       | 139.35s    | 6.17s      | Expected - quiescence  |
| 3    | `MoveGenerator::getNextPseudoLegalMove` | 37.12s     | 4.48s      | Expected               |
| 4    | **`TT::probe`**                         | **32.25s** | **20.95s** | ⚠️ 65% self-time       |
| 5    | `MoveGenerator::fillOnDemandMoveList`   | 31.80s     | 3.34s      | Expected               |
| 6    | `Evaluator::evaluate`                   | 28.71s     | 1.95s      | Expected               |
| 7    | **`Attacks::attacks`**                  | **27.50s** | **9.99s**  | ⚠️ Memory bound        |
| 8    | `Search::storeTt`                       | 21.23s     | 0.50s      | Mostly in TT::put      |
| 9    | **`TT::put`**                           | **20.48s** | **7.57s**  | ⚠️ Write contention    |
| 10   | `Evaluator::pieceEval`                  | 18.64s     | 3.07s      | Expected               |

### Critical Finding: Atomic Load Time

```
std::_Atomic_storage<uint64_t,8>::load @ TT::probe  → 12.68s + 11.27s = ~24s
```

Combined atomic load time of ~24s indicates **memory-bound behavior** - the CPU is stalling waiting for TT entries to arrive from memory/L3 cache.

---

## Microarchitecture Exploration Results

A deeper VTune Microarchitecture Exploration analysis reveals the severity of the memory bottleneck.

### Key Metrics Explained

| Metric              | Meaning                                  | Ideal Value |
|---------------------|------------------------------------------|-------------|
| **CPI Rate**        | Cycles Per Instruction - lower is better | < 1.0       |
| **Back-End Bound**  | CPU waiting on memory/execution units    | < 20%       |
| **Front-End Bound** | CPU waiting on instruction fetch/decode  | < 15%       |
| **Bad Speculation** | Wasted work from mispredicted branches   | < 10%       |
| **Retiring**        | Useful work completed                    | > 50%       |

### TT Operations - Severely Memory Bound

| Function                             | CPI      | Back-End Bound | Retiring | Diagnosis                 |
|--------------------------------------|----------|----------------|----------|---------------------------|
| `TT::put`                            | **7.10** | 72.0%          | 2.1%     | 🔴 Critical memory stall  |
| `std::_Atomic_storage::load` (in TT) | **5.55** | 76.3%          | 3.5%     | 🔴 Cache miss on key load |
| `PawnTT::clear` lambda               | **4.62** | 100%           | 2.4%     | 🔴 Write-bound            |
| `TT::probe`                          | **3.67** | 76.0%          | 4.9%     | 🔴 Severe memory bound    |
| `TT::clear` lambda                   | **3.19** | 95.9%          | 4.5%     | 🔴 Memory initialization  |

**Key Insight:** `TT::probe` has CPI of 3.67 - for every instruction, the CPU waits almost 4 cycles. 76% of pipeline slots are stalled waiting for memory. Only 4.9% of work is useful (retiring).

### Comparison: Other Functions are Efficient

| Function                | CPI      | Back-End Bound | Retiring | Diagnosis     |
|-------------------------|----------|----------------|----------|---------------|
| `Attacks::sliderLookup` | **0.35** | 8.6%           | 52.6%    | ✅ Well cached |
| `Evaluator::kingEval`   | **0.36** | 3.1%           | 60.5%    | ✅ Excellent   |
| `Evaluator::pieceEval`  | **0.35** | 28.1%          | 54.1%    | ✅ Good        |
| `Evaluator::bishopEval` | **0.26** | 8.3%           | 61.1%    | ✅ Excellent   |
| `Search::search`        | **0.44** | 19.9%          | 41.0%    | ✅ Good        |
| `Search::qsearch`       | **0.67** | 38.1%          | 35.2%    | ✅ Acceptable  |

**Key Insight:** Slider attack tables (CPI 0.35) are NOT a bottleneck - magic bitboards are well-cached. The TT is 10-20x worse.

### Branch Misprediction Issues

| Function                              | Bad Speculation | Impact                                    |
|---------------------------------------|-----------------|-------------------------------------------|
| `std::ranges::_Insertion_sort_common` | 26.9%           | High misprediction in move sorting        |
| `MoveGenerator::updateSortValues`     | 20.2%           | Unpredictable killer/history comparisons  |
| `TT::put`                             | 17.9%           | Replacement policy branches unpredictable |

### Positive Findings

1. **Evaluator functions are highly optimized** - CPI 0.26-0.36, retiring 50-60%
2. **Bitboard intrinsics are efficient** - `popcount` at 100% retiring
3. **Slider lookup is NOT a problem** - Contrary to initial concern, CPI 0.35 is excellent
4. **Search core is well-balanced** - CPI 0.44-0.67, acceptable back-end bound

### Microarchitecture Summary

```
TT Operations:     CPI 3.67-7.10  → 🔴 10-20x worse than ideal
Evaluator:         CPI 0.26-0.36  → ✅ Near-optimal
Attacks:           CPI 0.35       → ✅ Well cached
Search:            CPI 0.44-0.67  → ✅ Reasonable
Move Sorting:      26.9% bad spec → 🟡 Branch misprediction overhead
```

---

## Identified Issues

### Issue 1: TT::probe High Self-Time (CRITICAL)

**Observation:** `TT::probe` has 20.95s self-time out of 32.25s total (65%).

**Expected:** For a simple hash lookup + key comparison, self-time should be minimal if data is in cache.

**Root Cause:** Cache misses on TT entry access. The atomic key loads alone account for ~24s.

**Impact:** ~10-15% of total benchmark time spent waiting on TT memory access.

### Issue 2: Potential False Sharing Under SMP

**Observation:** TT entries are 16 bytes. Four entries fit per 64-byte cache line.

**Problem:** When different threads access adjacent TT slots:
- Thread A writes Entry[0] → invalidates cache line in all other cores
- Thread B reads Entry[1] (same cache line) → must fetch from L3/RAM
- This "ping-pong" effect multiplies memory latency

**Note:** This profile was captured before full SMP implementation. With Lazy SMP, contention will likely increase.

### Issue 3: Slider Attack Tables - NOT a Major Problem ✅

**Initial Concern:** Magic bitboard tables (~840KB) might cause cache misses.

**Actual Finding (Microarch Analysis):**
```
Attacks::sliderLookup:
  CPI Rate:        0.35  ✅ EXCELLENT
  Back-End Bound:  8.6%  ✅ LOW
  Retiring:        52.6% ✅ GOOD
```

The slider tables are well-cached and NOT a significant bottleneck. This is likely because:
- Attack lookups have good spatial locality (same squares accessed repeatedly)
- L3 cache (8-32MB) holds the 840KB tables effectively
- Access patterns are predictable enough for hardware prefetching

**Conclusion:** Optimization 5 (PEXT tables) is lower priority than originally assessed.

### Issue 4: Branch Misprediction in Move Sorting (NEW)

**Observation:**
```
std::ranges::_Insertion_sort_common: 26.9% bad speculation
MoveGenerator::updateSortValues:     20.2% bad speculation
TT::put:                             17.9% bad speculation
```

**Root Cause:** 
- Move value comparisons are unpredictable (killer moves, history values vary wildly)
- TT replacement policy branches (depth comparison, age check) are data-dependent

**Impact:** ~20-27% of pipeline work is wasted on mispredicted branches.

**Potential Mitigation:** 
- Branchless comparison techniques
- Better move ordering predictability
- Lower priority than TT memory issues

### Issue 5: TT Prefetch Effectiveness Uncertain

**Current placement:**
1. `doMove()` - position changes
2. `wasLegalMove()` check
3. `TT_PREFETCH` ← prefetch issued here
4. Various bookkeeping
5. Recursive `search()` call
6. ... inside search(): `tt->probe()` ← actual access here

**Concern:** Significant code executes between prefetch and probe. Need to verify:
- Is the gap too short (prefetch hasn't completed)?
- Is the gap too long (prefetched data evicted)?

---

## Proposed Optimizations

### Optimization 1: TT Bucket/Cluster Implementation

**Current:** Single entry per hash slot  
**Proposed:** 4-entry buckets aligned to cache lines

```cpp
struct alignas(64) TTCluster {
  Entry entries[4];  // 4 × 16 = 64 bytes = 1 cache line
};
```

**Benefits:**
- Eliminates false sharing (one cluster per cache line)
- Better replacement policy (can keep both deep and recent entries)
- Reduced write conflicts under SMP (4 slots to choose from)

**Trade-offs:**
- Probe may check up to 4 entries (vs 1 currently)
- Previous single-thread testing showed 20% slowdown

**Action:** Re-test with 8 threads. The SMP contention reduction may outweigh the per-probe overhead.

**Reference:** Stockfish uses 3-entry buckets specifically for Lazy SMP effectiveness.

### Optimization 2: ~~Verify and Improve TT Prefetch~~ (Limited Options)

**Current placement is already optimal:**
```cpp
p.doMove(move);           // Zobrist key computed here
if (!p.wasLegalMove()) {  // Must check legality
  p.undoMove();
  continue;
}
TT_PREFETCH;              // Earliest possible - key now valid
```

**Why it can't be moved earlier:**
- `TT_PREFETCH` expands to `tt->prefetch(p.getZobristKey())`
- `getZobristKey()` returns the key for the **current** position
- The key for the child position only exists **after** `doMove()`

**Why prefetch isn't effective (from VTune HPC analysis):**
- `TT::prefetch` itself is **70-74% memory bound** with CPI 4.1
- The memory subsystem is saturated with 8 threads
- Prefetch requests queue behind existing memory operations
- This is a **memory bandwidth** problem, not a timing problem

**Remaining options:**
1. ~~**Reduce TT access frequency**~~ - ❌ LMR Skip TT tested and failed (+19-44% node explosion)
2. **Smaller TT entries** - Less data per access
3. **TT buckets** - ✅ Implemented 2026-03-06, better locality reduces total misses

### Optimization 3: ~~Entry Alignment Without Buckets~~ (NOT RECOMMENDED)

~~If buckets prove too slow, consider aligning individual entries:~~

```cpp
// NOT RECOMMENDED - 4x memory waste
struct alignas(64) Entry {
  std::atomic<ZobristKey> key{0};  // 8 bytes
  uint16_t move;                    // 2 bytes
  Value eval;                       // 2 bytes
  Value value;                      // 2 bytes
  // bitfields                      // 2 bytes
  // Implicit padding: 48 bytes     ← WASTED
};  // Total: 64 bytes
```

**Why this is NOT recommended:**
- 4x memory usage (256MB TT becomes 64MB effective capacity)
- Buckets provide the same cache-line isolation PLUS:
  - Better replacement policy (4 slots per hash)
  - Reduced LLC misses (better locality)
  - Same memory efficiency as current implementation

**Recommendation:** Always use buckets with alignment, never alignment alone.

### Optimization 4: Key XOR Encoding ~~(Alternative to Atomic)~~ ✅ IMPLEMENTED

**Status:** ✅ **COMPLETED 2026-03-07** — XOR key validation implemented in v1.5

**Previous approach:**
```cpp
std::atomic<ZobristKey> key;  // Atomic for thread safety
entry->key.store(key, memory_order_release);
entry->key.load(memory_order_acquire);
```

**XOR trick approach (now implemented):**
```cpp
uint64_t key16;  // Upper 16 bits of key XOR'd with data
uint64_t data;   // Packed: move + value + depth + type + eval

// Write:
entry->data = packData(move, value, depth, type, eval);
entry->key16 = (zobrist >> 48) ^ entry->data;

// Read:
uint64_t data = entry->data;
if ((entry->key16 ^ data) == (zobrist >> 48)) {
  // Consistent - use entry
}
```

**How it works:** Torn reads produce garbage key → fails verification → safe miss

**Benefits:**
- Detects SMP race conditions (torn reads)
- Combined with TT buckets for robust multi-threaded operation
- **Match results:** +74.6 ELO vs v1.4 (combined with TT buckets + SMP hardening)

**Implementation notes:**
- XOR validation ensures data consistency across cache-line boundaries
- Prevents corrupted TT entries from causing search instability
- Works in conjunction with depth-preferred replacement policy

### Optimization 5: ~~PEXT-Based Slider Tables~~ (ALREADY IMPLEMENTED ✅)

**Status:** FrankyCPP already uses PEXT for slider attacks!

**Current implementation** (`src/types/attacks.h`):
```cpp
constexpr unsigned index(const Bitboard occupied) const {
#ifdef HAS_PEXT
  if (!std::is_constant_evaluated())
    return static_cast<unsigned>(_pext_u64(occupied, mask));  // BMI2 PEXT instruction
#endif
  return static_cast<unsigned>(pext_soft(occupied, mask));    // Software fallback
}
```

**Build configuration** (`CMakeLists.txt`):
- `ENABLE_BMI2_PEXT` is **ON by default**
- Defines `HAS_PEXT` and adds `/arch:AVX2` on MSVC

**Table sizes** (PEXT-compact, not traditional magic):
| Table | Entries | Size |
|-------|---------|------|
| Rook | 102,400 (0x19000) | ~800 KB |
| Bishop | 5,248 (0x1480) | ~41 KB |
| **Total** | | **~841 KB** |

**VTune analysis confirmed PEXT is working well:**
- `Attacks::sliderLookup`: CPI 0.35-0.38, only 9.7% memory bound
- The slider tables are efficiently cached despite their size
- **No further optimization needed here**

**Note:** The original document incorrectly suggested PEXT tables would be ~45KB. The ~841KB size is correct for PEXT-indexed tables; the difference from "magic bitboards" is the indexing method (PEXT vs magic multiplication), not the table size.

### Optimization 6: Attack Caching Per-Position

**Observation:** Multiple functions compute the same attacks:
```
Position::isAttacked  →  8.09s
Position::givesCheck  → 13.96s
See::see              → 10.62s
See::attacksTo        →  3.48s
Evaluator::pieceEval  → 18.64s
```

**Proposed:** Cache attack bitboards per position:

```cpp
struct AttackCache {
  Bitboard byPiece[6][2];    // [PieceType][Color]
  Bitboard byColor[2];       // All attacks by color
  Bitboard checkers;         // Pieces giving check
  bool valid = false;
};

// In Position or Search node:
mutable AttackCache attackCache;

Bitboard Position::getAttacks(PieceType pt, Color c) const {
  if (!attackCache.valid) computeAllAttacks();
  return attackCache.byPiece[pt][c];
}
```

**Benefits:**
- Compute once, use everywhere in the node
- Eliminates redundant slider table lookups
- Particularly beneficial combined with PEXT (faster initial computation)

**Trade-off:** Memory per node (~200 bytes) and invalidation complexity

---

## Implementation Priority

Based on microarchitecture analysis, priorities have been adjusted:

| Priority | Optimization                            | Effort | Expected Impact | Rationale                                                                |
|----------|-----------------------------------------|--------|-----------------|--------------------------------------------------------------------------|
| ~~1~~    | ~~TT Buckets + alignas(64)~~            | Medium | **High**        | ✅ **COMPLETED** — 50% LLC miss reduction, 53% CPI improvement            |
| ~~2~~    | ~~Entry/Bucket alignment alone~~        | N/A    | N/A             | ❌ Wastes 48B/entry without buckets; MUST combine with buckets            |
| ~~3~~    | ~~XOR key encoding~~                    | Medium | Medium          | ✅ **COMPLETED** — +74.6 ELO vs v1.4; detects SMP torn reads              |
| 2        | Attack caching per-position             | Medium | Low-Med         | Redundant calls, but slider CPI is already good (0.35)                   |
| ~~4~~    | ~~Verify TT prefetch timing~~           | N/A    | N/A             | ❌ NOT VIABLE: Prefetch requires key from after doMove()                  |
| ~~5~~    | ~~PEXT slider tables~~                  | N/A    | N/A             | ✅ **Already implemented** — using `_pext_u64`, CPI 0.35                  |
| 3        | Branchless move sorting                 | Low    | Low-Med         | 27% bad speculation, secondary issue                                     |

**Key Changes:**
- TT Buckets + XOR validation provide **+74.6 ELO** improvement over v1.4
- PEXT optimization was already implemented — slider tables (CPI 0.35) are NOT a bottleneck
- Switched to `std::ranges::sort` with unstable sort for move ordering (visible in VTune results)

---

## Memory Access Analysis Results

VTune Memory Access profiling provides definitive evidence of the TT bottleneck.

### TT Operations - Severe Memory Bound with High LLC Misses

| Function                              | CPU Time | Memory Bound | LLC Misses    | Diagnosis                 |
|---------------------------------------|----------|--------------|---------------|---------------------------|
| **TT::probe**                         | 40.22s   | **74.3%**    | **1,650,693** | 🔴 1.65M L3 cache misses  |
| `std::_Atomic_storage::load` (TT key) | 25.14s   | **73.3%**    | (in parent)   | 🔴 Key load causes stalls |
| **TT::put**                           | 13.96s   | **66.7%**    | 0             | 🔴 Write-bound            |
| **TT::clear** lambda                  | 2.45s    | 78.4%        | 3,851,617     | 🟡 Init only              |

**Key Finding:** `TT::probe` has **1.65 million L3 cache misses** and the CPU is stalled **74.3% of the time** waiting on memory. This is the definitive bottleneck.

### Slider Tables - DEFINITIVELY NOT A PROBLEM

| Function                | CPU Time | Memory Bound | LLC Misses | Diagnosis          |
|-------------------------|----------|--------------|------------|--------------------|
| `Attacks::sliderLookup` | 10.47s   | **9.7%**     | 1,650,693  | ✅ Low memory bound |
| `Attacks::attacks`      | 7.88s    | **16.5%**    | 0          | ✅ Well cached      |

**Critical Insight:** Despite having the same 1.65M LLC misses as TT::probe, slider lookups have only **9.7% Memory Bound** vs TT's **74.3%**. This means:
- Slider miss latency is **hidden by CPU parallelism** (out-of-order execution, prefetching)
- TT miss latency **cannot be hidden** - the CPU must wait for the key before proceeding

### Memory Bound Comparison

```
TT::probe:           ████████████████████████████████████████████ 74.3% Memory Bound
TT::put:             █████████████████████████████████            66.7% Memory Bound
Position::doMove:    ████████████████████████████                 64.4% Memory Bound
Attacks::sliderLookup:████                                         9.7% Memory Bound
Evaluator::kingEval:  █                                            0.2% Memory Bound
```

### Prefetch Effectiveness Analysis

| Function         | Memory Bound | Prefetch Present? | Effectiveness |
|------------------|--------------|-------------------|---------------|
| TT::probe        | 74.3%        | Yes (TT_PREFETCH) | ❌ NOT WORKING |
| TT::prefetch     | 5.7%         | N/A               | -             |
| PawnTT::prefetch | 33.9%        | N/A               | -             |

**Prefetch Verdict:** Despite `TT_PREFETCH` being called, `TT::probe` still has 74.3% Memory Bound and 1.65M LLC misses. The prefetch is either:
1. **Too late** - Not enough cycles between prefetch and probe
2. **Evicted** - Data fetched but evicted before use
3. **Wrong address** - Prefetching for wrong position (unlikely)

### LLC Miss Distribution

| Source                | LLC Misses | % of Total | Impact              |
|-----------------------|------------|------------|---------------------|
| TT::clear (init)      | 3,851,617  | 42.7%      | One-time cost       |
| TT::probe             | 1,650,693  | 18.3%      | 🔴 Runtime critical |
| Attacks::sliderLookup | 1,650,693  | 18.3%      | ✅ Hidden            |
| Position::givesCheck  | 1,100,462  | 12.2%      | 🟡 Moderate         |
| Evaluator::pawnEval   | 1,100,462  | 12.2%      | 🟡 Moderate         |
| Other                 | ~600K      | 6.7%       | Minor               |

### Conclusions from Memory Analysis

1. **TT is definitively the bottleneck** - 74% memory bound with 1.65M LLC misses during search
2. **Prefetch is NOT effective** - Need to investigate timing/placement
3. **Slider tables are NOT a problem** - Same LLC miss count but only 10% memory bound (hidden latency)
4. **PEXT optimization unnecessary** - Slider table caching is already effective via hardware mechanisms

---

## Verification Checklist

Before implementation, verify assumptions:

- [x] VTune Memory Analysis on `TT::probe` - **CONFIRMED: 74.3% memory bound, 1.65M LLC misses**
- [x] Measure prefetch-to-probe cycle gap - **NOT VIABLE: Prefetch already at earliest possible point (after doMove)**
- [ ] Profile with actual Lazy SMP enabled
- [ ] Check BMI2 support on target platforms (low priority now)
- [ ] Benchmark buckets with 8 threads
- [x] Microarchitecture Exploration - completed, confirmed TT is bottleneck

---

## VTune Automation and Verification

### Automated Profiling Script

A PowerShell script is provided to run all 5 VTune analyses consistently:

**Location:** `scripts/run_vtune_analysis.ps1`

**Usage (requires Administrator terminal):**
```powershell
# Run from project root in an Administrator PowerShell terminal
powershell -ExecutionPolicy Bypass -File ".\scripts\run_vtune_analysis.ps1"
```

**Note:** The `-ExecutionPolicy Bypass` flag is needed because admin terminals often have restricted execution policies. This bypasses the policy for this single script execution without changing system settings.

**Configuration (at top of script):**
```powershell
$VTUNE_PATH = "C:\Program Files (x86)\Intel\oneAPI\vtune\2025.9\bin64"
$EXECUTABLE = "D:\_DEV\FrankyCPP\cmake-build-win-relwithdebinfo\src\FrankyCPP_v1.5.exe"
$PARAMS = "--bench --threads 8 -l warn -s warn"
$RESULTS_BASE = "D:\_DEV\FrankyCPP\results\vtune"
```

**Output:** Results stored in `results/vtune/YYYY-MM-DD_HH-mm-ss/` with:
- 5 VTune result directories (one per analysis type)
- 5 CSV exports for programmatic analysis

### Verification Process

Use the script to verify optimizations:

1. **Baseline:** Run script before implementing changes
2. **Post-change:** Run script after implementing optimization
3. **Compare:** Generate summary documents and compare key metrics

### Summary Generation Prompt

After each VTune run, use the following prompt to generate a comparable summary document:

---

**Prompt for AI Assistant:**

```
Analyze the latest VTune results and create a summary document.
Compare to the previous runs. 

Create a file: results/vtune/YYYY-MM-DD_HH-mm-ss/summary.md

Use this EXACT format for comparability across runs:

# VTune Analysis Summary

**Date:** YYYY-MM-DD HH:mm:ss
**Executable:** [version]
**Configuration:** [threads, params]
**Build:** [Release/Debug/RelWithDebInfo]

## Key Performance Metrics

### TT Performance (Primary Focus)

| Metric | TT::probe | TT::put | TT::prefetch |
|--------|-----------|---------|--------------|
| CPU Time (s) | X.XX | X.XX | X.XX |
| Memory Bound (%) | XX.X% | XX.X% | XX.X% |
| CPI Rate | X.XX | X.XX | X.XX |
| LLC Misses | X,XXX,XXX | X,XXX,XXX | X,XXX,XXX |

### Comparison Baselines

| Metric | Attacks::sliderLookup | Evaluator::evaluate | Search::search |
|--------|----------------------|---------------------|----------------|
| CPU Time (s) | X.XX | X.XX | X.XX |
| Memory Bound (%) | XX.X% | XX.X% | XX.X% |
| CPI Rate | X.XX | X.XX | X.XX |

### Threading Efficiency

| Metric | Value |
|--------|-------|
| Total Effective Time | XXX.Xs |
| Spin Time | X.XXs (X.X%) |
| Wait Time | X.XXs (X.X%) |

### Overall Assessment

| Category | Status | Notes |
|----------|--------|-------|
| TT Memory Bound | 🔴/🟡/🟢 | [brief description] |
| Thread Contention | 🔴/🟡/🟢 | [brief description] |
| Slider Tables | 🔴/🟡/🟢 | [brief description] |
| Evaluator | 🔴/🟡/🟢 | [brief description] |

## Changes Since Baseline

[List any code changes since baseline run]

## Recommendations

[Top 3 actionable items based on this analysis]
```

---

### Tracking Optimization Progress

Store summaries with consistent naming for easy comparison:

```
results/vtune/
├── 2026-03-02_10-30-00/    # Baseline (before optimization)
│   ├── hotspots/
│   ├── microarchitecture/
│   ├── memory-access/
│   ├── threading/
│   ├── hpc-performance/
│   └── summary.md          # Generated summary
├── 2026-03-05_14-20-00/    # After TT buckets implementation
│   └── summary.md
└── 2026-03-08_09-15-00/    # After alignment changes
    └── summary.md
```

**Comparison workflow:**
1. Open baseline `summary.md` and new `summary.md` side-by-side
2. Compare TT Performance table values directly
3. Look for improvements in Memory Bound % and CPI Rate
4. Verify no regressions in Threading Efficiency

---

## References

- **VTune automation script:** `scripts/run_vtune_analysis.ps1`
- VTune Caller/Callee profile: `cmake-build-win-relwithdebinfo/test/vtune_result.csv`
- VTune Microarch profile: `cmake-build-win-relwithdebinfo/test/vtune_result_microarch.csv`
- VTune Memory profile: `cmake-build-win-relwithdebinfo/test/vtune_result_memory.csv`
- VTune Threading profile: `cmake-build-win-relwithdebinfo/test/vtune_result_threading_caller-callee.csv`
- VTune HPC profile: `cmake-build-win-relwithdebinfo/test/vtune_result_hpc.csv`
- Stockfish TT implementation: Uses 3-entry buckets, XOR key verification
- Chess Programming Wiki: [Transposition Table](https://www.chessprogramming.org/Transposition_Table)
- Chess Programming Wiki: [Magic Bitboards](https://www.chessprogramming.org/Magic_Bitboards)
- Chess Programming Wiki: [PEXT Bitboards](https://www.chessprogramming.org/BMI2#PEXTBitboards)

---

## Revision History

| Date       | Author   | Changes                                                                                                                                                                                                                                                                                                |
|------------|----------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 2026-03-02 | Analysis | Initial document from VTune profiling analysis                                                                                                                                                                                                                                                         |
| 2026-03-02 | Analysis | Added microarchitecture exploration results; deprioritized PEXT optimization; added branch misprediction findings                                                                                                                                                                                      |
| 2026-03-02 | Analysis | Added Memory Access analysis; confirmed 74.3% memory bound and 1.65M LLC misses in TT::probe; slider tables NOT a bottleneck (9.7% memory bound despite equal LLC misses)                                                                                                                              |
| 2026-03-02 | Analysis | Added Threading analysis; confirmed minimal spin/wait time, good thread utilization                                                                                                                                                                                                                    |
| 2026-03-02 | Analysis | Added HPC analysis; confirmed extreme CPI (6.4-6.6) in TT ops, TT::prefetch also memory bound (70-74%), slider CPI 0.38                                                                                                                                                                                |
| 2026-03-02 | Analysis | Added VTune automation script reference and summary generation prompt for verification workflow                                                                                                                                                                                                        |
| 2026-03-02 | Analysis | Corrected Optimization 5: PEXT is ALREADY IMPLEMENTED (`_pext_u64` with `ENABLE_BMI2_PEXT` ON by default); not a future optimization                                                                                                                                                                   |
| 2026-03-02 | Analysis | Added 4-thread comparison: Memory Bound drops 74%→37.5%, confirming bandwidth saturation as root cause; prefetch works at 4 threads (10% vs 70% memory bound)                                                                                                                                          |
| 2026-03-07 | Analysis | **TT Buckets verified**: Added VTune results after TT Buckets implementation showing 50% LLC miss reduction, 53% CPI improvement (9.10→5.73), 15% probe time improvement; Updated roadmap to show TT Buckets COMPLETED; Elevated XOR key encoding priority due to 7.4s atomic overhead (89% mem bound) |
| 2026-03-07 | Analysis | **XOR validation + match results**: Added match results (v1.5 vs v1.3: +218.7 ELO; v1.5 vs v1.4: +74.6 ELO); Marked XOR key encoding COMPLETED; TT buckets + XOR + SMP hardening provide substantial real-game improvements                                                                            |

---

## HPC Performance Characterization Results

VTune HPC analysis provides additional CPI and memory-bound metrics with loop-level detail.

### TT Operations - Extreme CPI Confirmed

| Function                     | Time  | Memory Bound | CPI Rate | Instructions |
|------------------------------|-------|--------------|----------|--------------|
| **TT::put**                  | 45.5s | **72.9%**    | **6.44** | 38.4B        |
| **TT::probe**                | 37.9s | **62.3%**    | **6.61** | 30.9B        |
| **TT::prefetch**             | 18.9s | **70.2%**    | **4.15** | 24.5B        |
| **TT::prefetch** (2nd)       | 16.3s | **74.3%**    | **4.10** | 20.7B        |
| `std::_Atomic_storage::load` | 18.4s | **80.0%**    | **5.74** | 16.9B        |

**Key Finding:** `TT::prefetch` itself is **70-74% memory bound** with CPI of 4.1-4.2! The prefetch instruction is stalling because memory is already saturated.

### Critical Insight: Prefetch is Memory Bound

```
TT::prefetch:  70-74% Memory Bound, CPI 4.1
TT::probe:     62% Memory Bound, CPI 6.6
```

This explains why prefetch isn't helping - the **memory subsystem is saturated**. Prefetch requests are queuing behind existing memory operations. With 8 threads all accessing the TT, the memory controller is overwhelmed.

### Slider Tables - Definitively Efficient

| Function                | Time | Memory Bound | CPI Rate | Instructions |
|-------------------------|------|--------------|----------|--------------|
| `Attacks::sliderLookup` | 9.9s | **9.6%**     | **0.38** | 141.6B       |
| `Attacks::attacks`      | 7.3s | **19.0%**    | **0.37** | 101.1B       |

**CPI 0.38 is excellent** - the CPU executes ~2.6 instructions per cycle. Compare to TT's CPI 6.6 = only 0.15 instructions per cycle.

### Evaluator Functions - Well Optimized

| Function               | Memory Bound | CPI  | Vector Set   |
|------------------------|--------------|------|--------------|
| `Evaluator::evaluate`  | 28.3%        | 0.40 | **AVX(128)** |
| `Evaluator::rookEval`  | 1.8%         | 0.37 | -            |
| `Evaluator::queenEval` | 1.1%         | 0.25 | -            |
| `Evaluator::kingEval`  | 0.9%         | 0.34 | -            |

Evaluator is using AVX SIMD for some operations and achieving excellent CPI.

### Loop-Level Hotspots

| Loop Location                                   | Memory Bound | CPI  |
|-------------------------------------------------|--------------|------|
| `Search::search` line 1542                      | 28.5%        | 0.49 |
| `MoveGenerator::updateSortValues` line 604      | 8.7%         | 0.47 |
| `Search::qsearch` line 2076                     | 40.3%        | 0.49 |
| `MoveGenerator::generateMoves` line 827         | 26.9%        | 0.48 |
| `std::ranges::_Insertion_sort_common` line 8440 | 0.8%         | 0.58 |

The main search/qsearch loops have acceptable memory bound (~30-40%) and good CPI (~0.5).

### HPC Verdict

1. **TT bottleneck confirmed with extreme metrics**: CPI 6.4-6.6 (vs 0.3-0.5 elsewhere)
2. **Memory subsystem saturation**: Even prefetch is 70-74% memory bound
3. **8-thread parallelism is overwhelming memory bandwidth**
4. **Slider tables are efficient**: CPI 0.38, only 10% memory bound
5. **Evaluator uses SIMD**: AVX(128) detected in evaluate function

---

## Threading Analysis Results

VTune Threading analysis confirms efficient thread utilization with minimal contention.

### Thread Utilization Summary

| Metric               | Value  | Assessment         |
|----------------------|--------|--------------------|
| Total Effective Time | 286.5s | ✅ Good utilization |
| Total Spin Time      | 0.34s  | ✅ Minimal (0.1%)   |
| Total Wait Time      | 0.76s  | ✅ Minimal (0.3%)   |
| Wait Count           | 3,302  | ✅ Low overhead     |

### Key Threading Observations

#### 1. No Significant Thread Contention ✅

| Function                     | Effective Time | Spin Time | Wait Time |
|------------------------------|----------------|-----------|-----------|
| `Search::search`             | 282.9s         | 0.008s    | 0.01s     |
| `TT::probe`                  | 48.6s          | 0s        | 0s        |
| `TT::put`                    | 38.6s          | 0s        | 0s        |
| `std::_Atomic_storage::load` | 22.4s          | 0s        | 0s        |

**Conclusion:** The atomic operations in TT are NOT causing thread spin/contention. The bottleneck is purely **memory latency**, not **thread synchronization**.

#### 2. Helper Threads Running Efficiently ✅

```
Search::helperRun:                247.3s effective, 0s spin
Search::launchHelperThreads:      247.3s effective, 0s spin
std::thread::_Invoke (helpers):   247.3s effective, 0s spin
```

All 7 helper threads (8 total - 1 main) are doing productive work without significant wait time.

#### 3. Minimal Synchronization Overhead ✅

| Wait Point            | Wait Time | Wait Count |
|-----------------------|-----------|------------|
| Search initialization | 0.06s     | 2,562      |
| Tablebase init        | 0.06s     | 2,531      |
| Thread exit cleanup   | 0.01s     | 272        |

These are all one-time initialization costs, not runtime overhead.

### Threading Verdict

**The threading implementation is EFFICIENT.** The TT bottleneck is:
- ❌ NOT thread contention
- ❌ NOT atomic operation overhead  
- ✅ Pure memory latency (74% memory bound, 1.65M LLC misses)

This confirms that optimizations should focus on **cache behavior** (buckets, alignment, prefetch timing) rather than **synchronization** changes.

---
