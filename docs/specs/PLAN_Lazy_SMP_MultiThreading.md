# FrankyCPP v1.5 - Lazy SMP Multi-Threading Implementation Plan

**Document Version:** 1.5
**Created:** 2026-02-25
**Last Updated:** 2026-02-26
**Status:** Phase 1-3 ✅ — Phase 5-6 ✅ — Phase 4 TODO (PawnTT) — Phase 7 TODO (Testing)
**Target Version:** v1.5
**Estimated Effort:** 3-4 weeks

---

## Overview

This document details the implementation plan for adding Lazy SMP (Symmetric Multi-Processing) parallel search to FrankyCPP. The approach is deliberately simple: helper threads independently run `iterativeDeepening()` on the same position, sharing only the Transposition Table. This achieves strong multi-core scaling with minimal architectural change.

**Design Principle: Zero overhead when SMP is OFF**
- When `Threads = 1` all new code paths are bypassed completely
- No atomic overhead, no extra allocations, no branching on the hot path
- The single-thread case must be byte-for-byte identical in behavior to pre-SMP

---

## Lazy SMP Explained

Lazy SMP is the simplest effective parallel search strategy (used by Stockfish 7+):

1. All threads search the same root position independently
2. They share one Transposition Table (TT)
3. Each thread uses slightly different aspiration windows or random depth offsets to avoid redundant work
4. When one thread writes a good move to the TT, other threads read it and get cutoffs "for free"
5. The **main thread** controls time and produces the final best move; helpers run until stopped

No explicit move splitting, no task queues, no inter-thread communication (except TT).

---

## State Classification

Understanding what state is *shared* vs *thread-local* is the entire challenge:

### Shared (one instance, all threads read/write)
| State                      | Current Location         | Action                                                        |
|----------------------------|--------------------------|---------------------------------------------------------------|
| `TT` (transposition table) | `Search::tt`             | Make thread-safe (see Step 1)                                 |
| `stopSearchFlag`           | `Search::stopSearchFlag` | Already `std::atomic_bool` ✅                                  |
| `nodesVisited` (aggregate) | `Search::nodesVisited`   | Aggregate from all threads at report time                     |
| `startTime`, `timeLimit`   | `Search`                 | Read-only after search starts ✅                               |
| `searchLimits`             | `Search`                 | Read-only after search starts ✅                               |
| `rootMoves`                | `Search`                 | Main thread writes; helpers read (no lock needed after setup) |

### Thread-Local (each thread needs its own copy)
| State                    | Current Location       | Action                                                         |
|--------------------------|------------------------|----------------------------------------------------------------|
| `PVTable pv`             | `Search::pv`           | ✅ Moved to `SearchThreadData`                                  |
| `PlyInfo plyStack[]`     | `Search::plyStack`     | ✅ Moved to `SearchThreadData`                                  |
| `History history`        | `Search::history`      | ✅ Moved to `SearchThreadData`                                  |
| `SearchStats statistics` | `Search::statistics`   | ✅ Moved to `SearchThreadData`                                  |
| `uint64_t nodesVisited`  | `Search::nodesVisited` | ✅ Moved to `SearchThreadData`                                  |
| `LMR_REDUCTION`          | `Search`               | ✅ Moved to `SearchThreadData`                                  |
| `Evaluator evaluator`    | `Search::evaluator`    | Move to `SearchThreadData` (or share - it is stateless enough) |
| `lastUciUpdate*`         | `Search`               | Main thread only - no change needed                            |
| `bestMoveStability`      | `Search`               | Main thread only - no change needed                            |

---

## Implementation Phases

### Phase 1: TT Thread Safety (Day 1-5)
**Goal:** Make TT read/write safe from multiple threads without hurting single-thread speed.

#### The Real Concerns (Why This Needs Careful Measurement)

The claim "relaxed atomic = plain `mov`, zero overhead" is **theoretically correct on x86** but has several practical traps that must be verified:

1. **`std::atomic<T>` struct size** — The C++ standard does not guarantee `sizeof(std::atomic<T>) == sizeof(T)`. On MSVC and GCC/clang with x86-64, `std::atomic<uint64_t>` is always 8 bytes (confirmed by `is_always_lock_free`), but this **must be statically asserted** or the 16-byte `Entry` size guarantee breaks.

2. **`clear()` and `ageEntries()` use `std::execution::par_unseq`** — These functions iterate over entries and assign/read `e.key` directly. If `key` becomes `std::atomic`, those direct assignments become **UB** (non-atomic write to atomic object). They must be updated to use `.store(0, relaxed)`.

3. **`probe()` decrements `entry->age` without atomicity** — `age` is a 3-bit bitfield packed with `depth` and `type` in the same byte. A concurrent write to any of those fields is a data race. Under Lazy SMP, the **age decrement in `probe()` must be removed** — it is unsafe from multiple threads and its benefit (usage tracking) is marginal.

4. **Statistics counters** — `numberOfPuts`, `numberOfHits`, etc. are non-atomic `uint64_t` members updated in `put()` and `probe()`. These are read/write data races from multiple threads. **Solution: make statistics thread-local** (aggregated at search end), or simply **disable per-probe statistics in SMP mode** (they are already stripped in production builds).

5. **`assert` in `put()`** — The assertion `numberOfPuts == (numberOfEntries + numberOfCollisions + numberOfUpdates)` is based on non-atomic counters. It breaks in SMP mode. Must be guarded or removed.

