# FrankyCPP Move Ordering & Pruning Improvements Plan

**Document Version:** 1.2  
**Created:** 2026-03-07  
**Last Updated:** 2026-03-08  
**Status:** 🟡 IN PROGRESS (Features 1 & 4 Tested)  
**Target:** FrankyCPP v1.5+  
**Priority:** Medium (Incremental strength improvements)  
**Predecessor:** `archive/PLAN_Search_Tree_Reduction_Review.md` (Phase 1 LMR complete)

---

## Executive Summary

This plan documents potential move ordering and pruning improvements for FrankyCPP, extracted from the completed Search Tree Reduction Review. Phase 1 (LMR improvements) achieved **+177 ELO** and is complete. The remaining features target move ordering quality (Phase 2) and additional pruning techniques (Phase 3).

**Current State:** FrankyCPP v1.5 has solid LMR, improving flag integration, and basic history heuristics. The next level of strength gains requires more sophisticated move ordering to increase cutoff rates.

**⚠️ Feature 1 (Capture History) was implemented and tested but FAILED validation — see details below.**

**⚠️ Feature 4 (SEE Quiet Pruning) was implemented and tested — ELO-NEUTRAL, not merged. Code available in branch.**

---

## Hypotheses: Expected Improvements

Each proposed feature is based on Stockfish's implementation and chess programming theory. Here's why each is expected to improve strength:

### 1. Capture History Table — ❌ FAILED (Shelved)

| Aspect                  | Description                                                                                           |
|-------------------------|-------------------------------------------------------------------------------------------------------|
| **Hypothesis**          | Ordering captures by historical success rate (not just MVV-LVA) will produce more cutoffs             |
| **Mechanism**           | Track which piece×square×victim combinations caused beta cutoffs; boost those captures in ordering    |
| **Expected Impact**     | ~~Medium-High~~ **NEGATIVE** — Disrupts MVV-LVA ordering, causes tactical blindness                   |
| **Evidence**            | Stockfish uses capture history; exchanges that "work" in one position often work in similar positions |
| **Validation Metric**   | Fewer nodes (better cutoff rate); measure first-move cutoff % increase; ELO gain in matches           |
| **ACTUAL RESULT**       | **-19% nodes BUT -5.6% test suite accuracy, -4.1% vs v1.3** — Speed gain, strength loss               |

**Why it FAILED:** MVV-LVA ordering is already highly effective for captures. Adding capture history bonus disrupted good tactical ordering, causing the engine to miss critical captures in tactical positions. The 19% node reduction was illusory — the engine searched faster but found worse moves.

**Implementation Status:** Code was implemented and tested but REVERTED. This document retained for future reference.

### 2. Counter-Move History (Scored)

| Aspect                  | Description                                                                                         |
|-------------------------|-----------------------------------------------------------------------------------------------------|
| **Hypothesis**          | Scoring counter-moves (not just storing them) provides better quiet move ordering                   |
| **Mechanism**           | Track how often each counter-move caused cutoffs; use score in move ordering                        |
| **Expected Impact**     | Medium — Complements existing counter-move storage                                                  |
| **Evidence**            | Current implementation stores one counter-move per opponent move; scoring adds confidence weighting |
| **Validation Metric**   | Fewer nodes (better ordering); measure quiet move cutoff rate; ELO gain in matches                  |

**Why it should work:** If counter-move A has caused 50 cutoffs and counter-move B only 2, we should try A first even if B was stored more recently.

### 3. Continuation History

| Aspect                  | Description                                                                                    |
|-------------------------|------------------------------------------------------------------------------------------------|
| **Hypothesis**          | 2-move sequences that work well in one position often work in similar positions                |
| **Mechanism**           | Track [prev_piece][prev_to][curr_piece][curr_to] — "after opponent plays X, my move Y is good" |
| **Expected Impact**     | High — Stockfish considers this one of the most important history tables                       |
| **Evidence**            | Many chess patterns are sequential: after opponent attacks, certain defenses are thematic      |
| **Validation Metric**   | Fewer nodes (significantly better move ordering); higher depth at fixed time; ELO gain         |

