# VTune Comparison: 4 Threads vs 8 Threads

**4T run:** `2026-04-09_10-58-10` | **8T run:** `2026-04-09_10-42-26`

---

## 1. Hotspots — Top 20 Comparison

| #  | 4 Threads (CPU Time)       | 8 Threads (CPU Time)       | Δ                                 |
|----|----------------------------|----------------------------|-----------------------------------|
| 1  | **TT::put — 9.58s**        | atomic::load — 2.92s       | TT::put was 0.26s in 8T (#20)     |
| 2  | **TT::probe — 7.19s**      | sliderLookup — 1.92s       | TT::probe was 0.085s in 8T (#39!) |
| 3  | sliderLookup — 5.80s       | attacks — 0.91s            | 3.0× more compute with 4T         |
| 4  | atomic::load — 5.02s       | **TT::ageEntries — 0.76s** | ❌ Gone in 4T!                     |
| 5  | attacks — 3.05s            | kingEval — 0.70s           |                                   |
| 6  | TT::prefetch — 2.95s       | search — 0.57s             |                                   |
| 7  | TT::prefetch — 2.66s       | pieceEval — 0.44s          |                                   |
| 8  | atomic::store — 2.50s      | func@vcruntime — 0.41s     |                                   |
| 9  | kingEval — 2.30s           | qsearch — 0.38s            |                                   |
| 10 | PawnTT::probe — 1.56s      | countr_zero — 0.33s        |                                   |
| 11 | search — 1.48s             | **Search::thread — 0.32s** | thread overhead 21.6% µarch       |
| 12 | pieceEval — 1.43s          | getNextPseudo — 0.32s      |                                   |
| 13 | qsearch — 1.19s            | **Search::thread — 0.30s** | thread overhead 21.6% µarch       |
| 14 | countr_zero — 1.03s        | popcount — 0.30s           |                                   |
| 15 | **Search::thread — 1.02s** | fillOnDemand — 0.28s       |                                   |
| 16 | updateSortValues — 1.00s   | insertion_sort — 0.28s     |                                   |
| 17 | insertion_sort — 0.99s     | rookEval — 0.27s           |                                   |
| 18 | fillOnDemand — 0.96s       | updateSortValues — 0.27s   |                                   |
| 19 | getNextPseudo — 0.94s      | **TT::put — 0.26s**        | ← vs #1 in 4T!                    |
| 20 | rookEval — 0.93s           | coordinationEval — 0.26s   |                                   |

### Key Hotspot Observations

- **TT::put / TT::probe rocketed to #1 and #2** in 4T (9.58s + 7.19s = 16.77s combined). In 8T, they were buried at #20 and #39. This means with fewer threads, TT operations are doing **more actual work** rather than spinning on contention.
- **TT::ageEntries completely vanished** from 4T hotspots — it was #4 in 8T at 0.76s. Fewer threads = less TT churn = no visible aging cost.
- **atomic::load dropped from #1 (8T) to #4 (4T)** — still significant but no longer the dominant bottleneck.
- **`Search::thread` overhead dropped dramatically**: 8T had two thread instances at 0.32s each with 21.6% µarch efficiency. 4T has one at 1.02s but at a still-low 2.0% µarch — these are the thread-sync wait loops.

---

## 2. TT Contention — The Core Story

| Metric                        | 4 Threads     | 8 Threads   | Analysis                                          |
|-------------------------------|---------------|-------------|---------------------------------------------------|
| **TT::put CPU**               | 9.58s         | 0.26s       | 37× more self-time — doing real work, not blocked |
| **TT::probe CPU**             | 7.19s         | 0.085s      | 85× more self-time                                |
| **atomic::load (TT reads)**   | 5.02s         | 2.92s       | 1.7× — still the hot atomic path                  |
| **atomic::store (TT writes)** | 2.50s + 0.79s | ~0.10s      |                                                   |
| **TT::prefetch**              | 2.95s + 2.66s | not visible | Prefetch is now a real cost                       |
| **TT::ageEntries**            | not visible   | 0.76s (#4)  | Eliminated in 4T                                  |

### CPI Rates (Cycles Per Instruction)

| Function           | 4T CPI     | 8T CPI      | Analysis                                                |
|--------------------|------------|-------------|---------------------------------------------------------|
| TT::put            | 2.71       | 0.49        | 8T had low CPI because most time was in atomics         |
| TT::probe          | 3.60       | 1.15        | Both high — cache miss dominated                        |
| atomic::load (TT)  | 1.72       | 2.17        | **4T is better** — less cross-core invalidation         |
| atomic::store (TT) | 6.33       | —           | Very expensive store-forwarding stalls                  |
| Search::thread     | 11.1 / 7.3 | 0.89 / 0.87 | **8T threads had low CPI** (good efficiency per thread) |

### Interpretation
With 8T, the TT functions themselves had low self-time because threads were **stalling on atomics** (cache-line bouncing between 8 cores). The "work" was accounted to `atomic::load/store`.

With 4T, TT functions have high self-time because threads can actually **execute the TT logic** — less cache-line contention lets the instructions retire. The high CPI (2.7-3.6) reflects genuine L3/DRAM latency on TT lookups, not contention stalls.

---

## 3. Memory Access Analysis

### TT Memory Bound Breakdown

| Function           | 4T Mem Bound% | 4T Dominant Level        | 8T Mem Bound% | 8T Dominant Level |
|--------------------|---------------|--------------------------|---------------|-------------------|
| TT::probe          | 24.8%         | L3 (24.4%) + DRAM (9.3%) | 35.9%         | DRAM (40.8%)      |
| TT::put            | 26.6%         | L3 (39.8%)               | 2.5%          | L1 (12.3%)        |
| atomic::load (TT)  | 80.6%         | **DRAM (71.0%)**         | 61.1%         | **DRAM (66.2%)**  |
| atomic::store (TT) | 71.9%         | L3 (43.8%)               | —             | —                 |
| TT::Entry ctor     | 65.4%         | **DRAM (60.0%)**         | 100%          | DRAM (100%)       |

### HPC Performance — Memory Bound

| Function         | 4T Mem% | 4T L3% | 4T DRAM%  | 8T Mem% | 8T L3% | 8T DRAM%  |
|------------------|---------|--------|-----------|---------|--------|-----------|
| **TT::put**      | 33.1%   | 41.6%  | 0.0%      | 1.2%    | 0.0%   | 0.0%      |
| **TT::probe**    | 25.0%   | 22.9%  | 13.4%     | 25.0%   | 0.0%   | 26.2%     |
| **atomic::load** | 62.9%   | 8.7%   | **66.1%** | 66.0%   | 2.2%   | **70.3%** |
| TT loop (put)    | 49.2%   | 49.0%  | 0.0%      | 1.0%    | 0.6%   | 0.0%      |
| Search::thread   | 77.7%   | 43.2%  | 0.9%      | 11.6%   | 1.2%   | 6.4%      |

### Key Memory Observations

1. **DRAM bound on atomic::load slightly improved**: 66.1% (4T) vs 70.3% (8T) — fewer cores means less DRAM contention, but the working set still doesn't fit L3.
2. **TT::put shifted to L3-bound**: 4T shows 41.6% L3 bound with 0% DRAM. In 8T, TT::put barely registered. This means with 4 threads, TT entries are mostly found in L3 cache — much better locality.
3. **TT::probe L3 hit rate improved**: 4T has 22.9% L3 + 13.4% DRAM vs 8T's 26.2% DRAM. More probes hitting L3 instead of DRAM.
4. **LLC Miss Count**: 4T TT::probe had 1.1M LLC misses. 8T had 550K — but 8T had far less self-time, so the miss *rate* is higher per-access in 8T.

---

## 4. Threading Analysis

### Thread Overhead (from threading CSV)

| Metric                | 4 Threads           | 8 Threads        |
|-----------------------|---------------------|------------------|
| Search::thread T0     | 1.02s (CPI 11.1)    | 0.32s (CPI 0.89) |
| Search::thread T1+    | 0.81s (CPI 7.3)     | 0.30s (CPI 0.87) |
| stopConditions        | **1.12s** (visible) | not in top       | 
| sendSearchUpdateToUci | 0.15s               | not visible      |

**`Search::stopConditions` appeared at 1.12s** in 4T but was invisible in 8T. With 4T, the stop-condition checking (which involves atomic reads) becomes a measurably expensive operation — likely due to frequent time checks during the longer search.

### Thread Efficiency

The 8T `Search::thread` entries had µarch efficiency of 21.6% — quite good for sync code. The 4T thread entries show 2.0-2.7% — meaning threads spend most time waiting (spinning/blocking on semaphores). This is expected for the thread initialization/completion overhead.

---

## 5. Compute Function Scaling

| Function     | 4T CPU | 8T CPU | Ratio | CPI 4T | CPI 8T |
|--------------|--------|--------|-------|--------|--------|
| sliderLookup | 5.80s  | 1.92s  | 3.0×  | 0.36   | 0.39   |
| attacks      | 3.05s  | 0.91s  | 3.4×  | 0.36   | 0.36   |
| kingEval     | 2.30s  | 0.70s  | 3.3×  | 0.43   | 0.40   |
| pieceEval    | 1.43s  | 0.44s  | 3.3×  | 0.39   | 0.36   |
| search       | 1.48s  | 0.57s  | 2.6×  | 0.44   | 0.43   |
| qsearch      | 1.19s  | 0.38s  | 3.1×  | 0.53   | 0.51   |
| rookEval     | 0.93s  | 0.27s  | 3.4×  | 0.32   | 0.33   |

**Average scaling: ~3.2×** for compute functions going from 8T→4T.

If the engine were perfectly scaling, we'd expect 4T to have 2× the per-thread time (same work, half threads → 2× wall time × same threads). The **3.2× ratio** suggests 4T is doing **~60% more useful compute work** per CPU-second, because less time is wasted on TT contention.

The CPI rates for compute functions are virtually identical (0.32-0.53) — the compute pipeline is equally efficient. The difference is entirely in how much time reaches these functions vs being consumed by TT stalls.

---

## 6. Summary & Conclusions

### What 4T Confirms About the 8T Profile

| Finding                                       | Evidence                                                                        |
|-----------------------------------------------|---------------------------------------------------------------------------------|
| ✅ **TT is the #1 bottleneck at 8T**           | TT functions went from buried (#20+) to dominant (#1-2) when contention dropped |
| ✅ **Cache-line bouncing is the mechanism**    | atomic::load dropped from #1→#4; DRAM bound dropped from 70%→66%                |
| ✅ **TT::ageEntries was a real cost at 8T**    | Completely eliminated in 4T profile                                             |
| ✅ **Compute efficiency is fine**              | CPI 0.32-0.53 for eval/movegen in both configurations — pipeline is happy       |
| ✅ **L3 locality improves with fewer threads** | TT::put became L3-bound (41.6%) instead of invisible; probe L3 hits up          |

### Efficiency Comparison

| Aspect                     | 4 Threads               | 8 Threads            |
|----------------------------|-------------------------|----------------------|
| **TT contention overhead** | Low-moderate            | **Very high**        |
| **DRAM pressure**          | Moderate                | High                 |
| **Useful compute ratio**   | ~60% better per CPU-sec | Wasted on contention |
| **TT aging overhead**      | None visible            | 0.76s (#4 hotspot)   |
| **Cache locality**         | Good (L3 dominant)      | Poor (DRAM dominant) |

### Actionable Insights

1. **4 threads is likely the sweet spot** for this TT size on your hardware — good parallelism with manageable contention.
2. **8 threads would benefit most from**: larger TT (more cache lines → fewer collisions), cluster-based sharding, or NUMA-aware allocation.
3. **The 64 MB TT is fine for 4T** — entries mostly hit L3 (which is ~30 MB on your CPU). At 8T, the effective working set exceeds L3, causing DRAM spills.
4. **PawnTT emerged as a new cost** in 4T (1.56s, #10) — with TT contention reduced, PawnTT probe/prefetch becomes a visible secondary cache bottleneck.
5. **stopConditions is surprisingly expensive at 1.12s** in 4T — the time management checks may be called too frequently. Worth investigating if the check interval can be reduced.
