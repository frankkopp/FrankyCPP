# FrankyCPP - MultiPV Implementation Plan

**Document Version:** 2.0
**Created:** 2026-03-07
**Completed:** 2026-04-06
**Status:** ✅ COMPLETE
**Target Version:** v1.7+
**Actual Effort:** ~3 hours (implementation + testing + refinement)
**Dependencies:** Best Thread Selection (Phases 1-2 recommended first)

---

## Overview

**MultiPV** (Multiple Principal Variations) is a standard UCI option that tells the engine to find and report the **N best moves** instead of just the single best move. Essential for analysis mode.

**Implemented behavior:** `setoption name MultiPV value N` reports top N moves with scores and PVs per iteration depth, with Stockfish-style batched output and guaranteed monotonically non-increasing scores.

---

## UCI Protocol

### Input
```
setoption name MultiPV value 3
```

### Output (per iteration)
```
info depth 12 multipv 1 score cp 45 nodes 1234567 nps 1000000 time 1234 pv e2e4 e7e5 g1f3 ...
info depth 12 multipv 2 score cp 32 nodes 1234567 nps 1000000 time 1234 pv d2d4 d7d5 c2c4 ...
info depth 12 multipv 3 score cp 28 nodes 1234567 nps 1000000 time 1234 pv c2c4 e7e5 b1c3 ...
```

All PV lines for a given depth are sent as a batch with identical node counts (Stockfish-style).

---

## Performance Impact

- **MultiPV=1:** Zero overhead (loop executes once, no vector allocation for results)
- **MultiPV>1:** Significantly slower (~2-4x per additional PV)
  - Secondary PVs use full-window search (no aspiration)
  - More nodes searched per iteration
  - Helper threads always use MultiPV=1 for efficiency

---

## Implementation Summary

### Files Modified

| File                 | Changes                                                                                                |
|----------------------|--------------------------------------------------------------------------------------------------------|
| `SearchConfigData.h` | Added `CONFIG_ESSENTIAL int MULTI_PV = 1;`                                                             |
| `ConfigRegistry.cpp` | Registered UCI option `MultiPV` (spin, 1-128)                                                          |
| `Search.h`           | Added `MultiPvResult` struct, `sendMultiPvResultsToUci()`, `startIndex` param on `rootSearch()`        |
| `Search.cpp`         | MultiPV loop in `iterativeDeepening()`, batched sorted reporting, aspiration suppression for MultiPV>1 |
| `UciHandler.h`       | Added `multipvIndex` parameter to `sendIterationEndInfo()` and `sendAspirationResearchInfo()`          |
| `UciHandler.cpp`     | Dynamic `multipv` field in UCI output                                                                  |
| `SearchTest.cpp`     | 5 unit tests                                                                                           |

### Design Decisions

1. **Stockfish-style batched output:** All PV lines for a given depth are collected during the loop, sorted by score (descending), and sent to UCI in a single batch. This guarantees monotonically non-increasing scores and consistent node counts across all PV lines.

2. **Post-loop sort handles search instability:** Secondary PV searches run with richer TT state and full windows, which can occasionally produce higher scores than pvIdx=0's aspiration search. Sorting all completed PVs by score after the loop ensures correct ordering. `rootMoves[0..N]` are also re-sorted to match, so post-iteration code (stability tracking, aspiration centering, mate detection) sees the true best move.

3. **Aspiration info suppressed for MultiPV>1:** Aspiration `lowerbound`/`upperbound` lines send `multipv 1` during pvIdx=0's search. When the GUI still displays the previous depth's complete multi-PV set, this creates a non-monotonic display artifact. Solution: suppress aspiration UCI output when MultiPV>1 (log output still emitted).

4. **Helper threads always use MultiPV=1:** Helpers contribute to TT diversity; doing full MultiPV on helpers would waste search effort.

5. **Aspiration only for pvIdx=0:** Secondary PVs have no reliable previous-iteration estimate, so they use full window `[VALUE_MIN, VALUE_MAX]`.

6. **effectiveMultiPV clamped:** `min(MULTI_PV, rootMoves.size())` prevents searching more PVs than legal moves.

### Tests

- `multiPV1NoRegression` — MultiPV=1 produces same results as default
- `multiPV3ProducesValidResult` — MultiPV=3 completes with valid best move
- `multiPVClampedToLegalMoves` — MultiPV > legal moves doesn't crash
- `multiPVWithSMP` — MultiPV works with Lazy SMP (helpers use MultiPV=1)
- `multiPVTimedSearch` — MultiPV works with time-controlled search

---

## Open Questions (Resolved)

1. **Max MultiPV value?** → 128 (sufficient for any analysis scenario)
2. **Aspiration windows with MultiPV?** → First PV uses aspiration, subsequent PVs use full window
3. **Helper threads + MultiPV?** → Helpers always use MultiPV=1

---

## References

- UCI Protocol: `multipv` info field
- Stockfish: `search.cpp` MultiPV implementation (batched sorted output)
- Best Thread Selection: `docs/specs/PLAN_Best_Thread_Selection.md`

---

*Created: 2026-03-07 | Completed: 2026-04-06*