**Why it should work:** Chess has recurring tactical and strategic patterns. If Nf3-g5 is often followed by Qd1-h5 (threatening mate), continuation history learns this. More sophisticated than from-to history alone.

### 4. SEE-Based Quiet Move Pruning — 🟡 ELO-NEUTRAL (Shelved)

| Aspect                  | Description                                                                         |
|-------------------------|-------------------------------------------------------------------------------------|
| **Hypothesis**          | Quiet moves that lose material (negative SEE) at low depths are unlikely to be best |
| **Mechanism**           | At low depths, skip quiet moves where the moving piece can be captured for a loss   |
| **Expected Impact**     | ~~Medium~~ **NEUTRAL** — Node reduction doesn't translate to strength gain          |
| **Evidence**            | Stockfish prunes quiet moves with bad SEE scaled by depth                           |
| **Validation Metric**   | Fewer nodes (direct pruning); NPS may drop slightly (SEE cost); net ELO gain        |
| **ACTUAL RESULT**       | **-16% nodes (1T), +8% first-move cutoffs (4T), BUT -0.2% test suite, ELO neutral** |

**Why it's NEUTRAL:** The feature works as intended — it prunes quiet moves that land on attacked squares, achieving 16-18% node reduction in single-threaded tests. However, in SMP (4 threads), the node reduction disappears (+0.6% nodes) while first-move cutoff rate improves (+8.3%). The ELO vs v1.4 is identical (+74.6), suggesting the pruning is orthogonal to playing strength.

**Test Results:**
- vs v1.4: +74.6 ELO (same as TTBuckets baseline), -0.2% test suite (slight regression)
- vs v1.3: +169.2 ELO (vs +218.7 for TTBuckets), +1.3% test suite
- Best parameters: `SEE_QUIET_PRUNE_DEPTH=4`, `SEE_QUIET_PRUNE_MARGIN=-80`

**Implementation Status:** Code complete and tested, available in separate branch. NOT merged to dev-v1.5 since ELO gain is zero.

### 5. History-Based Pruning

| Aspect                  | Description                                                                               |
|-------------------------|-------------------------------------------------------------------------------------------|
| **Hypothesis**          | Quiet moves with very negative history scores are statistically unlikely to be good       |
| **Mechanism**           | At low depths, skip quiet moves with history below a threshold                            |
| **Expected Impact**     | Medium — Aggressive pruning of "known bad" moves                                          |
| **Evidence**            | Complements history-based LMR; if a move consistently fails, don't search it at low depth |
| **Validation Metric**   | Fewer nodes (direct pruning); minimal NPS impact; ELO gain in matches                     |

**Why it should work:** History heuristic already identifies bad moves (negative scores). Rather than just ordering them last, we can skip them entirely at shallow depths where they're unlikely to be best.

---

## Priority & Implementation Order

| Priority | Feature                       | Effort | Expected Impact | Dependencies    | Status         |
|----------|-------------------------------|--------|-----------------|-----------------|----------------|
| 1        | Capture History               | 2-3h   | ~~Medium-High~~ | None            | ❌ FAILED       |
| 2        | Counter-Move History (Scored) | 1-2h   | Medium          | None            | 🔴 Not Started |
| 3        | Continuation History          | 4-6h   | High            | PlyInfo changes | 🔴 Not Started |
| 4        | SEE-Based Quiet Pruning       | 2-3h   | ~~Medium~~      | None            | 🟡 NEUTRAL     |
| 5        | History-Based Pruning         | 1-2h   | Medium          | None            | 🔴 Not Started |

**Recommended approach:** Implement and test each feature individually. Combining untested features makes it impossible to attribute gains/losses.

**Lesson from Feature 1:** Node reduction does NOT equal strength gain. Always validate with test suites and matches, not just node counts.

