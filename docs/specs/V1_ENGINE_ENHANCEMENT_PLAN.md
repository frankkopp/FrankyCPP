# FrankyCPP v1.x Engine Enhancement Plan

**Document Version:** 1.5
**Created:** 2026-02-01
**Last Updated:** 2026-03-06
**Status:** Phases 1, 2, 3, 5, 6 Complete. Phase 4 Partial.
**Target:** FrankyCPP v1.5+ releases

---

## Executive Summary

This document outlines a comprehensive plan for enhancing FrankyCPP's playing strength through systematic improvements to search, evaluation, and supporting infrastructure. The plan is organized into logical phases, each building on previous work while maintaining the engine's stability and production quality.

**Current State (v1.5 Dev):**
- Production-ready classical chess engine
- Alpha-beta search with modern pruning (NMP, LMR, futility, razoring)
- Classical evaluation (material, PST, pawn structure, mobility, king safety)
- **Multi-threaded search (Lazy SMP)** implemented in v1.4 (+119 ELO vs v1.3)
- **TT Buckets** (4-way associative, cache-line aligned) implemented in v1.5
- **XOR Key Verification** for torn-read detection (SMP race safety)
- **Endgame Tablebase Support (Syzygy)** integrated in v1.2
- **Search Optimizations** (LMR, History, PVS fixes) integrated in v1.3 (+109 ELO vs v1.1)
- 100+ configurable parameters via YAML
- Comprehensive test suite (266+ tests)
- Cross-platform support (Windows/Linux)

**Target State (v1.x):**
- ~~Multi-threaded parallel search (Lazy SMP)~~ ✅ Complete (v1.4)
- Neural network evaluation (NNUE) with classical fallback
- Automated parameter tuning infrastructure
- Improved move ordering and time management
- Significant ELO gain (+300-500 estimated)

---

## Enhancement Categories

### 1. Search Enhancements
Improvements to the alpha-beta search algorithm, pruning, and extensions.

### 2. Evaluation Enhancements
Moving from classical evaluation to NNUE while maintaining evaluation quality.

### 3. Performance & Parallelization
Multi-threaded search and performance optimizations.

### 4. Infrastructure & Tooling
Supporting tools for tuning, testing, and benchmarking.

### 5. Endgame & Specialized Knowledge
Tablebase support and endgame-specific techniques.

---

## Phase-Based Roadmap

### Phase 1: Strength Testing Infrastructure (v1.1) - **1-2 weeks** ✅ COMPLETE
**Focus:** Establish automated strength testing framework for validating improvements

| Task                       | Category       | Effort      | Complexity | ELO Gain | Priority | Status     |
|----------------------------|----------------|-------------|------------|----------|----------|------------|
| Arena Integration          | Infrastructure | 🟢 2-3 days | 🟡 Medium  | N/A      | CRITICAL | ✅ Complete |
| Automated Match Runner     | Infrastructure | 🟢 2-3 days | 🟡 Medium  | N/A      | HIGH     | ✅ Complete |
| ELO Tracking & Statistics  | Infrastructure | 🟢 1-2 days | 🟢 Low     | N/A      | HIGH     | ✅ Complete |
| Opponent Engine Collection | Infrastructure | 🟢 1-2 days | 🟢 Low     | N/A      | MEDIUM   | ✅ Complete |

**Expected Total ELO Gain:** N/A (infrastructure)  
**Risk:** Low - setup and configuration work  
**Notes:**
- Arena GUI for interactive testing and tournament management
- cutechess-cli for automated batch matches
- Collection of calibrated opponent engines (various ELO levels)
- Scripts for result parsing and ELO calculation
- **Completed in v1.1** - Foundation for measuring all future improvements

---

### Phase 2: Performance Fundamentals & Quick Wins (v1.2) - **2-3 weeks** ✅ COMPLETE
**Focus:** Eliminate heap allocations, improve cache performance, add proven search enhancements

| Task                            | Category       | Effort      | Complexity | ELO Gain | Priority | Status     |
|---------------------------------|----------------|-------------|------------|----------|----------|------------|
| **Performance Fundamentals**    |                |             |            |          |          |            |
| Triangular PV Table             | Performance    | 🟢 1-2 days | 🟡 Medium  | +5-10    | HIGH     | ✅ Complete |
| MoveList Static Array Refactor  | Performance    | 🟡 3-5 days | 🟡 Medium  | +5-15    | HIGH     | ✅ Complete |
| **Infrastructure**              |                |             |            |          |          |            |
| UCI `bench` Command             | Infrastructure | 🟢 2-3 days | 🟢 Low     | N/A      | HIGH     | ✅ Complete |
| Arena Bench Integration         | Infrastructure | 🟢 2-3 days | 🟢 Low     | N/A      | MEDIUM   | ✅ Complete |
| **Search Quick Wins**           |                |             |            |          |          |            |
| Singular Extensions             | Search         | 🟢 2-3 days | 🟡 Medium  | +20-30   | HIGH     | ✅ Complete |
| Check Extensions                | Search         | 🟢 2-3 days | 🟡 Medium  | +10-20   | HIGH     | ✅ Complete |
| Counter-Move History            | Search         | 🟡 3-5 days | 🟡 Medium  | +10-20   | LOW      | ✅ Complete |
| Best-Move Instability Time Mgmt | Search         | 🟢 2-3 days | 🟡 Medium  | +5-15    | MEDIUM   | ✅ Complete |
| Selective Checks in Quiescence  | Search         | 🟡 3-5 days | 🟡 Medium  | +15-25   | MEDIUM   | 📋 Planned |

**Expected Total ELO Gain:** +70-135  
**Risk:** Low - internal refactoring and proven techniques  
**Notes:**
- Triangular PV Table: Replace `std::array<MoveList, DEPTH_MAX+1>` with fixed 2D array ✅
- MoveList/VariationStack: Unified into `StaticMoveList<N>` template with zero heap allocations ✅
- **`bench` command:** ✅ UCI benchmark for NPS measurement using 50 curated positions
  - UCI command: `bench [depth] [hash] [threads]` (default: `bench 10 128 1`)
  - Command-line: `FrankyCPP --bench --benchDepth 10 --benchHash 128`
  - Clears TT before each position for fair, independent measurement
  - Measures only search time (excludes TT clearing overhead)
  - Results stored in `results/benchmarks/benchmarks.json`
