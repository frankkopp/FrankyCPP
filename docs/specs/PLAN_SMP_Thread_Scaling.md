# PLAN: SMP Thread Scaling & Performance Optimization

**Created:** 2026-04-10
**Status:** Planning
**Priority:** High — SMP is a core differentiator; current scaling is 3.3× at 12T vs Stockfish's 10.9×

---

## 1. Problem Statement

FrankyCPP's SMP (Lazy SMP) thread scaling is dramatically behind Stockfish on the same hardware. Bench measurements on an i9-13900KF (24C/32T hybrid P+E cores) reveal:

### NPS Scaling Comparison (Bench, FrankyCPP = average of 2 runs excluding anomalies)

| Threads | FrankyCPP NPS | FC Scaling | SF NPS     | SF Scaling | FC Efficiency | SF Efficiency |
|---------|---------------|------------|------------|------------|---------------|---------------|
| 1       | 2,645,000     | 1.00×      | 1,512,432  | 1.00×      | 100%          | 100%          |
| 2       | 4,377,000     | 1.65×      | 3,079,527  | 2.04×      | 83%           | 102%          |
| 3       | 5,755,000     | 2.18×      | 5,143,708  | 3.40×      | 73%           | 113%          |
| 4       | 6,634,000     | 2.51×      | 7,059,649  | 4.67×      | 63%           | 117%          |
| 5       | 7,682,000     | 2.90×      | 8,848,050  | 5.85×      | 58%           | 117%          |
| 6       | 7,793,000     | 2.95×      | 10,274,503 | 6.79×      | 49%           | 113%          |
| 7       | 8,261,000     | 3.12×      | 12,371,728 | 8.18×      | 45%           | 117%          |
| 8       | 8,602,000     | 3.25×      | 13,990,992 | 9.25×      | 41%           | 116%          |
| 9       | 8,284,000     | 3.13×      | 14,977,235 | 9.90×      | 35%           | 110%          |
| 10      | 8,550,000     | 3.23×      | 16,124,289 | 10.66×     | 32%           | 107%          |
| 11      | 8,436,000 ¹   | 3.19×      | 17,038,850 | 11.27×     | 29%           | 102%          |
| 12      | 8,580,000     | 3.24×      | 17,644,678 | 11.67×     | 27%           | 97%           |
| 13      | 8,524,000     | 3.22×      | 17,948,881 | 11.87×     | 25%           | 91%           |
| 14      | 8,573,000 ¹   | 3.24×      | 18,102,633 | 11.97×     | 23%           | 85%           |
| 15      | 8,687,000     | 3.28×      | 18,928,729 | 12.52×     | 22%           | 83%           |
| 16      | 9,272,000 ¹   | 3.50×      | 19,690,064 | 13.02×     | 22%           | 81%           |

*(¹ = one of two runs had an anomalous spike at this thread count; shown value is the normal run)*
*(Efficiency = scaling / threads × 100%. >100% means super-linear due to TT sharing benefit.)*
*(Hardware: i9-13900KF — 8 P-cores/16 HT + 16 E-cores = 24C/32T)*

#### ⚠️ FrankyCPP NPS Anomalies at High Thread Counts

Multiple bench runs show erratic NPS spikes at varying thread counts ≥8:

| Run   | Anomalous Thread(s) | NPS at anomaly | Adjacent NPS | Nodes     | Time        |
|-------|---------------------|----------------|--------------|-----------|-------------|
| Run 1 | 11T, 16T            | 13.8M, 14.5M   | ~8.5M        | 201M/230M | 14.6s/15.9s |
| Run 2 | 14T                 | 14.0M          | ~8.5M        | 227M      | 16.2s       |

The anomalies are **non-deterministic** — they hit different thread counts on different runs:
- Run 1: 11T and 16T spiked; 14T was normal (8.6M)
- Run 2: 14T spiked; 11T (8.4M) and 16T (9.3M) were normal

