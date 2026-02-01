# FrankyCPP v1.x Engine Strength Roadmap

**Document Version:** 2.0  
**Created:** 2026-02-01  
**Status:** Active Planning  
**Target:** FrankyCPP v1.1 → v2.0  
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

This document provides a comprehensive, prioritized roadmap for enhancing FrankyCPP's playing strength from v1.1 to v2.0. Based on extensive analysis of modern chess engine techniques and empirical data from top engines (Stockfish, Ethereal, Koivisto), this plan identifies **high-impact, feasible improvements** organized into logical implementation phases.

### Current State (v1.1.0 - February 2026)

**Strengths:**
- ✅ Production-ready classical chess engine with UCI protocol
- ✅ Alpha-beta search with modern pruning (NMP, LMR, futility, razoring)
- ✅ Classical evaluation (material, PST, pawn structure, mobility, king safety)
- ✅ Comprehensive testing infrastructure (266+ tests, CI/CD)
- ✅ Cross-platform support (Windows MSVC, Linux GCC/Clang)
- ✅ 100+ configurable parameters via YAML
- ✅ Estimated strength: ~2400-2450 ELO (amateur master level)

**Key Limitations:**
- ❌ Single-threaded search (underutilizes modern multi-core CPUs)
- ❌ Classical evaluation only (NNUE offers +200-400 ELO improvement)
- ❌ No endgame tablebase support (imperfect endgame play)
- ❌ Missing modern search techniques (singular extensions, check extensions)
- ❌ Limited move ordering heuristics (no continuation/capture history)

### Target State (v2.0 - End Goal)

**Target Playing Strength:** ~2800-2900 ELO (approaching master/IM level)  
**Expected Total Gain:** +400-500 ELO

**Key Enhancements:**
1. ✅ Multi-threaded parallel search (Lazy SMP) - **+60-120 ELO**
2. ✅ Neural network evaluation (NNUE) - **+200-400 ELO**
3. ✅ Endgame tablebase support (Syzygy) - **+35-60 ELO**
4. ✅ Advanced search techniques - **+50-100 ELO**
5. ✅ Enhanced move ordering - **+35-65 ELO**
6. ✅ Automated parameter tuning - **+30-60 ELO**

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

| Component | Current State | Quality | Notes |
|-----------|---------------|---------|-------|
| **Build System** | CMake + vcpkg | ⭐⭐⭐⭐⭐ | Excellent cross-platform support |
| **Testing** | 266+ unit tests | ⭐⭐⭐⭐⭐ | Comprehensive coverage, CI/CD integrated |
| **Configuration** | YAML-based | ⭐⭐⭐⭐⭐ | Flexible, runtime-configurable |
| **Code Quality** | Modern C++20 | ⭐⭐⭐⭐⭐ | Clean architecture, good separation of concerns |
| **Move Generation** | Bitboard-based | ⭐⭐⭐⭐⭐ | Fast, PEXT-optimized, well-tested |
| **UCI Protocol** | Full compliance | ⭐⭐⭐⭐⭐ | Compatible with all GUIs/tournament managers |
| **Logging** | spdlog-based | ⭐⭐⭐⭐⭐ | Excellent debugging infrastructure |

### Areas for Improvement

#### 1. Search Algorithm (High Priority)

| Feature | Current State | Industry Standard | Gap Analysis |
|---------|---------------|-------------------|--------------|
| **Parallel Search** | ❌ Single-threaded | ✅ Lazy SMP (8-16 threads) | **-60-120 ELO** |
| **Singular Extensions** | ❌ Not implemented | ✅ Standard in top engines | **-20-30 ELO** |
| **Check Extensions** | ❌ Not implemented | ✅ Standard in top engines | **-10-20 ELO** |
| **Multi-Cut Pruning** | ❌ Not implemented | ✅ Common in modern engines | **-10-20 ELO** |
| **Probcut** | ❌ Not implemented | ✅ Used in Stockfish/Ethereal | **-10-15 ELO** |
| **LMR Formula** | ⚠️ Basic implementation | ✅ Tuned formula | **-15-25 ELO** |