- **Arena Bench Integration:** ✅ Configured via `arena.yaml`, supports internal and external engines
  - Arena CLI: `FrankyCPP_Arena --bench` or `-b`
  - History report: `FrankyCPP_Arena --bench-report`
  - External engines use UCI protocol with same 50 positions
  - Results table shows version, depth, hash, nodes, NPS, time
- Zero heap allocations during search
- Better cache locality
- Foundation for multi-threading (simpler per-thread state)
- Detailed plans: `docs/specs/PLAN_PV_Triangular_Migration.md`, `docs/specs/PLAN_Speedtest_Benchmark.md`

---

### Phase 3: Multi-Threading (v1.4-v1.5) - **3-4 weeks** ✅ COMPLETE
**Focus:** Parallel search for multi-core CPUs (Lazy SMP)

| Task                                      | Category    | Effort      | Complexity | ELO Gain | Priority | Status     |
|-------------------------------------------|-------------|-------------|------------|----------|----------|------------|
| Step 1: Thread-Safe TT (atomic key)       | Performance | 🟢 2-3 days | 🟡 Medium  | N/A      | CRITICAL | ✅ Complete |
| Step 2: `SearchThread` struct + refactor  | Architect.  | 🟡 3-5 days | 🟡 Medium  | N/A      | CRITICAL | ✅ Complete |
| Step 3: Helper thread launch/join         | Performance | 🟡 3-5 days | 🟡 Medium  | +50-100  | CRITICAL | ✅ Complete |
| Step 4: `Threads` UCI option + config     | Config      | 🟢 1-2 days | 🟢 Low     | N/A      | HIGH     | ✅ Complete |
| Step 5: Node count aggregation            | UCI         | 🟢 1-2 days | 🟢 Low     | N/A      | MEDIUM   | ✅ Complete |
| Step 6: Testing & strength validation     | Testing     | 🟡 5-7 days | 🟡 Medium  | +10-20   | HIGH     | ✅ Complete |
| Step 7: TT Buckets (v1.5)                 | Performance | 🟡 3-5 days | 🟡 Medium  | +5-10    | HIGH     | ✅ Complete |
| Step 8: XOR Key Verification (v1.5)       | Safety      | 🟢 1-2 days | 🟡 Medium  | N/A      | HIGH     | ✅ Complete |

**Expected Total ELO Gain:** +60-120 (on multi-core hardware) ✅ **Verified: +119 ELO (v1.4 vs v1.3)**
**Risk:** Medium — TT atomic key is zero-overhead on x86; overall approach is minimal and well-proven
**Design Constraint:** `Threads=1` must have **zero overhead** vs pre-SMP (no atomics on hot path, no extra threads created)
**Detailed Plan:** `docs/specs/PLAN_Lazy_SMP_MultiThreading.md`

**v1.4 Implementation:**
- Strategy: Lazy SMP — helpers independently run `iterativeDeepening()`, share only the TT
- Comprehensive test suite (`SearchSmpTest.cpp`)
- TT thread-safety via `std::atomic<ZobristKey>` key field (acquire/release memory order)
- New `SearchThreadData` struct holds all per-thread state (PV table, ply stack, history, statistics)
- Helper threads do NOT manage time, report UCI, or select the final best move (main thread only)
- PawnTT shared across threads with lock-free concurrent access

**v1.5 Enhancements:**
- **TT Bucket Design:** 4-way associative with 64-byte cache-line alignment
- **XOR Key Verification:** Torn-read detection for SMP race safety
  - Store: key stored as `(originalKey ^ dataHash)` after writing data
  - Load: verify `(storedKey ^ dataHash) == probeKey` to detect corruption
- **Cache-line alignment:** Eliminates false sharing under SMP
- **Replacement policy:** Depth-preferred + age tiebreak
- **Single prefetch:** Loads entire bucket (4 entries)
- **Race condition fixes:** All remaining SMP races eliminated

---

### Phase 4: Enhanced Move Ordering (v1.3 - v1.5) - **Ongoing** 🔄 PARTIAL
**Focus:** Better move ordering for deeper effective search

| Task                             | Category | Effort      | Complexity | ELO Gain | Priority | Status            |
|----------------------------------|----------|-------------|------------|----------|----------|-------------------|
| Capture History Heuristic        | Search   | 🟡 3-5 days | 🟡 Medium  | +10-20   | HIGH     | 📋 Planned        |
| Continuation History             | Search   | 🟡 3-5 days | 🟡 Medium  | +15-25   | HIGH     | 📋 Planned        |
| Static Exchange Eval Enhancement | Search   | 🟢 2-3 days | 🟡 Medium  | +5-10    | MEDIUM   | 📋 Planned        |
| Killer Move Slot Optimization    | Search   | 🟢 1-2 days | 🟢 Low     | +5-10    | LOW      | ✅ Complete        |
| History Heuristic Fixes          | Search   | 🟢 1-2 days | 🟢 Low     | +20-30   | HIGH     | ✅ Complete (v1.3) |
| Counter-Move History             | Search   | 🟡 3-5 days | 🟡 Medium  | +10-20   | HIGH     | ✅ Complete (v1.2) |

**Expected Total ELO Gain:** +35-65  
**Risk:** Low - proven techniques with minimal architectural impact

---

### Phase 5: Endgame Tablebases (v1.2) - **Done** ✅ COMPLETE
**Focus:** Perfect endgame play with Syzygy tablebases

| Task                       | Category | Effort      | Complexity | ELO Gain | Priority | Status     |
|----------------------------|----------|-------------|------------|----------|----------|------------|
| Fathom Library Integration | Endgame  | 🟢 2-3 days | 🟡 Medium  | N/A      | HIGH     | ✅ Complete |
| Root Tablebase Probing     | Endgame  | 🟢 2-3 days | 🟡 Medium  | +20-30   | HIGH     | ✅ Complete |
| Search Tablebase Probing   | Endgame  | 🟡 1 week   | 🟡 Medium  | +10-20   | MEDIUM   | ✅ Complete |
| TB Configuration & Testing | Endgame  | 🟢 2-3 days | 🟢 Low     | N/A      | MEDIUM   | ✅ Complete |

**Expected Total ELO Gain:** +35-60 (in TB-relevant endgames)  
**Risk:** Low - well-defined API (Fathom)

---

