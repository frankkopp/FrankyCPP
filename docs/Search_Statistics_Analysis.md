# FrankyCPP Search Statistics Analysis Guide

## Overview

This document provides a comprehensive analysis framework for evaluating FrankyCPP's search statistics. Use this guide to assess whether search features are working correctly, effectively, and efficiently.

---

## How to Generate a Search Statistics Report

### Step 1: Run the Search Test

Run the `SearchTreeSizeTest` or any search test that outputs detailed statistics:

```powershell
# Run the specific test
.\cmake-build-win-release\test\FrankyCPP_v1.5_Test.exe --gtest_filter=SearchTreeSizeTest.searchTreeSizeTest
```

Or trigger a timed search from UCI and use the `detailedstats` command.

### Step 2: Collect the Statistics Output

The test outputs a detailed statistics block like:

```
==================== Search Results ====================
Position       : [FEN string]
Best Move      : [move]
Score          : [evaluation]
Depth          : X/Y (regular/selective)
Nodes          : N
NPS            : N
EBF            : X.XX
...
========================================================
```

### Step 3: Request Analysis

Use this prompt template with an AI assistant:

```
Do an in-depth analysis of the search statistics from the search of this test.
Compare to other engines where such information might be available.
The goal is to understand if the search features implemented are:
- working correctly
- effective
- efficient

Provide a comprehensive report.

[Paste the full statistics output here]
```

---

## Reference Analysis: Complex Middlegame Position

### Test Position

```
5k2/1rn2p2/3pb1p1/7p/p3PP2/PnNBK2P/3N2P1/1R6 w - - 0 1
```

A complex middlegame with unbalanced material (bishop pair vs knights), open files, passed pawns, and king safety considerations.

### Search Results Summary

| Metric | Value      | Assessment                                     |
|--------|------------|------------------------------------------------|
| Depth  | 22/35      | Good nominal, excellent selective              |
| Nodes  | 69,795,942 |                                                |
| Time   | 9,993 ms   |                                                |
| NPS    | 6,984,483  | Good for classical eval                        |
| EBF    | 2.27       | Very good (competitive with classical engines) |
| Score  | +98 cp     | Reasonable for the position                    |

---

## Analysis Framework

### 1. Search Depth & Node Distribution

#### Key Metrics
- **Nominal depth**: Main search depth reached
- **Selective depth**: Maximum depth with extensions
- **Search nodes %**: Portion of nodes in main search
- **QSearch nodes %**: Portion of nodes in quiescence search

#### Benchmarks

| Engine Type            | Typical NPS | Notes            |
|------------------------|-------------|------------------|
| Stockfish (NNUE)       | 15-25M      | Highly optimized |
| Classical eval engines | 8-15M       | Ethereal, Laser  |
| **FrankyCPP**          | **7-10M**   | Competitive      |

#### QSearch Ratio Guidelines
- **50-65%**: Excellent - tight tactical pruning
- **65-75%**: Good - typical for complex positions
- **>75%**: Investigate - may need tighter QSearch pruning

---

### 2. Move Ordering Quality

#### Beta Cutoff Distribution

The percentage of cutoffs caused by the first move tried is the best indicator of move ordering quality.

| First Move Cutoff % | Assessment                       |
|---------------------|----------------------------------|
| 90%+                | Elite (Stockfish-level)          |
| 85-90%              | Excellent                        |
| 80-85%              | Good                             |
| 70-80%              | Needs improvement                |
| <70%                | Poor - investigate move ordering |

#### Reference (FrankyCPP)
```
Move 0: 86.85% ← Target: >85%
Move 1: 8.58%
Move 2: 2.11%
Move 3+: 2.46%
```

**Assessment**: 86.85% first-move cutoffs indicates excellent move ordering.

---

### 3. Pruning Effectiveness

#### 3.1 Late Move Reductions (LMR)

