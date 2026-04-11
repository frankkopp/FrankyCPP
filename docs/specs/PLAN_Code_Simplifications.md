# FrankyCPP — Code Simplification & Cleanup Plan

**Document Version:** 2.4
**Created:** 2026-04-11
**Last updated:** 2026-04-12
**Status:** 📋 IN PROGRESS (S1 ✅, S2 ✅, S11 ✅, S12 ✅, S13 ✅, S14 ✅, S20 ✅ via S1, S22 ✅)
**Scope:** `src/engine/`, `src/chesscore/`, `src/tablebase/`, `src/types/`

---

## Goal

Reduce code duplication, verbosity, and unnecessary complexity across the engine and
chess-core modules while identifying performance improvement opportunities. All changes
must preserve identical behavior (same search results, same evaluation scores) unless
explicitly flagged as behavior-changing (requires Elo testing). The result should be
easier to read, maintain, and extend — without sacrificing a single nanosecond in hot code.

---

## Guiding Principles

- **⚠️ EVERY NANOSECOND COUNTS IN HOT CODE.**
  `evaluate()`, `search()`, `qsearch()`, and `moveGen()` are called millions of times per second.
  Every unnecessary function call, lambda, abstraction layer, or stack allocation that the compiler
  cannot fully eliminate **will** cost measurable NPS. Do not assume "the compiler will optimize
  it away" — **verify with bench timing**, not just signature. Generic lambdas, `std::function`,
  indirect calls, and unnecessary object initialization are **banned** in hot paths.
  *Lesson learned (S1):* A generic `auto&&` lambda wrapping eval calls caused a 6% NPS regression
  despite being "logically zero-cost" — 6 lambda instantiations bloated code size and trashed
  the instruction cache. Replaced with direct inline code, regression gone.
- **Behavior-preserving:** Identical bench signature before and after (unless flagged otherwise).
- **Performance first in hot code:** Never sacrifice speed for aesthetics in search, movegen, eval.
- **Readability first in cold code:** Simplifications in display/stats/init code prioritize clarity.
- **No over-engineering:** Don't add abstraction layers that obscure intent.
- **One step at a time:** Each item is independently implementable and testable.
- **Verify with bench:** After each change, run `--bench --threads 1` and confirm signature matches.
  **Also compare NPS** (run multiple times) to catch performance regressions that don't change the
  signature.
- **Test before refactoring (when needed):** For complex items, ensure adequate unit test coverage
  exists *before* changing the code. Each item notes the existing test coverage and whether
  additional tests should be written first.

---

## Test Coverage Overview

Existing test files relevant to this plan:

| Test File                       | Tests | Covers                                                                                                 |
|---------------------------------|-------|--------------------------------------------------------------------------------------------------------|
| `test/engine/EvaluatorTest.cpp` | ~35   | Material, mobility, pawns, pieces, king safety, threats, coordination. **No `evaluateTrace()` tests.** |
| `test/engine/SearchTest.cpp`    | ~40   | Search mechanics, mate/stalemate, time control, LMR, singular extensions, MultiPV, handicap.           |
| `test/engine/SeeTest.cpp`       | 4     | `attacksTo`, `revealedAttacks`, `leastValuablePiece`, `see` scoring.                                   |
| `test/engine/TT_Test.cpp`       | ~18   | Entry size, put/probe, bucket replacement, concurrent access.                                          |
| `test/engine/SearchSmpTest.cpp` | —     | SMP-specific search tests (stats aggregation indirectly).                                              |
| `test/chesscore/PerftTest.cpp`  | —     | Full move generation correctness (perft node counts).                                                  |

**Not tested:** `formatDetailedStats()`, `SearchStats::operator+=`, `Evaluator::reset()`, `evaluateTrace()`.

---

## Summary Table