**Estimated Total Search Gap:** -125-230 ELO

#### 2. Evaluation Function (Critical Priority)

| Feature | Current State | Industry Standard | Gap Analysis |
|---------|---------------|-------------------|--------------|
| **NNUE** | ❌ Not implemented | ✅ Standard in 2600+ engines | **-200-400 ELO** |
| **Eval Tuning** | ⚠️ Manual tuning | ✅ Texel/SPSA automated | **-30-60 ELO** |
| **Pawn Structure** | ✅ Basic implementation | ✅ Similar | ~0 ELO |
| **King Safety** | ✅ Basic implementation | ✅ Similar | ~0 ELO |
| **Mobility** | ✅ Implemented | ✅ Similar | ~0 ELO |

**Estimated Total Eval Gap:** -230-460 ELO

#### 3. Move Ordering (Medium Priority)

| Feature | Current State | Industry Standard | Gap Analysis |
|---------|---------------|-------------------|--------------|
| **History Heuristic** | ✅ Implemented | ✅ Standard | ~0 ELO |
| **Killer Moves** | ✅ Implemented (2 slots) | ✅ Standard | ~0 ELO |
| **Counter-Move History** | ❌ Not implemented | ✅ Standard | **-10-20 ELO** |
| **Continuation History** | ❌ Not implemented | ✅ Standard | **-15-25 ELO** |
| **Capture History** | ❌ Not implemented | ✅ Common | **-10-20 ELO** |
| **SEE (Static Exchange)** | ✅ Basic implementation | ✅ Similar | ~0 ELO |

**Estimated Total Move Ordering Gap:** -35-65 ELO

#### 4. Endgame Play (Medium Priority)

| Feature | Current State | Industry Standard | Gap Analysis |
|---------|---------------|-------------------|--------------|
| **Syzygy Tablebases** | ❌ Not implemented | ✅ Standard (6-7 piece) | **-35-60 ELO** |
| **Endgame Patterns** | ⚠️ Basic (draw detection) | ✅ Comprehensive | **-10-20 ELO** |
| **Scaling Functions** | ✅ Basic implementation | ✅ Similar | ~0 ELO |

**Estimated Total Endgame Gap:** -45-80 ELO

### Total Estimated Strength Gap

| Category | ELO Gap |
|----------|---------|
| Search Algorithm | -125 to -230 |
| Evaluation Function | -230 to -460 |
| Move Ordering | -35 to -65 |
| Endgame Play | -45 to -80 |
| **TOTAL** | **-435 to -835 ELO** |

**Interpretation:** FrankyCPP v1.1 is performing 400-800 ELO below modern 2800+ engines. Closing this gap is achievable through systematic implementation of proven techniques.

---

## Enhancement Categories

### Category A: Foundation (Prerequisites for other enhancements)
- Thread-safe transposition table
- Refactored search architecture (per-thread state)
- Automated testing infrastructure for strength validation

### Category B: Quick Wins (High ROI, Low Risk)
- Singular extensions
- Check extensions
- Counter-move history
- Best-move instability time management

### Category C: Parallelization (Hardware Utilization)
- Lazy SMP multi-threaded search
- Thread pool integration
- SMP-specific optimizations

### Category D: Evaluation Revolution (Largest Single Gain)
- NNUE implementation (incremental updates, inference)
- Training pipeline
- Classical/NNUE hybrid mode

### Category E: Advanced Search (Incremental Gains)
- Multi-cut pruning
- Probcut
- Improved LMR formula
- Late move pruning
- SEE-based pruning enhancements

### Category F: Move Ordering Refinements
- Continuation history (2-ply, 4-ply)
- Capture history heuristic
- Static exchange evaluation improvements

### Category G: Endgame Specialization
- Syzygy tablebase integration (Fathom library)
- Root and search probing
- Endgame pattern recognition

