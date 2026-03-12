# FrankyCPP Mate Score Stability & Aspiration Window Fixes

**Document Version:** 1.0  
**Created:** 2026-03-13  
**Last Updated:** 2026-03-13  
**Status:** 🔴 NOT STARTED  
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

| # | Root Cause | Effect |
|---|-----------|--------|
| 1 | NMP clamps mate values to `VALUE_CHECKMATE_THRESHOLD` (9871), which is NOT recognized as a checkmate → stored in TT without ply adjustment | TT contamination with artificial near-mate scores |
| 2 | Aspiration loop has off-by-one: only 2 re-searches instead of 3 — the full-window search never executes | Fail-low values from narrow windows returned as final results |
| 3 | Aspiration uses narrow windows around mate scores from previous iteration | Cascading fail-lows that "lose" confirmed mates |
| 4 | `sendAspirationResearchInfo()` reports previous iteration's value, not current search value | Misleading UCI output during aspiration re-searches |

### Test Positions (from the bug report)

| FEN | Description | Expected | FrankyCPP Actual |
|-----|-------------|----------|-----------------|
| `4q3/k7/8/8/3K4/8/8/8 w - - 33 227` | KQ vs K, White to move (losing) | -M7 quickly | Finds -M7 at depth 28 after wild oscillation |
| `4q3/k7/8/3K4/8/8/8/8 b - - 34 227` | KQ vs K, Black to move (winning) | +M7 quickly | Finds +M7 at depth 21 after oscillation through +98.71, +M44, +18.40 |
| `8/8/3k1b2/8/5K2/8/8/4q3 b - - 55 193` | KQB vs K, Black to move (winning) | +M5-M7 quickly | **Finds +M7 at depth 4, DROPS to +18.40 at depth 5**, recovers at depth 10 |

---

## Step 1 — Fix NMP Mate Clamping: Clamp to `beta` Instead of `VALUE_CHECKMATE_THRESHOLD`

**Priority:** Critical  
**Dependency:** None (root cause of TT contamination)  
**Status:** 🔴 NOT STARTED  
**Risk:** Low — `beta` is always below the checkmate threshold when NMP fires (guarded by `nearMateWindow`)

### Problem

In `Search.cpp:1366-1370`, when Null Move Pruning discovers a mate score (the position is so good that even without making a move, the opponent gets mated), the value is clamped:

```cpp
if (nValue > VALUE_CHECKMATE_THRESHOLD) {
    nValue = VALUE_CHECKMATE_THRESHOLD;  // = 9871
}
```

`VALUE_CHECKMATE_THRESHOLD = VALUE_CHECKMATE - MAX_DEPTH - 1 = 10000 - 128 - 1 = 9871`

The value 9871 sits right on the boundary:
- `isCheckMate()` requires `absVal > 9871` — so 9871 is **NOT** a checkmate
- `valueToTt()` only applies ply adjustment for checkmate values — so 9871 is stored **raw** in TT
- `valueFromTt()` returns it **unchanged** regardless of probe ply

This clamped value contaminates the TT and propagates through the search tree, appearing as the confusing "+98.71 pawns" score in UCI output (9871 cp ÷ 100 = 98.71 pawns).

The value -98.52 (= -9852 cp) likely arises from the clamped 9871 interacting with other heuristics during multi-ply propagation (e.g., mate distance pruning adjustments at different plies).

### Fix

Replace the clamping target with `beta`:

```cpp
if (nValue > VALUE_CHECKMATE_THRESHOLD) {
    // Clamp to beta rather than VALUE_CHECKMATE_THRESHOLD to avoid storing
    // an artificial near-mate non-mate value in TT. NMP only needs to prove
    // fail-high (value >= beta), so beta is sufficient. The nearMateWindow
    // guard already disables NMP when beta is near checkmate range, so beta
    // is always a "normal" score here.
    nValue = beta;
}
```

**Why `beta` is safe:** The `nearMateWindow` check (line 1329-1331) disables NMP when `beta > VALUE_CHECKMATE_THRESHOLD - NMP_NEAR_MATE_MARGIN` (i.e., `beta > 9807`). So when NMP fires, `beta ≤ 9807`, which is well below the checkmate range. Clamping to `beta` stores a value that:
- Is meaningful (represents the fail-high threshold)
- Is well below `VALUE_CHECKMATE_THRESHOLD` (no TT contamination)
- Preserves the NMP cutoff semantics (`nValue >= beta` is trivially true)

### Files

- `src/engine/Search.cpp` — ~line 1366-1370