| #   | Item                                            | Category      | Severity | Effort     | Risk   | Test Coverage                  | Status |
|-----|-------------------------------------------------|---------------|----------|------------|--------|--------------------------------|--------|
| S1  | Deduplicate `evaluate()` / `evaluateTrace()`    | REDUNDANCY    | HIGH     | 🟡 2–3 hrs | 🟢 Low | ✅ Eval tests + bench           | ✅      |
| S2  | Loop over piece types in `pieceEval()` calls    | REDUNDANCY    | LOW      | 🟢 15 min  | 🟢 Low | ✅ Eval tests + bench           | ✅      |
| S3  | Extract TT bound-type stats helper              | REDUNDANCY    | MEDIUM   | 🟢 30 min  | 🟢 Low | ✅ SearchTest + bench           | —      |
| S4  | ~~`formatDetailedStats()` helpers~~ → S16       | REDUNDANCY    | —        | —          | —      | —                              | → S16  |
| S5  | Loop in `See::getLeastValuablePiece()`          | REDUNDANCY    | LOW      | 🟢 15 min  | 🟢 Low | ✅ SeeTest.leastValuablePiece   | —      |
| S6  | Add `Score::addSigned()` helper                 | REDUNDANCY    | MEDIUM   | 🟢 30 min  | 🟢 Low | ✅ Eval tests + bench           | —      |
| S7  | Eliminate `if (mid \|\| end)` guard pattern     | PERFORMANCE   | LOW      | 🟢 15 min  | 🟢 Low | ✅ Eval tests + bench           | —      |
| S8  | Collapse king-safety safe-check blocks          | REDUNDANCY    | MEDIUM   | 🟢 30 min  | 🟢 Low | ✅ SafeCheck eval tests         | —      |
| S9  | `SearchStats::operator+=` via field list macro  | REDUNDANCY    | MEDIUM   | 🟡 1–2 hrs | 🟡 Med | ❌ No operator+= tests          | —      |
| S10 | Deduplicate TT-move validation pattern          | REDUNDANCY    | MEDIUM   | 🟢 15 min  | 🟢 Low | ✅ SearchTest + bench           | —      |
| S11 | Bulk pawn attack computation (shift vs loop)    | PERFORMANCE   | CRITICAL | 🟡 1 hr    | 🟢 Low | ✅ Eval tests + bench           | ✅      |
| S12 | King safety attack dedup + precompute kingzone  | PERF+REDUND   | MEDIUM   | 🟢 30 min  | 🟢 Low | ✅ KingSafety eval tests        | ✅      |
| S13 | Remove unnecessary variable in qsearch drop     | READABILITY   | LOW      | 🟢 5 min   | 🟢 Low | ✅ Bench (mechanical)           | ✅      |
| S14 | Move `do_null` before move loop                 | READABILITY   | LOW      | 🟢 5 min   | 🟢 Low | ✅ Bench (mechanical)           | ✅      |
| S15 | Draw-check pattern (dependent on S21)           | REDUNDANCY    | LOW      | 🟢 15 min  | 🟢 Low | ✅ SearchTest (indirectly)      | —      |
| S16 | `formatDetailedStats()` comprehensive cleanup   | REDUNDANCY    | MEDIUM   | 🟡 1–2 hrs | 🟢 Low | ❌ No output format tests       | —      |
| S17 | TT `aggregateStats()` pattern duplication       | REDUNDANCY    | LOW      | 🟢 30 min  | 🟢 Low | ✅ TT_Test (indirectly)         | —      |
| S18 | TT getter batch aggregation waste               | PERFORMANCE   | MEDIUM   | 🟢 30 min  | 🟢 Low | ✅ TT_Test (indirectly)         | —      |
| S19 | History decay: allow negative values            | CHESS_PATTERN | HIGH     | 🟢 15 min  | 🟡 Med | ⚠️ Needs Elo test              | —      |
| S20 | `passedPawns[]` fallback duplication            | REDUNDANCY    | MEDIUM   | 🟢 15 min  | 🟢 Low | ✅ Eval tests + bench           | ✅ S1   |
| S21 | Beta-cut stats block duplication                | REDUNDANCY    | MEDIUM   | 🟢 30 min  | 🟢 Low | ✅ Bench (stats only)           | —      |
| S22 | `EVAL_PREFETCH` inconsistency search vs qsearch | PERFORMANCE   | HIGH     | 🟢 5 min   | 🟢 Low | ✅ Bench (no behavioral change) | ✅      |
| S23 | `pieceEval` switch unreachable default          | DEAD_CODE     | LOW      | 🟢 5 min   | 🟢 Low | ✅ Eval tests + bench           | —      |
| S24 | `relRank` computation duplication               | REDUNDANCY    | LOW      | 🟢 30 min  | 🟢 Low | ✅ Eval tests + bench           | —      |
| S25 | `Evaluator::reset()` is empty                   | DEAD_CODE     | LOW      | 🟢 5 min   | 🟢 Low | ✅ Trivial removal              | —      |

