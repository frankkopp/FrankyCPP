# FrankyCPP "Improving" Flag Implementation Plan

**Document Version:** 1.4  
**Created:** 2026-02-20  
**Last Updated:** 2026-02-20  
**Status:** 🟡 IN PROGRESS (Phase 2 Complete & Verified)  
**Target:** FrankyCPP v1.4+  
**Priority:** High (Identified as ⭐⭐ HIGH IMPACT in Search Tree Reduction Review)  
**Related:** `PLAN_Search_Tree_Reduction_Review.md` (Change 1.4.5), `V1_ENGINE_STRENGTH_ROADMAP.md`

---

## Executive Summary

The **"improving"** flag tracks whether the static evaluation of the current position is better than it was 2 plies ago (i.e., whether our last move improved our position). This is a well-proven, low-risk technique used by virtually all modern engines (Stockfish, Ethereal, Koivisto) to modulate pruning aggressiveness:

- **Improving = true** → Position got better → be more cautious with pruning (search deeper)
- **Improving = false** → Position didn't improve → prune more aggressively (search less)

**Key Finding:** `PlyInfo` already has a `staticEval` field, but it is **not populated** — the search method uses a local `staticEval` variable instead. The core work is to store the eval in `PlyInfo` and then use the 2-ply comparison to modulate existing pruning techniques.

### Expected Impact

| Application                      | ELO Estimate | Risk    | Effort         |
|----------------------------------|--------------|---------|----------------|
| LMR adjustment                   | +5–15        | Low     | Low            |
| NMP adjustment                   | +2–5         | Low     | Low            |
| Futility Pruning margins         | +2–5         | Low     | Low            |
| Reverse Futility Pruning margins | +1–3         | Low     | Low            |
| Late Move Pruning thresholds     | +1–3         | Low     | Low            |
| **Combined**                     | **+10–25**   | **Low** | **Low–Medium** |

---

## Current State Analysis

### PlyInfo Already Has staticEval (But Unused)

From `src/engine/PlyInfo.h`:
```cpp
struct PlyInfo {
  // ...
  Value staticEval{VALUE_NONE};  // Static evaluation at this ply
  // ...
};
```

However, in `Search.cpp::search()`, the evaluation is stored in a **local variable**:
```cpp
Value staticEval = VALUE_NONE;  // line ~892 — local, NOT stored in PlyInfo
```

The `plyStack[ply]` is referenced (line ~1183) only for MoveGenerator access. The `PlyInfo::staticEval` is never written to or read from during search.

### Pruning Techniques That Would Benefit

All of these already use `staticEval` locally and can trivially incorporate the improving flag:

| Technique                          | Location (Search.cpp) | Current Use of staticEval                         |
|------------------------------------|-----------------------|---------------------------------------------------|
| **Razoring**                       | ~line 1024–1036       | `staticEval <= alpha - RAZOR_MARGIN`              |
| **Reverse Futility Pruning (RFP)** | ~line 1040–1052       | `staticEval - margin >= beta`                     |
| **Null Move Pruning (NMP)**        | ~line 1065–1140       | Reduction formula (could add `!improving` bonus)  |
| **Futility Pruning (FP)**          | ~line 1320–1330       | `staticEval + moveGain + futilityMargin <= alpha` |
| **Late Move Pruning (LMP)**        | ~line 1335–1340       | Move count threshold (could vary with improving)  |
| **Late Move Reductions (LMR)**     | ~line 1345–1365       | Reduction table lookup (primary target)           |

---

## Implementation Plan

### Phase 1: Foundation — Store staticEval in PlyInfo & Compute Improving ✅ COMPLETE

**Effort:** Low (1–2 hours)  
**Files:** `Search.cpp`, `SearchStats.h`, `SearchConfigData.h`, `ConfigRegistry.cpp`, `search.yaml`  
**Status:** ✅ Implemented & Verified

**Verified Results** (10s search, position `5k2/1rn2p2/3pb1p1/7p/p3PP2/PnNBK2P/3N2P1/1R6 w - -`):
- Improving True: 2,718,944 (59.9%) — within expected 40–65% range ✅
- Improving False: 1,819,770 (40.1%)
- Search Nodes: 5,817,632 (32.6%) / QSearch Nodes: 12,032,233 (67.4%)
- `pvNodes + nonPvNodes == searchNodes + qsearchNodes` invariant holds ✅
- Fixed pre-existing pvNodes/nonPvNodes double-count on qsearch drop-through

**Additional work done in Phase 1:**
- Added `searchNodes` / `qsearchNodes` counters for clean search vs qsearch split
- Fixed `pvNodes`/`nonPvNodes` double-counting (search→qsearch drop-through no longer counted twice)
- Added `USE_IMPROVING` config flag, registry entry, and YAML entry

#### Step 1.1: Store staticEval in PlyInfo

