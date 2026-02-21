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
---
