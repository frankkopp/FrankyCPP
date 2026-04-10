# VTune Analysis Summary — Single-Thread (1T)

**Date:** 2026-04-09 10:42:26  
**Executable:** FrankyCPP v1.8  
**Configuration:** Single-thread (`--threads 1`), TT=256MB  
**Build:** RelWithDebInfo (MSVC)  
**Purpose:** Isolate SMP contention from algorithmic regression (follow-up to MT run)

---

## 🎯 Key Finding: TT CPI Regression is **100% SMP Contention**

| Metric              | v1.5 8T | v1.5 1T | **v1.8 MT** | **v1.8 1T**  | Verdict    |
|---------------------|---------|---------|-------------|--------------|------------|
| TT::probe CPI       | 4.25    | 4.31    | **7.72** 🔴 | **1.15** 🟢  | SMP only   |
| TT::put CPI         | 4.50    | 6.75    | **5.60** 🔴 | **0.49** 🟢  | SMP only   |
| TT::probe Mem Bound | 64.5%   | 58.9%   | 60.2%       | **35.9%** 🟢 | Reduced    |
| TT::put Mem Bound   | 64.7%   | 62.7%   | 62.3%       | **2.5%** 🟢  | Negligible |

**Conclusion:** With 1 thread, TT::probe CPI dropped from 7.72 → **1.15** (6.7× improvement) and TT::put from 5.60 → **0.49** (11.4× improvement). The v1.8 TT is algorithmically faster than v1.5 — the regression seen in MT was purely cache-line contention across threads.

---

## Top 20 Hotspots (1-Thread)

| Rank | Function                           | CPU Time (s) |  CPI  | µArch Usage | Memory Bound |
|------|------------------------------------|:------------:|:-----:|:-----------:|:------------:|
| 1    | **atomic::load** (TT entry reads)  |     2.92     | 2.17  |    7.9%     |    61.1%     |
| 2    | **sliderLookup**                   |     1.92     | 0.39  |    40.5%    |     5.8%     |
| 3    | **Attacks::attacks**               |     0.91     | 0.36  |    45.4%    |     4.6%     |
| 4    | **TT::ageEntries** (λ)             |     0.76     | 1.39  |    12.0%    |    94.3%     |
| 5    | **Evaluator::kingEval**            |     0.70     | 0.40  |    37.8%    |     0.0%     |
| 6    | **Search::search**                 |     0.57     | 0.43  |    34.8%    |     3.7%     |
| 7    | **Evaluator::pieceEval**           |     0.44     | 0.36  |    50.0%    |     0.8%     |
| 8    | func@vcruntime140 (memcpy)         |     0.41     | 374.5 |    1.2%     |     100%     |
| 9    | **Search::qsearch**                |     0.38     | 0.51  |    28.8%    |     6.7%     |
| 10   | countr_zero                        |     0.33     | 0.33  |      —      |     0.0%     |
| 11   | Search::thread (site 1)            |     0.32     | 0.89  |    21.6%    |    14.8%     |
| 12   | **getNextPseudoLegalMove**         |     0.32     | 0.42  |    41.9%    |     0.0%     |
| 13   | Search::thread (site 2)            |     0.30     | 0.87  |    21.6%    |     5.8%     |
| 14   | popcount                           |     0.30     | 0.44  |    40.0%    |     0.0%     |
| 15   | **fillOnDemandMoveList**           |     0.28     | 0.44  |    38.5%    |     0.0%     |
| 16   | **insertion_sort** (move ordering) |     0.28     | 0.58  |      —      |     0.0%     |
| 17   | **Evaluator::rookEval**            |     0.27     | 0.33  |    50.6%    |     2.4%     |
| 18   | **updateSortValues**               |     0.27     | 0.43  |    37.2%    |     1.0%     |
| 19   | **TT::put**                        |     0.26     | 0.49  |    36.5%    |     2.5%     |
| 20   | **coordinationEval**               |     0.26     | 0.50  |    35.0%    |     0.0%     |

---

## CPU Time by Category