| Metric         | Good Range | Notes                        |
|----------------|------------|------------------------------|
| Re-search rate | 0.5-2%     | Lower = better move ordering |
| CutNode %      | 50-70%     | Aggressive at cut nodes      |

**Reference**: 1.08% re-search rate is excellent.

#### 3.2 Null Move Pruning (NMP)

| Metric            | Expected    |
|-------------------|-------------|
| Verification rate | <1%         | Low = well-tuned conditions |
| Cut contribution  | Significant | Should be major pruning source |

#### 3.3 Futility Pruning

| Type     | Purpose                   |
|----------|---------------------------|
| FP       | Shallow depth pruning     |
| RFP      | Reverse futility (deeper) |
| QFP      | Quiescence futility       |
| Razoring | Very shallow pruning      |

High pruning counts relative to node count indicates aggressive but hopefully accurate pruning.

#### 3.4 Late Move Pruning (LMP)

Should prune significant nodes. Typical: LMP cuts > LMR reductions.

---

### 4. Extension Analysis

#### Singular Extensions

| Metric         | Good Range                            |
|----------------|---------------------------------------|
| Extension rate | 20-40% of singular searches           |
| Filter rate    | 30-50% (pre-filtering bad candidates) |

**Reference**: 30.5% extension rate is within expected range.

#### Check Extensions

Should be active and contribute to selective depth.

---

### 5. Transposition Table Effectiveness

#### Hit Rate & Quality

| Metric           | Good Range | Notes                          |
|------------------|------------|--------------------------------|
| Hit rate         | 40-50%     | Position dependent             |
| Sufficient depth | >60%       | Higher = better                |
| TT fill          | 50-90%     | Should utilize available space |

#### TT Usage Breakdown

| Usage Type    | Good % of Hits |
|---------------|----------------|
| Cutoffs       | 25-35%         |
| Eval reuse    | 50-70%         |
| Move ordering | 10-20%         |

**Reference**:
- 46.1% hit rate ✓
- 65.6% sufficient depth ✓
- 29.7% cutoff rate ✓

---

### 6. Pawn TT Performance

| Metric    | Expected    |
|-----------|-------------|
| Hit rate  | >95%        | Pawn structure changes slowly |
| Fill rate | Low (1-10%) | Limited unique structures |

**Reference**: 98% hit rate is excellent.

---

### 7. Effective Branching Factor (EBF)

**Formula**: `EBF = nodes^(1/depth)`

| EBF     | Assessment                      |
|---------|---------------------------------|
| 1.8-2.2 | Elite (Stockfish with NNUE)     |
| 2.0-2.5 | Good (strong classical engines) |
| 2.5-3.0 | Average                         |
| >3.0    | Poor - needs optimization       |

**Reference**: EBF 2.27 is very good for classical evaluation.

---

### 8. Improving Heuristic

| Ratio            | Assessment                    |
|------------------|-------------------------------|
| 55-65% improving | Balanced, healthy             |
| >70% improving   | May indicate eval instability |
| <45% improving   | Position may be declining     |

**Reference**: 59% improving is healthy.

---

## Common Issues & Diagnostics

### Issue: High QSearch Ratio (>75%)

**Possible causes**:
1. Position is tactically sharp (acceptable)
2. QFP margins too loose
3. SEE thresholds need tuning
4. Delta pruning not aggressive enough

**Diagnostic**: Compare across multiple positions.

### Issue: Low First-Move Cutoff Rate (<80%)

**Possible causes**:
1. TT move not being used properly
2. Killer moves not effective
3. History heuristics need tuning
4. MVV-LVA ordering issues

**Diagnostic**: Check TT move usage stats.

### Issue: High LMR Re-search Rate (>3%)

**Possible causes**:
1. Reduction formula too aggressive
2. History-based adjustments not working
3. Move ordering issues at reduced depth

**Diagnostic**: Review LMR formula and history integration.

### Issue: Low TT Hit Rate (<35%)