**Legend:** ✅ Existing tests sufficient — ⚠️ Tests needed before/during implementation — ❌ No tests (acceptable if low risk)

**Dependencies:** S7 depends on S6. S20 is resolved by S1. S4 is superseded by S16. S15 depends on S21.

---

## Detailed Proposals

### S1: ✅ Deduplicate `evaluate()` / `evaluateTrace()`

**Status:** ✅ COMPLETE
**File:** `src/engine/Evaluator.cpp`, `src/engine/Evaluator.h`
**Category:** REDUNDANCY — **Severity:** HIGH — **Confidence:** CERTAIN

**Problem:** `evaluate()` and `evaluateTrace()` shared ~90% identical code. The only difference
was that `evaluateTrace()` recorded per-component score deltas into an `EvalTrace` struct.

**Solution:** `template<bool Trace> evaluateCore()` with `std::conditional_t` return type:

```cpp
// Evaluator.h — private:
template<bool Trace>
std::conditional_t<Trace, EvalTrace, Value> evaluateCore(const Position& p);

// Evaluator.cpp — thin wrappers:
Value Evaluator::evaluate(const Position& p) { return evaluateCore<false>(p); }
EvalTrace Evaluator::evaluateTrace(const Position& p) { return evaluateCore<true>(p); }
```

Key design choices:
- **Conditional return type** — `<false>` returns `Value`, `<true>` returns `EvalTrace`. No pointer
  parameter, no null. Local `EvalTrace trace{}` optimized away for `<false>`.
- **Direct `if constexpr` blocks** — `[[maybe_unused]] const Score before = score;` captures the
  score before each eval section; `if constexpr (Trace)` records the delta. Standard C++17 idiom.
- **No lambdas in hot code** — an earlier version used a generic `runEvalFn` lambda but it caused
  a 6% NPS regression (i-cache bloat from 6 `auto&&` instantiations). Removed in favor of direct code.
- **Explicit instantiations** in .cpp — keeps template definition out of the header.

**Lines saved:** ~140 — **Risk:** Low — verified by bench signature.
**Note:** Also resolved S20 (passedPawns fallback duplication).

**Test coverage:** Bench signature unchanged. evaluateTrace() is debug-only output — unit tests
deemed unnecessary (trace correctness is guaranteed by shared code path with evaluate()).

---

### S2: ✅ Loop over piece types in `pieceEval()` calls

**Status:** ✅ COMPLETE
**File:** `src/engine/Evaluator.cpp`
**Category:** REDUNDANCY — **Severity:** LOW — **Confidence:** CERTAIN

**Solution:** Replaced 8 explicit calls with nested loop `for (pt = KNIGHT..QUEEN) × Color::all()`.
Single call site inside S1's `evaluateCore` template.

**Lines saved:** ~4 — **Risk:** Low — iteration order preserved (piece-type outer, color inner).

**Test coverage:** ✅ Verified by bench signature + existing eval tests.

---

### S3: Extract TT bound-type stats helper

**File:** `src/engine/Search.cpp` (3 identical switch blocks)
**Category:** REDUNDANCY — **Severity:** MEDIUM — **Confidence:** CERTAIN

**Solution:** Inline helper `trackTtBoundType(SearchStats&, ValueType)` replaces 3 × 6-line switches.

**Lines saved:** ~24 — **Risk:** Low — pure statistics.

**Test coverage:** ✅ Pure stats tracking — no behavioral change. Bench verifies search correctness.
SearchTest covers TT interaction functionally. No additional tests needed.

---

### S4: ~~Simplify `formatDetailedStats()`~~ → Superseded by S16

---

### S5: Loop in `See::getLeastValuablePiece()`

**File:** `src/engine/See.cpp` (lines 86–107)
**Category:** REDUNDANCY — **Severity:** LOW — **Confidence:** CERTAIN

**Solution:** Replace 6 sequential if-blocks with a loop over `{PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}`.

**Lines saved:** ~12 — **Risk:** Low — identical behavior.