---

## Feature 1: Capture History Table

### Current State

FrankyCPP orders captures using:
1. MVV-LVA (Most Valuable Victim - Least Valuable Attacker)
2. SEE (Static Exchange Evaluation) to separate good/bad captures
3. TT move bonus

**Gap:** No learning from which captures actually caused cutoffs.

### Proposed Implementation

#### Config Flags
```cpp
// SearchConfigData.h
CONFIG_CONST bool USE_CAPTURE_HISTORY = true;   // Enabled by default
CONFIG_CONST int CAPTURE_HISTORY_BONUS = 32;    // Bonus per cutoff (tunable)
CONFIG_CONST int CAPTURE_HISTORY_MAX = 8000;    // Clamp threshold
```

#### Data Structure
```cpp
// History.h - add to History struct
// [moving_piece_type][to_square][captured_piece_type]
// Memory: 6 * 64 * 6 * sizeof(int16_t) = 4.5 KB
int16_t captureHistory[6][64][6]{};

void updateCaptureHistory(PieceType piece, Square to, PieceType captured, int bonus);
[[nodiscard]] int getCaptureHistoryScore(PieceType piece, Square to, PieceType captured) const;
void ageCaptureHistory();  // Called with regular history aging
```

#### Integration Points

1. **MoveGenerator.cpp** — `updateSortValues()`:
   ```cpp
   // For captures, add capture history to sort value
   if (SearchConfig.USE_CAPTURE_HISTORY && move.isCapture()) {
     const int capHistScore = history.getCaptureHistoryScore(
       pieceType(move.piece()), move.to(), pieceType(capturedPiece));
     sortValue += capHistScore / 8;  // Scale down to not dominate MVV-LVA
   }
   ```

2. **Search.cpp** — on beta cutoff from capture:
   ```cpp
   if (SearchConfig.USE_CAPTURE_HISTORY && bestMove.isCapture()) {
     history.updateCaptureHistory(
       pieceType(position.pieceAt(bestMove.from())),
       bestMove.to(),
       pieceType(position.pieceAt(bestMove.to())),
       depth * depth);  // depth² bonus like main history
   }
   ```

#### Validation
- Add `USE_CAPTURE_HISTORY` to SearchTreeSizeTest initialization (disabled)
- Add test case after existing history tests
- Run 200+ game match vs baseline

---

## Feature 2: Counter-Move History (Scored)

### Current State

FrankyCPP stores counter-moves in `History::counterMoves[64][64]`:
- Indexed by [previous_from][previous_to]
- Stores a single Move (the refutation)
- Binary: either the counter-move or nothing

**Gap:** No score to indicate confidence/frequency.

### Proposed Implementation

#### Config Flags
```cpp
// SearchConfigData.h
CONFIG_CONST bool USE_COUNTERMOVE_HISTORY = true;  // Enabled by default
CONFIG_CONST int COUNTERMOVE_HISTORY_BONUS = 32;
```

#### Data Structure
```cpp
// History.h - add to History struct
// Score for counter-moves, indexed same as counterMoves
// Memory: 64 * 64 * sizeof(int16_t) = 8 KB
int16_t counterMoveScore[64][64]{};

void updateCounterMoveScore(Square prevFrom, Square prevTo, int bonus);
[[nodiscard]] int getCounterMoveScore(Square prevFrom, Square prevTo) const;
```

#### Integration Points

1. **MoveGenerator.cpp** — `updateSortValues()`:
   ```cpp
   // For quiet moves, add counter-move score if this is the counter-move
   if (SearchConfig.USE_COUNTERMOVE_HISTORY && !move.isCapture()) {
     if (move == history.counterMoves[prevFrom][prevTo]) {
       sortValue += history.getCounterMoveScore(prevFrom, prevTo) / 4;
     }
   }
   ```