| Category            | CPU Time (s) | % of Top Functions | Notes                                                                                                                                                                  |
|---------------------|:------------:|:------------------:|------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **TT Operations**   |     ~4.1     |        ~28%        | atomic::load 2.92 + ageEntries 0.76 + TT::put 0.26 + TT::probe 0.08                                                                                                    |
| **Move Generation** |     ~1.6     |        ~11%        | fillOnDemand 0.28 + generateMoves 0.24 + getNext 0.32 + updateSort 0.27 + pawnMoves 0.11 + kingMoves 0.08                                                              |
| **Attack Tables**   |     ~2.8     |        ~19%        | sliderLookup 1.92 + attacks 0.91                                                                                                                                       |
| **Evaluation**      |     ~3.2     |        ~22%        | kingEval 0.70 + pieceEval 0.44 + rookEval 0.27 + coordEval 0.26 + threatEval 0.20 + evaluate 0.22 + bishopEval 0.14 + knightEval 0.15 + queenEval 0.09 + pawnEval 0.08 |
| **Search Core**     |     ~1.3     |        ~9%         | search 0.57 + qsearch 0.38 + storeTt 0.03                                                                                                                              |
| **Position Ops**    |     ~0.9     |        ~6%         | removePiece 0.18 + putPiece 0.17 + doMove 0.15 + givesCheck 0.26 + isAttacked 0.17                                                                                     |
| **SEE**             |     ~0.3     |        ~2%         | see 0.14 + attacksTo 0.06 + revealedAttacks 0.04 + getLeastValuable 0.03                                                                                               |
| **Move Sorting**    |     ~0.4     |        ~3%         | insertion_sort 0.28 + comparator 0.06 + Sort_common 0.06                                                                                                               |

---

## Memory Analysis Highlights

### Memory-Bound Functions (1T)

| Function                        | Memory Bound | L1 Bound | L2 Bound | L3 Bound | DRAM Bound |  LLC Misses   |
|---------------------------------|:------------:|:--------:|:--------:|:--------:|:----------:|:-------------:|
| **TT::ageEntries** (λ)          |  **94.3%**   |   0.0%   |   0.0%   |  43.6%   |   43.6%    | **4,952,079** |
| **atomic::load** (TT)           |  **61.1%**   |   0.0%   |   3.0%   |   0.5%   |   66.2%    |    550,231    |
| **TT::probe**                   |  **35.9%**   |  10.7%   |   0.0%   |   0.0%   |   40.8%    |    550,231    |
| **atomic_address_as** (TT sync) |    27.5%     |   0.0%   |  11.6%   |  15.5%   |   15.5%    |   6,602,772   |
| **PawnTT::probe**               |    27.5%     |   0.0%   |  18.4%   |   9.2%   |     —      |       0       |
| **PawnTT::Entry copy**          |    18.2%     |  20.8%   |  34.6%   |   0.0%   |    0.0%    |       0       |
| Search::thread (site 2)         |     5.8%     |   4.5%   |   4.5%   |   0.0%   |   11.4%    |       0       |
| Attacks::sliderLookup           |     5.8%     |   6.6%   |   0.0%   |   2.2%   |    5.0%    |       0       |
| Search::search                  |     3.7%     |   8.3%   |   1.5%   |   0.0%   |    0.8%    |    550,231    |
| TT::put                         |     2.5%     |  12.3%   |   0.0%   |   1.4%   |    0.0%    |       0       |

### Key Memory Observations

1. **TT::ageEntries dominates LLC misses** (4.95M) — streaming scan of entire 256MB TT every search iteration
2. **atomic::load for TT reads** is 66% DRAM-bound — TT entries miss L3 cache even with 1 thread (256MB > L3)
3. **TT::put is cache-friendly with 1 thread** — only 2.5% memory bound (vs 62.3% in MT)
4. **PawnTT** shows L1/L2 pressure (34.6% L2 bound for Entry copy) — pawn TT fits in cache but has contention on entry copy
5. **Move generation and evaluation are compute-bound** — negligible memory issues

---

## HPC / Microarchitecture Insights

