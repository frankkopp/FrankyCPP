# PlyStack Refactoring Plan

**Status:** 📋 Planned (v1.3+)  
**Priority:** Medium  
**Estimated Effort:** 3-5 days  
**Dependencies:** None (can be done independently)

---

## Overview

Refactor the per-ply search state from scattered arrays into a unified `PlyInfo` struct. This improves code organization, cache locality, and prepares for future enhancements like Lazy SMP and continuation history.

---

## Current Architecture (v1.2)

Per-ply data is stored in separate arrays/pointers in the `Search` class:

```cpp
// Current: Scattered arrays and heap-allocated MG pools
PVTable pv;                                          // Triangular PV table (embedded)
std::unique_ptr<std::array<MoveGenerator, DEPTH_MAX + 1>> mg;        // Heap-allocated
std::unique_ptr<std::array<MoveGenerator, DEPTH_MAX + 1>> mgSingular; // Heap-allocated
std::array<Move, DEPTH_MAX + 1> excludedMove{};      // Excluded move for singular
```

**Note:** MoveGenerator arrays were moved to heap in v1.2 to keep `Search` class size small (~64 KB instead of ~865 KB), allowing stack allocation of `Search` objects.

**Problems:**
1. Related data is scattered across multiple arrays
2. Poor cache locality when accessing multiple per-ply fields
3. Adding new per-ply data requires adding new arrays
4. Not scalable for Lazy SMP (each thread needs its own state)

---

## Proposed Architecture

### PlyInfo Struct

```cpp
/// Per-ply search state - groups all ply-specific data together
struct PlyInfo {
    // MoveGenerator pointers (point to pre-allocated pools)
    MoveGenerator* mg{nullptr};           // Normal search MoveGenerator
    MoveGenerator* mgSingular{nullptr};   // Singular verification MoveGenerator
    
    // Move tracking
    Move currentMove{MOVE_NONE};          // Move being searched at this ply
    Move excludedMove{MOVE_NONE};         // Excluded move for singular extension
    
    // Evaluation
    Value staticEval{VALUE_NONE};         // Static evaluation at this ply
    
    // Search state
    int moveCount{0};                     // Number of moves searched at this ply
    bool inCheck{false};                  // Is side to move in check?
    
    // Future: Continuation history pointers (Phase 4)
    // PieceToHistory* continuationHistory[6]{};
};
```

### Memory Layout

```cpp
class Search {
    // Pre-allocated MoveGenerator pools on heap (~400 KB each)
    // Already implemented in v1.2
    std::unique_ptr<std::array<MoveGenerator, DEPTH_MAX + 1>> mgPool;
    std::unique_ptr<std::array<MoveGenerator, DEPTH_MAX + 1>> mgSingularPool;
    
    // Per-ply state (~64 bytes × 128 = ~8 KB)
    std::array<PlyInfo, DEPTH_MAX + 1> plyInfo{};
    
    // PV table remains separate (different access pattern)
    PVTable pv;
};
```

### Initialization (in Search constructor or newGame)

```cpp
void Search::initPlyInfo() {
    for (int ply = 0; ply <= DEPTH_MAX; ++ply) {
        plyInfo[ply].mg = &mgPool[ply];
        plyInfo[ply].mgSingular = &mgSingularPool[ply];
    }
}
```

---

## Usage Examples

### Before (Current)

```cpp
// Scattered access
auto* myMg = excludedMove[ply] != MOVE_NONE ? &mgSingular[ply] : &mg[ply];
myMg->resetOnDemand();

// Setting excluded move
excludedMove[ply] = ttMove;
```

### After (With PlyInfo)

```cpp
// Unified access
auto& info = plyInfo[ply];
auto* myMg = info.excludedMove != MOVE_NONE ? info.mgSingular : info.mg;
myMg->resetOnDemand();

// Setting excluded move
info.excludedMove = ttMove;

// Or even cleaner with a helper:
MoveGenerator* Search::getMoveGenerator(Depth ply) {
    return plyInfo[ply].excludedMove != MOVE_NONE 
         ? plyInfo[ply].mgSingular 
         : plyInfo[ply].mg;
}
```

---

## Benefits

| Aspect | Before | After |
|--------|--------|-------|
| **Code Organization** | Scattered arrays | Unified struct |
| **Cache Locality** | Poor (separate allocations) | Better (related data together) |
| **Adding New Fields** | Add new array | Add field to struct |
| **Readability** | `excludedMove[ply]` | `plyInfo[ply].excludedMove` |
| **Lazy SMP Ready** | Difficult | Each thread gets own `plyInfo[]` |
| **Future Extensions** | More arrays | More fields |

