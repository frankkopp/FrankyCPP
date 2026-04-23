# FrankyCPP v1.x Engine Strength Roadmap

**Document Version:** 3.0  
**Created:** 2026-02-01  
**Last Updated:** 2026-04-11  
**Status:** Active Development  
**Target:** FrankyCPP v1.8 → v2.0  
**Focus:** Maximum Playing Strength Through Systematic Enhancement

---

## Executive Summary

Prioritized roadmap for enhancing FrankyCPP's playing strength from v1.8 to v2.0, based on
empirical data from top engines (Stockfish, Ethereal, Koivisto) and FrankyCPP's own gauntlet results.

### Current State (v1.8.0 — April 2026)

**Verified cumulative improvements:**
- v1.1 → v1.2: +57 ELO (Singular/Check Extensions, Counter-Move History)
- v1.2 → v1.3: +52 ELO (LMR/History fixes, Late Move Pruning, PV Node fixes)
- v1.3 → v1.4: +119 ELO (Lazy SMP multi-threading)
- v1.4 → v1.5: +92.5 ELO (TT Buckets, XOR Key verification, SMP hardening)
- v1.5 → v1.6: +81 ELO (Eval enrichment — mobility, king safety, threats, pawn structure)
- v1.6 → v1.7: +60–78 ELO (Texel tuning, eval cleanup — 14 dead features removed)
- v1.7 → v1.8: +84 ELO at 4T, +217 ELO at 8T (SMP thread scaling — per-thread TT stats, skip-table depth diversification, TT generation counter)
- **Total: ~+500–580 ELO** vs v1.1 baseline
- Estimated strength: **~2900–3000 ELO** (thread-count dependent)

**Strengths:**
- ✅ Production-ready UCI engine, cross-platform (Windows/Linux)
- ✅ Lazy SMP with TT buckets and XOR key verification (v1.4–v1.5)
- ✅ SMP thread scaling: per-thread TT stats, skip-table depth diversification, TT generation counter (v1.8)
- ✅ Texel-tuned classical evaluation with dead features removed (v1.7)
- ✅ Syzygy tablebase support (v1.2)
- ✅ Automated tuning infrastructure (Texel, v1.7)
- ✅ 266+ unit tests, CI/CD, comprehensive test suite coverage
- ✅ 2.5M NPS single-thread, ~12M NPS 8-thread

**Remaining Limitations:**
- ❌ Classical evaluation only (NNUE offers +200–400 ELO)
- ❌ Missing continuation history (2-ply, 4-ply)
- ❌ Missing capture history heuristic
- ❌ No multi-cut or probcut pruning
- ❌ No SPSA for search parameter tuning

### Target State (v2.0)
**Target:** ~2900–3100 ELO | **Remaining gain:** +200–500 ELO (primarily NNUE)

---

## Remaining Strength Gap Analysis

| Category            | Remaining Gap    | Key Items                      | Priority    |
|---------------------|------------------|--------------------------------|-------------|
| Evaluation (NNUE)   | −200 to −400 ELO | NNUE implementation            | 🔴 Critical |
| Move Ordering       | −25 to −45 ELO   | Continuation + Capture History | 🟡 Medium   |
| Search Pruning      | −20 to −35 ELO   | Multi-Cut, Probcut             | 🟡 Medium   |
| Tuning (SPSA)       | −15 to −30 ELO   | Search parameter optimization  | 🟢 Low      |
| CPU Optimization    | −15 to −35 ELO   | PEXT fallback, SIMD, PGO       | 🟢 Low      |
| **TOTAL REMAINING** | **−275 to −545** |                                |             |

---

## Completed Phases (v1.1 → v1.7)

### Phase 1: Strength Testing Infrastructure (v1.1) ✅
Arena GUI + cutechess-cli automation, calibrated opponents, ELO tracking.

### Phase 2: Performance Fundamentals & Quick Wins (v1.2) ✅ — +57 ELO
Triangular PV Table, StaticMoveList, bench command, Singular Extensions (+27),
Check Extensions (+30), Counter-Move History, Best-Move Instability Time Management.

### Phase 3: Multi-Threading (v1.4–v1.5) ✅ — +211.5 ELO
Lazy SMP (+119 ELO), TT Buckets with XOR Key (+92.5 ELO), cache-line alignment,
race condition elimination. See `docs/Lazy_SMP_Explained.md`.

### Phase 5: Endgame Tablebases (v1.2) ✅
Fathom/Syzygy integration, root + search WDL/DTZ probing.

