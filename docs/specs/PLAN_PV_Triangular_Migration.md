# Plan: Migration to Triangular PV Table

**Status:** ✅ Complete  
**Created:** 2026-02-06  
**Last Updated:** 2026-02-08  

---

## 1. Analysis: Current FrankyCPP PV Implementation

### 1.1 Data Structure

```cpp
// Search.h line 156
std::array<MoveList, DEPTH_MAX + 1> pv{};
```

Where `MoveList` is defined as:

```cpp
// movelist.h line 54
class MoveList : public std::vector<Move> { ... };
```

And `Move` is:

```cpp
// move.h line 85-92
class Move {
  using Raw = uint32_t;  // 4 bytes
  Raw raw_{0};
  static_assert(sizeof(Raw) == 4, "Move::Raw must remain 32-bit");
  ...
};
```

**Summary:** FrankyCPP uses a **fixed-size array of 128 dynamic vectors**. The outer array is stack-allocated, but each `MoveList` element inherits from `std::vector<Move>` which manages heap-allocated storage for its elements.

### 1.2 Memory Layout

```
pv[0]  → std::vector<Move> → [heap: e4, e5, Nf3, Nc6, Bb5]  (full PV from root)
pv[1]  → std::vector<Move> → [heap: e5, Nf3, Nc6, Bb5]      (full PV from ply 1)
pv[2]  → std::vector<Move> → [heap: Nf3, Nc6, Bb5]          (full PV from ply 2)
pv[3]  → std::vector<Move> → [heap: Nc6, Bb5]               (full PV from ply 3)
pv[4]  → std::vector<Move> → [heap: Bb5]                    (full PV from ply 4)
pv[5]  → std::vector<Move> → [heap: empty]                  (leaf)
...
pv[127] → std::vector<Move> → [heap: empty]
```

Each ply stores the **entire PV line from that ply onwards** as a separate heap allocation.

### 1.3 Current Update Mechanism

**`savePV()` function (Search.cpp ~line 1519):**

```cpp
void Search::savePV(Move move, MoveList& src, MoveList& dest) {
  dest.clear();
  dest.push_back(move);
  dest.insert(dest.end(), src.begin(), src.end());  // Copy entire child PV
}
```

**Called when a move improves alpha:**
- In `search()`: `savePV(move, pv[ply + 1], pv[ply]);`
- In `qsearch()`: `savePV(move, pv[ply + 1], pv[ply]);`
- In `rootSearch()`: `savePV(moveRef, pv[1], pv[0]);`

### 1.4 Current Clearing Mechanism

**At node entry:**
```cpp
pv[ply].clear();  // Clears vector contents, keeps capacity
```

**After IID (if executed):**
```cpp
pv[ply].clear();  // Second clear to remove IID pollution
```

### 1.5 Memory Characteristics

| Aspect | Current Implementation |
|--------|------------------------|
| Outer array | `std::array<MoveList, 128>` - stack/static, ~3KB metadata |
| Inner storage | `std::vector<Move>` - heap allocated per ply |
| Move size | 4 bytes (`uint32_t`) |
| Allocation | Dynamic - vectors grow/shrink on heap |
| Typical PV length | 10-40 moves |
| Memory per active ply | 24 bytes (vector metadata) + heap data |

### 1.6 Performance Characteristics

**Costs:**
1. **Heap allocations**: Each `push_back` may trigger reallocation
2. **Copy overhead**: `savePV()` copies entire child PV via `insert()`
3. **Cache unfriendly**: Vector data scattered across heap
4. **Redundant storage**: Same moves stored multiple times (pv[0] contains pv[1] which contains pv[2]...)

**Mitigating factors:**
- Vectors may retain capacity after `clear()`, reducing reallocations
- Small-string optimization doesn't apply (Move is 4 bytes, not a string)
- Modern allocators handle small allocations reasonably well

---

## 2. Triangular PV Table Pattern

### 2.1 Data Structure

```cpp
std::array<std::array<Move, MAX_PLY>, MAX_PLY> table_;  // Fixed 2D array, zero overhead
```

### 2.2 Memory Layout (Triangular Usage)

