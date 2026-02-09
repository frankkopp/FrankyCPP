# FrankyCPP v1.x Engine Enhancement Plan

**Document Version:** 1.2  
**Created:** 2026-02-01  
**Last Updated:** 2026-02-09  
**Status:** Phase 2 In Progress  
**Target:** FrankyCPP v1.0 → v1.x releases

---

## Executive Summary

This document outlines a comprehensive plan for enhancing FrankyCPP's playing strength through systematic improvements to search, evaluation, and supporting infrastructure. The plan is organized into logical phases, each building on previous work while maintaining the engine's stability and production quality.

**Current State (v1.0):**
- Production-ready classical chess engine
- Alpha-beta search with modern pruning (NMP, LMR, futility, razoring)
- Classical evaluation (material, PST, pawn structure, mobility, king safety)
- Single-threaded search
- 100+ configurable parameters via YAML
- Comprehensive test suite (266+ tests)
- Cross-platform support (Windows/Linux)

**Target State (v1.x):**
- Multi-threaded parallel search (Lazy SMP)
- Neural network evaluation (NNUE) with classical fallback
- Endgame tablebase support (Syzygy)
- Enhanced search techniques (singular extensions, check extensions)
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

### Phase 2: Performance Fundamentals & Quick Wins (v1.2) - **2-3 weeks** 🔄 IN PROGRESS
**Focus:** Eliminate heap allocations, improve cache performance, add proven search enhancements

| Task                            | Category       | Effort      | Complexity | ELO Gain | Priority | Status        |
|---------------------------------|----------------|-------------|------------|----------|----------|---------------|
| **Performance Fundamentals**    |                |             |            |          |          |               |
| Triangular PV Table             | Performance    | 🟢 1-2 days | 🟡 Medium  | +5-10    | HIGH     | ✅ Complete    |
| MoveList Static Array Refactor  | Performance    | 🟡 3-5 days | 🟡 Medium  | +5-15    | HIGH     | ✅ Complete    |
| **Infrastructure**              |                |             |            |          |          |               |
| UCI `bench` Command             | Infrastructure | 🟢 2-3 days | 🟢 Low     | N/A      | HIGH     | ✅ Complete    |
| Arena Bench Integration         | Infrastructure | 🟢 2-3 days | 🟢 Low     | N/A      | MEDIUM   | ✅ Complete    |
| **Search Quick Wins**           |                |             |            |          |          |               |
| Singular Extensions             | Search         | 🟢 2-3 days | 🟡 Medium  | +20-30   | HIGH     | ✅ Complete    |
| Check Extensions                | Search         | 🟢 2-3 days | 🟡 Medium  | +10-20   | HIGH     | ✅ Complete    |
| Counter-Move History            | Search         | 🟡 3-5 days | 🟡 Medium  | +10-20   | LOW      | ❌ Not Planned |
| Best-Move Instability Time Mgmt | Search         | 🟢 2-3 days | 🟡 Medium  | +5-15    | MEDIUM   | 📋 Planned    |
| Selective Checks in Quiescence  | Search         | 🟡 3-5 days | 🟡 Medium  | +15-25   | MEDIUM   | 📋 Planned    |

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

### Phase 3: Multi-Threading (v1.3) - **3-4 weeks**
**Focus:** Parallel search for multi-core CPUs

| Task                    | Category    | Effort       | Complexity | ELO Gain | Priority |
|-------------------------|-------------|--------------|------------|----------|----------|
| Thread-Safe TT          | Performance | 🟡 1 week    | 🔴 High    | N/A      | CRITICAL |
| Lazy SMP Search         | Performance | 🔴 2-3 weeks | 🔴 High    | +50-100  | CRITICAL |
| Thread Pool Integration | Performance | 🟢 2-3 days  | 🟡 Medium  | N/A      | HIGH     |
| SMP Testing & Tuning    | Performance | 🟡 3-5 days  | 🟡 Medium  | +10-20   | HIGH     |