**Test coverage:** ✅ `SeeTest.leastValuablePiece` directly tests this function with multiple
positions and piece configurations. `SeeTest.seeTest` validates end-to-end SEE scoring.
No additional tests needed.

---

### S6: Add `Score::addSigned()` helper

**File:** `src/types/score.h`, `src/engine/Evaluator.cpp`
**Category:** REDUNDANCY — **Severity:** MEDIUM — **Confidence:** CERTAIN

**Problem:** `s.midgame += static_cast<Value>(mid * us.sign()); s.endgame += ...` appears
11 times across 7 eval functions.

**Solution:** Free function `addSigned(Score&, int mid, int end, int sign)` in `score.h`.

**Lines saved:** ~40 — **Risk:** Low — trivially equivalent. Foundation for S7.

**Test coverage:** ✅ Eval tests cover all call sites (knight, bishop, rook, queen, king, threat,
coordination). Bench catches any accumulation error. Consider adding a small unit test for
`addSigned()` itself (3-4 lines, trivial) in a new `ScoreTest.cpp` or in `TypesTest.cpp`.
**Recommendation:** Optional — existing coverage is sufficient.

---

### S7: Eliminate `if (mid || end)` guard pattern

**File:** `src/engine/Evaluator.cpp` (rookEval, queenEval, threatEval, coordinationEval)
**Category:** PERFORMANCE — **Severity:** LOW — **Confidence:** CERTAIN — **Depends on:** S6

**Solution:** Remove guard; always accumulate. Combined with S6: `addSigned(s, mid, end, us.sign());`

**Lines saved:** ~16 — **Risk:** Low — adding zero is a no-op.

**Test coverage:** ✅ Same eval tests as S6. Bench verifies identical output. No additional tests needed.

---

### S8: Collapse king-safety safe-check blocks

**File:** `src/engine/Evaluator.cpp` (kingEval, 3 near-identical blocks)
**Category:** REDUNDANCY — **Severity:** MEDIUM — **Confidence:** CERTAIN

**Solution:** Lambda `countSafeChecks(PieceType, weight)` with ternary for knight vs slider attacks.

**Lines saved:** ~15 — **Risk:** Low — identical logic.

**Test coverage:** ✅ `SafeCheck_ExposedKingWorseThanSheltered` and `SafeCheck_ToggleChangesEval`
directly test this functionality. Bench catches regressions. No additional tests needed.

---

### S9: `SearchStats::operator+=` via X-macro

**File:** `src/engine/SearchStats.h` (85-line operator)
**Category:** REDUNDANCY — **Severity:** MEDIUM — **Confidence:** CERTAIN

**Problem:** 50+ field-by-field additions. Adding a new stat field requires updating operator+=
manually. Forgetting causes silent data loss in SMP aggregation.

**Solution:** X-macro defining all cumulative fields, used for both declarations and `operator+=`.

**Lines saved:** ~50 — **Risk:** Medium — X-macros reduce readability.

**Test coverage:** ❌ No direct tests for `operator+=`. SMP tests (`SearchSmpTest.cpp`) exercise
aggregation indirectly but don't verify field-by-field correctness.
**Recommendation:** Write a unit test that creates two SearchStats with known values, adds them,
and verifies every cumulative field. This test catches both the current code and the X-macro
version. Write test first, then refactor.

---

### S10: Deduplicate TT-move validation pattern

**File:** `src/engine/Search.cpp` (3 identical patterns)
**Category:** REDUNDANCY — **Severity:** MEDIUM — **Confidence:** CERTAIN

**Solution:** `[[nodiscard]] inline Move validateTtMove(const Position&, uint16_t rawMove)`

**Lines saved:** ~8 — **Risk:** Low — identical validation logic.

**Test coverage:** ✅ SearchTest covers TT move usage in search/qsearch. TT_Test covers
put/probe mechanics. Bench verifies functional equivalence. No additional tests needed.

---

### S11: ✅ Bulk pawn attack computation (shift vs loop)

**Status:** ✅ COMPLETE
**File:** `src/engine/Evaluator.cpp`
**Category:** PERFORMANCE — **Severity:** CRITICAL — **Confidence:** HIGH

