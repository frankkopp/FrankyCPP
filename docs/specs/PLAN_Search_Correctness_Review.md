# Search Feature Correctness Review Plan

**Created:** 2026-02-21  
**Status:** In Progress  
**Purpose:** Systematic correctness review of all search features

---

## ⚠️ CRITICAL RULE: NO CODE CHANGES WITHOUT APPROVAL

**Before making ANY code changes:**
1. Complete the analysis for the feature
2. Document findings in this plan
3. Outline proposed changes clearly
4. **ASK FOR EXPLICIT APPROVAL**
5. Only proceed with code edits after user says "go ahead", "approved", "proceed", etc.

**Allowed without approval:**
- Reading source files
- Analyzing code
- Updating this plan document with findings
- Comparing to other engines

---

## Objective

Review each search feature for **correctness** - verifying implementations match:
- Chess programming theory
- Logical requirements (guards, conditions, edge cases)
- Established engine implementations (Stockfish, Ethereal, Crafty, etc.)

Focus is on finding bugs and correctness issues, not just tuning parameters.

---

## Review Order (by engine evolution/importance)

Features ordered by how a chess engine naturally evolves - core algorithm first.

| Phase                   | #  | Feature                        | Status    | Notes                             |
|-------------------------|----|--------------------------------|-----------|-----------------------------------|
| **Core Algorithm**      |    |                                |           |                                   |
|                         | 1  | Alpha-Beta / PVS               | ✅ Correct | Reviewed 2026-02-21               |
|                         | 2  | Aspiration Windows             | ✅ Fixed   | Comment fix applied               |
|                         | 3  | Quiescence Search              | ✅ Fixed   | Removed history/killer updates    |
| **Infrastructure**      |    |                                |           |                                   |
|                         | 4  | TT Lookup & Cutoff             | ✅ Correct | Textbook correct                  |
|                         | 5  | TT Storage                     | ✅ Correct | Replacement + bound types correct |
|                         | 6  | Draw Detection                 | ✅ Correct | Repetition + 50-move correct      |
|                         | 7  | Static Eval                    | ✅ Correct | Integration correct               |
| **Move Ordering**       |    |                                |           |                                   |
|                         | 8  | Move Ordering (overall)        | ✅ Correct | MVV-LVA, killers, history correct |
|                         | 9  | History Heuristic              | ✅ Correct | Bonus/penalty/ordering correct    |
|                         | 10 | Killer Moves                   | ✅ Correct | Per-ply, FIFO, protected          |
|                         | 11 | Counter Moves                  | ✅ Correct | Refutation tracking correct       |
| **Simple Pruning**      |    |                                |           |                                   |
|                         | 12 | Mate Distance Pruning          | ✅ Correct | Both bounds correct               |
|                         | 13 | Razoring                       | ✅ Correct | Fixed in v1.3                     |
|                         | 14 | Reverse Futility Pruning (RFP) | ⬜ Pending | Static eval based                 |
|                         | 15 | Futility Pruning (FP)          | ⬜ Pending | Move-level pruning                |
| **Advanced Pruning**    |    |                                |           |                                   |
|                         | 16 | Null Move Pruning (NMP)        | ⬜ Pending | Zugzwang handling                 |
|                         | 17 | Late Move Pruning (LMP)        | ⬜ Pending | Move count based                  |
|                         | 18 | Late Move Reduction (LMR)      | ⬜ Pending | Already tuned (+82 ELO)           |
| **Extensions**          |    |                                |           |                                   |
|                         | 19 | Check Extension                | ⬜ Pending | Basic extension                   |
|                         | 20 | Singular Extension             | ⬜ Pending | Complex implementation            |
|                         | 21 | Threat Extension               | ⬜ Pending | Currently disabled                |
| **Search Enhancements** |    |                                |           |                                   |
|                         | 22 | IID / IIR                      | ⬜ Pending | Finding moves without TT          |
|                         | 23 | Tablebase Probing              | ⬜ Pending | Endgame knowledge                 |
| **Control**             |    |                                |           |                                   |
|                         | 24 | Time Management                | ⬜ Pending | Resource allocation               |

---

## Checklist Per Feature

For each feature, verify:

- [ ] **Guards/Conditions** - Correct checks (`!isPvNode`, `!inCheck`, depth thresholds)
- [ ] **Formulas** - Calculations match theory (margins, reductions, bonuses)
- [ ] **Edge Cases** - Special handling (mate scores, draws, zugzwang)
- [ ] **Theory Compliance** - Matches chess programming wiki / literature
- [ ] **Cross-Engine Check** - Compare to Stockfish, Ethereal, etc.

---

## Completed Reviews

### Feature #1: Alpha-Beta / PVS
**Date:** 2026-02-21  
**Status:** ✅ Correct (with minor observations)

**Implementation Location:** 
- `src/engine/Search.cpp` lines 768-862 (rootSearch)
- `src/engine/Search.cpp` lines 864-1639 (search)
- `src/engine/Search.cpp` lines 1641-end (qsearch)

**Theory Background:**
- Alpha-beta is a minimax optimization that prunes branches that cannot affect the final result
- PVS (Principal Variation Search) assumes first move is best, searches others with null window
- Node types: PvNode (full window), CutNode (expect fail-high), AllNode (expect fail-low)

**Checks:**

#### ✅ Core Alpha-Beta Logic (CORRECT)
- Beta cutoff at line 1576: `if (value >= beta && SearchConfig.USE_ALPHABETA)`
- Alpha update at line 1601: `alpha = value`
- Returns `bestNodeValue` correctly
- Mate/stalemate detection at lines 1616-1632

#### ✅ PVS Implementation (CORRECT)
The PVS logic follows the correct pattern:

**Root Search (lines 807-820):**
```cpp
if (!SearchConfig.USE_PVS || i == 0) {
  value = -search(p, depth - 1, ply, -beta, -alpha, PvNode, Do_Null_Move);
} else {
  value = -search(p, depth - 1, ply, -alpha - 1, -alpha, CutNode, Do_Null_Move);
  if (value > alpha && value < beta && !stopConditions()) {
    value = -search(p, depth - 1, ply, -beta, -alpha, PvNode, Do_Null_Move);
  }
}
```

**Internal Search (lines 1505-1536):**
```cpp
if (!SearchConfig.USE_PVS || movesSearched == 0) {
  const NodeType childType = (nodeType == PvNode) ? PvNode : (nodeType == CutNode ? AllNode : CutNode);
  value = -search(p, newDepth, ply + 1, -beta, -alpha, childType, do_null);
} else {
  const NodeType childType = (nodeType == CutNode) ? AllNode : CutNode;
  value = -search(p, lmrDepth, ply + 1, -alpha - 1, -alpha, childType, do_null);
  if (value > alpha && !stopConditions()) {
    // LMR re-search or PVS re-search
    const NodeType researchType = (nodeType == PvNode) ? PvNode : childType;
    value = -search(p, newDepth, ply + 1, -beta, -alpha, researchType, do_null);
  }
}
```

#### ✅ Node Type Propagation (CORRECT - was fixed in v1.3)
- PvNode's first child → PvNode ✅
- PvNode's other children → CutNode (null window) ✅
- CutNode's children → AllNode ✅
- AllNode's children → CutNode ✅
- Re-searches correctly inherit PvNode when parent is PvNode ✅

#### ✅ PV Table Updates (CORRECT)
- `pv.clear(ply)` at search entry (line 871)
- `pv.update(move, ply)` only when move raises alpha AND is < beta (line 1597)
- Triangular table correctly propagates child PV

