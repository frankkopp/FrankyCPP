# FrankyCPP v1.x Engine Enhancement Plan

**Document Version:** 2.0
**Created:** 2026-02-01
**Last Updated:** 2026-03-31
**Status:** Phases 1–3, 5, 7 Complete. Phases 4, 6 Partial. Phase 8–9 Planned.
**Target:** FrankyCPP v1.5 → v2.0

---

## Executive Summary

Comprehensive plan for enhancing FrankyCPP's playing strength through systematic improvements
to search, evaluation, and supporting infrastructure.

**Current State (v1.7):**
- Production-ready classical chess engine with UCI protocol
- Alpha-beta search with modern pruning (NMP, LMR, futility, razoring, singular/check extensions)
- Classical evaluation — Texel-tuned weights, dead features removed (v1.7)
- Multi-threaded search (Lazy SMP) with TT buckets (v1.4–v1.5)
- Syzygy tablebase support (v1.2)
- Automated Texel tuning infrastructure (v1.7)
- 100+ configurable parameters via YAML
- 266+ unit tests, cross-platform (Windows/Linux)
- **Cumulative: ~+400 ELO** vs v1.1 baseline (verified via Arena matches)

**Target State (v2.0):**
- Neural network evaluation (NNUE) with classical fallback
- Enhanced move ordering (continuation/capture history)
- Additional search pruning (multi-cut, probcut)
- Broader CPU compatibility (PEXT fallback)
- **Expected remaining gain:** +250–500 ELO

---

## Phase-Based Roadmap

### Phase 1: Strength Testing Infrastructure (v1.1) ✅ COMPLETE
Arena integration, cutechess-cli automation, ELO tracking, calibrated opponent collection.
Foundation for measuring all future improvements.

---

### Phase 2: Performance Fundamentals & Quick Wins (v1.2) ✅ COMPLETE
**Actual: +57 ELO (v1.2 vs v1.1)**
- Triangular PV Table — zero heap allocations during search
- `StaticMoveList<N>` template — unified MoveList/VariationStack, better cache locality
- `bench` command + Arena bench integration
- Singular Extensions: +27 ELO verified
- Check Extensions: +30 ELO verified (combined +57 with singular)
- Counter-Move History, Best-Move Instability Time Management
- Deferred: Selective Checks in Quiescence (planned for future)

---

### Phase 3: Multi-Threading (v1.4–v1.5) ✅ COMPLETE
**Actual: +119 ELO (v1.4 vs v1.3), +92.5 ELO (v1.5 vs v1.4)**

**v1.4:** Lazy SMP — helper threads run independent `iterativeDeepening()`, shared TT with
atomic key ops. `SearchThreadData` per-thread isolation. UCI `Threads` option.

**v1.5:** TT Bucket Design (4-way associative, 64-byte cache-line aligned), XOR Key Verification
for torn-read detection, all SMP race conditions eliminated.

See `docs/Lazy_SMP_Explained.md` for details.

---

### Phase 4: Enhanced Move Ordering (v1.3–v1.5) 🔄 PARTIAL
**Focus:** Better move ordering for deeper effective search
**Target:** +25–45 ELO remaining

| Task                         | Effort      | Complexity | ELO Gain | Status            |
|------------------------------|-------------|------------|----------|-------------------|
| Capture History Heuristic    | 🟡 3-5 days | 🟡 Medium  | +10-20   | 📋 Planned        |
| Continuation History (2-ply) | 🟡 3-5 days | 🟡 Medium  | +15-25   | 📋 Planned        |
| SEE Enhancement              | 🟢 2-3 days | 🟡 Medium  | +5-10    | 📋 Planned        |
| Killer Move Optimization     | —           | —          | —        | ✅ Complete        |
| History Heuristic Fixes      | —           | —          | —        | ✅ Complete (v1.3) |
| Counter-Move History         | —           | —          | —        | ✅ Complete (v1.2) |

**Remaining Work:**

#### Capture History Heuristic
Separate history table for captures indexed by `[piece][to][captured]`.
Tracks which captures historically cause beta cutoffs.
```cpp
int captureHistory[PIECE_TYPE_NB][SQUARE_NB][PIECE_TYPE_NB];
```

#### Continuation History (2-ply, 4-ply)
Generalization of counter-move history. Tracks history scores for moves that follow
specific previous move types at 1-ply, 2-ply, and 4-ply context.