This proves the spikes are **not tied to specific thread counts or CPU topology**. Instead, they
are caused by **SMP search instability**: at high thread counts, the non-deterministic order of
TT writes occasionally creates a "lucky" cutoff pattern that dramatically reduces the search tree
for some positions. The result is fewer total nodes AND less wall time, producing an inflated NPS.

**Run 2 full data (for reference):**
```
1T:  2,649,351 NPS  (1.00×)   38M nodes   14.5s
2T:  4,369,885 NPS  (1.65×)   65M nodes   14.9s
3T:  5,744,844 NPS  (2.17×)   98M nodes   17.0s
4T:  6,692,945 NPS  (2.53×)  120M nodes   18.0s
5T:  7,511,525 NPS  (2.84×)  140M nodes   18.6s
6T:  7,848,528 NPS  (2.96×)  164M nodes   20.9s
7T:  8,369,243 NPS  (3.16×)  162M nodes   19.3s
8T:  8,567,144 NPS  (3.23×)  195M nodes   22.7s
9T:  8,321,233 NPS  (3.14×)  219M nodes   26.3s
10T: 8,652,656 NPS  (3.27×)  246M nodes   28.4s
11T: 8,436,211 NPS  (3.18×)  227M nodes   26.9s
12T: 8,519,111 NPS  (3.22×)  235M nodes   27.6s
13T: 8,622,064 NPS  (3.25×)  219M nodes   25.4s
14T: ⚠️ 14,007,207 NPS (5.29×) 227M nodes 16.2s
15T: 8,701,972 NPS  (3.28×)  233M nodes   26.7s
16T: 9,272,166 NPS  (3.50×)  287M nodes   31.0s
```

**Stable NPS range (excluding anomalies):** 8.3-8.7M for 7-16T → **hard ceiling ~8.5M NPS**.

Key observations:
- **FrankyCPP hits a hard NPS wall at 5-6 threads** (~8.5M NPS). Adding threads 7-16 yields near-zero throughput gain.
- **NPS anomalies are non-deterministic** — spikes to ~14M hit random thread counts ≥8 on different runs, caused by SMP search tree instability, not real throughput gains.
- **Stockfish achieves super-linear scaling** up to ~8T (>100% efficiency), indicating TT sharing actively helps.
- **SF maintains >80% efficiency even at 16T** — scaling only gently degrades past the 8 P-cores (into E-core territory).
- **At 12T, Stockfish has 3.6× better scaling** (11.67× vs 3.22×).
- **SF at 16T (19.7M NPS) is 2.3× FrankyCPP's stable peak NPS** (~8.5M, excluding anomalies).
- **Crossover point: ~3-4T** — despite FrankyCPP having 1.7× higher 1T NPS (faster per-node eval), SF overtakes in absolute NPS by 4 threads due to superior scaling.

### ELO Impact

Thread scaling matches (200 games, 5+0.05, vs FrankyCPP 1T):
```
1T:  -5.2 ELO  (baseline)
2T:  +20.9 ELO
4T:  +34.9 ELO
7T:  +36.6 ELO  (best)
8T:  +15.6 ELO
12T: +29.6 ELO
```

Maximum ELO gain from SMP: **~35 ELO**. Expected for a well-scaling engine: **80-120+ ELO at 8T**.

> Note: 100 games are insufficient for statistically significant conclusions (±60-70 ELO CI).

---

## 2. Root Cause Hypotheses (To Be Validated in Research Phase)

### 2.1 Memory-Bound vs Compute-Bound Architecture

This is the fundamental issue. VTune profiling confirmed:

| Aspect             | FrankyCPP                                 | Stockfish                           |
|--------------------|-------------------------------------------|-------------------------------------|
| Evaluation         | Classical (PSTs, table lookups, bitboard) | NNUE (matrix multiply, ALU-heavy)   |
| Per-node profile   | Low compute, high memory access           | High compute, lower memory pressure |
| Primary bottleneck | **Memory bandwidth**                      | **ALU/compute**                     |
| SMP scaling        | Saturates at ~8M NPS (memory wall)        | Scales until cores exhausted        |

