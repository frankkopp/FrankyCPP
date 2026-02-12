# Plan: Selective Quiet Checks in Quiescence Search

**Status:** Planned  
**Created:** 2026-02-11  
**Priority:** Medium  
**Expected Impact:** +15-25 ELO

---

## Overview

Add a new `GenQuietChecks` mode to the move generator that generates non-capturing moves which give direct check. This will be used in quiescence search to avoid the "horizon effect" where tactical sequences involving checks are cut off prematurely.

## The Problem

Standard quiescence search only examines captures. This misses tactics like:

```
1. Qxh7+ Kxh7 2. Rh1# (discovered check leading to mate)
```

If QSearch stops after `Qxh7+`, it doesn't see the follow-up check `Rh1#` because it's not a capture.

## Solution

Extend quiescence search to include non-capture moves that give **direct check**. The key insight is that quiet checks should be a separate on-demand generation phase, integrated into the existing phased move generation.

### Key Design Points

1. **New GenMode flag** - `GenQuietChecks = 0b100` added to existing bit flags
2. **On-demand generation** - New `QUIET_CHECKS` stage in phased generation
3. **Efficient check detection** - Use `Attacks::attacks(pt, enemyKing, occupiedBb)` to get check squares, then `& ~occupiedBb` for non-captures
4. **Configurable** - Disabled by default, controlled via YAML and UCI options
5. **Direct checks only** - Discovered checks deferred to Phase 2 (more complex detection)

---

## Implementation Steps

### 1. Add `GenQuietChecks` to GenMode enum
**File:** `src/chesscore/MoveGenerator.h` (line ~92)

```cpp
enum GenMode : uint8_t {
  GenZero        = 0b000,  ///< No moves
  GenNonQuiet    = 0b001,  ///< Captures and promotions only
  GenQuiet       = 0b010,  ///< Non-capturing moves only
  GenAll         = 0b011,  ///< All moves (captures + quiet)
  GenQuietChecks = 0b100   ///< Non-capturing moves that give check
};
```

### 2. Add `QUIET_CHECKS` stage to onDemandStage enum
**File:** `src/chesscore/MoveGenerator.h` (line ~107)

Insert `QUIET_CHECKS` after `KING_CAPTURES` and before `QUIET_SWITCH`:

```cpp
enum onDemandStage {
  OD_NEW,
  PV_MOVE,
  PAWN_CAPTURES,
  OFFICER_CAPTURES,
  KING_CAPTURES,
  QUIET_CHECKS,     // NEW - quiet moves that give check
  QUIET_SWITCH,
  PAWN_MOVES,
  CASTLING_MOVES,
  OFFICER_MOVES,
  KING_MOVES,
  OD_END
};
```

### 3. Add `lastMoveWasQuietCheck()` method to MoveGenerator
**File:** `src/chesscore/MoveGenerator.h`

Add member variable to track which stage produced the last returned move:

```cpp
// Member variable
onDemandStage lastReturnedStage = OD_NEW;
```

Add method to query if last move was a quiet check:

```cpp
[[nodiscard]] bool lastMoveWasQuietCheck() const noexcept { 
  return lastReturnedStage == QUIET_CHECKS; 
}
```

Update `getNextPseudoLegalMove()` to track the stage before returning:

```cpp
// Before returning a move
lastReturnedStage = currentODStage;
return move;
```

### 4. Add config parameters to SearchConfigData
**File:** `src/engine/config/SearchConfigData.h`

Add after existing quiescence parameters (~line 60):

```cpp
// quiescence check extensions
bool USE_QS_CHECKS = false;   // disabled by default for safe rollout
int QS_CHECKS_DEPTH = 0;      // only at first ply of quiescence (qsDepth <= this)
int QS_CHECKS_LIMIT = 2;      // max quiet checks searched per node (0 = unlimited)
```

Add YAML encode/decode entries in the same file.

### 4. Modify generateMoves() to handle GenQuietChecks
**File:** `src/chesscore/MoveGenerator.cpp` (in generateMoves function)

Add after existing quiet move handling:

```cpp
// Direct checks (non-capturing moves that give check)
if (genMode & GenQuietChecks) {
  const Square enemyKing = position.getKingSquare(~nextPlayer);
  const Bitboard checkSquares = Attacks::attacks(pt, enemyKing, occupiedBb);
  
  Bitboard quietChecks = pseudoMoves & ~occupiedBb & checkSquares;
  if (evasion) { quietChecks &= evasionTargets; }
  
  while (quietChecks) {
    const Square toSquare = quietChecks.popLSB();
    const Value value = Values::posValue[piece][toSquare][gamePhase] + 500; // bonus for check
    pMoves->push_back(Move::normal(fromSquare, toSquare, value));
  }
}
```

### 5. Modify generatePawnMoves() for pawn checks
**File:** `src/chesscore/MoveGenerator.cpp`

Add handling for `GenQuietChecks`:

```cpp
if (genMode & GenQuietChecks) {
  const Square enemyKing = position.getKingSquare(~nextPlayer);
  // Squares where a pawn would give check
  const Bitboard pawnCheckSquares = Bitboards::pawnAttacks[nextPlayer][enemyKing];
  
  // Single pawn pushes that give check (non-promotion only)
  Bitboard singleChecks = pawnSinglePush & pawnCheckSquares & ~promotionRank;
  // ... generate moves
  
  // Double pawn pushes that give check  
  Bitboard doubleChecks = pawnDoublePush & pawnCheckSquares;
  // ... generate moves
}
```

### 6. Modify fillOnDemandMoveList() for QUIET_CHECKS stage
**File:** `src/chesscore/MoveGenerator.cpp`

Add case for `QUIET_CHECKS` stage:

```cpp
case QUIET_CHECKS:
  if (genMode & GenQuietChecks) {
    // Generate pawn quiet checks
    generatePawnMoves(position, &onDemandMoves, GenQuietChecks, evasion, onDemandEvasionTargets);
    // Generate officer quiet checks
    generateMoves(position, &onDemandMoves, GenQuietChecks, evasion, onDemandEvasionTargets);
    onDemandMoves.sort();
  }
  currentODStage = QUIET_SWITCH;
  break;
```

### 7. Modify qsearch() to use GenQuietChecks
**File:** `src/engine/Search.cpp` (in qsearch function, ~line 1454)

Track quiescence depth and conditionally add quiet checks:

```cpp
// Modify genMode selection:
GenMode genMode = hasCheck ? GenAll : GenNonQuiet;
if (!hasCheck && SearchConfig.USE_QS_CHECKS && qsDepth <= SearchConfig.QS_CHECKS_DEPTH) {
  genMode = static_cast<GenMode>(genMode | GenQuietChecks);
}
```

Add quiet check limiting in the move loop:

```cpp
int quietChecksSearched = 0;

// MOVE LOOP
while ((move = myMg->getNextPseudoLegalMove(p, genMode, hasCheck)) != MOVE_NONE) {
  
  // Limit quiet checks searched per node
  if (myMg->lastMoveWasQuietCheck()) {
    if (SearchConfig.QS_CHECKS_LIMIT > 0 && ++quietChecksSearched > SearchConfig.QS_CHECKS_LIMIT) {
      continue;  // skip remaining quiet checks
    }
  }
  
  // ... rest of move loop
}
```

Note: Need to track `qsDepth` - either pass as parameter or derive from `ply - qsStartPly`.

### 8. Add UCI options
**File:** `src/engine/UciOptions.cpp`

Add after existing quiescence options (~line 80):

```cpp
optionVector.emplace_back(
  "Use Quiescence Checks", SearchConfig.USE_QS_CHECKS,
  [&](UciHandler*) {
    CONFIG_OVERRIDE(s.USE_QS_CHECKS = getOption("Use Quiescence Checks")->currentValue == "true";);
  });

optionVector.emplace_back(
  "Quiescence Checks Depth", SearchConfig.QS_CHECKS_DEPTH, 0, 3,
  [&](UciHandler*) {
    CONFIG_OVERRIDE(s.QS_CHECKS_DEPTH = getInt(getOption("Quiescence Checks Depth")->currentValue););
  });

optionVector.emplace_back(
  "Quiescence Checks Limit", SearchConfig.QS_CHECKS_LIMIT, 0, 10,
  [&](UciHandler*) {
    CONFIG_OVERRIDE(s.QS_CHECKS_LIMIT = getInt(getOption("Quiescence Checks Limit")->currentValue););
  });
```

