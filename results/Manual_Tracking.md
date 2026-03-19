# Manual Tracking Results

## Triangular PV Table

### TEST SUITE SUMMARY (FrankyCPP v1.2 vs FrankyCPP v1.1)

| Metric               | Value                 |
|:---------------------|:----------------------|
| **Total positions**  | 2873                  |
| **Regression**       | -44 positions (-1.5%) |
| **Suites improved**  | 3                     |
| **Suites regressed** | 3                     |
| **Status**           | [X] **REGRESSION**    |

### MATCH COMPARISON: FrankyCPP v1.2 vs Baselines

| Opponent       | Games | Score |  W/D/L   | ELO | vs FrankyCPP.. |
|:---------------|:-----:|:-----:|:--------:|:---:|:---------------|
| FrankyCPP v1.1 |  104  | 51.9% | 31/46/27 | +13 | [baseline]     |

---

## Static Move List

### TEST SUITE SUMMARY (FrankyCPP v1.2 vs FrankyCPP v1.1)

| Metric               | Value                 |
|:---------------------|:----------------------|
| **Total positions**  | 2873                  |
| **Regression**       | -45 positions (-1.6%) |
| **Suites improved**  | 2                     |
| **Suites regressed** | 3                     |
| **Status**           | [X] **REGRESSION**    |

### MATCH COMPARISON: FrankyCPP v1.2 vs Baselines

| Opponent       | Games | Score |  W/D/L   | ELO | vs FrankyCPP.. |
|:---------------|:-----:|:-----:|:--------:|:---:|:---------------|
| FrankyCPP v1.1 |  104  | 47.6% | 29/41/34 | -17 | [baseline]     |

---

## USE_SINGULAR_EXT

### Arena Results (v1.2 vs v1.1)

*   **Match:** +27 ELO (53.8% score, 32W/48D/24L over 104 games) ✅
*   **Test suites:** -1.3% (within noise margin, expected trade-off)

### Arena Results (v1.2 vs v1.1 - Combined Singular + Check Extensions)

| Metric     | Result                 |
|:-----------|:-----------------------|
| Games      | 104                    |
| Score      | 58.2%                  |
| W/D/L      | 38/45/21               |
| ELO        | +57                    |
| Test Suite | -1.2% (2873 positions) |

---

## USE_BESTMOVE_INSTABILITY: false

```text
Match Complete: v1.1_vs_v1.2_blitz_100-dev
FrankyCPP v1.2: 146 wins, 169 draws, 101 losses
FrankyCPP v1.1: 101 wins, 169 draws, 146 losses
Score: 230.5 - 185.5
ELO Difference: +37.7
Duration: 30816.9s
```

---

## USE_BESTMOVE_INSTABILITY: true

```text
Match Complete: v1.1_vs_v1.2_blitz_100-dev
FrankyCPP v1.2: 125 wins, 173 draws, 118 losses
FrankyCPP v1.1: 118 wins, 173 draws, 125 losses
Score: 211.5 - 204.5
ELO Difference: +5.8
Duration: 30805.8s
```

---

## USE_BESTMOVE_INSTABILITY: true (with less aggressive settings)

*   **INSTABILITY_MIN_DEPTH:** 8
*   **INSTABILITY_STABLE_COUNT:** 5
*   **INSTABILITY_CHANGE_THRESHOLD:** 3
*   **INSTABILITY_STABLE_FACTOR:** 0.9
*   **INSTABILITY_EXTEND_FACTOR:** 1.25

```text
Match Complete: v1.1_vs_v1.2_blitz_100-dev
FrankyCPP v1.2: 131 wins, 186 draws, 99 losses
FrankyCPP v1.1: 99 wins, 186 draws, 131 losses
Score: 224 - 192
ELO Difference: +26.8
Duration: 1776.6s
```

---

## USE_QS_CHECKS: false

```text
Match Complete: v1.1_vs_v1.2_blitz_100-dev
FrankyCPP v1.2: 49 wins, 96 draws, 63 losses
FrankyCPP v1.1: 63 wins, 96 draws, 49 losses
Score: 97 - 111
ELO Difference: -23.4
Duration: 15365.9s
```

---

## USE_QS_CHECKS: true

```text
Match Complete: v1.1_vs_v1.2_blitz_100-dev
FrankyCPP v1.2: 54 wins, 100 draws, 54 losses
FrankyCPP v1.1: 54 wins, 100 draws, 54 losses
Score: 104 - 104
ELO Difference: -0.0
Duration: 15394.0s
```