### Phase 6: Advanced Search Refinements (v1.3 - v1.4) - **Ongoing** 🔄 PARTIAL
**Focus:** Modern search techniques for deeper tactical vision

| Task                 | Category | Effort      | Complexity | ELO Gain | Priority | Status            |
|----------------------|----------|-------------|------------|----------|----------|-------------------|
| Multi-Cut Pruning    | Search   | 🟡 3-5 days | 🟡 Medium  | +10-20   | HIGH     | 📋 Planned        |
| Probcut Pruning      | Search   | 🟡 3-5 days | 🟡 Medium  | +10-15   | MEDIUM   | 📋 Planned        |
| Late Move Pruning    | Search   | 🟢 2-3 days | 🟡 Medium  | +10-15   | MEDIUM   | ✅ Complete        |
| Improved LMR Formula | Search   | 🟡 1 week   | 🟡 Medium  | +15-25   | HIGH     | ✅ Complete (v1.3) |
| PV Node Fixes        | Search   | 🟢 2-3 days | 🔴 High    | +20-30   | CRITICAL | ✅ Complete (v1.3) |

**Expected Total ELO Gain:** +50-85  
**Risk:** Medium - requires careful tuning to avoid search instability

---

### Phase 7: Automated Tuning Infrastructure (v1.7) - **2-3 weeks**
**Focus:** Scientific parameter optimization

| Task                      | Category       | Effort       | Complexity | ELO Gain | Priority |
|---------------------------|----------------|--------------|------------|----------|----------|
| SPSA Framework            | Infrastructure | 🟡 1 week    | 🟡 Medium  | N/A      | HIGH     |
| Texel Tuning Engine       | Infrastructure | 🟡 1 week    | 🟡 Medium  | N/A      | HIGH     |
| Automated Match Runner    | Infrastructure | 🟡 3-5 days  | 🟡 Medium  | N/A      | MEDIUM   |
| Parameter Tuning Sessions | Infrastructure | 🔴 2-4 weeks | 🟡 Medium  | +30-60   | HIGH     |

**Expected Total ELO Gain:** +30-60 (from optimized parameters)  
**Risk:** Low - infrastructure project  
**Notes:**
- SPSA: Simultaneous Perturbation Stochastic Approximation
- Texel: Supervised learning from self-play games
- Critical for optimizing 100+ existing parameters

---

### Phase 8: NNUE Evaluation (v1.8 → v2.0) - **6-10 weeks**
**Focus:** Neural network evaluation for dramatic strength gain

| Task                       | Category   | Effort       | Complexity | ELO Gain | Priority |
|----------------------------|------------|--------------|------------|----------|----------|
| NNUE Architecture Design   | Evaluation | 🟡 1 week    | 🔴 High    | N/A      | CRITICAL |
| Incremental Update System  | Evaluation | 🔴 2-3 weeks | 🔴 High    | N/A      | CRITICAL |
| NNUE Inference Engine      | Evaluation | 🟡 1-2 weeks | 🔴 High    | N/A      | CRITICAL |
| Training Data Generation   | Evaluation | 🟡 1 week    | 🟡 Medium  | N/A      | HIGH     |
| Network Training Pipeline  | Evaluation | 🟡 1-2 weeks | 🔴 High    | N/A      | HIGH     |
| NNUE Integration & Testing | Evaluation | 🟡 1 week    | 🔴 High    | N/A      | HIGH     |
| Classical/NNUE Hybrid Mode | Evaluation | 🟢 2-3 days  | 🟡 Medium  | N/A      | MEDIUM   |

**Expected Total ELO Gain:** +200-400  
**Risk:** Very High - major architectural change  
**Notes:**
- Largest single strength improvement possible
- Requires ML expertise and significant compute for training
- Can leverage existing NNUE frameworks (Bullet, NNUE-pytorch)
- Consider starting with existing trained nets (HalfKP architecture)

---

### Phase 9: CPU Compatibility & Optimization (v2.0) - **1-2 weeks**
**Focus:** Broader hardware support and performance optimization

| Task                              | Category    | Effort      | Complexity | ELO Gain | Priority |
|-----------------------------------|-------------|-------------|------------|----------|----------|
| Runtime PEXT Detection            | Performance | 🟢 2-3 days | 🟡 Medium  | N/A      | HIGH     |
| Software PEXT Fallback            | Performance | 🟡 3-5 days | 🟡 Medium  | N/A      | HIGH     |
| SIMD Optimization (AVX2/AVX-512)  | Performance | 🟡 1 week   | 🔴 High    | +10-20   | MEDIUM   |
| Profile-Guided Optimization (PGO) | Performance | 🟢 2-3 days | 🟢 Low     | +5-15    | MEDIUM   |

**Expected Total ELO Gain:** +15-35 (performance, not tactical)  
**Risk:** Medium - hardware compatibility critical  
**Notes:**
- Current PEXT requirement excludes pre-2013 CPUs
- Software fallback maintains functionality with ~20% performance penalty

---

## Detailed Enhancement Specifications

### 1. Strength Testing Infrastructure (Phase 1)

**Description:**  
Establish a comprehensive strength testing framework to validate ELO gains from all future enhancements. This is the foundation for evidence-based development.

**Components:**

#### Arena GUI Integration
- Chess GUI for interactive testing and tournament management
- Visual analysis of games
- Engine-vs-engine matches with various time controls

#### cutechess-cli for Automated Matches
- Command-line tool for batch game execution
- Support for gauntlets (one engine vs many opponents)
- Round-robin tournaments
- PGN output for analysis

#### Opponent Engine Collection
Calibrated engines at various ELO levels:
- **Weak (1200-1600):** Stockfish with skill level limits
- **Intermediate (1800-2200):** Older engine versions
- **Strong (2400-2600):** Modern engines without NNUE
- **Elite (2800+):** Latest Stockfish (for ceiling testing)

#### ELO Tracking & Statistics
- Bayesian ELO calculation from match results
- Confidence intervals for estimates
- Regression detection (comparing versions)
- Historical tracking of improvements

**Configuration Example:**
```bash
# Run 100-game match with cutechess-cli
cutechess-cli \
  -engine name=FrankyCPP_v1.1 cmd=./FrankyCPP_v1.1 \
  -engine name=FrankyCPP_v1.0 cmd=./FrankyCPP_v1.0 \
  -each tc=60+0.5 proto=uci \
  -games 100 -rounds 1 -pgnout results.pgn
```

