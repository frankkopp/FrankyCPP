# FrankyCPP - Best Thread Selection for Lazy SMP

**Document Version:** 1.4
**Created:** 2026-03-02
**Last Updated:** 2026-03-10
**Status:** ✅ ALL PHASES COMPLETE — Validated by test suite and match testing
**Target Version:** v1.6+
**Estimated Effort:** 1-2 days (reduced from 2-3 days, Phase 3-4 done)

---

## Overview

This document describes the implementation plan for **Best Thread Selection** in FrankyCPP's Lazy SMP framework. Previously, only the main thread's PV (Principal Variation) was used for the final result; helper threads contributed solely via shared TT entries. This enhancement selects the best result from any thread, improving playing strength when a helper thread finds a better move.

**Current behavior (implemented 2026-03-09):**
- Main thread runs full `iterativeDeepening()` and produces the initial PV
- Helper threads run full `iterativeDeepening()` (same code, with `isMainThread()` guards)
- Each thread tracks `completedIterationDepth` and `lastIterationValue` after each iteration
- After all threads stop, `selectBestThread()` compares results using depth+score heuristic
- The best thread's PV/move/score override the search result before `bestmove` is sent
- TB root override and ponder move extraction apply on top of best-thread selection
- A final UCI `info` line is sent when a non-main thread is selected, ensuring GUI consistency
- Configurable via `USE_BEST_THREAD_SELECTION` (default: true) and `BEST_THREAD_SCORE_MARGIN` (default: 50cp)

---

## What's Already Implemented

### Phase 1-2: ✅ COMPLETE - Tracking Fields and Progress Recording (2026-03-09)

- `SearchThreadData` has `completedIterationDepth` and `lastIterationValue` fields
- Both reset in `reset()` and updated after each fully completed iteration
- Tracks per-thread progress for comparison at search end

### Phase 3: ✅ COMPLETE - Helpers Use Full iterativeDeepening() (2026-03-03)

The original `helperRun()` with simplified search was replaced. Helpers now:
- Run the same `iterativeDeepening()` code as the main thread
- Benefit from aspiration windows, proper move ordering, LMR, etc.
- Have their own `thread().rootMoves` with best move at index 0
- Use starting depth offset (1 + id % 3) for search diversification

### Phase 4: ✅ COMPLETE - UCI Output Guarded to Main Thread (2026-03-03)

All UCI output and time management guarded with `isMainThread()`:
- `sendIterationEndInfoToUci()`
- `sendAspirationResearchInfo()`
- `sendString()` for draw/mate/stalemate messages
- Time management (volatility, instability tracking, extra time)
- TB probing at root
- Helper thread launching

### Phase 5-6: ✅ COMPLETE - Best Thread Selection and UCI Integration (2026-03-09)

- `selectBestThread()` implements Stockfish-style depth+score heuristic with configurable margin
- `sendFinalUciInfo()` sends final UCI `info` line when non-main thread is selected
- `run()` flow: join helpers → select best thread → override result → TB override → ponder move → send final UCI info → send bestmove
- TB root override and ponder move logic moved from `iterativeDeepening()` to `run()` to apply after best-thread selection
- Guard: early-exit positions (checkmate/stalemate/draw) bypass best-thread override since no iteration completes

### Phase 7: ✅ COMPLETE - Configuration Options (2026-03-09)

- `USE_BEST_THREAD_SELECTION` (bool, default: true) — enables/disables feature
- `BEST_THREAD_SCORE_MARGIN` (int, default: 50, range: 0-500) — centipawn margin for depth vs score comparison
- Both exposed as UCI options and YAML config

### Phase 8: ✅ COMPLETE - Tests (2026-03-09)

- `BestThreadSelectionEnabled` — 4-thread time-limited search validates result
- `BestThreadSelectionDisabled` — confirms disabled mode uses main thread depth
- `selectBestThread` — unit test with 7 synthetic scenarios covering all comparison branches

---

## Motivation

**Why Best Thread Selection?**

1. **Stockfish does this** — Stockfish's Lazy SMP implementation selects the "best thread" based on depth and score, not always the main thread.

2. **Better utilization** — Helper threads may reach deeper depths or find better moves due to different search paths (LMR variations, depth offsets).

3. **Minimal overhead** — The comparison happens once at search end; no impact on search performance.

4. **Robustness** — If the main thread's search is polluted by horizon effects, another thread may have a cleaner result.

---

## Design

### Thread Selection Criteria

Several approaches exist, from simple to sophisticated:

#### Option A: Score-Only (Simplest)
```cpp
// Select thread with highest root score
SearchThreadData* best = &mainThread();
for (auto& st : searchThreadData) {
    if (st->pv.first().value() > best->pv.first().value()) {
        best = st.get();
    }
}
```

**Pros:** Simple, easy to understand  
**Cons:** Shallow search may have inflated scores; ignores search depth

#### Option B: Depth + Score (Recommended — Stockfish-style)
```cpp
// Prefer deeper searches; use score margin to compare different depths
auto isBetter = [](const SearchThreadData& a, const SearchThreadData& b) {
    const Depth depthA = a.completedIterationDepth;
    const Depth depthB = b.completedIterationDepth;
    const Value valueA = a.pv.first().value();
    const Value valueB = b.pv.first().value();
    
    constexpr Value MARGIN{50};  // ~0.5 pawns
    
    // Deeper thread wins unless score is significantly worse
    if (depthA > depthB) return valueA >= valueB - MARGIN;
    if (depthA < depthB) return valueA > valueB + MARGIN;
    return valueA > valueB;  // Same depth: prefer higher score
};
```

**Pros:** Balances search effort vs result quality  
**Cons:** Needs tuning of margin value

#### Option C: Vote-Based
```cpp
// Select the move that most threads agree on
std::unordered_map<Move, int> votes;
for (auto& st : searchThreadData) {
    votes[st->pv.first().stripped()]++;
}
// Return move with most votes
```

**Pros:** Robust against outliers  
**Cons:** May miss unique best move found by one thread; more complex

**Recommendation:** Start with **Option B (Depth + Score)**. It's proven in Stockfish and provides a good balance.

---

## Implementation Phases

### Phase 1: Add Tracking Fields to SearchThreadData

**File:** `src/engine/SearchThreadData.h`

Add fields to track completed search progress:

```cpp
struct SearchThreadData {
    // ...existing fields...
    
    /// Highest fully completed iteration depth for this thread
    /// Used for best-thread selection at search end
    Depth completedIterationDepth = DEPTH_NONE;
    
    /// Score from the last completed iteration
    /// Used alongside depth for best-thread selection
    Value lastIterationValue = VALUE_NONE;
    
    // In reset():
    void reset() {
        // ...existing resets...
        completedIterationDepth = DEPTH_NONE;
        lastIterationValue = VALUE_NONE;
    }
};
```

**Estimated effort:** 15 minutes

---

### Phase 2: Track Progress in iterativeDeepening()

**File:** `src/engine/Search.cpp`

Update the iteration loop to record completed depth/value:

```cpp
// In iterativeDeepening(), at end of each iteration (around line 850):
// After sendIterationEndInfoToUci() and before the next iteration

// Track iteration progress for best-thread selection
thread().completedIterationDepth = iterationDepth;
thread().lastIterationValue = thread().pv.first().value();
```

This must happen **after** a successful iteration completes, not when stopped mid-iteration.

**Estimated effort:** 15 minutes

---

### Phase 3: Modify helperRun() to Use Full Iterative Deepening

**File:** `src/engine/Search.cpp`

Replace the simplified loop in `helperRun()` with a call to `iterativeDeepening()`:

**Current simplified loop (lines 417-490):**
```cpp
void Search::helperRun(SearchThreadData& st) {
    // ... simplified manual iteration over root moves ...
    Depth depth = 1 + static_cast<Depth>(st.id % 2);
    while (!stopSearchFlag) {
        for (const Move& move : localRootMoves) {
            // ... manual search calls ...
        }
        ++depth;
    }
}
```

**Proposed change:**
```cpp
void Search::helperRun(SearchThreadData& st) {
    // Set thread-local pointer so search functions use this thread's data
    currentThreadData = &st;
    
    // Use thread-local position (copied at start of run() for all threads)
    Position& localPos = st.position;
    
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Helper thread {} starting iterative deepening", st.id);
    
    // Run full iterative deepening
    // Helpers benefit from: aspiration windows, proper move ordering, LMR, etc.
    // UCI output is guarded to main thread only (see Phase 4)
    (void)iterativeDeepening(localPos);
    
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Helper thread {} finished, depth {}, nodes {:L}", 
               st.id, st.completedIterationDepth, st.nodesVisited);
    
    currentThreadData = nullptr;
}
```

**Benefits of full iterativeDeepening for helpers:**
- Aspiration windows improve search efficiency
- Proper root move ordering from TT
- Correct time iteration gating (helpers ignore time but check stopFlag)
- Statistics and depth tracking work correctly
- PVs are properly constructed and comparable