---

## Tablebases

```text
Match Complete: v1.1_vs_v1.3_blitz_100-dev
FrankyCPP v1.3: 34 wins, 47 draws, 23 losses
FrankyCPP v1.1: 23 wins, 47 draws, 34 losses
Score: 57.5 - 46.5
ELO Difference: +36.9
Duration: 7709.4s
```

---

## Tablebases (Blitz 208)

```text
Match Complete: v1.1_vs_v1.3_blitz_208-dev
FrankyCPP v1.3: 69 wins, 79 draws, 60 losses
FrankyCPP v1.1: 60 wins, 79 draws, 69 losses
Score: 108.5 - 99.5
ELO Difference: +15.0
Duration: 15350.0s
```
## Tablebases (Blitz 208)
USE_TB_PROBE_PV = OFF
```text
Match Complete: v1.1_vs_v1.3_blitz_208-dev
FrankyCPP v1.3: 62 wins, 84 draws, 62 losses
FrankyCPP v1.1: 62 wins, 84 draws, 62 losses
Score: 104 - 104
ELO Difference: -0.0
Duration: 1779.2s
```

After LMR fix
```text
Match Complete: v1.1_vs_v1.3_blitz_208-dev-lmr
FrankyCPP v1.3: 77 wins, 92 draws, 39 losses
FrankyCPP v1.1: 39 wins, 92 draws, 77 losses
Score: 123 - 85
ELO Difference: +64.2
Duration: 15461.5s

TEST SUITE SUMMARY (FrankyCPP v1.3 vs FrankyCPP v1.1)

Total positions:      2873
Improvement:          +63 positions (+2.2%)
Suites improved:      6
Suites regressed:     0
Status:               [+] IMPROVEMENT

MATCH COMPARISON: FrankyCPP v1.3 vs Baselines
--------------------------------------------------------------------------------
Opponent                    Games     Score       W/D/L           ELO         vs FrankyCPP..
--------------------------------------------------------------------------------
FrankyCPP v1.1              208       59.1%       77/92/39        +64         [baseline]
================================================================================
```

```text
LMR log 22 factor 1.5
--------------------------------------------------------------------------------
TEST SUITE SUMMARY (FrankyCPP v1.3 vs FrankyCPP v1.1)
--------------------------------------------------------------------------------
Total positions:      2873
Improvement:          +43 positions (+1.5%)
Suites improved:      5
Suites regressed:     1
Status:               [!] MIXED (some suites regressed)
================================================================================

================================================================================
MATCH COMPARISON: FrankyCPP v1.3 vs Baselines
================================================================================
--------------------------------------------------------------------------------
Opponent                    Games     Score       W/D/L           ELO         vs FrankyCPP..
--------------------------------------------------------------------------------
FrankyCPP v1.1              208       61.5%       89/78/41        +82         [baseline]
================================================================================
```

```text
After isPVNode fix
--------------------------------------------------------------------------------
TEST SUITE SUMMARY (FrankyCPP v1.3 vs FrankyCPP v1.1)
--------------------------------------------------------------------------------
Total positions:      2873
Improvement:          +10 positions (+0.3%)
Suites improved:      3
Suites regressed:     3
Status:               [!] MIXED (some suites regressed)
================================================================================

================================================================================
MATCH COMPARISON: FrankyCPP v1.3 vs Baselines
================================================================================
--------------------------------------------------------------------------------
Opponent                    Games     Score       W/D/L           ELO         vs FrankyCPP..
--------------------------------------------------------------------------------
FrankyCPP v1.1              208       65.1%       92/87/29        +109        [baseline]
================================================================================
```

```
Improvement Flag
--------------------------------------------------------------------------------
TEST SUITE SUMMARY (FrankyCPP v1.4 vs FrankyCPP v1.3)
--------------------------------------------------------------------------------
Total positions:      2873
Improvement:          +59 positions (+2.1%)
Suites improved:      6
Suites regressed:     1
Status:               [!] MIXED (some suites regressed)
================================================================================

================================================================================
MATCH COMPARISON: FrankyCPP v1.4 vs Baselines
================================================================================
--------------------------------------------------------------------------------
Opponent                    Games     Score       W/D/L           ELO         vs FrankyCPP..
--------------------------------------------------------------------------------
FrankyCPP v1.3              208       59.6%       80/88/40        +68         [baseline]
================================================================================
```