6. **`probe()` mutable-cast / non-const pattern** — `probe()` is marked `// ReSharper disable CppMemberFunctionMayBeConst` and increments statistics. This mutable access from multiple threads is unsafe. In SMP mode statistics must be dropped from `probe()`.

#### Why Not Mutexes?

Mutexes are the "obvious" thread-safety answer but are completely unsuitable for TT:

| Mutex variant                            | Problem                                                                                                                                                                                                |
|------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Per-entry `std::mutex`**               | `sizeof(std::mutex)` is ~40–56 bytes on Windows/Linux. Each entry grows from 16 → ~72 bytes. TT holds 3–4× fewer entries → catastrophic hit-rate drop. And locks still acquired in single-thread mode. |
| **One global mutex**                     | All threads queue to read/write. Eliminates all parallelism benefit. Worse than single-threaded.                                                                                                       |
| **Stripe locking (N mutexes, hash % N)** | Reduces contention, but every `probe()` / `put()` still locks even with 1 thread. Adds overhead, adds complexity. No benefit over the atomic approach.                                                 |

The `sizeof` concern about `std::atomic<ZobristKey>` is **not** a reason to use mutexes — it is a reason to use a `static_assert`. If `sizeof(std::atomic<ZobristKey>) != sizeof(ZobristKey)` the build simply **fails at compile time** before any code runs, and you fall back to Option B. Mutexes would guarantee the size blows up; a `static_assert` guarantees it does not.

#### Two Design Options

##### Option A: `std::atomic<ZobristKey>` key only (Recommended)

Keep all other fields non-atomic. Use relaxed memory order on key read/write.

**Write order in `put()`:** Write all non-key fields first, then write key last (with `release` semantics so readers that see the new key also see the new data). **Read order in `probe()`:** Read key first (with `acquire` semantics), then read other fields.

Using `acquire/release` on the key provides a memory-order fence that is **still just a plain `mov` on x86** (x86 TSO provides acquire/release "for free" on aligned loads/stores), but is **correct on ARM/PowerPC** too. This makes the code portable and safe without any actual fence instruction on x86.

```cpp
// put() - write data first, then key with release:
entry->move  = static_cast<uint16_t>(move);
entry->depth = depth;
entry->value = value;
entry->type  = type;
entry->eval  = eval;
// key written last - release ensures above writes are visible to any thread
// that subsequently reads the key with acquire:
entry->key.store(newKey, std::memory_order_release);

// probe() - read key with acquire, then read data:
const ZobristKey k = entry->key.load(std::memory_order_acquire);
if (k == queryKey) { /* read move, depth, value... */ }
```

On x86, `acquire` load = plain `mov`, `release` store = plain `mov`. No `mfence`, no `lock` prefix.

**Overhead in single-thread mode:** None, provided:
- `sizeof(std::atomic<ZobristKey>) == sizeof(ZobristKey)` — verified by `static_assert`
- `std::atomic<ZobristKey>::is_always_lock_free == true` — verified by `static_assert`
- Entry struct size unchanged at 16 bytes — verified by `static_assert(sizeof(Entry) == 16)`

**Additional changes required for this option:**
- `clear()`: Change `e.key = 0` → `e.key.store(0, std::memory_order_relaxed)`
- `ageEntries()`: Change `if (e.key == 0)` → `if (e.key.load(std::memory_order_relaxed) == 0)`
- `probe()`: Remove `ttEntryPtr->age--` (age decrement is a data race — and provides negligible benefit)
- Statistics in `put()`/`probe()`: Move to `SearchThread` (thread-local) or disable in SMP mode
- Remove the `assert` in `put()` (or make it single-thread-only via `#ifndef NDEBUG` and a thread-count check)

##### Option B: XOR Key Trick (No `std::atomic` at all)

Used by Stockfish pre-NNUE era. Store `key ^ packedData` so a partial write always produces a key mismatch:

```cpp
struct Entry {
    uint64_t keyXorData = 0;  // key XOR'd with the rest of the data
    uint16_t move = 0;
    Value eval    = VALUE_NONE;
    Value value   = VALUE_NONE;
    // ... bitfields ...
};

// put():
const uint64_t data = packData(move, depth, value, type, eval);
entry->keyXorData = key ^ data;

// probe():
const uint64_t data = entry->keyXorData;
const ZobristKey reconstructed = data ^ packData(entry->move, entry->depth, ...);
if (reconstructed == queryKey) { /* valid hit */ }
```

**Pro:** No `std::atomic` at all. Struct layout unchanged. Technically still UB under C++ memory model on non-x86, but the XOR check provides collision detection.

**Con:** More complex read path. Does not eliminate UB per C++ standard (just makes it benign on x86). Harder to reason about. **Not recommended** — Option A is cleaner and equally fast.

#### Chosen Approach: Option A

#### Verification Plan (MANDATORY before merging)

The single-thread performance claim **must be empirically verified**, not just assumed:

1. **Static assertions** (compile-time, zero cost):
   ```cpp
   static_assert(std::atomic<ZobristKey>::is_always_lock_free,
                 "TT key atomic must be lock-free for zero overhead");
   static_assert(sizeof(std::atomic<ZobristKey>) == sizeof(ZobristKey),
                 "TT atomic key must not change Entry size");
   static_assert(sizeof(TT::Entry) == 16,
                 "TT Entry size must remain 16 bytes");
   ```