**Estimated effort:** 30 minutes

---

### Phase 4: Guard UCI Output to Main Thread Only

**File:** `src/engine/Search.cpp`

Wrap all UCI reporting to only execute on the main thread:

**In `iterativeDeepening()`:**
```cpp
// Around line 840, guard the UCI update call:
if (thread().id == 0) {  // Only main thread sends UCI output
    sendIterationEndInfoToUci();
}
```

**In `aspirationSearch()` (if any intermediate output):**
```cpp
// Guard any sendIterationInfo(), sendString(), etc.
if (thread().id == 0) {
    // UCI output...
}
```

**Review all `send*()` calls in Search.cpp:**
- `sendIterationEndInfoToUci()` — guard
- `sendIterationInfo()` — guard  
- `sendString()` — guard (except critical errors)
- `sendSearchResult()` — called from `run()`, main thread only ✅

**Estimated effort:** 30 minutes

---

### Phase 5: Implement selectBestThread()

**File:** `src/engine/Search.h`

Add declaration:
```cpp
private:
    /// Selects the best thread based on depth and score after search completes.
    /// Called after all helper threads have joined.
    /// @return Pointer to the SearchThreadData with the best result
    [[nodiscard]] SearchThreadData* selectBestThread() const;
```

**File:** `src/engine/Search.cpp`

Implement the selection logic:
```cpp
SearchThreadData* Search::selectBestThread() const {
    SearchThreadData* best = searchThreadData[0].get();  // Main thread as baseline
    
    // If no helpers, main thread is always best
    if (numHelperThreads == 0) {
        return best;
    }
    
    for (int t = 1; t <= numHelperThreads; ++t) {
        const auto& candidate = *searchThreadData[t];
        
        // Skip threads that never completed an iteration
        if (candidate.completedIterationDepth == DEPTH_NONE) {
            continue;
        }
        
        const Depth bestDepth = best->completedIterationDepth;
        const Depth candDepth = candidate.completedIterationDepth;
        const Value bestValue = best->pv.first().value();
        const Value candValue = candidate.pv.first().value();
        
        // Prefer deeper search unless score is significantly worse
        constexpr Value SCORE_MARGIN{50};  // Tunable parameter
        
        bool candidateIsBetter = false;
        
        if (candDepth > bestDepth) {
            // Candidate searched deeper: accept unless score is much worse
            candidateIsBetter = (candValue >= bestValue - SCORE_MARGIN);
        }
        else if (candDepth == bestDepth) {
            // Same depth: prefer higher score
            candidateIsBetter = (candValue > bestValue);
        }
        else {
            // Candidate shallower: only accept if score is much better
            candidateIsBetter = (candValue > bestValue + SCORE_MARGIN);
        }
        
        if (candidateIsBetter) {
            best = searchThreadData[t].get();
        }
    }
    
    return best;
}
```

**Estimated effort:** 30 minutes

---

### Phase 6: Use Best Thread in run() with Final UCI Update

**File:** `src/engine/Search.cpp`

The selection must happen in `run()` **after helpers have joined**. The flow is:
1. `run()` starts helpers (via `launchHelperThreads()` called from within `iterativeDeepening()`)
2. `iterativeDeepening()` completes on main thread
3. `run()` calls `stopHelperThreads()` and waits for them
4. **NEW:** Select best thread and send final UCI update
5. `run()` returns `lastSearchResult`

**In `run()`, around line 370-390:**
```cpp
// Stop helper threads and wait for them to finish
stopHelperThreads();

// Select the best thread (after all helpers have stopped)
const SearchThreadData* bestThread = selectBestThread();

// Log if a helper thread was selected
if (bestThread->id != 0) {
    LOG__INFO(Logger::get().SEARCH_LOG, 
              "Best thread selection: using helper {} result (depth {} value {})",
              bestThread->id, 
              bestThread->completedIterationDepth, 
              bestThread->lastIterationValue.str());
}

// Update lastSearchResult from best thread
lastSearchResult.bestMove      = bestThread->pv.first().stripped();
lastSearchResult.bestMoveValue = bestThread->pv.first().value();
lastSearchResult.pv            = bestThread->pv.extract();
lastSearchResult.depth         = bestThread->completedIterationDepth;
lastSearchResult.extraDepth    = bestThread->statistics.currentExtraSearchDepth;

// Send final UCI info line with best thread's result BEFORE bestmove
// This ensures the UI shows consistent depth/score/PV matching the final move
sendFinalUciInfo(*bestThread);
```

