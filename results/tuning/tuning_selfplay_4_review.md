# FrankyCPP Texel Tuning — Selfplay 4.57M, 50-Pass Review

**Run:** `tuning_selfplay_4`  
**Date:** 2026-03-27  
**Dataset:** `selfplay_v1.7_50k_score.txt` (4,568,763 positions, 80/20 split)  

---

## Tuning Run Summary

| Metric                   | Value                             |
|--------------------------|-----------------------------------|
| **Dataset**              | 4,568,763 positions (80/20 split) |
| **K (scaling constant)** | 1.009253                          |
| **Parameters tuned**     | 122                               |
| **Passes completed**     | 50                                |
| **Wall time**            | ~14.3 hours (51,468s)             |
| **Train MSE**            | 0.09178 → 0.08700 **(−5.21%)**    |
| **Test MSE**             | 0.09000 → 0.08540 **(−5.11%)**    |

---

## 1. Convergence — Effectively Converged

The MSE improvement by decade shows clear diminishing returns:

| Passes | Train MSE Δ             | Params Changed (last pass) | Biggest Delta |
|--------|-------------------------|----------------------------|---------------|
| 1–10   | −0.00402 (84% of total) | 65                         | 2.21e-05      |
| 11–20  | −0.00051 (10.7%)        | 53                         | 1.09e-05      |
| 21–30  | −0.00017 (3.5%)         | 38                         | 1.50e-06      |
| 31–40  | −0.00005 (1.1%)         | 27                         | 5.79e-07      |
| 41–50  | −0.00002 (0.5%)         | 23                         | 2.98e-07      |

23 params are still technically changing at pass 50, but the biggest mover contributes only **2.98e-07** MSE delta — completely negligible. The run is effectively converged. Further passes would be wasted compute. **Verdict: ✅ No need to extend.**

---

## 2. Overfitting — None Whatsoever

| Split         | Baseline MSE | Final MSE |
|---------------|--------------|-----------|
| Train (3.65M) | 0.09178      | 0.08700   |
| Test (914K)   | 0.09000      | 0.08540   |

Test MSE is **below** train MSE throughout the entire run (by ~0.16 percentage points). This means:
- **Zero overfitting** — the 4.57M dataset is large enough for 122 parameters
- The test split happens to contain slightly more predictable positions (expected with random splits)
- Train-test gap stayed consistent from pass 1 through 50

**Verdict: ✅ Excellent generalization.**

---

## 3. Sign-Flipped Parameters (6) — Mixed Signals

| Parameter                   | Original | Tuned | Concern Level                                                |
|-----------------------------|----------|-------|--------------------------------------------------------------|
| `SPACE_BONUS_MID`           | 3        | −2    | 🗑️ **Confirm removal** — flipped in all 3 datasets          |
| `SPACE_BONUS_END`           | 1        | −1    | 🗑️ **Confirm removal** — flipped in all 3 datasets          |
| `CONNECTED_ROOKS_MID_BONUS` | 8        | −13   | ⚠️ MID detection may be flawed; END went 5 → +46, legitimate |
| `PAWN_ADVANCE_END_BONUS[0]` | 3        | −10   | ⚠️ Only rank-4; higher ranks unchanged. Interaction effect?  |
| `PAWN_STORM_MID_PENALTY[0]` | 5        | −12   | ⚠️ Only distant storm pawns. Small magnitude                 |
| `THREAT_BY_MINOR_ROOK_END`  | 6        | −14   | ⚠️ Possible piece-imbalance proxy in endgame                 |

**Recommendation for Phase 7.3:** Zero out all 6 sign-flipped values before applying. The SPACE_BONUS entries are confirmed dead (zeroed/flipped across all 3 datasets). The others are small enough that zeroing is safe. CONNECTED_ROOKS is interesting — the MID/END split suggests the feature is real in endgames but the MID detection logic may be capturing something else (file control? target alignment?).

---

## 4. Zeroed-Out Parameters (14) — Strong Removal Candidates

These all converged to exactly 0, confirming they contribute nothing to prediction accuracy:

| Category                   | Zeroed Parameters                                                                                                                    |
|----------------------------|--------------------------------------------------------------------------------------------------------------------------------------|
| **Pawn structure**         | `PASSED_PAWN_MID_WEIGHT`, `BLOCKED_PAWN_MID_WEIGHT`, `SUPPORTED_PAWN_END_WEIGHT`                                                     |
| **Low mobility penalties** | `KNIGHT_LOW_MOBILITY_LEQ1_MID`, `KNIGHT_LOW_MOBILITY_LEQ2_MID/END`, `BISHOP_LOW_MOBILITY_LEQ3_MID`, `ROOK_LOW_MOBILITY_LEQ3_MID/END` |
| **Bad bishop**             | `BAD_BISHOP_PER_PAWN_MID/END`                                                                                                        |
| **King safety**            | `KING_SHIELD_MID_PER_PAWN`, `KING_SEMIOPEN_FILE_MID_PENALTY`, `SAFE_CHECK_BISHOP_MID`                                                |