### Category H: Infrastructure & Tuning
- SPSA parameter optimization
- Texel tuning for evaluation weights
- Automated match running and ELO tracking

---

## Prioritized Roadmap

### Phase 1: Quick Wins (v1.1 → v1.2) - **2-3 weeks**
**Goal:** Immediate strength gains with minimal risk  
**Target:** +60-110 ELO

| Task | Effort | Complexity | ELO Gain | Status |
|------|--------|------------|----------|--------|
| Singular Extensions | 🟢 2-3 days | 🟡 Medium | +20-30 | 📋 Planned |
| Check Extensions | 🟢 2-3 days | 🟡 Medium | +10-20 | 📋 Planned |
| Counter-Move History | 🟡 3-5 days | 🟡 Medium | +10-20 | 📋 Planned |
| Best-Move Instability Time Mgmt | 🟢 2-3 days | 🟡 Medium | +5-15 | 📋 Planned |
| Selective Checks in Quiescence | 🟡 3-5 days | 🟡 Medium | +15-25 | 📋 Planned |

**Why This Order?**
1. **Extensions first:** Improve search depth quality
2. **Move ordering next:** Makes extensions more effective
3. **Time management:** Ensures we search to useful depths
4. **Quiescence:** Improves tactical reliability

**Success Criteria:**
- All unit tests pass
- Benchmark suite shows +60-110 ELO gain
- No search instability (verified via long TC games)

---

### Phase 2: Multi-Threading (v1.2 → v1.3) - **3-4 weeks**
**Goal:** Utilize modern multi-core CPUs  
**Target:** +60-120 ELO (on 4-8 cores)

| Task | Effort | Complexity | ELO Gain | Status |
|------|--------|------------|----------|--------|
| Thread-Safe TT | 🟡 1 week | 🔴 High | N/A | 📋 Planned |
| Per-Thread Search State | 🟡 1 week | 🔴 High | N/A | 📋 Planned |
| Lazy SMP Implementation | 🟡 1-2 weeks | 🔴 High | +50-100 | 📋 Planned |
| Thread Pool Integration | 🟢 2-3 days | 🟡 Medium | N/A | 📋 Planned |
| SMP Testing & Tuning | 🟡 3-5 days | 🟡 Medium | +10-20 | 📋 Planned |

**Implementation Strategy:**
1. **Week 1:** Make TT thread-safe (atomic operations, lock-free probe/store)
2. **Week 2:** Refactor Search class (extract per-thread state into SearchThread)
3. **Week 3:** Implement Lazy SMP (spawn threads, diversification, stop coordination)
4. **Week 4:** Test and tune (1, 2, 4, 8, 16 threads benchmarks)

**Critical Success Factors:**
- ThreadSanitizer reports no races
- Single-threaded mode produces identical results (determinism)
- Scaling efficiency: 80%+ at 4 threads, 65%+ at 8 threads
- Self-play matches confirm ELO gain

**Documentation:**
- See `docs/Lazy_SMP_Explained.md` for detailed explanation
- Implementation checklist provided in that document

---

### Phase 3: Enhanced Move Ordering (v1.3 → v1.4) - **1-2 weeks**
**Goal:** Better move ordering for deeper effective search  
**Target:** +35-65 ELO

| Task | Effort | Complexity | ELO Gain | Status |
|------|--------|------------|----------|--------|
| Continuation History (2-ply) | 🟡 3-5 days | 🟡 Medium | +15-25 | 📋 Planned |
| Capture History Heuristic | 🟡 3-5 days | 🟡 Medium | +10-20 | 📋 Planned |
| SEE Enhancement | 🟢 2-3 days | 🟡 Medium | +5-10 | 📋 Planned |
| Killer Move Optimization | 🟢 1-2 days | 🟢 Low | +5-10 | 📋 Planned |