After computing `staticEval` in `search()`, store it in `plyStack`:

```cpp
// After existing staticEval computation (~line 1020):
plyStack[ply].staticEval = staticEval;

// When in check (no eval available), set to VALUE_NONE:
if (hasCheck) {
  plyStack[ply].staticEval = VALUE_NONE;
}
```

#### Step 1.2: Compute the Improving Flag

After storing `staticEval`, compute the improving flag:

```cpp
// Compute "improving" — is our position better than 2 plies ago?
// Edge cases:
//   - ply < 2: no data → default false (conservative start)
//   - in check now: no eval → improving = false 
//   - in check 2 plies ago (VALUE_NONE): fall back to ply-4, else assume true
const bool improving = [&]() {
  if (hasCheck || staticEval == VALUE_NONE) return false;
  if (ply < 2) return false;
  const Value prevEval = plyStack[ply - 2].staticEval;
  if (prevEval != VALUE_NONE) return staticEval > prevEval;
  // 2 plies ago was in check — try 4 plies ago
  if (ply >= 4) {
    const Value prevEval4 = plyStack[ply - 4].staticEval;
    if (prevEval4 != VALUE_NONE) return staticEval > prevEval4;
  }
  return true; // Conservative: assume improving when no data
}();
```

**Note:** This goes in `search()` only (ply > 0). Root search (`rootSearch()`) always uses ply 0, so improving doesn't apply there.

#### Step 1.3: Add Statistics Tracking

In `SearchStats.h`, add:

```cpp
uint64_t improvingTrue = 0;   // Nodes where improving == true
uint64_t improvingFalse = 0;  // Nodes where improving == false
```

And track in the search:
```cpp
if (improving) statistics.improvingTrue++;
else statistics.improvingFalse++;
```

Expected ratio: ~40–60% improving in typical middlegame positions.

---

### Phase 2: Apply to LMR (Primary Target) ✅ COMPLETE & VERIFIED

**Effort:** Low (30 min)  
**Config Flag:** `USE_LMR_IMPROVING` (bool, default true)  
**Config Parameter:** `LMR_IMPROVING_REDUCTION` (int, default 1)

#### Implementation

In the LMR section of `search()` (~line 1345–1365):

```cpp
if (SearchConfig.USE_LMR
    && depth >= SearchConfig.LMR_MIN_DEPTH
    && movesSearched >= SearchConfig.LMR_MIN_MOVES
    && !isPvNode
    && !givesCheck
    && !p.isCapturingMove(move)
    && move.type() != PROMOTION
    && !matethreat) {
  const int d = std::min(depth, Depth{31});
  const int m = std::min(movesSearched, 63);
  lmrDepth -= static_cast<Depth>(LMR_REDUCTION[d][m]);

  // NEW: Reduce more when NOT improving
  if (SearchConfig.USE_LMR_IMPROVING && !improving) {
    lmrDepth -= static_cast<Depth>(SearchConfig.LMR_IMPROVING_REDUCTION);
  }

  lmrDepth = std::max(lmrDepth, DEPTH_NONE);
  statistics.lmrReductions++;
}
```

#### SearchTreeSizeTest Entry

```cpp
CONFIG_OVERRIDE(s.USE_LMR_IMPROVING = true;);
result.tests.push_back(measureTreeSize(..., "66 LMR+Improving"));
```

#### Validation

1. Run SearchTreeSizeTest — expect node count reduction (more aggressive pruning when not improving)
2. Run test suites (WAC, STS) — expect neutral to slight improvement
3. Self-play: 200+ games vs baseline

---

### Phase 3: Apply to Null Move Pruning

**Effort:** Low (30 min)  
**Config Flag:** `USE_NMP_IMPROVING` (bool, default true)  
**Config Parameter:** `NMP_IMPROVING_REDUCTION` (int, default 1)

#### Implementation

In the NMP section (~line 1090):

```cpp
// Determine depth reduction
auto r = Depth{SearchConfig.NMP_REDUCTION};
if (depth > 8 || (depth > 6 && p.getGamePhase() >= 3)) { ++r; }

// NEW: Increase NMP reduction when not improving
if (SearchConfig.USE_NMP_IMPROVING && !improving) {
  r += static_cast<Depth>(SearchConfig.NMP_IMPROVING_REDUCTION);
}

Depth newDepth = depth - r - 1;
if (newDepth < 0) { newDepth = DEPTH_NONE; }
```

**Rationale:** If our position isn't improving, we can be more confident that a null move will also fail high — the opponent likely can't improve either. More aggressive NMP is safe here.

---

### Phase 4: Apply to Futility Pruning / RFP

**Effort:** Low (30 min)  
**Config Flag:** `USE_FP_IMPROVING` (bool, default true)  
**Config Parameter:** `FP_IMPROVING_MARGIN` (int, default 80) — additional margin when not improving