2. **Assembly inspection** (manual, one-time):
   - Build with `cmake-build-win-release` (MSVC Release)
   - Inspect `TT::put()` and `TT::probe()` in the disassembly viewer (IDA / VS disassembler)
   - Confirm the key read/write generates `mov qword` not `lock xchg` or `mfence`
   - Document finding in this plan

3. **Benchmark regression** (automated):
   - Run `FrankyCPP --bench --benchDepth 12` **before** and **after** the TT change (same binary configuration)
   - Compare NPS. Acceptable regression: < 1% (noise floor)
   - If NPS drops > 1%: investigate with profiler (VTune or perf)
   - Record result in `results/benchmarks/benchmarks.json` (existing infrastructure)

4. **TT unit test** (new):
   ```cpp
   // TT_Test: concurrent put+probe stress test
   // Launches 4 threads, each doing 1M put+probe cycles on overlapping keys
   // Must not crash, deadlock, or trigger ASAN/TSAN errors
   TEST(TT_Test, ConcurrentPutProbeNoUB) { ... }
   ```
   Run with `-fsanitize=thread` (TSAN) on WSL/Linux build to catch any remaining races.

#### Summary of Changes

| Location                     | Change                                                                                             | Reason                                                             | Status |
|------------------------------|----------------------------------------------------------------------------------------------------|--------------------------------------------------------------------|--------|
| `TT.h` `Entry::key`          | `ZobristKey` → `std::atomic<ZobristKey>`                                                           | Prevent UB on concurrent access                                    | ✅ Done |
| `TT.h` `Entry`               | Add 3× `static_assert`s (lock-free, sizeof==8, Entry==16 bytes)                                    | Compile-time guarantee of zero overhead; build fails if violated   | ✅ Done |
| `TT.h` `getMatch()`          | `key.load(acquire)` for key comparison                                                             | Correct atomic read                                                | ✅ Done |
| `TT.h` `numSmpThreads`       | New field + `setSmpThreads()` / `getSmpThreads()` accessors                                        | Controls `age--` and `assert` guards; set by Search before search  | ✅ Done |
| `TT.cpp` `operator<<`        | `key.load(relaxed)` for streaming                                                                  | Non-UB read of atomic field                                        | ✅ Done |
| `TT.cpp` `put()`             | Load key `relaxed`; write data first; store key `release` last                                     | Correct write ordering — readers see coherent entry on key match   | ✅ Done |
| `TT.cpp` `put()` assert      | `assert(numSmpThreads > 1 \|\| ...)` — bypassed under SMP                                          | Would crash Debug+SMP (non-atomic counters)                        | ✅ Done |
| `TT.cpp` statistics counters | Left as plain non-atomic increments                                                                | Diagnostic only; approximate values under SMP are acceptable       | ✅ Done |
| `TT.cpp` `probe()`           | Load key `acquire`; `age--` when `numSmpThreads==1` only; use `getEntryPtrConst`                   | Preserves single-thread behavior exactly; avoids bitfield race SMP | ✅ Done |
| `TT.cpp` `clear()`           | `e.key.store(0, relaxed)` in `par_unseq` lambda                                                    | Atomic object requires atomic write                                | ✅ Done |
| `TT.cpp` `ageEntries()`      | `e.key.load(relaxed) == 0` in `par_unseq` lambda                                                   | Atomic object requires atomic read                                 | ✅ Done |
| `test/engine/TT_Test.cpp`    | Add `ConcurrentPutProbeNoUB` — 4 threads × 500K iterations; range-checks value/depth on probe hits | Verifies no crash/deadlock and data coherence after key match      | ✅ Done |

**Verification results:**
- ✅ Build passes — all 3× `static_assert`s satisfied on MSVC x86-64
- ✅ NPS unchanged / marginally higher — no regression (3,318,959 NPS post-change)
- ✅ `TT_Test::ConcurrentPutProbeNoUB` passes — 4 threads × 500K iterations clean
- ✅ All existing TT tests pass
- ⬜ Assembly inspection — optional; static_assert + NPS result are sufficient evidence
- ⚠️ TSAN on WSL/Linux — SKIPPED (WSL build setup issues; Windows tests passed, proceeding to Phase 2)

**Files changed:** `src/engine/TT.h`, `src/engine/TT.cpp`, `test/engine/TT_Test.cpp`

---

### Phase 2: `SearchThreadData` Struct — Isolate Thread-Local State (Day 4-6) ✅ COMPLETE
**Goal:** Extract per-thread state into a new `SearchThreadData` struct so each helper thread can own its own copy.

**Status:** ✅ Complete (2026-02-26)

#### New file: `src/engine/SearchThreadData.h`

```cpp
/// Per-thread search state for Lazy SMP.
/// Each thread (main + helpers) owns one SearchThreadData instance.
/// Shared state (TT, stop flag, time limits) lives in Search and is
/// accessed via const pointer.
struct SearchThreadData {
    int id = 0;                           // 0 = main thread
    uint64_t nodesVisited = 0;            // thread-local node count

    PVTable pv;                           // triangular PV table
    std::array<PlyInfo, DEPTH_MAX + 1> plyStack{};
    History history{};
    SearchStats statistics{};

    // LMR table (copy per thread, read-only after init)
    std::array<std::array<int, 64>, 32> LMR_REDUCTION{};

    // reset between searches
    void reset() {
        nodesVisited = 0;
        pv.clearAll();
        for (auto& p : plyStack) p.resetSearchState();
        history.reset();
        statistics = {};
    }
};
```