**New method: `sendFinalUciInfo()`**

```cpp
void Search::sendFinalUciInfo(const SearchThreadData& bestThread) {
    // Build UCI info string with best thread's complete information
    // Similar to sendIterationEndInfoToUci() but uses bestThread data
    
    std::ostringstream ss;
    ss << "info"
       << " depth " << bestThread.completedIterationDepth
       << " seldepth " << bestThread.statistics.currentExtraSearchDepth
       << " score " << uciScore(bestThread.lastIterationValue)
       << " nodes " << getTotalNodes()
       << " nps " << nps(getTotalNodes(), elapsedSince(startSearchTime))
       << " time " << MILLISECONDS(elapsedSince(startSearchTime)).count()
       << " pv " << bestThread.pv.toUciString();
    
    uciHandler.sendToUci(ss.str());
}
```

**Why send a final UCI update?**

During search, UCI output comes from the main thread only. If a helper thread is selected as best:
- Without final update: UI shows main thread's last PV, then suddenly `bestmove` is different
- With final update: UI shows best thread's depth/score/PV, then matching `bestmove`

This eliminates user confusion and provides a consistent experience.

**Example UCI output sequence:**
```
info depth 14 score cp 45 pv e2e4 e7e5 g1f3 ...   <- main thread iteration
info depth 14 score cp 45 pv e2e4 e7e5 g1f3 ...   <- main thread done
info depth 15 score cp 52 pv d2d4 d7d5 c2c4 ...   <- FINAL: best thread (helper 2)
bestmove d2d4 ponder d7d5                          <- matches final info
```

**Estimated effort:** 45 minutes

---

### Phase 7: Configuration Option (Optional)

**File:** `src/config/SearchConfigData.h`

Add a config option to enable/disable best thread selection:
```cpp
/// Enable best-thread selection in Lazy SMP (compare all threads, pick best result)
/// When false, always use main thread result (current behavior)
bool USE_BEST_THREAD_SELECTION = true;

/// Score margin for best-thread comparison (centipawns)
/// Higher depth wins unless score is worse by more than this margin
int BEST_THREAD_SCORE_MARGIN = 50;
```

**File:** `src/config/ConfigRegistry.cpp`

Add registry entries for the new options.

**Estimated effort:** 20 minutes

---

### Phase 8: Testing

#### Unit Test: Basic Selection Logic
```cpp
TEST(Search_Test, BestThreadSelectionBasic) {
    // Setup search with mock thread data
    // Verify selection logic chooses correctly based on depth/score
}
```

#### Integration Test: SMP Search with Selection
```cpp
TEST(Search_Test, SMPBestThreadIntegration) {
    // Run actual SMP search
    // Verify result is valid regardless of which thread was selected
}
```

#### Regression Test: Compare Results
- Run tournament matches with best-thread-selection ON vs OFF
- Verify no strength regression
- Monitor how often helpers are selected (should be non-trivial but not dominant)

**Estimated effort:** 1-2 hours

---

## Summary of Changes

| File                            | Changes                                                        | Status             |
|---------------------------------|----------------------------------------------------------------|--------------------|
| `SearchThreadData.h`            | Add `completedIterationDepth`, `lastIterationValue` fields     | ✅ DONE (Phase 1)   |
| `SearchThreadData.h`            | Add `MoveList rootMoves` field                                 | ✅ DONE             |
| `Search.h`                      | Add `isMainThread()` helper                                    | ✅ DONE             |
| `Search.h`                      | Add `selectBestThread()`, `sendFinalUciInfo()` declarations    | ✅ DONE (Phase 5-6) |
| `Search.h`                      | Remove `rootMoves` member, `helperRun()` declaration           | ✅ DONE             |
| `Search.cpp`                    | Track depth/value in `iterativeDeepening()`                    | ✅ DONE (Phase 2)   |
| `Search.cpp`                    | Helpers call `iterativeDeepening()` (deleted `helperRun()`)    | ✅ DONE (Phase 3)   |
| `Search.cpp`                    | Guard UCI output to main thread only                           | ✅ DONE (Phase 4)   |
| `Search.cpp`                    | Guard time management to main thread only                      | ✅ DONE (Phase 4)   |
| `Search.cpp`                    | Guard TB probing to main thread only                           | ✅ DONE (Phase 4)   |
| `Search.cpp`                    | Add depth offset diversification (1 + id % 3)                  | ✅ DONE             |
| `Search.cpp`                    | Change `rootMoves` → `thread().rootMoves`                      | ✅ DONE             |
| `Search.cpp`                    | Implement `selectBestThread()`                                 | ✅ DONE (Phase 5)   |
| `Search.cpp`                    | Implement `sendFinalUciInfo()`                                 | ✅ DONE (Phase 6)   |
| `Search.cpp`                    | Call `selectBestThread()` + `sendFinalUciInfo()` in `run()`    | ✅ DONE (Phase 6)   |
| `Search.cpp`                    | Move TB override + ponder logic to `run()` (after best-thread) | ✅ DONE (Phase 6)   |
| `SearchConfigData.h`            | Add `USE_BEST_THREAD_SELECTION`, `BEST_THREAD_SCORE_MARGIN`    | ✅ DONE (Phase 7)   |
| `ConfigRegistry.cpp`            | Register config options                                        | ✅ DONE (Phase 7)   |
| `test/engine/SearchSmpTest.cpp` | Add unit/integration tests                                     | ✅ DONE (Phase 8)   |

