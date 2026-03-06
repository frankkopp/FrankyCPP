# FrankyCPP v1.x Engine Strength Roadmap

**Document Version:** 2.3  
**Created:** 2026-02-01  
**Last Updated:** 2026-03-06  
**Status:** Active Development  
**Target:** FrankyCPP v1.5 → v2.0  
**Focus:** Maximum Playing Strength Through Systematic Enhancement

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Current State Analysis](#current-state-analysis)
3. [Enhancement Categories](#enhancement-categories)
4. [Prioritized Roadmap](#prioritized-roadmap)
5. [Detailed Enhancement Specifications](#detailed-enhancement-specifications)
6. [Implementation Tracking](#implementation-tracking)
7. [Testing & Validation Strategy](#testing--validation-strategy)
8. [Risk Assessment](#risk-assessment)
9. [References & Resources](#references--resources)

---

## Executive Summary

This document provides a comprehensive, prioritized roadmap for enhancing FrankyCPP's playing strength from v1.5 to v2.0. Based on extensive analysis of modern chess engine techniques and empirical data from top engines (Stockfish, Ethereal, Koivisto), this plan identifies **high-impact, feasible improvements** organized into logical implementation phases.

### Current State (v1.5.0 - March 2026)

**Recent Improvements (v1.5):**
- ✅ **TT Bucket Design** - 4-way associative with 64-byte cache-line alignment
- ✅ **XOR Key Verification** - Torn-read detection for SMP race safety
- ✅ **Strengthened SMP** - Eliminated remaining race conditions
- ✅ Cache-line alignment eliminates false sharing under SMP
- ✅ Depth-preferred + age tiebreak replacement policy
- ✅ Single prefetch loads entire bucket (4 entries)

**Previous Improvements (v1.4):**
- ✅ **+119 ELO** vs v1.3 baseline (+228 ELO total vs v1.1)
- ✅ **Lazy SMP Multi-Threading** implemented (SearchThreadData, helper threads)
- ✅ Thread-safe TT with atomic key operations
- ✅ Per-thread search state (PVTable, PlyStack, History, Statistics)
- ✅ UCI `Threads` option (auto-detect or manual 1-N)
- ✅ Comprehensive SMP test suite (SearchSmpTest.cpp)
- ✅ PawnTT shared across threads with lock-free concurrent access

**Previous Improvements (v1.3):**
- ✅ **+109 ELO** vs v1.1 baseline
- ✅ Logarithmic LMR formula (configurable divisor)
- ✅ Fixed isPvNode propagation bugs (PV ratio: 20% → 0.02%)
- ✅ Fixed history heuristic (quiet moves only, skip alpha-raising)
- ✅ PV/NonPV node tracking statistics
- ✅ Late Move Pruning with depth-dependent limits

**Strengths:**
- ✅ Production-ready classical chess engine with UCI protocol
- ✅ Alpha-beta search with modern pruning (NMP, LMR, futility, razoring)
- ✅ **Multi-threaded Lazy SMP search** with TT buckets (v1.4-v1.5)
- ✅ Classical evaluation (material, PST, pawn structure, mobility, king safety)
- ✅ Comprehensive testing infrastructure (266+ tests, CI/CD)
- ✅ Cross-platform support (Windows MSVC, Linux GCC/Clang)
- ✅ 100+ configurable parameters via YAML
- ✅ Syzygy tablebase support (v1.2)
- ✅ Engine Arena testing framework (v1.1)
- ✅ Counter-Move History heuristic (v1.2)
- ✅ Singular and Check Extensions (v1.2)
- ✅ Estimated strength: ~2650+ ELO (strong master level)

**Remaining Limitations:**
- ❌ Classical evaluation only (NNUE offers +200-400 ELO improvement)
- ❌ Missing continuation history (2-ply, 4-ply)
- ❌ Missing capture history heuristic
- ❌ No automated parameter tuning (SPSA/Texel)

### Target State (v2.0 - End Goal)

**Target Playing Strength:** ~2900-3100 ELO (approaching IM/GM level)  
**Expected Remaining Gain:** +300-500 ELO (from v1.5)

**Key Enhancements:**
1. ✅ Multi-threaded parallel search (Lazy SMP) - **+119 ELO** ✅ COMPLETE (v1.4)
2. 📋 Neural network evaluation (NNUE) - **+200-400 ELO** - PLANNED
3. ✅ Endgame tablebase support (Syzygy) - **+35-60 ELO** ✅ COMPLETE (v1.2)
4. ✅ Advanced search techniques - **+50-100 ELO** ✅ MOSTLY COMPLETE (v1.2-v1.4)
5. 🔄 Enhanced move ordering - **+25-45 ELO** - Continuation/Capture History remaining
6. 📋 Automated parameter tuning - **+30-60 ELO** - PLANNED

### Development Philosophy

**Incremental + Measurable:** Each enhancement is:
- Implemented in isolation (minimize risk)
- Tested against previous version (quantify ELO gain)
- Documented and merged before starting next enhancement
- Reversible (can rollback if regression detected)

**Evidence-Based:** Prioritize based on:
- Empirical data from other engines (Stockfish, Ethereal)
- Effort vs. impact ratio (ROI)
- Implementation risk and complexity
- Architectural fit with FrankyCPP

---

## Current State Analysis

### Strengths to Preserve

| Component           | Current State   | Quality | Notes                                           |
|---------------------|-----------------|---------|-------------------------------------------------|
| **Build System**    | CMake + vcpkg   | ⭐⭐⭐⭐⭐   | Excellent cross-platform support                |
| **Testing**         | 266+ unit tests | ⭐⭐⭐⭐⭐   | Comprehensive coverage, CI/CD integrated        |
| **Configuration**   | YAML-based      | ⭐⭐⭐⭐⭐   | Flexible, runtime-configurable                  |
| **Code Quality**    | Modern C++20    | ⭐⭐⭐⭐⭐   | Clean architecture, good separation of concerns |
| **Move Generation** | Bitboard-based  | ⭐⭐⭐⭐⭐   | Fast, PEXT-optimized, well-tested               |
| **UCI Protocol**    | Full compliance | ⭐⭐⭐⭐⭐   | Compatible with all GUIs/tournament managers    |
| **Logging**         | spdlog-based    | ⭐⭐⭐⭐⭐   | Excellent debugging infrastructure              |

### Areas for Improvement

#### 1. Search Algorithm (Low Priority - Mostly Complete)

| Feature                 | Current State        | Industry Standard            | Gap Analysis   |
|-------------------------|----------------------|------------------------------|----------------|
| **Parallel Search**     | ✅ Lazy SMP (v1.4)    | ✅ Lazy SMP (8-16 threads)    | **✅ CLOSED**   |
| **Singular Extensions** | ✅ Implemented (v1.2) | ✅ Standard in top engines    | **✅ +27 ELO**  |
| **Check Extensions**    | ✅ Implemented (v1.2) | ✅ Standard in top engines    | **✅ +30 ELO**  |
| **Multi-Cut Pruning**   | ❌ Not implemented    | ✅ Common in modern engines   | **-10-20 ELO** |
| **Probcut**             | ❌ Not implemented    | ✅ Used in Stockfish/Ethereal | **-10-15 ELO** |
| **LMR Formula**         | ✅ Logarithmic (v1.3) | ✅ Tuned formula              | **✅ CLOSED**   |
| **Late Move Pruning**   | ✅ Implemented (v1.3) | ✅ Standard                   | **✅ CLOSED**   |

**Estimated Remaining Search Gap:** -20-35 ELO (Multi-Cut + Probcut only)

#### 2. Evaluation Function (Critical Priority)

| Feature            | Current State          | Industry Standard           | Gap Analysis     |
|--------------------|------------------------|-----------------------------|------------------|
| **NNUE**           | ❌ Not implemented      | ✅ Standard in 2600+ engines | **-200-400 ELO** |
| **Eval Tuning**    | ⚠️ Manual tuning       | ✅ Texel/SPSA automated      | **-30-60 ELO**   |
| **Pawn Structure** | ✅ Basic implementation | ✅ Similar                   | ~0 ELO           |
| **King Safety**    | ✅ Basic implementation | ✅ Similar                   | ~0 ELO           |
| **Mobility**       | ✅ Implemented          | ✅ Similar                   | ~0 ELO           |

**Estimated Total Eval Gap:** -230-460 ELO

#### 3. Move Ordering (Medium Priority - Partially Complete)

| Feature                   | Current State          | Industry Standard | Gap Analysis   |
|---------------------------|------------------------|-------------------|----------------|
| **History Heuristic**     | ✅ Fixed (v1.3)         | ✅ Standard        | **✅ CLOSED**   |
| **Killer Moves**          | ✅ Optimized (2 slots)  | ✅ Standard        | **✅ CLOSED**   |
| **Counter-Move History**  | ✅ Implemented (v1.2)   | ✅ Standard        | **✅ CLOSED**   |
| **Continuation History**  | ❌ Not implemented      | ✅ Standard        | **-15-25 ELO** |
| **Capture History**       | ❌ Not implemented      | ✅ Common          | **-10-20 ELO** |
| **SEE (Static Exchange)** | ✅ Basic implementation | ✅ Similar         | ~0 ELO         |

**Estimated Remaining Move Ordering Gap:** -25-45 ELO (Continuation + Capture History)

#### 4. Endgame Play (Low Priority - Mostly Complete)

| Feature               | Current State             | Industry Standard      | Gap Analysis   |
|-----------------------|---------------------------|------------------------|----------------|
| **Syzygy Tablebases** | ✅ Implemented (v1.2)      | ✅ Standard (6-7 piece) | **✅ CLOSED**   |
| **Endgame Patterns**  | ⚠️ Basic (draw detection) | ✅ Comprehensive        | **-10-20 ELO** |
| **Scaling Functions** | ✅ Basic implementation    | ✅ Similar              | ~0 ELO         |

**Estimated Remaining Endgame Gap:** -10-20 ELO (endgame patterns only)

### Total Estimated Strength Gap

| Category            | Original Gap (v1.1)  | Remaining Gap (v1.5) | Status          |
|---------------------|----------------------|----------------------|-----------------|
| Search Algorithm    | -125 to -230         | **-20 to -35**       | ✅ Mostly Closed |
| Evaluation Function | -230 to -460         | -230 to -460         | ❌ NNUE Needed   |
| Move Ordering       | -35 to -65           | **-25 to -45**       | 🔄 Partial      |
| Endgame Play        | -45 to -80           | **-10 to -20**       | ✅ Mostly Closed |
| **TOTAL**           | **-435 to -835 ELO** | **-285 to -560 ELO** | **~40% Closed** |

**Progress Summary:**
- **Closed:** +228 ELO verified (v1.1 → v1.4)
- **Remaining:** -285 to -560 ELO (primarily NNUE)
- **NNUE alone:** +200-400 ELO potential (largest remaining opportunity)

---

## Enhancement Categories

### Category A: Foundation (Prerequisites for other enhancements) ✅ COMPLETE
- ✅ Thread-safe transposition table (v1.4)
- ✅ Refactored search architecture (SearchThreadData per-thread state)
- ✅ Automated testing infrastructure for strength validation (v1.1)

### Category B: Quick Wins (High ROI, Low Risk) ✅ COMPLETE
- ✅ Singular extensions (v1.2)
- ✅ Check extensions (v1.2)
- ✅ Counter-move history (v1.2)
- ✅ Best-move instability time management (v1.2)

### Category C: Parallelization (Hardware Utilization) ✅ COMPLETE
- ✅ Lazy SMP multi-threaded search (v1.4)
- ✅ Thread pool integration (v1.4)
- ✅ SMP-specific optimizations (v1.4)

### Category D: Evaluation Revolution (Largest Single Gain) 📋 PLANNED
- 📋 NNUE implementation (incremental updates, inference)
- 📋 Training pipeline
- 📋 Classical/NNUE hybrid mode

### Category E: Advanced Search (Incremental Gains) 🔄 PARTIAL
- ✅ Improved LMR formula (v1.3)
- ✅ Late move pruning (v1.3)
- 📋 Multi-cut pruning
- 📋 Probcut
- 📋 SEE-based pruning enhancements

### Category F: Move Ordering Refinements 🔄 PARTIAL
- ✅ Counter-move history (v1.2)
- ✅ History heuristic fixes (v1.3)
- 📋 Continuation history (2-ply, 4-ply)
- 📋 Capture history heuristic
- 📋 Static exchange evaluation improvements

### Category G: Endgame Specialization ✅ COMPLETE
- ✅ Syzygy tablebase integration via Fathom library (v1.2)
- ✅ Root and search probing (v1.2)
- ⚠️ Endgame pattern recognition (basic)

### Category H: Infrastructure & Tuning 📋 PLANNED
- 📋 SPSA parameter optimization
- 📋 Texel tuning for evaluation weights
- ✅ Automated match running and ELO tracking (Arena v1.1)
- Root and search probing
- Endgame pattern recognition

### Category H: Infrastructure & Tuning
- SPSA parameter optimization
- Texel tuning for evaluation weights
- Automated match running and ELO tracking

---

## Prioritized Roadmap

### Phase 1: Strength Testing Infrastructure (v1.1) - **1-2 weeks** ✅ COMPLETE
**Goal:** Establish automated strength testing framework for validating improvements  
**Target:** Foundation for measuring ELO gains

| Task                       | Effort      | Complexity | ELO Gain | Status     |
|----------------------------|-------------|------------|----------|------------|
| Arena Integration          | 🟢 2-3 days | 🟡 Medium  | N/A      | ✅ Complete |
| Automated Match Runner     | 🟢 2-3 days | 🟡 Medium  | N/A      | ✅ Complete |
| ELO Tracking & Statistics  | 🟢 1-2 days | 🟢 Low     | N/A      | ✅ Complete |
| Opponent Engine Collection | 🟢 1-2 days | 🟢 Low     | N/A      | ✅ Complete |

**Why Testing First?**
1. **Measure twice, cut once:** Cannot validate ELO gains without proper testing
2. **Scientific approach:** Every enhancement needs quantifiable proof
3. **Regression detection:** Catch strength regressions early
4. **Baseline establishment:** Know starting point for all improvements

**Infrastructure Components:**
- Arena GUI for interactive testing and tournament management
- cutechess-cli for automated batch matches
- Collection of calibrated opponent engines (various ELO levels)
- Scripts for result parsing and ELO calculation

**Completed in v1.1:** Testing infrastructure established, ready for measuring improvements.

---

### Phase 2: Performance Fundamentals & Quick Wins (v1.2) - **2-3 weeks** ✅ COMPLETE
**Goal:** Eliminate heap allocations, improve cache performance, add proven search enhancements  
**Target:** +70-135 ELO | **Actual:** +57 ELO

| Task                            | Effort      | Complexity | ELO Gain | Status      |
|---------------------------------|-------------|------------|----------|-------------|
| **Performance Fundamentals**    |             |            |          |             |
| Triangular PV Table             | 🟢 1-2 days | 🟡 Medium  | +5-10    | ✅ Complete  |
| MoveList Static Array Refactor  | 🟡 3-5 days | 🟡 Medium  | +5-15    | ✅ Complete  |
| **Search Quick Wins**           |             |            |          |             |
| Singular Extensions             | 🟢 2-3 days | 🟡 Medium  | +20-30   | ✅ Complete  |
| Check Extensions                | 🟢 2-3 days | 🟡 Medium  | +10-20   | ✅ Complete  |
| Counter-Move History            | 🟡 3-5 days | 🟡 Medium  | +10-20   | ✅ Complete  |
| Best-Move Instability Time Mgmt | 🟢 2-3 days | 🟡 Medium  | +5-15    | ✅ Complete  |
| Selective Checks in Quiescence  | 🟡 3-5 days | 🟡 Medium  | +15-25   | 📋 Deferred |

**Results:**
- Singular + Check Extensions: +57 ELO (v1.2 vs v1.1)
- Zero heap allocations during search achieved
- Foundation laid for Lazy SMP

---

### Phase 3: Multi-Threading (v1.4-v1.5) - **3-4 weeks** ✅ COMPLETE
**Goal:** Utilize modern multi-core CPUs  
**Target:** +60-120 ELO | **Actual:** +119 ELO (v1.4 vs v1.3)

| Task                    | Effort       | Complexity | ELO Gain | Status     |
|-------------------------|--------------|------------|----------|------------|
| Thread-Safe TT          | 🟡 1 week    | 🔴 High    | N/A      | ✅ Complete |
| Per-Thread Search State | 🟡 1 week    | 🔴 High    | N/A      | ✅ Complete |
| Lazy SMP Implementation | 🟡 1-2 weeks | 🔴 High    | +50-100  | ✅ Complete |
| Thread Pool Integration | 🟢 2-3 days  | 🟡 Medium  | N/A      | ✅ Complete |
| SMP Testing & Tuning    | 🟡 3-5 days  | 🟡 Medium  | +10-20   | ✅ Complete |
| TT Buckets (v1.5)       | 🟡 3-5 days  | 🟡 Medium  | +5-10    | ✅ Complete |
| XOR Key Verification    | 🟢 1-2 days  | 🟡 Medium  | N/A      | ✅ Complete |

**Implementation Summary (v1.4):**
- `SearchThreadData` struct: Per-thread PVTable, PlyStack, History, Statistics
- Thread-safe TT with atomic key operations (acquire/release memory order)
- Helper threads run independent `iterativeDeepening()` with shared TT
- Comprehensive test suite: `SearchSmpTest.cpp`
- UCI `Threads` option: auto-detect or manual 1-N

**v1.5 Enhancements:**
- **TT Bucket Design:** 4-way associative with 64-byte cache-line alignment
- **XOR Key Verification:** Torn-read detection for race safety
- **Cache-line alignment:** Eliminates false sharing under SMP
- **Race condition fixes:** All remaining SMP races eliminated

**Verification:**
- ✅ ThreadSanitizer: No data races
- ✅ Determinism: Threads=1 produces identical results
- ✅ Scaling: Verified at 2, 4, 8 threads
- ✅ Self-play: +119 ELO confirmed

**Documentation:**
- See `docs/Lazy_SMP_Explained.md` for detailed explanation

---

### Phase 4: Enhanced Move Ordering (v1.5) - **1-2 weeks** 🔄 IN PROGRESS
**Goal:** Better move ordering for deeper effective search  
**Target:** +25-45 ELO (remaining)

| Task                         | Effort      | Complexity | ELO Gain | Status     |
|------------------------------|-------------|------------|----------|------------|
| Continuation History (2-ply) | 🟡 3-5 days | 🟡 Medium  | +15-25   | 📋 Planned |
| Capture History Heuristic    | 🟡 3-5 days | 🟡 Medium  | +10-20   | 📋 Planned |
| SEE Enhancement              | 🟢 2-3 days | 🟡 Medium  | +5-10    | 📋 Planned |
| Killer Move Optimization     | 🟢 1-2 days | 🟢 Low     | +5-10    | ✅ Complete |
| Counter-Move History         | 🟡 3-5 days | 🟡 Medium  | +10-20   | ✅ Complete |

**Already Completed (v1.2-v1.3):**
- Counter-Move History implemented
- Killer Move slots optimized (2 slots, no replace if same)
- History Heuristic fixed (quiet moves only, skip alpha-raising)

**Remaining Work:**
- Continuation History (2-ply, 4-ply contexts)
- Capture History (separate table for captures)

---

### Phase 5: Endgame Tablebases (v1.2) - **2-3 weeks** ✅ COMPLETE
**Goal:** Perfect endgame play with Syzygy tablebases  
**Target:** +35-60 ELO | **Result:** Complete in v1.2

| Task                       | Effort      | Complexity | ELO Gain | Status     |
|----------------------------|-------------|------------|----------|------------|
| Fathom Library Integration | 🟢 2-3 days | 🟡 Medium  | N/A      | ✅ Complete |
| Root Tablebase Probing     | 🟢 2-3 days | 🟡 Medium  | +20-30   | ✅ Complete |
| Search Tablebase Probing   | 🟡 1 week   | 🟡 Medium  | +10-20   | ✅ Complete |
| TB Configuration & Testing | 🟢 2-3 days | 🟢 Low     | N/A      | ✅ Complete |
| TB Cache Optimization      | 🟢 2-3 days | 🟡 Medium  | +5-10    | ✅ Complete |

**Configuration (Implemented):**
```yaml
USE_TABLEBASES: true
TB_PATH: "./syzygy/"
TB_PROBE_DEPTH: 1
TB_PROBE_LIMIT: 6
```

---

### Phase 6: Advanced Search Refinements (v1.3 - v1.5) - **2-3 weeks** 🔄 PARTIAL
**Goal:** Modern search techniques for deeper tactical vision  
**Target:** +50-85 ELO | **Actual:** +52 ELO (partial)

| Task                 | Effort      | Complexity | ELO Gain | Status            |
|----------------------|-------------|------------|----------|-------------------|
| Improved LMR Formula | 🟡 1 week   | 🟡 Medium  | +15-25   | ✅ Complete (v1.3) |
| Late Move Pruning    | 🟢 2-3 days | 🟡 Medium  | +10-15   | ✅ Complete (v1.3) |
| Multi-Cut Pruning    | 🟡 3-5 days | 🟡 Medium  | +10-20   | 📋 Planned        |
| Probcut Pruning      | 🟡 3-5 days | 🟡 Medium  | +10-15   | 📋 Planned        |
| SEE-Based Pruning    | 🟢 2-3 days | 🟡 Medium  | +5-10    | 📋 Planned        |

**Completed in v1.3:**
- Logarithmic LMR formula with history-based adjustments
- Late Move Pruning with depth-dependent move count limits
- PV node fixes (isPvNode propagation)

**Remaining:**
- Multi-Cut Pruning (-10-20 ELO potential)
- Probcut (-10-15 ELO potential)
reduction = log(depth) * log(moveCount) / 2.0;
// Adjust based on: history score, TT move, checks, captures
```

#### Multi-Cut Pruning
If multiple moves fail high at reduced depth, assume beta cutoff will occur
```cpp
if (multiCutCount >= 3 && depth >= 8) {
    return beta;  // Prune remaining moves
}
```

#### Probcut
Use shallow search to predict deep search result
```cpp
if (depth >= 5) {
    Value rbeta = beta + 200;  // Raised beta
    if (shallowSearch(depth - 4, rbeta) >= rbeta) {
        return rbeta;  // Likely beta cutoff
    }
}
```

---

### Phase 7: Automated Tuning Infrastructure (v1.7) - **2-3 weeks**
**Goal:** Scientific parameter optimization  
**Target:** +30-60 ELO (from optimized parameters)

| Task                      | Effort       | Complexity | ELO Gain | Status     |
|---------------------------|--------------|------------|----------|------------|
| SPSA Framework            | 🟡 1 week    | 🟡 Medium  | N/A      | 📋 Planned |
| Texel Tuning Engine       | 🟡 1 week    | 🟡 Medium  | N/A      | 📋 Planned |
| Automated Match Runner    | 🟡 3-5 days  | 🟡 Medium  | N/A      | 📋 Planned |
| Parameter Tuning Sessions | 🔴 2-4 weeks | 🟡 Medium  | +30-60   | 📋 Planned |

**SPSA (Simultaneous Perturbation Stochastic Approximation):**
- Gradient-free optimization via game results
- Perturb parameters, play matches, update based on win rate
- Ideal for search parameters (reductions, margins, depths)

**Texel Tuning:**
- Supervised learning from labeled positions
- Minimize error between engine eval and game outcome
- Ideal for evaluation parameters (piece values, PST)

**Infrastructure Requirements:**
- Python scripts for SPSA and Texel algorithms
- cutechess-cli integration for automated matches
- Result parsing and statistical analysis
- Convergence tracking and visualization

**Configuration:**
```yaml
# tuning/spsa_config.yaml
parameters:
  - name: NMP_REDUCTION
    initial: 2
    min: 1
    max: 4
    c: 0.1  # Perturbation size
    a: 100  # Learning rate
  # ... 50+ parameters
```

---

### Phase 8: NNUE Evaluation (v1.8 → v2.0) - **6-10 weeks**
**Goal:** Neural network evaluation for dramatic strength gain  
**Target:** +200-400 ELO (largest single improvement)

| Task                       | Effort       | Complexity | ELO Gain | Status     |
|----------------------------|--------------|------------|----------|------------|
| NNUE Architecture Design   | 🟡 1 week    | 🔴 High    | N/A      | 📋 Planned |
| Incremental Update System  | 🔴 2-3 weeks | 🔴 High    | N/A      | 📋 Planned |
| NNUE Inference Engine      | 🟡 1-2 weeks | 🔴 High    | N/A      | 📋 Planned |
| Training Data Generation   | 🟡 1 week    | 🟡 Medium  | N/A      | 📋 Planned |
| Network Training Pipeline  | 🟡 1-2 weeks | 🔴 High    | N/A      | 📋 Planned |
| NNUE Integration & Testing | 🟡 1 week    | 🔴 High    | +200-400 | 📋 Planned |
| Classical/NNUE Hybrid      | 🟢 2-3 days  | 🟡 Medium  | N/A      | 📋 Planned |

**Architecture (HalfKP-256x2):**
```
Input Layer: 40960 features (king position + piece locations)
    ↓ (feature transformer)
Hidden Layer 1: 256 neurons (ClippedReLU, int16 quantized)
    ↓
Hidden Layer 2: 256 neurons (ClippedReLU, int16 quantized)
    ↓
Output: 1 neuron (linear, scaled to centipawns)
```

**Why HalfKP-256x2 (not 512x2)?**
- Smaller network = faster inference (critical for NPS)
- 256 neurons sufficient for 2600-2700 ELO
- Can upgrade to 512x2 or 1024x2 later for additional +50-100 ELO

**Implementation Strategy:**

#### Week 1-2: Architecture & Data Structures
```cpp
class NNUE {
    // Weights (loaded from file)
    alignas(64) int16_t featureWeights[40960][256];
    alignas(64) int16_t l1Weights[256][256];
    alignas(64) int16_t l2Weights[256][1];
    int32_t l1Bias[256];
    int32_t l2Bias[256];
    
    // Accumulators (incrementally updated)
    alignas(64) int16_t accumulator[2][256];  // [side][neurons]
    
    // Active feature tracking
    std::vector<int> activeFeatures[2];
};
```

#### Week 3-5: Incremental Updates
- Efficiently update accumulator when pieces move
- Only recalculate affected neurons (not full network)
- SIMD optimization (AVX2/AVX-512)

```cpp
void NNUE::addFeature(int feature, Color side) {
    // Add feature weights to accumulator
    for (int i = 0; i < 256; ++i) {
        accumulator[side][i] += featureWeights[feature][i];
    }
}

void NNUE::removeFeature(int feature, Color side) {
    // Subtract feature weights from accumulator
    for (int i = 0; i < 256; ++i) {
        accumulator[side][i] -= featureWeights[feature][i];
    }
}
```

#### Week 6-7: Inference Engine
- Forward propagation with quantized arithmetic
- ClippedReLU activation: `max(0, min(127, x))`
- Scale output to centipawn units

```cpp
Value NNUE::evaluate(const Position& pos) {
    // Layer 1: ClippedReLU(accumulator)
    int32_t l1Output[2][256];
    for (Color c : {WHITE, BLACK}) {
        for (int i = 0; i < 256; ++i) {
            l1Output[c][i] = clippedReLU(accumulator[c][i]);
        }
    }
    
    // Layer 2: Linear combination
    int32_t l2Output[2];
    for (Color c : {WHITE, BLACK}) {
        l2Output[c] = l2Bias[0];
        for (int i = 0; i < 256; ++i) {
            l2Output[c] += l1Output[c][i] * l2Weights[i][0];
        }
    }
    
    // Final output (side-to-move relative)
    int32_t output = l2Output[pos.sideToMove()] - l2Output[~pos.sideToMove()];
    
    // Scale to centipawns
    return static_cast<Value>(output / 16);  // Scaling factor
}
```

#### Week 8-9: Training Pipeline
1. **Data Generation:** Self-play games with classical eval (10M positions)
2. **Training:** Use NNUE-pytorch or custom trainer
3. **Network Export:** Convert to FrankyCPP format (.nnue file)
4. **Validation:** Test against classical eval (should be +200-300 ELO)

**Training Configuration:**
```yaml
# training/nnue_config.yaml
architecture: HalfKP-256x2
training_data: selfplay_10M.bin
batch_size: 16384
learning_rate: 0.001
epochs: 100
validation_split: 0.1
```

#### Week 10: Integration & Testing
- Add UCI option: `setoption name Use NNUE value true`
- Load network from file: `setoption name EvalFile value nn-epoch99.nnue`
- Fallback to classical if file missing or incompatible
- Verify correctness: incremental vs. full update should match

**Success Criteria:**
- NNUE eval speed: 1-2M NPS (comparable to classical)
- Self-play matches: NNUE +200-400 ELO over classical
- No evaluation spikes or instabilities
- Incremental update correctness (verified via tests)

---

### Phase 9: CPU Compatibility & Optimization (v2.0) - **1-2 weeks**
**Goal:** Broader hardware support and performance optimization  
**Target:** +15-35 ELO (performance, not tactical strength)

| Task                        | Effort      | Complexity | ELO Gain | Status     |
|-----------------------------|-------------|------------|----------|------------|
| Runtime PEXT Detection      | 🟢 2-3 days | 🟡 Medium  | N/A      | 📋 Planned |
| Software PEXT Fallback      | 🟡 3-5 days | 🟡 Medium  | N/A      | 📋 Planned |
| SIMD Optimization (AVX2)    | 🟡 1 week   | 🔴 High    | +10-20   | 📋 Planned |
| Profile-Guided Optimization | 🟢 2-3 days | 🟢 Low     | +5-15    | 📋 Planned |

**Current Limitation:** FrankyCPP requires BMI2 (PEXT instruction), excluding pre-2013 CPUs.

**Solution:**
1. **Runtime CPU Detection:** Check for BMI2 support at startup
2. **Software Fallback:** Implement PEXT using lookup tables (20% slower but functional)
3. **Dual Compilation:** Compile two versions of hot paths, select at runtime

**SIMD Optimization Targets:**
- NNUE inference (AVX2 for 256-bit vectors)
- Bitboard operations (POPCNT, TZCNT)
- Move generation (vectorized)

**Profile-Guided Optimization (PGO):**
```bash
# Step 1: Build with instrumentation
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-fprofile-generate" ...
cmake --build .

# Step 2: Run benchmark to collect profile data
./FrankyCPP_v2.0 bench

# Step 3: Rebuild with profile-guided optimization
cmake -DCMAKE_CXX_FLAGS="-fprofile-use" ...
cmake --build .
```

Expected gain: +5-15 ELO from better inlining and branch prediction.

---

## Detailed Enhancement Specifications

### 1. Singular Extensions (Phase 1) ✅ IMPLEMENTED

**Status:** Implemented in v1.2

**Concept:** Extend search depth by 1 ply when one move is significantly better than all alternatives.

**Implementation:**
- Added separate `mgSingular[ply]` MoveGenerator array for verification searches
- This prevents corruption of outer search's MoveGenerator state (singular search runs at same ply)
- `excludedMove[ply]` array tracks which move to skip during verification
- MoveGenerator selection: `excludedMove[ply] != MOVE_NONE ? mgSingular[ply] : mg[ply]`

**Configuration:**
```yaml
USE_SINGULAR_EXT: true
SINGULAR_MARGIN: 64             # centipawns
SINGULAR_MIN_DEPTH: 8           # plies
SINGULAR_REDUCTION: 4           # plies for verification search
```

**Files Changed:**
- `src/engine/config/SearchConfigData.h` - Configuration parameters
- `src/engine/Search.h` - `mgSingular` and `excludedMove` arrays
- `src/engine/Search.cpp` - Singular extension logic with separate MG
- `src/engine/SearchStats.h` - Statistics tracking
- `src/engine/UciOptions.cpp` - UCI options
- `config/search.yaml` - Default configuration
- `test/engine/SearchTest.cpp` - Unit tests

**Future:** See `docs/specs/PLAN_PlyStack_Refactoring.md` for planned `PlyInfo` struct refactoring.

**Testing Results:**
- Arena Match v1.2 vs v1.1: **+27 ELO** (53.8% score, 104 games) ✅
- Test suites: -1.3% (within noise margin)

**Expected Impact:** +20-30 ELO ✅ **Verified: +27 ELO**

---

### 2. Check Extensions (Phase 1) ✅ COMPLETE

**Concept:** Extend search when a move gives check, but only for well-ordered moves.

**Implementation (Optimized - Early Move Limit):**
Instead of counting legal replies (expensive with lazy move generation), we leverage move ordering:
- Moves are sorted by quality: TT move → captures → killers → history
- Checks appearing early in move order are more likely to be tactically important
- Only extend checks in the first N moves per node

```cpp
// In search(), after move ordering but before recursion
if (SearchConfig.USE_CHECK_EXT 
    && givesCheck 
    && movesSearched < SearchConfig.CHECK_EXT_EARLY_LIMIT) {
    statistics.checkExtension++;
    extension = DEPTH_ONE;
}
```

**Configuration:**
```yaml
USE_CHECK_EXT: true
CHECK_EXT_EARLY_LIMIT: 3      # Only extend checks in first 3 moves per node
                               # Set to 999 to extend all checks
```

**Advantages:**
- Zero overhead (just one integer comparison)
- Natural tree size control
- Focuses extensions on tactically important checks

**Testing & Results:**
- Test Suite: -35 positions (-1.2%) - minor regression
- Match Play: +30 ELO improvement
- Combined with Singular Extensions: **+57 ELO total (v1.2 vs v1.1)**

**Arena Results (v1.2 vs v1.1):**
| Metric | Result |
|--------|--------|
| Games | 104 |
| Score | 58.2% |
| W/D/L | 38/45/21 |
| ELO | +57 (combined) |

**Expected Impact:** +10-20 ELO ✅ **Verified: ~+30 ELO**

---

### 3. Counter-Move History (Phase 2) ✅ COMPLETE

**Status:** Implemented in v1.2

**Concept:** Track which moves work well as refutations to opponent's last move.

**Implementation (in `src/chesscore/History.h`):**
```cpp
// Counter-move table: [previousFrom][previousTo] -> best refutation move
std::array<std::array<Move, 64>, 64> counterMoves{};

// Update counter-move when a move causes beta cutoff
void updateCounterMove(Square prevFrom, Square prevTo, Move refutation) {
    counterMoves[prevFrom][prevTo] = refutation;
}

// Retrieve counter-move for move ordering
Move getCounterMove(Square prevFrom, Square prevTo) const {
    return counterMoves[prevFrom][prevTo];
}
```

**Usage in Search:**
- Counter-move is tried after TT move, captures, and killers
- Provides good move ordering for quiet positions
- Low memory overhead (64×64×4 = 16KB)

**Expected Impact:** +10-20 ELO ✅ **Implemented**

---

### 4. Lazy SMP Multi-Threading (Phase 3) ✅ COMPLETE

**Status:** Implemented in v1.4, enhanced in v1.5

See `docs/Lazy_SMP_Explained.md` for comprehensive explanation.

**Summary:**
- Multiple threads run independent alpha-beta searches
- Share only transposition table (thread-safe with atomic ops)
- Minimal coordination (stop flag only)
- Each thread has own `SearchThreadData` with isolated state

**Implementation (v1.4-v1.5):**
- `SearchThreadData` struct: Per-thread PVTable, PlyStack, History, Statistics
- Thread-safe TT with atomic key operations (acquire/release memory order)
- Helper threads run independent `iterativeDeepening()` with shared TT
- Comprehensive test suite: `SearchSmpTest.cpp`
- UCI `Threads` option: auto-detect or manual 1-N

**v1.5 TT Enhancements:**
- **TT Bucket Design:** 4-way associative with 64-byte cache-line alignment
- **XOR Key Verification:** Torn-read detection protocol for SMP safety
  - Store: key stored as `(originalKey ^ dataHash)` after writing data
  - Load: verify `(storedKey ^ dataHash) == probeKey` to detect corruption
- **Cache-line alignment:** Eliminates false sharing under SMP
- **Replacement policy:** Depth-preferred + age tiebreak
- **Prefetch optimization:** Single prefetch loads entire bucket (4 entries)

**Key Files:**
- `src/engine/SearchThreadData.h` - Per-thread state struct
- `src/engine/TT.h` - TT buckets with XOR key verification
- `src/engine/Search.cpp` - Thread spawning and coordination
- `test/engine/SearchSmpTest.cpp` - Comprehensive test suite

**Implementation Checklist:** ✅ All Complete
- [x] Thread-safe TT (atomic reads/writes with XOR verification)
- [x] TT bucket design (4-way associative, cache-line aligned)
- [x] Per-thread search state (History, KillerMoves, Position)
- [x] Thread spawning and coordination
- [x] Stop flag synchronization
- [x] Best move extraction from TT
- [x] Testing: 1, 2, 4, 8 threads verified
- [x] ThreadSanitizer clean (race conditions eliminated)

**Expected Impact:** +60-120 ELO ✅ **Verified: +119 ELO (v1.4 vs v1.3)**

---

### 5. Syzygy Tablebase Integration (Phase 5) ✅ COMPLETE

**Status:** Implemented in v1.2

**Library:** Fathom (https://github.com/jdart1/Fathom) - integrated via CMake

**Implementation:**
- Root probing: Before iterative deepening, check if position is in TB
- Search probing: At low depth, use WDL to adjust search bounds
- DTZ probing: Distance-to-zero for optimal conversion

**Configuration (Implemented):**
```yaml
USE_TABLEBASES: true
TB_PATH: "./syzygy/"
TB_PROBE_DEPTH: 1
TB_PROBE_LIMIT: 6               # 6-piece TBs
```

**UCI Options:**
- `SyzygyPath` - Path to tablebase files
- `SyzygyProbeDepth` - Minimum depth for TB probing
- `SyzygyProbeLimit` - Maximum pieces for TB probing

**Testing:**
- ✅ Perfect play verified in 3-6 piece endgames
- ✅ TB lookup overhead minimal (< 1% of search time)
- ✅ Graceful fallback when TBs not available

**Expected Impact:** +35-60 ELO ✅ **Implemented in v1.2**

---

### 6. NNUE Implementation (Phase 7)

**Architecture:** HalfKP-256x2 (40960 → 256 → 256 → 1)

**Key Components:**
1. **Feature Extraction:** King position + piece positions → 40960 input features
2. **Incremental Updates:** Efficiently update when pieces move (add/remove features)
3. **Inference:** Forward propagation with quantized int16 arithmetic
4. **Training:** Self-play data + gradient descent

**Performance Target:**
- Evaluation speed: 1-2M NPS (comparable to classical)
- Strength: +200-400 ELO over classical

**Fallback:** Classical eval if NNUE file missing or incompatible

**Expected Impact:** +200-400 ELO (largest single improvement)

---

## Implementation Tracking

### Progress Dashboard

| Phase     | Version         | Tasks  | Completed | In Progress | Planned | ELO Gain Target | ELO Gain Actual | Status              |
|-----------|-----------------|--------|-----------|-------------|---------|-----------------|-----------------|---------------------|
| 1         | v1.1            | 4      | 4         | 0           | 0       | N/A             | N/A             | ✅ Complete          |
| 2         | v1.2            | 7      | 6         | 0           | 1       | +70-135         | +57             | ✅ Complete          |
| 3         | v1.4-v1.5       | 8      | 8         | 0           | 0       | +60-120         | +119            | ✅ Complete          |
| 4         | v1.5+           | 5      | 2         | 0           | 3       | +35-65          | TBD             | 🔄 In Progress      |
| 5         | v1.2            | 5      | 5         | 0           | 0       | +35-60          | +30-50          | ✅ Complete          |
| 6         | v1.3-v1.5       | 5      | 2         | 0           | 3       | +50-85          | +52 (partial)   | 🔄 In Progress      |
| 7         | v1.6            | 4      | 0         | 0           | 4       | +30-60          | TBD             | 📋 Planned          |
| 8         | v1.7-v1.9       | 7      | 0         | 0           | 7       | +200-400        | TBD             | 📋 Planned          |
| 9         | v2.0            | 4      | 0         | 0           | 4       | +15-35          | TBD             | 📋 Planned          |
| **TOTAL** | **v1.1 → v2.0** | **49** | **27**    | **0**       | **22**  | **+495-960**    | **+228 actual** | 🔄 **55% Complete** |

**Cumulative ELO Gains:**
- v1.1 → v1.2: +57 ELO (Singular/Check Extensions, Counter-Move History)
- v1.2 → v1.3: +52 ELO (LMR/History fixes)
- v1.3 → v1.4: +119 ELO (SMP + search refinements)
- v1.4 → v1.5: TT buckets, XOR key verification, race fixes (stability improvements)
- **Total v1.1 → v1.5: +228 ELO + improved SMP stability**

### Gantt Chart (Actual Progress + Estimated Timeline)

```
Month 1 (Feb 2026):
  ├─ Phase 1: Testing Infrastructure     ✅ COMPLETE
  ├─ Phase 2: Quick Wins (v1.2)          ✅ COMPLETE (+57 ELO)
  └─ Phase 5: Tablebases (v1.2)          ✅ COMPLETE

Month 2 (Feb-Mar 2026):
  ├─ Phase 6: LMR/History fixes (v1.3)   ✅ COMPLETE (+109 ELO cumulative)
  └─ Phase 3: Multi-Threading (v1.4)     ✅ COMPLETE (+228 ELO cumulative)

Month 3 (Mar 2026) - CURRENT:
  ├─ Phase 3: TT Buckets/XOR Key (v1.5)  ✅ COMPLETE (SMP stability)
  └─ Phase 4: Move Ordering (v1.5)       🔄 IN PROGRESS

Month 4 (Apr 2026):
  └─ Phase 6: Remaining Search (v1.6)    📋 PLANNED (Multi-Cut, Probcut)

Month 5-6 (May-Jun 2026):
  └─ Phase 7: Tuning Infrastructure      📋 PLANNED

Month 7-9 (Jul-Sep 2026):
  └─ Phase 8: NNUE Implementation        📋 PLANNED

Month 10 (Oct 2026):
  └─ Phase 9: Optimization & Release     📋 PLANNED
```

**Actual Timeline:** Ahead of original schedule due to combined phase completion

---

## Testing & Validation Strategy

### 1. Unit Tests (Regression Prevention)
- Extend GoogleTest suite for each new feature
- Maintain 100% test pass rate across all platforms
- Use FRIEND_TEST for internal state validation
- CI/CD runs all tests on every commit

### 2. Tactical Test Suites (Tactical Strength)
| Suite         | Positions | Focus         | Baseline | Target  |
|---------------|-----------|---------------|----------|---------|
| **WAC**       | 300       | Tactics       | 250/300  | 285/300 |
| **STS**       | 1500      | Strategy      | 60%      | 75%     |
| **Arasan**    | 2400      | Comprehensive | 55%      | 70%     |
| **Mate in N** | 100       | Mating        | 65/100   | 90/100  |

### 3. Benchmark Matches (Strength Validation)
- Play 1000+ game matches after each phase
- Opponents: Previous FrankyCPP version + external engines
- Time controls: 10+0.1 (rapid), 60+0.6 (classical)
- Tools: cutechess-cli, BayesElo for rating calculation

**Match Configuration:**
```bash
# Example: v1.2 vs v1.1 (after Phase 1 complete)
cutechess-cli \
  -engine cmd=./FrankyCPP_v1.2 name=v1.2 \
  -engine cmd=./FrankyCPP_v1.1 name=v1.1 \
  -each tc=10+0.1 \
  -rounds 1000 \
  -openings file=UHO_Lichess_4852_v1.pgn format=pgn \
  -pgnout matches/v1.2_vs_v1.1.pgn \
  -ratinginterval 10
```

### 4. Performance Benchmarks (Speed Validation)
- Track nodes-per-second on standard positions
- Monitor search depth reached at fixed time
- Profile hot paths with perf/VTune/Visual Studio Profiler

**Benchmark Positions:**
```
1. Initial position (e2e4)
2. Kiwipete (tactical position)
3. Endgame position (KQK)
4. Complex middlegame (20+ pieces)
```

**Targets:**

| Metric             | Baseline (v1.1) | Current (v1.4) | Target (v2.0) |
|--------------------|-----------------|----------------|---------------|
| NPS (1 thread)     | 1.5M            | 1.8M           | 2.0M          |
| NPS (8 threads)    | N/A             | 8-10M ✅        | 10-12M        |
| Depth (10s search) | 10-12 ply       | 12-14 ply      | 14-16 ply     |

### 5. Regression Testing (Stability)
- Maintain EPD test suite with expected results
- Automated CI/CD testing on every commit
- Fail builds on significant strength regression (> -20 ELO)

---

## Risk Assessment

### Technical Risks

| Risk                              | Impact | Probability | Mitigation                                                               |
|-----------------------------------|--------|-------------|--------------------------------------------------------------------------|
| **Search Instability (Lazy SMP)** | High   | Medium      | Extensive testing with 1 thread (deterministic), gradual rollout         |
| **NNUE Training Failure**         | High   | Medium      | Start with existing trained nets, fallback to classical always available |
| **Performance Regression**        | Medium | Low         | Continuous benchmarking in CI/CD, automated regression detection         |
| **TT Contention (SMP)**           | Medium | Medium      | Proper TT sizing (128 MB per thread), lock-free atomic ops               |
| **CPU Compatibility (PEXT)**      | Low    | High        | Software fallback implementation, runtime detection                      |

### Project Risks

| Risk                   | Impact | Probability | Mitigation                                                                |
|------------------------|--------|-------------|---------------------------------------------------------------------------|
| **Scope Creep**        | Medium | Medium      | Strict phase boundaries, no mixing of tasks                               |
| **Time Overruns**      | Low    | Medium      | Conservative effort estimates, prioritize high-impact items first         |
| **Loss of Motivation** | Medium | Low         | Celebrate milestones, track progress visibly, focus on measurable gains   |
| **NNUE Complexity**    | High   | Medium      | Break into small tasks, use existing libraries/frameworks, allow 10 weeks |

### Risk Mitigation Summary

1. **Incremental Development:** Each phase delivers working, tested functionality
2. **Automated Testing:** CI/CD catches regressions immediately
3. **Fallback Options:** Classical eval, single-threaded mode always available
4. **Community Support:** Reference implementations (Stockfish, Ethereal) for guidance
5. **Flexible Timeline:** Adjust priorities based on actual progress and learnings

---

## References & Resources

### Academic Papers
1. **"Lazy SMP" by Martin Sedlak (2013)** - Original Lazy SMP paper
2. **"Efficiently Updatable Neural Networks for Chess" by Nasu (2018)** - NNUE introduction
3. **"Texel's Tuning Method" by Peter Österlund (2012)** - Evaluation parameter tuning
4. **"Parallel Alpha-Beta Search" by Feldmann (1993)** - Classical parallel search

### Open Source Engines
| Engine        | Language | Notable Features                            | GitHub                                                                |
|---------------|----------|---------------------------------------------|-----------------------------------------------------------------------|
| **Stockfish** | C++17    | NNUE, Lazy SMP, reference implementation    | [official-stockfish](https://github.com/official-stockfish/Stockfish) |
| **Ethereal**  | C        | Clean classical eval, tuning infrastructure | [AndyGrant/Ethereal](https://github.com/AndyGrant/Ethereal)           |
| **Koivisto**  | C++      | Modern C++, NNUE training pipeline          | [Luecx/Koivisto](https://github.com/Luecx/Koivisto)                   |
| **Berserk**   | C        | Recent NNUE implementation, good docs       | [jhonnold/berserk](https://github.com/jhonnold/berserk)               |
| **RubiChess** | C++      | Clean SMP implementation                    | [Matthies/RubiChess](https://github.com/Matthies/RubiChess)           |

### Libraries & Tools
- **Fathom:** Syzygy tablebase probing (https://github.com/jdart1/Fathom)
- **NNUE-pytorch:** Network training framework (https://github.com/glinscott/nnue-pytorch)
- **cutechess-cli:** Automated engine matches (https://github.com/cutechess/cutechess)
- **BayesElo:** Rating calculation (https://www.remi-coulom.fr/Bayesian-Elo/)
- **fastchess:** Modern match runner (https://github.com/Disservin/fastchess)

### Community Resources
1. **Chess Programming Wiki:** https://www.chessprogramming.org/
2. **TalkChess Forum:** http://talkchess.com/
3. **Engine Programming Discord:** Invite via TalkChess
4. **Computer Chess Club:** http://www.computerchess.org.uk/ccrl/

### Training Data Sources
1. **Lichess Database:** Millions of games (https://database.lichess.org/)
2. **CCRL Games:** High-quality engine games (http://ccrl.chessdom.com/)
3. **Self-Play Data:** Generate via engine matches

---

## Appendix: Quick Reference

### ELO Gain Summary by Phase

| Phase | Version     | Timeframe    | ELO Target | ELO Actual | Cumulative | Status      |
|-------|-------------|--------------|------------|------------|------------|-------------|
| 1     | v1.1        | Feb 2026     | N/A        | N/A        | 0          | ✅ Complete  |
| 2     | v1.1 → v1.2 | Feb 2026     | +70-135    | **+57**    | +57        | ✅ Complete  |
| 3     | v1.3 → v1.4 | Mar 2026     | +60-120    | **+119**   | +228       | ✅ Complete  |
| 4     | v1.4 → v1.5 | Mar-Apr 2026 | +25-45     | TBD        | ~250-275   | 🔄 Progress |
| 5     | v1.2        | Feb 2026     | +35-60     | (incl.)    | (included) | ✅ Complete  |
| 6     | v1.3        | Feb-Mar 2026 | +50-85     | **+52**    | (included) | 🔄 Partial  |
| 7     | v1.6        | Apr-May 2026 | +30-60     | TBD        | ~280-335   | 📋 Planned  |
| 8     | v1.7-v1.9   | May-Sep 2026 | +200-400   | TBD        | ~480-735   | 📋 Planned  |
| 9     | v2.0        | Oct 2026     | +15-35     | TBD        | ~495-770   | 📋 Planned  |

**Verified Results:**
- v1.1 baseline: ~2400-2450 ELO (estimated)
- v1.4 current: ~2600-2650 ELO (+228 ELO verified)
- v2.0 target: ~2900-3100 ELO

### Configuration File Changes

Each phase adds new YAML parameters:

**Phase 2 (v1.2) - ✅ IMPLEMENTED:**
```yaml
# config/search.yaml additions (Singular Extensions)
USE_SINGULAR_EXT: true
SINGULAR_MARGIN: 64
SINGULAR_MIN_DEPTH: 8
SINGULAR_REDUCTION: 4

# Check Extensions
USE_CHECK_EXT: true
CHECK_EXT_EARLY_LIMIT: 3
```

**Phase 3 (v1.4) - ✅ IMPLEMENTED:**
```yaml
# config/search.yaml additions (Lazy SMP)
THREADS: 0                      # 0 = auto-detect CPU cores
# Per-thread state managed internally via SearchThreadData
```

**Phase 5 (v1.2) - ✅ IMPLEMENTED:**
```yaml
# config/engine.yaml additions (Tablebases)
USE_TABLEBASES: true
TB_PATH: "./syzygy/"
TB_PROBE_DEPTH: 1
TB_PROBE_LIMIT: 6
```

**Phase 4 (v1.5) - 📋 PLANNED:**
```yaml
# Continuation History (when implemented)
USE_CONTINUATION_HISTORY: true
CONT_HIST_BONUS_SCALE: 32
```

**Phase 8 (v1.8+) - 📋 PLANNED:**
```yaml
# config/eval.yaml additions
USE_NNUE: true
NNUE_NET_PATH: "./networks/nn-epoch99.nnue"
NNUE_FALLBACK_CLASSICAL: true
```

---

**Document Status:** Active Development  
**Maintainer:** Frank Kopp  
**Created:** 2026-02-01  
**Last Updated:** 2026-03-06  
**Next Review:** After v1.5 release

---

## Change Log

| Version | Date       | Changes                                                                              | Author     |
|---------|------------|--------------------------------------------------------------------------------------|------------|
| 2.3     | 2026-03-06 | Updated for v1.4/v1.5: SMP complete (+119 ELO), phases reorganized, progress tracked | Frank Kopp |
| 2.2     | 2026-02-20 | Phase 2 progress update                                                              | Frank Kopp |
| 2.1     | 2026-02-08 | Phase 1 (v1.1) marked complete; Phase 2 in progress with Triangular PV Table done    | Frank Kopp |
| 2.0     | 2026-02-01 | Complete rewrite with detailed phase planning, ELO estimates, implementation specs   | Frank Kopp |
| 1.0     | 2026-02-01 | Initial V1_ENGINE_ENHANCEMENT_PLAN.md                                                | Frank Kopp |

---

*This document is a living roadmap and will be updated as implementation progresses. Each phase completion triggers a review and potential adjustment of subsequent phases based on actual results.*
