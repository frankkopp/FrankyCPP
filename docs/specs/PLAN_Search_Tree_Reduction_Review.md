# FrankyCPP Search Tree Reduction Review Plan

**Document Version:** 1.0  
**Created:** 2026-02-18  
**Last Updated:** 2026-02-18  
**Status:** 🚀 NOT STARTED  
**Target:** FrankyCPP v1.4+  
**Priority:** High (Performance / Strength Improvement)

---

## Executive Summary

This plan documents a systematic review of FrankyCPP's search tree reduction techniques compared to Stockfish, with the goal of significantly reducing node counts and improving search efficiency.

### The Problem

In a 10-second analysis of the same position:

| Engine    | Depth | Total Nodes | Efficiency             |
|-----------|-------|-------------|------------------------|
| FrankyCPP | 13/30 | 41.5M       | Baseline               |
| Stockfish | 28/46 | 11.7M       | ~3.5x better per depth |

Stockfish reaches **2x the depth** while using **3.5x fewer nodes**. This massive efficiency gap comes from superior:
1. **Late Move Reductions (LMR)** - More aggressive, better tuned
2. **Move Ordering** - Better history heuristics, more cutoffs
3. **Pruning** - More aggressive conditions
4. **Extensions** - More selective application

### Goals

1. **Understand** current FrankyCPP implementation of each technique
2. **Compare** with modern Stockfish approaches
3. **Identify** config-only improvements (quick wins)
4. **Plan** code changes needed for significant improvements
5. **Test** changes with clear before/after metrics

---

## Phase 1: Late Move Reductions (LMR)

**Status:** 🔴 Not Started  
**Priority:** Highest - LMR has the largest impact on tree size

### 1.1 Current Implementation Analysis

**Location:** 
- `src/engine/Search.h` (lines ~206-218) - LMR table generation
- `src/engine/Search.cpp` (lines ~1325-1345) - LMR application

#### Current LMR Table Generation (Search.h)
```cpp
// Hardcoded linear formula - NOT configurable!
static constexpr int lmr_reduction(const int depth, const int movesSearched) {
  // 1 + round(depth * movesSearched * 0.0035)
  // exact integer rounding of 35/10000
  return 1 + (depth * movesSearched * 35 + 5000) / 10000;
}
```

**⚠️ Key Finding:** The LMR factor (0.0035) is **hardcoded** in `Search.h`, not configurable!

#### Current LMR Conditions (from Search.cpp ~line 1325)
LMR is applied when:
- `SearchConfig.USE_LMR` is true
- `!matethreat`
- `depth >= SearchConfig.LMR_MIN_DEPTH` (configurable, default: 3)
- `movesSearched >= SearchConfig.LMR_MIN_MOVES` (configurable, default: 3)
- `!isPv` - **NOT applied on PV nodes**
- `!givesCheck`
- Not a capturing move
- Not a promotion

#### Current LMR Re-search Logic
If reduced search returns value > alpha:
- Re-search at full depth with null window
- If still > alpha and isPV: full window re-search

### 1.2 Stockfish Comparison

#### Stockfish LMR Formula
```cpp
// Stockfish uses logarithmic formula:
Reductions[d][m] = std::log(d) * std::log(m) / baseFactor;
```

This grows **logarithmically** rather than linearly, providing:
- Less aggressive reductions at shallow depths
- Gradually increasing reductions for late moves at high depth

#### Stockfish LMR Adjustments (Missing in FrankyCPP)
1. **PV vs non-PV**: Reduce less on PV nodes
2. **Improving flag**: Reduce more if eval is NOT improving (vs 2 plies ago)
3. **History score**: Reduce more for moves with bad history, less for good history
4. **Cut nodes**: Reduce more aggressively on expected cut nodes
5. **Singular move bonus**: Reduce less if move is near-singular
6. **Threat/pawn push**: Reduce less for pawn pushes to 6th/7th rank

### 1.3 Configuration Review

**Current LMR Config Options:**

| Parameter       | Current Default  | Description               | Tunable?            |
|-----------------|------------------|---------------------------|---------------------|
| `USE_LMR`       | true             | Enable LMR                | Yes                 |
| `LMR_MIN_DEPTH` | 3                | Minimum depth for LMR     | Yes                 |
| `LMR_MIN_MOVES` | 3                | Moves before LMR kicks in | Yes                 |
| LMR Formula     | `1 + d*m*0.0035` | Reduction factor          | **NO - Hardcoded!** |

**⚠️ Critical Gap:** The LMR formula factor is hardcoded in `Search.h`. To tune it, we need to either:
1. Make it configurable via `SearchConfigData`
2. Switch to a logarithmic formula (Stockfish approach)