Replaced per-square loop with `shifted(NE) | shifted(NW)` bulk computation, matching
the MoveGenerator pattern.

**Test coverage:** ✅ Covered by eval tests + bench. Verified post-implementation.

---

### S12: ✅ King safety attack dedup + precompute `enemyKingZone`

**Status:** ✅ COMPLETE
**File:** `src/engine/Evaluator.cpp` (knightEval, bishopEval, rookEval, queenEval)
**Category:** PERFORMANCE + REDUNDANCY — **Severity:** MEDIUM — **Confidence:** CERTAIN

**Problem:** Identical 5-line king safety block in all 4 piece evals. `enemyKingZone`
lookup (`Bitboards::nonSliderAttacks[KING][p.getKingSquare(them)]`) recomputed up to
8 times per `evaluateCore()` call.

**Solution:** Reused `attackedByPT[KING][them]` — already computed once in `evaluateCore()`'s
pre-compute block and never modified afterward. No new member variable needed.
Each 5–7 line block collapsed to 3 lines with a merged `if` condition. Added clarifying
comment in the pre-compute block documenting the dual use.

**Lines saved:** ~16 — **Risk:** Low — behavioral equivalent, verified by bench signature.

**Test coverage:** ✅ `KingSafety_AttackedKingWorseThanSafe` tests king safety scoring.
Multiple eval tests exercise piece evals that accumulate king attack data. Bench catches
regressions.

---

### S13: ✅ Remove unnecessary variable in qsearch drop

**Status:** ✅ COMPLETE
**File:** `src/engine/Search.cpp`
**Category:** READABILITY — **Severity:** LOW — **Confidence:** CERTAIN

Replaced `const auto value = qsearch(...); return value;` with `return qsearch(...);`

**Lines saved:** 1 — **Risk:** None.

**Test coverage:** ✅ Mechanical change. Bench is sufficient.

---

### S14: ✅ Move `do_null` before move loop

**Status:** ✅ COMPLETE
**File:** `src/engine/Search.cpp`
**Category:** READABILITY — **Severity:** LOW — **Confidence:** CERTAIN

`matethreat` is constant after the NMP block. Moved `do_null` computation from inside the
move loop (per-iteration) to the "prepare move loop" block (computed once).

**Lines saved:** 1 — **Risk:** None.

**Test coverage:** ✅ Mechanical change. Bench is sufficient. SearchTest mate tests cover
null-move/matethreat interaction.

---

### S15: Draw-check pattern extraction (dependent on S21)

**File:** `src/engine/Search.cpp`
**Category:** REDUNDANCY — **Severity:** LOW — **Confidence:** HIGH

No standalone change — only worthwhile if combined with S21.

**Test coverage:** ✅ SearchTest covers draw detection (stalemate, repetition). Bench sufficient.

---

### S16: `formatDetailedStats()` comprehensive cleanup (supersedes S4)

**File:** `src/engine/Search.cpp` (lines 3567–3837)
**Category:** REDUNDANCY — **Severity:** MEDIUM — **Confidence:** CERTAIN

**Problem:** 270 lines with 3 repeating formatting patterns.

**Solution:** Three local lambdas (`line`, `pctLine`, `pctLineFmt`).

**Lines saved:** ~120–150 — **Risk:** Low — pure display code.

**Test coverage:** ❌ No tests for output formatting. This is cold-path display code with no
behavioral impact. Manual visual inspection of before/after output is sufficient.
**Recommendation:** No tests needed. Optionally snapshot-test the output string for one known
SearchResult, but this is low priority and fragile (any stats change breaks the snapshot).

---

### S17: TT aggregate function pattern duplication

**File:** `src/engine/TT.h`
**Category:** REDUNDANCY — **Severity:** LOW — **Confidence:** CERTAIN

**Solution:** Add `operator+=` to both Stats structs, then generic loop.

**Lines saved:** ~15 — **Risk:** None — cold path.

**Test coverage:** ✅ TT_Test exercises probe/put paths that update stats. Aggregation is
tested indirectly via `hashFull()` and `getNumberOf*()` getters in TT_Test. No additional tests needed.

---

### S18: TT getter batch aggregation waste

**File:** `src/engine/TT.h`
**Category:** PERFORMANCE — **Severity:** MEDIUM — **Confidence:** HIGH

