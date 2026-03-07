# FrankyCPP Move Ordering & Pruning Improvements Plan

**Document Version:** 1.0  
**Created:** 2026-03-07  
**Last Updated:** 2026-03-07  
**Status:** 🔴 NOT STARTED  
**Target:** FrankyCPP v1.5  
**Priority:** Medium (Incremental strength improvements)  
**Predecessor:** `archive/PLAN_Search_Tree_Reduction_Review.md` (Phase 1 LMR complete)

---

## Executive Summary

This plan documents potential move ordering and pruning improvements for FrankyCPP, extracted from the completed Search Tree Reduction Review. Phase 1 (LMR improvements) achieved **+177 ELO** and is complete. The remaining features target move ordering quality (Phase 2) and additional pruning techniques (Phase 3).

**Current State:** FrankyCPP v1.5 has solid LMR, improving flag integration, and basic history heuristics. The next level of strength gains requires more sophisticated move ordering to increase cutoff rates.

---

## Hypotheses: Expected Improvements

Each proposed feature is based on Stockfish's implementation and chess programming theory. Here's why each is expected to improve strength:

### 1. Capture History Table

| Aspect                  | Description                                                                                           |
|-------------------------|-------------------------------------------------------------------------------------------------------|
| **Hypothesis**          | Ordering captures by historical success rate (not just MVV-LVA) will produce more cutoffs             |
| **Mechanism**           | Track which piece×square×victim combinations caused beta cutoffs; boost those captures in ordering    |
| **Expected Impact**     | Medium-High — Better capture ordering in tactical positions                                           |
| **Evidence**            | Stockfish uses capture history; exchanges that "work" in one position often work in similar positions |
| **Validation Metric**   | Fewer nodes (better cutoff rate); measure first-move cutoff % increase; ELO gain in matches           |

**Why it should work:** MVV-LVA assumes QxP > BxP, but in practice a bishop capturing a specific pawn might be a recurring tactical theme. Capture history learns these patterns.

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

### 4. SEE-Based Quiet Move Pruning

| Aspect                  | Description                                                                         |
|-------------------------|-------------------------------------------------------------------------------------|
| **Hypothesis**          | Quiet moves that lose material (negative SEE) at low depths are unlikely to be best |
| **Mechanism**           | At low depths, skip quiet moves where the moving piece can be captured for a loss   |
| **Expected Impact**     | Medium — Reduces nodes in tactical positions                                        |
| **Evidence**            | Stockfish prunes quiet moves with bad SEE scaled by depth                           |
| **Validation Metric**   | Fewer nodes (direct pruning); NPS may drop slightly (SEE cost); net ELO gain        |

**Why it should work:** If a quiet move puts a piece on a square where it can be captured for free, that move is almost never best at shallow depths. Only search it at higher depths where tactics might justify the sacrifice.

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

| Priority | Feature                       | Effort | Expected Impact | Dependencies    |
|----------|-------------------------------|--------|-----------------|-----------------|
| 1        | Capture History               | 2-3h   | Medium-High     | None            |
| 2        | Counter-Move History (Scored) | 1-2h   | Medium          | None            |
| 3        | Continuation History          | 4-6h   | High            | PlyInfo changes |
| 4        | SEE-Based Quiet Pruning       | 2-3h   | Medium          | None            |
| 5        | History-Based Pruning         | 1-2h   | Medium          | None            |

**Recommended approach:** Implement and test each feature individually. Combining untested features makes it impossible to attribute gains/losses.

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

## Feature 4: SEE-Based Quiet Move Pruning

### Current State

FrankyCPP uses SEE for:
- Ordering captures (good vs bad)
- QS delta pruning
- Check extension filtering

**Gap:** SEE not used to prune quiet moves that put pieces on attacked squares.

### Proposed Implementation

#### Config Flags
```cpp
// SearchConfigData.h
CONFIG_CONST bool USE_SEE_QUIET_PRUNING = true;   // Enabled by default
CONFIG_CONST int SEE_QUIET_PRUNE_DEPTH = 6;       // Max depth for pruning
CONFIG_CONST int SEE_QUIET_PRUNE_MARGIN = -80;    // Threshold (negative = allow slight loss)
```

#### Integration Point

**Search.cpp** — in move loop, before searching quiet moves:
```cpp
// Prune quiet moves that lose material at low depths
if (SearchConfig.USE_SEE_QUIET_PRUNING 
    && depth <= SearchConfig.SEE_QUIET_PRUNE_DEPTH
    && !move.isCapture()
    && !givesCheck
    && !isPvNode
    && movesSearched > 0) {  // Never prune first move
  
  const int seeThreshold = SearchConfig.SEE_QUIET_PRUNE_MARGIN * depth;
  if (see(position, move) < seeThreshold) {
    statistics.seeQuietPruned++;
    continue;  // Skip this move
  }
}
```

#### Validation
- Add statistics counter for tracking
- Test carefully — overly aggressive pruning can miss tactical defenses
- Start with conservative threshold (-80), tune based on results

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

| Feature              | Implementation | Testing | Total      |
|----------------------|----------------|---------|------------|
| Capture History      | 2-3h           | 2h      | 4-5h       |
| Counter-Move History | 1-2h           | 1h      | 2-3h       |
| Continuation History | 4-6h           | 3h      | 7-9h       |
| SEE Quiet Pruning    | 2-3h           | 2h      | 4-5h       |
| History Pruning      | 1-2h           | 1h      | 2-3h       |
| **Total**            | **10-16h**     | **9h**  | **19-25h** |

---

## References

- **Stockfish source:** `search.cpp`, `movepick.cpp`, `history.h`
- **Chess Programming Wiki:** History Heuristic, Counter-Move History
- **Previous work:** `archive/PLAN_Search_Tree_Reduction_Review.md`

---

## Change Log

| Version | Date       | Changes                                                      |
|---------|------------|--------------------------------------------------------------|
| 1.0     | 2026-03-07 | Initial document extracted from Search Tree Reduction Review |

---

*Document created as continuation of FrankyCPP search efficiency improvement initiative*