**Missing Config Options to Consider:**
- `LMR_BASE_FACTOR` - Make the 0.0035 factor configurable
- `LMR_PV_REDUCTION_DIVISOR` - Reduce less on PV nodes (currently skipped entirely)
- `LMR_IMPROVING_BONUS` - Less reduction when improving
- `LMR_HISTORY_DIVISOR` - History-based adjustment divisor
- `LMR_CUTNODE_EXTRA` - Extra reduction on cut nodes

### 1.4 Action Items

**⚠️ Each change requires a config flag for SearchTreeSizeTest validation!**

#### Config-Only Quick Wins
- [ ] **Task 1.4.1**: Test lower `LMR_MIN_MOVES` (2 instead of 3)
- [ ] **Task 1.4.2**: Test lower `LMR_MIN_DEPTH` (2 instead of 3)

#### Code Changes Required

**Change 1.4.3: Make LMR Factor Configurable** (Prerequisite for tuning)
- **Config Flag:** `LMR_FACTOR` (double, default 0.0035)
```cpp
// In SearchConfigData.h:
double LMR_FACTOR = 0.0035;

// In Search.h - compute at runtime or regenerate table on config change
```
- **Effort:** Low-Medium
- **Impact:** Enables tuning experiments
- **Risk:** Low
- **SearchTreeSizeTest:** Compare different factor values

**Change 1.4.4: Switch to Logarithmic LMR Formula** ⭐ HIGH IMPACT
- **Config Flag:** `LMR_USE_LOG_FORMULA` (bool, default false initially)
```cpp
// Current linear formula:
lmrReduction[d][m] = 1 + d * m * factor;

// Proposed logarithmic formula:
lmrReduction[d][m] = std::max(1, int(std::log(d) * std::log(m) / baseFactor));
```
- **Effort:** Low (table generation change only)
- **Impact:** Medium-High
- **Risk:** Low (easy to test/revert)
- **SearchTreeSizeTest:** Run with flag on/off, compare node counts

**Change 1.4.5: Add "Improving" Flag** ⭐⭐ HIGH IMPACT
- **Config Flag:** `USE_LMR_IMPROVING` (bool, default false initially)
```cpp
// staticEval is already tracked in PlyInfo! Just need to use it:
// Track if static eval improved vs 2 plies ago
bool improving = (ply >= 2 && plyInfo[ply].staticEval > plyInfo[ply-2].staticEval);

// Apply more reduction when NOT improving
if (!improving) reduction += 1;
```
- **Effort:** LOW (PlyInfo already has staticEval!)
- **Impact:** High - Stockfish considers this essential
- **Risk:** Low
- **SearchTreeSizeTest:** Add after "65 LMR" entry

**Change 1.4.6: History-Based LMR Adjustment** ⭐⭐ HIGH IMPACT
- **Config Flag:** `USE_LMR_HISTORY` (bool, default false initially)
```cpp
// Adjust reduction based on history score
int historyReduction = -history.getQuietMoveScore(...) / HISTORY_LMR_DIVISOR;
reduction += historyReduction;
```
- **Effort:** Medium (need to integrate history into LMR)
- **Impact:** High
- **Risk:** Low
- **SearchTreeSizeTest:** Add after LMR improving entry

**Change 1.4.7: PV Node Reduction Adjustment**
- **Config Flag:** `USE_LMR_PV_REDUCE` (bool, default false initially)
```cpp
// Apply smaller reduction on PV nodes (currently skipped entirely)
if (isPV) reduction = reduction * 2 / 3;  // or similar
```
- **Effort:** Low
- **Impact:** Medium
- **Risk:** Low
- **SearchTreeSizeTest:** Measure impact on PV node handling

**Change 1.4.8: Cut Node Extra Reduction**
- **Config Flag:** `USE_LMR_CUTNODE` (bool, default false initially)
```cpp
// Reduce more on expected cut nodes
if (cutNode) reduction += 2;
```
- **Effort:** Low (need to track cutNode flag)
- **Impact:** Medium
- **Risk:** Low
- **SearchTreeSizeTest:** Add after LMR history entry

### 1.5 Validation Approach

1. **Benchmark Suite**: Run fixed-depth searches on EPD test positions
2. **Metrics to Track**:
   - Nodes per depth level
   - Time to depth N
   - Node count reduction percentage
   - Search quality (best move agreement with longer searches)
3. **A/B Testing**: Compare with/without each change
4. **Regression Testing**: Ensure no strength loss via self-play or SPRT tests