#### Futility Pruning Implementation

In the FP section (~line 1320):

```cpp
if (SearchConfig.USE_FP && depth < 7) {
  auto futilityMargin = SearchConfig.FP_MARGIN[depth];
  
  // NEW: When not improving, reduce the margin (prune more aggressively)
  if (SearchConfig.USE_FP_IMPROVING && !improving) {
    futilityMargin -= SearchConfig.FP_IMPROVING_MARGIN;
  }
  
  if (staticEval + moveGain + futilityMargin <= alpha) {
    // ... existing pruning logic
  }
}
```

#### Reverse Futility Pruning Implementation

In the RFP section (~line 1044):

```cpp
if (SearchConfig.USE_RFP && doNull && depth <= 3 && !isPvNode && !hasCheck) {
  auto margin = Value{SearchConfig.RFP_MARGIN[depth]};
  
  // NEW: When not improving, reduce the margin (prune more aggressively)
  if (SearchConfig.USE_RFP_IMPROVING && !improving) {
    margin -= Value{SearchConfig.RFP_IMPROVING_MARGIN};
  }
  
  if (staticEval - margin >= beta) {
    statistics.rfp_cuts++;
    return staticEval - margin;
  }
}
```

**Config Flags:**
- `USE_RFP_IMPROVING` (bool, default true)
- `RFP_IMPROVING_MARGIN` (int, default 60) — margin reduction in centipawns

---

### Phase 5: Apply to Late Move Pruning

**Effort:** Low (15 min)  
**Config Flag:** `USE_LMP_IMPROVING` (bool, default true)

#### Implementation

In the LMP section (~line 1335):

```cpp
if (SearchConfig.USE_LMP) {
  const int lmpDepth = depth > 15 ? 15 : depth;
  int lmpThreshold = SearchConfig.LMP_MOVES[lmpDepth];
  
  // NEW: When improving, allow searching more moves (be less aggressive)
  if (SearchConfig.USE_LMP_IMPROVING && improving) {
    lmpThreshold += lmpThreshold / 2;  // 50% more moves when improving
  }
  
  if (movesSearched >= lmpThreshold) {
    statistics.lmpCuts++;
    continue;
  }
}
```

**Alternative:** Divide LMP threshold by 2 when NOT improving (Stockfish approach). Either way, the effect is: improving = search more moves, not improving = prune earlier.

---

## Configuration Summary

### New Config Parameters (SearchConfigData.h)

```cpp
// Improving flag
bool USE_IMPROVING             = true;  // Master switch for improving flag

// LMR + Improving
bool USE_LMR_IMPROVING         = true;  // Use improving flag in LMR
int LMR_IMPROVING_REDUCTION    = 1;     // Extra reduction when not improving

// NMP + Improving  
bool USE_NMP_IMPROVING         = true;  // Use improving flag in NMP
int NMP_IMPROVING_REDUCTION    = 1;     // Extra NMP reduction when not improving

// Futility Pruning + Improving
bool USE_FP_IMPROVING          = true;  // Use improving flag in FP
int FP_IMPROVING_MARGIN        = 80;    // Margin reduction when not improving (cp)

// Reverse Futility Pruning + Improving
bool USE_RFP_IMPROVING         = true;  // Use improving flag in RFP
int RFP_IMPROVING_MARGIN       = 60;    // Margin reduction when not improving (cp)

// LMP + Improving
bool USE_LMP_IMPROVING         = true;  // Use improving flag in LMP
```

### ConfigRegistry.cpp Entries

Each parameter needs a registry entry with getter/setter lambdas (per project conventions). All flags default to `true` for immediate effect after validation.

### YAML Configuration

```yaml
# config/search.yaml additions:
USE_IMPROVING: true

USE_LMR_IMPROVING: true
LMR_IMPROVING_REDUCTION: 1

USE_NMP_IMPROVING: true
NMP_IMPROVING_REDUCTION: 1

USE_FP_IMPROVING: true
FP_IMPROVING_MARGIN: 80

USE_RFP_IMPROVING: true
RFP_IMPROVING_MARGIN: 60

USE_LMP_IMPROVING: true
```

---

## Files Changed

| File                            | Change                                                                                                                                           | Phase |
|---------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------|-------|
| `src/engine/Search.cpp`         | Store staticEval in PlyInfo, compute improving flag, fix pvNode double-count, add searchNodes/qsearchNodes counters, apply to LMR/NMP/FP/RFP/LMP | 1–5   |
| `src/engine/SearchStats.h`      | Add `improvingTrue`/`improvingFalse`, `searchNodes`/`qsearchNodes` counters                                                                      | 1     |
| `src/config/SearchConfigData.h` | Add `USE_IMPROVING` master switch, future phase config parameters                                                                                | 1–5   |
| `src/config/ConfigRegistry.cpp` | Add registry entries for new parameters                                                                                                          | 1–5   |
| `config/search.yaml`            | Add `USE_IMPROVING` and future phase default values                                                                                              | 1–5   |