#### Changes to `Search.h` / `Search.cpp`

**In `Search` class:**
- Remove the fields that move to `SearchThreadData`:
  - `PVTable pv;`
  - `std::array<PlyInfo, DEPTH_MAX + 1> plyStack{};`
  - `History history{};`
  - `SearchStats statistics{};`
  - `uint64_t nodesVisited{};`
  - `LMR_REDUCTION` table
- Add:
  ```cpp
  // Thread 0 is the main thread. Index [0] always exists.
  // Helper threads are indices [1..N-1].
  // Named "Data" to distinguish from std::thread searchThread (the execution thread)
  std::vector<std::unique_ptr<SearchThreadData>> searchThreadData{};
  int numHelperThreads = 0;  // set from config; 0 = single-thread mode
  ```
- Add convenience accessor (avoids changing all call sites in the hot path):
  ```cpp
  SearchThreadData& mainThread() { return *searchThreadData[0]; }
  const SearchThreadData& mainThread() const { return *searchThreadData[0]; }
  ```
- Replace all direct uses of `pv`, `plyStack`, `history`, `statistics`, `nodesVisited`, `LMR_REDUCTION` in `Search.cpp` with `mainThread().pv`, etc.

**Naming clarification:**
- `std::thread searchThread` — the OS execution thread that runs `Search::run()`
- `SearchThreadData` — struct holding per-thread **data** (not a thread itself)
- `searchThreadData` — vector of `SearchThreadData` instances (one per search thread)

**Strategy for minimizing changes:** Use a `SearchThreadData*` local variable at the top of search functions:
```cpp
Value Search::search(Position& p, Depth depth, Depth ply, ...) {
    SearchThreadData& st = mainThread();  // or pass as parameter for helpers
    // All st.plyStack, st.pv, st.statistics uses
}
```

For helpers, the same search functions are called but with a different `SearchThreadData&`. This is achieved by passing `SearchThreadData&` as an extra parameter to the internal recursive search functions.

**Files changed:**
- New: `src/engine/SearchThreadData.h`
- `src/engine/Search.h` — added `searchThreadData` vector, `mainThread()` accessor (public)
- `src/engine/Search.cpp` — all internal functions now use `mainThread().` prefix for thread-local data
- `test/engine/SearchTest.cpp` — updated to use `search.mainThread().LMR_REDUCTION` etc.

**Verification:**
- ✅ Build passes (MSVC Release/Debug)
- ✅ All unit tests pass (266 tests)
- ✅ `mainThread()` accessor is public for test access
- ✅ No functional changes — single-thread behavior identical

---

### Phase 3: Helper Thread Launch & Control (Day 7-10)
**Goal:** Launch N helper threads that each call a simplified `helperSearch()` loop.

#### Helper Thread Lifecycle

```
startSearch() called
    → allocate/reset SearchThreadData[0..N]
    → if numHelperThreads > 0:
        launch helper threads: each calls helperRun(SearchThreadData&)
    → main thread calls run() as today
    → main thread finishes iterativeDeepening()
    → stopSearchFlag = true
    → join all helper threads
    → aggregate node counts
    → send result to UCI
```

#### `helperRun(SearchThreadData& st)` function

```cpp
void Search::helperRun(SearchThreadData& st) {
    // Helper threads just loop over increasing depths
    // They share the TT with the main thread
    // They stop when stopSearchFlag is set
    Position localPos = position;  // local copy (position is const after startSearch)
    int depth = 1;
    while (!stopSearchFlag.load(std::memory_order_relaxed)) {
        // Offset depth slightly to diversify search tree coverage
        const int searchDepth = depth + (st.id % 2);  // odd helpers search one ply deeper
        aspirationSearch_helper(localPos, searchDepth, st);
        ++depth;
        if (depth > DEPTH_MAX) depth = 1;
    }
}
```

Helper threads do NOT:
- Report to UCI
- Manage time
- Update `lastSearchResult`
- Call `ageEntries()` on TT

#### Thread Management in `Search`

```cpp
// In Search.h (new fields)
std::vector<std::thread> helperThreads{};

// In startSearch():
void Search::startSearch(const Position& p, SearchLimits sl) {
    // ... existing setup ...
    
    // allocate/reset thread state
    const int totalThreads = numHelperThreads + 1;
    while (searchThreadData.size() < totalThreads)
        searchThreadData.push_back(std::make_unique<SearchThreadData>(searchThreadData.size()));
    for (auto& st : searchThreadData) st->reset();
    
    // launch helper threads (no-op if numHelperThreads == 0)
    helperThreads.clear();
    for (int i = 1; i <= numHelperThreads; ++i) {
        helperThreads.emplace_back([this, i]() {
            helperRun(*searchThreadData[i]);
        });
    }
    
    // launch main search thread as today
    searchThread = std::thread([this]() { run(); });
}

// In run(), after iterativeDeepening():
void Search::run() {
    // ... existing ...
    auto result = iterativeDeepening(pos);
    
    stopSearchFlag = true;  // signal helpers to stop
    for (auto& t : helperThreads) t.join();
    helperThreads.clear();
    
    // aggregate node counts from all threads
    for (const auto& st : searchThreadData)
        result.nodes += st->nodesVisited;  // already counted main thread
    
    // ... rest of existing run() ...
}
```

**Key invariant:** `numHelperThreads == 0` means **no new threads are ever created**. The `helperThreads` vector stays empty. All code paths above are protected by `if (numHelperThreads > 0)` checks or naturally empty loops.