**Success Criteria:**
- Can run 100+ game matches automatically
- ELO estimates have < ±30 confidence interval
- Results are reproducible and logged
- Automated regression detection working

**Expected Impact:** N/A (infrastructure - enables measurement of all future improvements)

---

### 2a. Triangular PV Table (Phase 2)

**Description:**  
Replace the current `std::array<MoveList, DEPTH_MAX+1>` PV storage with a fixed triangular 2D array. Eliminates heap allocations and improves cache locality.

**Current Implementation:**
```cpp
std::array<MoveList, DEPTH_MAX + 1> pv{};  // MoveList inherits from std::vector<Move>
```

**New Implementation:**
```cpp
class PVTable {
    std::array<std::array<Move, MAX_PLY>, MAX_PLY> table_{};
public:
    constexpr Move& operator()(Depth ply, int index) noexcept;
    constexpr void clear(Depth ply) noexcept;
    void clearAll() noexcept;
    constexpr void update(Move move, Depth ply) noexcept;
    MoveList extract(Depth ply = 0) const;
    // ... other methods
};
```

**Key Changes:**
- Zero-overhead wrapper class with `constexpr`/`inline` methods
- `operator()` for direct array access: `pv_(ply, index)`
- Semantic methods: `clear()`, `update()`, `extract()`
- 64KB fixed memory (128 × 128 × 4 bytes)
- No heap allocations during search

**Files Affected:**
- `src/types/PVTable.h` (new file)
- `src/engine/Search.h`
- `src/engine/Search.cpp`

**Testing:**
- Unit tests for PVTable class
- Verify UCI PV output unchanged
- Benchmark NPS improvement

**Expected Impact:** +5-10 ELO (from improved NPS)

**Detailed Plan:** `docs/specs/PLAN_PV_Triangular_Migration.md`

---

### 2b. MoveList Static Array Refactor (Phase 2) ✅ COMPLETE

**Description:**  
Replace `std::vector<Move>` inheritance in MoveList with a unified `StaticMoveList<N>` template.
Also replaces the separate `VariationStack` class. Eliminates heap allocations in MoveGenerator
and other hot paths.

**Previous Implementation:**
```cpp
// MoveList - heap allocation on first push_back()
class MoveList : public std::vector<Move> { ... };

// VariationStack - separate fixed-size class
class VariationStack {
    std::array<Move, 128> moves_{};
    int size_{0};
    ...
};
```

**New Implementation:**
```cpp
// Unified template in src/types/staticmovelist.h
template<size_t Capacity>
class StaticMoveList {
    static constexpr size_t MAX_SIZE = Capacity;
    std::array<Move, Capacity> data_{};
    size_t size_{0};
public:
    constexpr void push_back(Move m) noexcept;
    constexpr void pop_back() noexcept;
    constexpr void clear() noexcept;
    constexpr void reserve(size_t) noexcept;  // No-op for backward compatibility
    
    [[nodiscard]] constexpr size_t size() const noexcept;
    [[nodiscard]] constexpr bool empty() const noexcept;
    [[nodiscard]] static constexpr size_t capacity() noexcept;
    
    [[nodiscard]] constexpr Move& operator[](size_t i) noexcept;
    [[nodiscard]] constexpr Move& at(size_t i);  // With bounds checking
    [[nodiscard]] constexpr Move& front() noexcept;
    [[nodiscard]] constexpr Move& back() noexcept;
    
    // STL-compatible iterators for range-based for and std::ranges algorithms
    [[nodiscard]] constexpr Move* begin() noexcept;
    [[nodiscard]] constexpr Move* end() noexcept;
    
    [[nodiscard]] std::string str() const;
    [[nodiscard]] std::string strVerbose() const;
};

// Type aliases
using MoveList = StaticMoveList<256>;        // For move generation
using VariationStack = StaticMoveList<128>;  // For search variation (MAX_PLY)
```

**Key Changes:**
- Unified template replaces both MoveList and VariationStack
- MoveList: Fixed 1KB (256 × 4 bytes) - covers all legal move scenarios
- VariationStack: Fixed 512 bytes (128 × 4 bytes) - MAX_PLY capacity
- Zero heap allocations
- STL-compatible interface preserved (iterators, std::ranges support)
- Debug asserts on capacity overflow
- `at()` provides bounds-checked access (throws std::out_of_range)

**Files Changed:**
- `src/types/staticmovelist.h` - New unified template (created)
- `src/types/movelist.h` - Deleted (was std::vector-based)
- `src/engine/VariationStack.h` - Deleted (merged into StaticMoveList)
- `src/types/types.h` - Updated to include staticmovelist.h
- `src/engine/SearchStats.h` - Updated to include staticmovelist.h
- `src/engine/UciHandler.h` - Updated to include staticmovelist.h
- `src/chesscore/MoveGenerator.cpp` - Removed reserve() calls
- `test/types/StaticMoveListTest.cpp` - New comprehensive test suite
- `test/engine/VariationStackTest.cpp` - Updated to use staticmovelist.h and MAX_SIZE

**Migration Notes:**
- `VariationStack::MAX_PLY` renamed to `VariationStack::MAX_SIZE`
- `size()` returns `size_t` instead of `int` (VariationStack change)
- `reserve()` method removed (not needed with fixed capacity)
- Include `types/staticmovelist.h` instead of old headers

**Testing:**
- Unit tests for StaticMoveList template (StaticMoveListTest.cpp)
- VariationStack tests updated and passing
- MoveGenerator, Search, and UCI functionality verified
- std::ranges::stable_sort and std::ranges::for_each compatibility tested

**Expected Impact:** +5-15 ELO (from improved NPS and cache efficiency)

---

### 2c. Singular Extensions (Phase 2) ✅ COMPLETE

**Description:**  
When a move is significantly better than all alternatives (by a margin), extend its search depth by 1 ply. This prevents horizon effects in critical tactical lines.

**Implementation:**
1. When processing the TT move in the move loop
2. If depth >= SINGULAR_MIN_DEPTH and TT has a valid value from similar depth
3. Do a reduced-depth null-window search excluding the TT move
4. If no other move beats (ttValue - singularMargin), extend the TT move by 1 ply