**Implementation Notes:**
- **Continuation History:** Track move effectiveness based on previous move context
- **Capture History:** Separate history table for capture moves
- **SEE Enhancement:** Improve static exchange evaluation accuracy
- **Killer Optimization:** Experiment with 3-4 killer slots per ply

**Success Criteria:**
- First move tried is correct more often (measure via search logs)
- Node count reduction on standard test positions
- Benchmark matches show +35-65 ELO

---

### Phase 4: Endgame Tablebases (v1.4 → v1.5) - **2-3 weeks**
**Goal:** Perfect endgame play with Syzygy tablebases  
**Target:** +35-60 ELO (in TB-relevant positions)

| Task | Effort | Complexity | ELO Gain | Status |
|------|--------|------------|----------|--------|
| Fathom Library Integration | 🟢 2-3 days | 🟡 Medium | N/A | 📋 Planned |
| Root Tablebase Probing | 🟢 2-3 days | 🟡 Medium | +20-30 | 📋 Planned |
| Search Tablebase Probing | 🟡 1 week | 🟡 Medium | +10-20 | 📋 Planned |
| TB Configuration & Testing | 🟢 2-3 days | 🟢 Low | N/A | 📋 Planned |
| TB Cache Optimization | 🟢 2-3 days | 🟡 Medium | +5-10 | 📋 Planned |

**Implementation Strategy:**
1. Add Fathom as vcpkg dependency
2. Implement root probing (before iterative deepening)
3. Implement search probing (at depth 1, before quiescence)
4. Add UCI options for TB path configuration
5. Test with standard TB test suites

**Configuration:**
```yaml
USE_TABLEBASES: true
TB_PATH: "./syzygy/"
TB_PROBE_DEPTH: 1
TB_PROBE_LIMIT: 6           # 6-piece TBs (7-piece optional)
TB_CACHE_MB: 32
```

**Testing:**
- Verify perfect play in 3-6 piece endgames
- Benchmark TB lookup overhead (should be < 1% of search time)
- Ensure graceful fallback when TBs not available

---

### Phase 5: Advanced Search Refinements (v1.5 → v1.6) - **2-3 weeks**
**Goal:** Modern search techniques for deeper tactical vision  
**Target:** +50-85 ELO

| Task | Effort | Complexity | ELO Gain | Status |
|------|--------|------------|----------|--------|
| Improved LMR Formula | 🟡 1 week | 🟡 Medium | +15-25 | 📋 Planned |
| Multi-Cut Pruning | 🟡 3-5 days | 🟡 Medium | +10-20 | 📋 Planned |
| Probcut Pruning | 🟡 3-5 days | 🟡 Medium | +10-15 | 📋 Planned |
| Late Move Pruning | 🟢 2-3 days | 🟡 Medium | +10-15 | 📋 Planned |
| SEE-Based Pruning | 🟢 2-3 days | 🟡 Medium | +5-10 | 📋 Planned |

**Implementation Notes:**