```
        Index:  0   1   2   3   4   5   ...
             +---+---+---+---+---+---+
pv[0]     →  | e4| e5|Nf3|Nc6|Bb5| · |  ← Root PV starts at [0][0]
             +---+---+---+---+---+---+
pv[1]     →  | · | e5|Nf3|Nc6|Bb5| · |  ← Ply 1 PV starts at [1][1]
             +---+---+---+---+---+---+
pv[2]     →  | · | · |Nf3|Nc6|Bb5| · |  ← Ply 2 PV starts at [2][2]
             +---+---+---+---+---+---+
pv[3]     →  | · | · | · |Nc6|Bb5| · |  ← Ply 3 PV starts at [3][3]
             +---+---+---+---+---+---+
pv[4]     →  | · | · | · | · |Bb5| · |  ← Ply 4 PV starts at [4][4]
             +---+---+---+---+---+---+
pv[5]     →  | · | · | · | · | · | · |  ← Empty (leaf)
             +---+---+---+---+---+---+
```

Only the upper-right triangle is used. `pv[ply][i]` for `i >= ply` contains the PV continuation.

### 2.3 Update Mechanism

```cpp
// At node entry - mark PV as empty
pv[ply][ply] = MOVE_NONE;

// When move improves alpha - copy child PV up
pv[ply][ply] = move;
for (int i = ply + 1; pv[ply + 1][i] != MOVE_NONE; ++i) {
    pv[ply][i] = pv[ply + 1][i];
}
pv[ply][i] = MOVE_NONE;  // Sentinel terminator
```

### 2.4 Memory Characteristics

| Aspect | Triangular PV Table |
|--------|----------------------|
| Structure | `std::array<std::array<Move, 128>, 128>` - single contiguous block |
| Move size | 2 bytes in some engines (16-bit), 4 bytes if we keep our Move |
| Total size | 128 × 128 × 4 = **64 KB** (with 32-bit Move) |
| Allocation | **Zero** - compile-time fixed |
| Cache behavior | **Excellent** - contiguous memory |
| Termination | `MOVE_NONE` sentinel |

### 2.5 Key Differences

| Aspect | FrankyCPP Current | Triangular PV Table |
|--------|-------------------|----------------------|
| Allocation | Dynamic (heap) | Static (none at runtime) |
| Copy method | `vector::insert` | Simple indexed loop |
| Clear method | `vector::clear()` | Single assignment |
| Memory layout | Scattered (heap) | Contiguous |
| Cache locality | Poor | Excellent |
| PV termination | `vector::size()` | Sentinel value |
| TT cutoff PV | `getPvLine()` reconstruction | No reconstruction |

---

## 3. Migration Plan

### 3.1 Phase 1: Create PVTable Class

**New File: `src/types/PVTable.h`**

Create a zero-overhead wrapper class for the triangular PV table:

```cpp
#ifndef FRANKYCPP_PVTABLE_H
#define FRANKYCPP_PVTABLE_H

#include "move.h"
#include "movelist.h"
#include <array>
#include <cstring>

/// Triangular PV Table - zero-overhead wrapper for PV storage
/// All methods inline/constexpr - compiler optimizes to direct array access
class PVTable {
public:
    static constexpr int MAX_PLY = DEPTH_MAX + 1;  // 128

private:
    std::array<std::array<Move, MAX_PLY>, MAX_PLY> table_{};

public:
    // =========================================================================
    // Direct Access - zero overhead (compiles to raw array access)
    // =========================================================================

    /// Direct access to move at [ply][index] - zero overhead
    [[nodiscard]] constexpr Move& operator()(Depth ply, int index) noexcept {
        return table_[ply][index];
    }
    [[nodiscard]] constexpr const Move& operator()(Depth ply, int index) const noexcept {
        return table_[ply][index];
    }

    // =========================================================================
    // Semantic Operations
    // =========================================================================

    /// Clear PV at given ply (O(1) - single assignment)
    constexpr void clear(Depth ply) noexcept {
        table_[ply][ply] = MOVE_NONE;
    }

    /// Clear entire table (called once at search start)
    void clearAll() noexcept {
        // std::array is contiguous, so memset works safely
        std::memset(table_.data(), 0, sizeof(table_));  // MOVE_NONE == 0
    }

    /// Update PV: prepend move and copy child PV from ply+1
    constexpr void update(Move move, Depth ply) noexcept {
        table_[ply][ply] = move;
        int i = ply + 1;
        while (table_[ply + 1][i] != MOVE_NONE && i < MAX_PLY) {
            table_[ply][i] = table_[ply + 1][i];
            ++i;
        }
        if (i < MAX_PLY) {
            table_[ply][i] = MOVE_NONE;
        }
    }

    // =========================================================================
    // Query Operations
    // =========================================================================

    /// Get first move at ply (equivalent to pv[ply][ply])
    [[nodiscard]] constexpr Move first(Depth ply = 0) const noexcept {
        return table_[ply][ply];
    }

    /// Check if PV at ply is empty
    [[nodiscard]] constexpr bool empty(Depth ply = 0) const noexcept {
        return table_[ply][ply] == MOVE_NONE;
    }

    /// Check if PV has at least minLength moves
    [[nodiscard]] constexpr bool hasLength(Depth ply, int minLength) const noexcept {
        for (int i = 0; i < minLength; ++i) {
            if (table_[ply][ply + i] == MOVE_NONE) return false;
        }
        return true;
    }

    /// Get PV length at ply
    [[nodiscard]] constexpr int length(Depth ply = 0) const noexcept {
        int len = 0;
        while (table_[ply][ply + len] != MOVE_NONE && ply + len < MAX_PLY) {
            ++len;
        }
        return len;
    }

    // =========================================================================
    // Extraction (for UCI output - only called at iteration end)
    // =========================================================================

    /// Extract PV as MoveList
    [[nodiscard]] MoveList extract(Depth ply = 0) const {
        MoveList result;
        for (int i = ply; table_[ply][i] != MOVE_NONE && i < MAX_PLY; ++i) {
            result.push_back(table_[ply][i]);
        }
        return result;
    }

    // =========================================================================
    // Low-level Access (for special cases like getPvLine reconstruction)
    // =========================================================================

    /// Direct row access (returns pointer to first element of row)
    [[nodiscard]] constexpr Move* row(Depth ply) noexcept {
        return table_[ply].data();
    }
    [[nodiscard]] constexpr const Move* row(Depth ply) const noexcept {
        return table_[ply].data();
    }
};

#endif // FRANKYCPP_PVTABLE_H
```

**Memory impact:** 128 × 128 × 4 = 64 KB (acceptable for a single Search instance)

**Zero-overhead guarantee:**
- All methods are `constexpr`/`inline`
- No virtual functions
- `operator()` compiles to direct array access: `pv_(ply, i)` → `table_[ply][i]`
- No heap allocations

---

### 3.2 Phase 2: Update Search.h

**File: `Search.h`**

Replace:
```cpp
std::array<MoveList, DEPTH_MAX + 1> pv{};
```

With:
```cpp
#include "types/PVTable.h"
// ...
PVTable pv_;
```

Update `getPV()` method:
```cpp
// Old:
const MoveList& getPV() const { return pv[0]; }

// New:
MoveList getPV() const { return pv_.extract(); }
```

Note: Return type changes from `const MoveList&` to `MoveList` (value). Check all callers.

---

### 3.3 Phase 3: Update Call Sites in `search()`

**File: `Search.cpp`**

| Current Code | New Code |
|--------------|----------|
| `pv[ply].clear();` | `pv_.clear(ply);` |
| `savePV(move, pv[ply + 1], pv[ply]);` | `pv_.update(move, ply);` |

**Locations in `search()`:**
1. Node entry clear (~line 699)
2. After IID clear (inside IID block, ~line 931)
3. Alpha improvement (~line 1197)

---

### 3.4 Phase 4: Update Call Sites in `qsearch()`

**File: `Search.cpp`**

| Current Code | New Code |
|--------------|----------|
| `pv[ply].clear();` | `pv_.clear(ply);` |
| `savePV(move, pv[ply + 1], pv[ply]);` | `pv_.update(move, ply);` |