#### ✅ Window Negation (CORRECT)
- All recursive calls correctly negate and swap alpha/beta: `-beta, -alpha`
- Null window correctly uses `-alpha - 1, -alpha`

#### ⚠️ Minor Observation: Re-search condition (ANALYZED - NO ISSUE)
Line 1521: `if (value > alpha && !stopConditions() && !isTimeAlmostUp())`
Line 1529: `else if (value < beta)`

The re-search logic:
1. If LMR was applied (`lmrDepth < newDepthFixed`), re-search when `value > alpha`
2. Otherwise (no LMR), re-search when `value > alpha && value < beta`

**Why `else if` is correct:**
- The LMR re-search already uses **full window** (`-beta, -alpha`), not null window
- So the value from LMR re-search is already accurate (not a bound)
- No additional PVS re-search needed after LMR re-search
- The `else if` correctly ensures PVS re-search only happens when no LMR was applied

**Alternative considered (sequential checks, Stockfish-style):**
```cpp
// LMR re-search
if (value > alpha && lmrDepth < newDepthFixed) {
  value = -search(..., -beta, -alpha, ...);  // full window
}
// PVS re-search  
if (value > alpha && value < beta) {
  value = -search(..., -beta, -alpha, ...);  // full window
}
```
This would re-check after LMR, but since LMR re-search uses full window, the second
check would never trigger (value already accurate). **No functional benefit.**

**Current code is correct and efficient.**

#### ✅ Edge Cases (CORRECT)
- Depth == 0 → qsearch (line 875)
- ply >= MAX_DEPTH → qsearch (line 875)
- Stop conditions checked appropriately
- Draw detection before recursion (line 1486)

**Findings:**
1. ✅ Alpha-beta pruning is correctly implemented
2. ✅ PVS null-window search is correct
3. ✅ Node type alternation is correct (fixed in v1.3)
4. ✅ PV table management is correct
5. ✅ Window negation is correct
6. ✅ Re-search logic is correct - LMR re-search uses full window so no additional PVS re-search needed

**Changes Made:** None required

---

### Feature #2: Aspiration Windows
**Date:** 2026-02-21  
**Status:** ⚠️ Potential Issue Found

**Implementation Location:** 
- `src/engine/Search.cpp` lines 513-514 (call site)
- `src/engine/Search.cpp` lines 715-766 (aspirationSearch function)

**Theory Background:**
- Aspiration windows narrow the alpha-beta window around the expected value
- Starts with small window, expands on fail-high or fail-low
- Reduces search tree size when prediction is accurate
- Must handle both fail-high and fail-low correctly

**Checks:**

#### ✅ Initial Window Setup (CORRECT)
```cpp
constexpr std::array aspirationSteps = {Value{50}, Value{200}, VALUE_MAX};
Value alpha = std::max(bestValue - aspirationSteps[0], VALUE_MIN);
Value beta  = std::min(bestValue + aspirationSteps[0], VALUE_MAX);
```
- Window centered on previous iteration's best value ✅
- Clamped to valid range ✅
- Reasonable step sizes (50, 200, then full window) ✅

#### ✅ Activation Condition (CORRECT)
```cpp
if (SearchConfig.USE_ASP && iterationDepth > 3) {
  bestValue = aspirationSearch(p, iterationDepth, bestValue);
}
```
- Only activates at depth > 3 (shallow depths have unstable values) ✅

#### ⚠️ POTENTIAL ISSUE: Fail-Low Window Expansion
```cpp
if (value <= alpha) {
  // FAIL LOW - decrease upper bound   <-- COMMENT IS WRONG
  sendAspirationResearchInfo("upperbound");
  // ...
  alpha = std::max(bestValue - aspirationSteps[i], VALUE_MIN);  // Only alpha changes
  // beta unchanged!
}
```

**Issue Analysis:**
- Comment says "decrease upper bound" but code only changes alpha (lower bound)
- On fail-low, we need to **widen alpha** (search lower) - this is correct
- BUT: beta is NOT reset to `bestValue + aspirationSteps[i]`

**Is this a bug?** Let me think through the scenarios:

1. **First search**: window = [bestValue-50, bestValue+50]
2. **Fail low** (value <= alpha): alpha drops to bestValue-200, beta stays at bestValue+50
3. **Second search**: window = [bestValue-200, bestValue+50]

This is actually **asymmetric expansion** - only expanding the failing side.
This is a **valid approach** and arguably more efficient than symmetric expansion.

**However**, Stockfish typically does:
- Fail-low: expand alpha, keep beta OR reset beta to +infinity
- Fail-high: expand beta, keep alpha

The current code keeps the opposite bound tight, which is fine.

**The comment is misleading but the code is functionally correct.**

#### ✅ Fail-High Window Expansion (CORRECT)
```cpp
else if (value >= beta) {
  // FAIL HIGH - increase upper bound
  beta = std::min(bestValue + aspirationSteps[i], VALUE_MAX);
}
```
- Correctly expands beta on fail-high ✅
- Alpha unchanged (asymmetric, same pattern) ✅

#### ✅ Stop/Time Handling (CORRECT)
- Returns VALUE_NONE if stopped mid-search with value outside window (line 731)
- Returns current value if time almost up (lines 734, 744, 755)
- Adds extra time on fail-low (line 742) - good for finding better moves

#### ⚠️ Minor: Loop starts at i=1
```cpp
for (auto i = 1; i < steps; ++i) {
```
- First search uses `aspirationSteps[0]` (50)
- On fail, expands to `aspirationSteps[1]` (200), then `aspirationSteps[2]` (VALUE_MAX)
- This is correct but slightly confusing - the loop index doesn't match the step being used

#### ✅ Final Window Guarantee (CORRECT)
- `aspirationSteps[2] = VALUE_MAX` ensures final search has full window
- Loop will always terminate with a valid result

**Findings:**
1. ✅ Window centering and clamping correct
2. ✅ Activation depth threshold correct
3. ⚠️ Misleading comment "decrease upper bound" on fail-low (code is correct)
4. ✅ Asymmetric expansion is valid approach
5. ✅ Stop/time handling correct
6. ✅ Final full-window fallback correct

**Proposed Changes:**
1. ~~Fix misleading comment on line 738~~ ✅ DONE

**Changes Made:** Fixed comment "FAIL LOW - decrease upper bound" → "FAIL LOW - widen alpha (lower bound)"

---

### Feature #3: Quiescence Search
**Date:** 2026-02-21  
**Status:** ⚠️ Potential Issue Found

**Implementation Location:** 
- `src/engine/Search.cpp` lines 1641-1881 (qsearch function)
- `src/engine/Search.cpp` lines 1889-1915 (goodCapture helper)