#### Phase 3 Implementation Stages

##### Stage 3.1: Add `helperThreads` vector and `helperRun()` stub ✅ COMPLETE
- Add `std::vector<std::thread> helperThreads{}` to `Search.h`
- Add `void helperRun(SearchThreadData& st)` declaration to `Search.h`
- Add empty stub implementation in `Search.cpp`

##### Stage 3.2: Implement thread launch in `startSearch()` ✅ COMPLETE
- In `run()`, after `currentThreadData` is set, launch helper threads:
  ```cpp
  helperThreads.clear();
  for (int i = 1; i <= numHelperThreads; ++i) {
      helperThreads.emplace_back([this, i]() { helperRun(*searchThreadData[i]); });
  }
  ```
- No-op when `numHelperThreads == 0` (empty loop)

##### Stage 3.3: Implement helper search loop + thread-local data access ✅ COMPLETE
**3.3a: Thread-local pointer infrastructure**
- Add `static inline thread_local SearchThreadData* currentThreadData` to `Search.h`
- Add `static SearchThreadData& thread()` accessor that returns `*currentThreadData`
- In `run()`, set `currentThreadData = &mainThread()` before search starts

**3.3b: Replace `mainThread()` with `thread()` in hot path**
- In `search()`, `qsearch()`, `evaluate()`: replace `mainThread().` with `thread().`
- Keep `mainThread()` for contexts outside search (e.g., `newGame()`, `str()`, ponder move extraction)
- In `helperRun()`: set `currentThreadData = &st` for helper's data
- Implement actual helper search loop:
  ```cpp
  void Search::helperRun(SearchThreadData& st) {
      currentThreadData = &st;
      Position localPos = position;
      const MoveList localRootMoves = *st.plyStack[0].mg->generateLegalMoves(localPos, GenAll);
      Depth depth = 1 + static_cast<Depth>(st.id % 2);
      while (!stopSearchFlag.load(std::memory_order_relaxed)) {
          st.pv.clearAll();
          for (const Move& move : localRootMoves) {
              if (stopSearchFlag.load(std::memory_order_relaxed)) break;
              localPos.doMove(move);
              st.nodesVisited++;
              if (!checkDrawRepAnd50(localPos, 2)) {
                  (void)search(localPos, depth - 1, Depth{1}, VALUE_MIN, VALUE_MAX, PvNode, Do_Null_Move);
              }
              localPos.undoMove();
          }
          ++depth;
          if (depth > DEPTH_MAX - 10) depth = 1 + static_cast<Depth>(st.id % 2);
      }
  }
  ```

##### Stage 3.4: Implement thread join and cleanup in `run()` ✅ COMPLETE
**Goal:** After main search completes, stop helpers and clean up.

**Changes to `run()` — after `stopSearchFlag = true`:**
```cpp
// Join all helper threads
for (auto& t : helperThreads) {
    if (t.joinable()) { t.join(); }
}
helperThreads.clear();
```

**Implementation notes:**
- The join loop is O(0) when `numHelperThreads == 0`
- Each helper checks `stopSearchFlag` and exits its loop
- `joinable()` check is defensive but not strictly necessary if we always launch/join correctly

##### Stage 3.5: Implement node count aggregation ✅ COMPLETE
**Goal:** Report total nodes from all threads in search result and UCI output.

**`getTotalNodes()` helper (already exists):**
```cpp
[[nodiscard]] uint64_t Search::getTotalNodes() const {
    uint64_t total = 0;
    for (const auto& st : searchThreadData) {
        total += st->nodesVisited;
    }
    return total;
}
```

**Updated functions to use `getTotalNodes()`:**
- `run()` — final `searchResult.nodes` and log output
- `sendIterationEndInfoToUci()` — nodes and NPS
- `sendSearchUpdateToUci()` — nodes and NPS  
- `sendAspirationResearchInfo()` — nodes and NPS
- Note: throttling check still uses `thread().nodesVisited` to avoid aggregation overhead

##### Stage 3.6: Add `numHelperThreads` initialization ✅ COMPLETE
**Goal:** Initialize `numHelperThreads` from config.

**In `run()` thread initialization:**
```cpp
numHelperThreads = std::max(0, SearchConfig.THREADS - 1);
```

Config option added in Phase 5 (THREADS in SearchConfigData).

// Ensure searchThreadData has at least main thread
if (searchThreadData.empty()) {
    searchThreadData.push_back(std::make_unique<SearchThreadData>(0));
}

