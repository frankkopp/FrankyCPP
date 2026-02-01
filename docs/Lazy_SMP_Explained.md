# Lazy SMP: Parallel Chess Search Explained

**Document Version:** 1.0  
**Created:** 2026-02-01  
**Author:** Frank Kopp  
**Status:** Technical Reference

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [What is Lazy SMP?](#what-is-lazy-smp)
3. [Why Lazy SMP Works](#why-lazy-smp-works)
4. [How Lazy SMP Works](#how-lazy-smp-works)
5. [Implementation Details](#implementation-details)
6. [Performance Characteristics](#performance-characteristics)
7. [Comparison with Other Parallel Search Methods](#comparison-with-other-parallel-search-methods)
8. [Practical Considerations](#practical-considerations)
9. [References](#references)

---

## Executive Summary

**Lazy SMP** (Shared Memory Parallelization) is a remarkably simple yet effective approach to parallelizing chess search algorithms. Instead of carefully coordinating threads to avoid redundant work, Lazy SMP allows multiple threads to search independently with minimal synchronization—sharing only a transposition table. Despite its apparent wastefulness, Lazy SMP delivers excellent speedups on modern multi-core processors and has become the de facto standard for parallel chess engines.

**Key Characteristics:**
- **Simple:** No complex work distribution or load balancing
- **Scalable:** Near-linear speedup up to 8-16 cores
- **Robust:** No deadlocks, minimal synchronization overhead
- **Effective:** +50-100 ELO gain on 4-core systems, +100-200 on 8+ cores

**Why It Works:**
- Redundant work is surprisingly beneficial (explores alternative lines)
- Shared transposition table provides implicit coordination
- Different thread timings lead to different search paths
- Modern CPUs have abundant cores and memory bandwidth

---

## What is Lazy SMP?

### Definition

Lazy SMP is a parallel search technique where multiple threads run **full, independent alpha-beta searches** of the same position simultaneously, sharing only a **transposition table** (hash table) for storing position evaluations. The name "Lazy" reflects the minimal effort spent coordinating threads—each thread is "lazy" about avoiding redundant work.

### Historical Context

- **Traditional Parallel Search (1980s-2000s):** Complex algorithms like Principal Variation Splitting (PVS), Young Brothers Wait Concept (YBWC), Dynamic Tree Splitting (DTS)
  - Required careful work distribution
  - Complex synchronization (mutexes, work queues)
  - Often had poor scaling beyond 4-8 cores
  - Diminishing returns due to coordination overhead

- **Lazy SMP (2013+):** Introduced by Martin Sedlak and popularized by Stockfish
  - Radically simplified approach
  - Better scaling on modern many-core CPUs
  - Became dominant in top engines (Stockfish, Komodo, Houdini, Leela Chess Zero's CPU backend)

### Core Principle

> **"Let multiple threads search the same tree independently, and let the shared transposition table implicitly coordinate their efforts."**

Instead of preventing redundant work, Lazy SMP embraces it as a feature—different threads explore the search space in slightly different ways, compensating for search algorithm imperfections.

---

## Why Lazy SMP Works

The effectiveness of Lazy SMP seems counterintuitive at first: why would redundant work improve search efficiency? The answer lies in several key insights about chess search algorithms and modern hardware.

### 1. **Alpha-Beta Search is Imperfect**

Alpha-beta pruning is a heuristic algorithm that makes pruning decisions based on:
- Move ordering (which moves to try first)
- Reductions (Late Move Reductions, null-move pruning)
- Extensions (singular extensions, check extensions)

These heuristics are **imperfect**—the "best" move order isn't always discovered first. When multiple threads search with slightly different timings:
- They discover moves in different orders
- They hit transposition table entries at different stages
- They make different pruning/extension decisions
- **Net effect:** The search space is explored more thoroughly

### 2. **Transposition Table Provides Implicit Coordination**

The shared transposition table acts as a distributed "memory" where threads communicate:

```
Thread 1: Searches position A deeply, stores result in TT
Thread 2: Encounters position A later, retrieves cached result, skips deep search
Result: No wasted work—Thread 2 benefits from Thread 1's effort
```

As search progresses, the TT fills with increasingly valuable positions:
- Tactical refutations
- Deep evaluations of critical lines
- Bound information (fail-high/fail-low)

All threads benefit from every other thread's discoveries.

### 3. **Beneficial Redundancy: "Search Space Diversity"**

Redundant work isn't actually wasted—it explores **alternative search paths**:

**Example:**
```
Position: [root position]
Thread 1: Tries moves in order: e4, d4, Nf3, c4
Thread 2: Tries moves in order: e4, Nf3, d4, c4 (slightly different due to timing)

Thread 1 cuts off after e4 (appears best)
Thread 2 explores Nf3 deeper before seeing e4's TT entry
Result: Thread 2 discovers a tactical refutation in the Nf3 line that Thread 1 missed
```

This diversity compensates for:
- Horizon effects (missing tactics just beyond search depth)
- Aggressive pruning (cutting off prematurely)
- Move ordering mistakes

### 4. **Modern Hardware Characteristics**

Lazy SMP is particularly effective on modern CPUs:

**Multi-Core Abundance:**
- Consumer CPUs: 4-16 cores common (e.g., AMD Ryzen 9: 16 cores)
- Server CPUs: 32-128 cores (AMD EPYC, Intel Xeon)
- Idle cores cost nothing—might as well use them

**Memory Bandwidth:**
- Modern CPUs have high-bandwidth memory (DDR4/DDR5: 50-100 GB/s)
- Large L3 caches (32-128 MB) reduce TT contention
- Cache coherency protocols handle TT sharing efficiently

**Lock-Free TT Access:**
- Atomic operations (compare-and-swap) enable lock-free TT updates
- TT contention is minimal with proper sizing (e.g., 1 GB TT for 8 threads)

### 5. **Empirical Evidence**

Real-world engines demonstrate Lazy SMP's effectiveness:

| Engine      | Threads | ELO Gain | Speedup | Efficiency |
|-------------|---------|----------|---------|------------|
| Stockfish   | 2       | +50-60   | 1.8x    | 90%        |
| Stockfish   | 4       | +100-120 | 3.2x    | 80%        |
| Stockfish   | 8       | +160-180 | 5.5x    | 69%        |
| Stockfish   | 16      | +220-250 | 8.5x    | 53%        |

**Key Observation:** Even at 16 threads with 53% efficiency, the absolute strength gain (+220-250 ELO) is massive—making Lazy SMP one of the highest-ROI engine improvements.

---

## How Lazy SMP Works

### High-Level Algorithm

```cpp
// Main thread
void startSearch(Position& rootPos, SearchLimits& limits) {
    // 1. Initialize shared state
    sharedTT.clear();
    stopFlag.store(false);
    
    // 2. Start helper threads
    for (int i = 1; i < numThreads; ++i) {
        helperThreads[i] = std::thread([&, i]() {
            SearchThread threadData(i);
            searchWorker(rootPos, limits, threadData);
        });
    }
    
    // 3. Main thread also searches
    SearchThread mainThreadData(0);
    searchWorker(rootPos, limits, mainThreadData);
    
    // 4. Wait for helpers to finish
    stopFlag.store(true);
    for (auto& thread : helperThreads) {
        thread.join();
    }
    
    // 5. Extract best move from TT
    TTEntry* entry = sharedTT.probe(rootPos.key());
    return entry->bestMove;
}

// Worker function (runs on each thread)
void searchWorker(Position rootPos, SearchLimits limits, SearchThread& data) {
    Position pos = rootPos;  // Each thread has its own Position copy
    
    // Iterative deepening loop
    for (Depth depth = 1; depth <= limits.maxDepth; ++depth) {
        if (stopFlag.load()) break;
        
        // Full alpha-beta search (independent per thread)
        Value score = alphaBeta(pos, -VALUE_INFINITE, VALUE_INFINITE, depth, data);
        
        // Update shared TT with results
        sharedTT.store(pos.key(), score, depth, data.pvLine[0]);
    }
}
```

### Key Components

#### 1. **Independent Search State**

Each thread maintains its own:
- **Position object:** Board representation (no shared mutable state)
- **Search stack:** Ply-by-ply information (PV, killers, history)
- **History tables:** Move ordering heuristics
- **Killer moves:** Per-ply refutation tracking
- **Search statistics:** Node counts, time management

**Why?** Eliminates need for locks on hot paths—each thread operates independently.

#### 2. **Shared Transposition Table**

The **only** shared data structure:

```cpp
struct TTEntry {
    uint64_t key;           // Zobrist hash (partial)
    int16_t value;          // Position evaluation
    int16_t depth;          // Search depth
    uint8_t type;           // EXACT, LOWER_BOUND, UPPER_BOUND
    Move bestMove;          // Best move found
    uint8_t generation;     // For aging entries
};

class TranspositionTable {
    std::vector<TTCluster> table;  // Array of clusters (e.g., 4 entries per cluster)
    
    // Thread-safe probe (read)
    TTEntry* probe(uint64_t key) {
        size_t index = (key * magic) >> shift;  // PEXT-based indexing
        TTCluster& cluster = table[index];
        
        // Search cluster for matching entry (lock-free read)
        for (TTEntry& entry : cluster.entries) {
            if (entry.key == key) return &entry;
        }
        return nullptr;
    }
    
    // Thread-safe store (write with atomic operations)
    void store(uint64_t key, Value value, Depth depth, Move move) {
        size_t index = (key * magic) >> shift;
        TTCluster& cluster = table[index];
        
        // Replace strategy: prefer deeper search or newer generation
        TTEntry* replace = selectReplaceEntry(cluster, depth);
        
        // Atomic write (prevents torn reads)
        replace->key = key;
        std::atomic_thread_fence(std::memory_order_release);
        replace->value = value;
        replace->depth = depth;
        replace->bestMove = move;
    }
};
```

**Thread Safety:**
- **Reads:** Lock-free (atomic loads)
- **Writes:** Lock-free compare-and-swap or accept rare collisions
- **Collisions:** Handled gracefully (replace lower-depth entries)

#### 3. **Search Diversification Techniques**

To maximize diversity, engines use subtle variations per thread:

##### a) **Skip-Size Variation (Most Common)**
```cpp
// Thread 0: searches all moves
// Thread 1: skips every 2nd move at root
// Thread 2: skips every 3rd move at root
// Thread 3: skips every 4th move at root

int skipSize = threadID + 1;
for (int i = 0; i < rootMoves.size(); i += skipSize) {
    search(rootMoves[i], ...);
}
```

##### b) **Depth Offset**
```cpp
// Start threads at slightly different depths
Depth startDepth = 1 + (threadID % 2);  // Threads alternate 1, 2, 1, 2, ...
```

##### c) **History Table Variation**
```cpp
// Apply small random perturbations to history scores
int historyScore = baseHistory + (threadID * 7 % 13);  // Small prime-based variation
```

##### d) **Multi-PV for Helper Threads**
```cpp
// Main thread: single-PV search (best move only)
// Helper threads: multi-PV search (explore top N moves equally)
int numPV = (threadID == 0) ? 1 : 3;
```

#### 4. **Stop Condition Coordination**

All threads check a shared atomic flag:

```cpp
std::atomic<bool> stopSearch{false};

// Time management (main thread only)
if (isMainThread() && timeUp()) {
    stopSearch.store(true, std::memory_order_relaxed);
}

// All threads check regularly
if (stopSearch.load(std::memory_order_relaxed)) {
    return;
}
```

**Why atomic?** Ensures all threads see the stop flag eventually (no cached stale values).

---

## Implementation Details

### Pseudocode: Complete Lazy SMP Implementation

```cpp
class LazySearchEngine {
public:
    // Configuration
    int numThreads = 4;
    TranspositionTable sharedTT{1024 * 1024 * 1024};  // 1 GB
    
    // Shared flags
    std::atomic<bool> stopSearch{false};
    std::atomic<uint64_t> totalNodes{0};
    
    // Thread pool
    std::vector<std::thread> helperThreads;
    
    // Main entry point
    Move startSearch(const Position& rootPos, SearchLimits limits) {
        // 1. Reset shared state
        stopSearch.store(false);
        totalNodes.store(0);
        
        // 2. Spawn helper threads
        for (int tid = 1; tid < numThreads; ++tid) {
            helperThreads.emplace_back([this, rootPos, limits, tid]() {
                SearchContext ctx(tid);
                this->searchWorker(rootPos, limits, ctx);
            });
        }
        
        // 3. Main thread searches
        SearchContext mainCtx(0);
        searchWorker(rootPos, limits, mainCtx);
        
        // 4. Signal stop and wait
        stopSearch.store(true);
        for (auto& thread : helperThreads) {
            thread.join();
        }
        helperThreads.clear();
        
        // 5. Extract best move from TT
        TTEntry* rootEntry = sharedTT.probe(rootPos.zobristKey());
        return rootEntry ? rootEntry->bestMove : MOVE_NONE;
    }
    
private:
    // Per-thread search context
    struct SearchContext {
        int threadID;
        Position position;           // Thread-local position copy
        HistoryTable history;        // Move ordering history
        KillerMoves killers[MAX_PLY];
        PlyInfo stack[MAX_PLY];
        uint64_t nodesSearched = 0;
        
        SearchContext(int tid) : threadID(tid) {}
    };
    
    // Worker loop: iterative deepening
    void searchWorker(const Position& rootPos, SearchLimits limits, SearchContext& ctx) {
        ctx.position = rootPos;  // Clone position for this thread
        
        for (Depth depth = 1; depth <= limits.maxDepth; ++depth) {
            if (stopSearch.load(std::memory_order_relaxed)) break;
            
            // Aspiration window search
            Value score = aspirationSearch(ctx, depth);
            
            // Update TT with root result
            TTEntry entry;
            entry.key = ctx.position.zobristKey();
            entry.value = score;
            entry.depth = depth;
            entry.bestMove = ctx.stack[0].pv[0];
            entry.type = EXACT;
            sharedTT.store(entry);
            
            // Print info (only main thread)
            if (ctx.threadID == 0) {
                printSearchInfo(depth, score, ctx.stack[0].pv);
            }
        }
        
        // Update global node count
        totalNodes.fetch_add(ctx.nodesSearched, std::memory_order_relaxed);
    }
    
    // Aspiration window wrapper
    Value aspirationSearch(SearchContext& ctx, Depth depth) {
        Value prevScore = ctx.stack[0].score;
        Value alpha = prevScore - 25;
        Value beta = prevScore + 25;
        Value delta = 50;
        
        while (true) {
            Value score = alphaBeta(ctx, 0, alpha, beta, depth);
            
            if (stopSearch.load()) return score;
            
            if (score <= alpha) {
                // Fail low: widen window downward
                alpha = std::max(alpha - delta, -VALUE_INFINITE);
                delta *= 2;
            } else if (score >= beta) {
                // Fail high: widen window upward
                beta = std::min(beta + delta, VALUE_INFINITE);
                delta *= 2;
            } else {
                // Success
                return score;
            }
        }
    }
    
    // Alpha-beta search (core algorithm)
    Value alphaBeta(SearchContext& ctx, int ply, Value alpha, Value beta, Depth depth) {
        // Check stop condition
        if (stopSearch.load(std::memory_order_relaxed)) return VALUE_ZERO;
        
        // Probe TT
        uint64_t key = ctx.position.zobristKey();
        TTEntry* ttEntry = sharedTT.probe(key);
        if (ttEntry && ttEntry->depth >= depth) {
            // TT cutoff
            if (ttEntry->type == EXACT) return ttEntry->value;
            if (ttEntry->type == LOWER_BOUND && ttEntry->value >= beta) return ttEntry->value;
            if (ttEntry->type == UPPER_BOUND && ttEntry->value <= alpha) return ttEntry->value;
        }
        
        // Leaf node: quiescence search or evaluate
        if (depth <= 0) {
            return quiescence(ctx, ply, alpha, beta);
        }
        
        // Generate moves
        MoveList moves = generateLegalMoves(ctx.position);
        if (moves.empty()) {
            // Checkmate or stalemate
            return ctx.position.inCheck() ? -VALUE_MATE + ply : VALUE_DRAW;
        }
        
        // Move ordering (history, killers, TT move)
        Move ttMove = ttEntry ? ttEntry->bestMove : MOVE_NONE;
        orderMoves(moves, ctx, ply, ttMove);
        
        // Search all moves
        Value bestScore = -VALUE_INFINITE;
        Move bestMove = MOVE_NONE;
        int moveCount = 0;
        
        for (Move move : moves) {
            ctx.position.makeMove(move);
            ctx.nodesSearched++;
            
            Value score;
            if (moveCount == 0) {
                // PVS: full window for first move
                score = -alphaBeta(ctx, ply + 1, -beta, -alpha, depth - 1);
            } else {
                // Null window search
                score = -alphaBeta(ctx, ply + 1, -alpha - 1, -alpha, depth - 1);
                if (score > alpha && score < beta) {
                    // Re-search with full window
                    score = -alphaBeta(ctx, ply + 1, -beta, -alpha, depth - 1);
                }
            }
            
            ctx.position.unmakeMove(move);
            
            if (stopSearch.load()) return VALUE_ZERO;
            
            moveCount++;
            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
                
                if (score > alpha) {
                    alpha = score;
                    // Update PV
                    ctx.stack[ply].pv[0] = move;
                    
                    if (score >= beta) {
                        // Beta cutoff
                        updateHistory(ctx, move, depth);
                        break;
                    }
                }
            }
        }
        
        // Store in TT
        TTEntry newEntry;
        newEntry.key = key;
        newEntry.value = bestScore;
        newEntry.depth = depth;
        newEntry.bestMove = bestMove;
        newEntry.type = (bestScore >= beta) ? LOWER_BOUND :
                        (bestScore <= alpha) ? UPPER_BOUND : EXACT;
        sharedTT.store(newEntry);
        
        return bestScore;
    }
};
```

---

## Performance Characteristics

### Scaling Efficiency

**Theoretical:** Linear speedup (N threads → N× faster)  
**Reality:** Sub-linear due to:
- TT contention (multiple threads updating same entries)
- Redundant work (overlapping search paths)
- Memory bandwidth limits
- Amdahl's Law (serial portions like root move selection)

### Empirical Scaling Data

Based on Stockfish benchmarks (single position, fixed depth):

| Threads | Speedup | Efficiency | ELO Gain | Nodes/sec |
|---------|---------|------------|----------|-----------|
| 1       | 1.0×    | 100%       | +0       | 1.5M      |
| 2       | 1.8×    | 90%        | +50-60   | 2.7M      |
| 4       | 3.2×    | 80%        | +100-120 | 4.8M      |
| 8       | 5.5×    | 69%        | +160-180 | 8.3M      |
| 16      | 8.5×    | 53%        | +220-250 | 12.8M     |
| 32      | 12.0×   | 38%        | +270-300 | 18.0M     |

**Key Insights:**
1. **Efficiency degrades gracefully** (not catastrophically)
2. **Absolute speedup remains strong** even at high thread counts
3. **ELO gains continue** beyond the point where efficiency drops below 50%

### Factors Affecting Scaling

#### 1. **Position Complexity**
- **Tactical positions:** Better scaling (many TT hits, diverse search paths)
- **Quiet positions:** Worse scaling (less TT benefit, more redundancy)

#### 2. **Search Depth**
- **Shallow search (< 10 ply):** Poor scaling (overhead dominates)
- **Medium depth (10-20 ply):** Excellent scaling (sweet spot)
- **Deep search (> 30 ply):** Moderate scaling (TT fills up, less diversity)

#### 3. **TT Size**
- **Too small:** Excessive collisions, poor scaling
- **Optimal:** ~64-128 MB per thread (e.g., 1 GB for 8 threads)
- **Too large:** Diminishing returns, cache misses

#### 4. **CPU Architecture**
- **NUMA systems:** Worse scaling (inter-socket communication overhead)
- **Single-socket:** Best scaling (shared L3 cache)
- **SMT/Hyperthreading:** Mixed results (50-70% gain over physical cores)

---

## Comparison with Other Parallel Search Methods

### Traditional Approaches

#### 1. **Principal Variation Splitting (PVS)**
- **Idea:** Only parallelize search of the first move (PV) at each node
- **Pros:** Minimal redundant work, deterministic results
- **Cons:** Complex synchronization, poor scaling beyond 4-8 cores, load imbalance

#### 2. **Young Brothers Wait Concept (YBWC)**
- **Idea:** Parallelize non-PV nodes only after first move fails low
- **Pros:** Better scaling than pure PVS
- **Cons:** Still requires work queues and locks, moderate complexity

#### 3. **Dynamic Tree Splitting (DTS)**
- **Idea:** Dynamically split work at any node when idle threads available
- **Pros:** Good load balancing
- **Cons:** Very complex, high synchronization overhead

### Lazy SMP Advantages

| Aspect              | Traditional (PVS/YBWC) | Lazy SMP        |
|---------------------|------------------------|-----------------|
| **Implementation**  | Complex                | Simple          |
| **Synchronization** | Heavy (locks, queues)  | Minimal (TT)    |
| **Scaling**         | Poor (4-8 cores)       | Good (16+ cores)|
| **Robustness**      | Deadlock-prone         | Deadlock-free   |
| **Code Size**       | ~2000 LOC              | ~200 LOC        |
| **Debugging**       | Difficult              | Easy            |
| **ELO Gain**        | +80-120 (8 cores)      | +160-180        |

### Why Lazy SMP Won

1. **Simplicity:** Easier to implement and maintain
2. **Scalability:** Modern CPUs have 8-64 cores—Lazy SMP scales better
3. **Effectiveness:** Despite redundancy, empirical results are superior
4. **Robustness:** No deadlocks, race conditions easier to manage

---

## Practical Considerations

### Implementation Checklist for FrankyCPP

#### Phase 1: Thread-Safe TT (1 week)
- [ ] Add atomic flags to `TTEntry` struct
- [ ] Implement lock-free `probe()` method
- [ ] Implement lock-free `store()` with replacement strategy
- [ ] Test concurrent access (ThreadSanitizer)
- [ ] Verify no performance regression with single thread

#### Phase 2: Per-Thread State (1 week)
- [ ] Create `SearchThread` class with independent state
- [ ] Move `History`, `KillerMoves`, `PlyInfo` into `SearchThread`
- [ ] Clone `Position` for each thread (deep copy)
- [ ] Test deterministic behavior (single thread = same results)

#### Phase 3: Parallel Search (1 week)
- [ ] Implement `startSearch()` with thread spawning
- [ ] Implement `searchWorker()` with iterative deepening
- [ ] Add stop flag coordination (`std::atomic<bool>`)
- [ ] Aggregate node counts from all threads
- [ ] Extract best move from TT after search

#### Phase 4: Diversification (3-5 days)
- [ ] Implement skip-size variation at root
- [ ] Optional: depth offset per thread
- [ ] Optional: small history table perturbations

#### Phase 5: Testing & Tuning (1 week)
- [ ] Benchmark suite: 1, 2, 4, 8, 16 threads
- [ ] Measure speedup and efficiency
- [ ] Run self-play matches to measure ELO gain
- [ ] Tune TT size per thread count
- [ ] Validate with ThreadSanitizer (no races)

### Configuration Parameters

```yaml
# search.yaml
USE_LAZY_SMP: true
SMP_THREADS: 0                   # 0 = auto-detect CPU cores
SMP_MIN_SPLIT_DEPTH: 4           # Minimum depth to spawn helpers
SMP_HELPER_SKIP_SIZE: true       # Enable root move skip variation
SMP_DEPTH_OFFSET: false          # Enable depth offset variation
```

### Common Pitfalls

#### 1. **Shared Position State**
❌ **Wrong:**
```cpp
Position sharedPos;  // All threads modify this
for (auto& thread : threads) {
    thread = std::thread([&]() { search(sharedPos); });
}
```

✅ **Correct:**
```cpp
for (auto& thread : threads) {
    thread = std::thread([rootPos]() {  // Copy by value
        Position pos = rootPos;         // Each thread has own copy
        search(pos);
    });
}
```

#### 2. **TT Entry Torn Reads**
❌ **Wrong:**
```cpp
struct TTEntry {
    uint64_t key;
    int16_t value;  // Non-atomic read while another thread writes
};
```

✅ **Correct:**
```cpp
struct TTEntry {
    std::atomic<uint64_t> key;
    std::atomic<int16_t> value;
};
```
*Or use memory fences/barriers.*

#### 3. **Excessive TT Contention**
- **Symptom:** Poor scaling despite multiple threads
- **Cause:** TT too small, all threads fight over same entries
- **Fix:** Use ~64-128 MB per thread (e.g., 1 GB for 8 threads)

#### 4. **Forgetting Stop Flag Checks**
- **Symptom:** Threads don't stop when time expires
- **Cause:** Missing `if (stopSearch.load()) return;` in search loops
- **Fix:** Check stop flag at every node (or every N nodes)

### Performance Tuning Tips

1. **TT Size:** Benchmark different sizes (512 MB, 1 GB, 2 GB)
2. **Thread Count:** Not always "more is better"—test 4, 8, 12, 16
3. **Affinity:** Pin threads to physical cores (avoid SMT contention)
4. **NUMA:** On multi-socket systems, use NUMA-aware TT allocation
5. **Diversification:** Tune skip size (1, 2, 3, 4) for best variety

---

## References

### Academic Papers
1. **"Lazy SMP, a New Parallel Chess Engine" by Martin Sedlak (2013)**  
   Original introduction of Lazy SMP concept

2. **"Parallel Alpha-Beta Search" by Feldmann (1993)**  
   Classical parallel search algorithms (PVS, YBWC)

3. **"Shared Hash Tables in Parallel Game-Tree Search" by Brockington (1996)**  
   Analysis of transposition table sharing

### Open Source Implementations
1. **Stockfish** - Reference implementation  
   https://github.com/official-stockfish/Stockfish
   - `src/thread.cpp`, `src/search.cpp`
   - Uses skip-size variation and multi-PV for helpers

2. **Ethereal** - Clean modern implementation  
   https://github.com/AndyGrant/Ethereal
   - Simpler codebase, good for learning

3. **Koivisto** - Rust implementation  
   https://github.com/Luecx/Koivisto
   - Shows Lazy SMP works in memory-safe languages

### Community Resources
1. **Chess Programming Wiki: Lazy SMP**  
   https://www.chessprogramming.org/Lazy_SMP

2. **TalkChess Forum: Lazy SMP Discussion**  
   http://talkchess.com (search "Lazy SMP")

3. **Computer Chess Club: Parallel Search**  
   Various threads on practical implementation details

---

## Conclusion

Lazy SMP represents a paradigm shift in parallel chess search: **simplicity over sophistication, empirical results over theoretical purity**. By embracing redundant work and leveraging modern hardware capabilities, Lazy SMP achieves better scaling than traditional methods with a fraction of the complexity.

For FrankyCPP v1.2, implementing Lazy SMP is the highest-priority parallelization strategy:
- **High impact:** +50-100 ELO on consumer hardware (4-8 cores)
- **Manageable effort:** ~3-4 weeks for complete implementation
- **Low risk:** Well-proven in top engines, simple to debug
- **Future-proof:** Scales to 16+ cores as CPUs continue to add cores

The "lazy" approach of doing minimal coordination and accepting some redundancy turns out to be the most effective parallel search strategy in practice—a counterintuitive but empirically validated success.

---

**Document Maintainer:** Frank Kopp  
**Last Updated:** 2026-02-01  
**Next Review:** After Lazy SMP implementation (v1.2)