FrankyCPP's search hot path is dominated by memory operations:
- **TT::probe** — random access into a large hash table (LLC miss-heavy)
- **Attacks::sliderLookup** — PEXT table lookups (another memory-bound operation)
- **PST lookups** — incrementally maintained but still table-driven
- **Evaluator::evaluate** — bitboard table lookups (non-slider attack tables, etc.)

All these operations go through the same shared memory bus. Adding cores doesn't add memory bandwidth on consumer CPUs, so throughput saturates early.

NNUE's advantage isn't just better evaluation quality — it shifts the bottleneck from memory to compute. Matrix multiply-accumulate operations live in each core's registers and L1 cache. Each additional core brings its own ALU capacity → near-linear scaling.

### 2.2 TT Contention & Statistics Overhead

Current TT has shared mutable state accessed from all threads on the hot path:

```cpp
// TT::put() — called from every thread on every search node
numberOfPuts++;       // ← non-atomic increment, data race, false sharing
numberOfCollisions++; // ← same
numberOfOverwrites++; // ← same

// TT::probe() — called from every thread on every search node
numberOfProbes++;     // ← same
numberOfHits++;       // ← same
numberOfMisses++;     // ← same
```

These 6+ counters live in the same cache line as core TT data. Every increment from any thread invalidates that cache line for all other threads (false sharing). This is a constant drag on throughput even though the values are only used for diagnostics.

Similarly, `PawnTT` has the same shared counter pattern.

### 2.3 Hybrid CPU Architecture (i9-13900KF)

The test machine has:
- 8 P-cores (Performance, HT → 16 logical) @ up to 5.8 GHz
- 16 E-cores (Efficiency, no HT) @ up to 4.3 GHz

With >8 search threads, some land on E-cores (~40% slower). These slower threads:
- Produce shallower TT entries that may overwrite deeper P-core entries
- Contribute proportionally less useful work
- Still consume full TT/memory bandwidth

FrankyCPP has no awareness of core heterogeneity.

### 2.4 Short Time Control Amplification

At bullet (5+0.05), ~0.1-0.5s per move:
- Main thread reaches only depth 12-15
- Helper threads at similar depths have minimal room to diverge
- Thread coordination overhead is a significant fraction of total search time
- SMP benefit only materializes at deeper search (longer TC)

---

## 3. Research Phase — Understand Before Fixing

Before implementing any changes, we need to answer several open questions. The current data
has too many unknowns and the hypotheses in Section 2 are partially speculative.

### R1. Bench Stability & Reproducibility
**Status:** ✅ Partially answered — anomalies confirmed as non-deterministic (see Section 1).
**Goal:** Determine if bench NPS is a reliable measurement at various thread counts.
**Method:**
- Run `bench_thread_scaling.ps1` **3× consecutive** for 1-16T and compare results
- Compute standard deviation per thread count
- If NPS varies >5% between runs at the same thread count, bench is unreliable for SMP measurement

**Findings so far (2 runs):**
- 1T-6T: NPS is stable across runs (within ~2%)
- 7T-16T (excluding anomalies): NPS is stable in the 8.3-8.7M range
- Anomalies (NPS ~14M) hit different thread counts on different runs: 11T+16T in Run 1, 14T in Run 2
- The anomalies correlate with ~30-40% fewer total nodes AND proportionally less time

**Remaining work:**
- One more run to confirm the pattern (3 total runs)
- Compute per-thread-count standard deviation
- Run SF bench 3× to establish baseline variability

### R2. Code Review: Bench → Search → Thread Lifecycle (Top-Down)
**Goal:** Deep, careful review of the entire code path from bench invocation to per-thread search
completion. Identify any bugs, race conditions, or subtle issues that could explain the NPS anomalies
or degrade SMP effectiveness.