---

## Phase 2: Move Ordering Quality

**Status:** 🔴 Not Started  
**Priority:** High - Better ordering = more cutoffs = exponentially fewer nodes

### 2.1 Current Implementation Analysis

**Location:** 
- `src/chesscore/History.h` - History struct definition
- `src/chesscore/MoveGenerator.cpp` - `updateSortValues()`
- `src/engine/Search.cpp` - Killer moves, history updates

#### Current Move Ordering Priority
1. TT move (hash move)
2. Good captures (MVV-LVA + SEE positive)
3. Killer moves (2 per ply)
4. Counter move
5. History heuristic (quiet moves)
6. Bad captures (SEE negative)

#### Current History Implementation (from History.h)
```cpp
struct History {
  // History heuristic: [color][from_square][to_square]
  int historyCount[2][64][64]{};
  
  // Counter move: [prev_from][prev_to] -> refutation move
  Move counterMoves[64][64]{};
};
```
- Single history table indexed by from/to squares
- Counter-move tracking exists ✅
- Aging: divided by 2 periodically

### 2.2 Stockfish Comparison

#### Stockfish History Tables (Multiple!)
1. **Main history**: `history[color][from][to]` ✅ Have this
2. **Capture history**: `captureHistory[piece][to][capturedType]` ❌ Missing
3. **Continuation history**: `contHistory[piece1][to1][piece2][to2]` ❌ Missing
4. **Counter-move history**: What reply worked after opponent's move ❌ Partial

#### Stockfish Killer Moves
- 2-3 killers per ply (FrankyCPP has 2) ✅

#### Stockfish Capture Ordering
- MVV-LVA base ✅ Have this
- SEE for bad capture detection ✅ Have this
- Capture history bonus ❌ Missing

### 2.3 Action Items

#### Config-Only Quick Wins
- [ ] **Task 2.3.1**: Tune history aging factor
- [ ] **Task 2.3.2**: Tune killer move bonus values

#### Code Changes Required

**Change 2.3.3: Add Capture History Table**
```cpp
// Track which captures are good/bad
int16_t captureHistory[PIECE_TYPE_NB][SQUARE_NB][PIECE_TYPE_NB];
```
- **Effort:** Medium
- **Impact:** Medium

**Change 2.3.4: Add Continuation History**
```cpp
// Track 2-ply sequences that work well
int16_t contHistory[PIECE_NB][SQUARE_NB][PIECE_NB][SQUARE_NB];
```
- **Effort:** High (significant memory, complex update logic)
- **Impact:** High

**Change 2.3.5: Improve Counter-Move Tracking**
- Ensure counter-move gets significant bonus in ordering
- Track counter-move history (not just the move)

---

## Phase 3: Pruning Techniques

**Status:** 🔴 Not Started  
**Priority:** High

### 3.1 Current Implementation Analysis

**Existing Pruning in FrankyCPP:**

| Technique                      | Implemented | Location             | Configurable |
|--------------------------------|-------------|----------------------|--------------|
| Razoring                       | ✅ Yes       | Search.cpp:1006-1014 | Yes          |
| Reverse Futility Pruning (RFP) | ✅ Yes       | Search.cpp:1017-1031 | Yes          |
| Null Move Pruning (NMP)        | ✅ Yes       | Search.cpp:1043-1119 | Yes          |
| Futility Pruning               | ✅ Yes       | Search.cpp:1296-1306 | Yes          |
| Late Move Pruning (LMP)        | ✅ Yes       | Search.cpp:1308-1313 | Yes          |
| SEE Pruning                    | ✅ Partial   | Via bad captures     | Partial      |
| Delta Pruning (QS)             | ✅ Yes       | Quiescence           | Yes          |

### 3.2 Stockfish Comparison

#### Razoring
- Stockfish: Drop to qsearch if eval + margin < alpha at low depth
- FrankyCPP: Similar ✅

#### Futility Pruning
- Stockfish: More aggressive margins, applies at higher depths
- FrankyCPP: Check margins and depth limits

#### Late Move Pruning (LMP)
- Stockfish: Prune quiet moves entirely after N moves at low depth
- FrankyCPP: Similar, check thresholds

#### SEE Pruning
- Stockfish: Prune moves with bad SEE at all depths (scaled by depth)
- FrankyCPP: Only skip bad captures in ordering ❌ Could be more aggressive

### 3.3 Action Items

#### Config-Only Quick Wins
- [ ] **Task 3.3.1**: Tune RFP margins (test more aggressive)
- [ ] **Task 3.3.2**: Tune LMP move counts (test lower thresholds)
- [ ] **Task 3.3.3**: Tune futility margins