```
LMR History-based Reductions
--------------------------------------------------------------------------------
TEST SUITE SUMMARY (FrankyCPP v1.4 vs FrankyCPP v1.3)
--------------------------------------------------------------------------------
  Total positions:      2873
  Improvement:          +32 positions (+1.1%)
  Suites improved:      5
  Suites regressed:     2
  Status:               [!] MIXED (some suites regressed)
================================================================================

================================================================================
MATCH COMPARISON: FrankyCPP v1.4 vs Baselines
================================================================================
--------------------------------------------------------------------------------
Opponent                    Games     Score       W/D/L           ELO         vs FrankyCPP..
--------------------------------------------------------------------------------
FrankyCPP v1.3              208       58.7%       79/86/43        +61         [baseline]
================================================================================
```

```
CutNode Reductions (includes LMR History)
--------------------------------------------------------------------------------
TEST SUITE SUMMARY (FrankyCPP v1.4 vs FrankyCPP v1.3)
--------------------------------------------------------------------------------
  Total positions:      2873
  Improvement:          +38 positions (+1.3%)
  Suites improved:      6
  Suites regressed:     1
  Status:               [!] MIXED (some suites regressed)
================================================================================

================================================================================
MATCH COMPARISON: FrankyCPP v1.4 vs Baselines
================================================================================
--------------------------------------------------------------------------------
Opponent                    Games     Score       W/D/L           ELO         vs FrankyCPP..
--------------------------------------------------------------------------------
FrankyCPP v1.3              104       58.2%       38/45/21        +57         [baseline]
================================================================================

**Summary:** CutNode reductions + LMR History combined show +57 ELO vs v1.3.
This is −11 ELO compared to Improving Flag baseline (+68 ELO), but within
measurement tolerance (±15 ELO). Features are functional but ELO-neutral.

**Implementation notes:**
- Refactored `isPvNode` + `cutNode` into unified `NodeType` enum (PvNode, CutNode, AllNode)
- SearchTreeSizeTest shows 1M+ cut node reductions applied, −4.4% nodes vs LMR+History alone
- LMR re-searches increased (111K vs 83K) due to more aggressive reductions
```

```
After Feature Review
--------------------------------------------------------------------------------
TEST SUITE SUMMARY (FrankyCPP v1.4 vs FrankyCPP v1.3)
--------------------------------------------------------------------------------
  Total positions:      2873
  Improvement:          +21 positions (+0.7%)
  Suites improved:      5
  Suites regressed:     2
  Status:               [!] MIXED (some suites regressed)
================================================================================

================================================================================
MATCH COMPARISON: FrankyCPP v1.4 vs Baselines
================================================================================
--------------------------------------------------------------------------------
Opponent                    Games     Score       W/D/L           ELO         vs FrankyCPP..
--------------------------------------------------------------------------------
FrankyCPP v1.3              208       62.3%       95/69/44        +87         [baseline]
================================================================================

Search Feature Correctness Review Summary (v1.4):
- All 24 search features reviewed for correctness
- Key fixes: RFP/FP improving logic, LMR scope expansion, IIR replacement for IID
- Enhancements: Check extension SEE filter, Threat extension configurable depth
- Time Management fix: Added MAX_EXTRA_TIME_FACTOR cap (2.0) to prevent unbounded extensions
- Result: +87 ELO vs v1.3 (208 games, 62.3% score)
- Review document: docs/specs/PLAN_Search_Correctness_Review.md
```

```
After SMP (second approach incl. iterative deepening fix)
==================================================================
Match Complete: v1.4_vs_v1.3_blitz_208_smp_v2
  FrankyCPP v1.4: 103 wins, 71 draws, 34 losses
  FrankyCPP v1.3: 34 wins, 71 draws, 103 losses
  Score: 138.5 - 69.5
  ELO Difference: +119.8
  Duration: 11898.8s
==================================================================

===================================================================
All Matches Complete
===================================================================
  v1.4_vs_v1.3_blitz_208_smp_v2: 138.5 - 69.5 (ELO: +119.8)
===================================================================

Saving match results...
  Saved: ./results/matches/FrankyCPP_v1.4-v1.4_vs_FrankyCPP_v1.3-v1.3_300_0__20260303_165311.json

===================================================================
Matches Complete
===================================================================
```