**Solution:** Expose `getAggregatedStats()` publicly; callers aggregate once.

**Lines saved:** ~10 — **Risk:** None — cold path.

**Test coverage:** ✅ Same as S17. No additional tests needed.

---

### S19: History decay — allow negative values

**File:** `src/engine/Search.cpp` (lines 2070–2073)
**Category:** CHESS_PATTERN — **Severity:** HIGH — **Confidence:** MEDIUM

**Problem:** History count clamped to 0 on fail-low, discarding negative information.

**Solution:** Remove the clamp-to-zero. Optionally add a floor (e.g., -16384).

**Lines saved:** 3
**Risk:** ⚠️ MEDIUM — **changes search behavior**. Bench signature will change.

**Test coverage:** ⚠️ No unit tests for history decay specifically. This requires **Elo testing**
(SPRT, ~2000 games at 10+0.1) rather than unit tests — the question is whether the change
gains or loses Elo, not whether it's mechanically correct.
**Recommendation:** No unit tests needed. Elo test is the only meaningful validation.

---

### S20: `passedPawns[]` fallback duplication → Resolved by S1

Automatically resolved when S1 merges `evaluate()`/`evaluateTrace()` into `evaluateCore()`.

**Test coverage:** Same as S1.

---

### S21: Beta-cut stats block duplication (search/qsearch)

**File:** `src/engine/Search.cpp`
**Category:** REDUNDANCY — **Severity:** MEDIUM — **Confidence:** CERTAIN

**Solution:** Extract `recordBetaCut(move, ttMove, movesSearched)` inline helper.

**Lines saved:** ~8 — **Risk:** None — pure statistics.

**Test coverage:** ✅ Pure stats tracking. Bench verifies search correctness. No additional tests needed.

---

### S22: ✅ `EVAL_PREFETCH` inconsistency

**Status:** ✅ COMPLETE
**File:** `src/engine/Search.cpp` (line 1940 vs line 2290)
**Category:** PERFORMANCE — **Severity:** HIGH — **Confidence:** HIGH

**Problem:** `EVAL_PREFETCH` commented out in `search()` but active in `qsearch()`.

**Solution:** Enabled `EVAL_PREFETCH` in `search()` to match `qsearch()`. The prefetch hides
pawn TT latency for the child node's `evaluate()` call at line 1490 (static eval used for
pruning decisions: NMP, futility, razoring). Despite some wasted prefetches (TT cuts, cached
eval, check positions), benchmarking showed a net positive effect.

**Benchmark result** (`--bench --threads 1`, 50 positions):
- Before: ~2,482 kNPS (15.43s)
- After:  ~2,528 kNPS (15.15s)
- **Delta: +1.8% NPS improvement**, bench signature unchanged.

**Risk:** None — prefetch is a CPU hint with no behavioral effect.

---

### S23: `pieceEval` switch unreachable default

**File:** `src/engine/Evaluator.cpp`
**Category:** DEAD_CODE — **Severity:** LOW — **Confidence:** CERTAIN

**Solution:** Replace `default: break;` with `__assume(false)` / `__builtin_unreachable()`.

**Lines saved:** 2 — **Risk:** None.

**Test coverage:** ✅ Eval tests exercise all 4 valid piece types. Bench is sufficient.
No additional tests needed.

---

### S24: `relRank` computation duplication

**File:** `src/engine/Evaluator.cpp` (4+ call sites)
**Category:** REDUNDANCY — **Severity:** LOW — **Confidence:** CERTAIN

**Solution:** Add `constexpr int relativeRank(Color, Square)` utility to `square.h`.

**Lines saved:** ~4 — **Risk:** None.

**Test coverage:** ✅ Eval tests cover all call sites. Consider adding 2-3 trivial assertions
for the new function in `SquareIteratorTest.cpp` or `TypesTest.cpp` (WHITE+e4=3, BLACK+e4=4).
**Recommendation:** Optional but easy — add if convenient.

---

### S25: `Evaluator::reset()` is empty

**File:** `src/engine/Evaluator.h`
**Category:** DEAD_CODE — **Severity:** LOW — **Confidence:** CERTAIN

**Solution:** Remove method and all call sites.

**Lines saved:** 5 — **Risk:** None.