---

### Phase 5: Endgame Tablebases (v1.2) ✅ COMPLETE
Fathom library integration, root + search probing (WDL/DTZ), configurable via `SyzygyPath`,
`SyzygyProbeDepth`, `SyzygyProbeLimit` UCI options. Perfect play in 3–6 piece endgames.

---

### Phase 6: Advanced Search Refinements (v1.3–v1.4) 🔄 PARTIAL
**Actual (partial): +52 ELO from LMR/history fixes**

| Task                 | Effort      | Complexity | ELO Gain | Status            |
|----------------------|-------------|------------|----------|-------------------|
| Multi-Cut Pruning    | 🟡 3-5 days | 🟡 Medium  | +10-20   | 📋 Planned        |
| Probcut Pruning      | 🟡 3-5 days | 🟡 Medium  | +10-15   | 📋 Planned        |
| Late Move Pruning    | —           | —          | —        | ✅ Complete (v1.3) |
| Improved LMR Formula | —           | —          | —        | ✅ Complete (v1.3) |
| PV Node Fixes        | —           | —          | —        | ✅ Complete (v1.3) |

**Remaining Work:**

#### Multi-Cut Pruning
If multiple moves fail high at reduced depth, assume beta cutoff:
```cpp
if (multiCutCount >= 3 && depth >= 8) {
    return beta;
}
```

#### Probcut
Use shallow search to predict deep search result:
```cpp
if (depth >= 5) {
    Value rbeta = beta + 200;
    if (shallowSearch(depth - 4, rbeta) >= rbeta)
        return rbeta;
}
```

---

### Phase 7: Automated Tuning Infrastructure (v1.7) ✅ COMPLETE
**Actual: +60–78 ELO (v1.7 vs v1.6)**

Texel Tuning implemented in v1.7 (see `docs/archive/PLAN_Texel_Tuning.md`):
- **PGN Parser Library** (`src/common/pgn/`) — extracted from OpeningBook, 30+ tests
- **Position Extractor** (`src/tuning/extractor/`) — 6 configurable filters, CLI tool
- **Texel Optimizer** (`src/tuning/optimizer/`) — multi-threaded coordinate descent, sigmoid MSE,
  K-tuning, incremental MSE via activation flags, monotonicity constraints, YAML checkpoint/resume
- 75 eval params tuned on 4.57M self-play positions (50 passes, MSE −4.4%)
- **Eval Cleanup:** 14 dead features removed (−503 lines), +3.5% NPS
- Gauntlet: +72 ELO vs v1.6 (200 games), +69 ELO vs SF18 @2700

**Not yet implemented:** SPSA (search parameter tuning) — only eval weights tuned so far.

---

### Phase 8: NNUE Evaluation (v1.8 → v2.0) — **6–10 weeks** 📋 PLANNED
**Focus:** Neural network evaluation for dramatic strength gain
**Target:** +200–400 ELO (largest single improvement)

| Task                       | Effort       | Complexity | ELO Gain | Status     |
|----------------------------|--------------|------------|----------|------------|
| NNUE Architecture Design   | 🟡 1 week    | 🔴 High    | N/A      | 📋 Planned |
| Incremental Update System  | 🔴 2-3 weeks | 🔴 High    | N/A      | 📋 Planned |
| NNUE Inference Engine      | 🟡 1-2 weeks | 🔴 High    | N/A      | 📋 Planned |
| Training Data Generation   | 🟡 1 week    | 🟡 Medium  | N/A      | 📋 Planned |
| Network Training Pipeline  | 🟡 1-2 weeks | 🔴 High    | N/A      | 📋 Planned |
| NNUE Integration & Testing | 🟡 1 week    | 🔴 High    | +200-400 | 📋 Planned |
| Classical/NNUE Hybrid Mode | 🟢 2-3 days  | 🟡 Medium  | N/A      | 📋 Planned |

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

**Implementation Strategy:**
1. **Weeks 1–2:** Architecture + data structures (accumulator, feature tracking)
2. **Weeks 3–5:** Incremental updates (add/remove features on make/unmake), SIMD optimization
3. **Weeks 6–7:** Inference engine (forward propagation, quantized arithmetic, ClippedReLU)
4. **Weeks 8–9:** Training pipeline (self-play data, NNUE-pytorch or custom trainer)
5. **Week 10:** Integration (UCI option `Use NNUE`, `EvalFile`, classical fallback)