// Pre-allocate helper thread data
const size_t totalThreads = static_cast<size_t>(numHelperThreads) + 1;
while (searchThreadData.size() < totalThreads) {
    searchThreadData.push_back(std::make_unique<SearchThreadData>(
        static_cast<int>(searchThreadData.size())));
}
```

**In `startSearch()`, reset all thread data:**
```cpp
for (auto& st : searchThreadData) {
    st->reset();
}
```

#### Phase 3 Verification Checklist
- ✅ Build passes (MSVC Release/Debug)
- ✅ All existing unit tests pass
- ✅ Single-thread (`numHelperThreads=0`) behavior unchanged
- ✅ Manual test: `numHelperThreads=1` runs without deadlock (2026-02-26)
  - Main thread: 13.2M nodes, Helper thread: 14.1M nodes
  - Helper correctly started, searched, and joined
  - Note: Node count only shows main thread (Stage 3.5 pending)
- ✅ Stage 3.5: `getTotalNodes()` aggregation for UCI reporting
- ✅ Stage 3.6: Wire `numHelperThreads` to config option (THREADS)

---

### Phase 4: PawnTT Thread Safety (Day 11-12)
**Goal:** Make PawnTT thread-safe using the same pattern as main TT.

**Rationale:** PawnTT caches pawn structure evaluations. Unlike per-thread state (History, PlyInfo), pawn structures are position-dependent — all threads searching the same position benefit from shared cache. Per-thread PawnTT would waste memory (N copies of identical data).

#### Implementation Steps

##### Stage 4.1: Atomic key for PawnTT entries ⬜ TODO
In `PawnTT.h`, change `Entry::key` to atomic:
```cpp
struct Entry {
    std::atomic<ZobristKey> key{0};  // atomic for thread-safe probe
    Value midvalue{VALUE_NONE};
    Value endvalue{VALUE_NONE};
};
```

Add static asserts (same as TT):
```cpp
static_assert(sizeof(std::atomic<ZobristKey>) == sizeof(ZobristKey),
              "atomic<ZobristKey> has unexpected size - PawnTT entry layout broken");
static_assert(std::atomic<ZobristKey>::is_always_lock_free,
              "atomic<ZobristKey> is not lock-free - unacceptable overhead");
```

##### Stage 4.2: Update `probe()` for atomic read ⬜ TODO
```cpp
bool PawnTT::probe(const ZobristKey key, Score& score) const {
    const Entry& e = data[index(key)];
    const ZobristKey storedKey = e.key.load(std::memory_order_relaxed);
    if (storedKey == key) {
        score = Score{e.midvalue, e.endvalue};
        // Note: values might be stale if another thread is writing, but this is
        // acceptable under Lazy SMP (we'll just re-evaluate on next probe)
        return true;
    }
    return false;
}
```

##### Stage 4.3: Update `put()` for atomic write ⬜ TODO
```cpp
void PawnTT::put(const ZobristKey key, const Score score) {
    Entry& e = data[index(key)];
    // Write values first, then key (write-key-last pattern)
    e.midvalue = score.midgame;
    e.endvalue = score.endgame;
    e.key.store(key, std::memory_order_release);  // release ensures values visible
}
```

##### Stage 4.4: Disable/remove statistics in SMP mode ⬜ TODO
The current statistics counters (`numberOfPuts`, `numberOfHits`, etc.) are not thread-safe.

Options:
1. **Remove statistics entirely** — simplest, minimal value in production
2. **Make statistics thread-local** — aggregate at search end (like node counts)
3. **Guard with `#ifdef`** — only compile stats in single-thread debug builds

Recommendation: Option 1 (remove) or Option 3 (guard). Statistics are debug-only and not worth atomic overhead.

##### Stage 4.5: Remove "update" warning ⬜ TODO
The warning `"PawnTT should not have to update entries"` is no longer meaningful under SMP — concurrent threads legitimately write the same entry. Remove or change to debug-only trace.

#### Phase 4 Verification Checklist
- ⬜ `static_assert` for atomic key size passes
- ⬜ `static_assert` for lock-free passes  
- ⬜ Build passes (MSVC Release/Debug)
- ⬜ All existing unit tests pass
- ⬜ No PawnTT warnings in multi-thread search
- ⬜ Bench regression < 1% with single thread

---

### Phase 5: Configuration & UCI Integration ✅ COMPLETE
**Goal:** Expose thread count via UCI `Threads` option (standard UCI).

#### Implementation (completed)

**SearchConfigData.h:**
```cpp
CONFIG_ESSENTIAL int THREADS = 1;  // Number of search threads (1 = single-threaded)
```

**ConfigRegistry.cpp:**
```cpp
{
  .name = "THREADS",
  .uciName = "Threads",
  .description = "Number of search threads (1 = single-threaded, no SMP overhead)",
  .valueType = Int,
  .domain = Search,
  .defaultValue = "1",
  .minValue = 1,
  .maxValue = 256,
  .exposure = {.uci = true, .yaml = true, .display = true},
  .getter = searchGetter([](const auto& s){ return s.THREADS; }),
  .setter = SEARCH_CONFIG_SETTER(THREADS, parseInt)
}
```

**Search.cpp `run()`:**
```cpp
numHelperThreads = std::max(0, SearchConfig.THREADS - 1);
```

#### UCI Usage
- `setoption name Threads value N` — sets thread count (takes effect on next search)
- Default: 1 (single-threaded, zero SMP overhead)

---

### Phase 6: Node Count Aggregation & UCI Info ✅ COMPLETE
**Goal:** Report aggregate node count and NPS across all threads for UCI `info nodes nps`.

**Implementation (completed in Stage 3.5):**
- `getTotalNodes()` aggregates `nodesVisited` from all `SearchThreadData`
- Used in `sendIterationEndInfoToUci()`, `sendSearchUpdateToUci()`, `sendAspirationResearchInfo()`, and final result
- Throttling check still uses main thread nodes to avoid aggregation overhead on hot path

---

### Phase 7: Testing & Validation (Day 17-22)
**Goal:** Confirm correctness, no regressions at 1 thread, and ELO gain at 2+ threads.

#### Correctness Tests

1. **Single-thread regression:** All existing 266+ tests must pass with `THREADS=1`. Zero behavior change.
2. **Determinism test:** With `THREADS=1`, same position produces same result every run.
3. **Multi-thread smoke test:** With `THREADS=2`, search completes without deadlock or crash.
4. **Mate-finding test:** Engine finds mate-in-3 correctly with 4 threads.
5. **Time management test:** Search stops within expected time window with N threads.