**Locations in `qsearch()`:**
1. Node entry clear (~line 1244)
2. Alpha improvement (~line 1434)

---

### 3.5 Phase 5: Update Call Sites in `rootSearch()`

**File: `Search.cpp`**

| Current Code | New Code |
|--------------|----------|
| `savePV(moveRef, pv[1], pv[0]);` | `pv_.update(moveRef, 0);` |

**Location:** ~line 672

---

### 3.6 Phase 6: Update Initialization

**File: `Search.cpp` in `run()`**

Current (~line 196-199):
```cpp
for (int i = DEPTH_NONE; i < DEPTH_MAX; i++) {
    this->mg[i] = MoveGenerator{};
    if (SearchConfig.USE_HISTORY_COUNTER || SearchConfig.USE_HISTORY_MOVES) { this->mg[i].setHistoryData(&history); }
    pv[i].clear();
}
```

New:
```cpp
// Clear entire PV table once (uses memset internally)
pv_.clearAll();

for (int i = DEPTH_NONE; i < DEPTH_MAX; i++) {
    this->mg[i] = MoveGenerator{};
    if (SearchConfig.USE_HISTORY_COUNTER || SearchConfig.USE_HISTORY_MOVES) { this->mg[i].setHistoryData(&history); }
}
```

---

### 3.7 Phase 7: Update PV Access for UCI

**File: `Search.cpp`**

**`sendIterationEndInfoToUci()` (~line 1887):**

Current:
```cpp
uciHandler->sendIterationEndInfo(..., pv[0]);
```

New:
```cpp
uciHandler->sendIterationEndInfo(..., pv_.extract());
```

**`sendAspirationResearchInfo()` (~line 1961):**

Current:
```cpp
uciHandler->sendAspirationResearchInfo(..., pv[0]);
```

New:
```cpp
uciHandler->sendAspirationResearchInfo(..., pv_.extract());
```

**`getPV()` public method in Search.h:**

Current:
```cpp
const MoveList& getPV() const { return pv[0]; }
```

New:
```cpp
MoveList getPV() const { return pv_.extract(); }
```

Note: Return type changes from `const MoveList&` to `MoveList` (value). Check all callers.

---

### 3.8 Phase 8: Update `iterativeDeepening()` PV Access

**File: `Search.cpp`**

All accesses to `pv[0].at(0)`, `pv[0].at(1)`, `pv[0].size()`, etc. need updating:

| Current            | New                                               |
|--------------------|---------------------------------------------------|
| `pv[0].at(0)`      | `pv_.first()` or `pv_(0, 0)`                      |
| `pv[0].at(1)`      | `pv_(0, 1)`                                       |
| `pv[0].size() > 1` | `pv_.hasLength(0, 2)` or `pv_(0, 1) != MOVE_NONE` |
| `pv[0].empty()`    | `pv_.empty()`                                     |
| `!pv[ply].empty()` | `!pv_.empty(ply)`                                 |

**Locations (~lines 455, 456, 460, 479, 492-494, 508-509, 515):**
- Assertions checking `pv[0].at(0)`
- Best move extraction
- Ponder move extraction

---

### 3.9 Phase 9: Update `SearchResult`

**File: `SearchResult.h`**

Current:
```cpp
MoveList pv{};
```

**Options:**
1. Keep as-is - copy from PVTable at search end (current approach via `pv_.extract()`)
2. Change to store reference/pointer to PVTable (complicates lifetime)

**Recommendation:** Keep `MoveList pv{}` in SearchResult. Copy at search end:
```cpp
searchResult.pv = pv_.extract();
```

---

### 3.10 Phase 10: Remove TT Cutoff PV Reconstruction

**File: `Search.cpp`**

Remove `getPvLine()` call at TT cutoffs entirely.

Current (~line 756-759):
```cpp
getPvLine(p, pv[ply], depth);
statistics.TtCuts++;
return ttValue;
```

New:
```cpp
// No PV reconstruction - pv_(ply, ply) remains MOVE_NONE (cleared at node entry)
// Parent will have shorter PV, which is acceptable
statistics.TtCuts++;
return ttValue;
```