Consistent with the 10-pass Phase 6.11 analysis. All low-mobility MID penalties zeroed — the engine's mobility-per-move already handles this. BAD_BISHOP zeroed in all 3 datasets. KING_SHIELD zeroed while PAWN_STORM and KING_SAFETY_TABLE took over the king safety role.

**Verdict: ✅ Confirms Phase 8 removal plan.**

---

## 5. Most Significant Parameter Shifts

### 🔴 TEMPO: 34 → 4 (−88%)
The single most impactful finding. Tempo was adding ~34 cp to every evaluation, which is enormous. Dropping to 4 is a radical change. This will significantly affect the engine's playing style — less aggressive positional decisions, more conservative evaluations. **This is the highest-risk change to validate in gauntlet testing.**

### 🟢 Threats Massively Increased
| Parameter                  | Original → Tuned | Change |
|----------------------------|------------------|--------|
| `THREAT_BY_PAWN_MINOR_MID` | 5 → 44           | +780%  |
| `THREAT_BY_PAWN_ROOK_MID`  | 10 → 48          | +380%  |
| `THREAT_BY_MINOR_ROOK_MID` | 5 → 55           | +1000% |
| `THREAT_HANGING_END`       | 10 → 43          | +330%  |

The hand-tuned threat values were an order of magnitude too low. This is a strong, consistent signal — the engine should care much more about threats.

### 🟢 Endgame Mobility Undervalued
| Parameter             | Original → Tuned | Change |
|-----------------------|------------------|--------|
| `ROOK_MOBILITY_END`   | 2 → 9            | +350%  |
| `QUEEN_MOBILITY_END`  | 1 → 11           | +1000% |
| `BISHOP_MOBILITY_END` | 3 → 6            | +100%  |
| `QUEEN_TROPISM_END`   | 1 → 20           | +1900% |

The engine was severely undervaluing piece activity in the endgame. This should translate to better endgame play.

### 🟢 Bishop Pair: 30/45 → 52/66 (+73%/+47%)
Bishop pair was significantly undervalued — now closer to the theoretical ~0.5 pawn bonus that most engines converge to.

### 🟢 Knight Outposts: Doubled
Supported MID 20→39, unsupported MID 10→39. The engine wasn't rewarding outpost placement enough.

### 🟢 King Safety Table — Sharper Curve
Low danger (indices 4–9): decreased. High danger (indices 11–13): increased by +40–48. The king safety function is now more nonlinear — tolerating moderate attacks better but punishing severe ones harder. This is typical of well-tuned engines.

### 🟢 Passed Pawn Rank Bonuses — Endgame Much Higher
END bonuses uniformly increased (18, 41, 51, 84, 101, 120 vs old 0, 5, 15, 35, 70, 120). The engine was undervaluing passed pawns on lower ranks in endgames.

### 🟡 KING_OPP_PASSED_PROXIMITY_END: 3 → 20 (+567%)
Enemy king distance from passed pawns is now weighted very heavily. Combined with the passed pawn rank bonus increases, endgame passed pawn play should improve dramatically.

---

## 6. Comparison with 10-Pass Results

The 40 additional passes (11–50) added ~16% more total improvement:
- Pass 10 MSE: 0.08776 / 0.08608 (train/test)
- Pass 50 MSE: 0.08700 / 0.08540 (train/test)
- Extra improvement: 0.00076 (train), 0.00068 (test)

The directional signals from the 10-pass analysis were all confirmed. No parameter reversed direction between pass 10 and pass 50 — the optimizer simply refined magnitudes. This validates the earlier Phase 6.11 decision.

---

## 7. Recommendations for Phase 7

1. **Prepare `eval.yaml`:** Copy `tuning_selfplay_4.yaml` to `config/eval.yaml`
2. **Zero sign-flipped params:** Set `SPACE_BONUS_MID/END`, `CONNECTED_ROOKS_MID_BONUS`, `PAWN_ADVANCE_END_BONUS[0]`, `PAWN_STORM_MID_PENALTY[0]`, `THREAT_BY_MINOR_ROOK_END` all to 0
3. **Smoke test:** UCI `isready`, quick fixed-depth search to verify no crashes
4. **Gauntlet priority:** The TEMPO change (34→4) is the biggest risk — if ELO drops, try a compromise value (15–20) first
5. **STS/WAC regression:** Run test suites before gauntlet to catch gross eval regressions early
6. **No need for more passes** — the optimizer is converged

---

## Overall Assessment

**This is a very clean, well-converged tuning run with no overfitting and strong cross-dataset consistency. The 5.2% MSE improvement should translate to meaningful ELO gains.**