```
--------------------------------------------------------------------------------
TEST SUITE SUMMARY (FrankyCPP v1.5 vs FrankyCPP v1.3)
--------------------------------------------------------------------------------
  Total positions:      2874
  Improvement:          +68 positions (+2.4%)
  Suites improved:      6
  Suites regressed:     1
  Status:               [!] MIXED (some suites regressed)
================================================================================
==================================================================
Match Complete: v1.5_vs_v1.3_300s_TTbuckets
  FrankyCPP v1.5: 69 wins, 24 draws, 11 losses
  FrankyCPP v1.3: 11 wins, 24 draws, 69 losses
  Score: 81.0 - 23.0
  ELO Difference: +218.7
  Duration: 15521.3s
==================================================================
```

```
--------------------------------------------------------------------------------
TEST SUITE SUMMARY (FrankyCPP v1.5 vs FrankyCPP v1.4)
--------------------------------------------------------------------------------
  Total positions:      2874
  Improvement:          +27 positions (+0.9%)
  Suites improved:      4
  Suites regressed:     3
  Status:               [!] MIXED (some suites regressed)
================================================================================
==================================================================
Match Complete: v1.5_vs_v1.4_300s_TTbuckets
  FrankyCPP v1.5: 42 wins, 42 draws, 20 losses
  FrankyCPP v1.4: 20 wins, 42 draws, 42 losses
  Score: 63 - 41
  ELO Difference: +74.6
  Duration: 15533.0s
==================================================================
```

Re-run after Arena Improvements in v1.5
```
===================================================================
Test Suites (2026-03-08 14:22):
  STS1-STS15_LAN:      780/1500  (52.00 %)
  crafty_test:         166/346   (47.98 %)
  ecm98:               543/769   (70.61 %)
  franky_tests:         13/13    (100.00%)
  kaufman:              19/25    (76.00 %)
  mate_test_suite:      16/20    (80.00 %)
  wac:                 194/201   (96.52 %)
-------------------------------------------------------------------
  TOTAL:              1731/2874  (60.23 %)
  Total Nodes:      82,793,955,345
  Total Time:       3h 16m 47s
===================================================================
==================================================================
Match Complete: v1.5_vs_v1.4_300s
  FrankyCPP v1.5: 44 wins, 38 draws, 18 losses
  FrankyCPP v1.4: 18 wins, 38 draws, 44 losses
  Score: 63 - 37
  ELO Difference: +92.5
  Duration: 14884.7s
==================================================================
==================================================================
Match Complete: v1.5_vs_v1.3_300s
  FrankyCPP v1.5: 62 wins, 27 draws, 11 losses
  FrankyCPP v1.3: 11 wins, 27 draws, 62 losses
  Score: 75.5 - 24.5
  ELO Difference: +195.5
  Duration: 14930.6s
==================================================================
==================================================================
Match Complete: v1.5_vs_FrankyGo_300s
  FrankyCPP v1.5: 72 wins, 17 draws, 11 losses
  FrankyGo v1.0.3 (4.6.2021): 11 wins, 17 draws, 72 losses
  Score: 80.5 - 19.5
  ELO Difference: +246.3
  Duration: 14754.0s
==================================================================
==================================================================
Stockfish set to 2200 ELO
Match Complete: v1.5_vs_Stockfish18_2200_300s
  FrankyCPP v1.5: 80 wins, 4 draws, 16 losses
  Stockfish 18: 16 wins, 4 draws, 80 losses
  Score: 82.0 - 18.0
  ELO Difference: +263.4
  Duration: 14622.2s
==================================================================

===================================================================
All Matches Complete
===================================================================
  v1.5_vs_v1.4_300s: 63.0 - 37.0 (ELO: +92.5)
  v1.5_vs_v1.3_300s: 75.5 - 24.5 (ELO: +195.5)
  v1.5_vs_FrankyGo_300s: 80.5 - 19.5 (ELO: +246.3)
  v1.5_vs_Stockfish18_2200_300s: 82.0 - 18.0 (ELO: +263.4)
===================================================================
```

```
Reference Test Stockfish 18 2700ELO
==================================================================
Match Complete: v1.5_vs_Stockfish18_2700_300s
  FrankyCPP v1.5: 39 wins, 17 draws, 44 losses
  Stockfish 18: 44 wins, 17 draws, 39 losses
  Score: 47.5 - 52.5
  ELO Difference: -17.4
  Duration: 14996.8s
==================================================================
```

