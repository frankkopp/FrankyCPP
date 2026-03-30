# FrankyCPP Mate Score Stability & Aspiration Window Fixes

**Document Version:** 2.0  
**Created:** 2026-03-13  
**Last Updated:** 2026-03-13  
**Status:** 🟡 IN PROGRESS  
**Target:** FrankyCPP v1.6+  
**Priority:** High (Search correctness — mate scores lost between iterations)

---

## Executive Summary

FrankyCPP exhibits several interrelated bugs causing erratic behavior in endgame positions (KQ vs K, KQB vs K). The symptoms include:

1. **Bogus near-mate scores** (+98.71 / -98.52 pawns) appearing in the output
2. **Phantom mates** (e.g., M44) appearing at one depth and disappearing at the next
3. **Confirmed mates getting "lost"** — M7 found at depth 4, dropped to +18.40 at depth 5
4. **Severe score oscillation** between iterations (mate → near-mate → regular → mate → …)
5. **Stale aspiration UCI output** showing previous iteration's value during re-searches

Stockfish finds M7 at depth 1 in these positions and holds it stably across all deeper iterations. FrankyCPP takes 20+ depths to stabilize, wasting enormous amounts of search time.

### Root Causes (in dependency order)

| # | Root Cause                                                                                                                                 | Effect                                                        |
|---|--------------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------|
| 1 | NMP clamps mate values to `VALUE_CHECKMATE_THRESHOLD` (9871), which is NOT recognized as a checkmate → stored in TT without ply adjustment | TT contamination with artificial near-mate scores             |
| 2 | Aspiration loop has off-by-one: only 2 re-searches instead of 3 — the full-window search never executes                                    | Fail-low values from narrow windows returned as final results |
| 3 | Aspiration uses narrow windows around mate scores from previous iteration                                                                  | Cascading fail-lows that "lose" confirmed mates               |
| 4 | `sendAspirationResearchInfo()` reports previous iteration's value, not current search value                                                | Misleading UCI output during aspiration re-searches           |

### Test Positions (from the bug report)

| FEN                                    | Description                       | Expected       | FrankyCPP Actual                                                           |
|----------------------------------------|-----------------------------------|----------------|----------------------------------------------------------------------------|
| `4q3/k7/8/8/3K4/8/8/8 w - - 33 227`    | KQ vs K, White to move (losing)   | -M7 quickly    | Finds -M7 at depth 28 after wild oscillation                               |
| `4q3/k7/8/3K4/8/8/8/8 b - - 34 227`    | KQ vs K, Black to move (winning)  | +M7 quickly    | Finds +M7 at depth 21 after oscillation through +98.71, +M44, +18.40       |
| `8/8/3k1b2/8/5K2/8/8/4q3 b - - 55 193` | KQB vs K, Black to move (winning) | +M5-M7 quickly | **Finds +M7 at depth 4, DROPS to +18.40 at depth 5**, recovers at depth 10 |

---

## Step 1 — Fix NMP: Fail-Hard Clamping to `beta`

**Priority:** Critical  
**Dependency:** None (root cause of TT contamination)  
**Status:** ✅ DONE (commit 4559e2e, refined in aspiration rewrite)  
**Risk:** Low — `beta` is always below the checkmate threshold when NMP fires (guarded by `nearMateWindow`)

### Problem

NMP's fail-soft null-move search can return wildly inflated values that contaminate the TT:

1. **Original bug (commit 4559e2e):** Values > `VALUE_CHECKMATE_THRESHOLD` (9871) were clamped to 9871 itself, which is NOT recognized as checkmate → stored raw in TT → appeared as "+98.71 pawns" in UCI output.

2. **Refinement:** Values between `beta` and `VALUE_CHECKMATE_THRESHOLD` (e.g., 8998 cp = +89.98 pawns) passed through unclamped. In endgame positions (KQB vs KR), fail-soft NMP regularly returned values like 8998 from deep in the search tree, causing persistent +89.98 artifacts in aspiration re-searches.

### Fix

Fail-hard NMP: clamp ALL NMP values to `beta`. NMP only proves `value >= beta`, not the exact value. Both the `storeTt()` calls and `return` statements use `beta` instead of `nValue`:

```cpp
// Fail-hard NMP: clamp to beta
if (nValue > beta) {
    nValue = beta;
}

if (nValue >= beta) {
    // ... verification search ...
    storeTt(p, depth, ply, MOVE_NONE, beta, BETA, staticEval);
    return beta;
}
```

This is standard practice in classical-eval engines. NNUE engines (Stockfish) can afford fail-soft NMP because their evaluation naturally stays bounded.

### Files

- `src/engine/Search.cpp` — NMP section (~line 1370-1420)

---