**Possible causes**:
1. TT too small for search depth
2. Replacement scheme issues
3. Hash collision problems

**Diagnostic**: Increase TT size, check collision stats.

---

## Engine Comparison Reference

### NPS (Single-threaded)

| Engine        | NPS       | Eval Type     |
|---------------|-----------|---------------|
| Stockfish 16  | 15-25M    | NNUE          |
| Ethereal      | 10-15M    | Classical     |
| Laser         | 8-12M     | Classical     |
| Demolito      | 6-10M     | Classical     |
| Weiss         | 8-12M     | Classical     |
| **FrankyCPP** | **7-10M** | **Classical** |

### First-Move Cutoff Rate

| Engine        | Rate       |
|---------------|------------|
| Stockfish     | 90-92%     |
| Ethereal      | 87-90%     |
| **FrankyCPP** | **86-88%** |

### LMR Re-search Rate

| Engine        | Rate     |
|---------------|----------|
| Stockfish     | 0.5-1.5% |
| Ethereal      | 1-2%     |
| **FrankyCPP** | **1-2%** |

---

## Full Reference Statistics

Below is a complete statistics output from the reference position for comparison:

```
==================== Search Results ====================
Position       : 5k2/1rn2p2/3pb1p1/7p/p3PP2/PnNBK2P/3N2P1/1R6 w - - 0 1
Best Move      : c3a4
Score          : cp 98
Ponder Move    : b7a7
Depth          : 22/35 (regular/selective)
Time           : 9.993 ms
Nodes          : 69.795.942
NPS            : 6.984.483
EBF            : 2,27
Book Move      : no
TB Hit         : no
Mate Found     : no
PV             : c3a4 b7a7 d2b3 a7a4 b3d4 a4a3 b1c1 a3a7 d4c6 a7b7 f4f5 g6f5 e4f5 e6d5 d3e4 d5e4 e3e4 d6d5 e4f4 f7f6 c6d4 f8e7

------------------- Terminal Nodes --------------------
Checkmates     : 8.987
Stalemates     : 67.077
Leaf Positions : 32.723.793
Evaluations    : 32.723.793
Perft Nodes    : 19

------------------- Node Type Stats -------------------
PV Nodes       : 103.468 (0,15%)
Non-PV Nodes   : 70.900.685 (99,85%)
Search Nodes   : 19.743.381 (27,81%)
QSearch Nodes  : 51.260.772 (72,19%)

------------------- Pruning Stats ---------------------
Beta Cuts      : 16.838.730
MDP Cuts       : 0
Razorings      : 465.775
RFP Cuts       : 2.508.606
NMP Cuts       : 1.712.218
NMP Verifies   : 253
FP Prunings    : 40.044.975
QFP Prunings   : 5.110.610
Standpat Cuts  : 24.225.196

------------------- LMR/LMP Stats ---------------------
LMR Reductions : 17.876.083
LMR Researches : 193.649
LMR CutNode    : 10.827.987 (60,6% of LMR)
LMR Hist Less  : 472.061 (2,6% of LMR)
LMR Hist Saved : 18.885.528 plies (avg 40,01 per move)
LMP Cuts       : 26.447.625

------------------- Improving Stats -------------------
Improving True : 8.654.360 (58,9%)
Improving False: 6.038.383

------------------- Extension Stats -------------------
Check Ext      : 1.397.920
Threat Ext     : 8
Singular Srch  : 103.149
Singular Filt  : 43.169
Singular Ext   : 31.459

------------------- TT Stats --------------------------
TT Size (MB)   : 512
TT Max Entries : 33.554.432
TT Entries     : 20.964.255
TT Fill        : 62,48%
TT Puts        : 55.150.544 (approx)
TT Updates     : 33.007.808 (approx)
TT Collisions  : 2.061.905 (approx)
TT Overwrites  : 2.061.905 (approx)
--- Probe Stats (from SearchStats - accurate) ---
TT Probes      : 71.004.074
TT Hits        : 32.743.529 (46,1%)
TT Misses      : 38.260.545 (53,9%)
--- Hit Quality (Depth) ---
Sufficient Dep : 21.477.327 (65,6%)
Insuffic. Dep  : 11.266.202 (34,4%)
--- Hit Quality (Bound) ---
NONE Hits      : 15.531.741 (47,4%) [eval-only]
EXACT Hits     : 36.222 (0,1%)
ALPHA Hits     : 6.529.023 (19,9%)
BETA Hits      : 10.646.543 (32,5%)
--- TT Effectiveness ---
TT Cuts        : 9.715.259
  Search Cuts  : 3.313.188 (avg depth 2,5)
  Qsearch Cuts : 6.402.071
TT No Cuts     : 315.234
TT Move Used   : 5.064.010 (2.860.404 = 56,5% best move)
No TT Move     : 27.313.010
Eval from TT   : 21.984.051
--- Value Breakdown (% of hits, overlapping) ---
  Cutoffs      : 9.715.259 (29,7%)
  Eval Reused  : 21.984.051 (67,1%)
  Move Ordering: 5.064.010 (15,5%)

----------------- PawnTT Stats ------------------------
PTT Size (MB)  : 16
PTT Max Entries: 1.048.576
PTT Entries    : 199.287
PTT Fill       : 19,01%
PTT Puts       : 337.557
PTT Updates    : 69.441
PTT Collisions : 72.135
PTT Hits       : 27.204.567 (98%)
PTT Misses     : 323.445 (1%)

------------------- IID/IIR Stats ---------------------
IIR Reductions : 256.276

------------------- Re-search Stats -------------------
Root PVS Re    : 27
PVS Researches : 15.214
ASP Researches : 4
Best Move Chg  : 89

------------------- Tablebase Stats -------------------
TB Root Hits   : 0
TB Search Prbs : 0
TB Search Hits : 0
TB Search Miss : 0
TB Cutoffs     : 0

------------------- Beta Cuts Distribution ------------
(Shows which move index caused cutoff - lower index = better ordering)
  Move  0    :  86,87% (14.627.927)
  Move  1    :   8,52% (1.435.137)
  Move  2    :   2,10% (353.895)
  Move  3    :   0,75% (126.227)
  Move  4    :   0,50% (83.461)
  Move  5    :   0,38% (63.607)
  Move  6    :   0,28% (47.647)
  Move  7    :   0,21% (35.518)
  Move  8    :   0,11% (19.328)
  Move  9+   :   0,27% (45.983)
========================================================
```