2. **Search.cpp** — on beta cutoff from quiet move:
   ```cpp
   // Update counter-move score (in addition to storing the counter-move)
   if (SearchConfig.USE_COUNTERMOVE_HISTORY && prevMove != MOVE_NONE) {
     history.updateCounterMoveScore(prevMove.from(), prevMove.to(), depth * depth);
   }
   ```

#### Validation
- Add config flag to SearchTreeSizeTest
- Low effort; can be combined with Capture History for testing

---

## Feature 3: Continuation History

### Current State

FrankyCPP has basic history `[color][from][to]` but no multi-ply context.

**Gap:** No tracking of move sequences that work well together.

### Proposed Implementation

#### Config Flags
```cpp
// SearchConfigData.h
CONFIG_CONST bool USE_CONTINUATION_HISTORY = true;  // Enabled by default
CONFIG_CONST int CONT_HISTORY_BONUS = 32;
CONFIG_CONST int CONT_HISTORY_MAX = 8000;
```

#### Data Structure
```cpp
// History.h - add to History struct
// [previous_piece][previous_to][current_piece][current_to]
// Memory: 6 * 64 * 6 * 64 * sizeof(int16_t) = 294 KB (significant!)
int16_t contHistory[6][64][6][64]{};

void updateContHistory(PieceType prevPiece, Square prevTo, 
                       PieceType currPiece, Square currTo, int bonus);
[[nodiscard]] int getContHistoryScore(PieceType prevPiece, Square prevTo,
                                       PieceType currPiece, Square currTo) const;
```

#### PlyInfo Changes Required
```cpp
// PlyInfo.h - add fields to track previous move info
PieceType previousPiece{NO_PIECE_TYPE};  // Piece that moved in parent's move
Square previousTo{SQ_NONE};               // Destination of parent's move
```

#### Integration Points

1. **Search.cpp** — store previous move info in PlyInfo:
   ```cpp
   // Before recursive search call
   plyInfo[ply + 1].previousPiece = pieceType(position.pieceAt(move.from()));
   plyInfo[ply + 1].previousTo = move.to();
   ```

2. **MoveGenerator.cpp** — `updateSortValues()`:
   ```cpp
   // For quiet moves, add continuation history score
   if (SearchConfig.USE_CONTINUATION_HISTORY && !move.isCapture() 
       && plyInfo.previousPiece != NO_PIECE_TYPE) {
     const int contScore = history.getContHistoryScore(
       plyInfo.previousPiece, plyInfo.previousTo,
       pieceType(position.pieceAt(move.from())), move.to());
     sortValue += contScore / 4;
   }
   ```

3. **Search.cpp** — on beta cutoff:
   ```cpp
   if (SearchConfig.USE_CONTINUATION_HISTORY && plyInfo.previousPiece != NO_PIECE_TYPE) {
     history.updateContHistory(
       plyInfo.previousPiece, plyInfo.previousTo,
       pieceType(position.pieceAt(bestMove.from())), bestMove.to(),
       depth * depth);
   }
   ```

#### Validation
- Highest impact but also highest complexity
- Test after Capture History and Counter-Move History are stable
- Memory impact: ~294 KB — acceptable for modern systems

---

## Feature 4: SEE-Based Quiet Move Pruning — 🟡 IMPLEMENTED, ELO-NEUTRAL

### Current State

FrankyCPP uses SEE for:
- Ordering captures (good vs bad)
- QS delta pruning
- Check extension filtering

**Gap:** SEE not used to prune quiet moves that put pieces on attacked squares.

### Implementation (Complete)

#### Config Flags (SearchConfigData.h)
```cpp
CONFIG_CONST bool USE_SEE_QUIET_PRUNING  = true;
CONFIG_CONST int SEE_QUIET_PRUNE_DEPTH   = 4;   // Optimal for SMP (tested 4 vs 6)
CONFIG_CONST int SEE_QUIET_PRUNE_MARGIN  = -80; // Per-depth threshold (tested -40, -80, -120)
```

