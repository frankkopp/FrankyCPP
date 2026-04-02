# FrankyCPP v1.8 Development Plan

**Document Version:** 1.0  
**Created:** 2026-04-01  
**Status:** 📋 PLANNING  
**Baseline:** v1.7.0 (~2800–2850 ELO, ~+400–470 vs v1.1)  
**Target:** v1.8.0 (~2900–2950 ELO)  
**Focus:** Proven strength gains, missing UCI features, search & move ordering improvements

---

## Guiding Principles

- **No NNUE** — Training infrastructure not available; classical eval stays.
- **Proven techniques only** — Every strength feature must be validated via Arena gauntlet.
- **Missing UCI features** — Fill protocol gaps (MultiPV, `debug`, contempt).
- **Measurable** — Each item has an expected ELO range; reject if neutral or negative.

---

## Quick Wins (Minimal Code Changes)

Small, low-risk items that can be done in hours, not days. Good candidates for warming up
or for filling gaps between larger features.

| #    | Item                                          | Effort     | Category   | Description                                                                                                                                                                                                                                                                                                                               |
|------|-----------------------------------------------|------------|------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| QW1  | ✅ **Remove dead `USE_SINGULAR_TT_BOUND`**     | 🟢 15 min  | Cleanup    | Removed config option, registry entry, conditional in Search.cpp, orphaned stat tracking, and `ttBound` variable. Bound check investigation added as A9.                                                                                                                                                                                  |
| QW2  | ✅ **Remove dead `USE_IID` / `IID_*` config**  | 🟢 30 min  | Cleanup    | Removed IID config fields, 3 registry entries, IID code block in Search.cpp, mutual-exclusion check, IID stats, and stale IID comments/YAML.                                                                                                                                                                                              |
| QW3  | ✅ **Add FEN error handling in `position`**    | 🟢 30 min  | Robustness | Wrapped `Position` construction in try-catch; `uciError()` reports invalid FEN via `info string`, previous position preserved. Tests added.                                                                                                                                                                                               |
| QW4  | ✅ **Smarter book move selection**             | 🟢 1–2 hrs | Strength   | Frequency-weighted book move selection via `getBookMove()`. Uses destination position counter as weight, blended with uniform random via `BOOK_VARIETY` UCI option (0–100, default 30). No cache-breaking changes.                                                                                                                        |
| QW5  | ✅ **Add `ucinewgame` state audit**            | 🟢 30 min  | Robustness | Audited and fixed: `Search::newGame()` now also clears `lastSearchResult`, `resultReady`, and `measuredPostStopOverheadMs`. `UciHandler::uciNewGameCommand()` resets position to startpos. TODO removed; test added.                                                                                                                      |
| QW6  | ✅ **Evasion move generation tests**           | 🟢 1–2 hrs | Testing    | Replaced print-only evasion test with 6 position blocks: move count assertions (pseudo-legal, evasion, legal), structural invariants (evasion ≤ pseudo-legal, legal ≤ evasion), legal⊆evasion containment check, en passant evasion (pos 4), no-castling-in-check (pos 1), double-check king-only (pos 6), illegal evasion delta (pos 5). |
| QW7  | ✅ **Sort value / history ordering tests**     | 🟢 1–2 hrs | Testing    | Replaced `TODO real tests` in `sortValueTest` with 5 assertion groups: PV move first with VALUE_MAX, monotonic sort order, killer ordering (1001 before 1000), value-zone check (captures > killers > quiet), no non-PV exceeds VALUE_MAX. Also removed stale T9 TODO comment in `MoveGenerator.cpp`.                                     |
| QW8  | ✅ **History & counter-move sort value tests** | 🟢 1–2 hrs | Testing    | New `sortValueWithHistoryTest`: seeds `History` struct with known counts, verifies exact history boost (+500 = 50000/100), exact counter-move boost (+600 = history 100 + counter 500), counter-boosted sorts above history-only, compares against no-history baseline to confirm value delta.                                            |
| QW9  | **TestSuite config reset fix**                | 🟢 30 min  | Robustness | `TestSuite.cpp:150` — FIXME about hardcoded config reset. Allow test suite to run with custom configs by removing the forced reset.                                                                                                                                                                                                       |
| QW10 | **Bench hash for CI regression gate**         | 🟢 1 hr    | CI         | Same as E3. Record deterministic bench hash; assert in CI. Catches accidental search changes. Near-zero maintenance.                                                                                                                                                                                                                      |