#### Improved LMR Formula
Current: Simple depth-based reduction  
Target: Logarithmic formula with move count and depth factors
```cpp
// Stockfish-style LMR formula
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

### Phase 6: Automated Tuning Infrastructure (v1.6 → v1.7) - **2-3 weeks**
**Goal:** Scientific parameter optimization  
**Target:** +30-60 ELO (from optimized parameters)

| Task | Effort | Complexity | ELO Gain | Status |
|------|--------|------------|----------|--------|
| SPSA Framework | 🟡 1 week | 🟡 Medium | N/A | 📋 Planned |
| Texel Tuning Engine | 🟡 1 week | 🟡 Medium | N/A | 📋 Planned |
| Automated Match Runner | 🟡 3-5 days | 🟡 Medium | N/A | 📋 Planned |
| Parameter Tuning Sessions | 🔴 2-4 weeks | 🟡 Medium | +30-60 | 📋 Planned |

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

### Phase 7: NNUE Evaluation (v1.7 → v1.9) - **6-10 weeks**
**Goal:** Neural network evaluation for dramatic strength gain  
**Target:** +200-400 ELO (largest single improvement)

| Task | Effort | Complexity | ELO Gain | Status |
|------|--------|------------|----------|--------|
| NNUE Architecture Design | 🟡 1 week | 🔴 High | N/A | 📋 Planned |
| Incremental Update System | 🔴 2-3 weeks | 🔴 High | N/A | 📋 Planned |
| NNUE Inference Engine | 🟡 1-2 weeks | 🔴 High | N/A | 📋 Planned |
| Training Data Generation | 🟡 1 week | 🟡 Medium | N/A | 📋 Planned |
| Network Training Pipeline | 🟡 1-2 weeks | 🔴 High | N/A | 📋 Planned |
| NNUE Integration & Testing | 🟡 1 week | 🔴 High | +200-400 | 📋 Planned |
| Classical/NNUE Hybrid | 🟢 2-3 days | 🟡 Medium | N/A | 📋 Planned |

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

### Phase 8: CPU Compatibility & Optimization (v1.9 → v2.0) - **1-2 weeks**
**Goal:** Broader hardware support and performance optimization  
**Target:** +15-35 ELO (performance, not tactical strength)

| Task | Effort | Complexity | ELO Gain | Status |
|------|--------|------------|----------|--------|
| Runtime PEXT Detection | 🟢 2-3 days | 🟡 Medium | N/A | 📋 Planned |
| Software PEXT Fallback | 🟡 3-5 days | 🟡 Medium | N/A | 📋 Planned |
| SIMD Optimization (AVX2) | 🟡 1 week | 🔴 High | +10-20 | 📋 Planned |
| Profile-Guided Optimization | 🟢 2-3 days | 🟢 Low | +5-15 | 📋 Planned |

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

### 1. Singular Extensions (Phase 1)

**Concept:** Extend search depth by 1 ply when one move is significantly better than all alternatives.

**Algorithm:**
```cpp
// In search(), after trying TT move
if (ttMove != MOVE_NONE && depth >= SINGULAR_MIN_DEPTH) {
    Value ttValue = ttEntry->value;
    
    // Reduced-depth search with null window, excluding TT move
    Value rBeta = ttValue - SINGULAR_MARGIN;
    Value singularValue = search(pos, depth - 4, rBeta - 1, rBeta, excludeTTMove);
    
    // If no other move beats (ttValue - margin), extend TT move
    if (singularValue < rBeta) {
        extension = 1;  // Extend by 1 ply
    }
}
```

**Configuration:**
```yaml
USE_SINGULAR_EXTENSIONS: true
SINGULAR_MARGIN: 50             # centipawns
SINGULAR_MIN_DEPTH: 8           # plies
SINGULAR_REDUCTION: 4           # plies for verification search
SINGULAR_MAX_EXTENSIONS: 16     # per branch
```

**Testing:**
- Tactical test suites (WAC, STS)
- Verify no search explosion (cap extensions per branch)
- Measure depth increase and ELO gain

**Expected Impact:** +20-30 ELO

---

### 2. Check Extensions (Phase 1)

**Concept:** Extend search when a move gives check with few legal replies.

**Algorithm:**
```cpp
// After makeMove() in search()
if (pos.inCheck()) {
    int legalMoves = countLegalMoves(pos);  // Fast early-exit count
    
    if (legalMoves <= CHECK_EXT_MAX_REPLIES) {
        extension = 1;
    }
}
```

**Configuration:**
```yaml
USE_CHECK_EXTENSIONS: true
CHECK_EXT_MAX_REPLIES: 2        # Extend if ≤ 2 legal replies
CHECK_EXT_MAX_DEPTH: 16         # Cap total extensions per branch
```

**Testing:**
- Mating test suites (mate in N problems)
- Measure mate-finding improvement
- Verify no search explosion

**Expected Impact:** +10-20 ELO

---

### 3. Counter-Move History (Phase 1)

**Concept:** Track which moves work well as refutations to opponent's last move.

**Data Structure:**
```cpp
// counterMoveHistory[opponentPiece][opponentTo][myPiece][myTo]
int16_t counterMoveHistory[PIECE_NB][SQUARE_NB][PIECE_NB][SQUARE_NB];

