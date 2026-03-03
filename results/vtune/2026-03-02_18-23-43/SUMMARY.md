# VTune Analysis Summary: 1-Thread Run

**Date:** 2026-03-02  
**Configuration:** 1 thread, `--bench` mode  
**Purpose:** Isolate memory bandwidth effects by comparing with 8-thread baseline

---

## Key Finding: Memory Bandwidth Saturation Confirmed

The 1-thread run proves that TT bottlenecks under 8-threads are caused by **memory bandwidth saturation**, not algorithmic issues.

| Metric | 8 Threads | 1 Thread | Change |
|--------|-----------|----------|--------|
| TT::probe CPU Time | 42.3s | **0.05s** | **-99.9%** |
| TT::probe CPI | 3.37 | **1.64** | -51% |
| TT::probe Memory Bound | 74% | **29%** | -61% |
| TT::put CPI | 5.65-6.65 | **0.47** | -92% |
| Slider CPI | 0.38 | **0.39** | ~0% (unchanged) |

---

## Top Hotspots (1 Thread)

| Rank | Function | CPU Time | CPI | Memory Bound |
|------|----------|----------|-----|--------------|
| 1 | `std::_Atomic_storage::load` | 2.79s | 2.21 | 61% |
| 2 | `TT::clear` lambda | 2.08s | 2.90 | 71% |
| 3 | `TT::ageEntries` lambda | 1.56s | 3.29 | 79% |
| 4 | `Attacks::sliderLookup` | 1.43s | 0.39 | 10% |
| 5 | `Attacks::attacks` | 0.80s | 0.32 | 6% |
| 6 | `Search::search` | 0.45s | 0.44 | 1% |
| 7 | `Search::qsearch` | 0.37s | 0.68 | 8% |

**Note:** `TT::probe` dropped from #1 hotspot (8 threads) to negligible (not in top 10).

---

## Conclusions

1. **TT contention is entirely multi-threaded**: Single-threaded TT access is fast (CPI 1.64 vs 3.37)
2. **Core algorithms are efficient**: Sliders, evaluator, search have same CPI regardless of thread count
3. **Prefetch works when not saturated**: Memory bus capacity is the limiting factor at 8 threads
4. **TT buckets + alignas(64) is the correct optimization**: Will reduce false sharing and cache line ping-pong

---

## Generated Files

- `hotspots.csv` - CPU time by function
- `microarchitecture.csv` - CPI, pipeline analysis
- `memory-access.csv` - Memory bound percentages, LLC misses
- `hpc-performance.csv` - DRAM bound, vectorization
- `threading.csv` - Thread activity (minimal for 1-thread)

---

*Compare with 8-thread baseline: `2026-03-02_16-36-13`*
