# Stockfish 18 ELO Bracketing — Results & Conclusions

**Date:** 2026-04-04  
**Versions tested:** FrankyCPP v1.7, v1.8  
**Opponent:** Stockfish 18 with `UCI_LimitStrength=true`  
**FrankyCPP settings:** `OwnBook=false; Threads=4`

---

## Background

After releasing v1.8, initial ELO bracketing matches (v1.8 vs SF18 at 10+0.1 with
`Threads=4` for both engines) showed an apparent **~170 Elo regression** compared to the
v1.7 baseline of +69 vs SF@2700. Investigation revealed **no code regression** — the
discrepancy was caused by two confounding factors in the test setup.

---

## Root Causes of Discrepancy

### 1. Stockfish Thread Count Mismatch

The original v1.7 baseline match did **not** specify `Threads` for Stockfish, so SF
defaulted to **1 thread**, while FrankyCPP used its default of 4 threads.

The v1.8 bracket matches explicitly set `Threads=4` for SF, making it significantly
stronger even under `UCI_LimitStrength`.

**Impact at 10+0.1, SF@2700:**

| SF Threads | v1.7 Score | Elo      |
|------------|------------|----------|
| 1          | 43.5%      | **-45**  |
| 4          | 32.5%      | **-127** |

**~82 Elo difference from thread count alone.**

### 2. Time Control Effect on `UCI_LimitStrength`

SF's strength limiter is much less effective at short time controls. At 10+0.1, natural
search depth is already shallow, so the limiter has less room to weaken SF. At 300+0,
SF must deliberately weaken its deep search across many iterations — the limitation is
far more pronounced.

**SF@2700, Threads=1:**

| TC     | v1.7 Score | Elo                 |
|--------|------------|---------------------|
| 300+0  | 59.8%      | **+69** (200 games) |
| 10+0.1 | 43.5%      | **-45** (100 games) |

**~114 Elo difference from time control alone.**

---

## No v1.8 Code Regression

All v1.8 code changes were verified as functional no-ops:

- **IID removal:** `USE_IID` was already `false` — dead code removal
- **Singular bound check removal:** condition was always `true` — dead code removal
- **Contempt:** default `CONTEMPT=0` → `drawScore()` returns `VALUE_DRAW` (identical)
- **Book variety:** matches use `OwnBook=false` — not exercised
- **Debug eval:** only active when `debug on` — off by default
- **EvalConfigData.h:** zero diff between v1.7 and v1.8
- **Config files:** identical between releases (all values commented out)
- **Binary sizes:** identical (7,049,216 bytes)

**Direct confirmation (same conditions as original v1.7 baseline):**

| Version | TC    | SF Threads | Games | Score | Elo     |
|---------|-------|------------|-------|-------|---------|
| v1.7    | 300+0 | 1          | 200   | 59.8% | **+69** |
| v1.8    | 300+0 | 1          | 100   | 57.0% | **+49** |

Difference is within statistical margin for 100 games (~±40–50 Elo). **No regression.**

---

## Full Results

### v1.8 vs SF18 — 10+0.1, SF Threads=4 (initial, flawed setup)

| Opponent | Games | Score | W/D/L    | Elo  |
|----------|-------|-------|----------|------|
| SF@2700  | 100   | 36.0% | 29/14/57 | -100 |
| SF@2800  | 100   | 23.5% | 14/19/67 | -205 |
| SF@2900  | 100   | 12.5% | 6/13/81  | -338 |
| SF@3000  | 100   | 9.0%  | 2/14/84  | -402 |
| SF@3100  | 100   | 5.5%  | 0/11/89  | -494 |

### v1.7 vs SF18 — 10+0.1, SF Threads=4

| Opponent | Games | Score | Elo  |
|----------|-------|-------|------|
| SF@2700  | 100   | 32.5% | -127 |
| SF@2800  | 100   | 16.5% | -282 |
| SF@2900  | 100   | 18.0% | -263 |
| SF@3000  | 100   | 7.5%  | -436 |
| SF@3100  | 100   | 4.5%  | -531 |

### v1.7 vs SF18 — 10+0.1, SF Threads=1

| Opponent | Games | Score | Elo  |
|----------|-------|-------|------|
| SF@2700  | 100   | 43.5% | -45  |
| SF@2800  | 100   | 21.5% | -225 |
| SF@2900  | 100   | 16.0% | -288 |
| SF@3000  | 100   | 10.5% | -372 |
| SF@3100  | 100   | 5.0%  | -512 |

### Baseline comparisons — 300+0, SF Threads=1

| Match           | Games | Score | Elo |
|-----------------|-------|-------|-----|
| v1.7 vs SF@2700 | 200   | 59.8% | +69 |
| v1.8 vs SF@2700 | 100   | 57.0% | +49 |

---

## ELO Estimate

Based on the 10+0.1 bracket with SF Threads=1, FrankyCPP v1.7/v1.8 scores 43.5% vs
SF@2700, placing it roughly around **~2650–2680** on SF's `UCI_LimitStrength` scale.

⚠️ **Caveat:** SF's `UCI_LimitStrength` is known to be an imprecise Elo limiter,
especially at short time controls. These numbers are only meaningful relative to this
specific implementation and should be treated as rough estimates, not FIDE-equivalent
ratings.

---

## Recommendations for Future Matches

1. **Always specify `Threads` explicitly** for both engines to avoid silent defaults
2. **Use consistent TC** when comparing across versions — 300+0 for strength measurement,
   10+0.1 for quick A/B testing
3. **SF `UCI_LimitStrength` results are TC-dependent** — do not compare results across
   different time controls
4. **For fair 4-thread matches**, set `Threads=4` for both engines (but note this makes
   SF stronger than its nominal ELO rating suggests)