void updateCounterMove(Move counterMove, Move previousMove, int bonus) {
    Piece oppPiece = pieceOn(from_sq(previousMove));
    Square oppTo = to_sq(previousMove);
    Piece myPiece = pieceOn(from_sq(counterMove));
    Square myTo = to_sq(counterMove);
    
    // Update with decaying bonus
    int& entry = counterMoveHistory[oppPiece][oppTo][myPiece][myTo];
    entry += bonus - entry * abs(bonus) / CMH_MAX;
}
```

**Configuration:**
```yaml
USE_COUNTER_MOVE_HISTORY: true
CMH_MAX_BONUS: 400
CMH_BONUS_SCALE: 32
```

**Testing:**
- Measure first-move success rate improvement
- Node count reduction on standard positions

**Expected Impact:** +10-20 ELO

---

### 4. Lazy SMP Multi-Threading (Phase 2)

See `docs/Lazy_SMP_Explained.md` for comprehensive explanation.

**Summary:**
- Multiple threads run independent alpha-beta searches
- Share only transposition table (thread-safe with atomic ops)
- Minimal coordination (stop flag only)
- Diversification via skip-size variation at root

**Implementation Checklist:**
- [ ] Thread-safe TT (atomic reads/writes)
- [ ] Per-thread search state (History, KillerMoves, Position)
- [ ] Thread spawning and coordination
- [ ] Stop flag synchronization
- [ ] Best move extraction from TT
- [ ] Testing: 1, 2, 4, 8, 16 threads

**Expected Impact:** +60-120 ELO (on 4-8 cores)

---

### 5. Syzygy Tablebase Integration (Phase 4)

**Library:** Fathom (https://github.com/jdart1/Fathom)

**Implementation:**
```cpp
class TablebaseProber {
    // Initialize Fathom library
    bool init(const std::string& tbPath) {
        return tb_init(tbPath.c_str());
    }
    
    // Probe at root (before search)
    Move probeRoot(const Position& pos) {
        unsigned result = tb_probe_root(...);
        return extractBestMove(result);
    }
    
    // Probe during search (at depth 1)
    Value probeWDL(const Position& pos) {
        unsigned wdl = tb_probe_wdl(...);
        return convertToValue(wdl);  // WIN, DRAW, LOSS
    }
};
```

**Configuration:**
```yaml
USE_TABLEBASES: true
TB_PATH: "./syzygy/"
TB_PROBE_DEPTH: 1
TB_PROBE_LIMIT: 6               # 6-piece TBs
TB_CACHE_MB: 32
```

**Testing:**
- Standard TB test suites (Lomonosov, Nalimov converted)
- Verify perfect play in 3-6 piece endgames
- Benchmark lookup overhead (should be minimal)

**Expected Impact:** +35-60 ELO (significant in endgames)

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

| Phase | Version | Tasks | Completed | In Progress | Planned | ELO Gain Target | ELO Gain Actual | Status |
|-------|---------|-------|-----------|-------------|---------|-----------------|-----------------|--------|
| 1 | v1.1 → v1.2 | 5 | 0 | 0 | 5 | +60-110 | TBD | 📋 Planned |
| 2 | v1.2 → v1.3 | 5 | 0 | 0 | 5 | +60-120 | TBD | 📋 Planned |
| 3 | v1.3 → v1.4 | 4 | 0 | 0 | 4 | +35-65 | TBD | 📋 Planned |
| 4 | v1.4 → v1.5 | 5 | 0 | 0 | 5 | +35-60 | TBD | 📋 Planned |
| 5 | v1.5 → v1.6 | 5 | 0 | 0 | 5 | +50-85 | TBD | 📋 Planned |
| 6 | v1.6 → v1.7 | 4 | 0 | 0 | 4 | +30-60 | TBD | 📋 Planned |
| 7 | v1.7 → v1.9 | 7 | 0 | 0 | 7 | +200-400 | TBD | 📋 Planned |
| 8 | v1.9 → v2.0 | 4 | 0 | 0 | 4 | +15-35 | TBD | 📋 Planned |
| **TOTAL** | **v1.1 → v2.0** | **39** | **0** | **0** | **39** | **+485-935** | **TBD** | 📋 **Planned** |

### Gantt Chart (Estimated Timeline)

```
Month 1 (Feb 2026):
  ├─ Phase 1: Quick Wins (2-3 weeks) ███████████░░░░
  └─ Phase 2: Multi-Threading (start) ░░░░░░░░░░███░