**Expected Total ELO Gain:** +60-120 (on multi-core hardware)  
**Risk:** High - requires careful synchronization and testing  
**Notes:** 
- ThreadPool already exists but search is single-threaded
- Requires refactoring Search class for shared state
- Must maintain deterministic behavior for testing

---

### Phase 4: Enhanced Move Ordering (v1.4) - **1-2 weeks**
**Focus:** Better move ordering for deeper effective search

| Task                             | Category | Effort      | Complexity | ELO Gain | Priority |
|----------------------------------|----------|-------------|------------|----------|----------|
| Capture History Heuristic        | Search   | 🟡 3-5 days | 🟡 Medium  | +10-20   | HIGH     |
| Continuation History             | Search   | 🟡 3-5 days | 🟡 Medium  | +15-25   | HIGH     |
| Static Exchange Eval Enhancement | Search   | 🟢 2-3 days | 🟡 Medium  | +5-10    | MEDIUM   |
| Killer Move Slot Optimization    | Search   | 🟢 1-2 days | 🟢 Low     | +5-10    | LOW      |

**Expected Total ELO Gain:** +35-65  
**Risk:** Low - proven techniques with minimal architectural impact

---

### Phase 5: Endgame Tablebases (v1.5) - **2-3 weeks**
**Focus:** Perfect endgame play with Syzygy tablebases

| Task                       | Category | Effort      | Complexity | ELO Gain | Priority |
|----------------------------|----------|-------------|------------|----------|----------|
| Fathom Library Integration | Endgame  | 🟢 2-3 days | 🟡 Medium  | N/A      | HIGH     |
| Root Tablebase Probing     | Endgame  | 🟢 2-3 days | 🟡 Medium  | +20-30   | HIGH     |
| Search Tablebase Probing   | Endgame  | 🟡 1 week   | 🟡 Medium  | +10-20   | MEDIUM   |
| TB Configuration & Testing | Endgame  | 🟢 2-3 days | 🟢 Low     | N/A      | MEDIUM   |
| TB Cache Optimization      | Endgame  | 🟢 2-3 days | 🟡 Medium  | +5-10    | LOW      |

**Expected Total ELO Gain:** +35-60 (in TB-relevant endgames)  
**Risk:** Low - well-defined API (Fathom)  
**Notes:**
- Syzygy tablebases are standard (Stockfish, Komodo, etc.)
- Significant strength gain in 6-piece and 7-piece endgames
- Requires configuration for TB path

---

### Phase 6: Advanced Search Refinements (v1.6) - **2-3 weeks**
**Focus:** Modern search techniques for deeper tactical vision

| Task                          | Category | Effort      | Complexity | ELO Gain | Priority |
|-------------------------------|----------|-------------|------------|----------|----------|
| Multi-Cut Pruning             | Search   | 🟡 3-5 days | 🟡 Medium  | +10-20   | HIGH     |
| Probcut Pruning               | Search   | 🟡 3-5 days | 🟡 Medium  | +10-15   | MEDIUM   |
| Late Move Pruning             | Search   | 🟢 2-3 days | 🟡 Medium  | +10-15   | MEDIUM   |
| Improved LMR Formula          | Search   | 🟡 1 week   | 🟡 Medium  | +15-25   | HIGH     |
| SEE-Based Pruning Enhancement | Search   | 🟢 2-3 days | 🟡 Medium  | +5-10    | LOW      |

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
| Metric | Result |
|--------|--------|
| Games | 104 |
| Score | 58.2% |
| W/D/L | 38/45/21 |
| ELO | +57 |
| Test Suite | -1.2% (2873 positions) |

**Expected Impact:** +10-20 ELO ✅ **Verified: ~+30 ELO**

---

### 3. Lazy SMP Multi-Threaded Search (Phase 3)

**Description:**  
Parallel search where N helper threads each run full alpha-beta searches independently, sharing only the transposition table. Simple, scalable, and effective.

