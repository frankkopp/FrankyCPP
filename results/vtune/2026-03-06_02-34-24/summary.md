# VTune Analysis Summary

**Date:** 2026-03-06 02:34:24  
**Executable:** FrankyCPP v1.5  
**Configuration:** Single thread, TT=256MB, No Book  
**Build:** Release (MSVC)

## Key Performance Metrics

### TT Performance (Primary Focus)

| Metric               | TT::probe | TT::put | TT::prefetch (combined) |
|----------------------|-----------|---------|-------------------------|
| CPU Time (s)         | 19.23     | 31.60   | 20.41                   |
| Memory Bound (%)     | 58.9%     | 62.7%   | 64.8%                   |
| L3 Bound (%)         | 52.4%     | 70.8%   | 66.8%                   |
| DRAM Bound (%)       | 9.9%      | 0.0%    | 0.1%                    |
| CPI Rate             | 4.31      | 6.75    | 4.13                    |
| LLC Miss Count       | 550,231   | 0       | 0                       |
| Instructions Retired | 23.6B     | 23.9B   | 25.8B                   |

**TT Total: 71.24s (~57% of total CPU time)**

### Atomic Operations (TT Synchronization)

| Metric           | atomic::store | atomic::load |
|------------------|---------------|--------------|
| CPU Time (s)     | 6.12          | 4.16         |
| Memory Bound (%) | 83.1%         | 72.6%        |
| CPI Rate         | 6.65          | 2.91         |

### Comparison Baselines

| Metric           | Attacks::sliderLookup | Evaluator::evaluate | Search::search | Search::qsearch |
|------------------|-----------------------|---------------------|----------------|-----------------|
| CPU Time (s)     | 7.16                  | 1.83                | 2.62           | 1.74            |
| Memory Bound (%) | 9.2%                  | 55.5%               | 38.0%          | 34.3%           |
| CPI Rate         | 0.39                  | 0.56                | 0.87           | 0.80            |
| µArch Usage (%)  | 44.2%                 | 27.8%               | 34.0%          | 25.5%           |

### stopConditions() Optimization Result

| Metric       | Before (02-34-24) | After (02-34-24) | Improvement       |
|--------------|-------------------|------------------|-------------------|
| CPU Time (s) | 5.89              | 2.42             | **-59%**          |
| Instructions | 23.8B             | 8.6B             | **-64%**          |
| CPI Rate     | 1.28              | 1.50             | +17% (acceptable) |

### Threading Efficiency

| Metric                  | Value                        |
|-------------------------|------------------------------|
| Total Effective Time    | 125.4s                       |
| Spin Time               | 0.0s (0.0%)                  |
| Overhead Time           | 0.0s (0.0%)                  |
| Search::thread accessor | 3.15s (memory-bound waiting) |

### Overall Assessment

| Category          | Status       | Notes                                     |
|-------------------|--------------|-------------------------------------------|
| TT Memory Bound   | 🔴 Critical  | 57% of CPU time, L3 bound 52-71%, CPI 4-7 |
| stopConditions    | 🟢 Optimized | Reduced 59% (5.89s → 2.42s)               |
| Thread Contention | 🟢 Good      | No spin time, minimal overhead            |
| Slider Tables     | 🟢 Excellent | 9.2% memory bound, CPI 0.39               |
| Evaluator         | 🟡 Moderate  | 55% memory bound, but low CPI 0.56        |
| Atomic Operations | 🔴 High      | 10.3s total, 83% memory bound             |

## Top 20 Hotspots

| Rank | Function               | CPU Time (s) | % Total | Memory Bound |
|------|------------------------|--------------|---------|--------------|
| 1    | TT::put                | 33.62        | 26.8%   | 62.7%        |
| 2    | TT::probe              | 20.52        | 16.4%   | 58.9%        |
| 3    | TT::prefetch           | 20.71        | 16.5%   | 64.8%        |
| 4    | sliderLookup           | 6.85         | 5.5%    | 9.2%         |
| 5    | Attacks::attacks       | 4.93         | 3.9%    | 22.3%        |
| 6    | atomic::store          | 4.21         | 3.4%    | 90.6%        |
| 7    | atomic::load           | 3.15         | 2.5%    | 67.1%        |
| 8    | pieceEval              | 2.76         | 2.2%    | 65.4%        |
| 9    | Search::search         | 2.64         | 2.1%    | 38.0%        |
| 10   | stopConditions         | 2.42         | 1.9%    | 20.9%        |
| 11   | Search::qsearch        | 2.33         | 1.9%    | 34.3%        |
| 12   | atomic::store #2       | 2.29         | 1.8%    | 75.5%        |
| 13   | Evaluator::evaluate    | 1.87         | 1.5%    | 55.5%        |
| 14   | PawnTT::prefetch       | 1.65         | 1.3%    | 47.1%        |
| 15   | PawnTT::probe          | 1.58         | 1.3%    | 39.7%        |
| 16   | getNextPseudoLegalMove | 1.54         | 1.2%    | 17.6%        |
| 17   | generatePawnMoves      | 1.48         | 1.2%    | 67.7%        |
| 18   | fillOnDemandMoveList   | 1.48         | 1.2%    | 29.2%        |
| 19   | updateSortValues       | 1.42         | 1.1%    | 11.8%        |
| 20   | givesCheck             | 1.41         | 1.1%    | 9.1%         |

## Changes Since Baseline

### Code Changes Applied (2026-03-06)

1. **stopConditions() call reduction** - Removed 7 redundant `stopConditions()` checks from `search()` and `qsearch()`:
   - Removed after NMP (line ~1438)
   - Removed after NMP verification (line ~1464)
   - Removed after IID (line ~1527)
   - Removed after singular extension search (line ~1655)
   - Simplified re-search condition to only check `isTimeAlmostUp()` (line ~1861)
   - Removed after move loop in `search()` (line ~1890)
   - Removed after move loop in `qsearch()` (line ~2169)

2. **Result:** 59% reduction in `stopConditions()` CPU time, 64% fewer instructions

## Recommendations

### 1. 🔴 TT Optimization (Highest Priority)
The TT operations consume **57% of total CPU time** with severe L3 cache pressure:
- Consider reducing TT entry size (currently appears to be causing L3 thrashing)
- Investigate prefetch timing - prefetch may be too early or too late
- Consider bucket size optimization or alternative replacement schemes
- Profile with different TT sizes (64MB, 128MB, 512MB) to find optimal size for cache hierarchy

### 2. 🔴 Atomic Operations (High Priority)
Atomic store/load operations add **10.3s overhead** (8% of total):
- Review if all atomic operations are necessary
- Consider lock-free or batch update strategies
- Investigate memory ordering requirements (seq_cst vs relaxed)

### 3. 🟡 Evaluator Memory Access (Medium Priority)
`Evaluator::evaluate` and `pieceEval` show 55-65% memory bound:
- Consider caching intermediate evaluation results
- Review data locality in evaluation tables
- Profile piece-square table access patterns

## Historical Comparison

| Run Date             | stopConditions | TT::probe | TT::put | Notes                      |
|----------------------|----------------|-----------|---------|----------------------------|
| 2026-03-02 17:24     | 4.72s          | -         | -       | v1.4 baseline              |
| 2026-03-03 13:28     | 6.56s          | -         | -       | v1.4                       |
| 2026-03-06 01:54     | 5.89s          | -         | -       | v1.5 pre-optimization      |
| **2026-03-06 02:34** | **2.42s**      | 19.23s    | 31.60s  | **v1.5 post-optimization** |

---
*Generated from VTune analysis results*