#### Integration Point (Search.cpp)
Located in forward pruning block (after FP, before LMP):
```cpp
// SEE-based Quiet Move Pruning
// Prune quiet moves when SEE indicates the piece lands on an attacked
// square where it can be captured for a material loss.
// The outer guard already ensures this is a quiet move (non-capture).
if (SearchConfig.USE_SEE_QUIET_PRUNING && depth <= SearchConfig.SEE_QUIET_PRUNE_DEPTH) {
  const int seeThreshold = SearchConfig.SEE_QUIET_PRUNE_MARGIN * depth;
  if (See::see(p, move) < Value{seeThreshold}) {
    STAT_INC(thread().statistics.seeQuietPruned);
    continue;
  }
}
```

#### Statistics Counter (SearchStats.h)
```cpp
uint64_t seeQuietPruned = 0;  // SEE-based quiet move pruning cuts
```

### Test Results

#### SearchTreeSizeTest (Fixed Depth 14, 4 Threads)

| Depth | Margin | Nodes vs Baseline | Time  | SEE Pruned | Result              |
|-------|--------|-------------------|-------|------------|---------------------|
| 4     | -80    | **-16.3%**        | -15%  | 10.9M      | ✅ Best              |
| 4     | -40    | +10.7%            | +12%  | 10.9M      | ❌ Too aggressive    |
| 4     | -120   | -4.1%             | -2%   | 9.4M       | 🟡 Too conservative |
| 6     | -80    | +0.6% (4T)        | +2.7% | 4.4M       | ❌ SMP regression    |

**Key Finding:** `depth=4, margin=-80` optimal. Higher depth (6) causes SMP issues where helper threads do redundant work.

#### Single-Thread vs Multi-Thread Comparison

| Metric             | 1 Thread | 4 Threads |
|--------------------|----------|-----------|
| Node reduction     | -18.5% ✅ | +0.6% ❌   |
| First-move cutoffs | -18.9%   | +8.3% ✅   |
| Time               | -16.8% ✅ | +2.7%     |

**Observation:** In SMP, pruning improves move ordering quality (more first-move cutoffs) but doesn't reduce total work — helper threads search the pruned subtrees.

#### Match Results

**vs v1.4 (TTBuckets baseline):**
- ELO: +74.6 (identical to baseline)
- Test Suite: -0.2% (slight regression, 3 suites improved, 2 regressed)
- W/D/L: 40/46/18 (more draws than baseline 42/42/20)

**vs v1.3:**
- ELO: +169.2 (vs +218.7 for TTBuckets) — **-49 ELO regression**
- Test Suite: +1.3% (vs +2.4% for TTBuckets)
- W/D/L: 60/31/13

### Analysis

**Why the feature is ELO-neutral despite large node reduction:**

1. **SMP neutralization**: In multi-threaded search, pruned subtrees are searched by helper threads anyway. The "saved" work shifts between threads rather than being eliminated.

2. **Test suite regression**: The -0.2% test suite accuracy suggests occasional tactical blindness — some quiet moves that look bad by SEE are actually defensive resources.

3. **Draw tendency**: More draws vs v1.4 (46 vs 42) indicates the pruning may cut some winning lines in equal positions.

### Decision: NOT MERGED

The feature provides meaningful node reduction in single-threaded mode but no strength gain in practice. Since FrankyCPP targets multi-threaded play, the feature is shelved.

**Code availability:** Implementation complete in separate branch for future reference or re-evaluation.

---

## Feature 5: History-Based Pruning

### Current State

History scores are used for:
- Move ordering (quiet moves sorted by history)
- LMR modulation (less reduction for good history)

**Gap:** Very bad history moves still searched at all depths.

### Proposed Implementation

#### Config Flags
```cpp
// SearchConfigData.h
CONFIG_CONST bool USE_HISTORY_PRUNING = true;      // Enabled by default
CONFIG_CONST int HISTORY_PRUNE_DEPTH = 4;          // Max depth for pruning
CONFIG_CONST int HISTORY_PRUNE_THRESHOLD = -4000;  // Very negative = known bad
```