**Theory Background:**
- Quiescence search resolves tactical instability at leaf nodes
- Searches captures (and checks) until position is "quiet"
- Stand-pat: use static eval as lower bound (can always choose not to capture)
- When in check: must search ALL moves (no stand-pat, can't ignore check)

**Checks:**

#### ✅ Stand-Pat Implementation (CORRECT)
```cpp
if (!hasCheck) {
  if (staticEval == VALUE_NONE) {
    staticEval = evaluate(p);
  }
  if (SearchConfig.USE_QS_STANDPAT_CUT && staticEval > alpha) {
    if (staticEval >= beta) {
      return staticEval;  // Beta cutoff
    }
    alpha = staticEval;   // Raise alpha
  }
  bestNodeValue = staticEval;
}
```
- Only when NOT in check ✅
- Correctly raises alpha when eval > alpha ✅
- Returns early if eval >= beta ✅
- Sets bestNodeValue to staticEval as fallback ✅

#### ✅ Check Handling (CORRECT)
```cpp
const GenMode genMode = hasCheck ? GenAll : GenNonQuiet;
```
- When in check: generates ALL moves (must escape check) ✅
- When not in check: only generates captures/promotions ✅

#### ✅ Mate Detection (CORRECT)
```cpp
if (movesSearched == 0 && !stopConditions()) {
  if (hasCheck) {
    // mate
    bestNodeValue = -VALUE_CHECKMATE + static_cast<Value>(ply);
    ttType        = EXACT;
  }
  // else: no captures available, return standpat (bestNodeValue = staticEval)
}
```
- Only declares mate when in check with no legal moves ✅
- When not in check, correctly returns stand-pat value ✅

#### ✅ Good Capture Filter (CORRECT)
```cpp
if (!hasCheck && !goodCapture(p, move, givesCheck)) { continue; }
```
- Only filters when NOT in check ✅
- Uses SEE or heuristics to determine if capture is "good" ✅

#### ✅ TT Usage (CORRECT)
- TT lookup with cutoffs for non-PV nodes ✅
- TT storage at end ✅
- Reuses stored static eval ✅

#### ✅ Delta/Futility Pruning (QFP) (CORRECT)
```cpp
if (SearchConfig.USE_QFP
    && nodeType != PvNode
    && move != ttMove
    && move != myMg->getKillerMoves()[0]
    && move != myMg->getKillerMoves()[1]
    && move.type() != PROMOTION
    && !hasCheck
    && !givesCheck) {
  const auto moveGain = valueOf(p.getPiece(to));
  constexpr auto futilityMargin = Value{150};
  if (staticEval + moveGain + futilityMargin <= alpha) {
    // prune
  }
}
```
- Correct guards (not PV, not in check, not giving check, not promotion) ✅
- Reasonable margin (150 cp) ✅

#### ⚠️ POTENTIAL ISSUE: History Updates in QSearch
```cpp
if (SearchConfig.USE_HISTORY_COUNTER) { history.historyCount[us][from][to] += 1 << 1; }
// ...
if (SearchConfig.USE_HISTORY_COUNTER) {
  history.historyCount[us][from][to] -= 1 << 1;
  if (history.historyCount[us][from][to] < 0) { history.historyCount[us][from][to] = 0; }
}
```

**Issue Analysis:**
1. History is typically for **quiet moves** that cause beta cutoffs
2. QSearch primarily searches **captures**, not quiet moves
3. Applying history to captures can pollute the history table
4. The penalty is applied to ALL moves that don't raise alpha, including captures

**Compare to main search (line 1585, 1608):**
```cpp
if (SearchConfig.USE_HISTORY_COUNTER && !p.isCapturingMove(move)) { ... }
```
Main search correctly guards with `!p.isCapturingMove(move)`.

**QSearch is missing this guard!** It applies history to captures.

#### ⚠️ POTENTIAL ISSUE: Killer Moves for Captures
```cpp
if (SearchConfig.USE_KILLER_MOVES && !p.isCapturingMove(move)) { myMg->storeKiller(move); }
```
This is actually **correct** - has the `!p.isCapturingMove(move)` guard.
But in qsearch, we mostly search captures, so this rarely triggers.
The guard is correct, just potentially useless in this context.

**Findings:**
1. ✅ Stand-pat logic is correct
2. ✅ Check handling is correct (GenAll when in check)
3. ✅ Mate detection is correct
4. ✅ Good capture filter is correct
5. ✅ Delta pruning (QFP) is correct
6. ⚠️ **History/killer updates don't belong in qsearch** - qsearch primarily searches captures, history/killers are for quiet move ordering
7. ✅ Killer move storage had correct guard but was unnecessary

**Proposed Changes:**
1. ~~Add `!p.isCapturingMove(move)` guard to history updates~~ → Better solution: remove entirely

**Changes Made:** 
- Removed killer move storage from qsearch (lines 1833)
- Removed history bonus on beta cutoff from qsearch (line 1834)
- Removed counter move storage from qsearch (lines 1835-1838)
- Removed history penalty from qsearch (lines 1847-1850)
- Added comments explaining why these were removed
- Removed unused variables (`us`, `from`) that were only used for history

---

### Feature #4: TT Lookup & Cutoff
**Date:** 2026-02-21  
**Status:** ✅ Correct

**Implementation Location:** 
- `src/engine/Search.cpp` lines 914-954 (main search TT lookup)
- `src/engine/Search.cpp` lines 1673-1699 (qsearch TT lookup)
- `src/engine/Search.cpp` lines 1919-1933 (valueToTt/valueFromTt)
- `src/engine/TT.h` (TT entry structure)

**Theory Background:**
- TT stores search results to avoid re-searching positions
- Entry types: EXACT (exact score), ALPHA (upper bound), BETA (lower bound)
- Must handle mate scores specially (adjust by ply)
- Cutoff conditions depend on entry type and current bounds

**Checks:**

#### ✅ TT Probe (CORRECT)
```cpp
if (const TT::Entry* ttEntryPtr = tt->probe(p.getZobristKey())) {
  ttMove  = static_cast<Move>(ttEntryPtr->move);
  ttValue = valueFromTt(ttEntryPtr->value, ply);
  ttDepth = static_cast<Depth>(ttEntryPtr->depth);
```
- Probes by Zobrist key ✅
- Extracts move, value (with ply adjustment), and depth ✅

#### ✅ TT Cutoff Conditions (CORRECT)
```cpp
if (nodeType != PvNode && ttDepth >= depth) {
  if (SearchConfig.USE_TT_VALUE
      && ttValue.isValid()
      && (ttEntryPtr->type == EXACT
          || (ttEntryPtr->type == ALPHA && ttValue <= alpha)
          || (ttEntryPtr->type == BETA && ttValue >= beta))) {
    return ttValue;
  }
}
```

**Analysis of cutoff logic:**
1. `nodeType != PvNode` - Never cut off on PV nodes (need complete PV line) ✅
2. `ttDepth >= depth` - Only use if search was at least as deep ✅
3. `ttValue.isValid()` - Value must be valid ✅
4. Cutoff conditions:
   - `EXACT` - Exact score, always usable ✅
   - `ALPHA && ttValue <= alpha` - Upper bound ≤ alpha means all moves fail low ✅
   - `BETA && ttValue >= beta` - Lower bound ≥ beta means we have a beta cutoff ✅

**This is textbook correct!**

#### ✅ Mate Score Adjustment (CORRECT)
```cpp
Value Search::valueToTt(const Value value, const Depth ply) {
  if (value.isCheckMate()) {
    if (value > 0) { return value + static_cast<Value>(ply); }  // Store as distance from root
    return value - static_cast<Value>(ply);
  }
  return value;
}

Value Search::valueFromTt(const Value value, const Depth ply) {
  if (value.isCheckMate()) {
    if (value > 0) { return value - static_cast<Value>(ply); }  // Adjust to current ply
    return value + static_cast<Value>(ply);
  }
  return value;
}
```

**Analysis:**
- Mate scores are stored relative to the root (distance from root to mate)
- When retrieved, they're adjusted to be relative to current ply
- Positive mate (we're winning): store as `value + ply`, retrieve as `value - ply` ✅
- Negative mate (we're losing): store as `value - ply`, retrieve as `value + ply` ✅

**This ensures mate-in-3 at ply 5 is stored as mate-in-8 from root, and if retrieved at ply 2, becomes mate-in-6.**

#### ✅ Static Eval Caching (CORRECT)
```cpp
if (SearchConfig.USE_EVAL_TT && ttEntryPtr->eval != VALUE_NONE) {
  statistics.evalFromTT++;
  staticEval = ttEntryPtr->eval;
}
```
- Reuses cached static eval when available ✅
- Saves evaluation calls ✅

#### ✅ QSearch TT Lookup (CORRECT)
```cpp
// qsearch - no depth check needed (depth is always 0 in qsearch)
if (SearchConfig.USE_TT_VALUE
    && nodeType != PvNode
    && ttValue.isValid()
    && (ttEntryPtr->type == EXACT
        || (ttEntryPtr->type == ALPHA && ttValue <= alpha)
        || (ttEntryPtr->type == BETA && ttValue >= beta))) {
  return ttValue;
}
```
- Same cutoff logic as main search ✅
- No depth check (qsearch is depth 0) - correct ✅

#### ✅ TT Move Usage (CORRECT)
- TT move is used for move ordering (set as PV move in MoveGenerator)
- Also used for singular extension verification
- Move is extracted even when cutoff doesn't occur ✅

**Findings:**
1. ✅ TT probe is correct
2. ✅ Cutoff conditions are textbook correct (EXACT, ALPHA<=alpha, BETA>=beta)
3. ✅ PV node exclusion is correct
4. ✅ Depth check is correct
5. ✅ Mate score adjustment is correct (valueToTt/valueFromTt)
6. ✅ Static eval caching is correct
7. ✅ QSearch TT lookup mirrors main search correctly

**Changes Made:** None required

---

### Feature #5: TT Storage
**Date:** 2026-02-21  
**Status:** ✅ Correct

**Implementation Location:** 
- `src/engine/Search.cpp` lines 1908-1917 (storeTt wrapper)
- `src/engine/TT.cpp` lines 127-185 (TT::put)
- `src/engine/Search.cpp` lines 1576-1636 (where ttType is set)

**Theory Background:**
- TT storage must correctly classify values as EXACT, ALPHA (upper bound), or BETA (lower bound)
- Replacement strategy decides when to overwrite existing entries
- Must preserve move even when storing eval-only entries
- Age-based replacement helps prefer fresh entries

**Checks:**

#### ✅ Value Type Assignment (CORRECT)
```cpp
ValueType ttType = ALPHA;  // Default: fail-low (no move raised alpha)

if (value >= beta) {
  ttType = BETA;   // Fail-high: value is lower bound
  break;
}
if (value > alpha) {
  alpha = value;
  ttType = EXACT;  // Exact: value is within window
}
// else: ttType stays ALPHA (fail-low: value is upper bound)
```

**Analysis:**
- Default is ALPHA (upper bound) - if no move raises alpha, all moves failed low ✅
- BETA set on beta cutoff (value ≥ beta) - we have a lower bound ✅
- EXACT set when value raises alpha but doesn't exceed beta ✅

#### ✅ Mate/Stalemate Handling (CORRECT)
```cpp
if (movesSearched == 0 && !stopConditions()) {
  if (hasCheck) {
    bestNodeValue = -VALUE_CHECKMATE + static_cast<Value>(ply);
  } else {
    bestNodeValue = VALUE_DRAW;  // stalemate
  }
  ttType = EXACT;  // Mate and stalemate are always exact
}
```
- Mate/stalemate values are always EXACT ✅
- Mate score adjusted by ply ✅

#### ✅ storeTt Wrapper (CORRECT)
```cpp
void Search::storeTt(...) const {
  tt->put(p.getZobristKey(), depth, move, valueToTt(value, ply), valueType, eval);
}
```
- Correctly applies mate score adjustment via valueToTt ✅
- Passes all necessary information to TT ✅

#### ✅ TT::put Replacement Strategy (CORRECT)
```cpp
// New entry (empty slot)
if (entry->key == 0) {
  // Store new entry
}
// Collision (different position)
else if (entry->key != key) {
  // Overwrite if new depth > old depth
  // OR if same depth and old entry is aged
  if (depth > entry->depth || (depth == entry->depth && entry->age > 0)) {
    // Overwrite
  }
}
// Same position (update)
else {
  // Keep existing move if no move given
  // Update value/depth/type only if valid value
  // Update eval only if valid eval
}
```

**Analysis:**
- Empty slots: always store ✅
- Collisions: prefer deeper searches; if same depth, prefer fresh entries ✅
- Same position: update but preserve existing move if none given ✅
- Eval-only updates (`value == VALUE_NONE`) preserve existing search data ✅

#### ✅ Age-Based Replacement (CORRECT)
- New entries get `age = 1`
- On probe hit, age decremented (marks as "used")
- `ageEntries()` increments age of all entries (up to 7)
- Higher age = older/unused entry = more likely to be replaced

This is a sensible aging scheme that favors recently-used entries.

#### ✅ Move Preservation (CORRECT)
```cpp
// keep existing move if no move is given
if (move) {
  entry->move = static_cast<uint16_t>(move);
}
```
- When storing eval-only (MOVE_NONE), existing move is preserved ✅
- Important for maintaining TT move ordering hints ✅

#### ✅ QSearch TT Storage (CORRECT)
```cpp
if (SearchConfig.USE_TT && SearchConfig.USE_QS_TT) {
  storeTt(p, DEPTH_NONE, ply, bestNodeMove, bestNodeValue, ttType, staticEval);
}
```
- Uses DEPTH_NONE for qsearch entries ✅
- Same value type logic as main search ✅

**Findings:**
1. ✅ Value types (EXACT/ALPHA/BETA) correctly assigned
2. ✅ Mate score adjustment via valueToTt
3. ✅ Replacement strategy is reasonable (depth + age)
4. ✅ Move preservation for eval-only updates
5. ✅ Age-based freshness tracking
6. ✅ Same-position updates handled correctly

**Changes Made:** None required

---

### Feature #6: Draw Detection
**Date:** 2026-02-21  
**Status:** ✅ Correct

**Implementation Location:** 
- `src/engine/Search.cpp` line 2185-2187 (checkDrawRepAnd50)
- `src/chesscore/Position.cpp` lines 555-599 (checkRepetitions)
- Search call sites: lines 291, 704, 798, 1486, 1806

**Theory Background:**
- Draws must be detected: threefold repetition and 50-move rule
- Repetition detection only looks at positions with same side to move
- Cannot span across irreversible moves (captures, pawn moves)
- 50-move rule: draw if 100 half-moves without capture or pawn move

**Checks:**

#### ✅ checkDrawRepAnd50 (CORRECT)
```cpp
bool Search::checkDrawRepAnd50(const Position& p, const int numberOfRepetitions) {
  return p.checkRepetitions(numberOfRepetitions) || p.getHalfMoveClock() >= 100;
}
```
- Checks repetitions OR 50-move rule ✅
- Uses `>= 100` for half-move clock (100 half-moves = 50 full moves) ✅
- `numberOfRepetitions = 2` means detecting twofold (position seen once before) ✅

#### ✅ checkRepetitions Algorithm (CORRECT)
```cpp
// Step back by 2 (same side to move)
int i = historyCounter - 2;
while (i >= 0) {
  // Stop at irreversible boundary
  if (historyState[i].halfMoveClock >= lastHalfMoveClock) {
    break;
  }
  // Check for matching Zobrist key
  if (zobristKey == historyState[i].zobristKey) {
    counter++;
    if (counter >= reps) return true;
  }
  i -= 2;
}
```

**Analysis:**
- Steps back by 2 (only same-side positions can repeat) ✅
- Detects irreversible boundary correctly: half-move clock should strictly decrease going backward; if it increases or stays same, we crossed a reset ✅
- Uses Zobrist key comparison (position identity) ✅
- Early exit optimization when impossible to reach required reps ✅

#### ✅ Search Integration (CORRECT)

**At root (iterativeDeepening):**
```cpp
if (checkDrawRepAnd50(p, 2)) {
  searchResult.bestMoveValue = VALUE_DRAW;
  return searchResult;
}
```
- Immediately returns draw if root position is already drawn ✅

**In main search (after doMove):**
```cpp
if (checkDrawRepAnd50(p, 2)) { value = VALUE_DRAW; }
else { /* search recursively */ }
```
- Checks AFTER move is made (position after move) ✅
- Returns VALUE_DRAW without recursing ✅

**In qsearch (after doMove):**
```cpp
if (checkDrawRepAnd50(p, 2)) {
  value = VALUE_DRAW;
}
else { /* qsearch recursively */ }
```
- Same pattern as main search ✅

**In rootSearch (after doMove):**
```cpp
if (checkDrawRepAnd50(p, 2)) {
  value = VALUE_DRAW;
}
```
- Same pattern ✅

#### ✅ Twofold vs Threefold (CORRECT)
Using `numberOfRepetitions = 2` (twofold) is correct for search:
- We detect when position has been seen **once before**
- This is the current position making it the **second occurrence**
- Any further search is pointless as opponent can force draw

Threefold would require the position to be seen twice before, which is overly conservative.

#### ✅ Irreversible Move Detection (CORRECT)
The algorithm correctly detects when we've crossed an irreversible move:
- After capture/pawn move, half-move clock resets to 0
- Scanning backward: if we see a higher/equal value, we crossed a reset
- No need to scan further - no repetition can span that boundary

**Findings:**
1. ✅ 50-move rule correctly uses half-move clock >= 100
2. ✅ Repetition detection steps by 2 (same side to move)
3. ✅ Irreversible boundary detection is correct
4. ✅ Zobrist key comparison for position identity
5. ✅ Draw checks happen AFTER move is made
6. ✅ Twofold detection is appropriate for search
7. ✅ Early exit optimization is correct

**Changes Made:** None required

---

### Feature #7: Static Eval Integration
**Date:** 2026-02-21  
**Status:** ✅ Correct

**Implementation Location:** 
- `src/engine/Search.cpp` lines 1029-1036 (main search eval call)
- `src/engine/Search.cpp` lines 1710-1712 (qsearch eval call)
- `src/engine/Search.cpp` line 1877 (evaluate wrapper)
- `src/engine/Evaluator.cpp` (evaluation implementation)

**Theory Background:**
- Static evaluation provides positional assessment without further search
- Used for pruning decisions (razoring, RFP, futility, etc.)
- Must be called appropriately (not when in check)
- Value should be cached in TT to avoid redundant computation

**Checks:**

#### ✅ Eval Call in Main Search (CORRECT)
```cpp
const bool hasCheck = p.hasCheck();

if (!hasCheck && staticEval == VALUE_NONE) {
  staticEval = evaluate(p);
  if (SearchConfig.USE_TT && SearchConfig.USE_EVAL_TT) {
    storeTt(p, DEPTH_NONE, DEPTH_NONE, MOVE_NONE, VALUE_NONE, NONE, staticEval);
  }
}
```
- Only evaluates when NOT in check ✅
- Only evaluates if not already set from TT ✅
- Caches eval in TT for future use ✅

#### ✅ Eval Call in QSearch (CORRECT)
```cpp
if (!hasCheck) {
  if (staticEval == VALUE_NONE) {
    staticEval = evaluate(p);
  }
  // Stand-pat logic uses staticEval
}
```
- Only evaluates when NOT in check ✅
- Used for stand-pat decision ✅

#### ✅ PlyInfo Storage (CORRECT)
```cpp
plyStack[ply].staticEval = hasCheck ? VALUE_NONE : staticEval;
```
- Stores eval for "improving" flag computation ✅
- Stores VALUE_NONE when in check (no meaningful eval) ✅

#### ✅ Insufficient Material Check (CORRECT)
```cpp
// In Evaluator::evaluate():
if (p.checkInsufficientMaterial()) {
  return VALUE_DRAW;
}
```
- Returns draw immediately for insufficient material ✅
- Handles KvK, KBvK, KNvK, etc. ✅

#### ✅ Side-to-Move Normalization (CORRECT)
```cpp
inline Value Evaluator::finalEval(const Position& p, const Value value) {
  return value * p.getNextPlayer().sign();
}
```
- Evaluation computed from White's perspective ✅
- Normalized for side to move at the end ✅
- Positive = good for side to move ✅

#### ✅ Game Phase Interpolation (CORRECT)
```cpp
inline Value Evaluator::valueFromScore(const Score& score, const double gamePhaseFactor) {
  return score.midgame * gamePhaseFactor + score.endgame * (1.0 - gamePhaseFactor);
}
```
- Blends midgame and endgame scores ✅
- gamePhaseFactor: 1.0 = opening, 0.0 = endgame ✅

#### ✅ Lazy Eval (CORRECT)
```cpp
if (EvalConfig.USE_LAZY_EVAL) {
  const Value value = valueFromScore(score, gamePhaseFactor);
  if (value > static_cast<Value>(EvalConfig.LAZY_THRESHOLD + ...)) {
    return finalEval(p, value);
  }
}
```
- Early exit when position is clearly winning ✅
- Saves computation on one-sided positions ✅

#### ✅ Pawn Cache (CORRECT)
```cpp
if (EvalConfig.USE_PAWN_TT) {
  key = p.getPawnZobristKey();
  ep = pawnCache.getEntryPtr(key);
  if (key != 0 && ep->key == key) {
    s.midgame += ep->midvalue;
    s.endgame += ep->endvalue;
    return;
  }
}
```
- Caches expensive pawn structure evaluation ✅
- Uses separate Zobrist key for pawns ✅

**Findings:**
1. ✅ Eval only called when not in check
2. ✅ Eval cached in TT to avoid recomputation
3. ✅ Stored in PlyInfo for improving flag
4. ✅ Insufficient material → VALUE_DRAW
5. ✅ Side-to-move normalization correct
6. ✅ Game phase interpolation correct
7. ✅ Lazy eval for early exit
8. ✅ Pawn cache for expensive structure eval

**Changes Made:** None required

---

### Feature #8: Move Ordering (overall)
**Date:** 2026-02-21  
**Status:** ✅ Correct

**Implementation Location:** 
- `src/chesscore/MoveGenerator.cpp` lines 500-594 (fillOnDemandMoveList - phased generation)
- `src/chesscore/MoveGenerator.cpp` lines 596-642 (updateSortValues)
- `src/chesscore/MoveGenerator.cpp` lines 667-812 (generatePawnMoves)
- `src/chesscore/MoveGenerator.cpp` lines 813-858 (generateMoves)

**Theory Background:**
- Move ordering is critical for alpha-beta efficiency
- Good ordering: PV move → captures (MVV-LVA) → killers → quiet moves (history)
- Better ordering = more cutoffs = smaller search tree

**Checks:**

#### ✅ Move Generation Phases (CORRECT)
```cpp
enum onDemandStage {
  OD_NEW,
  PV_MOVE,           // 1. PV move first
  PAWN_CAPTURES,     // 2. Captures (pawns first)
  OFFICER_CAPTURES,  // 3. Captures (officers)
  KING_CAPTURES,     // 4. Captures (king)
  QUIET_SWITCH,
  PAWN_MOVES,        // 5. Quiet pawn moves
  CASTLING_MOVES,    // 6. Castling
  OFFICER_MOVES,     // 7. Quiet officer moves
  KING_MOVES,        // 8. Quiet king moves
  OD_END
};
```
- Phased generation avoids generating all moves when cutoff happens early ✅
- Captures before quiet moves ✅

#### ✅ Sort Value Priorities (CORRECT)
```cpp
// PV move
move->setValue(VALUE_MAX);  // Highest priority

// Killer 1
move->setValue(static_cast<Value>(1001));

// Killer 2
move->setValue(static_cast<Value>(1000));

// Captures: 2000 + MVV-LVA + positional
const Value value = 2000 + valueOf(captured) - valueOf(attacker) + posValue;

// Quiet moves: -2000 + positional + history
const Value value = posValue - 2000 + historyBonus;
```

**Priority order:**
1. PV move: VALUE_MAX (10000)
2. Captures: 2000 + (victim - attacker) → typically 2000-7000
3. Killers: 1000-1001
4. Counter moves: history + 500
5. Quiet moves with history: -2000 + history bonus
6. Other quiet moves: -2000 + positional value

✅ This is the correct ordering!

#### ✅ MVV-LVA for Captures (CORRECT)
```cpp
// Officer captures
const Value value = 2000 + valueOf(position.getPiece(toSquare)) - valueOf(position.getPiece(fromSquare)) + posValue;

// Pawn captures
const Value value = valueOf(position.getPiece(toSquare)) - valueOf(position.getPiece(fromSquare)) + posValue;
```
- MVV (Most Valuable Victim) is added positively ✅
- LVA (Least Valuable Attacker) is subtracted ✅
- QxP scores lower than PxQ ✅

#### ✅ Promotion Ordering (CORRECT)
```cpp
// Queen promotion: +5000 bonus
pMoves->push_back(Move::promotion(..., QUEEN, value + valueOf(QUEEN) + 5000));
// Knight promotion: +1500 bonus (can be useful for forks)
pMoves->push_back(Move::promotion(..., KNIGHT, value + valueOf(KNIGHT) + 1500));
// Rook/Bishop: -5000 (rarely useful underpromotions)
pMoves->push_back(Move::promotion(..., ROOK, value + valueOf(ROOK) - 5000));
pMoves->push_back(Move::promotion(..., BISHOP, value + valueOf(BISHOP) - 5000));
```
- Queen promotion prioritized ✅
- Knight promotion second (useful for forks) ✅
- Underpromotions deprioritized ✅

#### ✅ History Integration (CORRECT)
```cpp
const auto count = historyDataPtr->historyCount[us][move->from()][move->to()];
auto value = static_cast<Value>(count / 100);

// Counter Move bonus
if (historyDataPtr->counterMoves[lastMove.from()][lastMove.to()] == move->stripped()) {
  value = value + 500;
}

if (value > 0) {
  move->setValue(move->value() + value);
}
```
- History count improves sort value ✅
- Counter moves get +500 bonus ✅
- Only positive adjustments (don't penalize) ✅

#### ✅ Killer Move Handling (CORRECT)
```cpp
void MoveGenerator::storeKiller(const Move killerMove) {
  const Move m = killerMove.stripped();
  if (killerMoves[0] == m) {
    return;  // Don't duplicate
  }
  killerMoves[1] = killerMoves[0];  // Shift old killer
  killerMoves[0] = m;               // New killer first
}
```
- Two killer slots ✅
- Recent killer is first (higher priority: 1001 vs 1000) ✅
- No duplicates ✅

#### ✅ PV Move Duplicate Filtering (CORRECT)
```cpp
// In getNextPseudoLegalMove:
if (currentODStage != PAWN_CAPTURES && pvMovePush && 
    onDemandMoves[takeIndex].stripped() == pvMove.stripped()) {
  takeIndex++;  // Skip duplicate
  pvMovePush = false;
}
```
- PV move returned first, then skipped when it appears naturally ✅

**Findings:**
1. ✅ Phased generation is correct (captures before quiet)
2. ✅ PV move has highest priority
3. ✅ MVV-LVA capture ordering is correct
4. ✅ Killer moves ordered correctly (recent first)
5. ✅ History and counter-move bonuses applied
6. ✅ Promotion ordering is sensible
7. ✅ PV move deduplication works

**Changes Made:** None required

---

### Feature #9: History Heuristic
**Date:** 2026-02-21  
**Status:** ✅ Correct

**Implementation Location:** 
- `src/chesscore/History.h` lines 59-77 (History struct)
- `src/engine/Search.cpp` lines 1585, 1608-1610 (updates)
- `src/engine/Search.cpp` lines 1444-1452 (LMR adjustment)
- `src/chesscore/MoveGenerator.cpp` lines 612-640 (move ordering)

**Theory Background:**
- History heuristic tracks quiet moves that caused beta cutoffs
- Moves that frequently cause cutoffs are probably good and should be searched earlier
- Indexed by [color][from][to] - not piece-specific for simplicity
- Used for both move ordering and LMR reduction adjustments

**Checks:**

#### ✅ Data Structure (CORRECT)
```cpp
std::array<std::array<std::array<int, 64>, 64>, 2> historyCount{};
```
- Indexed by [color][from_square][to_square] ✅
- Uses `int` (signed) - allows negative values during penalty phase ✅
- Size: 2 * 64 * 64 * 4 = 32KB - reasonable ✅

#### ✅ History Bonus on Beta Cutoff (CORRECT)
```cpp
if (SearchConfig.USE_HISTORY_COUNTER && !p.isCapturingMove(move)) {
  history.historyCount[us][from][to] += 1L << depth;
}
```
- Only for quiet moves (`!isCapturingMove`) ✅ (was fixed in v1.3)
- Bonus scaled by depth (`1 << depth`) - deeper cutoffs weighted more ✅
- Applied only on beta cutoff ✅

#### ✅ History Penalty on Fail (CORRECT)
```cpp
// Decrease history only for quiet moves that failed to improve alpha
if (SearchConfig.USE_HISTORY_COUNTER && !p.isCapturingMove(move)) {
  history.historyCount[us][from][to] -= 1L << depth;
  if (history.historyCount[us][from][to] < 0) { history.historyCount[us][from][to] = 0; }
}
```
- Only for quiet moves ✅
- Same magnitude as bonus (`1 << depth`) ✅
- Clamped to 0 (no negative history) ✅
- Skips the move that raised alpha (line 1604: `continue`) ✅ (was fixed in v1.3)

#### ✅ Move Ordering Integration (CORRECT)
```cpp
const auto count = historyDataPtr->historyCount[us][move->from()][move->to()];
auto value = static_cast<Value>(count / 100);
if (value > 0) {
  move->setValue(move->value() + value);
}
```
- Scaled down by /100 for sort value ✅
- Only improves sort value (doesn't penalize) ✅

#### ✅ LMR History Adjustment (CORRECT)
```cpp
if (SearchConfig.USE_LMR_HISTORY) {
  const int histScore = history.historyCount[us][from][to];
  const int histReduction = -histScore / SearchConfig.LMR_HISTORY_DIVISOR;
  lmrDepth -= static_cast<Depth>(histReduction);
}
```
- Positive history → negative histReduction → less LMR reduction ✅
- Configurable divisor for tuning ✅

#### ✅ Reset Timing (CORRECT)
```cpp
// In newGame():
history.reset();
```
- Reset on new game ✅
- Accumulated within a game (persists across searches) ✅

**Findings:**
1. ✅ Data structure is appropriate [color][from][to]
2. ✅ Bonus only for quiet moves on beta cutoff
3. ✅ Penalty only for quiet moves that failed (not alpha-raising)
4. ✅ Depth-weighted updates (`1 << depth`)
5. ✅ Clamped to non-negative
6. ✅ Used for move ordering (scaled by /100)
7. ✅ Used for LMR adjustments
8. ✅ Reset on new game, persists within game

**Changes Made:** None required

---

### Feature #10: Killer Moves
**Date:** 2026-02-21  
**Status:** ✅ Correct

**Implementation Location:** 
- `src/chesscore/MoveGenerator.h` line 126 (storage)
- `src/chesscore/MoveGenerator.cpp` lines 169-176 (storeKiller)
- `src/chesscore/MoveGenerator.cpp` lines 608-611 (sort value)
- `src/engine/Search.cpp` line 1582 (store on cutoff)
- `src/engine/Search.cpp` lines 1362-1363 (pruning guard)
- `src/engine/PlyInfo.h` (per-ply MoveGenerator)

**Theory Background:**
- Killer moves are quiet moves that caused beta cutoffs at sibling nodes
- Stored per-ply (same depth level, different positions)
- If a move refuted one position, it might refute another at same ply
- Used to improve move ordering and protect from pruning

**Checks:**

#### ✅ Storage Structure (CORRECT)
```cpp
std::array<Move, 2> killerMoves = {MOVE_NONE, MOVE_NONE};
```
- Two killer slots (standard) ✅
- Per-MoveGenerator, which is per-ply (via PlyInfo) ✅
- Initialized to MOVE_NONE ✅

#### ✅ storeKiller Implementation (CORRECT)
```cpp
void MoveGenerator::storeKiller(const Move killerMove) {
  const Move m = killerMove.stripped();
  if (killerMoves[0] == m) {
    return;  // Already first killer, don't duplicate
  }
  killerMoves[1] = killerMoves[0];  // Demote old first to second
  killerMoves[0] = m;               // New move becomes first
}
```
- Strips sort value before storing ✅
- Avoids duplicates (if already first, skip) ✅
- FIFO-like: new killer pushes old one to slot 2 ✅
- Most recent killer in slot 0 ✅

#### ✅ Store on Beta Cutoff (CORRECT)
```cpp
if (SearchConfig.USE_KILLER_MOVES && !p.isCapturingMove(move)) {
  myMg->storeKiller(move);
}
```
- Only stored on beta cutoff ✅
- Only for quiet moves (`!isCapturingMove`) ✅
- Stored in the MoveGenerator for current ply ✅

#### ✅ Sort Value Assignment (CORRECT)
```cpp
} else if (move->stripped() == killerMoves[1]) { // Killer 2
  move->setValue(static_cast<Value>(1000));
} else if (move->stripped() == killerMoves[0]) { // Killer 1
  move->setValue(static_cast<Value>(1001));
}
```
- Killer 1 (most recent) has higher priority (1001) ✅
- Killer 2 has lower priority (1000) ✅
- Both below captures (~2000+) but above regular quiet moves (~-2000) ✅

#### ✅ Pruning Protection (CORRECT)
```cpp
if (nodeType != PvNode
    && extension == 0
    && move != ttMove
    && move != myMg->getKillerMoves()[0]
    && move != myMg->getKillerMoves()[1]
    // ... other conditions
) {
  // Futility pruning, LMP, etc.
}
```
- Killer moves protected from forward pruning ✅
- Both killer slots checked ✅

#### ✅ Per-Ply Scope (CORRECT)
```cpp
// In PlyInfo:
std::unique_ptr<MoveGenerator> mg;  // Each ply has its own MoveGenerator

// In Search:
std::array<PlyInfo, DEPTH_MAX + 1> plyStack{};
auto* const myMg = plyStack[ply].mg.get();
```
- Each ply has its own MoveGenerator ✅
- Killers are ply-specific (not shared across plies) ✅
- Persists across nodes at same ply (sibling positions) ✅

#### ✅ Reset Timing (CORRECT)
```cpp
// In MoveGenerator::reset():
killerMoves[0] = MOVE_NONE;
killerMoves[1] = MOVE_NONE;

// In PlyInfo::resetSearchState():
mg->reset();  // Clears killers
```
- Reset on newGame (via resetSearchState) ✅
- NOT reset between nodes at same ply (correct - killers should persist) ✅

**Findings:**
1. ✅ Two killer slots (standard practice)
2. ✅ FIFO replacement (newest first)
3. ✅ No duplicates
4. ✅ Only quiet moves stored
5. ✅ Stored only on beta cutoff
6. ✅ Higher sort value than regular quiet moves
7. ✅ Protected from pruning
8. ✅ Per-ply scope correct

**Changes Made:** None required

---

### Feature #11: Counter Moves
**Date:** 2026-02-21  
**Status:** ✅ Correct

**Implementation Location:** 
- `src/chesscore/History.h` line 68 (storage)
- `src/engine/Search.cpp` lines 1587-1589 (store on cutoff)
- `src/chesscore/MoveGenerator.cpp` lines 628-634 (move ordering)

**Theory Background:**
- Counter move heuristic: if move A was played and move B refuted it, store B as counter to A
- Next time opponent plays A, try B early - it worked before
- Indexed by [prev_from][prev_to] - the opponent's last move
- Simpler than killer moves (not ply-specific)

**Checks:**

#### ✅ Storage Structure (CORRECT)
```cpp
std::array<std::array<Move, 64>, 64> counterMoves{};
```
- Indexed by [from_square][to_square] of opponent's move ✅
- Stores a single Move per opponent move ✅
- Global (not per-ply) - counter to a move is position-independent ✅

#### ✅ Store on Beta Cutoff (CORRECT)
```cpp
if (SearchConfig.USE_HISTORY_MOVES) {
  const Move lastMove = p.getLastMove();
  if (lastMove != MOVE_NONE) {
    history.counterMoves[lastMove.from()][lastMove.to()] = move;
  }
}
```
- Stored on beta cutoff ✅
- Indexed by opponent's last move ✅
- Guards against MOVE_NONE ✅
- No `!isCapturingMove` guard - captures can also be counters ✅

#### ✅ Move Ordering Integration (CORRECT)
```cpp
if (historyDataPtr->counterMoves[p.getLastMove().from()][p.getLastMove().to()] == move->stripped()) {
  value = value + 500;
}
```
- Checks if current move is the counter to opponent's last move ✅
- Adds +500 bonus to sort value ✅
- Combined with history bonus ✅

#### ✅ Reset Timing (CORRECT)
```cpp
// In History::reset():
std::memset(counterMoves.data(), 0, sizeof(counterMoves));
```
- Reset on new game ✅
- Persists within game (like history) ✅

#### ⚠️ Note: No Guard for Captures
Unlike killer moves and history, counter moves are stored for ANY move that causes a cutoff (including captures). This is intentional:
- A capture that refutes an opponent move is still a good counter
- Counter moves are about refutation, not quiet move ordering

**Findings:**
1. ✅ Data structure: [from][to] indexed by opponent's move
2. ✅ Stored on beta cutoff (any move, not just quiet)
3. ✅ Guards against MOVE_NONE last move
4. ✅ +500 bonus in move ordering
5. ✅ Combined with history score
6. ✅ Reset on new game

**Changes Made:** None required

---

### Feature #12: Mate Distance Pruning
**Date:** 2026-02-21  
**Status:** ✅ Correct

**Implementation Location:**
- `src/engine/Search.cpp` lines 889-899 (main search)
- `src/engine/Search.cpp` lines 1662-1671 (qsearch)

**Theory Background:**
- If we already found a mate in N, we don't need to search for mate in N+1 or longer
- Tighten alpha/beta bounds based on current ply to prune hopeless branches
- Safe pruning - doesn't miss any shorter mates

**Checks:**

#### ✅ Alpha Bound Adjustment (CORRECT)
```cpp
alpha = std::max(alpha, -VALUE_CHECKMATE + static_cast<Value>(ply));
```
- The worst we can do at this ply is get mated immediately: `-VALUE_CHECKMATE + ply`
- If alpha is already better than that, keep alpha
- This is the "we're being mated" lower bound ✅

#### ✅ Beta Bound Adjustment (CORRECT)
```cpp
beta = std::min(beta, VALUE_CHECKMATE - static_cast<Value>(ply));
```
- The best we can do at this ply is mate the opponent immediately: `VALUE_CHECKMATE - ply`
- If beta is already worse than that, keep beta
- This is the "we're mating" upper bound ✅

#### ✅ Pruning Condition (CORRECT)
```cpp
if (alpha >= beta) {
  return alpha;
}
```
- If bounds cross, no valid scores exist in this range
- Return alpha (the tightened lower bound) ✅

#### ✅ Applied in Both Search and QSearch (CORRECT)
- Main search: lines 892-898
- QSearch: lines 1664-1670
- Same logic in both ✅

**Example Analysis:**
- If we found mate in 3 (value = VALUE_CHECKMATE - 6) at root
- At ply 4, beta would be tightened to `VALUE_CHECKMATE - 4`
- Since mate in 3 = VALUE_CHECKMATE - 6 < VALUE_CHECKMATE - 4, we keep searching
- But at ply 8, beta becomes VALUE_CHECKMATE - 8 < VALUE_CHECKMATE - 6
- So alpha >= beta triggers, pruning the search

**Findings:**
1. ✅ Alpha tightening (mated bound) correct
2. ✅ Beta tightening (mating bound) correct
3. ✅ Pruning condition correct
4. ✅ Present in both search and qsearch

**Changes Made:** None required

---

### Feature #13: Razoring
**Date:** 2026-02-21  
**Status:** ✅ Correct (was fixed in v1.3)

**Implementation Location:** 
- `src/engine/Search.cpp` lines 1069-1083

**Theory Background:**
- When static eval is far below alpha at low depth, skip normal search
- Jump directly to qsearch - unlikely to raise alpha with quiet moves
- Risk: might miss a non-capturing move that saves the position
- Only safe at shallow depths (typically depth 1-3)

**Checks:**

#### ✅ Guards (CORRECT)
```cpp
if (SearchConfig.USE_RAZORING
    && nodeType != PvNode  // Don't razor on PV - need complete line
    && depth == 1          // Only at depth 1 (frontier node)
    && staticEval != VALUE_NONE
    && staticEval <= alpha - SearchConfig.RAZOR_MARGIN) {
```
- `nodeType != PvNode` - Never razor on PV nodes ✅ (fixed 19.2.2026)
- `depth == 1` - Only at frontier nodes ✅
- `staticEval != VALUE_NONE` - Need valid eval ✅
- Margin check (531 cp default) ensures significantly below alpha ✅

#### ✅ Implicit !hasCheck Guard (CORRECT)
```cpp
// Earlier in search():
if (!hasCheck && staticEval == VALUE_NONE) {
  staticEval = evaluate(p);
}
```
- When in check, staticEval stays VALUE_NONE
- Razoring condition `staticEval != VALUE_NONE` fails
- Therefore razoring is implicitly disabled when in check ✅

#### ✅ QSearch Call (CORRECT)
```cpp
return qsearch(p, ply, alpha, beta, AllNode);
```
- Uses `AllNode` since we expect to fail low ✅ (fixed 19.2.2026)
- Passes current alpha/beta bounds ✅
- Returns qsearch result directly ✅

#### ✅ Margin Value (REASONABLE)
```cpp
int RAZOR_MARGIN = 531;  // ~5.3 pawns
```
- Large margin ensures we only razor when truly hopeless
- Typical values: 300-600 cp depending on engine
- 531 is reasonable ✅

**Findings:**
1. ✅ `nodeType != PvNode` guard present (fixed in v1.3)
2. ✅ Depth 1 condition correct
3. ✅ Margin-based threshold correct
4. ✅ Uses AllNode for qsearch (fixed in v1.3)
5. ✅ Check handling implicit (staticEval == VALUE_NONE when in check)
6. ✅ Reasonable margin value

**Changes Made:** None required (fixes already applied in v1.3)

---

## Known Bugs Already Fixed (v1.3)

For reference - these were found and fixed before this review:

1. **Razoring** - Used `PV` instead of `NonPV`, missing `!isPvNode` guard
2. **PVS first move** - Hardcoded `PV` instead of inheriting `isPvNode`
3. **LMR/PVS re-searches** - Hardcoded `PV` instead of inheriting `isPvNode`
4. **History penalty** - Penalized alpha-raising moves (best move got penalty)
5. **History on captures** - Applied history to captures (should be quiet only)

**Result:** PV node ratio: 20% → 0.02%, +109 ELO vs v1.1

---

## Reference Resources

- [Stockfish source](https://github.com/official-stockfish/Stockfish)
- [Chess Programming Wiki](https://www.chessprogramming.org/)
- [Ethereal source](https://github.com/AndyGrant/Ethereal)
- [Crafty source](https://craftychess.com/)

---

## Session Log

| Date       | Session | Work Done                                                      |
|------------|---------|----------------------------------------------------------------|
| 2026-02-21 | 1       | Created plan document                                          |
| 2026-02-21 | 1       | Reviewed Feature #1: Alpha-Beta / PVS ✅                        |
| 2026-02-21 | 1       | Reviewed Feature #2: Aspiration Windows ✅ (comment fixed)      |
| 2026-02-21 | 1       | Reviewed Feature #3: Quiescence Search ✅ (removed hist/killer) |
| 2026-02-21 | 1       | Reviewed Feature #4: TT Lookup & Cutoff ✅                      |
| 2026-02-21 | 1       | Reviewed Feature #5: TT Storage ✅                              |
| 2026-02-21 | 1       | Reviewed Feature #6: Draw Detection ✅                          |
| 2026-02-21 | 1       | Reviewed Feature #7: Static Eval ✅                             |
| 2026-02-21 | 1       | Reviewed Feature #8: Move Ordering ✅                           |
| 2026-02-21 | 1       | Reviewed Feature #9: History Heuristic ✅                       |
| 2026-02-21 | 1       | Reviewed Feature #10: Killer Moves ✅                           |
| 2026-02-21 | 1       | Reviewed Feature #11: Counter Moves ✅                          |
| 2026-02-21 | 1       | Reviewed Feature #12: Mate Distance Pruning ✅                  |
| 2026-02-21 | 1       | Reviewed Feature #13: Razoring ✅                               |
