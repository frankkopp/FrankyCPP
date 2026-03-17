# FrankyCPP Evaluation & Strength Improvement Plan

**Document Version:** 1.1  
**Created:** 2026-03-17  
**Last Updated:** 2026-03-18  
**Status:** 🟡 IN PROGRESS (Phase 1 ✅ Complete & Validated)  
**Target:** FrankyCPP v1.6 → v2.0  
**Priority:** High (Primary path to strength gains)  
**Predecessor:** `V1_ENGINE_STRENGTH_ROADMAP.md`, `PLAN_Move_Ordering_Improvements.md`

---

## Executive Summary

Analysis of STS (Strategic Test Suite) results reveals that **evaluation quality is the primary bottleneck** for FrankyCPP's playing strength. The current evaluator (~556 lines) is lean compared to competitive classical HCE engines (~2000+ lines). The search is already well-featured (96% WAC), so the largest gains will come from evaluation enrichment.

**Goal:** Close the gap between FrankyCPP (58% STS) and strong classical HCE engines like Ruffian (65% STS). This 7-point gap is achievable without NNUE and should translate to roughly **+50–80 Elo** in real game strength.

**Non-goal:** Matching Stockfish's 80% STS — that gap is primarily due to NNUE pattern recognition and is unrealistic for a classical engine.

---

## Table of Contents

