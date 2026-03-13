# FrankyCPP - MultiPV Implementation Plan

**Document Version:** 1.0
**Created:** 2026-03-07
**Status:** 📋 PLANNED
**Target Version:** v1.7+
**Estimated Effort:** 1-2 days
**Dependencies:** Best Thread Selection (Phases 1-2 recommended first)

---

## Overview

**MultiPV** (Multiple Principal Variations) is a standard UCI option that tells the engine to find and report the **N best moves** instead of just the single best move. Essential for analysis mode.

**Current behavior:** Engine finds and reports only the single best move (MultiPV=1 implicit).

**Proposed behavior:** Support `setoption name MultiPV value N` to report top N moves with scores and PVs.

---

## UCI Protocol

### Input
```
setoption name MultiPV value 3
```

### Output (per iteration)
```
info depth 12 multipv 1 score cp 45 pv e2e4 e7e5 g1f3 ...
info depth 12 multipv 2 score cp 32 pv d2d4 d7d5 c2c4 ...
info depth 12 multipv 3 score cp 28 pv c2c4 e7e5 b1c3 ...
```

---

## Performance Impact

- **MultiPV=1:** No overhead (loop executes once)
- **MultiPV>1:** Significantly slower (~2-4x per additional PV)
  - Cannot prune as aggressively (must find N good moves)
  - LMR less effective
  - More nodes searched

---

## Implementation Phases

### Phase 1: Add UCI Option

**Files:** `UciHandler.cpp`, `SearchConfigData.h`, `ConfigRegistry.cpp`

- Add `MultiPV` UCI option (default=1, min=1, max=~64)
- Store in search config: `int MULTI_PV = 1;`

**Effort:** 20 minutes

---

### Phase 2: Track Multiple PVs in SearchThreadData

**File:** `SearchThreadData.h`

```cpp
// Current: single PV
PrincipalVariation pv;

// New: support multiple PVs
std::vector<PrincipalVariation> pvLines;  // size = multiPV
```

Or leverage existing `rootMoves` which already stores move + value per root move.

**Effort:** 30 minutes

---

### Phase 3: Modify iterativeDeepening() Root Search Loop

**File:** `Search.cpp`

Core change — wrap root move search in MultiPV loop:

```cpp
void iterativeDeepening() {
    for (Depth depth = 1; depth <= maxDepth; ++depth) {
        
        // MultiPV loop: find top N moves
        for (int pvIdx = 0; pvIdx < multiPV; ++pvIdx) {
            
            // Alpha bound: first PV uses normal alpha,
            // subsequent PVs use previous PV's score as alpha
            Value alpha = (pvIdx == 0) ? aspirationAlpha 
                                       : thread().rootMoves[pvIdx - 1].value;
            
            // Search root moves starting from index pvIdx
            // (moves 0..pvIdx-1 are already ranked/locked)
            searchRootMoves(depth, alpha, beta, pvIdx);
            
            // Partial sort: bubble best remaining move to position pvIdx
            std::stable_sort(thread().rootMoves.begin() + pvIdx,
                            thread().rootMoves.end(),
                            compareByScore);
            
            // Send UCI info for this PV line
            if (isMainThread()) {
                sendMultiPvInfo(depth, pvIdx + 1, thread().rootMoves[pvIdx]);
            }
        }
    }
}
```

**Key points:**
- When `multiPV=1`, loop executes once with `pvIdx=0` — standard behavior
- Moves 0..pvIdx-1 are "locked in" after being searched
- Only sort from `pvIdx` onwards to preserve rankings

**Effort:** 1-2 hours (main complexity)

---

### Phase 4: Modify UCI Output

**File:** `Search.cpp`

Add/modify UCI output method:

```cpp
void Search::sendMultiPvInfo(Depth depth, int pvIndex, const RootMove& rm) {
    std::ostringstream ss;
    ss << "info"
       << " depth " << depth
       << " multipv " << pvIndex
       << " score " << uciScore(rm.value)
       << " nodes " << getTotalNodes()
       << " nps " << nps(...)
       << " time " << elapsed
       << " pv " << rm.pv.toUciString();
    
    uciHandler.sendToUci(ss.str());
}
```

**Effort:** 30 minutes

---

### Phase 5: Best Move Selection with MultiPV

**File:** `Search.cpp`

After search completes:
- `bestmove` is always `rootMoves[0]` (the top-ranked move)
- `ponder` move comes from `rootMoves[0].pv[1]`

No change needed if rootMoves properly sorted.

**Effort:** 15 minutes (verification)

---

### Phase 6: Interaction with Lazy SMP

**Consideration:** With MultiPV and multiple threads:
- Each thread finds its own top N moves
- Best thread selection compares `rootMoves[0]` (top move) across threads
- Alternative: compare all N PVs? (complex, likely overkill)

**Recommendation:** Keep simple — best thread selection uses only the #1 move from each thread.

**Effort:** 15 minutes (review/document)

---

### Phase 7: Testing

- Verify UCI output format matches spec
- Test MultiPV=1 has no regression
- Test MultiPV=3,5 produces valid distinct moves
- Performance benchmark: measure slowdown per additional PV

**Effort:** 1 hour

---

## Summary of Changes

| File                 | Changes                                  | Phase |
|----------------------|------------------------------------------|-------|
| `SearchConfigData.h` | Add `int MULTI_PV = 1;`                  | 1     |
| `ConfigRegistry.cpp` | Register UCI option                      | 1     |
| `UciHandler.cpp`     | Parse `setoption name MultiPV`           | 1     |
| `SearchThreadData.h` | (Optional) Multiple PV storage           | 2     |
| `Search.cpp`         | MultiPV loop in `iterativeDeepening()`   | 3     |
| `Search.cpp`         | `sendMultiPvInfo()` with `multipv` field | 4     |
| `Search_Test.cpp`    | Unit tests                               | 7     |

---

## Open Questions

1. **Max MultiPV value?** Stockfish allows up to 500. Suggest 64 or 128 as reasonable max.
2. **Aspiration windows with MultiPV?** First PV uses aspiration, subsequent PVs typically use full window.
3. **Helper threads + MultiPV?** Helpers probably run MultiPV=1 for efficiency, only main thread does full MultiPV.

---

## References

- UCI Protocol: `multipv` info field
- Stockfish: `search.cpp` MultiPV implementation
- Best Thread Selection: `docs/specs/PLAN_Best_Thread_Selection.md`

---

*Created: 2026-03-07*