### Phase 6 (partial): Search Refinements (v1.3) ✅ — +52 ELO
Logarithmic LMR with history adjustments, Late Move Pruning, PV Node fixes.

### Phase 7: Automated Tuning (v1.7) ✅ — +60–78 ELO
Texel Tuning: PGN parser library, position extractor, multi-threaded optimizer.
75 params tuned on 4.57M positions. 14 dead eval features removed (−503 lines, +3.5% NPS).
See `docs/archive/PLAN_Texel_Tuning.md`.

### Phase 8: SMP Thread Scaling (v1.8) ✅ — +84 ELO (4T), +217 ELO (8T)
Per-thread TT statistics (eliminated false sharing, +235% NPS scaling at 16T),
skip-table depth diversification for helper threads (−38% nodes at 8T, +39% main-search TT hits),
TT generation counter replacing legacy age increment/decrement system (engineering cleanup, ELO-neutral).
See `docs/specs/PLAN_SMP_Thread_Scaling.md`.

---

## Open Phases

### Phase 4: Enhanced Move Ordering 🔄 PARTIAL
**Target:** +25–45 ELO remaining

| Task                         | Effort      | ELO Gain | Status     |
|------------------------------|-------------|----------|------------|
| Capture History Heuristic    | 🟡 3-5 days | +10-20   | 📋 Planned |
| Continuation History (2-ply) | 🟡 3-5 days | +15-25   | 📋 Planned |
| SEE Enhancement              | 🟢 2-3 days | +5-10    | 📋 Planned |

**Capture History:** Separate table `[piece][to][captured]` for capture move ordering.
**Continuation History:** 2-ply and 4-ply context-based move scoring (generalized counter-move).

---

### Phase 6 (remaining): Advanced Search Pruning
**Target:** +20–35 ELO remaining

| Task              | Effort      | ELO Gain | Status     |
|-------------------|-------------|----------|------------|
| Multi-Cut Pruning | 🟡 3-5 days | +10-20   | 📋 Planned |
| Probcut Pruning   | 🟡 3-5 days | +10-15   | 📋 Planned |

**Multi-Cut:** If ≥3 moves fail high at reduced depth → prune (beta cutoff).
**Probcut:** Shallow search with raised beta predicts deep search result.

---

### Phase 8: NNUE Evaluation (v1.8 → v2.0) — **6–10 weeks** 📋 PLANNED
**Target:** +200–400 ELO (largest single improvement)

| Task                       | Effort       | Complexity | Status     |
|----------------------------|--------------|------------|------------|
| NNUE Architecture Design   | 🟡 1 week    | 🔴 High    | 📋 Planned |
| Incremental Update System  | 🔴 2-3 weeks | 🔴 High    | 📋 Planned |
| NNUE Inference Engine      | 🟡 1-2 weeks | 🔴 High    | 📋 Planned |
| Training Data Generation   | 🟡 1 week    | 🟡 Medium  | 📋 Planned |
| Network Training Pipeline  | 🟡 1-2 weeks | 🔴 High    | 📋 Planned |
| NNUE Integration & Testing | 🟡 1 week    | 🔴 High    | 📋 Planned |
| Classical/NNUE Hybrid      | 🟢 2-3 days  | 🟡 Medium  | 📋 Planned |

**Architecture (HalfKP-256x2):**
```
Input: 40960 features (king position + piece positions)
  ↓ (feature transformer)
Hidden Layer 1: 256 neurons (ClippedReLU, int16 quantized)
  ↓
Hidden Layer 2: 256 neurons (ClippedReLU, int16 quantized)
  ↓
Output: 1 neuron (linear, scaled to centipawns)
```

**Key Design:**
```cpp
class NNUE {
    alignas(64) int16_t featureWeights[40960][256];
    alignas(64) int16_t l1Weights[256][256];
    alignas(64) int16_t l2Weights[256][1];
    int32_t l1Bias[256], l2Bias[256];
    alignas(64) int16_t accumulator[2][256];  // [side][neurons]
};
```

**Implementation Plan:**
1. **Weeks 1–2:** Architecture, data structures, feature extraction
2. **Weeks 3–5:** Incremental accumulator updates, SIMD optimization (AVX2)
3. **Weeks 6–7:** Inference engine (quantized forward propagation)
4. **Weeks 8–9:** Training pipeline (self-play data, NNUE-pytorch)
5. **Week 10:** UCI integration (`Use NNUE`, `EvalFile`), classical fallback