Best Thread Selection improvement (v1.5)
```
===================================================================
Engine Summary: FrankyCPP v1.5

===================================================================
Historical Test Suite Runs:
-------------------------------------------------------------------
  [2026-03-10] 1747/2875  ( 60,8%)  (7 suites)  [bestThread]
  [2026-03-08] 1731/2874  ( 60,2%)  (7 suites)  [baseAfterTtBuckets]
  [2026-03-06] 1759/2874  ( 61,2%)  (7 suites)
===================================================================

===================================================================
Benchmarks:
  [2026-03-10]    7.001.492 NPS  (d12, 128MB, 4T)  [bestThread]
  [2026-03-08]    6.956.053 NPS  (d12, 128MB, 4T)  [baseAfterTtBuckets]
===================================================================

==================================================================
Match Complete: v1.5_vs_v1.4_300s
  FrankyCPP v1.5: 46 wins, 37 draws, 17 losses
  FrankyCPP v1.4: 17 wins, 37 draws, 46 losses
  Score: 64.5 - 35.5
  ELO Difference: +103.7
  Duration: 14924.8s
==================================================================
==================================================================
Match Complete: v1.5_vs_Stockfish18_2700_300s
  FrankyCPP v1.5: 49 wins, 4 draws, 47 losses
  Stockfish 18: 47 wins, 4 draws, 49 losses
  Score: 51.0 - 49.0
  ELO Difference: +6.9
  Duration: 14980.1s
==================================================================
===================================================================
All Matches Complete
===================================================================
  v1.5_vs_v1.4_300s: 64.5 - 35.5 (ELO: +103.7)
  v1.5_vs_Stockfish18_2700_300s: 51.0 - 49.0 (ELO: +6.9)
===================================================================

```

```
EPD File:   D:/_DEV/FrankyCPP/test/testsets/STS1-STS15_LAN.EPD
SearchTime: 5,000 s
MaxDepth:   0
Date:       2026-03-17 10:13:31
Successful: 870 (58 %)
Failed:     630 (42 %)
Skipped:    0   (0 %)
Not tested: 0   (0 %)
Test time:  1h:50m:6s:595.913.700ns
```

```
v1.6.0 Eval and Strength Improvement Phase 1

===================================================================
All Test Suites Complete
===================================================================
  franky_tests (FrankyCPP v1.6.0 v1.6): 13/13 passed (100%)
  mate_test_suite (FrankyCPP v1.6.0 v1.6): 16/20 passed (80%)
  wac (FrankyCPP v1.6.0 v1.6): 192/201 passed (95.5224%)
  STS1-STS15_LAN (FrankyCPP v1.6.0 v1.6): 858/1500 passed (57.2%)
  crafty_test (FrankyCPP v1.6.0 v1.6): 181/347 passed (52.1614%)
  ecm98 (FrankyCPP v1.6.0 v1.6): 559/769 passed (72.6918%)
  kaufman (FrankyCPP v1.6.0 v1.6): 21/25 passed (84%)
  eigenmann-rapid-engine (FrankyCPP v1.6.0 v1.6): 12/109 passed (11.0092%)
-------------------------------------------------------------------
  TOTAL: 1852/2984 passed (62.0643%)
  Total Nodes: 79124050606
  Total Time:  12355963ms
===================================================================

Benchmarks:
  [2026-03-18]    6.614.192 NPS  (d12, 128MB, 4T)  [Phase 1 Eval Improvement]

==================================================================
Match Complete: v1.6_vs_v1.5_300s
  FrankyCPP v1.6.0: 48 wins, 27 draws, 25 losses
  FrankyCPP v1.5: 25 wins, 27 draws, 48 losses
  Score: 61.5 - 38.5
  ELO Difference: +81.4
  Duration: 14977.5s
==================================================================
==================================================================
Match Complete: v1.6_vs_Stockfish18_2700_300s
  FrankyCPP v1.6.0: 50 wins, 16 draws, 34 losses
  Stockfish 18: 34 wins, 16 draws, 50 losses
  Score: 58.0 - 42.0
  ELO Difference: +56.1
  Duration: 15006.7s
==================================================================
===================================================================
All Matches Complete
===================================================================
  v1.6_vs_v1.5_300s: 61.5 - 38.5 (ELO: +81.4)
  v1.6_vs_Stockfish18_2700_300s: 58.0 - 42.0 (ELO: +56.1)
===================================================================
```

