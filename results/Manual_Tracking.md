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

---