**Test coverage:** ✅ Trivial removal. Compile + bench is sufficient. No additional tests needed.

---

## Implementation Order

Ordered by: performance impact first, then dependencies, then effort.
Quick wins (🟢 5–15 min) are grouped together for batch implementation.

### Phase 1 — Performance wins
1. ~~**S11** — Bulk pawn attacks~~ ✅ COMPLETE
2. ~~**S22** — Enable EVAL_PREFETCH in search~~ ✅ COMPLETE (+1.8% NPS)
3. ~~**S12** — King safety dedup + precompute enemyKingZone~~ ✅ COMPLETE

### Phase 2 — Quick wins (batch these)
4. ~~**S13** — Remove intermediate variable~~ ✅ COMPLETE
5. ~~**S14** — Move `do_null` before loop~~ ✅ COMPLETE
6. **S23** — Unreachable default branch
7. **S25** — Remove empty `reset()`
8. **S5** — SEE loop
9. **S10** — TT-move validation helper
10. **S3** — TT bound-type stats helper

### Phase 3 — Eval simplifications (dependency chain)
11. **S6** — `addSigned()` helper (foundation)
12. **S7** — Remove `if (mid || end)` guards (depends on S6)
13. **S8** — Safe-check lambda
14. ~~**S2** — pieceEval loop~~ ✅ COMPLETE (done with S1)
15. **S24** — `relativeRank()` utility

### Phase 4 — Large refactors
16. ~~**S1** — Evaluator dedup~~ ✅ COMPLETE (also resolved S20)
17. **S16** — `formatDetailedStats()` cleanup (supersedes S4)
18. **S21** — Beta-cut stats helper
19. **S15** — Draw-check pattern (only if S21 motivates)

### Phase 5 — Optional / risky
20. **S9** — SearchStats X-macro (⚠️ write operator+= test first; discuss before implementing)
21. **S17** — TT aggregate dedup
22. **S18** — TT batch aggregation
23. **S19** — History negative values (⚠️ requires Elo testing — do separately)

---

## Verification Checklist

For each item:
- [ ] Pre-implementation: check test coverage column — write tests first if ⚠️
- [ ] Bench signature unchanged (`--bench --threads 1`) — except S19
- [ ] NPS within reference range (see below) — run multiple times, noise is ~40–50 kNPS
- [ ] All unit tests pass (excluding speed/timing tests)
- [ ] No new compiler warnings
- [ ] Code review: readability improved, not degraded

**NPS Reference** (`--bench --threads 1`, 50 positions, Windows/MSVC Release):
- Expected range: **2,500–2,540 kNPS** (measured 2026-04-11 after S22)
- Noise: ±40–50 kNPS between runs — always measure several times in a row
- A sustained drop below 2,450 kNPS indicates a performance regression

---

## Items Considered and Rejected

| Item                                                                       | Reason                                                                                                                                                                                                                                                        |
|----------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Sub-struct grouping of SearchStats**                                     | One flat struct is simpler than 5 sub-structs. Adds indirection without real benefit.                                                                                                                                                                         |
| **~~Template-based Evaluator `evaluate<bool Trace>`~~**                    | ~~Initially rejected~~ → **Adopted for S1.** Template overhead is negligible (one extra instantiation), and `if constexpr` + `std::conditional_t` return type gives zero-cost trace recording with no pointers, no null, no `[[maybe_unused]]` proliferation. |
| **Template `search<NodeType>`**                                            | Eliminates runtime `nodeType` checks but makes code significantly harder to read/debug. Branches are well-predicted. Not justified at FrankyCPP's current strength tier.                                                                                      |
| **Replace `std::ostringstream` with `std::format` in formatDetailedStats** | `std::format` doesn't support all formatting features used (locale + setprecision). Would require multiple calls without real simplification.                                                                                                                 |
| **Merge `search()` and `qsearch()`**                                       | Structurally similar but semantically different enough that merging hurts readability. Move gen mode, standpat, and depth handling are fundamentally different.                                                                                               |
| **Eliminate `tmpScore` member in Evaluator**                               | Scratch variable in `pawnEval`. Making it local is cleaner but Score is 8 bytes — zero cost either way. Not worth the churn.                                                                                                                                  |

---

*Last updated: 2026-04-12*