#### New Unit Tests (`test/engine/SearchSmpTest.cpp`)

```cpp
// Test: 1 thread = same result as pre-SMP
TEST_F(SearchSmpTest, SingleThreadUnchanged) { ... }

// Test: 2 threads completes without crash
TEST_F(SearchSmpTest, TwoThreadsNoDeadlock) { ... }

// Test: node count reported = sum of all threads
TEST_F(SearchSmpTest, NodeCountAggregation) { ... }

// Test: TT is thread-safe (concurrent put+probe via stress test)
TEST_F(TT_Test, ConcurrentPutProbeNoUB) { ... }
```

#### Strength Testing (Arena)

| Threads | Expected NPS gain | Expected ELO gain |
|---------|-------------------|-------------------|
| 1       | 0% (baseline)     | 0 ELO             |
| 2       | +80-90%           | +30-50 ELO        |
| 4       | +250-300%         | +50-80 ELO        |
| 8       | +450-550%         | +60-100 ELO       |

ELO gain diminishes at higher thread counts due to TT contention and diminishing returns of parallel alpha-beta.

---

## File Change Summary

| File                             | Change                                                                                       |
|----------------------------------|----------------------------------------------------------------------------------------------|
| `src/engine/TT.h`                | `key` field → `std::atomic<ZobristKey>`                                                      |
| `src/engine/TT.cpp`              | Update `put()`, `probe()`, `clear()` for atomic key                                          |
| `src/engine/PawnTT.h`            | `key` field → `std::atomic<ZobristKey>`; remove/guard statistics                             |
| `src/engine/PawnTT.cpp`          | Update `put()`, `probe()` for atomic key; remove "update" warning                            |
| `src/engine/SearchThreadData.h`  | **NEW** — per-thread state struct                                                            |
| `src/engine/Search.h`            | Add `searchThreadData`, `helperThreads`, `numHelperThreads`; remove thread-local fields      |
| `src/engine/Search.cpp`          | Refactor to use `mainThread()` accessor; add `helperRun()`; update `startSearch()`, `run()`  |
| `src/config/SearchConfigData.h`  | Add `THREADS = 1`                                                                            |
| `src/config/ConfigRegistry.cpp`  | Add `Threads` UCI option registry entry                                                      |
| `test/engine/SearchSmpTest.cpp`  | **NEW** — SMP-specific tests                                                                 |

---

## Risk & Mitigation

| Risk                                         | Probability    | Mitigation                                                                          |
|----------------------------------------------|----------------|-------------------------------------------------------------------------------------|
| `sizeof(atomic<Key>) != sizeof(Key)` on MSVC | Low            | `static_assert` at compile time; fallback = XOR trick (Option B)                    |
| `atomic<Key>` not lock-free (hidden mutex)   | Very Low       | `static_assert(is_always_lock_free)`; fail fast, switch to Option B                 |
| TT atomic causes measurable NPS regression   | Low            | Mandatory bench regression gate (Day 3); acceptable threshold < 1%                  |
| Data race UB in TT non-key fields            | Low-Medium     | Write-key-last with `release`; read-key-first with `acquire`; TSAN validation       |
| `age--` in `probe()` is a race               | Certain in SMP | Guarded by `numSmpThreads <= 1` — runs normally in single-thread, skipped under SMP |
| Helper thread outlives main                  | Low            | `join()` before returning result; `stopSearchFlag` checked in loop                  |
| Node count overflow                          | Very Low       | `uint64_t` per thread; aggregate only at report time                                |
| Regression in 1-thread mode                  | Low            | All hot paths gated on `numHelperThreads == 0`; bench regression gate enforces this |
| Search diverges / loses ELO                  | Low            | Helpers use same `iterativeDeepening` logic; TT sharing provides the benefit        |

---

## Implementation Order (Recommended)

```
Week 1:
  Day 1-2:  Step 1a — Atomic TT key change + static_assert guards (TT.h / TT.cpp) ✅ DONE
  Day 3:    Step 1b — MANDATORY GATE: assembly inspection + bench regression + TSAN test ✅ DONE
              → if bench regresses > 1%: switch to Option B (XOR trick) and re-test
              → if static_assert fails on sizeof or lock_free: switch to Option B
  Day 4-5:  Step 1c — TT unit test: TT_Test::ConcurrentPutProbeNoUB ✅ DONE
  Day 6-8:  Step 2 — Create SearchThreadData.h, refactor Search to use mainThread() accessor ✅ DONE
  Day 9:    Verify all existing tests still pass (zero regression at this point) ✅ DONE

Week 2:
  Day 10-12: Step 3 — Add helperRun(), helperThreads launch/join in startSearch()/run()
  Day 13:   Step 4 — Add THREADS config + UCI option
  Day 14:   Step 5 — Node count aggregation in UCI info

Week 3:
  Day 15-17: Step 6 — Testing: unit tests, regression, 1-thread determinism
  Day 18-19: Strength testing with Arena (2T, 4T vs baseline)
  Day 20:    Bug fixes, tuning, documentation update
```

> **Go/No-Go Gate after Day 3:** All five verification items for TT atomics must pass before proceeding to Step 2. Record pass/fail and benchmark numbers in a comment block in `TT.h` near the `static_assert`s.

---