**Rationale:** Standard triangular PV approach. Simpler, faster, no risk of illegal moves from TT reconstruction. May result in shorter PV lines in UCI output, but this is an acceptable trade-off.

---

### 3.11 Phase 11: Remove Old Functions

**File: `Search.cpp`**

- Remove `savePV()` function entirely
- Remove `getPvLine()` function entirely (no longer needed)

---

### 3.12 Phase 12: Update Tests

**Files: `test/engine/*`**

Search for tests that:
- Access `pv[0]` directly
- Call `getPV()`
- Check PV contents

Update to work with new return type and access patterns.

---

### 3.13 Phase 13: Optimize `currentVariation` in SearchStats

**Issue:** `statistics.currentVariation` is a `MoveList` (inherits `std::vector<Move>`) that is modified in the hot path of search:

```cpp
// Called on EVERY move searched (millions of times)
statistics.currentVariation.push_back(move);  // In search(), qsearch(), rootSearch()
statistics.currentVariation.pop_back();        // After undoMove()
```

**Current Location:** `SearchStats.h` line 72
```cpp
MoveList currentVariation{};
```

**Problem:** While `push_back`/`pop_back` are O(1) amortized, this still involves:
- Bounds checking
- Potential reallocation (though unlikely after warmup)
- Cache misses from heap pointer chasing
- Called millions of times during search

**Solution:** Replace with a fixed-size stack class similar to PVTable:

**New class in `SearchStats.h` or separate file:**

```cpp
/// Fixed-size stack for tracking current search variation
/// Zero heap allocation, cache-friendly, called millions of times in hot path
class VariationStack {
public:
    static constexpr int MAX_PLY = DEPTH_MAX + 1;  // 128

private:
    std::array<Move, MAX_PLY> moves_{};
    int size_{0};

public:
    constexpr void push_back(Move m) noexcept { moves_[size_++] = m; }
    constexpr void pop_back() noexcept { --size_; }
    constexpr void clear() noexcept { size_ = 0; }
    
    [[nodiscard]] constexpr int size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    
    [[nodiscard]] constexpr Move& operator[](int i) noexcept { return moves_[i]; }
    [[nodiscard]] constexpr const Move& operator[](int i) const noexcept { return moves_[i]; }
    
    // Iterator support for range-based for loops
    constexpr Move* begin() noexcept { return moves_.data(); }
    constexpr Move* end() noexcept { return moves_.data() + size_; }
    constexpr const Move* begin() const noexcept { return moves_.data(); }
    constexpr const Move* end() const noexcept { return moves_.data() + size_; }
    
    // String output for UCI (only called periodically, not hot path)
    [[nodiscard]] std::string str() const {
        std::ostringstream os;
        for (int i = 0; i < size_; ++i) {
            if (i > 0) os << ' ';
            os << moves_[i];
        }
        return os.str();
    }
};
```

**Update SearchStats.h:**
```cpp
// Old:
MoveList currentVariation{};

// New:
VariationStack currentVariation{};
```

**Call sites remain unchanged** (same interface):
- `statistics.currentVariation.push_back(move);`
- `statistics.currentVariation.pop_back();`
- `statistics.currentVariation.clear();`

**Files Affected:**
- `SearchStats.h` - Replace `MoveList` with `VariationStack`
- `UciHandler.cpp` - May need to update `sendCurrentLine()` if it expects `MoveList`

**Memory:** 128 × 4 = 512 bytes (negligible)

**Expected Impact:** Eliminates heap operations in the innermost search loop.

---

## 4. File Change Summary