**Configuration Parameters:**
```yaml
USE_SINGULAR_EXT: true
SINGULAR_MARGIN: 64           # centipawns
SINGULAR_MIN_DEPTH: 8         # plies
SINGULAR_REDUCTION: 4         # plies for verification search
```

**Files Changed:**
- `src/engine/config/SearchConfigData.h` - Added configuration parameters
- `src/engine/Search.h` - Added `mgSingular` array and `excludedMove` array for per-ply tracking
- `src/engine/Search.cpp` - Implemented singular extension logic, uses mgSingular to avoid MG state corruption
- `src/engine/SearchStats.h` - Added `singularSearches` and `singularExtension` statistics
- `src/engine/UciOptions.cpp` - Added UCI options for singular extension parameters
- `config/search.yaml` - Added singular extension configuration
- `test/engine/SearchTest.cpp` - Added unit tests
- `src/enginetest/SearchTreeSizeTest.cpp` - Added to tree size test sequence

**Implementation Details:**
- Uses separate `mgSingular[ply]` MoveGenerator array for verification searches
- This prevents corruption of outer search's MoveGenerator state (singular search runs at same ply)
- `excludedMove[ply]` array tracks which move to skip during verification
- MoveGenerator selection: `excludedMove[ply] != MOVE_NONE ? mgSingular[ply] : mg[ply]`
- Verification search uses `No_Null_Move` to avoid NMP interference
- Only triggers when TT entry depth is within 3 plies of current depth

**Future Improvement:** See `docs/specs/PLAN_PlyStack_Refactoring.md` for planned unification of per-ply state into a `PlyInfo` struct.

**Testing:**
- Unit tests verify singular extension triggers and disabled state
- SearchTreeSizeTest includes singular extension in test sequence
- Statistics tracking for searches and extensions applied
- **Arena Results (v1.2 vs v1.1):**
  - Match: +27 ELO (53.8% score, 32W/48D/24L over 104 games) ✅
  - Test suites: -1.3% (within noise margin, expected trade-off)

**Expected Impact:** +20-30 ELO ✅ **Verified: +27 ELO**

---

### 2d. Check Extensions (Phase 2) ✅ COMPLETE

**Description:**  
Extend search by 1 ply when a move gives check. Helps find forcing sequences (checks, captures) more reliably.

**Implementation (Optimized - Early Move Limit):**
Instead of counting opponent's legal replies (expensive with lazy move generation), we leverage move ordering:
1. Moves are sorted by quality: TT move → captures → killers → history
2. Checks appearing early in move order are more likely to be tactically important
3. Only extend checks in the first N moves per node (default: 3)
4. Late-ordered checks (often weak/quiet) don't benefit from extension

This approach provides:
- Zero overhead (just one integer comparison)
- Natural tree size control without explicit depth limits
- Focus on tactically important checks

**Configuration Parameters:**
```yaml
USE_CHECK_EXT: true
CHECK_EXT_EARLY_LIMIT: 3      # only extend checks in first N moves per node
                               # set to 999 to extend all checks regardless
```

**Files Modified:**
- `src/engine/config/SearchConfigData.h` - added `CHECK_EXT_EARLY_LIMIT` parameter
- `src/engine/Search.cpp` - updated extension logic with early move limit
- `src/engine/UciOptions.cpp` - added UCI spin option (range 1-10)
- `config/search.yaml` - added configuration parameter

**Testing & Results:**
- Test Suite: -35 positions (-1.2%) - minor regression in puzzle solving
- Match Play: +30 ELO improvement (combined with singular extensions: +57 ELO total)
- Node count increased ~26% at fixed depth, but stronger in time-limited play

**Arena Results (v1.2 vs v1.1 - Combined Singular + Check Extensions):**

| Metric     | Result                 |
|------------|------------------------|
| Games      | 104                    |
| Score      | 58.2%                  |
| W/D/L      | 38/45/21               |
| ELO        | +57                    |
| Test Suite | -1.2% (2873 positions) |

**Expected Impact:** +10-20 ELO ✅ **Verified: ~+30 ELO**

---

### 2e. Best-Move Instability Time Management (Phase 2) ✅ COMPLETE

**Description:**  
Dynamically adjust search time based on how stable the best move is across iterations. When the best move changes frequently, the position is likely critical or complex, warranting more search time. When the best move remains stable for several consecutive iterations, search can be stopped earlier to save time for later moves.

**Concept:**
- Track the best move from each completed iteration
- Count consecutive iterations where the best move has been stable
- Count how many times the best move has changed during the search
- Extend time when instability is detected (many best-move changes)
- Reduce time when stability is confirmed (same move for several iterations)

**Implementation:**
```cpp
// In iterative deepening loop:
if (iterationDepth >= INSTABILITY_MIN_DEPTH) {
    if (bestRootMove != prevBestRootMove) {
        bestMoveStableCount = 0;
        bestMoveChangeCount++;
    } else {
        bestMoveStableCount++;
    }
    prevBestRootMove = bestRootMove;
    
    // Extend time if best move is unstable
    if (bestMoveChangeCount >= INSTABILITY_CHANGE_THRESHOLD && !addedInstabilityExtraTime) {
        addExtraTime(INSTABILITY_EXTEND_FACTOR);
        addedInstabilityExtraTime = true;
    }
    
    // Stop early if best move is stable
    if (bestMoveStableCount >= INSTABILITY_STABLE_COUNT) {
        if (getElapsedSearchTime() > softTimeLimit * INSTABILITY_STABLE_FACTOR) {
            stopSearch();
        }
    }
}
```

**Configuration Parameters:**
```yaml
USE_BESTMOVE_INSTABILITY: true
INSTABILITY_MIN_DEPTH: 8          # Minimum depth before tracking instability
INSTABILITY_STABLE_COUNT: 5       # Iterations of stability before early stop
INSTABILITY_CHANGE_THRESHOLD: 3   # Best-move changes to trigger time extension
INSTABILITY_STABLE_FACTOR: 0.9    # Early stop after this fraction of soft limit
INSTABILITY_EXTEND_FACTOR: 1.25   # Time multiplier when unstable (+25%)
```

**Relationship to Existing Volatility Detection:**
This feature complements the existing eval volatility detection (which triggers on large evaluation swings between iterations). Best-move instability focuses on the actual move changing, while eval volatility focuses on score fluctuations. Both can occur independently:
- Best move stable but eval volatile: Unclear position, same best move but uncertain value
- Best move unstable but eval stable: Multiple equally-valued options competing
- Both unstable: Critical position requiring extra search time