#### Integration Point

**Search.cpp** — in move loop, for quiet moves:
```cpp
// Prune quiet moves with very bad history at low depths
if (SearchConfig.USE_HISTORY_PRUNING
    && depth <= SearchConfig.HISTORY_PRUNE_DEPTH
    && !move.isCapture()
    && !givesCheck
    && !isPvNode
    && movesSearched > 0) {  // Never prune first move
  
  const int histScore = history.historyCount[us][move.from()][move.to()];
  if (histScore < SearchConfig.HISTORY_PRUNE_THRESHOLD) {
    statistics.historyPruned++;
    continue;  // Skip this move
  }
}
```

#### Validation
- Conservative threshold to start (-4000 is very negative)
- Can combine with SEE quiet pruning (they complement each other)

---

## Validation Strategy

### Per-Feature Testing

Each feature follows this workflow:

```
1. Add config flag (default ON) to SearchConfigData.h
2. Add to ConfigRegistry.cpp
3. Implement feature gated by flag
4. Add to SearchTreeSizeTest.cpp initialization (OFF for baseline)
5. Add test entry measuring impact when enabled vs baseline
6. Run SearchTreeSizeTest — verify node change is expected direction
7. Run 200+ game match vs baseline (feature ON vs OFF)
8. If positive: keep enabled, update documentation
   If negative: disable by default, document findings
```

### Combined Testing

After individual validation:
- Test Feature 1 + 2 together (both affect ordering, minimal overlap)
- Test Feature 4 + 5 together (both are pruning, different criteria)
- Feature 3 alone (significant changes)
- Final: all enabled together

### Metrics to Track

| Metric              | Source             | Expected Change            |
|---------------------|--------------------|----------------------------|
| Nodes               | SearchTreeSizeTest | Decrease (better cutoffs)  |
| First-move cutoff % | Statistics         | Increase (better ordering) |
| NPS                 | SearchTreeSizeTest | Minimal change             |
| ELO                 | Match testing      | Increase                   |

---

## Timeline Estimate

| Feature              | Implementation | Testing | Total      | Status            |
|----------------------|----------------|---------|------------|-------------------|
| Capture History      | 2-3h           | 2h      | 4-5h       | ❌ Done (Failed)   |
| Counter-Move History | 1-2h           | 1h      | 2-3h       | 🔴 Not Started    |
| Continuation History | 4-6h           | 3h      | 7-9h       | 🔴 Not Started    |
| SEE Quiet Pruning    | 2-3h           | 2h      | 4-5h       | 🟡 Done (Neutral) |
| History Pruning      | 1-2h           | 1h      | 2-3h       | 🔴 Not Started    |
| **Remaining**        | **6-10h**      | **5h**  | **11-15h** |                   |

---

## References

- **Stockfish source:** `search.cpp`, `movepick.cpp`, `history.h`
- **Chess Programming Wiki:** History Heuristic, Counter-Move History
- **Previous work:** `archive/PLAN_Search_Tree_Reduction_Review.md`

---

## Change Log

| Version | Date       | Changes                                                                                                                                                                                             |
|---------|------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1.0     | 2026-03-07 | Initial document extracted from Search Tree Reduction Review                                                                                                                                        |
| 1.1     | 2026-03-07 | Feature 1 (Capture History) implemented and FAILED: -19% nodes but -5.6% test suite, -4.1% vs v1.3. Code REVERTED, document retained for reference.                                                 |
| 1.2     | 2026-03-08 | Feature 4 (SEE Quiet Pruning) implemented and tested: -16% nodes (1T) but ELO-neutral (+74.6 vs v1.4 = same as baseline). Best params: depth=4, margin=-80. Code in branch, NOT merged to dev-v1.5. |

---

*Document created as continuation of FrankyCPP search efficiency improvement initiative*