**Method — review in outside-to-inside order:**

1. **`Benchmark::run()`** (`src/engine/Benchmark.cpp`)
   - How are positions iterated? Is `newGame()` between each position correct?
   - Is `waitWhileSearching()` guaranteed to block until ALL threads finish?
   - Is `searchResult.nodes` captured after all helpers are joined?
   - Timing: `elapsedSince(searchStart)` — does it include thread join time or just search time?
   - Could `newGame()` between positions create TT cold-start effects that interact with SMP?

2. **`Search::startSearch()`** (`src/engine/Search.cpp`)
   - How are `searchThreadData` instances allocated, reset, and reused?
   - `resetSearchState()` — is there any shared mutable state NOT being reset?
   - `searchThreadData` grows but never shrinks — does `getTotalNodes()` iterate stale entries?
   - How is `numHelperThreads` calculated? Is it `THREADS - 1`?
   - Verify: `searchThreadData.size()` vs active thread count — could stale entries contribute nodes?

3. **Thread launch & join (`launchHelperThreads()` / `joinHelperThreads()`)**
   - How are helpers launched? `std::thread`, `std::async`, or thread pool?
   - Is there a synchronization barrier ensuring all helpers start before main begins?
   - `joinHelperThreads()` — is it a hard `thread::join()` or a flag-based wait?
   - Could a helper thread still be writing to TT after `joinHelperThreads()` returns?
   - Is `stopSearchFlag` propagated reliably to all threads?

4. **`Search::iterativeDeepening()`**
   - Main thread vs helper thread entry point — same function or different?
   - How does the main thread decide to stop? Depth limit? Time limit?
   - How do helpers know to stop? Same `stopSearchFlag`? What if main finishes depth 12 and
     a helper is mid-iteration at depth 10 — does it complete or abort?
   - `SMP_HELPER_START_DEPTH` — helpers skip initial iterations. How is this implemented?
     Do they start from `iterativeDeepening(startDepth=4)` or skip within the loop?

5. **`getTotalNodes()` and node counting**
   - Each thread writes to `st->nodesVisited` (non-atomic, per-thread). Verify no cross-thread writes.
   - `getTotalNodes()` sums ALL `searchThreadData` entries. If vector has 16 entries but only 8 threads
     ran, are entries 9-15 guaranteed to be zero? (They should be from `resetForNewSearch`, but verify.)
   - Is `nodesVisited` incremented at the right granularity? (Per-node, per-move, per-position?)

6. **`selectBestThread()`**
   - How does this interact with node counting? Does it affect which result is reported?
   - Could it cause the "fewer nodes" anomaly if it picks a thread that searched a smaller tree?
   - No — `result.nodes = getTotalNodes()` is the sum of all threads, not just the best thread's.

7. **TT interactions under SMP**
   - `TT::probe()` and `TT::put()` — are they truly lock-free? No mutex, no spinlock?
   - Key verification: XOR-based corruption detection. Could a torn write cause a "valid-looking"
     but wrong entry that dramatically changes the search tree?
   - `ageEntries()` is skipped under SMP (`numHelperThreads > 0`). Is there any age-related
     logic that still runs per-probe that could cause issues?
   - PawnTT: shared across threads via pointer. Same thread-safety concerns.

8. **Position copying**
   - Each thread gets `position = rootPosition` in `resetForNewSearch()`. Is `Position::operator=`
     a deep copy? Does it copy the move history, Zobrist key stack, etc.?
   - Could a shallow copy cause threads to share internal state (e.g., history stack)?

**Questions to answer:**
- Is there a race condition or off-by-one in how nodes are counted at search end?
- Could thread join timing explain the time anomalies (e.g., not waiting for slow threads)?
- Are there any hidden shared mutable state paths beyond the known TT statistics counters?
- Is there a bug where some threads don't properly stop at the depth limit under SMP?
- Could TT entry corruption (torn writes) cause non-deterministic tree size changes?