#### Code Changes Required

**Change 3.3.4: Add SEE-based Pruning for All Moves**
```cpp
// Prune moves with very negative SEE at low depths
if (depth <= SEE_PRUNE_DEPTH && !isCapture) {
  int seeThreshold = -SEE_PRUNE_MARGIN * depth;
  if (see(move) < seeThreshold) continue;  // skip this move
}
```
- **Effort:** Low-Medium
- **Impact:** Medium

**Change 3.3.5: History-based Pruning**
```cpp
// Prune quiet moves with very bad history at low depths
if (depth <= HIST_PRUNE_DEPTH && historyScore < HIST_PRUNE_THRESHOLD) {
  continue;
}
```
- **Effort:** Low
- **Impact:** Medium

---

## Phase 4: Extensions

**Status:** 🔴 Not Started  
**Priority:** Medium

### 4.1 Current Implementation Analysis

**Location:** `src/engine/Search.cpp` (lines ~1193-1266)

| Extension             | Implemented | Condition                 |
|-----------------------|-------------|---------------------------|
| Check Extension       | ✅ Yes       | When in check             |
| Singular Extension    | ✅ Yes       | When one move much better |
| Promotion Extension   | ❓ Check     | Pawn to 7th rank?         |
| Recapture Extension   | ❌ No        | -                         |
| Passed Pawn Extension | ❌ No        | -                         |

### 4.2 Stockfish Comparison

- **Singular Extensions**: More refined criteria, uses "improving" flag
- **Check Extensions**: Only when SEE >= 0 (not losing check)
- **Extensions capped**: Total extensions limited per search path

### 4.3 Action Items

- [ ] **Task 4.3.1**: Review singular extension threshold tuning
- [ ] **Task 4.3.2**: Add SEE filter to check extensions
- [ ] **Task 4.3.3**: Consider extension limit per path

---

## Phase 5: Internal Iterative Reductions (IIR)

**Status:** 🔴 Not Started  
**Priority:** Medium

### 5.1 Current Implementation Analysis

FrankyCPP currently uses **IID** (Internal Iterative Deepening):
- When no TT move at high depth
- Do a reduced search to find a good first move

**Location:** Search.cpp (check for IID implementation)

### 5.2 Stockfish Approach: IIR

Modern Stockfish replaced IID with simpler **IIR**:
```cpp
// If no TT move, reduce depth by 1 instead of doing a mini-search
if (!ttMove && depth >= IIR_DEPTH) {
  depth -= 1;  // Simple! Just search 1 ply less
}
```

**Benefits of IIR over IID:**
- Simpler code
- No recursive mini-search overhead
- Effective in practice

### 5.3 Action Items

- [ ] **Task 5.3.1**: Evaluate replacing IID with IIR
- [ ] **Task 5.3.2**: If IIR adopted, test depth threshold

---

## Implementation Order & Priority

### Implementation Workflow (Per Change)

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Add config flag to SearchConfigData.h                    │
│ 2. Implement feature (gated by flag, default OFF)           │
│ 3. Add to SearchTreeSizeTest.cpp                            │
│ 4. Run SearchTreeSizeTest → verify node reduction           │
│    └─ If no reduction → investigate, don't proceed          │
│ 5. Check NPS impact (same test output)                      │
│ 6. Enable flag by default, run test suites                  │
│ 7. Run Arena matches for strength validation                │
│ 8. Document results, update plan status                     │
└─────────────────────────────────────────────────────────────┘
```

### Quick Wins (Config-only, Test First)
1. LMR factor tuning
2. LMR min moves/depth tuning
3. Pruning margin tuning

### Phase 1 Code Changes (Highest Impact)
1. ⭐⭐⭐ Add "improving" flag tracking
2. ⭐⭐⭐ Switch to logarithmic LMR formula
3. ⭐⭐ History-based LMR adjustments
4. ⭐⭐ PV node LMR reduction

### Phase 2 Code Changes (High Impact)
5. Cut node extra reduction
6. SEE-based move pruning
7. Capture history table

### Phase 3 Code Changes (Medium Impact)
8. Continuation history
9. IIR replacement
10. Extension refinements

---

## Validation & Testing Strategy

### Three-Stage Validation Process

Every change must pass through these validation stages in order:

#### Stage 1: SearchTreeSizeTest (Node Count Verification) ⭐ REQUIRED FIRST
**Location:** `src/enginetest/SearchTreeSizeTest.cpp`

This test framework systematically enables/disables features to measure their impact on node counts. **Every new feature MUST have a config flag** that can be toggled in SearchTreeSizeTest.

**Process:**
1. Add feature flag to `SearchConfigData.h` (e.g., `USE_LMR_IMPROVING`)
2. Add to SearchTreeSizeTest.cpp with before/after measurement
3. Run test to confirm node reduction
4. If nodes increase or stay same → investigate before proceeding

**Example for "improving" flag:**
```cpp
// In SearchTreeSizeTest.cpp featureMeasurements():
CONFIG_OVERRIDE(s.USE_LMR = true;);
result.tests.push_back(measureTreeSize(..., "65 LMR"));