### Validation

- Run the three test positions; verify no +98.71 / -98.52 scores appear in output
- Run existing `SearchTest::mate*` tests — must all pass
- Run ELO regression test (optional but recommended)

---

## Step 2 — Fix Aspiration Window Loop Off-by-One: Full-Window Search Must Execute

**Priority:** Critical  
**Dependency:** None (independent of Step 1, but both are needed)  
**Status:** 🔴 NOT STARTED  
**Risk:** Low — strictly more search, never less

### Problem

The aspiration loop in `Search.cpp:882-939` uses three steps `{50, 200, VALUE_MAX}` but only executes **two** search iterations:

```cpp
constexpr std::array aspirationSteps = {Value{50}, Value{200}, VALUE_MAX};
constexpr auto steps = aspirationSteps.size();  // = 3

Value alpha = std::max(bestValue - aspirationSteps[0], VALUE_MIN);  // initial window
Value beta  = std::min(bestValue + aspirationSteps[0], VALUE_MAX);

for (auto i = 1; i < steps; ++i) {      // i = 1, 2 — only TWO iterations
    value = rootSearch(p, depth, alpha, beta);
    // ... check fail-low/fail-high ...
    if (value <= alpha) {
        alpha = std::max(bestValue - aspirationSteps[i], VALUE_MIN);  // widen for NEXT iter
    }
    // ...
}
```

The flow:
1. **i=1**: Search with ±50 window. If fail → widen to ±200.
2. **i=2**: Search with ±200 window. If fail → widen to ±VALUE_MAX. **But loop exits!**

The widening at `i=2` to `VALUE_MAX` is **dead code** — no search ever runs with the fully-opened window. The comment at line 935 ("fully open search window of the last step") is inaccurate.

After two fail-lows, `aspirationSearch()` returns the fail-low value from the ±200 window, which can be a completely wrong score (e.g., +1840 when the true value is M7).

### Fix

Add an additional entry to the steps array so the full-window search actually executes:

```cpp
constexpr std::array aspirationSteps = {Value{50}, Value{200}, VALUE_MAX, VALUE_MAX};
```

The redundant `VALUE_MAX` at the end ensures the loop runs one more iteration (`i=3`) with the fully-widened window from step 2. After this final search, the value is guaranteed to be within `[VALUE_MIN, VALUE_MAX]`.

Update the comment at line 935 to accurately describe the behavior.

### Alternative Approach

Instead of adding a dummy step, restructure the loop to use a `do-while` or add an explicit post-loop search:

```cpp
// After the loop, if value is still outside the window, do one final full-window search
if (value <= alpha || value >= beta) {
    value = rootSearch(p, depth, VALUE_MIN, VALUE_MAX);
}
```

The extra-step approach is preferred for simplicity and minimal code change.

### Files

- `src/engine/Search.cpp` — ~line 884 (aspirationSteps array), ~line 935 (comment)

### Validation

- Run test positions — depth 5 should no longer return +18.40 when M7 is forced
- Verify that in normal positions, the extra step rarely fires (most searches resolve within ±200)
- Run ELO regression test — expected neutral or slight improvement (the fix only affects pathological cases)

---

## Step 3 — Skip Aspiration for Mate Scores: Use Full Window When Previous Iteration Found Mate

**Priority:** High  
**Dependency:** Step 2 should be done first (no point skipping if the loop itself is broken)  
**Status:** 🔴 NOT STARTED  
**Risk:** Very low — mate scores are rare; full-window search is always correct

### Problem

When the previous iteration returns a checkmate value (e.g., M7 = 9987 cp), the next iteration centers aspiration around it:
- `alpha = 9987 - 50 = 9937`
- `beta = min(9987 + 50, 10000) = 10000`

Both bounds are in/near the checkmate range. Even small search variations at the next depth (different pruning decisions, NMP, etc.) can fail to confirm the mate, causing cascading fail-lows. The mate is "lost" and the engine reports a much lower score.

This violates the principle that a forced mate should never disappear at deeper search.

### Fix

In the iterative deepening loop (`Search.cpp:717-718`), skip aspiration when the previous iteration found a mate:

```cpp
if (SearchConfig.USE_ASP && iterationDepth > 3 && !bestValue.isCheckMate()) {
    bestValue = aspirationSearch(p, iterationDepth, bestValue);
}
else {
    bestValue = rootSearch(p, iterationDepth, alpha, beta);
}
```