### R3. Per-Position Analysis at Different Thread Counts
**Goal:** Understand WHERE the search tree changes with threads.
**Method:**
- Instrument `Benchmark::run()` to log per-position: nodes, time, depth reached, best move
- Run at 1T, 4T, 8T, and one anomalous run — compare per-position results
- Identify which positions change dramatically (fewer/more nodes) at anomalous thread counts

**Questions to answer:**
- Are all 50 positions affected equally, or do a few positions account for most of the anomaly?
- Do positions reach the same depth at all thread counts? (They should — bench is depth-limited)
- Does the best move change between thread counts? (Would indicate search instability affecting quality)
- Is the node count variance concentrated in specific position types (open/closed/endgame)?

### R4. Memory Bandwidth Saturation Point
**Goal:** Confirm or refute the "memory wall" hypothesis with direct measurement.
**Method:**
- VTune memory-access analysis at 1T, 2T, 4T, 8T
- Track **memory bandwidth utilization** (GB/s) and **LLC miss rate** at each thread count
- Compare to system's theoretical max memory bandwidth (~76 GB/s for DDR5 dual-channel)
- Run a synthetic memory bandwidth benchmark (e.g., STREAM) for comparison

**Questions to answer:**
- At what thread count does memory bandwidth actually saturate?
- Is the NPS plateau at 5-6T correlated with memory bandwidth saturation?
- How much of the memory traffic is TT vs slider tables vs other data?
- What is the actual LLC miss cost in cycles (VTune can show this)?

### R5. False Sharing & Contention Quantification
**Goal:** Measure the actual cost of shared mutable state, not just theorize.
**Method:**
- Build a test binary with TT statistics **compiled out** (`#ifdef` them to no-ops)
- Run bench scaling 1-12T and compare NPS to the current binary
- If NPS improvement is <2%, false sharing on stats is not the bottleneck

**Questions to answer:**
- How much NPS is lost to TT/PawnTT statistics overhead?
- Is `numberOfEntries` (used for `hashFull`) a significant contention point?
- Are there other hidden shared mutable state hotspots beyond the obvious counters?

### R6. TT Hit Rate & Entry Quality Under SMP
**Goal:** Understand if helper threads produce useful or harmful TT entries.
**Method:**
- Log TT hit rate, overwrite rate, and average entry depth at 1T vs 4T vs 8T
- Track how often a helper thread's TT entry is used by the main thread (vs main using its own entries)
- Compare depth distribution of TT entries at different thread counts

**Questions to answer:**
- Does TT hit rate improve with more threads? (It should, if SMP is working correctly)
- Are shallow helper entries evicting deep main-thread entries? (Would indicate replacement policy problem)
- What fraction of TT overwrites replace a deeper entry with a shallower one?

### R7. ELO Scaling at Longer Time Controls
**Goal:** Determine if the poor ELO scaling is a short-TC artifact.
**Method:**
- Run thread scaling ELO tests at multiple TCs:
  - 5+0.05 (bullet — current data)
  - 60+0.6 (rapid — more realistic)
  - 300+3 (classical — if feasible, at least 1T/4T/8T)
- Use self-play (FrankyCPP NT vs FrankyCPP 1T) with sufficient games (500+ per TC per thread count)

**Questions to answer:**
- Does ELO scaling improve at longer TC? By how much?
- Is there a TC threshold where SMP becomes clearly beneficial?
- Does the 8T regression (8T worse than 7T) persist at longer TC?

### R8. Stockfish SMP Implementation Study
**Goal:** Understand what SF does differently in its Lazy SMP that produces super-linear scaling.
**Method:**
- Read SF source: `thread.cpp`, `search.cpp` — focus on:
  - How helpers are started/stopped
  - Thread depth differentiation (skip depths based on Skipsize/SkipPhase tables)
  - TT replacement policy (generation-based, depth comparison)
  - How `Threads.stop` propagates
  - How best thread is selected