---

## Feature Candidates

### A. Search Improvements (Strength)

| #  | Feature                          | Expected ELO | Effort      | Complexity | Notes                                                                                                                                                                                                                           |
|----|----------------------------------|--------------|-------------|------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| A1 | **Continuation History (2-ply)** | +15–25       | 🟡 3–5 days | 🟡 Medium  | Track `[prevPiece][prevTo][piece][to]` for quiet moves. Stockfish/Ethereal standard. Complement existing counter-move table.                                                                                                    |
| A2 | **Probcut Pruning**              | +10–15       | 🟡 3–5 days | 🟡 Medium  | Shallow search with raised beta predicts deep result; prune if exceeds. Stockfish uses `depth >= 5`, reduced by 4.                                                                                                              |
| A3 | **SEE Pruning for Quiet Moves**  | +5–10        | 🟢 2–3 days | 🟡 Medium  | Prune quiet moves with bad SEE at low depths. Tested ELO-neutral in v1.5 — retry with better tuning after continuation hist.                                                                                                    |
| A4 | **SPSA Search Tuning**           | +15–30       | 🟡 1–2 wks  | 🟡 Medium  | Tune LMR, NMP, RFP, FP, ASP, singular margins via SPSA (fishtest-style). Texel only covered eval; search params untouched.                                                                                                      |
| A5 | **Improving Flag in LMP**        | +3–8         | 🟢 1 day    | 🟢 Low     | Already have USE_LMP_IMPROVING config. Verify it's active & correctly tuned; may need threshold adjustment.                                                                                                                     |
| A6 | **Multi-Cut Pruning**            | +10–20       | 🟡 3–5 days | 🟡 Medium  | If ≥C moves fail high at reduced depth, assume beta cutoff. Stockfish-style; effective at high depths.                                                                                                                          |
| A7 | **QSearch Quiet Checks**         | +10–20       | 🟡 3–5 days | 🟡 Medium  | Implementation exists (archived) but was tested with config disabled. Needs proper retest. Avoids horizon effect on checks.                                                                                                     |
| A8 | **Contempt / Draw Score Bias**   | +5–15        | 🟢 1–2 days | 🟢 Low     | Return non-zero for draws (e.g., +10 cp vs weaker, −10 vs stronger). Simple UCI option `Contempt`. Avoids early draws.                                                                                                          |
| A9 | **Singular Ext Bound Check**     | +0–10        | 🟢 1–2 days | 🟢 Low     | Stockfish requires `BETA/EXACT` TT bound for singular ext. FrankyCPP had this but disabled it (claimed 99.98% filtered — suspicious, investigate distribution). Re-implement Stockfish-style with stats; validate via gauntlet. |

**Subtotal potential: +73–143 ELO** (not all additive; realistic estimate +50–80 after validation)

---

### B. Move Ordering Improvements

| #  | Feature                          | Expected ELO | Effort      | Complexity | Notes                                                                                                         |
|----|----------------------------------|--------------|-------------|------------|---------------------------------------------------------------------------------------------------------------|
| B1 | **Continuation History (2-ply)** | (see A1)     |             |            | Listed here for cross-ref — primary move ordering improvement.                                                |
| B2 | **Capture History (Revisit)**    | 0–10         | 🟡 3–5 days | 🟡 Medium  | Failed in v1.5 (disrupted MVV-LVA). Revisit with hybrid: MVV-LVA primary, capture history as tiebreaker only. |
| B3 | **Counter-Move History Scoring** | +5–10        | 🟢 2–3 days | 🟢 Low     | Current counter-move stores one move; add score weighting to prefer high-confidence counter moves.            |

---

### C. Missing UCI Features (Functionality)