## Step 2 — Aspiration Window Rewrite (Stockfish-Style)

> **Replaces previous Steps 2, 3, and 4** — off-by-one fix, mate score skip, and UCI display fix are all addressed by the rewrite.

**Priority:** Critical  
**Dependency:** None (independent of Step 1, but both are needed)  
**Status:** 🔴 NOT STARTED  
**Risk:** Low for normal play (first window usually succeeds); high-impact for mate stability

### Problems Solved

| # | Problem                                                                                                           | How the Rewrite Fixes It                                                                     |
|---|-------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------|
| 1 | **Off-by-one**: Fixed-step loop `for(i=1; i<3)` only runs 2 searches; full-window search is dead code             | `while(true)` loop + exponential growth guarantees full-window search eventually executes    |
| 2 | **Mate score narrow windows**: ±50 around a mate value (9987cp) → cascading fail-lows that "lose" confirmed mates | Mate bypass: if previous iteration found mate, skip aspiration entirely (full-window search) |
| 3 | **Stale centering**: Window widens relative to `bestValue` (previous iteration), not the current search result    | Value-centered re-search: on fail, widen relative to `value` (actual current result)         |
| 4 | **Stale UCI display**: `sendAspirationResearchInfo()` shows previous iteration's value during re-searches         | Update `currentBestRootMoveValue` before sending UCI info                                    |

### Current Code (broken)

```cpp
Value Search::aspirationSearch(Position& p, const Depth depth, const Value bestValue) {
  constexpr std::array aspirationSteps = {Value{50}, Value{200}, VALUE_MAX};
  constexpr auto steps = aspirationSteps.size();  // = 3
  Value value = VALUE_NONE;

  Value alpha = std::max(bestValue - aspirationSteps[0], VALUE_MIN);  // ±50
  Value beta  = std::min(bestValue + aspirationSteps[0], VALUE_MAX);

  for (auto i = 1; i < steps; ++i) {  // i = 1, 2 — only TWO searches
    value = rootSearch(p, depth, alpha, beta);
    // ... stop/time checks ...
    if (value <= alpha) {
      // ... UCI output ...
      alpha = std::max(bestValue - aspirationSteps[i], VALUE_MIN);  // bestValue-centered
    }
    else if (value >= beta) {
      // ... UCI output ...
      beta = std::min(bestValue + aspirationSteps[i], VALUE_MAX);   // bestValue-centered
    }
    else { break; }
  }
  return value;
}
```

**Failure trace** (two consecutive fail-lows):
1. `i=1`: Search ±50. Fail-low → widen alpha to `bestValue - 200`.
2. `i=2`: Search ±200. Fail-low → widen alpha to `bestValue - VALUE_MAX`. **Loop exits — no search with full window!**

### New Implementation

#### Algorithm (pseudocode)

```
aspirationSearch(p, depth, bestValue):
  // Mate bypass — skip aspiration entirely for mate scores
  if bestValue.isCheckMate():
    return rootSearch(p, depth, VALUE_MIN, VALUE_MAX)

  delta = ASP_INITIAL_DELTA                           // configurable, default 12
  alpha = max(bestValue - delta, VALUE_MIN)
  beta  = min(bestValue + delta, VALUE_MAX)

  while (true):
    value = rootSearch(p, depth, alpha, beta)

    if stopConditions() && (value <= alpha || value >= beta):
      return VALUE_NONE
    if isTimeAlmostUp():
      return value

    if value <= alpha:                                 // FAIL LOW
      update currentBestRootMoveValue to value         // fix stale UCI display
      sendAspirationResearchInfo("upperbound")
      addExtraTime(1.3)
      if isTimeAlmostUp(): return value
      alpha = max(value - delta, VALUE_MIN)            // value-centered, not bestValue
      // don't touch beta
      delta += delta / ASP_DELTA_GROWTH_DIVISOR        // configurable, default 3 (≈×1.33)
      aspirationResearches++

    else if value >= beta:                             // FAIL HIGH
      update currentBestRootMoveValue to value
      sendAspirationResearchInfo("lowerbound")
      if isTimeAlmostUp(): return value
      beta = min(value + delta, VALUE_MAX)             // value-centered
      // don't touch alpha
      delta += delta / ASP_DELTA_GROWTH_DIVISOR
      aspirationResearches++

    else:                                              // EXACT — within window
      break

  return value
```

#### Key Properties

- **No off-by-one**: `while(true)` with exponential delta growth guarantees `alpha → VALUE_MIN` and `beta → VALUE_MAX` after ~8–10 fails. The full-window search always executes if needed.
- **Value-centered**: On fail-low, alpha widens relative to `value` (the actual search result), not `bestValue` (stale from previous iteration). This adapts to where the true score actually is.
- **Mate bypass**: When the previous iteration found a mate, skip aspiration entirely. A mate score is either confirmed or improved by full-window search — no risk of losing it to a narrow window fail-low.
- **Fixed UCI display**: `currentBestRootMoveValue` is updated to the current `value` before `sendAspirationResearchInfo()`, so the GUI shows the actual search result during re-searches.