- Document key differences from FrankyCPP's approach

**Questions to answer:**
- Does SF vary search parameters per thread? Which ones?
- How does SF's TT replacement differ from ours? (Generation counter vs age increment?)
- Does SF do anything special for thread scheduling or core affinity?
- What is SF's TT entry size and cluster layout? How does it compare?

### R9. Search Tree Overlap Between Threads
**Goal:** Quantify how much redundant work helper threads do.
**Method:**
- Add optional instrumentation: track Zobrist keys visited by each thread
- At search end, compute overlap ratio: positions visited by >1 thread / total unique positions
- Compare at 2T, 4T, 8T

**Questions to answer:**
- What fraction of the search tree is explored by multiple threads? (High overlap = wasted work)
- Does overlap increase with more threads? (Expected, but rate matters)
- Is the overlap concentrated at shallow or deep nodes?

### Research Priority

| ID  | Investigation                              | Effort  | Blocking? | Priority        |
|-----|--------------------------------------------|---------|-----------|-----------------|
| R1  | Bench stability (3× repeat runs)           | Small   | Yes       | ⬆️ **Do first** |
| R2  | Code review: bench → search → threads      | Medium  | Yes       | ⬆️ **Do first** |
| R3  | Per-position analysis                      | Small   | Partially | ⬆️ **Do first** |
| R5  | False sharing quantification               | Small   | Yes       | ⬆️ **Do first** |
| R4  | Memory bandwidth saturation (VTune)        | Medium  | Yes       | ⬆️ High         |
| R6  | TT hit rate & entry quality                | Medium  | Partially | ⬆️ High         |
| R8  | Stockfish SMP source study                 | Medium  | No        | 🔶 Medium       |
| R7  | ELO at longer time controls                | Large   | No        | 🔶 Medium       |
| R9  | Search tree overlap measurement            | Large   | No        | 🔽 Low          |

**Gate:** Do not start Phase 1-5 implementation until R1, R2, R4, and R5 are complete. Their results
will determine which improvements actually matter and which are solving the wrong problem.

---

## 4. Improvement Plan (Gated on Research Phase)

> ⚠️ **Do not start implementation until R1, R2, R4, and R5 from Section 3 are complete.**
> Research results may invalidate or reprioritize these phases.

### Phase 1: Low-Hanging Fruit — Reduce Contention (Est. +5-15% NPS scaling)

#### 1a. Move TT/PawnTT Statistics to Per-Thread Counters
**Impact:** Eliminates false sharing on hot-path statistics
**Risk:** Low
**Effort:** Small

Move `numberOfPuts`, `numberOfProbes`, `numberOfHits`, `numberOfMisses`, `numberOfCollisions`, `numberOfOverwrites`, `numberOfUpdates` out of the shared TT object and into `SearchThreadData`. Aggregate only at search end for reporting.

```cpp
// SearchThreadData — add per-thread TT stats
struct TTStats {
    uint64_t puts = 0;
    uint64_t probes = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t collisions = 0;
    uint64_t overwrites = 0;
    uint64_t updates = 0;
};
TTStats ttStats{};
TTStats pawnTTStats{};
```

TT::probe() and TT::put() would no longer touch shared mutable counters.

#### 1b. Review TT Entry Layout for Cache Efficiency
**Impact:** Reduce cache-line bouncing
**Risk:** Low
**Effort:** Small

Current TT::Entry is 16 bytes (4 entries per 64-byte cache line). This is already optimal.
Verify that the `numberOfEntries` counter (incremented on every new entry) isn't causing false sharing with the cluster data pointer. Consider padding or separating counters from data pointers.

#### 1c. Ensure TT `numberOfEntries` Is Per-Thread or Atomic
**Impact:** Eliminates a data race and potential false sharing
**Risk:** Low
**Effort:** Small