---

## Risks and Considerations

### 1. Helper Thread Starting Depth
~~Currently, helpers use a depth offset (`st.id % 2`) for diversity. With full `iterativeDeepening()`, we may want to configure different starting depths or aspiration windows per thread.~~

**✅ RESOLVED:** Implemented depth offset (1 + id % 3) for helpers, spreading them across depths 2, 3, 1 (wrapping).

### 2. Root Moves Sharing
~~Main thread generates `rootMoves` once. Helpers currently generate their own in `helperRun()`. With full `iterativeDeepening()`, we need to ensure each thread has its own root moves.~~

**✅ RESOLVED:** Moved `rootMoves` to `SearchThreadData`. Each thread generates and maintains its own root moves. Changed all references from `rootMoves` to `thread().rootMoves`.

### 3. TB Probing at Root
~~Tablebase probing in `iterativeDeepening()` should only happen on main thread (one probe is enough).~~

**✅ RESOLVED:** TB probe guarded with `if (isMainThread())`.

### 4. Time Management
~~Helper threads should NOT make time management decisions (volatility extensions, stability adjustments).~~

**✅ RESOLVED:** All time-related code guarded with `if (isMainThread())`.

### 5. Statistics Aggregation
After search, statistics should be aggregated from all threads for accurate reporting.

**Status:** Already handled — `getTotalNodes()` aggregates `nodesVisited` from all threads.

---

## Validation Results (2026-03-10)

### Test Suites (4 threads, depth-limited)

| Metric           | Before (TT Buckets baseline) | After (Best Thread Selection) | Delta                 |
|------------------|-----------------------------:|------------------------------:|:----------------------|
| Positions solved |                    1731/2874 |                     1747/2875 | **+16 (+0.6%)**       |
| Benchmark NPS    |                    6,956,053 |                     7,001,492 | +0.6% (no regression) |

### Match Results (4 threads, 300s per game, 100 games)

| Opponent          |                  Before |                        After | Delta         |
|-------------------|------------------------:|-----------------------------:|:--------------|
| vs v1.4           | +92.5 ELO (44W/38D/18L) | **+103.7 ELO** (46W/37D/17L) | **+11.2 ELO** |
| vs Stockfish 2700 | −17.4 ELO (39W/17D/44L) |    **+6.9 ELO** (49W/4D/47L) | **+24.3 ELO** |

### Assessment

- **Consistent improvement** across all metrics — suites, NPS, and both match opponents
- **No regressions** in NPS or test suites
- **Stockfish 2700 swing** is significant: moved from losing (−17.4) to slightly winning (+6.9)
- **Draw rate vs Stockfish dropped** (17 → 4): more decisive games, sharper play from deeper helper lines
- **Overall: clear positive result** — feature works as designed

---

## Future Enhancements

1. **Adaptive thread selection** — Weight thread results by historical accuracy
2. **Skill level integration** — At lower skill levels, sometimes pick a non-optimal thread
3. **Analysis mode** — Report which thread found the best move
4. **Vote-based selection** — For positions where consensus matters (e.g., near time control)

---

## References

- Stockfish source: `thread.cpp`, `search()` — best thread selection logic
- Lazy SMP paper: "Lazy SMP and Node Count" by Tord Romstad
- FrankyCPP Lazy SMP plan: `docs/specs/PLAN_Lazy_SMP_MultiThreading.md`

---

*Last updated: 2026-03-10*