CONFIG_OVERRIDE(s.USE_LMR_IMPROVING = true;);  // NEW FLAG
result.tests.push_back(measureTreeSize(..., "66 LMR+Impr"));
```

**Success Criteria:** Node count should decrease (or NPS increase for same depth)

#### Stage 2: NPS & Depth Benchmarking
**Also in SearchTreeSizeTest output**

After confirming node reduction, verify:
- NPS (nodes per second) doesn't decrease significantly
- Same depth is reached faster, OR deeper depth in same time
- Search score stability (best move doesn't change erratically)

**Metrics tracked:**
- `nodes` - Total nodes searched
- `nps` - Nodes per second
- `depth` / `extra` - Depth reached
- `time` - Time taken

#### Stage 3: Strength Testing (Arena Framework)
**External validation via Arena GUI**

Only after Stage 1 & 2 pass:
1. **Test Suites:** WAC, STS, Eigenmann tests
2. **Self-Play Matches:** Modified vs baseline (100+ games)
3. **SPRT Testing:** Statistical significance via cutechess-cli

**Success Criteria:** 
- Test suite score same or better
- No Elo regression in matches (confidence > 95%)

---

### Feature Flag Requirements

**⚠️ CRITICAL:** Every search tree reduction change MUST be gated by a config flag.

| New Feature     | Required Config Flag  | SearchTreeSizeTest Entry |
|-----------------|-----------------------|--------------------------|
| LMR Improving   | `USE_LMR_IMPROVING`   | After "65 LMR"           |
| LMR Log Formula | `LMR_USE_LOG_FORMULA` | Compare with linear      |
| LMR History     | `USE_LMR_HISTORY`     | After LMR base           |
| LMR Cut Node    | `USE_LMR_CUTNODE`     | After LMR base           |
| Capture History | `USE_CAPTURE_HISTORY` | After History            |
| SEE Pruning     | `USE_SEE_PRUNING`     | After SEE                |

This ensures:
1. Easy A/B testing
2. Quick rollback if issues found
3. Incremental validation
4. Clear documentation of what each flag controls

---

### Benchmark Positions
Use standard EPD test suites:
- WAC (Win At Chess) - 300 positions
- STS (Strategic Test Suite) - 1500 positions
- Custom positions from engine testing

### Metrics to Track

```
Before/After comparison for each change:
- Nodes at depth 10, 15, 20
- Time to reach depth 15, 20
- NPS (nodes per second) - should stay similar
- Search score stability
- Best move agreement %
```

### Self-Play Testing
- Use cutechess-cli for SPRT testing
- Compare modified vs baseline version
- Ensure no strength regression

### Continuous Validation
- Run benchmarks after each change
- Keep log of all experiments
- Easy rollback if change hurts

---

## Appendix A: Current LMR Config Options

From `SearchConfigData.h`:

```cpp
// LMR/LMP
bool USE_LMR      = true;
int LMR_MIN_DEPTH = 3;
int LMR_MIN_MOVES = 3;
```

From `Search.h` (hardcoded):

```cpp
// LMR reduction table - HARDCODED formula
static constexpr int lmr_reduction(const int depth, const int movesSearched) {
  // 1 + round(depth * movesSearched * 0.0035)
  return 1 + (depth * movesSearched * 35 + 5000) / 10000;
}
```

## Appendix B: Stockfish LMR Reference

Key files in Stockfish source:
- `search.cpp` - Main LMR logic
- `movepick.cpp` - Move ordering with history
- `search.h` - Reduction tables

Stockfish LMR reduction (simplified):
```cpp
int reduction(int d, int m, bool improving, int history) {
  int r = Reductions[d][m];
  if (!improving) r += 1;
  r -= history / 8192;  // Good history = less reduction
  return std::max(0, r);
}
```

---

## Change Log

| Version | Date       | Changes                       |
|---------|------------|-------------------------------|
| 1.0     | 2026-02-18 | Initial plan document created |

---

*Document created as part of FrankyCPP search efficiency improvement initiative*