`numberOfEntries` is incremented in `put()` from multiple threads. Either make it per-thread (aggregate at end) or `std::atomic` (if accuracy needed for `hashFull`).

---

### Phase 2: TT Replacement Policy Improvements (Est. +10-30 ELO)

#### 2a. Depth-Aware Replacement for SMP
**Impact:** Prevents shallow helper entries from evicting deep main-thread entries
**Risk:** Medium
**Effort:** Medium

Current replacement: `depth * 16 - age * 2 + hasMove`. This doesn't account for SMP context. A shallow helper thread entry shouldn't replace a deep entry from any thread.

Consider:
- Minimum depth threshold for replacement (e.g., never replace an entry with depth >= current search depth - 2)
- Stockfish-like approach: "don't replace unless depth >= entry.depth - 4"

#### 2b. Generation Counter Instead of Age Increment
**Impact:** Cleaner age tracking without per-probe writes
**Risk:** Low
**Effort:** Medium

Currently `ageEntries()` iterates the entire TT at search start (expensive). Instead, use a global generation counter (0-7, 3 bits). New entries are tagged with the current generation. Replacement priority: older generation = more replaceable. No per-probe age decrement needed.

This eliminates:
- The expensive `ageEntries()` scan
- The age-- in probe() (already disabled under SMP due to data race)
- One source of shared mutable state

---

### Phase 3: Shift Evaluation from Memory-Bound to Compute-Bound (Est. +20-50 ELO, significant NPS scaling improvement)