**Key Data Structures:**
```cpp
class NNUE {
    alignas(64) int16_t featureWeights[40960][256];
    alignas(64) int16_t l1Weights[256][256];
    alignas(64) int16_t l2Weights[256][1];
    int32_t l1Bias[256], l2Bias[256];
    alignas(64) int16_t accumulator[2][256];  // [side][neurons]
};
```

**Configuration:**
```yaml
USE_NNUE: true
NNUE_NET_PATH: "./networks/nn-default.nnue"
NNUE_FALLBACK_CLASSICAL: true
```

**Risk:** Very High — major architectural change. Requires ML expertise and significant compute.
Can leverage existing NNUE frameworks (Bullet, NNUE-pytorch) and existing trained nets.

---

### Phase 9: CPU Compatibility & Optimization (v2.0) — **1–2 weeks** 📋 PLANNED
**Target:** +15–35 ELO (performance)

| Task                        | Effort      | Complexity | ELO Gain | Status     |
|-----------------------------|-------------|------------|----------|------------|
| Runtime PEXT Detection      | 🟢 2-3 days | 🟡 Medium  | N/A      | 📋 Planned |
| Software PEXT Fallback      | 🟡 3-5 days | 🟡 Medium  | N/A      | 📋 Planned |
| SIMD Optimization (AVX2)    | 🟡 1 week   | 🔴 High    | +10-20   | 📋 Planned |
| Profile-Guided Optimization | 🟢 2-3 days | 🟢 Low     | +5-15    | 📋 Planned |

**Current limitation:** FrankyCPP requires BMI2 (PEXT), excluding pre-2013 CPUs.
Solution: runtime CPU detection + software PEXT fallback (~20% slower but functional).

**SIMD targets:** NNUE inference (AVX2/AVX-512), bitboard ops, move generation.

**PGO workflow:**
```bash
cmake -DCMAKE_CXX_FLAGS="-fprofile-generate" ...  # Build instrumented
./FrankyCPP bench                                   # Collect profile
cmake -DCMAKE_CXX_FLAGS="-fprofile-use" ...        # Rebuild optimized
```

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

**Cumulative ELO Gains (verified):**
- v1.1 → v1.2: +57 ELO (Singular/Check Extensions, Counter-Move History)
- v1.2 → v1.3: +52 ELO (LMR/History fixes, Late Move Pruning)
- v1.3 → v1.4: +119 ELO (Lazy SMP)
- v1.4 → v1.5: +92.5 ELO (TT Buckets, XOR Key, SMP hardening)
- v1.5 → v1.6: +81 ELO (Eval enrichment, search hardening)
- v1.6 → v1.7: +60–78 ELO (Texel tuning, eval cleanup)
- **Total v1.1 → v1.7: ~+400–470 ELO**

---

## Success Metrics

| Metric                   | Baseline (v1.1) | Current (v1.7) | Target (v2.0) |
|--------------------------|-----------------|----------------|---------------|
| **ELO Rating**           | ~2400           | ~2800–2850     | ~2900–3100    |
| **NPS (Single Thread)**  | ~1.5M           | ~2.5M          | ~2.0M+ (NNUE) |
| **NPS (8 Threads)**      | N/A             | ~12M           | ~10–12M       |
| **Test Suite (overall)** | —               | 65%            | 75%+          |
| **WAC**                  | 250/300         | 282/300        | 285/300       |

---

## Future Considerations (Beyond v2.0)

- Multi-PV Search
- Monte Carlo Tree Search (MCTS) hybrid
- Larger NNUE networks (1024x2)
- SPSA for search parameters
- Distributed tuning
- Opening book learning

---

## References

- **Stockfish** — NNUE, Lazy SMP, reference implementation
- **Ethereal** — Clean classical eval, tuning infrastructure
- **Koivisto** — NNUE training pipeline
- **Fathom** — Syzygy tablebase probing
- **NNUE-pytorch** — Network training framework
- **cutechess-cli** — Automated engine matches

---

**Document Maintainer:** Frank Kopp
**Last Updated:** 2026-03-31
**Next Review:** After v1.8 release