When `bestValue.isCheckMate()` is true, the full-window `rootSearch()` is used. This guarantees:
- The mate is either confirmed or a shorter mate is found
- If the mate was bogus (from a shallow depth), the full-window search finds the correct value
- No aspiration overhead for positions where the result is already a mate

### Alternative: Widen Inside aspirationSearch()

Instead of bypassing `aspirationSearch()`, widen the initial window inside it:

```cpp
Value Search::aspirationSearch(Position& p, const Depth depth, const Value bestValue) {
    // Skip aspiration for mate scores — use full window to avoid losing confirmed mates
    if (bestValue.isCheckMate()) {
        return rootSearch(p, depth, VALUE_MIN, VALUE_MAX);
    }
    // ... existing aspiration logic ...
}
```

This is cleaner (single call site) but slightly less explicit. Both approaches are equivalent.

### Files

- `src/engine/Search.cpp` — ~line 717-718 (iterative deepening) or ~line 882-889 (aspirationSearch entry)

### Validation

- Test position `8/8/3k1b2/8/5K2/8/8/4q3 b - - 55 193`: M7 found at depth 4 must be preserved (or improved) at depths 5+
- Test position `4q3/k7/8/3K4/8/8/8/8 b - - 34 227`: No oscillation between mate and non-mate scores
- Run existing `SearchTest::mate*` tests — must all pass
- ELO regression: expected neutral (mate positions are rare in games)

---

## Step 4 — Fix Aspiration UCI Display: Report Current Search Value During Re-searches

**Priority:** Medium  
**Dependency:** None (cosmetic fix, independent of Steps 1-3)  
**Status:** 🔴 NOT STARTED  
**Risk:** Very low — display-only change, no search behavior impact

### Problem

During aspiration re-searches, `sendAspirationResearchInfo()` (`Search.cpp:3097`) reports `thread().statistics.currentBestRootMoveValue`, which is set only at the **end of the previous iteration** (line 852). This means all fail-low/fail-high lines in the UCI output show the **stale** value from the previous depth:

```
 6/8-  -9,32   ← Shows previous iteration's value (-932 cp), NOT the current fail-low value
 6/8-  -9,32   ← Same stale value
 6/8   -98,52  ← Final result — only now is the actual value shown
```

The UCI protocol expects `score` during `upperbound`/`lowerbound` to reflect the current search result (even though it's a bound). Stockfish reports the actual current value.

### Fix

Update `currentBestRootMoveValue` inside `aspirationSearch()` before calling `sendAspirationResearchInfo()`, so the UCI output reflects the current search state:

```cpp
if (value <= alpha) {
    // FAIL LOW
    if (isMainThread()) {
        // Update displayed value to reflect current search result
        ESSENTIAL_STAT_SET(thread().statistics.currentBestRootMoveValue, thread().pv.first().value());
        sendAspirationResearchInfo("upperbound");
        addExtraTime(1.3);
    }
    // ...
}
else if (value >= beta) {
    // FAIL HIGH
    if (isMainThread()) {
        ESSENTIAL_STAT_SET(thread().statistics.currentBestRootMoveValue, thread().pv.first().value());
        sendAspirationResearchInfo("lowerbound");
    }
    // ...
}
```

### Files

- `src/engine/Search.cpp` — ~line 904-930 (aspirationSearch fail-low/fail-high blocks)

### Validation

- Run any analysis position; verify that fail-low/fail-high lines show the actual current search value, not the previous iteration's value
- No search behavior changes — purely cosmetic

---

## Step 5 — Add Regression Tests for Mate Score Stability

**Priority:** Medium  
**Dependency:** Steps 1-3 should be done first  
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

Verify existing `mate1Search` through `mate5Search` tests still pass (no regressions from Steps 1-3).

### Files

- `test/engine/SearchTest.cpp`

---

## Further Considerations (Not Planned Yet)

These items were identified during analysis but do not require immediate action. Re-evaluate after Steps 1-5.

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
Step 1: NMP Clamping Fix ──────────────┐
                                        ├──→ Step 5: Regression Tests
Step 2: Aspiration Loop Fix ───────────┤
                                        │
Step 3: Skip Aspiration for Mates ─────┘
        (depends on Step 2)

Step 4: Aspiration UCI Display ────────── (independent, can be done anytime)
```

**Recommended implementation order:** 1 → 2 → 3 → 4 → 5

Steps 1 and 2 are independent and can be done in parallel. Step 3 depends on Step 2 (no point skipping aspiration if the loop itself is broken). Step 4 is cosmetic and independent. Step 5 validates everything.

---

*Last updated: 2026-03-13*
