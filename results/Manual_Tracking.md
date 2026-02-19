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

---

## FrankyCPP v1.3 - Configuration Settings

| Name                         | Type   | Default       | Current       | Min  | Max  | UCI Name                         |
|:-----------------------------|:-------|:--------------|:--------------|:-----|:-----|:---------------------------------|
| **General**                  |        |               |               |      |      |                                  |
| MOVE_OVERHEAD_MS             | int    | 10            | 10            | 0    | 5000 | Move Overhead                    |
| USE_BOOK                     | bool   | true          | true          | -    | -    | OwnBook                          |
| BOOK_PATH                    | string | ./books/boo.. | ./books/boo.. | -    | -    | Book Path                        |
| BOOK_TYPE                    | string | SIMPLE        | SIMPLE        | -    | -    | Book Format                      |
| USE_PONDER                   | bool   | true          | true          | -    | -    | Ponder                           |
| TB_PATH                      | string |               | D:/SYZYGY     | -    | -    | SyzygyPath                       |
| **Search**                   |        |               |               |      |      |                                  |
| USE_ALPHABETA                | bool   | true          | true          | -    | -    | Use AlphaBeta                    |
| USE_PVS                      | bool   | true          | true          | -    | -    | Use Pvs                          |
| USE_ASP                      | bool   | true          | true          | -    | -    | Use Aspiration                   |
| USE_QUIESCENCE               | bool   | true          | true          | -    | -    | Use Quiescence                   |
| USE_TT                       | bool   | true          | true          | -    | -    | Use Hash                         |
| USE_TT_VALUE                 | bool   | true          | true          | -    | -    | Use Hash Value                   |
| USE_EVAL_TT                  | bool   | true          | true          | -    | -    | Use Eval TT                      |
| TT_SIZE_MB                   | int    | 64            | 64            | 0    | 4096 | Hash                             |
| USE_QS_TT                    | bool   | true          | true          | -    | -    | Use Hash Quiescence              |
| USE_TB_PROBE_ROOT            | bool   | true          | true          | -    | -    | Use Syzygy Probe Root            |
| TB_ROOT_IMMEDIATE            | bool   | false         | false         | -    | -    | Syzygy Root Immediate            |
| USE_TB_PROBE_SEARCH          | bool   | true          | true          | -    | -    | Use Syzygy Probe Search          |
| USE_TB_PROBE_PV              | bool   | true          | true          | -    | -    |                                  |
| TB_PROBE_DEPTH               | int    | 1             | 1             | 0    | 20   | Syzygy Probe Depth               |
| TB_PROBE_LIMIT               | int    | 6             | 6             | 3    | 7    | Syzygy Probe Limit               |
| TB_RULE50_THRESHOLD          | int    | 80            | 80            | 0    | 100  | Syzygy 50 Move Rule              |
| USE_TT_PV_MOVE_SORT          | bool   | true          | true          | -    | -    | Use TT Move as PvMove            |
| USE_KILLER_MOVES             | bool   | true          | true          | -    | -    | Use Killer Moves                 |
| USE_HISTORY_COUNTER          | bool   | true          | true          | -    | -    | Use History Counter              |
| USE_HISTORY_MOVES            | bool   | true          | true          | -    | -    | Use History Moves                |
| USE_IID                      | bool   | true          | true          | -    | -    | Use Internal Iterative Deepening |
| IID_DEPTH                    | int    | 6             | 6             | 1    | 20   | IID Move Depth                   |
| IID_REDUCTION                | int    | 2             | 2             | 1    | 10   | IID Depth Reduction              |
| USE_MDP                      | bool   | true          | true          | -    | -    | Use Mate Distance Pruning        |
| USE_QS_STANDPAT_CUT          | bool   | true          | true          | -    | -    | Use Quiescence Standpat          |
| USE_QS_SEE                   | bool   | true          | true          | -    | -    | Use Quiescence SEE               |
| USE_RAZORING                 | bool   | true          | true          | -    | -    | Use Razoring                     |
| RAZOR_MARGIN                 | int    | 531           | 531           | 0    | 1000 | Razor Margin                     |
| USE_RFP                      | bool   | true          | true          | -    | -    | Use Reverse Futility Pruning     |
| RFP_MARGIN                   | int[]  | 0,200,400,800 | 0,200,400,800 | -    | -    |                                  |
| USE_NMP                      | bool   | true          | true          | -    | -    | Use Null Move Pruning            |
| NMP_DEPTH                    | int    | 3             | 3             | 1    | 10   | Null Move Depth                  |
| NMP_REDUCTION                | int    | 2             | 2             | 1    | 6    | Null Depth Reduction             |
| USE_NMP_VERIFY               | bool   | true          | true          | -    | -    | Use Null Move Verification       |
| NMP_VERIFY_MIN_DEPTH         | int    | 6             | 6             | 1    | 20   | Null Move Verify Min Depth       |
| NMP_VERIFY_MARGIN            | int    | 2             | 2             | 0    | 10   | Null Move Verify Margin          |
| NMP_NEAR_MATE_MARGIN         | int    | 64            | 64            | 0    | 200  | Null Move Near Mate Margin       |
| USE_NMP_ZUG_GUARD            | bool   | true          | true          | -    | -    | Use Null Move Zugzwang Guard     |
| NMP_ZUG_NONPAWN_THRESHOLD    | int    | 0             | 0             | 0    | 10   | Null Move Zug NonPawn Threshold  |
| USE_FP                       | bool   | true          | true          | -    | -    | Use Futility Pruning             |
| USE_QFP                      | bool   | true          | true          | -    | -    | Use Quiescence Futility Pruning  |
| FP_MARGIN                    | int[]  | 0,100,200,3.. | 0,100,200,3.. | -    | -    |                                  |
| USE_LMR                      | bool   | true          | true          | -    | -    | Use Late Move Reduction          |
| LMR_MIN_DEPTH                | int    | 1             | 1             | 1    | 10   | LMR Min Depth                    |
| LMR_MIN_MOVES                | int    | 3             | 3             | 1    | 10   | LMR Min Moves                    |
| USE_LMP                      | bool   | true          | true          | -    | -    | Use Late Move Pruning            |
| LMP_MOVES                    | int[]  | 0,7,9,11,13.. | 0,7,9,11,13.. | -    | -    |                                  |
| USE_EXTENSIONS               | bool   | true          | true          | -    | -    | Use Extensions                   |
| USE_CHECK_EXT                | bool   | true          | true          | -    | -    | Use Check Extension              |
| CHECK_EXT_EARLY_LIMIT        | int    | 3             | 3             | 0    | 20   | Check Ext Early Limit            |
| USE_THREAT_EXT               | bool   | false         | false         | -    | -    | Use Threat Extension             |
| USE_EXT_ADD_DEPTH            | bool   | true          | true          | -    | -    | Use Extension Add                |
| USE_SINGULAR_EXT             | bool   | true          | true          | -    | -    | Use Singular Extension           |
| SINGULAR_MARGIN              | int    | 64            | 64            | 0    | 200  | Singular Margin                  |
| SINGULAR_MIN_DEPTH           | int    | 8             | 8             | 1    | 20   | Singular Min Depth               |
| SINGULAR_REDUCTION           | int    | 4             | 4             | 1    | 10   | Singular Reduction               |
| MOVES_LEFT_OPENING           | int    | 36            | 36            | 5    | 60   | Moves Left Opening               |
| MOVES_LEFT_MIDGAME           | int    | 28            | 28            | 5    | 60   | Moves Left Midgame               |
| MOVES_LEFT_ENDGAME           | int    | 16            | 16            | 5    | 60   | Moves Left Endgame               |
| MOVES_LEFT_LOW_MAT           | int    | 10            | 10            | 1    | 30   | Moves Left Low Material          |
| MOVES_LEFT_QUEENLESS         | int    | 22            | 22            | 5    | 60   | Moves Left Queenless             |
| NPP_HEAVY_THRESHOLD          | int    | 10            | 10            | 0    | 20   | NPP Heavy Threshold              |
| NPP_LIGHT_THRESHOLD          | int    | 4             | 4             | 0    | 20   | NPP Light Threshold              |
| REPETITION_HMC_HIGH          | int    | 80            | 80            | 0    | 100  | Repetition HMC High              |
| REPETITION_RISK_PENALTY      | int    | 6             | 6             | 0    | 20   | Repetition Risk Penalty          |
| MOVES_LEFT_MIN_CLAMP         | int    | 6             | 6             | 1    | 20   | Moves Left Min Clamp             |
| MOVES_LEFT_MAX_CLAMP         | int    | 50            | 50            | 10   | 100  | Moves Left Max Clamp             |
| USE_BESTMOVE_INSTABILITY     | bool   | true          | true          | -    | -    | Use BestMove Instability         |
| INSTABILITY_MIN_DEPTH        | int    | 5             | 8             | 1    | 20   | Instability Min Depth            |
| INSTABILITY_STABLE_COUNT     | int    | 3             | 5             | 1    | 10   | Instability Stable Count         |
| INSTABILITY_CHANGE_THRESHOLD | int    | 2             | 3             | 1    | 10   | Instability Change Threshold     |
| INSTABILITY_STABLE_FACTOR    | double | 0.80          | 0.9           | 50   | 100  | Instability Stable Factor Pct    |
| INSTABILITY_EXTEND_FACTOR    | double | 1.25          | 1.25          | 100  | 200  | Instability Extend Factor Pct    |
| **Eval**                     |        |               |               |      |      |                                  |
| USE_MATERIAL                 | bool   | true          | true          | -    | -    | Use Material                     |
| USE_POSITIONAL               | bool   | true          | true          | -    | -    | Use Positional                   |
| USE_TEMPO                    | bool   | true          | true          | -    | -    | Use Tempo                        |
| TEMPO                        | int    | 34            | 34            | 0    | 100  | Tempo Bonus                      |
| USE_LAZY_EVAL                | bool   | true          | true          | -    | -    | Use Lazy Eval                    |
| LAZY_THRESHOLD               | int    | 700           | 700           | 0    | 2000 | Lazy Threshold                   |
| USE_PAWN_EVAL                | bool   | true          | true          | -    | -    | Use Pawn Eval                    |
| USE_PAWN_TT                  | bool   | true          | true          | -    | -    | Use Pawn Hash                    |
| PAWN_TT_SIZE_MB              | int    | 64            | 64            | 1    | 1024 | Pawn Hash Size                   |
| ISOLATED_PAWN_MID_WEIGHT     | int    | -10           | -10           | -100 | 0    | Isolated Pawn Mid                |
| ISOLATED_PAWN_END_WEIGHT     | int    | -20           | -20           | -100 | 0    | Isolated Pawn End                |
| DOUBLED_PAWN_MID_WEIGHT      | int    | -10           | -10           | -100 | 0    | Doubled Pawn Mid                 |
| DOUBLED_PAWN_END_WEIGHT      | int    | -30           | -30           | -100 | 0    | Doubled Pawn End                 |
| PASSED_PAWN_MID_WEIGHT       | int    | 20            | 20            | 0    | 100  | Passed Pawn Mid                  |
| PASSED_PAWN_END_WEIGHT       | int    | 40            | 40            | 0    | 200  | Passed Pawn End                  |
| BLOCKED_PAWN_MID_WEIGHT      | int    | -2            | -2            | -50  | 0    | Blocked Pawn Mid                 |
| BLOCKED_PAWN_END_WEIGHT      | int    | -20           | -20           | -50  | 0    | Blocked Pawn End                 |
| PHALANX_PAWN_MID_WEIGHT      | int    | 4             | 4             | 0    | 50   | Phalanx Pawn Mid                 |
| PHALANX_PAWN_END_WEIGHT      | int    | 4             | 4             | 0    | 50   | Phalanx Pawn End                 |
| SUPPORTED_PAWN_MID_WEIGHT    | int    | 10            | 10            | 0    | 50   | Supported Pawn Mid               |
| SUPPORTED_PAWN_END_WEIGHT    | int    | 15            | 15            | 0    | 50   | Supported Pawn End               |
| USE_PIECE_EVAL               | bool   | true          | true          | -    | -    | Use Piece Eval                   |
| USE_BISHOP_PAIR_BONUS        | bool   | true          | true          | -    | -    | Use Bishop Pair Bonus            |
| BISHOP_PAIR_MID_BONUS        | int    | 20            | 20            | 0    | 100  | Bishop Pair Mid Bonus            |
| BISHOP_PAIR_END_BONUS        | int    | 20            | 20            | 0    | 100  | Bishop Pair End Bonus            |
| USE_KNIGHT_MOBILITY          | bool   | true          | true          | -    | -    | Use Knight Mobility              |
| KNIGHT_MOBILITY_MID_PER_MOVE | int    | 3             | 3             | 0    | 20   | Knight Mobility Mid              |
| KNIGHT_MOBILITY_END_PER_MOVE | int    | 2             | 2             | 0    | 20   | Knight Mobility End              |
| KNIGHT_LOW_MOBILITY_LEQ1_MID | int    | -6            | -6            | -50  | 0    | Knight Low Mob LEQ1 Mid          |
| KNIGHT_LOW_MOBILITY_LEQ1_END | int    | -6            | -6            | -50  | 0    | Knight Low Mob LEQ1 End          |
| KNIGHT_LOW_MOBILITY_LEQ2_MID | int    | -3            | -3            | -50  | 0    | Knight Low Mob LEQ2 Mid          |
| KNIGHT_LOW_MOBILITY_LEQ2_END | int    | -3            | -3            | -50  | 0    | Knight Low Mob LEQ2 End          |
| USE_BISHOP_MOBILITY          | bool   | true          | true          | -    | -    | Use Bishop Mobility              |
| BISHOP_MOBILITY_MID_PER_MOVE | int    | 2             | 2             | 0    | 20   | Bishop Mobility Mid              |
| BISHOP_MOBILITY_END_PER_MOVE | int    | 3             | 3             | 0    | 20   | Bishop Mobility End              |
| BISHOP_LOW_MOBILITY_LEQ3_MID | int    | -4            | -4            | -50  | 0    | Bishop Low Mob LEQ3 Mid          |
| BISHOP_LOW_MOBILITY_LEQ3_END | int    | -2            | -2            | -50  | 0    | Bishop Low Mob LEQ3 End          |
| USE_ROOK_MOBILITY            | bool   | true          | true          | -    | -    | Use Rook Mobility                |
| ROOK_MOBILITY_MID_PER_MOVE   | int    | 2             | 2             | 0    | 20   | Rook Mobility Mid                |
| ROOK_MOBILITY_END_PER_MOVE   | int    | 2             | 2             | 0    | 20   | Rook Mobility End                |
| ROOK_LOW_MOBILITY_LEQ3_MID   | int    | -3            | -3            | -50  | 0    | Rook Low Mob LEQ3 Mid            |
| ROOK_LOW_MOBILITY_LEQ3_END   | int    | -3            | -3            | -50  | 0    | Rook Low Mob LEQ3 End            |
| USE_ROOK_OPEN_FILE_BONUS     | bool   | true          | true          | -    | -    | Use Rook Open File Bonus         |
| ROOK_OPEN_FILE_MID_BONUS     | int    | 10            | 10            | 0    | 50   | Rook Open File Mid               |
| ROOK_OPEN_FILE_END_BONUS     | int    | 8             | 8             | 0    | 50   | Rook Open File End               |
| ROOK_SEMIOPEN_FILE_MID_BONUS | int    | 5             | 5             | 0    | 50   | Rook Semiopen File Mid           |
| ROOK_SEMIOPEN_FILE_END_BONUS | int    | 4             | 4             | 0    | 50   | Rook Semiopen File End           |
| USE_QUEEN_MOBILITY           | bool   | true          | true          | -    | -    | Use Queen Mobility               |
| QUEEN_MOBILITY_MID_PER_MOVE  | int    | 1             | 1             | 0    | 20   | Queen Mobility Mid               |
| QUEEN_MOBILITY_END_PER_MOVE  | int    | 1             | 1             | 0    | 20   | Queen Mobility End               |
| USE_QUEEN_TROPISM            | bool   | true          | true          | -    | -    | Use Queen Tropism                |
| QUEEN_TROPISM_MID_PER_STEP   | int    | 0             | 0             | 0    | 20   | Queen Tropism Mid                |
| QUEEN_TROPISM_END_PER_STEP   | int    | 1             | 1             | 0    | 20   | Queen Tropism End                |
| USE_KING_EVAL                | bool   | true          | true          | -    | -    | Use King Eval                    |
| USE_KING_SAFETY_SHIELD       | bool   | true          | true          | -    | -    | Use King Safety Shield           |
| KING_SHIELD_MID_PER_PAWN     | int    | 5             | 5             | 0    | 30   | King Shield Mid                  |
| KING_SHIELD_END_PER_PAWN     | int    | 0             | 0             | 0    | 30   | King Shield End                  |
| USE_GAMEPHASE_VALUE          | bool   | true          | true          | -    | -    | Use Game Phase Value             |
| **Debug**                    |        |               |               |      |      |                                  |
| CONFIG_SOURCE                | string | fallback      | current       | -    | -    |                                  |
| EVAL_CONFIG_SOURCE           | string | fallback      | current       | -    | -    |                                  |
