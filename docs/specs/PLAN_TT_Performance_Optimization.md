# TT and Memory Performance Optimization Plan

**Status:** Analysis Complete, Implementation Pending  
**Created:** 2026-03-02  
**Last Updated:** 2026-03-03  
**Priority:** Medium-High (significant performance impact under SMP)

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

## Executive Summary

### Key Conclusions from VTune Analysis (5 Test Types)

Comprehensive VTune profiling (Hotspots, Microarchitecture, Memory Access, Threading, HPC) on an 8-thread benchmark reveals:

| Finding                             | Evidence                                           | Impact                                             |
|-------------------------------------|----------------------------------------------------|----------------------------------------------------|
| **TT is the critical bottleneck**   | CPI 3.4-6.6, 62-74% memory bound, 1.65M LLC misses | 🔴 ~55s of search time in TT ops                   |
| **Memory subsystem is saturated**   | Even TT::prefetch is 70-74% memory bound           | 🔴 Prefetch cannot help when memory is overwhelmed |
| **Threading is efficient**          | 0% spin time, 0% wait time on TT atomics           | ✅ Not a synchronization problem                    |
| **Slider tables are NOT a problem** | CPI 0.35-0.38, only 9.7% memory bound              | ✅ PEXT already implemented & working               |
| **Evaluator is well optimized**     | CPI 0.25-0.40, using AVX SIMD                      | ✅ No action needed                                 |

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

| Priority | Optimization                      | Effort | Expected Impact | Rationale                                                                                                   |
|----------|-----------------------------------|--------|-----------------|-------------------------------------------------------------------------------------------------------------|
| **1**    | **TT Buckets + alignas(64)**      | Medium | 🔴 **HIGH**     | 4×16B entries = 64B cache line; eliminates false sharing; enables smarter replacement; Stockfish uses this  |
| ~~2~~    | ~~Cache-line alignment alone~~    | N/A    | N/A             | ❌ Must combine with buckets; alone wastes 48B/entry (4x memory overhead)                                    |
| **2**    | **Reduce TT access frequency**    | Medium | 🟡 Medium       | Skip TT probe in late move reductions; batch TT updates                                                     |
| **3**    | **Attack caching per-position**   | Medium | 🟢 Low          | Slider tables already efficient (CPI 0.38); diminishing returns                                             |
| ~~5~~    | ~~Earlier prefetch placement~~    | N/A    | N/A             | ❌ NOT POSSIBLE - requires zobrist key from after doMove()                                                   |
| ~~6~~    | ~~PEXT slider tables~~            | N/A    | N/A             | ✅ **Already implemented** - no action needed                                                                |
| ~~7~~    | ~~XOR key encoding~~              | N/A    | N/A             | ✅ Current atomics are efficient on x86                                                                      |
| ~~8~~    | ~~Thread synchronization~~        | N/A    | N/A             | ✅ Zero contention measured                                                                                  |

### Quick Wins (Implement First)

1. **Implement TT buckets with `alignas(64)`** - These two MUST go together:
   - 4 entries × 16 bytes = 64 bytes = exactly 1 cache line
   - `alignas(64)` ensures each bucket starts at cache line boundary
   - **Note:** `alignas(64)` alone on current 16-byte entries would waste 48 bytes per entry (4x memory overhead)
2. **Benchmark TT buckets with 8 threads** - Previous 20% slowdown was single-threaded; SMP may benefit

### Not Viable

- ~~**Move TT_PREFETCH earlier**~~ - NOT POSSIBLE: Prefetch requires `p.getZobristKey()` which is only valid after `doMove()`. Current placement is already optimal.

### Metrics to Track After Optimization

| Metric                 | Baseline (03-02) | Current (03-03) | Target | Status |
|------------------------|------------------|-----------------|--------|--------|
| TT::probe CPI          | 3.37             | 9.10            | < 2.0  | 🔴     |
| TT::probe Memory Bound | 74%              | 62.6%           | < 50%  | 🟡     |
| TT::probe CPU Time     | 42.3s            | 22.76s          | < 30s  | ✅      |
| LLC Misses (TT)        | 1.65M            | 1.10M           | < 1.0M | 🟡     |

