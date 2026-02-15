# Tablebase Implementation Review Prompt

**Date:** 2026-02-15  
**Purpose:** Independent review of Syzygy tablebase implementation in FrankyCPP chess engine

---

## Context

FrankyCPP is a UCI chess engine written in modern C++20. We have just completed implementing Syzygy tablebase support across 6 phases:

1. **Phase 1:** Fathom library integration (CMake FetchContent)
2. **Phase 2:** Storage & download management (path resolution, CLI download)
3. **Phase 3:** Root tablebase probing (best move at root position)
4. **Phase 4:** Search tablebase probing (cutoffs during alpha-beta search)
5. **Phase 5:** Configuration & UCI options
6. **Phase 6:** Testing (unit tests, integration tests)

---

## Your Task

Please review the tablebase implementation for:

1. **Correctness** - Is the Fathom API used correctly? Are WDL/DTZ values interpreted properly?
2. **Performance** - Are there unnecessary overhead or redundant checks in hot paths?
3. **Thread Safety** - Is the implementation safe for future multi-threaded search?
4. **Edge Cases** - En passant handling, castling positions, 50-move rule?
5. **Code Quality** - Const-correctness, error handling, documentation?

---

## Key Files to Review

### Core Implementation
- `src/tablebase/Tablebase.h` - C++ wrapper interface
- `src/tablebase/Tablebase.cpp` - Fathom integration, position conversion, move conversion

### Search Integration
- `src/engine/Search.cpp` - Look for:
  - `probeTablebaseAtRoot()` call around line 320
  - Search probing around line 925 (feature gate: `USE_TB_PROBE_SEARCH`)
  - `filterRootMovesByTB()` function
  - `getTBScoreForSearch()` function (50-move rule handling)

### Configuration
- `src/config/SearchConfigData.h` - TB config fields (lines 64-73):
  - `TB_PATH` - Path to tablebase files
  - `USE_TB_PROBE_ROOT` - Enable root probing
  - `USE_TB_PROBE_SEARCH` - Enable search probing  
  - `TB_PROBE_DEPTH` - Minimum depth to probe in search
  - `TB_PROBE_LIMIT` - Maximum pieces for probing
  - `TB_RULE50_THRESHOLD` - 50-move rule handling threshold

### Tests
- `test/tablebase/TablebaseTest.cpp` - Comprehensive test suite (~1600 lines)

---

## Specific Areas of Concern

### 1. Position Conversion (Tablebase.cpp)

The `convertPositionToFathom()` function converts FrankyCPP's Position to Fathom's format. Key concerns:

- **En passant legality**: We only pass EP square if a capturing pawn actually exists
- **Square encoding**: Must match Fathom's little-endian rank-file (A1=0, H8=63)
- **Castling**: Positions with castling rights should not be probed (TBs don't include them)

```cpp
// Verify this logic is correct:
if (epSq != SQ_NONE) {
  const Bitboard attackers = Bitboards::pawnAttacks[~stm][epSq] & pos.getPieceBb(stm, PAWN);
  if (attackers) {
    ep = static_cast<unsigned>(epSq);
  }
}
```

### 2. Search Probing Feature Gate (Search.cpp ~line 925)

```cpp
if (SearchConfig.USE_TB_PROBE_SEARCH
    && syzygy_tb
    && syzygy_tb->isAvailable()
    && depth >= SearchConfig.TB_PROBE_DEPTH
    && p.getOccupiedBb().popcount() <= SearchConfig.TB_PROBE_LIMIT
    && p.getCastlingRights() == NO_CASTLING) {
```

- Is the order of checks optimal (cheapest first)?
- Should we check `!isPv` before probing to avoid overhead on PV nodes?

### 3. 50-Move Rule Handling (Search.cpp)

The `getTBScoreForSearch()` function handles positions near the 50-move limit:

```cpp
if (halfMoveClock < SearchConfig.TB_RULE50_THRESHOLD) {
  return tablebase::Tablebase::tbValueToScore(wdl, ply);  // Fast path
}
// Slow path: conservative scoring near 50-move limit
```

- Is `TB_RULE50_THRESHOLD=80` a good default?
- Is the conservative approach (treating uncertain wins as draws) correct?

### 4. Root Move Filtering (Search.cpp)

`filterRootMovesByTB()` removes root moves that worsen the TB result:
- Winning position: Keep only moves where opponent is losing
- Drawing position: Keep only moves where opponent is not winning
- Losing position: Keep all moves

Is this logic sound? What if all moves are filtered out?

### 5. DTZ-Based Scoring

`tbResultToScore(wdl, dtz)` converts WDL+DTZ to a search score:
- Shorter wins (smaller DTZ) should score higher
- Does the score range (~8800-8999 for wins) integrate well with mate scores?

---

## Known Design Decisions

1. **No master USE_TB switch** - Instead, `USE_TB_PROBE_ROOT=false` and `USE_TB_PROBE_SEARCH=false` disables all TB
2. **TT stores TB results** - To avoid re-probing the same position
3. **PV nodes don't cut off** - Only tighten bounds to maintain complete PV line
4. **probeWDL() for search, probeRoot() for root** - WDL is faster, DTZ+move only needed at root

---

## Questions to Answer

1. Are there any bugs or logic errors in the implementation?
2. Are there any performance issues in the hot path (search probing)?
3. Is the 50-move rule handling correct and safe?
4. Are there any thread safety concerns for future SMP support?
5. Are the tests comprehensive enough?
6. Any suggestions for improvement?

---

## Reference

- **Plan Document:** `docs/specs/PLAN_Syzygy_Tablebase_Support.md`
- **Fathom API:** https://github.com/jdart1/Fathom
- **Syzygy Format:** https://www.chessprogramming.org/Syzygy_Endgame_Tablebases