### 9. Update search.yaml
**File:** `config/search.yaml`

Add under quiescence section:

```yaml
# Quiescence check extensions
USE_QS_CHECKS: false
QS_CHECKS_DEPTH: 0
QS_CHECKS_LIMIT: 2
```

### 10. Add unit tests
**File:** `test/chesscore/MoveGeneratorTest.cpp`

Test cases:
- `GenQuietChecks` generates correct knight/bishop/rook/queen checks
- Pawn checks (single push, double push)
- Captures are NOT included in `GenQuietChecks`
- On-demand generation with `QUIET_CHECKS` stage
- `lastMoveWasQuietCheck()` returns true only for quiet check moves
- Evasion mode with quiet checks

---

## Files to Modify

| File | Changes |
|------|---------|
| `src/chesscore/MoveGenerator.h` | Add `GenQuietChecks` to enum, add `QUIET_CHECKS` stage, add `lastReturnedStage` member, add `lastMoveWasQuietCheck()` method |
| `src/chesscore/MoveGenerator.cpp` | Add check generation in `generateMoves()`, `generatePawnMoves()`, `fillOnDemandMoveList()`, track `lastReturnedStage` in `getNextPseudoLegalMove()` |
| `src/engine/config/SearchConfigData.h` | Add `USE_QS_CHECKS`, `QS_CHECKS_DEPTH`, `QS_CHECKS_LIMIT` with YAML support |
| `src/engine/Search.cpp` | Modify `qsearch()` to use `GenQuietChecks` conditionally, add quiet check limit counter using `lastMoveWasQuietCheck()` |
| `src/engine/UciOptions.cpp` | Add UCI options for the new parameters (3 options) |
| `config/search.yaml` | Add default values |
| `test/chesscore/MoveGeneratorTest.cpp` | Add tests for quiet check generation and `lastMoveWasQuietCheck()` |

---

## Performance Considerations

### Generation Cost
The move generation cost is approximately the same as full quiet move generation because:
1. Generate all quiet moves for each piece type
2. `&` with `checkSquares` instead of just `~occupiedBb`
3. Same iteration over piece bitboards, same attack lookups

### Search Cost Savings
The savings come from **fewer moves searched**, not fewer moves generated:

| Scenario | Moves Generated | Moves Searched |
|----------|-----------------|----------------|
| Normal QSearch | ~20 captures | ~20 |
| QSearch + Checks | ~20 captures + ~3 checks | ~23 |
| Normal Search | ~20 captures + ~30 quiet | ~50 |

### Mitigation
- Limit to `qsDepth == 0` (first ply of quiescence only) - controlled by `QS_CHECKS_DEPTH`
- Future: Add `QS_CHECKS_LIMIT` to cap number of checks searched per node

---

## Future Enhancements (Phase 2)

1. **Discovered checks** - More complex detection required (check if moving piece unmasks slider attack on enemy king)
2. **SEE filtering** - Skip checks where SEE < 0 (piece is lost after giving check)
3. **Delta pruning for checks** - Skip checks that can't raise alpha even with check bonus

---

## Testing Strategy

1. **Unit tests** - Verify correct moves generated for various positions
2. **Perft tests** - Ensure no regression in move generation correctness
3. **Self-play** - Compare with/without feature enabled
4. **SPRT testing** - Statistical testing against baseline to measure ELO impact

---

## Design Decisions

| Decision | Rationale |
|----------|-----------|
| Direct checks only | Discovered checks are complex; start simple |
| Disabled by default | Safe rollout; enable after ELO testing |
| New GenMode flag | Clean integration with existing bit flag system |
| Stage after captures | Checks are tactical, should be tried early |
| `QS_CHECKS_LIMIT` from start | Prevent runaway checking sequences (chessprogramming.org advice) |
| `lastMoveWasQuietCheck()` method | Zero-overhead detection using existing stage tracking; cleaner than analyzing move properties |

---

*Last updated: 2026-02-11*