| #  | Feature                        | Expected ELO | Effort      | Complexity | Notes                                                                                                      |
|----|--------------------------------|--------------|-------------|------------|------------------------------------------------------------------------------------------------------------|
| C1 | **MultiPV**                    | N/A          | 🟢 1–2 days | 🟡 Medium  | Existing plan in `PLAN_MultiPV.md`. Essential for analysis GUIs. Standard UCI option, not yet implemented. |
| C2 | **UCI `debug` command**        | N/A          | 🟢 0.5 day  | 🟢 Low     | Currently prints "not implemented". Should toggle verbose info strings (eval breakdown, search stats).     |
| C3 | **`info string` eval details** | N/A          | 🟢 1 day    | 🟢 Low     | When debug=on, send `info string` with eval breakdown (material, positional, mobility, etc.) per move.     |
| C4 | **UCI `Contempt` option**      | (see A8)     |             |            | Cross-ref with A8 — the UCI option part.                                                                   |

---

### D. Tuning & Infrastructure

| #  | Feature                           | Expected ELO | Effort      | Complexity | Notes                                                                                                  |
|----|-----------------------------------|--------------|-------------|------------|--------------------------------------------------------------------------------------------------------|
| D1 | **SPSA Tuning Framework**         | (see A4)     | 🟡 1–2 wks  | 🟡 Medium  | Extend existing Texel infra. Needs parameter perturbation, match-based feedback, convergence tracking. |
| D2 | **Profile-Guided Optimization**   | +5–15        | 🟢 2–3 days | 🟢 Low     | Add CMake PGO support: instrument build → run bench → optimized rebuild. Free NPS gain.                |
| D3 | **Re-tune eval after new search** | +5–15        | 🟡 3–5 days | 🟢 Low     | After adding continuation history / probcut, re-run Texel tuning. Search changes shift eval optima.    |
| D4 | **Runtime PEXT Detection**        | N/A (compat) | 🟡 3–5 days | 🟡 Medium  | CPUID check + software PEXT fallback. Broadens hardware compatibility. No ELO gain on BMI2 hardware.   |

---

### E. Code Quality & Robustness

| #  | Feature                             | Expected ELO | Effort     | Complexity | Notes                                                                                         |
|----|-------------------------------------|--------------|------------|------------|-----------------------------------------------------------------------------------------------|
| E1 | **SEE unit test expansion**         | N/A          | 🟢 1 day   | 🟢 Low     | Current SEE tested in integration; add dedicated edge-case unit tests (pins, x-rays, promos). |
| E2 | **Search regression test baseline** | N/A          | 🟢 1 day   | 🟢 Low     | Fixed-depth node count assertions for key positions. Catch accidental search changes early.   |
| E3 | **Bench hash stability**            | N/A          | 🟢 0.5 day | 🟢 Low     | `bench` command outputs a deterministic hash. Store expected hash in CI to catch regressions. |

---

## Proposed Implementation Order

Ordered by confidence of ELO gain and dependency chain:

### Phase 1 — Quick Wins & UCI Features (Week 1–2)
1. **C1: MultiPV** — High-value, existing plan, essential for analysis use.
2. **C2: UCI debug** — Trivial implementation, fills protocol gap.
3. **A8/C4: Contempt** — Simple draw score adjustment, measurable ELO impact.
4. **E3: Bench hash stability** — Safety net before making search changes.

### Phase 2 — Core Search Enhancements (Week 3–5)
5. **A1: Continuation History** — Biggest expected single-feature gain. Prerequisite for A3.
6. **A2: Probcut** — Independent of A1, can be developed in parallel.
7. **A6: Multi-Cut** — If probcut validates well, multi-cut adds similar pruning power.
8. **A7: QSearch Quiet Checks** — Retest existing code with proper config.

### Phase 3 — Tuning & Optimization (Week 6–8)
9. **D1/A4: SPSA Search Tuning** — Tune all search margins/thresholds scientifically.
10. **D3: Re-tune eval** — After search changes settle, retune eval weights.
11. **D2: PGO** — Free NPS; add CMake support for instrumented builds.