**Files Changed:**
- `src/engine/config/SearchConfigData.h` - Added 6 configuration parameters
- `src/engine/Search.h` - Added tracking variables: `bestMoveStableCount`, `bestMoveChangeCount`, `prevBestRootMove`, `addedInstabilityExtraTime`
- `src/engine/Search.cpp` - Implemented instability tracking and time adjustment logic
- `src/engine/UciOptions.cpp` - Added UCI option for runtime toggle
- `config/search.yaml` - Added configuration section

**Testing & Results:**
- Initial aggressive settings showed strength loss (-32 ELO)
- Conservative settings (higher thresholds, less aggressive factors) maintained strength
- **Arena Results (v1.2 vs v1.1 - with conservative settings):**
  - Match: +26.8 ELO (53.8% score, 131W/186D/99L over 416 games)
  - Feature expected to help more in longer time controls

**Tuning Notes:**
- Aggressive settings (low thresholds, high factors) hurt tactical play
- Conservative defaults chosen to avoid strength loss in blitz
- May benefit from different settings for different time controls
- Parameters are exposed via UCI for user tuning

**Expected Impact:** +5-15 ELO ✅ **Verified: Neutral to slightly positive with conservative settings**

---

### 3. Lazy SMP Multi-Threaded Search (Phase 3) ✅ COMPLETE

**Description:**  
Parallel search where N helper threads each run full alpha-beta searches independently, sharing only the transposition table. Simple, scalable, and effective.

**Implementation (v1.4-v1.5):**
```cpp
class Search {
  // Per-thread state via SearchThreadData
  std::vector<std::unique_ptr<SearchThreadData>> searchThreadData;
  std::atomic<bool> stopSearchFlag;
  
  // SearchThreadData struct holds all per-thread state:
  struct SearchThreadData {
    int id;                          // Thread ID (0 = main)
    uint64_t nodesVisited;           // Thread-local node counter
    Position position;               // Thread-local position copy
    Evaluator evaluator;             // Thread-local evaluator
    PVTable pv;                      // Triangular PV storage
    std::array<PlyInfo, DEPTH_MAX+1> plyStack;  // Per-ply state
    History history;                 // Move ordering heuristics
    SearchStats statistics;          // Debug statistics
  };
  
  // Shared state (protected)
  TT sharedTT;                    // Thread-safe with atomic key + XOR verification
  PawnTT sharedPawnTT;            // Lock-free concurrent access
};
```

**Key Implementation Details (v1.4):**
1. **Thread-Safe TT:** Atomic key operations with acquire/release memory order
2. **SearchThreadData:** Complete per-thread isolation (no shared mutable state except TT)
3. **Helper thread management:** Spawn N-1 helpers, each runs independent `iterativeDeepening()`
4. **Position cloning:** Each thread gets its own Position copy
5. **Node aggregation:** `getTotalNodes()` sums across all thread counters
6. **UCI Threads option:** Auto-detect cores or manual 1-N configuration

**v1.5 TT Enhancements:**
1. **TT Bucket Design:** 4-way associative with 64-byte cache-line alignment
2. **XOR Key Verification:** Torn-read detection for SMP race safety
   - Store: key stored as `(originalKey ^ dataHash)` after writing data fields
   - Load: verify `(storedKey ^ dataHash) == probeKey` to detect corruption
   - If any field is corrupted by torn read, XOR won't match = clean miss
3. **Cache-line alignment:** Eliminates false sharing under SMP
4. **Replacement policy:** Depth-preferred + age tiebreak
5. **Prefetch optimization:** Single prefetch loads entire bucket (4 entries)
6. **Race condition fixes:** All remaining SMP races eliminated

**Files Implemented:**
- `src/engine/SearchThreadData.h` - Per-thread state struct
- `src/engine/Search.h/cpp` - Thread spawning, coordination, node aggregation
- `src/engine/TT.h/cpp` - TT buckets with XOR key verification
- `src/engine/PawnTT.h/cpp` - Lock-free concurrent access
- `test/engine/SearchSmpTest.cpp` - Comprehensive test suite

**Testing & Validation:**
- ThreadSanitizer: No data races detected
- Determinism: `Threads=1` produces identical results
- Scaling: Verified at 2, 4, 8 threads
- Self-play: +119 ELO confirmed (v1.4 vs v1.3)

**Configuration Parameters:**
```yaml
THREADS: 0                      # 0 = auto-detect cores
# No SMP_MIN_SPLIT_DEPTH needed - Lazy SMP searches from root
```

**Expected Impact:** +50-100 ELO (on 4+ cores) ✅ **Verified: +119 ELO**

---

### 4. Enhanced Move Ordering (Phase 4) 🔄 PARTIAL

**Description:**  
Improve move ordering to push better moves higher in the list, increasing the effectiveness of alpha-beta pruning. Better ordering directly translates to higher nodes-per-second (NPS) effectively and deeper search.

**Components:**

#### 4a. Capture History Heuristic 📋 PLANNED
**Concept:**  
Similar to history heuristic for quiet moves, but for captures. Tracks which captures (by piece type and target square) historically cause beta cutoffs.
**Implementation:**
```cpp
// In History class
int captureHistory[PIECE_TYPE_NB][SQUARE_NB][PIECE_TYPE_NB]; // [piece][to][captured]
// usage: history.updateCapture(move, depth);
// sort: value = captureHistory[piece][to][captured];
```
**Expected Impact:** +10-20 ELO

#### 4b. Continuation History (Counter-Move History 2.0) 📋 PLANNED
**Concept:**  
Generalization of Counter-Move History. Instead of just 1 move, tracks history scores for moves that follow a specific previous move type (1-ply, 2-ply, 4-ply history).
**Implementation:**
```cpp
// History table indexed by previous move
int continuationHistory[2][64][64][2][64][64]; // [side][prevFrom][prevTo][side][currFrom][currTo]
// Often simplified to pointer-based access to avoid massive arrays
```
**Expected Impact:** +15-25 ELO

#### 4c. Killer Move Slot Optimization ✅ COMPLETE
**Concept:**  
Refined killer move logic to ensure high-quality moves are tried early.
**Status:** Implemented (2 slots, no replace if same).