**Note:** TT::probe CPU Time target met via Lazy SMP refactor (depth diversification). TT Buckets would further improve CPI and memory bound.

### What NOT to Optimize

- **Slider tables** - Already excellent (CPI 0.38)
- **Evaluator** - Well optimized with SIMD
- **Threading/atomics** - Zero contention
- **Move sorting branches** - Secondary issue (27% bad speculation but lower impact)

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
1. **Reduce TT access frequency** - Skip probes in some cases
2. **Smaller TT entries** - Less data per access
3. **TT buckets** - Better locality reduces total misses

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

### Optimization 4: Key XOR Encoding (Alternative to Atomic)

**Current approach:**
```cpp
std::atomic<ZobristKey> key;  // Atomic for thread safety
entry->key.store(key, memory_order_release);
entry->key.load(memory_order_acquire);
```

**XOR trick approach:**
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
- No atomic operations (plain loads/stores)
- Single 64-bit data field (one load vs multiple)
- No memory barriers on weak memory architectures

**Caveats:**
- Must pack all data into 64 bits
- XOR adds ALU cycles
- On x86, benefit is marginal (acquire/release = plain mov)
- Current code already verifies lock-free atomics

**Recommendation:** Lower priority. Current atomic approach is efficient on x86.

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

| Priority | Optimization                            | Effort | Expected Impact | Rationale                                                           |
|----------|-----------------------------------------|--------|-----------------|---------------------------------------------------------------------|
| 1        | **TT Buckets + alignas(64)** (combined) | Medium | **High**        | Primary bottleneck; 76% back-end bound; must combine for efficiency |
| ~~2~~    | ~~Entry/Bucket alignment alone~~        | N/A    | N/A             | ❌ Wastes 48B/entry without buckets; MUST combine with buckets       |
| 2        | Attack caching per-position             | Medium | Low-Med         | Redundant calls, but slider CPI is already good (0.35)              |
| ~~4~~    | ~~Verify TT prefetch timing~~           | N/A    | N/A             | ❌ NOT VIABLE: Prefetch requires key from after doMove()             |
| ~~5~~    | ~~PEXT slider tables~~                  | N/A    | N/A             | **ALREADY IMPLEMENTED** - using `_pext_u64`, CPI 0.35 ✅             |
| 6        | XOR key encoding                        | Medium | Low on x86      | Current atomic approach is efficient                                |
| 7        | Branchless move sorting                 | Low    | Low-Med         | 27% bad speculation, secondary issue                                |

**Key Change:** PEXT optimization deprioritized - microarchitecture analysis shows slider tables (CPI 0.35, 52.6% retiring) are NOT a bottleneck. TT memory access (CPI 3.67-7.10) is 10-20x worse.

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
Analyze the VTune results in [results/vtune/YYYY-MM-DD_HH-mm-ss/] and create a summary document.

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

| Date       | Author   | Changes                                                                                                                                                                   |
|------------|----------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 2026-03-02 | Analysis | Initial document from VTune profiling analysis                                                                                                                            |
| 2026-03-02 | Analysis | Added microarchitecture exploration results; deprioritized PEXT optimization; added branch misprediction findings                                                         |
| 2026-03-02 | Analysis | Added Memory Access analysis; confirmed 74.3% memory bound and 1.65M LLC misses in TT::probe; slider tables NOT a bottleneck (9.7% memory bound despite equal LLC misses) |
| 2026-03-02 | Analysis | Added Threading analysis; confirmed minimal spin/wait time, good thread utilization                                                                                       |
| 2026-03-02 | Analysis | Added HPC analysis; confirmed extreme CPI (6.4-6.6) in TT ops, TT::prefetch also memory bound (70-74%), slider CPI 0.38                                                   |
| 2026-03-02 | Analysis | Added VTune automation script reference and summary generation prompt for verification workflow                                                                           |
| 2026-03-02 | Analysis | Corrected Optimization 5: PEXT is ALREADY IMPLEMENTED (`_pext_u64` with `ENABLE_BMI2_PEXT` ON by default); not a future optimization                                      |
| 2026-03-02 | Analysis | Added 4-thread comparison: Memory Bound drops 74%→37.5%, confirming bandwidth saturation as root cause; prefetch works at 4 threads (10% vs 70% memory bound)             |

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