| File                                 | Changes                                                                      |
|--------------------------------------|------------------------------------------------------------------------------|
| `src/engine/PVTable.h`               | **New file** - PVTable class implementation ✅                                |
| `src/engine/VariationStack.h`        | **New file** - VariationStack class implementation ✅                         |
| `Search.h`                           | Replace `pv` with `pv_`, update `getPV()`, add include ✅                     |
| `Search.cpp`                         | All `pv[ply]` → `pv_.xxx()`, remove `savePV()`, remove `getPvLine()` ✅       |
| `SearchStats.h`                      | Replace `MoveList currentVariation` with `VariationStack currentVariation` ✅ |
| `SearchResult.h`                     | No change (keep `MoveList pv`)                                               |
| `UciHandler.h/cpp`                   | Update `sendCurrentLine()` to work with `VariationStack` ✅                   |
| `test/engine/PVTableTest.cpp`        | **New file** - Unit tests for PVTable ✅                                      |
| `test/engine/VariationStackTest.cpp` | **New file** - Unit tests for VariationStack ✅                               |

---

## 5. Testing Strategy

### 5.1 Unit Tests

**New test file: `test/engine/PVTableTest.cpp`** ✅

- Test `clear()`, `update()`, `extract()`, `length()`
- Test `operator()` direct access
- Test `first()`, `empty()`, `hasLength()`
- Test PV construction at various depths
- Test sentinel termination
- Test `clearAll()`

**New test file: `test/engine/VariationStackTest.cpp`** ✅

- Test `push_back()`, `pop_back()`, `clear()`
- Test `size()`, `empty()`
- Test `operator[]` access
- Test iterator support
- Test `str()` output

### 5.2 Integration Tests
- Verify UCI PV output matches expected lines
- Compare search results before/after migration
- Benchmark performance improvement

### 5.3 Regression Tests
- Run full test suite
- Play test games against previous version
- Verify identical move selection (when deterministic)

---

## 6. Estimated Effort

| Phase                                   | Effort         | Risk   |
|-----------------------------------------|----------------|--------|
| 1. Create PVTable class                 | 30 min         | Low    |
| 2. Update Search.h                      | 15 min         | Low    |
| 3-5. Update search/qsearch/root         | 1 hour         | Medium |
| 6. Initialization                       | 15 min         | Low    |
| 7. UCI output                           | 30 min         | Low    |
| 8. iterativeDeepening access            | 45 min         | Medium |
| 9. SearchResult                         | 15 min         | Low    |
| 10. TT cutoff handling                  | 30 min         | Medium |
| 11. Remove old functions                | 15 min         | Low    |
| 12. Update tests                        | 1 hour         | Medium |
| 13. VariationStack for currentVariation | 30 min         | Low    |
| **Total**                               | **~6-7 hours** |        |

---

## 7. Expected Benefits

1. **Performance**: Elimination of heap allocations during search
2. **Cache efficiency**: Contiguous memory access
3. **Simplicity**: No dynamic memory management
4. **Correctness**: No risk of illegal moves from TT PV reconstruction
5. **Alignment**: Matches industry-standard approach used by top engines

---

## 8. Design Decisions

1. **Keep 32-bit moves in PV table** (not 16-bit like some engines)
   - Stripping/reconstructing sort value costs more CPU than we'd save in memory
   - Modern CPUs handle 32-bit access as efficiently as 16-bit
   - Memory impact (64KB vs 32KB) is negligible

2. **Remove `getPvLine()` entirely (Option A)**
   - No PV reconstruction at TT cutoffs
   - Simpler, faster, no risk of illegal moves
   - May result in shorter PV lines in UCI output - acceptable trade-off
   - Will monitor in UI; can revisit if it impacts user experience significantly

3. **`getPV()` returns by value**
   - Simple and correct
   - Only called at iteration end, not performance-critical

---

## 9. References

- Chess Programming Wiki: [Triangular PV-Table](https://www.chessprogramming.org/Triangular_PV-Table)

---

## Changelog

| Date       | Change                                                                                      |
|------------|---------------------------------------------------------------------------------------------|
| 2026-02-06 | Initial planning document created                                                           |
| 2026-02-07 | Added Phase 13: VariationStack for currentVariation optimization                            |
| 2026-02-08 | Implementation complete: PVTable, VariationStack, Search updates, tests                     |
| 2026-02-08 | Added `!isPv` to TT cutoff condition - PV nodes always search fully for complete PV lines   |
| 2026-02-08 | Added `extractPvWithTT()` to extend PV using TT lookups for full PV lines in UCI output     |