**Configuration:**
```yaml
USE_NNUE: true
NNUE_NET_PATH: "./networks/nn-default.nnue"
NNUE_FALLBACK_CLASSICAL: true
```

**Risk:** Very High — major architectural change. Can leverage existing frameworks
(Bullet, NNUE-pytorch) and pre-trained nets (HalfKP).

---

### Phase 9: CPU Compatibility & Optimization (v2.0) — **1–2 weeks** 📋 PLANNED
**Target:** +15–35 ELO (performance)

| Task                        | Effort      | ELO Gain | Status     |
|-----------------------------|-------------|----------|------------|
| Runtime PEXT Detection      | 🟢 2-3 days | N/A      | 📋 Planned |
| Software PEXT Fallback      | 🟡 3-5 days | N/A      | 📋 Planned |
| SIMD Optimization (AVX2)    | 🟡 1 week   | +10-20   | 📋 Planned |
| Profile-Guided Optimization | 🟢 2-3 days | +5-15    | 📋 Planned |

**Current limitation:** BMI2 (PEXT) required, excluding pre-2013 CPUs.
**Solution:** Runtime detection + software fallback (~20% slower).
**SIMD targets:** NNUE inference (AVX2/AVX-512), bitboard operations.

---

## Progress Dashboard

| Phase | Version   | ELO Target | ELO Actual | Cumulative | Status     |
|-------|-----------|------------|------------|------------|------------|
| 1     | v1.1      | N/A        | N/A        | 0          | ✅ Complete |
| 2     | v1.2      | +70-135    | **+57**    | +57        | ✅ Complete |
| 3     | v1.4-v1.5 | +60-120    | **+211.5** | +268.5     | ✅ Complete |
| 4     | v1.3+     | +35-65     | partial    | —          | 🔄 Partial |
| 5     | v1.2      | +35-60     | (incl.)    | (included) | ✅ Complete |
| 6     | v1.3      | +50-85     | **+52**    | +320.5     | 🔄 Partial |
| 7     | v1.7      | +30-60     | **+60–78** | ~+400      | ✅ Complete |
| 8     | v1.8–v2.0 | +200-400   | TBD        | —          | 📋 Planned |
| 9     | v2.0      | +15-35     | TBD        | —          | 📋 Planned |

---

## Timeline (Estimated)

```
Completed (Feb–Mar 2026):
  ├─ Phase 1: Testing Infrastructure (v1.1)        ✅
  ├─ Phase 2: Quick Wins (v1.2)                    ✅ +57 ELO
  ├─ Phase 3: Multi-Threading (v1.4-v1.5)          ✅ +211.5 ELO
  ├─ Phase 5: Tablebases (v1.2)                    ✅
  ├─ Phase 6 partial: LMR/History (v1.3)           ✅ +52 ELO
  ├─ Phase 7: Texel Tuning (v1.6-v1.7)             ✅ +141 ELO (v1.6+v1.7)
  └─ Cumulative: ~+400-470 ELO                     ✅

Upcoming:
  ├─ Phase 4 remaining: Move Ordering              ~1-2 weeks
  ├─ Phase 6 remaining: Multi-Cut/Probcut          ~1-2 weeks
  ├─ Phase 8: NNUE Implementation                  ~6-10 weeks
  └─ Phase 9: CPU Optimization                     ~1-2 weeks
```

---

## References

### Open Source Engines
| Engine        | Notable Features                      |
|---------------|---------------------------------------|
| **Stockfish** | NNUE, Lazy SMP, reference impl        |
| **Ethereal**  | Classical eval, tuning infrastructure |
| **Koivisto**  | NNUE training pipeline                |
| **Berserk**   | Recent NNUE, good documentation       |
| **RubiChess** | Clean SMP implementation              |

### Libraries & Tools
- **Fathom** — Syzygy tablebase probing
- **NNUE-pytorch** — Network training framework
- **cutechess-cli** — Automated engine matches
- **fastchess** — Modern match runner

### Academic Papers
- Lazy SMP — Parallel Alpha-Beta (Martin, 2013)
- NNUE — Efficiently Updatable Neural Networks (Nasu, 2018)
- Texel Tuning — Evaluation Tuning (Österlund, 2014)

---

**Document Maintainer:** Frank Kopp  
**Last Updated:** 2026-03-31  
**Next Review:** After v1.8 release