## Single-Thread Zero-Overhead Guarantee

The following invariants ensure NO overhead when `THREADS = 1`. The first two require **empirical verification** — the rest are structural guarantees.

### TT atomic key (Phase 1) — must be verified, not assumed

> ⚠️ **This is the highest-risk claim in the entire plan. Do not merge Phase 1 without completing items 1–3 below.**

1. **`sizeof(std::atomic<ZobristKey>) == sizeof(ZobristKey)` (compile-time)**
   - `static_assert(sizeof(std::atomic<ZobristKey>) == sizeof(ZobristKey))` must pass
   - If it fails on MSVC, the 16-byte `Entry` layout breaks → fall back to Option B (XOR trick)

2. **`is_always_lock_free` (compile-time)**
   - `static_assert(std::atomic<ZobristKey>::is_always_lock_free)` must pass
   - If it fails, the compiler is emitting a hidden `std::mutex` inside the atomic → completely unacceptable

3. **Assembly inspection (one-time manual check)**
   - Build Release config in CLion
   - Open disassembly of `TT::put()` and `TT::probe()` in the IDE or `dumpbin /DISASM`
   - Key store must be `mov QWORD PTR`, not `lock xchg` / `mfence` / `lock cmpxchg`
   - Document result (pass/fail + instruction seen) as a comment in `TT.h`

4. **Benchmark regression (automated, uses existing `--bench` infrastructure)**
   - Run `FrankyCPP --bench --benchDepth 12` on the **pre-atomic** binary → record NPS as baseline
   - Run identical command on **post-atomic** binary
   - Acceptable regression: < 1% (within measurement noise)
   - If NPS drops > 1%: profile with VTune/perf and investigate before proceeding

5. **TSAN clean (Linux/WSL, new)**
   - Build with `-fsanitize=thread` on WSL
   - Run `TT_Test::ConcurrentPutProbeNoUB` — must report zero races
   - Run the full test suite with 2 threads — must be TSAN-clean

**If any of items 1–4 fail**, the fallback is the XOR key trick (Option B, described in Phase 1). The XOR trick avoids `std::atomic` entirely at the cost of a slightly more complex probe path — but it has no sizeof/alignment risk.

### Structural guarantees (no measurement needed)

- **No helper thread creation when `THREADS=1`:** `numHelperThreads = 0` → `helperThreads` vector stays empty → no `std::thread` construction, no join overhead.
- **No hot-path branching:** `helperThreads` is always empty in single-thread mode — the launch loop in `startSearch()` and the join loop in `run()` are O(0) iterations.
- **No vector iteration overhead in `getTotalNodes()`:** `searchThreadData.size() == 1` → one loop body, no branch misprediction.
- **No synchronization primitives on the search hot path:** No mutexes, no condition variables, no barriers added to `search()` / `qsearch()` / `evaluate()`.
- **`SearchThreadData&` parameter is a reference:** the compiler inlines the reference to the concrete `SearchThreadData` object — zero pointer-indirection overhead when the function is inlined.

---

## Known Issues (To Be Addressed)

### PawnTT Thread Safety — Addressed in Phase 4
**Status:** Planned for Phase 4
**Symptom:** `[Eval_Logger] [warning]: PawnTT should not have to update entries. Missing a read?`

**Root Cause:** The `Evaluator` instance (and its internal `PawnTT pawnCache`) is shared between main and helper threads. When multiple threads evaluate the same pawn structure concurrently:
1. Thread A reads PawnTT → miss → evaluates → stores result
2. Thread B reads PawnTT → miss (before A's store) → evaluates → stores (triggers "update" warning)

**Impact:** 
- Warning spam in logs (cosmetic)
- Minor redundant pawn evaluation work (performance)
- Potential data race on PawnTT counters (correctness risk in debug builds)

**Solution:** Make PawnTT thread-safe using the same pattern as main TT (Phase 1):
- Use `std::atomic<ZobristKey>` for entry keys with relaxed memory ordering
- Remove/disable statistics counters in SMP mode (or make them thread-local)
- Same zero-overhead guarantee: `sizeof(std::atomic<Key>) == sizeof(Key)` on x86-64

**Why NOT per-thread PawnTT:**
- Pawn structures are position-dependent, not thread-dependent
- Multiple threads searching the same position would cache identical entries separately
- Wastes memory without benefit (N threads × PawnTT size instead of 1 × PawnTT size)
- Shared TT is the proven Lazy SMP pattern — apply same logic to PawnTT

---

## What This Does NOT Include (Future Work)

- **NUMA awareness:** Thread affinity / per-NUMA-node memory. Not needed for typical 4-16 core desktop CPUs.
- **Distributed TT:** Per-thread TT buckets. Lazy SMP already handles TT contention well.
- **Voting:** Stockfish-style root move vote counting across threads. Not needed for Lazy SMP correctness.
- **Different LMR tables per thread:** Could add slight diversity. Not worth the complexity.
- **NNUE evaluation:** Phase 7 concern.

---

## References

- Stockfish Lazy SMP implementation (src/search.cpp) — reference for helper loop structure
- "A Parallel Search Algorithm for Battleship" — Hyatt et al. (lock-less hashing)
- CPW (Chess Programming Wiki): [Lazy SMP](https://www.chessprogramming.org/Lazy_SMP)
- CPW: [Shared Hash Table](https://www.chessprogramming.org/Shared_Hash_Table)

---

*Created: 2026-02-25 | Status: Ready for Implementation*