---

## Summary Assessment (Reference Position)

| Feature          | Status     | Grade | Notes                                 |
|------------------|------------|-------|---------------------------------------|
| Move Ordering    | ✅ Working | A     | 86.87% first-move cutoffs             |
| LMR              | ✅ Working | A     | 1.08% re-search rate                  |
| LMP              | ✅ Working | A-    | High pruning count                    |
| NMP              | ✅ Working | A     | Low verification rate (0.01%)         |
| Futility Pruning | ✅ Working | A     | Aggressive and effective              |
| TT               | ✅ Working | A-    | 46% hit, 30% cutoff                   |
| Pawn TT          | ✅ Working | A+    | 98% hit rate                          |
| Singular Ext     | ✅ Working | B+    | 30.5% extension rate                  |
| Check Ext        | ✅ Working | A     | Active                                |
| Improving        | ✅ Working | A     | 59/41 split                           |
| IIR              | ✅ Working | A     | Active replacement for IID            |
| MDP              | ✅ Working | N/A   | 0 cuts expected (no mates)            |
| Threat Ext       | ⚠️ Check  | C     | Very low (8) - may be too restrictive |

**Overall Grade: A-** (Very Good)

FrankyCPP's search is competitive with established classical evaluation engines.

---

*Document created: 2026-03-06*
*Last updated: 2026-03-06*
