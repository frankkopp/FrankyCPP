# VTune Analysis Summary

**Date:** 2026-04-09 10:25:47  
**Executable:** FrankyCPP v1.8  
**Configuration:** Multi-threaded (SMP), TT=256MB  
**Build:** Release (MSVC)

## Key Performance Metrics

### TT Performance (Primary Focus)

| Metric | TT::probe | TT::put | TT::prefetch (combined) |
|--------|-----------|---------|-------------------------|
| CPU Time (s) | 29.35 | 29.21 | 18.43 |
| Memory Bound (%) | 60.2% | 62.3% | 62.6% |
| CPI Rate | 7.72 | 5.60 | 4.24 |
| LLC Misses | 1,650,693 | 0 | 0 |
| Instructions Retired | 19.8B | 27.6B | 22.7B |

**TT Total: 76.99s (~43.6% of total CPU time)**  
**TT + Atomics Total: ~90.7s (~51.4% of total CPU time)**

### Atomic Operations (TT Synchronization)

| Metric | atomic::store (primary) | atomic::store (#2) | atomic::load (primary) | atomic::load (#2) |
|--------|------------------------|--------------------|-----------------------|-------------------|
| CPU Time (s) | 6.70 | 1.64 | 3.34 | 1.71 |
| Memory Bound (%) | 97.5% | 57.9% | 66.8% | 71.8% |
| CPI Rate | 11.95 | 2.64 | 2.19 | 8.95 |

### Comparison Baselines

| Metric | Attacks::sliderLookup | Evaluator::evaluate | Search::search | Search::qsearch |
|--------|-----------------------|---------------------|----------------|-----------------|
| CPU Time (s) | 8.27 | 1.32 | 3.00 | 2.05 |
| Memory Bound (%) | 4.7% | 9.8% | 42.5% | 44.5% |
| CPI Rate | 0.38 | 0.44 | 0.49 | 0.65 |
| µArch Usage (%) | 42.5% | 38.0% | 34.4% | 28.2% |

### Threading Efficiency

| Metric | Value |
|--------|-------|
| Total Effective Time | 176.5s |
| Spin Time | 0.00s (0.0%) |
| Overhead Time | 0.00s (0.0%) |
| Wait Time | 0.00s (0.0%) |

### Overall Assessment

| Category | Status | Notes |
|----------|--------|-------|
| TT Memory Bound | 🔴 Critical | 51% of CPU time (with atomics), L3 bound 60-63%, CPI 4.2-7.7 |
| TT CPI Regression | 🔴 Regressed | probe CPI 7.72 (was 4.25 in v1.5), put CPI 5.60 (was 4.50) |
| Thread Contention | 🟢 Excellent | Zero spin/wait/overhead — atomics working efficiently |
| Slider Tables | 🟢 Excellent | 4.7% memory bound, CPI 0.38, well optimized |
| Evaluator | 🟢 Good | CPI 0.44, 9.8% memory bound — efficient |
| stopConditions | 🟢 Stable | 2.61s, CPI 1.39 — holding previous optimization gains |
| Atomic Operations | 🔴 High | 13.7s total, primary store 97.5% memory bound, CPI up to 11.95 |

## Top 20 Hotspots

| Rank | Function | CPU Time (s) | % Total | Memory Bound | CPI |
|------|----------|--------------|---------|--------------|-----|
| 1 | TT::probe | 29.35 | 16.6% | 60.2% | 7.72 |
| 2 | TT::put | 29.21 | 16.6% | 62.3% | 5.60 |
| 3 | TT::prefetch (site 1) | 9.59 | 5.4% | 62.7% | 4.19 |
| 4 | TT::prefetch (site 2) | 8.85 | 5.0% | 62.4% | 4.28 |
| 5 | sliderLookup | 8.27 | 4.7% | 4.7% | 0.38 |
| 6 | atomic::store (TT) | 6.70 | 3.8% | 97.5% | 11.95 |
| 7 | Attacks::attacks | 4.91 | 2.8% | 8.5% | 0.38 |
| 8 | atomic::load (TT) | 3.34 | 1.9% | 66.8% | 2.19 |
| 9 | Search::search | 3.00 | 1.7% | 42.5% | 0.49 |
| 10 | Evaluator::kingEval | 2.99 | 1.7% | 0.3% | 0.43 |
| 11 | stopConditions | 2.61 | 1.5% | 17.1% | 1.39 |
| 12 | PawnTT::prefetch | 2.55 | 1.4% | — | 30.21 |
| 13 | PawnTT::probe | 2.33 | 1.3% | 34.2% | 1.02 |
| 14 | Evaluator::pieceEval | 2.13 | 1.2% | 8.6% | 0.50 |
| 15 | Search::qsearch | 2.05 | 1.2% | 44.5% | 0.65 |
| 16 | atomic::load (PawnTT) | 1.71 | 1.0% | 71.8% | 8.95 |
| 17 | Search::thread (site 1) | 1.67 | 0.9% | 79.4% | 31.22 |
| 18 | atomic::store (#2) | 1.64 | 0.9% | 57.9% | 2.64 |
| 19 | getNextPseudoLegalMove | 1.63 | 0.9% | 15.7% | 0.44 |
| 20 | updateSortValues | 1.61 | 0.9% | 7.6% | 0.46 |

## Historical Comparison

### TT Performance Across Versions

| Metric | v1.5 (03-06, 8T) | v1.5 (03-06, 1T) | **v1.8 (04-09, MT)** | Trend |
|--------|-------------------|-------------------|-----------------------|-------|
| TT::probe CPU (s) | 17.30 | 19.23 | **29.35** | 🔴 +70% vs 8T |
| TT::probe CPI | 4.25 | 4.31 | **7.72** | 🔴 +82% |
| TT::probe Mem Bound | 64.5% | 58.9% | **60.2%** | 🟡 −7% vs 8T |
| TT::put CPU (s) | 17.61 | 31.60 | **29.21** | 🔴 +66% vs 8T |
| TT::put CPI | 4.50 | 6.75 | **5.60** | 🔴 +24% vs 8T |
| TT::put Mem Bound | 64.7% | 62.7% | **62.3%** | 🟢 −4% vs 8T |
| TT::prefetch CPU (s) | 0.31 (inlined) | 20.41 | **18.43** | 🟡 Different inlining |
| LLC Misses (probe) | 2,200,924 | 550,231 | **1,650,693** | 🟡 −25% vs 8T |
| sliderLookup CPU (s) | 6.05 | 7.16 | **8.27** | 🟡 +37% vs 8T |
| stopConditions CPU (s) | 5.89 | 2.42 | **2.61** | 🟢 Stable vs opt |
| Total Effective (s) | ~100% | 125.4 | **176.5** | — (diff configs) |

### Key Observations

1. **TT::probe CPI nearly doubled** (4.25 → 7.72) compared to v1.5 8-thread run
   - Suggests increased cache contention or larger working set in v1.8
   - More threads or different search patterns exercising TT more heavily
2. **TT::prefetch is no longer inlined** — now shows ~18.4s explicitly
   - This was 0.31s (inlined) in v1.5 8T but 20.41s in v1.5 1T
   - Different compiler optimization or code changes affected inlining decisions
3. **LLC Misses improved** 25% vs v1.5 8T (2.2M → 1.65M)
   - Despite higher CPI, actual DRAM misses decreased
4. **stopConditions holding** at 2.61s — v1.5 optimization still effective
5. **Memory bound percentages stable** — similar L3 pressure to v1.5

## Changes Since Baseline (v1.5, 2026-03-06)

### Major Code Changes (v1.5 → v1.8)

1. **MultiPV analysis mode** — UCI `MultiPV` option for top-N move reporting; helper threads always use MultiPV=1
2. **Handicap system** — 5 weakening levers per level (time waste, MultiPV inflation, depth cap, pool size, suboptimal selection)
3. **Contempt / Draw Score Bias** — UCI `Contempt` option biasing draw scores
4. **UCI debug command** — Eval breakdown and search stats output
5. **SMP crash fix** — Fixed data race in `extractPvWithTT` (shared `pvMoveGenerator` → per-thread, shared position → thread-local)
6. **Bench signature** — Deterministic node count for CI regression gating
7. **Book variety** — Frequency-weighted book move selection (`BOOK_VARIETY` UCI option)
8. **Quality improvements** — Evasion tests, sort value tests, history/counter-move ordering tests, `ucinewgame` state audit

### Possible Performance Impact

- **SMP crash fix** moved `pvMoveGenerator` to per-thread data — may affect cache locality
- **MultiPV/Handicap** additions increase code size in hot search path
- **Additional UCI options** and config checks add marginal overhead per node
- **More search features** (contempt, debug eval) may slightly increase per-node cost

## Recommendations

### 1. 🔴 Investigate TT::probe CPI Regression (Highest Priority)
TT::probe CPI nearly doubled from 4.25 to 7.72 since v1.5. Possible causes:
- **Action:** Profile with 1 thread to isolate SMP contention vs. algorithmic regression
- **Action:** Compare TT entry struct layout — verify no padding or alignment changes
- **Action:** Check if TT size / bucket count changed relative to cache hierarchy
- **Action:** Profile with different TT sizes (64MB, 128MB, 512MB) on current v1.8

### 2. 🔴 Reduce Atomic Operation Overhead (High Priority)
Atomic store primary site shows **97.5% memory bound** with CPI 11.95 (13.7s total atomics):
- **Action:** Review memory ordering — switch `seq_cst` to `relaxed` where safe (TT is best-effort)
- **Action:** Consider store-buffer optimization or batch writes
- **Action:** Investigate if atomic store site corresponds to TT entry writes (XOR encoding)

### 3. 🟡 Evaluate Per-Thread TT Impact (Medium Priority)
The SMP crash fix moved `pvMoveGenerator` to per-thread data. Verify this didn't fragment cache usage:
- **Action:** Compare v1.8 1T vs 8T profiles to measure SMP-specific overhead
- **Action:** Consider `SearchThreadData` layout optimization for cache friendliness
- **Action:** Verify TT cluster alignment still holds with per-thread allocations

---

*Analysis generated from VTune results 2026-04-09_10-25-47*  
*Compared against baseline: v1.5 2026-03-06_01-54-49 (8T) and 2026-03-06_02-34-24 (1T)*