This is the highest-impact long-term change. The goal is to make each node do more *computation* (which scales with cores) and fewer *memory lookups* (which don't scale).

#### 3a. More Sophisticated Classical Evaluation Terms
**Impact:** Better evaluation quality AND shifts compute/memory ratio
**Risk:** Medium
**Effort:** Large

Current evaluation is relatively lightweight — many terms are simple popcount + multiply. Adding more compute-intensive evaluation terms shifts the balance:

- **Space evaluation**: Count controlled squares in each quadrant
- **Piece coordination**: More sophisticated piece interaction analysis (X-ray attacks, battery detection)
- **Pawn structure complexity**: Pawn chain analysis, lever detection, breakthrough potential
- **King danger zone**: More detailed king safety with virtual mobility computation
- **Endgame-specific patterns**: KBN vs K, fortress detection, wrong-colored bishop, etc.
- **Complexity factor**: Evaluate position sharpness/tactical potential

Each of these adds ALU work per node (bitboard operations that live in registers/L1) without adding random memory accesses.

#### 3b. Reduce Memory Footprint Per Node
**Impact:** Better cache utilization, less memory bandwidth pressure
**Risk:** Low-Medium
**Effort:** Medium

- Audit data structure sizes accessed per node
- Ensure hot data fits in L1/L2 (PlyInfo, Position, MoveGenerator internal state)
- Consider SoA (struct-of-arrays) layouts where cache-line utilization is poor
- Profile cache miss rates per structure (VTune memory-access analysis data available)

#### 3c. Evaluator Computation Caching
**Impact:** Reduce redundant evaluation work across the tree
**Risk:** Low
**Effort:** Medium

- **Eval cache in TT**: Already storing `eval` in TT entries (good). Ensure all paths use it.
- **Incremental attack maps**: Currently recomputed from scratch in every `evaluate()` call. Consider incremental update of attack maps (like PSTs are incremental).

---

### Phase 4: Thread Divergence & Quality (Est. +10-30 ELO)

#### 4a. Stronger Thread Differentiation
**Impact:** Reduce redundant work, improve TT entry diversity
**Risk:** Medium
**Effort:** Medium

Currently helpers run the same `iterativeDeepening()` starting from `SMP_HELPER_START_DEPTH`. Stockfish varies the starting depth per thread (e.g., thread i starts at depth + (i % 3) - 1) to ensure threads explore different depths and produce diverse TT entries.

Add per-thread depth offset or search parameter variation:
```cpp
// In helper thread setup
int depthOffset = (threadId % 3) - 1; // -1, 0, +1
// Or vary aspiration window, null-move depth, LMR parameters per thread
```

#### 4b. Consider Thread-Local TT Buckets (Advanced)
**Impact:** Reduce cross-thread cache invalidation
**Risk:** High
**Effort:** Large

Partition TT entries such that threads preferentially write to their own "region." Reads are global. This reduces cache-line bouncing on writes while preserving shared read benefit.

---

### Phase 5: Hybrid CPU Awareness (Low priority)

#### 5a. Thread Affinity / Core Pinning
**Impact:** Predictable performance on hybrid CPUs
**Risk:** Low
**Effort:** Small (platform-specific)

On Windows, use `SetThreadAffinityMask` to pin search threads to P-cores only. This ensures all threads run at maximum performance and prevents E-core TT pollution.

#### 5b. Auto-Detect Optimal Thread Count
**Impact:** Better out-of-box experience
**Risk:** Low
**Effort:** Small

Detect P-core count (Windows: `GetLogicalProcessorInformationEx`) and default THREADS to P-core count rather than total logical processors.

---

## 5. Implementation Priority

| Phase | Item                                     | Impact           | Effort | Priority            |
|-------|------------------------------------------|------------------|--------|---------------------|
| 1a    | Per-thread TT stats                      | Medium (NPS)     | Small  | ⬆️ **Do first**     |
| 1b    | TT layout audit                          | Low-Medium       | Small  | ⬆️ **Do first**     |
| 1c    | numberOfEntries thread safety            | Low              | Small  | ⬆️ **Do first**     |
| 2b    | Generation counter (replaces ageEntries) | Medium           | Medium | ⬆️ High             |
| 2a    | Depth-aware SMP replacement              | Medium (ELO)     | Medium | ⬆️ High             |
| 4a    | Thread depth differentiation             | Medium (ELO)     | Medium | ⬆️ High             |
| 3a    | Richer classical evaluation              | High (ELO + NPS) | Large  | 🔶 Medium (ongoing) |
| 3b    | Memory footprint reduction               | Medium (NPS)     | Medium | 🔶 Medium           |
| 3c    | Incremental attack maps                  | Medium (NPS)     | Medium | 🔶 Medium           |
| 5a    | P-core thread pinning                    | Low              | Small  | 🔽 Low              |
| 5b    | Auto-detect P-core count                 | Low              | Small  | 🔽 Low              |
| 4b    | Thread-local TT partitioning             | Unknown          | Large  | 🔽 Research         |

---

## 6. Measurement Plan

Each change should be validated with:

1. **NPS bench scaling**: `scripts/bench_thread_scaling.ps1` (1-12T)
2. **ELO testing**: Arena matches at 60+0.6 (minimum 500 games per config, concurrency 1)
3. **Regression check**: Ensure 1T strength doesn't decrease (run vs Stockfish baseline)
4. **VTune profiling**: Before/after memory-access and microarchitecture analysis

---

## 7. Reference Data

### VTune Profile (8T, 2026-04-09)

TT::probe is the #1 memory-bound hotspot:
- High LLC miss rate
- Memory-bound classification
- Dominates cache pressure at >4 threads

### Current TT Design (already well-designed)

- 4-way associative clusters, 64B aligned to cache line ✅
- Single `_mm_prefetch` loads entire cluster ✅  
- Atomic key with XOR verification ✅
- 16-byte entries (4 per cache line) ✅
- Copy-on-read pattern for thread safety ✅

The TT *structure* is solid. The issues are: (1) shared mutable statistics on the hot path, (2) replacement policy not SMP-aware, (3) the fundamental memory-bound nature of classical eval.

---

*End of plan document.*