**No new files created** — this is a purely additive change to existing search infrastructure.

---

## Implementation Order & Testing Strategy

### Recommended Order

```
Phase 1: Foundation (staticEval storage + improving computation) ✅ COMPLETE & VERIFIED
    │
    ├── ✅ improvingTrue/False ratio: 59.9% / 40.1% (within expected range)
    ├── ✅ searchNodes/qsearchNodes split: 32.6% / 67.4%
    ├── ✅ pvNodes + nonPvNodes == searchNodes + qsearchNodes (invariant holds)
    │
Phase 2: LMR + Improving  ✅ COMPLETE & VERIFIED
    │
    ├── ✅ SearchTreeSizeTest: −5.2% node reduction (206,678 → 195,855)
    ├── ✅ LMR re-searches: −7.1% (extra reductions mostly correct)
    ├── ✅ NPS stable (no overhead)
    ├── Test suites: WAC/STS neutral or better (pending)
    ├── Self-play: 200+ games → measure ELO (pending)
    │
    ├── If POSITIVE: continue to Phase 3
    ├── If NEGATIVE: tune LMR_IMPROVING_REDUCTION, investigate
    │
Phase 3: NMP + Improving
    │
    ├── SearchTreeSizeTest + self-play
    │
Phase 4: FP/RFP + Improving
    │
    ├── SearchTreeSizeTest + self-play
    │
Phase 5: LMP + Improving
    │
    └── Final combined test: all improving features enabled
```

### Validation per Phase (Three-Stage Process)

#### Stage 1: SearchTreeSizeTest (Node Count)
- Add config flag toggle in `SearchTreeSizeTest.cpp`
- Verify node count decreases when feature enabled
- If nodes increase → investigate before proceeding

#### Stage 2: NPS & Depth Benchmarking
- NPS should not decrease (improving flag is computationally trivial)
- Same depth should be reached faster, or deeper depth in same time

#### Stage 3: Strength Testing (Arena Framework)
- Test suite scores (WAC, STS, Eigenmann)
- Self-play matches: 200+ games, SPRT testing
- Target: no ELO regression, ideally +5–25 ELO combined

### Rollback Strategy

Each feature has an independent config flag. If any individual feature causes regression:
1. Disable via config flag (no code change needed)
2. Investigate with SearchTreeSizeTest
3. Tune parameters (margins, reduction amounts)
4. Re-test

---

## Risk Assessment

| Risk                                          | Likelihood | Impact   | Mitigation                                                 |
|-----------------------------------------------|------------|----------|------------------------------------------------------------|
| Improving flag causes no measurable gain      | Low        | Low      | Technique is well-proven in other engines                  |
| Over-aggressive pruning when not improving    | Medium     | Medium   | Each feature has independent config flag + tunable margins |
| Under-aggressive pruning when improving       | Low        | Low      | Conservative defaults; can always reduce margins           |
| Interaction effects between features          | Medium     | Low      | Enable one at a time, measure incrementally                |
| Edge case: ply < 2 handling                   | Low        | Low      | Fallback logic (ply-4, then assume true)                   |
| Performance overhead of improving computation | Very Low   | Very Low | Single comparison per node, negligible cost                |

---

## References

- **Stockfish source:** `search.cpp` — `improving` flag used in LMR, NMP, FP, LMP, RFP
- **Chess Programming Wiki:** [Improving Heuristic](https://www.chessprogramming.org/Improving)
- **Ethereal source:** Similar approach to Stockfish
- **FrankyCPP Search Tree Reduction Review:** `PLAN_Search_Tree_Reduction_Review.md` (Change 1.4.5)
- **FrankyCPP Roadmap:** `V1_ENGINE_STRENGTH_ROADMAP.md` — listed under "Missing improving flag for LMR"

---

## Change Log

| Version | Date       | Changes                                                                                                         |
|---------|------------|-----------------------------------------------------------------------------------------------------------------|
| 1.0     | 2026-02-20 | Initial plan document                                                                                           |
| 1.1     | 2026-02-20 | Phase 1 implemented: staticEval stored in PlyInfo, improving flag computed, stats added                         |
| 1.2     | 2026-02-20 | Phase 1 verified: 59.9% improving ratio confirmed, added searchNodes/qsearchNodes, fixed pvNode double-count    |
| 1.3     | 2026-02-20 | Phase 2 implemented: LMR+Improving — extra reduction when not improving, config flags, SearchTreeSizeTest entry |
| 1.4     | 2026-02-20 | Phase 2 verified: −5.2% nodes, −7.1% re-searches, stable NPS                                                    |

---

*Document created as part of FrankyCPP search efficiency improvement initiative*