```
v1.6.0 Eval and Strength Improvement Phase 2

===================================================================
All Test Suites Complete
===================================================================
  franky_tests (FrankyCPP v1.6.0 v1.6): 13/13 passed (100%)
  mate_test_suite (FrankyCPP v1.6.0 v1.6): 18/20 passed (90%)
  wac (FrankyCPP v1.6.0 v1.6): 191/201 passed (95.0249%)
  STS1-STS15_LAN (FrankyCPP v1.6.0 v1.6): 847/1500 passed (56.4667%)
  crafty_test (FrankyCPP v1.6.0 v1.6): 170/347 passed (48.9914%)
  ecm98 (FrankyCPP v1.6.0 v1.6): 568/769 passed (73.8622%)
  kaufman (FrankyCPP v1.6.0 v1.6): 20/25 passed (80%)
  eigenmann-rapid-engine (FrankyCPP v1.6.0 v1.6): 9/109 passed (8.25688%)
-------------------------------------------------------------------
  TOTAL: 1836/2984 passed (61.5282%)
  Total Nodes: 78889604230
  Total Time:  12263816ms
===================================================================

==================================================================
Match Complete: v1.6_vs_v1.5_300s
  FrankyCPP v1.6.0: 45 wins, 26 draws, 29 losses
  FrankyCPP v1.5: 29 wins, 26 draws, 45 losses
  Score: 58 - 42
  ELO Difference: +56.1
  Duration: 14952.7s
==================================================================
==================================================================
Match Complete: v1.6_vs_Stockfish18_2700_300s
  FrankyCPP v1.6.0: 40 wins, 15 draws, 45 losses
  Stockfish 18: 45 wins, 15 draws, 40 losses
  Score: 47.5 - 52.5
  ELO Difference: -17.4
  Duration: 15004.9s
==================================================================
```

```
v1.6.0 Eval and Strength Improvement Phase 2 v2

Benchmarks:
  [2026-03-18]    6.614.192 NPS  (d12, 128MB, 4T)  [Phase 1 Eval Improvement]
  [2026-03-19]    6.795.488 NPS  (d12, 128MB, 4T)  [Phase 2 v2 Eval Improvement]
  
===================================================================
All Test Suites Complete
===================================================================
  franky_tests (FrankyCPP v1.6.0 v1.6): 13/13 passed (100%)
  mate_test_suite (FrankyCPP v1.6.0 v1.6): 17/20 passed (85%)
  wac (FrankyCPP v1.6.0 v1.6): 193/201 passed (96.0199%)
  STS1-STS15_LAN (FrankyCPP v1.6.0 v1.6): 883/1500 passed (58.8667%)
  crafty_test (FrankyCPP v1.6.0 v1.6): 182/347 passed (52.4496%)
  ecm98 (FrankyCPP v1.6.0 v1.6): 560/769 passed (72.8218%)
  kaufman (FrankyCPP v1.6.0 v1.6): 18/25 passed (72%)
  eigenmann-rapid-engine (FrankyCPP v1.6.0 v1.6): 12/109 passed (11.0092%)
-------------------------------------------------------------------
  TOTAL: 1878/2984 passed (62.9357%)
  Total Nodes: 76392451181
  Total Time:  12293653ms  
  
==================================================================
Match Complete: v1.6_vs_v1.5_300s
  FrankyCPP v1.6.0: 47 wins, 27 draws, 26 losses
  FrankyCPP v1.5: 26 wins, 27 draws, 47 losses
  Score: 60.5 - 39.5
  ELO Difference: +74.1
  Duration: 14943.6s
==================================================================  
==================================================================
Match Complete: v1.6_vs_Stockfish18_2700_300s
  FrankyCPP v1.6.0: 47 wins, 20 draws, 33 losses
  Stockfish 18: 33 wins, 20 draws, 47 losses
  Score: 57.0 - 43.0
  ELO Difference: +49.0
  Duration: 15000.8s
==================================================================

===================================================================
All Matches Complete
===================================================================
  v1.6_vs_v1.5_300s: 60.5 - 39.5 (ELO: +74.1)
  v1.6_vs_Stockfish18_2700_300s: 57.0 - 43.0 (ELO: +49.0)
===================================================================
```

---