#### 4d. Counter-Move History ✅ COMPLETE
**Description:**  
Track which moves work well in response to opponent's last move. Improves move ordering by capturing "refutation move" patterns.
**Implementation:**
Current implementation in `src/chesscore/History.h`:
```cpp
std::array<std::array<Move, 64>, 64> counterMoves{}; // [prevFrom][prevTo]
```
Used in `Search.cpp` to sort counter-move just after killers.
**Status:** **Implemented** (contrary to previous plan).

---

### 5. Syzygy Tablebase Support (Phase 5)

**Description:**  
Use pre-computed endgame databases for perfect play in 6-piece (and optionally 7-piece) endgames.

**Implementation:**
1. **Fathom Library Integration**
   - Add Fathom as vcpkg dependency
   - Wrap C API in C++ RAII class

2. **Root Probing**
   - Before starting search, probe TB for position
   - If found, return TB move immediately (or include in root move scoring)

3. **Search Probing**
   - At depth 1 (before quiescence), probe TB
   - Use WDL (Win/Draw/Loss) to adjust search bounds
   - Use DTZ (Distance to Zero) for move selection

4. **Configuration**
   ```yaml
   USE_TABLEBASES: true
   TB_PATH: "./syzygy/"
   TB_PROBE_DEPTH: 1           # min depth to probe (0 = always)
   TB_PROBE_LIMIT: 6           # max pieces (6 or 7)
   TB_CACHE_MB: 32             # Fathom internal cache
   ```

**Testing:**
- Verify correct TB move in known positions
- Test graceful fallback when TB not available
- Benchmark TB lookup overhead

**Expected Impact:** +35-60 ELO (significant in TB-relevant endgames)

---

### 6. Advanced Search Refinements (Phase 6) 🔄 PARTIAL

**Description:**  
Implementing aggressive pruning techniques that reduce the size of the search tree without sacrificing strength. These are standard in top engines.

#### 6a. Multi-Cut Pruning (MCP) 📋 PLANNED
**Concept:**  
If a shallow search finds C (default 3) moves that fail high, assume the current node is a cut-node and prune.
**Logic:**
```cpp
if (ply < depth - R && !PvNode) {
    int c = 0;
    for (int i=0; i < C_LIMIT; i++) {
        score = -search(newDepth - R, ...);
        if (score >= beta) c++;
    }
    if (c >= C) return beta;
}
```
**Risk:** Can prune valid lines if tactical.

#### 6b. ProbCut (Probabilistic Cut) 📋 PLANNED
**Concept:**  
Use a shallow search with a wider window to determine if a deep search will likely fail high/low. Based on the linear relationship between shallow and deep search scores.
**Formula:** `v_shallow >= beta + margin * sigma`
**Status:** High priority for later phases.

#### 6c. Late Move Pruning (LMP) ✅ COMPLETE
**Concept:**  
At depths where we likely won't find better moves (e.g., depth < 7), stop searching quiet moves after checking the first M moves.
**Formula:** `M = 3 + depth * depth`
**Benefit:** Significant node reduction in quiet positions.
**Status:** Implemented with move count array and improving factor.

#### 6d. Improved LMR Formula ✅ COMPLETE
**Concept:**  
Refine the reduction formula `R = log(depth) * log(move_count) * constant`.
**Current:** Simple linear reduction.
**Future:** Logarithmic, history-dependent (reduce less if history score is high).
**Status:** Implemented with logarithmic formula capability, history-based reduction modulation, and improving factor adjustments.

---

### 7. Automated Tuning Infrastructure (Phase 7)

**Description:**  
Build tools to scientifically optimize the 100+ configurable parameters using game results.

**Components:**

1. **SPSA Tuner**
   - Simultaneous perturbation gradient estimation
   - Parallel game matches with perturbed parameters
   - Iterative convergence to optimal values

2. **Texel Tuner**
   - Minimize error on labeled position dataset
   - Faster than SPSA for evaluation parameters
   - Requires large dataset (millions of positions)

3. **Match Runner**
   - Automate engine vs. engine matches
   - Parse PGN results
   - Track ELO changes over iterations

**Implementation:**
```python
# spsa_tuner.py
def tune_parameters(params, num_games=1000, iterations=100):
    for iteration in range(iterations):
        # Perturb parameters
        params_plus = perturb(params, +delta)
        params_minus = perturb(params, -delta)
        
        # Run matches
        score_plus = run_match(params_plus, num_games // 2)
        score_minus = run_match(params_minus, num_games // 2)
        
        # Update parameters
        gradient = (score_plus - score_minus) / (2 * delta)
        params += learning_rate * gradient
```

**Configuration:**
```yaml
# tuning/spsa_config.yaml
parameters:
  - name: NMP_REDUCTION
    initial: 2
    min: 1
    max: 4
    c: 0.1
    a: 100
  # ... more parameters
```

**Testing:**
- Validate gradient estimation on known functions
- Ensure tuning converges (not oscillating)
- Compare tuned vs. default parameters

**Expected Impact:** +30-60 ELO (from optimized parameters)

---

### 8. NNUE Evaluation (Phase 8)

**Description:**  
Replace classical evaluation with efficiently updatable neural network. Requires architecture design, training pipeline, and inference engine.

**Architecture (HalfKP-512x2):**
```
Input: 40960 features (King position + piece positions)
  ↓
Hidden Layer 1: 512 neurons (ReLU)
  ↓
Hidden Layer 2: 512 neurons (ReLU)
  ↓
Output: 1 neuron (Linear)
```

**Implementation Phases:**

1. **NNUE Data Structures (1 week)**
   ```cpp
   class NNUE {
     int16_t featureWeights[40960][512];
     int16_t layer1Weights[512][512];
     int16_t layer2Weights[512][1];
     int32_t accumulator[2][512];  // One per side
   };
   ```

2. **Incremental Updates (2-3 weeks)**
   - Track active features per king position
   - Efficiently add/remove features on make/unmake move
   - SIMD optimization (AVX2/AVX-512)

3. **Inference Engine (1-2 weeks)**
   - Forward propagation with quantized int16 arithmetic
   - Clipped ReLU activation
   - Scaling to centipawn units

4. **Training Pipeline (1-2 weeks)**
   - Generate training data from self-play
   - Use NNUE-pytorch or custom trainer
   - Tune network architecture and hyperparameters