Month 2 (Mar 2026):
  ├─ Phase 2: Multi-Threading (complete) ███████░░░░░░░░░
  └─ Phase 3: Move Ordering (1-2 weeks) ░░░░░░░█████████

Month 3 (Apr 2026):
  ├─ Phase 4: Tablebases (2-3 weeks) ████████████░░░░
  └─ Phase 5: Advanced Search (start) ░░░░░░░░░░░░███

Month 4 (May 2026):
  ├─ Phase 5: Advanced Search (complete) ████████░░░░░░░░
  └─ Phase 6: Tuning Infrastructure ░░░░░░░████████░

Month 5-6 (Jun-Jul 2026):
  └─ Phase 7: NNUE Implementation ████████████████████

Month 7 (Aug 2026):
  └─ Phase 8: Optimization & Release ████████████░░░░
```

**Total Timeline:** 6-7 months (February - August 2026)

---

## Testing & Validation Strategy

### 1. Unit Tests (Regression Prevention)
- Extend GoogleTest suite for each new feature
- Maintain 100% test pass rate across all platforms
- Use FRIEND_TEST for internal state validation
- CI/CD runs all tests on every commit

### 2. Tactical Test Suites (Tactical Strength)
| Suite | Positions | Focus | Baseline | Target |
|-------|-----------|-------|----------|--------|
| **WAC** | 300 | Tactics | 250/300 | 285/300 |
| **STS** | 1500 | Strategy | 60% | 75% |
| **Arasan** | 2400 | Comprehensive | 55% | 70% |
| **Mate in N** | 100 | Mating | 65/100 | 90/100 |

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
| Metric | Baseline (v1.1) | Target (v2.0) |
|--------|-----------------|---------------|
| NPS (1 thread) | 1.5M | 2.0M |
| NPS (8 threads) | N/A | 8-10M |
| Depth (10s search) | 10-12 ply | 14-16 ply |

### 5. Regression Testing (Stability)
- Maintain EPD test suite with expected results
- Automated CI/CD testing on every commit
- Fail builds on significant strength regression (> -20 ELO)

---

## Risk Assessment

### Technical Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| **Search Instability (Lazy SMP)** | High | Medium | Extensive testing with 1 thread (deterministic), gradual rollout |
| **NNUE Training Failure** | High | Medium | Start with existing trained nets, fallback to classical always available |
| **Performance Regression** | Medium | Low | Continuous benchmarking in CI/CD, automated regression detection |
| **TT Contention (SMP)** | Medium | Medium | Proper TT sizing (128 MB per thread), lock-free atomic ops |
| **CPU Compatibility (PEXT)** | Low | High | Software fallback implementation, runtime detection |

### Project Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| **Scope Creep** | Medium | Medium | Strict phase boundaries, no mixing of tasks |
| **Time Overruns** | Low | Medium | Conservative effort estimates, prioritize high-impact items first |
| **Loss of Motivation** | Medium | Low | Celebrate milestones, track progress visibly, focus on measurable gains |
| **NNUE Complexity** | High | Medium | Break into small tasks, use existing libraries/frameworks, allow 10 weeks |

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
| Engine | Language | Notable Features | GitHub |
|--------|----------|------------------|--------|
| **Stockfish** | C++17 | NNUE, Lazy SMP, reference implementation | [official-stockfish](https://github.com/official-stockfish/Stockfish) |
| **Ethereal** | C | Clean classical eval, tuning infrastructure | [AndyGrant/Ethereal](https://github.com/AndyGrant/Ethereal) |
| **Koivisto** | C++ | Modern C++, NNUE training pipeline | [Luecx/Koivisto](https://github.com/Luecx/Koivisto) |
| **Berserk** | C | Recent NNUE implementation, good docs | [jhonnold/berserk](https://github.com/jhonnold/berserk) |
| **RubiChess** | C++ | Clean SMP implementation | [Matthies/RubiChess](https://github.com/Matthies/RubiChess) |

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

| Phase | Version | Timeframe | ELO Gain (Conservative) | ELO Gain (Optimistic) | Cumulative Total |
|-------|---------|-----------|-------------------------|-----------------------|------------------|
| 1 | v1.1 → v1.2 | Feb 2026 | +60 | +110 | 60-110 |
| 2 | v1.2 → v1.3 | Mar 2026 | +60 | +120 | 120-230 |
| 3 | v1.3 → v1.4 | Mar-Apr 2026 | +35 | +65 | 155-295 |
| 4 | v1.4 → v1.5 | Apr 2026 | +35 | +60 | 190-355 |
| 5 | v1.5 → v1.6 | May 2026 | +50 | +85 | 240-440 |
| 6 | v1.6 → v1.7 | May-Jun 2026 | +30 | +60 | 270-500 |
| 7 | v1.7 → v1.9 | Jun-Jul 2026 | +200 | +400 | 470-900 |
| 8 | v1.9 → v2.0 | Aug 2026 | +15 | +35 | 485-935 |

**Final Target Strength:**
- v1.1 baseline: ~2400-2450 ELO
- v2.0 target: ~2885-2935 ELO (conservative) to 2900-3000+ (optimistic)

### Configuration File Changes

Each phase adds new YAML parameters:

**Phase 1 (v1.2):**
```yaml
# config/search.yaml additions
USE_SINGULAR_EXTENSIONS: true
SINGULAR_MARGIN: 50
SINGULAR_MIN_DEPTH: 8
USE_CHECK_EXTENSIONS: true
CHECK_EXT_MAX_REPLIES: 2
USE_COUNTER_MOVE_HISTORY: true
```

**Phase 2 (v1.3):**
```yaml
# config/search.yaml additions
USE_LAZY_SMP: true
SMP_THREADS: 0
SMP_MIN_SPLIT_DEPTH: 4
```

**Phase 4 (v1.5):**
```yaml
# config/engine.yaml additions
USE_TABLEBASES: true
TB_PATH: "./syzygy/"
TB_PROBE_DEPTH: 1
TB_PROBE_LIMIT: 6
```

**Phase 7 (v1.9):**
```yaml
# config/eval.yaml additions
USE_NNUE: true
NNUE_NET_PATH: "./networks/nn-epoch99.nnue"
NNUE_FALLBACK_CLASSICAL: true
```

---

**Document Status:** Active Planning  
**Maintainer:** Frank Kopp  
**Created:** 2026-02-01  
**Last Updated:** 2026-02-01  
**Next Review:** After Phase 1 completion (v1.2 release)

---

## Change Log

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 2.0 | 2026-02-01 | Complete rewrite with detailed phase planning, ELO estimates, implementation specs | Frank Kopp |
| 1.0 | 2026-02-01 | Initial V1_ENGINE_ENHANCEMENT_PLAN.md | Frank Kopp |

---

*This document is a living roadmap and will be updated as implementation progresses. Each phase completion triggers a review and potential adjustment of subsequent phases based on actual results.*