**Architecture Changes:**
```cpp
class Search {
  // Existing single-threaded state
  std::vector<std::unique_ptr<SearchThread>> helperThreads;
  std::atomic<bool> stopSearchFlag;
  
  // Per-thread state
  struct SearchThread {
    History history;
    KillerMoves killers;
    PlyInfo plyStack[MAX_PLY];
    // ...
  };
  
  // Shared state (protected)
  TT sharedTT;                    // Already thread-safe with atomic ops
  std::atomic<uint64_t> nodesSearched;
  std::mutex pvMutex;             // For PV update
};
```

**Implementation Plan:**
1. **Thread-Safe TT (1 week)**
   - Add atomic flags to TTEntry (avoid torn reads)
   - Use lock-free updates (compare-and-swap)
   - Test concurrent access patterns

2. **SearchThread Class (1 week)**
   - Refactor per-ply state into SearchThread
   - Clone Position for each thread (no shared state)
   - Independent history/killer tables per thread

3. **Thread Pool Integration (2-3 days)**
   - Use existing ThreadPool infrastructure
   - Each thread runs iterativeDeepening() independently
   - Main thread coordinates start/stop

4. **Result Aggregation (2-3 days)**
   - Best move selection from TT after search complete
   - Node count aggregation (atomic counters)
   - PV selection from highest-depth thread

5. **Testing & Tuning (3-5 days)**
   - Verify deterministic behavior (with 1 thread)
   - Scale testing (2, 4, 8, 16 threads)
   - Tune thread count vs. NPS vs. depth reached

**Configuration Parameters:**
```yaml
USE_LAZY_SMP: true
SMP_THREADS: 0                  # 0 = auto-detect cores
SMP_MIN_SPLIT_DEPTH: 4          # minimum depth to spawn helper threads
```

**Testing:**
- Benchmark suite with 1, 2, 4, 8 threads
- Verify no race conditions (ThreadSanitizer)
- Ensure ELO gain scales with thread count

**Expected Impact:** +50-100 ELO (on 4+ cores)

---

### 4. Counter-Move History ❌ NOT PLANNED

**Status:** Not Planned

**Description:**  
Track which moves work well in response to opponent's last move. Improves move ordering by capturing "refutation move" patterns.

**Reason for Not Implementing:**  
Current move ordering already achieves ~97% first-move beta cutoff rate (tracked via `betaCutsByIndex` stats). The diminishing returns from counter-move history (~1-5% fewer nodes in benchmarks) do not justify the 128MB+ memory cost for the 4D table `[prevFrom][prevTo][currFrom][currTo]`.

The existing implementation already includes:
- Simple counter-move table (`USE_HISTORY_MOVES`): stores one best reply per opponent move
- History heuristic (`USE_HISTORY_COUNTER`): tracks move success globally
- Killer moves, TT moves, and good captures for ordering

These provide excellent move ordering without the memory overhead of full counter-move history.

**If Reconsidered in Future:**
```cpp
// Would add: CounterMoveHistory[opponentPiece][opponentTo][piece][to]
class CounterMoveHistory {
  int16_t table[PIECE_TYPE_NB][SQUARE_NB][PIECE_TYPE_NB][SQUARE_NB];
  
  void update(Move counterMove, Move previousMove, int bonus);
  int get(Move counterMove, Move previousMove) const;
};
```

**Expected Impact:** +10-20 ELO (not worth memory cost given current 97% cutoff rate)

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

| Metric                        | Baseline (v1.0)  | Target (v1.8) |
|-------------------------------|------------------|---------------|
| **ELO Rating**                | ~2400 (estimate) | ~2800-2900    |
| **Tactical Suite (WAC)**      | 250/300          | 285/300       |
| **NPS (Single Thread)**       | ~1.5M            | ~2.0M         |
| **NPS (8 Threads)**           | N/A              | ~8-10M        |
| **Endgame Accuracy (TB)**     | ~85%             | ~100%         |
| **Search Depth (Fixed Time)** | 10-12 ply        | 14-16 ply     |

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
**Last Updated:** 2026-02-09  
**Next Review:** After Phase 2 completion (v1.2 release)