### Phase 4 — Stretch Goals (If Time Permits)
12. **B2: Capture History Revisit** — Only if continuation history changes move ordering landscape.
13. **B3: Counter-Move History Scoring** — Incremental improvement.
14. **D4: Runtime PEXT fallback** — Compatibility only, no strength gain.

---

## ELO Budget Estimate

| Phase     | Features                               | Expected ELO (conservative) | Expected ELO (optimistic) |
|-----------|----------------------------------------|-----------------------------|---------------------------|
| Phase 1   | MultiPV, debug, contempt               | +5                          | +15                       |
| Phase 2   | ContHist, probcut, multi-cut, QSChecks | +30                         | +65                       |
| Phase 3   | SPSA, eval retune, PGO                 | +20                         | +50                       |
| Phase 4   | Capture hist, counter-move scoring     | +0                          | +15                       |
| **Total** |                                        | **+55**                     | **+145**                  |

**Realistic target: +60–80 ELO** (consistent with v1.6 and v1.7 release gains)

---

## Validation Protocol

Each strength feature follows the standard validation process:
1. **Implementation** with config toggle (`USE_*` / `CONFIG_CONST`)
2. **Unit tests** for correctness
3. **Arena gauntlet** — 200+ games vs v1.7 baseline, 95% confidence
4. **Test suite delta** — WAC, STS accuracy comparison
5. **Node count comparison** — Verify tree size reduction (where applicable)
6. **Merge only if ELO ≥ +5** with statistical significance

---

## Risk Assessment

| Risk                              | Likelihood | Impact | Mitigation                                                   |
|-----------------------------------|------------|--------|--------------------------------------------------------------|
| Continuation history ELO-neutral  | Low        | Medium | Well-proven in many engines; if neutral, check indexing bugs |
| SPSA tuning insufficient data     | Medium     | Medium | Use ≥5000 games per iteration; accept slower convergence     |
| Probcut regression on tactics     | Medium     | Low    | Configurable depth/margin; disable at low depth              |
| Multi-cut too aggressive          | Medium     | Low    | Conservative defaults (C=3, depth≥8); tune via SPSA later    |
| Capture history disrupts ordering | High       | Low    | Already failed once; only attempt as hybrid tiebreaker       |

---

## Success Criteria for v1.8 Release

- [ ] **MultiPV** working correctly (verified by GUI analysis)
- [ ] **UCI debug** implemented (info string eval breakdown)
- [ ] **≥1 major search feature** validated at +10 ELO or more
- [ ] **Overall ≥+40 ELO** vs v1.7 in Arena gauntlet (200+ games)
- [ ] All existing 266+ tests still passing
- [ ] No search regressions (bench hash, node counts)

---

## References

- `docs/specs/PLAN_MultiPV.md` — Detailed MultiPV implementation plan
- `docs/specs/V1_ENGINE_STRENGTH_ROADMAP.md` — Overall strength roadmap
- `docs/specs/V1_ENGINE_ENHANCEMENT_PLAN.md` — Phase-based enhancement plan
- `docs/archive/PLAN_Move_Ordering_Improvements.md` — Capture history failure analysis
- `docs/archive/PLAN_QSearch_Quiet_Checks.md` — QSearch checks implementation (archived)
- `docs/Search_Features.md` — Complete search feature inventory

---

## Appendix: TODO/FIXME Inventory

Codebase scan as of 2026-04-01. Each item evaluated for v1.8 relevance.

### Source Code TODOs