| Function                        |   CPI    | Vectorization | Vector ISA  | Observations            |
|---------------------------------|:--------:|:-------------:|:-----------:|-------------------------|
| Evaluator::evaluate             |   0.29   |     0.3%      | AVX-128/256 | Best CPI, some SIMD     |
| PawnTT::Entry copy              |   0.23   |     5.0%      |   AVX-128   | Memcpy-like, vectorized |
| TT::put                         |   0.50   |     0.7%      |   AVX-128   | Low CPI for TT write    |
| MoveGenerator::getNextPseudo    |   0.42   |     0.3%      |      —      | Minimal vectorization   |
| Search::search loop (line 1661) |   0.41   |     0.0%      |      —      | Integer-heavy search    |
| TT::ageEntries                  | **1.65** |     0.0%      |      —      | Bandwidth limited       |
| TT::Entry ctor                  | **4.52** |     0.0%      |      —      | High CPI, L3 bound      |
| atomic::load (TT)               | **2.05** |     0.0%      |      —      | Memory latency          |

**Nearly zero vectorization across the engine** — expected for a chess engine (integer logic, bitboard ops). The tiny vectorization seen is from compiler auto-vectorization of memcpy/loop constructs.

---

## 1T vs MT Comparison Summary

| Metric               | MT Run |  1T Run   |   Change   | Interpretation             |
|----------------------|:------:|:---------:|:----------:|----------------------------|
| Total Effective Time | 176.5s |   ~15s    |     —      | 1 thread vs many           |
| TT::probe CPI        |  7.72  | **1.15**  | **−85%** ✅ | No algorithmic regression  |
| TT::put CPI          |  5.60  | **0.49**  | **−91%** ✅ | SMP cache-line bouncing    |
| TT::probe Mem Bound  | 60.2%  | **35.9%** |   −40% ✅   | Less L3 contention         |
| TT::put Mem Bound    | 62.3%  | **2.5%**  |   −96% ✅   | No cross-core invalidation |
| atomic::store CPI    | 11.95  |     —     |     —      | Not a 1T hotspot           |
| atomic::load CPI     |  2.19  |   2.17    |    Flat    | Inherent TT access cost    |
| sliderLookup CPI     |  0.38  |   0.39    |    Flat    | Stable                     |
| Evaluator CPI        |  0.44  | 0.36–0.50 |   Stable   | Compute-efficient          |
| Spin/Wait/Overhead   |  0.0%  |   0.0%    |    N/A     | Zero contention            |

---

## Actionable Recommendations

### ✅ Resolved: TT Algorithmic Performance
The v1.8 TT is **not regressed algorithmically**. Single-thread CPI (probe=1.15, put=0.49) is better than v1.5 1T (probe=4.31, put=6.75). No code changes needed.

### 🔴 1. TT::ageEntries — Largest Single-Thread Bottleneck
- **94.3% memory bound**, 4.95M LLC misses, CPI 1.39
- Scans entire 256MB TT each iteration — streaming DRAM access
- **Actions:**
   - Consider partial aging (age 1/N of TT per iteration, rotating)
   - Use `std::execution::par_unseq` with prefetching for better bandwidth utilization
   - Or skip aging when search depth is low / time is short

### 🟡 2. TT Atomic Load Still DRAM-Bound (1T)
- 2.92s, 66.2% DRAM bound even with 1 thread — TT simply doesn't fit in L3
- This is **inherent** to a 256MB TT on a CPU with ~36MB L3
- **Actions:**
   - Consider adaptive TT sizing based on position complexity
   - Ensure prefetch distance is optimal (currently used, appears working)
   - No major gain expected — this is working-set vs cache-size physics

### 🟡 3. SMP TT Contention (For Multi-Thread Optimization)
- The MT CPI regression (7.72× for probe) is caused by cache-line bouncing
- **Actions:**
   - Consider per-thread TT segments or partitioned probing to reduce cross-core invalidation
   - Investigate larger cluster alignment (128B = 2 cache lines) to reduce false sharing
   - Review if `relaxed` memory ordering on atomic stores can reduce coherence traffic

### 🟢 4. Evaluation & Move Generation — Well Optimized
- All evaluator functions show CPI 0.29–0.50, <3% memory bound
- Move generation functions CPI 0.39–0.44, excellent µArch utilization (38–50%)
- **No changes recommended** for these code paths

### 🟢 5. Attack Tables — Excellent
- sliderLookup: CPI 0.39, 5.8% memory bound, zero LLC misses
- Magic bitboard tables fit well in L1/L2 cache
- **No changes needed**