5. **Integration (1 week)**
   - Uci option for NNUE vs. classical eval
   - Fallback to classical if NNUE file missing
   - Configuration for network file path

**Configuration Parameters:**
```yaml
USE_NNUE: true
NNUE_NET_PATH: "./networks/nn-default.nnue"
NNUE_FALLBACK_CLASSICAL: true
```

**Testing:**
- Verify evaluation consistency (no wild fluctuations)
- Test incremental update correctness (vs. full calculation)
- Benchmark evaluation speed (nodes per second)
- Self-play matches: NNUE vs. classical

**Expected Impact:** +200-400 ELO (largest single improvement)

---

## Priority Matrix

### High Priority (v1.1-v1.3)
1. **Singular Extensions** - Quick win, proven technique ✅
2. **Check Extensions** - Simple, effective ✅
3. **Lazy SMP** - Significant strength gain on modern hardware

### Medium Priority (v1.4-v1.6)
5. **Syzygy Tablebases** - Perfect endgame play
6. **Automated Tuning** - Optimize existing parameters
7. **Enhanced Move Ordering** - Incremental improvements

### Long-Term (v1.7-v1.9)
8. **NNUE Evaluation** - Dramatic strength gain, high effort
9. **CPU Compatibility** - Broader hardware support
10. **Advanced Search** - Modern pruning techniques

---

## Testing Strategy

### 1. Unit Tests
- Extend GoogleTest suite for each new feature
- Maintain 100% test pass rate across all platforms
- Use FRIEND_TEST for internal state validation

### 2. Tactical Test Suites
- **WAC (Win at Chess):** 300 tactical positions
- **STS (Strategic Test Suite):** Positional understanding
- **Arasan:** Comprehensive test suite
- Track improvement in solved positions and depth

### 3. Benchmark Matches
- Play 1000+ game matches against:
  - Previous FrankyCPP versions (regression testing)
  - External engines (Stockfish dev, Ethereal, RubiChess)
- Use cutechess-cli for automated match running
- Track ELO rating with confidence intervals

### 4. Performance Benchmarks
- Track nodes-per-second (NPS) on standard positions
- Monitor search depth reached at fixed time
- Profile hot paths with perf/VTune

### 5. Regression Testing
- Maintain EPD test suite with expected results
- Automated CI/CD testing on every commit
- Fail builds on significant strength regression

---

## Success Metrics

| Metric                        | Baseline (v1.0)  | Current (v1.4) | Target (v2.0) |
|-------------------------------|------------------|----------------|---------------|
| **ELO Rating**                | ~2400 (estimate) | ~2600 (+228)   | ~2900-3100    |
| **Tactical Suite (WAC)**      | 250/300          | 275/300        | 285/300       |
| **NPS (Single Thread)**       | ~1.5M            | ~1.8M          | ~2.0M         |
| **NPS (8 Threads)**           | N/A              | ~8-10M ✅      | ~10-12M       |
| **Endgame Accuracy (TB)**     | ~85%             | ~100% ✅       | ~100%         |
| **Search Depth (Fixed Time)** | 10-12 ply        | 12-14 ply      | 14-16 ply     |

---

## Risk Mitigation

### Technical Risks
1. **Search Instability (Lazy SMP)**
   - Mitigation: Extensive testing with 1 thread (deterministic)
   - Gradual rollout: 2, 4, 8 threads with validation at each step

2. **NNUE Training Failure**
   - Mitigation: Start with existing trained nets (HalfKP-512x2)
   - Fallback to classical evaluation always available

3. **Performance Regression**
   - Mitigation: Continuous benchmarking in CI/CD
   - Automated regression detection

### Project Risks
1. **Scope Creep**
   - Mitigation: Strict phase boundaries, no mixing of tasks
   - Each phase delivers working, tested functionality

2. **Time Overruns**
   - Mitigation: Conservative effort estimates
   - Prioritize high-impact items first

---

## Implementation Notes

### Development Environment
- Continue using Windows (MSVC) and Linux (GCC/Clang)
- All enhancements must be cross-platform
- Maintain CI/CD testing on every commit

### Backward Compatibility
- Keep classical evaluation as fallback
- Support both single-threaded and multi-threaded modes
- YAML configuration for all new features

### Documentation Requirements
- Update Architecture.md for major changes
- Inline code documentation for all new classes/functions
- User-facing documentation for new UCI options

---

## Future Considerations (Beyond v1.9)

### Search
- **Multi-PV Search:** Search multiple best moves simultaneously
- **Monte Carlo Tree Search (MCTS):** Hybrid MCTS/alpha-beta
- **Root Move Parallelization:** Dedicate threads to different root moves

### Evaluation
- **Larger NNUE Networks:** 1024x2 or larger
- **Positional Learning:** Train on master games, not just self-play
- **Hybrid NNUE+Classical:** Use both evaluations strategically

### Infrastructure
- **`speedtest` Command (v1.3+ optional):** Comprehensive game-based benchmark if `bench` proves insufficient. Only implement if needed. **Detailed plan:** `docs/specs/PLAN_Speedtest_Benchmark.md`
- **Distributed Tuning:** Cloud-based parameter optimization
- **Opening Book Tuning:** Learn from game statistics
- **Live ELO Tracking:** Web dashboard for strength monitoring

---

## Appendix: Reference Implementations

### Open Source Engines for Reference
- **Stockfish** - NNUE, Lazy SMP, singular extensions, syzygy
- **Ethereal** - Modern classical evaluation, tuning infrastructure
- **RubiChess** - Clean C++ implementation, lazy SMP
- **Berserk** - Recent NNUE implementation, good documentation
- **Koivisto** - NNUE training pipeline

### Academic Papers
- **Lazy SMP:** Parallel Alpha-Beta Search (Martin, 2013)
- **NNUE:** Efficiently Updatable Neural Networks (Nasu, 2018)
- **Texel Tuning:** Evaluation Tuning (Romstad, 2012)

### Libraries & Tools
- **Fathom:** Syzygy tablebase probing (C library)
- **NNUE-pytorch:** Network training framework
- **cutechess-cli:** Automated engine matches
- **BayesElo:** Rating calculation from match results

---

**Document Maintainer:** Frank Kopp  
**Last Updated:** 2026-03-06  
**Next Review:** After v1.5 release