---

## Memory Analysis

### Current Memory Usage (v1.2)

| Structure | Location | Size per ply | Total (128 plies) |
|-----------|----------|--------------|-------------------|
| `mg` | Heap | ~3.1 KB | ~400 KB |
| `mgSingular` | Heap | ~3.1 KB | ~400 KB |
| `excludedMove[]` | Stack | 4 bytes | 512 bytes |
| `pv` (triangular) | Stack | varies | 64 KB |
| **Search class size** | | | **~65 KB** (stack-safe) |
| **Total heap** | | | **~800 KB** |

### Proposed Memory Usage (with PlyInfo)

| Structure | Location | Size per ply | Total (128 plies) |
|-----------|----------|--------------|-------------------|
| `mgPool` | Heap | ~3.1 KB | ~400 KB |
| `mgSingularPool` | Heap | ~3.1 KB | ~400 KB |
| `plyInfo[]` | Stack | ~64 bytes | ~8 KB |
| `pv` (triangular) | Stack | varies | 64 KB |
| **Search class size** | | | **~72 KB** (stack-safe) |
| **Total heap** | | | **~800 KB** |

**Net change:** +7 KB in Search class (negligible, still stack-safe)

---

## Implementation Steps

### Phase 1: Create PlyInfo Struct (1 day)
1. Define `PlyInfo` struct in new header `src/engine/PlyInfo.h`
2. Add `plyInfo[]` array to Search class
3. Add `mgPool[]` and `mgSingularPool[]` arrays
4. Initialize pointers in constructor

### Phase 2: Migrate excludedMove (0.5 day)
1. Move `excludedMove` access to `plyInfo[ply].excludedMove`
2. Remove standalone `excludedMove[]` array
3. Update all usages in Search.cpp

### Phase 3: Migrate MoveGenerator Selection (0.5 day)
1. Use `plyInfo[ply].mg` and `plyInfo[ply].mgSingular`
2. Consider adding `getMoveGenerator(ply)` helper method
3. Update singular extension code

### Phase 4: Add Additional Fields (1 day)
1. Add `currentMove`, `staticEval`, `moveCount`, `inCheck` to PlyInfo
2. Migrate usages from local variables where beneficial
3. This prepares for continuation history (Phase 4 of roadmap)

### Phase 5: Testing & Cleanup (1 day)
1. Run full test suite
2. Benchmark to verify no performance regression
3. Update documentation
4. Remove old scattered arrays

---

## Future Extensions (Post-Refactoring)

### Lazy SMP (v1.3)
Each search thread will have its own:
- `std::array<PlyInfo, DEPTH_MAX + 1> plyInfo{}`
- `std::array<MoveGenerator, DEPTH_MAX + 1> mgPool{}`
- `std::array<MoveGenerator, DEPTH_MAX + 1> mgSingularPool{}`

The TT and history tables remain shared.

### Continuation History (v1.4)
Add to PlyInfo:
```cpp
PieceToHistory* continuationHistory[6]{};  // Pointers to history tables
```

Access pattern:
```cpp
// Score move using continuation history from previous plies
int score = (*plyInfo[ply-1].continuationHistory)[piece][to]
          + (*plyInfo[ply-2].continuationHistory)[piece][to];
```

### Additional Per-Ply State
Future enhancements may need:
- `int cutoffCount` - for adaptive pruning
- `int reduction` - LMR reduction at this ply
- `bool ttHit` - whether TT was hit
- `bool ttPv` - whether this is a TT-indicated PV node

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Performance regression | Low | Medium | Benchmark before/after |
| Bugs during migration | Medium | Low | Incremental changes with testing |
| Increased memory | Low | Low | ~7 KB increase is negligible |

---

## Success Criteria

1. ✅ All 266+ tests pass
2. ✅ No performance regression (±1% NPS)
3. ✅ Code is cleaner and more readable
4. ✅ Easy to add new per-ply fields
5. ✅ Prepares for Lazy SMP architecture

---

## References

- **Stockfish Stack struct:** `src/search.h` - Similar unified per-ply state
- **V1_ENGINE_ENHANCEMENT_PLAN.md** - Phase 3 (Lazy SMP), Phase 4 (Continuation History)
- **V1_ENGINE_STRENGTH_ROADMAP.md** - Overall architecture evolution

---

**Document Created:** 2026-02-09  
**Last Updated:** 2026-02-09  
**Author:** GitHub Copilot