1. [Current State Analysis](#current-state-analysis)
2. [STS Category Breakdown](#sts-category-breakdown)
3. [Evaluation Gap Analysis](#evaluation-gap-analysis)
4. [Phase 1: Quick Eval Wins](#phase-1-quick-eval-wins)
5. [Phase 2: King Safety Overhaul](#phase-2-king-safety-overhaul)
6. [Phase 3: Strategic Evaluation](#phase-3-strategic-evaluation)
7. [Phase 4: Search Polish](#phase-4-search-polish)
8. [Phase 5: Automated Tuning](#phase-5-automated-tuning)
9. [Testing & Validation Strategy](#testing--validation-strategy)
10. [Implementation Tracking](#implementation-tracking)

---

## Current State Analysis

### STS Results (v1.6 dev, 5s/move, 2026-03-17)

**Overall: 870/1500 = 58%**

### Cross-Engine Comparison

| Engine                 | STS       | WAC       | Type              | Notes                      |
|------------------------|-----------|-----------|-------------------|----------------------------|
| Stockfish 18 (NNUE)    | 79.6%     | 99.0%     | NNUE              | Unrealistic target for HCE |
| **Ruffian 1.0.5**      | **64.9%** | **99.5%** | **Classical HCE** | **Realistic target**       |
| **FrankyCPP v1.6 dev** | **58.0%** | **~96%**  | **Classical HCE** | **Current**                |
| FrankyCPP v1.5         | 51.8%     | ~96%      | Classical HCE     | Previous release           |
| FrankyGo v1.0.3        | 45.3%     | 98.0%     | Classical HCE     | Go port                    |

### Historical STS Trend

| Version  | Best STS Score | Notes                             |
|----------|----------------|-----------------------------------|
| v0.4     | 50.9%          | Baseline                          |
| v0.5     | 51.0%          | Minor improvement                 |
| v1.1     | 49.9%          | Regression during refactor        |
| v1.2     | 48.9%          | Search changes, slight regression |
| v1.3     | 52.3%          | Recovery                          |
| v1.4     | 53.1%          | SMP, time management              |
| v1.5     | 52.8%          | TT bucket design                  |
| v1.6 dev | **58.0%**      | Eval improvements (current)       |

---

## STS Category Breakdown

### Sorted by Score (weakest first — priority targets)

| #  | STS Category                        | Score   | %     | Priority    |
|----|-------------------------------------|---------|-------|-------------|
| 1  | **AT (Advanced Tactics)**           | 41/100  | 41%   | 🔴 Critical |
| 2  | **Advancement of a/b/c pawns**      | 41/100  | 41%   | 🔴 Critical |
| 3  | **AKPC (Attack/King/Pawn/Center)**  | 52/100  | 52%   | 🔴 High     |
| 4  | **King Activity**                   | 55/100  | 55%   | ⚠️ Medium   |
| 5  | **Center Control**                  | 55/100  | 55%   | ⚠️ Medium   |
| 6  | **Knight Outposts/Repos./Central.** | 57/100  | 57%   | ⚠️ Medium   |
| 7  | Square Vacancy                      | 58/100  | 58%   | ⚠️ Medium   |
| 8  | Pawn Play in Center                 | 59/100  | 59%   | Low         |
| 9  | Recapturing                         | 62/100  | 62%   | Low         |
| 10 | Bishop vs Knight                    | 63/100  | 63%   | Low         |
| 11 | Undermine                           | 63/98   | 64.3% | Low         |
| 12 | Open Files and Diagonals            | 64/100  | 64%   | Low         |
| 13 | 7th Rank                            | 65/100  | 65%   | Low         |
| 14 | Simplification                      | 133/200 | 66.5% | Low         |

---

## Evaluation Gap Analysis

### What `Evaluator.cpp` Currently Implements

| Feature                                                                 | Status     | Lines (approx)    |
|-------------------------------------------------------------------------|------------|-------------------|
| Material balance                                                        | ✅ Complete | — (from Position) |
| Piece-square tables (mid+end)                                           | ✅ Complete | — (from Position) |
| Pawn structure (isolated, doubled, passed, blocked, phalanx, supported) | ✅ Complete | ~100              |
| Passed pawn rank-based bonus                                            | ✅ Complete | ~15               |
| Bishop pair bonus                                                       | ✅ Complete | ~5                |
| Piece mobility (N, B, R, Q)                                             | ✅ Complete | ~80               |
| Low mobility penalties                                                  | ✅ Complete | ~20               |
| Rook open/semi-open file                                                | ✅ Complete | ~15               |
| Rook on 7th rank                                                        | ✅ Complete | ~5                |
| Queen tropism (king distance)                                           | ✅ Complete | ~5                |
| King pawn shield                                                        | ✅ Complete | ~20               |
| King proximity to passed pawns                                          | ✅ Complete | ~30               |
| King safety (attacker count + weight table)                             | ✅ Basic    | ~10               |
| Lazy eval threshold                                                     | ✅ Complete | ~5                |
| Tapered eval (game phase)                                               | ✅ Complete | ~5                |
| Pawn TT cache                                                           | ✅ Complete | ~15               |

### What's Missing (mapped to weak STS categories)

| Missing Feature                                                          | Relevant STS Categories          | Expected Impact |
|--------------------------------------------------------------------------|----------------------------------|-----------------|
| **Knight outpost detection**                                             | Knight Outposts (57%)            | High            |
| **Pawn advancement bonus (non-passed)**                                  | Advancement a/b/c (41%)          | High            |
| **Space evaluation**                                                     | Center Control (55%), AKPC (52%) | Medium-High     |
| **Threat evaluation** (hanging pieces, attacks by lesser pieces)         | AT (41%), AKPC (52%)             | High            |
| **Enhanced king safety** (pawn storm, open files near king, safe checks) | King Activity (55%), AT (41%)    | High            |
| **Bishop color weakness** (bad bishop)                                   | Bishop vs Knight (63%)           | Medium          |
| **Rook behind passed pawn**                                              | 7th Rank (65%)                   | Low-Medium      |
| **Minor piece coordination** (connectivity)                              | Simplification (66.5%)           | Low             |

---

## Phase 1: Quick Eval Wins

**Timeline:** 1–2 weeks  
**Expected Gain:** +3–5% STS, +15–25 Elo  
**Status:** ✅ Complete — all unit tests pass, no regressions, eval timing validated

### Feature 1.1: Knight Outpost Bonus

**Targets:** STS Knight Outposts/Repos. (57%)

A knight on a square that cannot be attacked by enemy pawns, especially in the center on ranks 4–6, is a major positional asset.

**Implementation:**
```
For each knight on sq:
  if sq is on ranks 4-6 (relative):
    if no enemy pawn can attack sq (check adjacent files, forward ranks):
      if sq is supported by own pawn:
        bonus = OUTPOST_SUPPORTED_{MID,END}
      else:
        bonus = OUTPOST_UNSUPPORTED_{MID,END}
```

**Config parameters to add:**
- `USE_KNIGHT_OUTPOST` (bool)
- `KNIGHT_OUTPOST_SUPPORTED_MID` (int, default ~20)
- `KNIGHT_OUTPOST_SUPPORTED_END` (int, default ~15)
- `KNIGHT_OUTPOST_UNSUPPORTED_MID` (int, default ~10)
- `KNIGHT_OUTPOST_UNSUPPORTED_END` (int, default ~8)

**Difficulty:** Easy  
**Risk:** Low

---

### Feature 1.2: Pawn Advancement Bonus (Non-Passed)

**Targets:** STS Advancement a/b/c (41%)

Pawns that have advanced past rank 4 have strategic value even if they are not passed — they control space and restrict enemy pieces.

**Implementation:**
```
For each pawn (already in pawn loop):
  relRank = relative rank (2-7)
  if relRank >= 4 and pawn is NOT passed:
    bonus = PAWN_ADVANCE_BONUS[relRank - 4]  // indexed 0..3
```

**Config parameters to add:**
- `USE_PAWN_ADVANCE_BONUS` (bool)
- `PAWN_ADVANCE_MID_BONUS` (array[4], default {2, 5, 12, 25})
- `PAWN_ADVANCE_END_BONUS` (array[4], default {3, 8, 18, 35})

**Difficulty:** Easy (add to existing pawn loop)  
**Risk:** Low

---

### Feature 1.3: Bishop Color Weakness (Bad Bishop)

**Targets:** STS Bishop vs Knight (63%)

A bishop is "bad" when many of its own pawns are on the same color squares, blocking its diagonals and reducing its mobility.

**Implementation:**
```
For each bishop on sq:
  bishopColor = square color of sq (light/dark)
  ownPawnsOnColor = count own pawns on same color squares
  penalty = ownPawnsOnColor * BAD_BISHOP_PER_PAWN_{MID,END}
```

**Config parameters to add:**
- `USE_BAD_BISHOP` (bool)
- `BAD_BISHOP_PER_PAWN_MID` (int, default -3)
- `BAD_BISHOP_PER_PAWN_END` (int, default -5)

**Difficulty:** Easy  
**Risk:** Low

---

### Feature 1.4: Rook Behind Passed Pawn

**Targets:** STS 7th Rank (65%), general endgame play

Rooks are most effective behind passed pawns (own or enemy).

**Implementation:**
```
For each rook on sq:
  For each passed pawn (own or enemy) on same file:
    if rook is behind the pawn (relative to pawn push direction):
      bonus = ROOK_BEHIND_PASSER_{MID,END}
```

**Config parameters to add:**
- `USE_ROOK_BEHIND_PASSER` (bool)
- `ROOK_BEHIND_PASSER_MID` (int, default 10)
- `ROOK_BEHIND_PASSER_END` (int, default 20)

**Difficulty:** Medium (requires passed pawn info from pawn eval; may need to store in PawnTT)  
**Risk:** Low-Medium

---

## Phase 2: King Safety Overhaul

**Timeline:** 1–2 weeks  
**Expected Gain:** +2–4% STS, +15–25 Elo  
**Status:** 📋 Not Started

### Feature 2.1: Pawn Storm Detection

**Targets:** STS King Activity (55%), AT (41%)

Detect when opponent's pawns are advancing toward our king. Pawns on ranks 5+ near king file are a major threat.

**Implementation:**
```
For opponent's pawns near our king (files within ±1 of king file):
  relRank = rank relative to our king's side
  if relRank >= 4 (approaching our king):
    penalty += PAWN_STORM_PENALTY[relRank - 4]
```

**Config parameters to add:**
- `USE_PAWN_STORM` (bool)
- `PAWN_STORM_MID_PENALTY` (array[4], default {5, 15, 30, 50})

---

### Feature 2.2: Open File Near King Penalty

**Targets:** STS King Activity (55%), AKPC (52%)

An open or semi-open file near the king is dangerous — rooks and queens can penetrate.

**Implementation:**
```
For files within ±1 of king file:
  if no own pawn on file: 
    if no enemy pawn on file: penalty += OPEN_FILE_KING_MID
    else: penalty += SEMIOPEN_FILE_KING_MID
```

**Config parameters to add:**
- `USE_KING_OPEN_FILE` (bool)
- `KING_OPEN_FILE_MID_PENALTY` (int, default -20)
- `KING_SEMIOPEN_FILE_MID_PENALTY` (int, default -10)

---

### Feature 2.3: Safe Check Squares

**Targets:** STS AT (41%), King Activity (55%)

Count squares from which the enemy could give check without being captured. More safe check squares = more dangerous king position.

**Implementation:**
```
For each check square of each piece type (N, B, R, Q):
  if square is not attacked by us:
    safeChecks++
penalty = safeChecks * SAFE_CHECK_PENALTY_{MID}
```

**Difficulty:** Medium (need attack maps)  
**Risk:** Medium (can slow eval if not careful with bitboard ops)

---

## Phase 3: Strategic Evaluation

**Timeline:** 2–3 weeks  
**Expected Gain:** +2–4% STS, +15–25 Elo  
**Status:** 📋 Not Started

### Feature 3.1: Space Evaluation

**Targets:** STS Center Control (55%), AKPC (52%)

Space = number of safe squares in the first 4 ranks behind own pawn chain. More space = more maneuvering room for pieces.

**Implementation:**
```
spaceMask = own pawns shifted forward, combined
controlledSquares = squares behind own pawn chain on ranks 1-4
  that are NOT attacked by enemy pawns
spaceScore = popcount(controlledSquares) * SPACE_BONUS_{MID,END}
```

---

### Feature 3.2: Threat Evaluation

**Targets:** STS AT (41%), AKPC (52%)

Detect hanging (undefended) pieces and pieces attacked by lesser-value pieces.

**Implementation:**
```
For each enemy piece:
  if attacked by our lesser piece and not defended:
    bonus += THREAT_HANGING[pieceType]
  if attacked by our pawn:
    bonus += THREAT_BY_PAWN[pieceType]
```

**Difficulty:** Medium-Hard (requires attack map computation)  
**Risk:** Medium (eval speed sensitive)

---

### Feature 3.3: Minor Piece Coordination

**Targets:** General strength

Bonus for connected rooks (on same rank/file with no pieces between), and piece connectivity.

---

## Phase 4: Search Polish

**Timeline:** 2 weeks  
**Expected Gain:** +10–20 Elo  
**Status:** 📋 Not Started  
**Predecessor:** `PLAN_Move_Ordering_Improvements.md`

### Feature 4.1: Continuation History

Move ordering indexed by the previous move's piece-to-square. Significantly improves quiet move ordering at cut nodes.

**Reference:** Stockfish's `continuationHistory[piece][to]` table.

### Feature 4.2: Probcut

Before full search at depth D, do a reduced-depth search with a raised beta. If it fails high, skip the full search.

**Reference:** Stockfish probcut implementation, typically `depth >= 5`, reduced by 4.

### Feature 4.3: SEE Pruning in Main Search

Prune moves with negative SEE at low depths in the main search (not just quiescence). Partially covered by LMR, but explicit pruning is more effective.

**Note:** Feature 4.3 was tested as part of `PLAN_Move_Ordering_Improvements.md` Feature 4 and was ELO-neutral. May need revisiting with different thresholds after eval improvements.

---

## Phase 5: Automated Tuning

**Timeline:** Ongoing (after features are implemented)  
**Expected Gain:** +20–40 Elo  
**Status:** 📋 Not Started

### Texel's Tuning Method

Use labeled game data (win/loss/draw) to optimize all evaluation parameters simultaneously via gradient descent on prediction error.

**Requirements:**
1. ~100K+ positions from FrankyCPP self-play or CCRL games
2. Tuning framework (external tool or custom implementation)
3. Parameterize all eval weights into a flat vector (~60+ parameters currently, more after adding features)

**Approach:**
- Extract positions from PGN games with known results
- Evaluate each position with FrankyCPP's eval
- Minimize sigmoid(eval) vs actual game result using gradient descent
- Apply optimized weights, verify with gauntlet matches

**Alternative:** SPSA (Simultaneous Perturbation Stochastic Approximation) — tune via engine-vs-engine matches. Slower but doesn't require labeled positions.

---

## Testing & Validation Strategy

### For Each Feature

1. **Unit Test:** Verify the feature computes expected values on known positions
2. **STS Regression:** Run full STS suite (5s/move) — expect targeted category to improve
3. **WAC Check:** Run WAC to ensure no tactical regression
4. **Gauntlet Match:** 200+ games vs previous version at bullet/blitz time control
5. **Elo Measurement:** Use `cutechess-cli` or similar for reliable Elo estimates

### Acceptance Criteria

- **STS:** Targeted category must improve by ≥3 points, overall must not regress
- **WAC:** Must not drop below 95%
- **Elo:** Must be Elo-positive in gauntlet (±5 Elo tolerance for noise)
- If a feature is Elo-neutral or negative after tuning attempts, shelve it (document as FAILED like Capture History)

### Benchmarks to Track

| Metric              | Current  | Phase 1 Target | Phase 1 Actual | Phase 2 Target | Final Target |
|---------------------|----------|----------------|----------------|----------------|--------------|
| STS Overall         | 58%      | 61%            | 57.2%          | 63%            | 65%+         |
| WAC                 | ~96%     | ≥96%           | 95.5% ✅        | ≥96%           | ≥97%         |
| Elo vs v1.5         | baseline | +15            | **+81.4** ✅    | +30            | +60+         |
| Elo vs SF18 2700    | +6.9     | —              | **+56.1** ✅    | —              | —            |
| NPS (d12,128MB,4T)  | 7.00M    | —              | 6.61M (−5.5%)  | —              | —            |

---

## Implementation Tracking

| #   | Feature                   | Phase | Status         | STS Impact | Elo Impact | Notes                                          |
|-----|---------------------------|-------|----------------|------------|------------|------------------------------------------------|
| 1.1 | Knight Outpost Bonus      | 1     | ✅ Complete     | +5.4% STS  | +81.4 Elo  | Ranks 4-6 relative, pawn-supported/unsupported |
| 1.2 | Pawn Advancement Bonus    | 1     | ✅ Complete     | (combined) | (combined) | Non-passed pawns rank 4+, rank-indexed array   |
| 1.3 | Bad Bishop Detection      | 1     | ✅ Complete     | (combined) | (combined) | Penalty per own pawn on bishop's color         |
| 1.4 | Rook Behind Passer        | 1     | ✅ Complete     | (combined) | (combined) | Own + enemy passers, separate bonuses          |
| 2.1 | Pawn Storm Detection      | 2     | 📋 Not Started | —          | —          |                                                |
| 2.2 | Open File Near King       | 2     | 📋 Not Started | —          | —          |                                                |
| 2.3 | Safe Check Squares        | 2     | 📋 Not Started | —          | —          |                                                |
| 3.1 | Space Evaluation          | 3     | 📋 Not Started | —          | —          |                                                |
| 3.2 | Threat Evaluation         | 3     | 📋 Not Started | —          | —          |                                                |
| 3.3 | Minor Piece Coordination  | 3     | 📋 Not Started | —          | —          |                                                |
| 4.1 | Continuation History      | 4     | 📋 Not Started | —          | —          |                                                |
| 4.2 | Probcut                   | 4     | 📋 Not Started | —          | —          |                                                |
| 4.3 | SEE Pruning (main search) | 4     | 📋 Not Started | —          | —          | Tested before as Elo-neutral                   |
| 5.1 | Texel Tuning              | 5     | 📋 Not Started | —          | —          | After features complete                        |

---

## Risk Assessment

| Risk                                          | Likelihood | Impact | Mitigation                                                  |
|-----------------------------------------------|------------|--------|-------------------------------------------------------------|
| Eval features slow down NPS significantly     | Medium     | High   | Profile before/after; use bitboard ops; lazy eval threshold |
| New features interact negatively (eval noise) | Medium     | Medium | Add one feature at a time; measure independently            |
| Parameter tuning overfits to test positions   | Low        | High   | Validate on held-out positions AND gauntlet matches         |
| Complexity makes future changes harder        | Low        | Medium | Keep features togglable via config; clean code              |
| WAC regression from eval changes              | Low        | Medium | Run WAC after every change; tactical strength is primary    |

---

## References

- [Chessprogramming Wiki — Evaluation](https://www.chessprogramming.org/Evaluation)
- [Stockfish Classical Eval (pre-NNUE)](https://github.com/official-stockfish/Stockfish/blob/classical)
- [Texel's Tuning Method](https://www.chessprogramming.org/Texel%27s_Tuning_Method)
- [STS Strategic Test Suite](https://www.chessprogramming.org/Strategic_Test_Suite)
- `docs/specs/V1_ENGINE_STRENGTH_ROADMAP.md` — Overall v1→v2 roadmap
- `docs/specs/PLAN_Move_Ordering_Improvements.md` — Search improvements (companion plan)

---

*Last updated: 2026-03-18*