#### Delta Growth (from initial value 12, divisor 3 → ≈×1.33)

```
12 → 16 → 21 → 28 → 37 → 50 → 66 → 88 → 117 → 156 → 208 → ...
```

After ~8–9 fails, delta exceeds the entire score range (VALUE_MAX = 10000) and both bounds saturate at VALUE_MIN/VALUE_MAX.

#### Growth Divisor Comparison

| Divisor | Effective multiplier | Fails to reach full window | Character                         |
|---------|----------------------|----------------------------|-----------------------------------|
| 3       | ×1.33                | ~10                        | Conservative (Stockfish's choice) |
| 2       | ×1.50                | ~8                         | Moderate                          |
| 1       | ×2.00                | ~6                         | Aggressive                        |

All use integer arithmetic: `delta = Value{static_cast<int>(delta) + static_cast<int>(delta) / divisor}`.

### New Config Parameters

Two new parameters in `SearchConfigData.h`:

```cpp
CONFIG_CONST int ASP_INITIAL_DELTA       = 12;  // Initial aspiration half-window (cp)
CONFIG_CONST int ASP_DELTA_GROWTH_DIVISOR = 3;  // Delta growth: delta += delta / divisor (≈×1.33)
```

Both exposed via UCI and YAML for SPRT tuning. Registry entries follow the standard pattern in `ConfigRegistry.cpp`.

### Files to Change

| File                            | Change                                                                                     |
|---------------------------------|--------------------------------------------------------------------------------------------|
| `src/config/SearchConfigData.h` | Add `ASP_INITIAL_DELTA` and `ASP_DELTA_GROWTH_DIVISOR` members (~line 64, after `USE_ASP`) |
| `src/config/ConfigRegistry.cpp` | Add registry entries for both new params (after `USE_ASP` block)                           |
| `config/search.yaml`            | Add commented-out entries for discoverability                                              |
| `src/engine/Search.cpp`         | Rewrite `aspirationSearch()` (lines 882–939)                                               |
| `src/engine/Search.h`           | Update `aspirationSearch()` doc comment (~line 451–456)                                    |

### What Does NOT Change

- **`iterativeDeepening()` call site** (line 717): stays as `if (USE_ASP && iterationDepth > 3)` — mate handling is internal to `aspirationSearch()`
- **`rootSearch()`**: untouched
- **Normal positions**: no behavioral change (first ±12 window almost always succeeds)

### Validation

- **Test position 1** (`8/8/3k1b2/8/5K2/8/8/4q3 b - - 55 193`): M7 found at depth 4 must be preserved (or improved) at depths 5+. Previously dropped to +18.40.
- **Test position 2** (`4q3/k7/8/3K4/8/8/8/8 b - - 34 227`): No oscillation between mate and non-mate scores.
- **Test position 3** (`8/8/3k1b2/4q3/8/3K4/8/8 b - - 69 200`): No +89.98 / +89.99 artifacts.
- **Aspiration UCI output**: Fail-low/fail-high lines show actual current search value, not previous iteration's stale value.
- **Normal games**: Run existing `SearchTest::mate*` tests — must all pass. ELO regression test recommended (expected neutral or slight improvement).

### Future Refinement (not in this change)

- **Fail-low beta narrowing**: Stockfish adjusts `beta = (alpha + beta) / 2` on fail-low to narrow from both sides for faster resolution. Worth considering as a follow-up.
- **Aspiration depth threshold**: Currently `iterationDepth > 3`. Could make configurable as `ASP_MIN_DEPTH` if needed for tuning.

---

## Step 3 — Add Regression Tests for Mate Score Stability

**Priority:** Medium  
**Dependency:** Steps 1-2 should be done first  
**Status:** 🔴 NOT STARTED  
**Risk:** None — test-only change

### Tests to Add

Add test cases in `test/engine/SearchTest.cpp` that verify mate stability across iterations:

#### Test 1: KQB vs K — Mate Must Not Drop

```cpp
// FEN: 8/8/3k1b2/8/5K2/8/8/4q3 b - - 55 193
// Black has KQB vs K — must find mate ≤ M7
// Previously: Found M7 at depth 4, DROPPED to +18.40 at depth 5
TEST_F(SearchTest, mateStabilityKQBvsK) {
    CONFIG_OVERRIDE(s.USE_BOOK = false;);
    const Position p{"8/8/3k1b2/8/5K2/8/8/4q3 b - - 55 193"};
    SearchLimits sl{};
    Search s{};
    sl.depth = 15;
    s.isReady();
    s.startSearch(p, sl);
    s.waitWhileSearching();
    const auto result = s.getLastSearchResult();
    EXPECT_TRUE(result.bestMoveValue.isCheckMate())
        << "Expected checkmate score, got: " << result.bestMoveValue.str();
    // Mate in 7 or shorter
    const int mateDistance = (VALUE_CHECKMATE - std::abs(static_cast<int>(result.bestMoveValue)) + 1) / 2;
    EXPECT_LE(mateDistance, 7) << "Expected mate in ≤7, got mate in " << mateDistance;
}
```

#### Test 2: KQ vs K — No VALUE_CHECKMATE_THRESHOLD in Final Score

```cpp
// FEN: 4q3/k7/8/3K4/8/8/8/8 b - - 34 227
// Black has KQ vs K — must find mate without near-mate artifacts
TEST_F(SearchTest, mateStabilityKQvsK) {
    CONFIG_OVERRIDE(s.USE_BOOK = false;);
    const Position p{"4q3/k7/8/3K4/8/8/8/8 b - - 34 227"};
    SearchLimits sl{};
    Search s{};
    sl.depth = 25;
    s.isReady();
    s.startSearch(p, sl);
    s.waitWhileSearching();
    const auto result = s.getLastSearchResult();
    EXPECT_TRUE(result.bestMoveValue.isCheckMate())
        << "Expected checkmate score, got: " << result.bestMoveValue.str();
    // Value must NOT be VALUE_CHECKMATE_THRESHOLD (the NMP artifact)
    EXPECT_NE(std::abs(static_cast<int>(result.bestMoveValue)),
              static_cast<int>(VALUE_CHECKMATE_THRESHOLD))
        << "Score should not be VALUE_CHECKMATE_THRESHOLD (NMP clamping artifact)";
}
```

#### Test 3: Simple Mate — Existing Tests Still Pass

Verify existing `mate1Search` through `mate5Search` tests still pass (no regressions from Steps 1-2).

### Files

- `test/engine/SearchTest.cpp`

---

## Further Considerations (Not Planned Yet)

These items were identified during analysis but do not require immediate action. Re-evaluate after Steps 1-3.

### `isCheckMate()` Boundary Condition

Currently: `absVal > VALUE_CHECKMATE_THRESHOLD` (strict `>`).  
This means exactly `VALUE_CHECKMATE_THRESHOLD` (9871) is NOT checkmate. After Step 1 fixes NMP clamping, this boundary is less critical. Changing to `>=` would affect `valueToTt`/`valueFromTt` and needs careful analysis. **Leave as-is for now.**

### `valueToTt` / `valueFromTt` Overflow Guard

If a mate value at ply P is stored as `VALUE_CHECKMATE + P`, it exceeds `VALUE_CHECKMATE` and `isCheckMate()` returns false on retrieval. Currently this doesn't happen because `valueToTt` always normalizes to `VALUE_CHECKMATE` for positive mates. But consider adding a defensive clamp:

```cpp
Value Search::valueToTt(const Value value, const Depth ply) {
    if (value.isCheckMate()) {
        if (value > 0) { return std::min(value + static_cast<Value>(ply), VALUE_CHECKMATE); }
        return std::max(value - static_cast<Value>(ply), -VALUE_CHECKMATE);
    }
    return value;
}
```

**Monitor but no change needed now.**

### NMP `nearMateWindow` Margin

`NMP_NEAR_MATE_MARGIN = 64` means NMP is disabled when beta > 9807. After Step 1, the exact margin matters less since the clamped value will be `beta` (always ≤ 9807). **No change needed.**

### Endgame Tablebases / Evaluation

The fundamental gap vs Stockfish in these positions is evaluation quality. Stockfish's NNUE immediately scores KQ vs K as +200.00 (capped win). FrankyCPP's classical eval gives ~9 pawns. Long-term, Syzygy tablebase integration (already partially implemented) or NNUE would eliminate these problems entirely for simple endgames. **Separate initiative, out of scope for this plan.**

---

## Implementation Order & Dependencies

```
Step 1: NMP Clamping Fix ────────────── ✅ DONE (commit 4559e2e)
                                         │
Step 2: Aspiration Rewrite ─────────────┼──→ Step 3: Regression Tests
   (merges old Steps 2+3+4)             │
                                         │
```

**Recommended implementation order:** 1 → 2 → 3

Step 1 is done. Step 2 is a full rewrite of `aspirationSearch()` that fixes the off-by-one, adds mate bypass, value-centered widening, and correct UCI display in a single change. Step 3 adds regression tests to validate everything.

---

*Last updated: 2026-03-13*