| #   | Location                     | TODO Text (Summary)                                                   | Value     | Action for v1.8                                                                                                              |
|-----|------------------------------|-----------------------------------------------------------------------|-----------|------------------------------------------------------------------------------------------------------------------------------|
| T1  | `TT.h:65`                    | Consider removing XOR key verification (Stockfish showed it may harm) | 🟡 Medium | **Defer** — needs strength testing; risky SMP change. Profile first via SPSA (Phase 3).                                      |
| T2  | `SearchConfigData.h:195`     | Remove `USE_SINGULAR_TT_BOUND` option (permanently `false`)           | ✅ Done    | **QW1 complete** — Option, registry entry, Search.cpp conditional, stat tracking, ttBound var all removed.                   |
| T3  | `UciHandler.cpp:157`         | Check if `ucinewgame` clears enough state                             | ✅ Done    | **QW5 complete** — Audited; added `lastSearchResult`/`resultReady`/`measuredPostStopOverheadMs` resets, position→startpos.   |
| T4  | `UciHandler.cpp:181`         | Error handling when FEN is invalid                                    | ✅ Done    | **QW3 complete** — try-catch around `Position` construction; `uciError()` reports, previous position preserved. Tests added. |
| T5  | `Search.cpp:287`             | Select book move by score/variation instead of random                 | ✅ Done    | **QW4 complete** — `getBookMove()` with frequency-weighted selection and `BOOK_VARIETY` UCI option.                          |
| T6  | `Search.cpp:501`             | Remove IID/IIR mutual-exclusion check after removing IID              | ✅ Done    | **QW2 complete** — IID code, config, stats, mutual-exclusion check, and stale comments all removed.                          |
| T7  | `Search.cpp:1358`            | Test RFP improving margin with different values                       | ✅ Done    | **QW1 side-effect** — Stale TODO comment removed. Tuning folded into A4 (SPSA).                                              |
| T8  | `Search.cpp:1694`            | Test FP improving margin with different values                        | 🟡 Medium | **→ A4 (SPSA)** — Fold into search param tuning.                                                                             |
| T9  | `MoveGenerator.cpp:789`      | Consider using non-stable sort                                        | ✅ Done    | **QW7 side-effect** — Stale TODO comment removed. `moveSort` is already `std::ranges::sort` (unstable).                      |
| T10 | `MoveGenerator.cpp:826`      | Testing for history count sort value logic                            | ✅ Done    | **QW8 complete** — `sortValueWithHistoryTest` verifies exact history boost (+500) and baseline comparison.                   |
| T11 | `MoveGenerator.cpp:833`      | Testing for counter-move sort value logic                             | ✅ Done    | **QW8 complete** — `sortValueWithHistoryTest` verifies exact counter-move boost (+600) and positional ordering.              |
| T12 | `OpeningBook.h:74`           | ABK format support                                                    | 🔴 Low    | **Skip** — No demand; SIMPLE/SAN/PGN cover all common formats.                                                               |
| T13 | `SearchTreeSizeTest.cpp:361` | LMP needs tuning and more test points                                 | 🟡 Medium | **→ A4 (SPSA)** — SPSA will tune LMP thresholds.                                                                             |
| T14 | `SearchTreeSizeTest.cpp:432` | Tablebases need more testing and tuning                               | 🟡 Medium | **Defer** — TB config already works well; revisit if issues found.                                                           |

### Source Code FIXMEs

| #  | Location            | FIXME Text (Summary)                                               | Value   | Action for v1.8                                             |
|----|---------------------|--------------------------------------------------------------------|---------|-------------------------------------------------------------|
| F1 | `TestSuite.cpp:150` | Config reset in `runAllTests()` prevents testing different configs | 🟢 Easy | **→ QW9** — Remove forced reset; let caller control config. |

### Test Code TODOs

| #  | Location                    | TODO Text (Summary)                                 | Value  | Action for v1.8                                                               |
|----|-----------------------------|-----------------------------------------------------|--------|-------------------------------------------------------------------------------|
| X1 | `MoveGeneratorTest.cpp:669` | Evasion test — `TODO - real tests` (no assertions)  | ✅ Done | **QW6 complete** — 6 positions with full assertions.                          |
| X2 | `MoveGeneratorTest.cpp:760` | Sort value test — `TODO real tests` (no assertions) | ✅ Done | **QW7 complete** — 5 assertion groups + new `sortValueWithHistoryTest` (QW8). |

### Summary

- **10 items** completed via Quick Wins (QW1–QW8) or as side-effects (T7, T9)
- **2 items** folded into feature phases (T8, T13 → A4 SPSA)
- **2 items** deferred (T1 TT XOR, T14 TB tuning — low priority / high risk)
- **1 item** skipped (T12 ABK format — no demand)

---

*Created: 2026-04-01*  
*Next Review: After Phase 1 completion*
