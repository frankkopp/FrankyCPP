# FrankyCPP Evaluation & Strength Improvement Plan

**Document Version:** 1.5  
**Created:** 2026-03-17  
**Last Updated:** 2026-03-22  
**Status:** ✅ COMPLETE (Phase 1 ✅, Phase 2 ✅, Phase 3 ✅ — 3.2 active, 3.1/3.3 disabled, Phase 4 deferred, Phase 5 → separate project)  
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
7. [Phase 4: Search Polish (Deferred)](#phase-4-search-polish-deferred)
8. [Phase 5: Automated Tuning (Separate Project)](#phase-5-automated-tuning-separate-project)
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
**Status:** ✅ Complete & Validated

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

**Status:** ✅ Complete

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

**Status:** ✅ Complete

---

### Feature 2.3: Safe Check Squares

**Targets:** STS AT (41%), King Activity (55%)

Count squares from which the enemy could give check without being captured.
Only counts squares that are (1) reachable by actual enemy pieces (attackedBy[them]),
(2) not defended by us, and (3) only for piece types the enemy actually has on the board.

**Implementation:**
```
For each piece type (N, B, R, Q) that the enemy has:
  checkSquares = attack squares of that piece type from our king square
  safeMask = attackedBy[them] & ~attackedBy[us]
  safeChecks = popcount(checkSquares & safeMask)
  penalty += safeChecks * SAFE_CHECK_PENALTY_{MID}
```

**Note:** Initial implementation (v1) counted all undefended check squares regardless of
enemy piece existence and reachability — this caused -73 ELO regression vs SF18. The fix (v2)
filters by `attackedBy[them]` and piece existence, recovering full strength.

**Difficulty:** Medium (need attack maps)  
**Risk:** Medium (can slow eval if not careful with bitboard ops)

**Status:** ✅ Complete (v2 — fixed with attackedBy filter)

### Phase 2 Additional: PawnTT Passed Pawn Caching

Extended PawnTT::Entry with passedWhite/passedBlack bitboards (16→32 bytes per entry).
Passed pawns are now computed once in pawnEval() and cached through PawnTT, eliminating
redundant computation in evaluate(). On PawnTT cache hit, passedPawns[] are restored
directly from the cached entry. Also added pre-computed attackedBy[] arrays (king+pawn
attacks in evaluate(), piece attacks accumulated in pieceEval()) used by safe check
evaluation and available for future threat evaluation (Phase 3).

**Performance impact:** +2.7% NPS, rook-behind-passer cost reduced from 9.1% → 5.2%.

**Status:** ✅ Complete

### Phase 2 Validation Results

Test suites (v1.6.0 Phase 2 v2):
- STS: 883/1500 (58.9%), WAC: 193/201 (96.0%), ecm98: 560/769 (72.8%)
- Overall: 1878/2984 (62.9%) — up from Phase 1 62.1% (+0.9%)

Match results (100 games, 300s):
- vs v1.5: +74.1 ELO (47W/27D/26L) — ELO-neutral vs Phase 1 (+81.4), within ±15 noise
- vs SF18 @2700: +49.0 ELO (47W/20D/33L) — ELO-neutral vs Phase 1 (+56.1), within ±15 noise

Benchmark: 6,795K NPS (+2.7% vs Phase 1), 76.4B nodes (-3.4% — better pruning from richer eval)

---

## Phase 3: Strategic Evaluation

**Timeline:** 2–3 weeks  
**Expected Gain:** +2–4% STS, +15–25 Elo  
**Status:** 🔄 In Progress — Feature 3.2 (Threat Evaluation) first

**Implementation order:** Feature 3.2 (Threats) first — highest expected impact, targets the two
weakest STS categories (AT 41%, AKPC 52%). Benchmark + STS validation after 3.2, then decide
whether to proceed with 3.1 (Space) and 3.3 (Coordination) or move to Phase 4.

### Prerequisite 3.0: Per-Piece-Type Attack Map (`attackedByPT`)

**Status:** ✅ Complete

The existing `attackedBy[Color]` array stores the *union* of all attacks per side but does not
distinguish which piece type generates which attacks. Threat evaluation (3.2) and space evaluation
(3.1) both require knowing *which* piece type attacks a given square — e.g., "is this square
attacked by an enemy pawn?" or "is this piece attacked by a lesser-value attacker?"

**Implementation:**

Add to `Evaluator.h`:
```cpp
/// Per-piece-type, per-color attack bitboards.
/// Indexed as attackedByPT[PieceType][Color].
/// Reset in evaluate(), populated alongside attackedBy[] in the pre-compute block
/// (pawn/king attacks) and in each piece eval function (knight/bishop/rook/queen).
std::array<std::array<Bitboard, 2>, PT_LENGTH> attackedByPT{};
```

Changes in `Evaluator.cpp`:
- `evaluate()`: zero out `attackedByPT` alongside `attackedBy[]`, populate `attackedByPT[PAWN][WHITE/BLACK]`
  and `attackedByPT[KING][WHITE/BLACK]` in the pre-compute block.
- `knightEval()`: add `attackedByPT[KNIGHT][us] |= attacks;` (one line, next to existing `attackedBy[us] |= attacks`)
- `bishopEval()`: add `attackedByPT[BISHOP][us] |= attacks;`
- `rookEval()`: add `attackedByPT[ROOK][us] |= attacks;`
- `queenEval()`: add `attackedByPT[QUEEN][us] |= attacks;`

**Cost:** ~96 bytes additional scratch state per Evaluator instance, one extra OR operation per piece.
Expected NPS impact: negligible (< 0.5%).

**Difficulty:** Easy  
**Risk:** Low

---

### Feature 3.2: Threat Evaluation ← IMPLEMENT FIRST

**Targets:** STS AT (41%), AKPC (52%)  
**Status:** ✅ Complete  
**Depends on:** Prerequisite 3.0 (attackedByPT)

Detect hanging (undefended) pieces and pieces attacked by lesser-value pieces. Uses a three-tier
model inspired by Stockfish-classical's threat evaluation — all pure bitboard AND/OR operations,
no per-square loops needed for tiers 1–2.

**Design rationale — three tiers, no SEE:**

- **Tier 1: Pawn attacks on pieces** — A pawn attacking any piece is always a threat regardless
  of defense. Use per-victim-type bonuses (minor: small, rook: medium, queen: large).
  `enemyPieces & attackedByPT[PAWN][us]` — one AND + popcount per piece type.

- **Tier 2: Minor attacks on major pieces** — Knights/bishops attacking rooks or queens.
  `(enemyRooks | enemyQueens) & (attackedByPT[KNIGHT][us] | attackedByPT[BISHOP][us])`.
  Also cheap and clearly valuable.

- **Tier 3: Hanging pieces** — Enemy pieces attacked by us and defended by none of theirs.
  `enemyPieces & attackedBy[us] & ~attackedBy[them]`.
  Simple definition of "hanging" — the over-counting concern (e.g., a queen "defended" by a pawn
  still being effectively capturable) actually helps because it *should* penalize poorly defended
  pieces. SEE was considered but rejected: ~50× more expensive per square, would be called for
  every enemy piece, blowing the performance budget.

**Implementation:**

New method in `Evaluator.h` / `Evaluator.cpp`:
```cpp
/// Evaluates threats: pieces attacked by lesser-value pieces, hanging pieces.
/// Must be called AFTER all pieceEval() calls complete (needs full attackedBy[]
/// and attackedByPT[][] data).
/// @param p   The position to evaluate
/// @param s   Score struct to update
/// @param us  Color whose threats to evaluate (bonus for us)
void threatEval(const Position& p, Score& s, Color us);
```

Call site in `evaluate()` — insert between `pieceEval` block and `kingEval` block:
```cpp
// evaluate threats (requires fully populated attackedBy[] and attackedByPT[][])
if (EvalConfig.USE_THREAT_EVAL) {
  threatEval(p, score, WHITE);
  threatEval(p, score, BLACK);
}
```

Pseudocode for `threatEval()`:
```
them = ~us
enemyPieces = occupiedBb(them) & ~getPieceBb(them, KING)  // exclude king
ourPawnAttacks = attackedByPT[PAWN][us]
ourMinorAttacks = attackedByPT[KNIGHT][us] | attackedByPT[BISHOP][us]

// Tier 1: pawn attacks on pieces
pawnThreats_minor = popcount(getPieceBb(them, KNIGHT|BISHOP) & ourPawnAttacks)
pawnThreats_rook  = popcount(getPieceBb(them, ROOK) & ourPawnAttacks)
pawnThreats_queen = popcount(getPieceBb(them, QUEEN) & ourPawnAttacks)
mid += pawnThreats_minor * THREAT_BY_PAWN_MINOR_MID
end += pawnThreats_minor * THREAT_BY_PAWN_MINOR_END
mid += pawnThreats_rook  * THREAT_BY_PAWN_ROOK_MID
end += pawnThreats_rook  * THREAT_BY_PAWN_ROOK_END
mid += pawnThreats_queen * THREAT_BY_PAWN_QUEEN_MID
end += pawnThreats_queen * THREAT_BY_PAWN_QUEEN_END

// Tier 2: minor attacks on major pieces
minorThreats_rook  = popcount(getPieceBb(them, ROOK)  & ourMinorAttacks)
minorThreats_queen = popcount(getPieceBb(them, QUEEN) & ourMinorAttacks)
mid += minorThreats_rook  * THREAT_BY_MINOR_ROOK_MID
end += minorThreats_rook  * THREAT_BY_MINOR_ROOK_END
mid += minorThreats_queen * THREAT_BY_MINOR_QUEEN_MID
end += minorThreats_queen * THREAT_BY_MINOR_QUEEN_END

// Tier 3: hanging pieces (attacked by us, not defended by them)
hanging = enemyPieces & attackedBy[us] & ~attackedBy[them]
mid += popcount(hanging) * THREAT_HANGING_MID
end += popcount(hanging) * THREAT_HANGING_END

s.midgame += mid * us.sign()
s.endgame += end * us.sign()
```

**Config parameters to add (`EvalConfigData.h` + `ConfigRegistry.cpp`):**
```
USE_THREAT_EVAL                  (bool, default true)
THREAT_BY_PAWN_MINOR_MID         (int, default 5)
THREAT_BY_PAWN_MINOR_END         (int, default 5)
THREAT_BY_PAWN_ROOK_MID          (int, default 10)
THREAT_BY_PAWN_ROOK_END          (int, default 12)
THREAT_BY_PAWN_QUEEN_MID         (int, default 15)
THREAT_BY_PAWN_QUEEN_END         (int, default 20)
THREAT_BY_MINOR_ROOK_MID         (int, default 5)
THREAT_BY_MINOR_ROOK_END         (int, default 6)
THREAT_BY_MINOR_QUEEN_MID        (int, default 8)
THREAT_BY_MINOR_QUEEN_END        (int, default 10)
THREAT_HANGING_MID               (int, default 6)
THREAT_HANGING_END               (int, default 10)
```
Default weights are deliberately conservative — Texel tuning (Phase 5) will optimize them.

**Unit tests to add (`EvaluatorTest.cpp`):**

1. **ThreatByPawn_PawnAttacksRook** — Position with white pawn attacking black rook should
   evaluate better for white than same position without the pawn attack.
2. **ThreatByMinor_BishopAttacksQueen** — Position with bishop attacking undefended queen.
3. **ThreatHanging_UndefendedPiece** — Position with hanging black knight should give white
   a better eval than a position where the knight is defended.
4. **ThreatEval_ToggleChangesEval** — Same position evaluates differently with USE_THREAT_EVAL
   on vs off (like existing SafeCheck toggle test).
5. **ThreatEval_SymmetricPosition** — Symmetric position should have ~0 threat impact.
6. **Timing case** — Add `"Disable THREAT_EVAL (with PIECE_EVAL)"` case to
   `Timing_EvalConfig_FeatureImpact`.

Update `set_eval_config()` to include `e.USE_THREAT_EVAL = onoff;`.

**Files to modify:**
- `src/engine/Evaluator.h` — add `attackedByPT` member + `threatEval()` declaration
- `src/engine/Evaluator.cpp` — implement `threatEval()`, wire into `evaluate()`, populate `attackedByPT`
- `src/config/EvalConfigData.h` — add 13 config members
- `src/config/ConfigRegistry.cpp` — add 13 registry entries
- `config/eval.yaml` — add default values
- `test/engine/EvaluatorTest.cpp` — add tests + update `set_eval_config()`

**Difficulty:** Medium  
**Risk:** Medium (eval speed sensitive — must stay within ~3% NPS budget)

**Validation plan:** After implementation, run benchmark + STS + WAC. Decide on gauntlet match
based on those results. If STS AT/AKPC categories improve ≥3 points, proceed to match testing.

### Feature 3.2 Validation Results

**Status:** ✅ Implemented & Validated

Benchmark: 6,676K NPS (−1.8% vs Phase 2 v2) ✅ within ≤3% budget.
VTune profile: threatEval = 5.6% of eval time (2.42s total, 1.05s self).

Test suites (Phase 2 v2 → Phase 3.2):
- STS: 883 → 886 (+3, +0.2%)
- WAC: 193 → 190 (−3) ⚠️ tactical regression
- ecm98: 560 → 563 (+3)
- kaufman: 18 → 21 (+3)
- Overall: 1878 → 1883 (+5, +0.2%)

Match results (100 games, 300s):
- vs v1.5: +81.4 ELO (48W/27D/25L) — ELO-stable vs Phase 1 (+81.4), +7.3 vs Phase 2 v2 (+74.1)
- vs SF18 @2700: +41.9 ELO (44W/24D/32L) — down from Phase 2 v2's +49.0 (−7.1)

**Assessment:** Marginal negative vs strong opposition. STS +3 points is below the +30-60 target
for AT/AKPC categories. kaufman +3 is a bright spot. WAC −3 and SF18 −7.1 ELO suggest threat
bonuses bias the engine toward positional evaluation at the expense of tactical sharpness against
strong opponents. Tier 3 (hanging) weights reduced from 12/15 to 6/10 to mitigate tactical
regression. Further tuning deferred to Phase 5 (Texel). Feature is retained: the attackedByPT
infrastructure is valuable for future features and the NPS cost is modest (−1.8%).

---

### Feature 3.1: Space Evaluation

**Targets:** STS Center Control (55%), AKPC (52%)  
**Status:** ✅ Complete  
**Depends on:** Prerequisite 3.0 (attackedByPT — for pawn attack exclusion)

Space = number of safe squares in the first 4 ranks behind own pawn chain. More space = more
maneuvering room for pieces. Consider making this midgame-weighted (space matters less in endgame).

**Implementation:**
```
spaceMask = own pawns shifted forward, combined
controlledSquares = squares behind own pawn chain on ranks 2-4 (relative)
  that are NOT attacked by enemy pawns (attackedByPT[PAWN][them])
spaceScore = popcount(controlledSquares) * SPACE_BONUS_{MID,END}
```

**Config parameters to add:**
- `USE_SPACE_EVAL` (bool)
- `SPACE_BONUS_MID` (int, default ~3)
- `SPACE_BONUS_END` (int, default ~1)

**Difficulty:** Easy-Medium  
**Risk:** Low

---

### Feature 3.3: Minor Piece Coordination

**Targets:** General strength  
**Status:** ✅ Complete

Bonus for connected rooks (on same rank/file with no pieces between using `intermediateBb[][]`),
and minor piece connectivity (knight/bishop defended by another minor piece).

**Config parameters to add:**
- `USE_CONNECTED_ROOKS` (bool)
- `CONNECTED_ROOKS_MID_BONUS` (int, default ~8)
- `CONNECTED_ROOKS_END_BONUS` (int, default ~5)
- `USE_MINOR_CONNECTIVITY` (bool)
- `MINOR_CONNECTIVITY_MID_BONUS` (int, default ~4)
- `MINOR_CONNECTIVITY_END_BONUS` (int, default ~3)

**Difficulty:** Easy-Medium  
**Risk:** Low (but may be Elo-neutral)

---

## Phase 4: Search Polish (Deferred)

**Timeline:** Deferred indefinitely  
**Expected Gain:** +10–20 Elo (speculative)  
**Status:** ⏸️ Deferred  
**Predecessor:** `PLAN_Move_Ordering_Improvements.md`

> **Decision (2026-03-22):** Phase 4 is deferred. Phases 2–3 demonstrated that adding features with
> hand-tuned parameters yields diminishing returns — only Phase 1 delivered measurable match strength.
> Search polish features (continuation history, probcut, SEE pruning) carry the same risk of being
> ELO-neutral after implementation effort. Priority shifts to Texel tuning of existing parameters
> (see `PLAN_Texel_Tuning.md`), which can optimize what we already have at zero NPS cost.
> These features may be revisited after tuning establishes a stronger baseline.

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

## Phase 5: Automated Tuning (Separate Project)

**Timeline:** Separate project  
**Expected Gain:** +20–50 Elo  
**Status:** 📋 Moved to `PLAN_Texel_Tuning.md`

> **Decision (2026-03-22):** Texel tuning is the most promising next step for strength gains.
> It optimizes all ~85 existing eval parameters simultaneously against labeled game data, adding
> zero NPS cost. Moved to a dedicated spec document with full implementation plan.
> See **`docs/archive/PLAN_Texel_Tuning.md`** for algorithm details, data pipeline, parameter
> selection, integration plan, and effort estimates (~3 weeks for Tier 1+2).

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

| Metric             | Current  | Phase 1 Target | Phase 1 Actual | Phase 2 Target | Phase 2 Actual | Phase 3 Target | Phase 3.2 Actual     | Phase 3all Actual   | Final Target |
|--------------------|----------|----------------|----------------|----------------|----------------|----------------|----------------------|---------------------|--------------|
| STS Overall        | 58%      | 61%            | 57.2%          | 63%            | 58.9%          | 61%            | 59.1% (+0.2%)        | 57.1% (−2.0%) ❌     | 65%+         |
| WAC                | ~96%     | ≥96%           | 95.5% ✅        | ≥96%           | 96.0% ✅        | ≥96%           | 94.5% ⚠️             | 95.5% ✅             | ≥97%         |
| Elo vs v1.5        | baseline | +15            | **+81.4** ✅    | +30            | **+74.1** ✅    | +85            | **+81.4** (~neutral) | **+3.5** ❌          | +60+         |
| Elo vs SF18 2700   | +6.9     | —              | **+56.1** ✅    | —              | **+49.0** ✅    | —              | **+41.9** (−7.1) ⚠️  | **−0.0** ❌          | —            |
| NPS (d12,128MB,4T) | 7.00M    | —              | 6.61M (−5.5%)  | —              | 6.80M (+2.7%)  | ≤3% regression | **6.68M (−1.8%)** ✅  | **6.32M (−7.0%)** ❌ | —            |

---

## Phase Results Summary

All results collected during development. **v1.5 Baseline** is the last measurement before Phase 1 started.
Phase 2 (first attempt) was abandoned in favor of Phase 2 v2 and is omitted.

### Key Metrics

| Metric                  | v1.5 Baseline | Phase 1    | Phase 2 v2 | Phase 3.2    | Phase 3all    |
|-------------------------|---------------|------------|------------|--------------|---------------|
| **Date**                | 2026-03-10    | 2026-03-18 | 2026-03-19 | 2026-03-19   | 2026-03-21    |
| **NPS** (d12,128MB,4T)  | 7,001,492     | 6,614,192  | 6,795,488  | 6,676,526    | 6,319,187     |
| **NPS Δ vs Baseline**   | —             | −5.5%      | −2.9%      | −4.6%        | **−9.7%**     |
| **NPS Δ vs Previous**   | —             | −5.5%      | +2.7%      | −1.8%        | **−5.4%**     |
| **Test Suite Total**    | 1747/2875†    | 1852/2984  | 1878/2984  | 1883/2984    | **1857/2984** |
| **Test Suite %**        | 60.8%†        | 62.1%      | 62.9%      | **63.1%**    | 62.2%         |
| **Δ Total vs Baseline** | —             | +105†      | +131†      | +136†        | +110†         |
| **Δ Total vs Previous** | —             | —          | +26        | **+5**       | **−26**       |
| **STS1-STS15**          | 870/1500‡     | 858/1500   | 883/1500   | **886/1500** | 856/1500      |
| **STS %**               | 58.0%‡        | 57.2%      | 58.9%      | **59.1%**    | 57.1%         |
| **ELO vs v1.5**         | —             | +81.4      | +74.1      | +81.4        | **+3.5** ❌    |
| **ELO vs SF18 2700**    | +6.9          | +56.1      | +49.0      | +41.9        | **−0.0** ❌    |

> † v1.5 used 7 suites (2875 positions, no eigenmann). Phase 1+ added eigenmann (2984 positions).  
>   Cross-phase deltas are approximate due to this suite change.  
> ‡ Standalone STS run (5s/move, 2026-03-17) — not from the same batch as the 7-suite run.

### Per-Suite Breakdown

| Suite        | Max      | v1.5 Baseline† | Phase 1          | Phase 2 v2       | Phase 3.2        | Phase 3all       | Δ 3all vs 3.2 |
|--------------|----------|----------------|------------------|------------------|------------------|------------------|---------------|
| STS1-STS15   | 1500     | —              | 858 (57.2%)      | 883 (58.9%)      | **886 (59.1%)**  | 856 (57.1%)      | **−30**       |
| crafty_test  | 347      | —              | 181 (52.2%)      | 182 (52.4%)      | 181 (52.2%)      | **186 (53.6%)**  | +5            |
| ecm98        | 769      | —              | 559 (72.7%)      | 560 (72.8%)      | **563 (73.2%)**  | 561 (73.0%)      | −2            |
| wac          | 201      | —              | 192 (95.5%)      | **193 (96.0%)**  | 190 (94.5%)      | 192 (95.5%)      | +2            |
| kaufman      | 25       | —              | 21 (84.0%)       | 18 (72.0%)       | **21 (84.0%)**   | **21 (84.0%)**   | 0             |
| mate_test    | 20       | —              | 16 (80.0%)       | 17 (85.0%)       | 16 (80.0%)       | 17 (85.0%)       | +1            |
| franky_tests | 13       | —              | 13 (100%)        | 13 (100%)        | 13 (100%)        | 13 (100%)        | 0             |
| eigenmann    | 109      | —              | 12 (11.0%)       | 12 (11.0%)       | **13 (11.9%)**   | 11 (10.1%)       | −2            |
| **TOTAL**    | **2984** | **1747/2875†** | **1852 (62.1%)** | **1878 (62.9%)** | **1883 (63.1%)** | **1857 (62.2%)** | **−26**       |

> † v1.5 individual suite scores not available for this run; total was 1747/2875 (7 suites, no eigenmann).

### Match Results

| Match            | v1.5 Baseline    | Phase 1           | Phase 2 v2        | Phase 3.2         | Phase 3all             |
|------------------|------------------|-------------------|-------------------|-------------------|------------------------|
| **vs v1.5**      | —                | 48W/27D/25L +81.4 | 47W/27D/26L +74.1 | 48W/27D/25L +81.4 | 37W/27D/36L **+3.5** ❌ |
| **vs SF18 2700** | 49W/4D/47L +6.9  | 50W/16D/34L +56.1 | 47W/20D/33L +49.0 | 44W/24D/32L +41.9 | 44W/12D/44L **−0.0** ❌ |

### Analysis

- **Phase 3.2** (threat eval only) was the **high-water mark**: 1883 total (63.1%), STS 886 (59.1%), NPS cost only −1.8%.
- **Phase 3all** (adding space + coordination) is a **confirmed regression**:
  - **+3.5 ELO vs v1.5** (down from +81.4) — nearly all playing strength erased
  - **−0.0 ELO vs SF18 2700** (down from +41.9) — completely neutral against external opponent
  - **−26 positions** vs Phase 3.2 (1883 → 1857), **−30 STS positions** (886 → 856)
  - **−5.4% additional NPS** cost (6.68M → 6.32M)
  - The NPS loss from space + coordination features far outweighed any eval accuracy gain
- **Decision (2026-03-22):** Features 3.1 (space eval) and 3.3 (coordination) **disabled by default**.
  Code retained behind `USE_*` flags for potential future Texel tuning (see `PLAN_Texel_Tuning.md`).
  Phase 3.2 (threat eval) kept active — the STS improvement and moderate SF18 impact are acceptable.

---

## Implementation Tracking

| #   | Feature                   | Phase | Status              | STS Impact | Elo Impact | Notes                                              |
|-----|---------------------------|-------|---------------------|------------|------------|----------------------------------------------------|
| 1.1 | Knight Outpost Bonus      | 1     | ✅ Complete          | +5.4% STS  | +81.4 Elo  | Ranks 4-6 relative, pawn-supported/unsupported     |
| 1.2 | Pawn Advancement Bonus    | 1     | ✅ Complete          | (combined) | (combined) | Non-passed pawns rank 4+, rank-indexed array       |
| 1.3 | Bad Bishop Detection      | 1     | ✅ Complete          | (combined) | (combined) | Penalty per own pawn on bishop's color             |
| 1.4 | Rook Behind Passer        | 1     | ✅ Complete          | (combined) | (combined) | Own + enemy passers, separate bonuses              |
| 2.1 | Pawn Storm Detection      | 2     | ✅ Complete          | +0.9% STS  | ~neutral   | Penalty for opponent pawns approaching king        |
| 2.2 | Open File Near King       | 2     | ✅ Complete          | (combined) | (combined) | Open/semi-open file penalty near king              |
| 2.3 | Safe Check Squares        | 2     | ✅ Complete          | (combined) | (combined) | Filtered by attackedBy[them] + piece existence     |
| 2.4 | PawnTT Passed Pawn Cache  | 2     | ✅ Complete          | +2.7% NPS  | (perf)     | passedPawns cached in PawnTT, 16→32 byte entry     |
| 3.0 | attackedByPT Array        | 3     | ✅ Complete          | —          | −1.8% NPS  | Per-piece-type attacks; memset reset               |
| 3.2 | Threat Evaluation         | 3     | ✅ Complete          | +0.2% STS  | −7.1 SF18  | 3-tier; hanging reduced 12/15→6/10; needs tuning   |
| 3.1 | Space Evaluation          | 3     | ❌ Disabled          | −2.0% STS  | +3.5/−0.0  | Disabled: −9.7% NPS, +3.5 v1.5 ELO (was +81.4)     |
| 3.3 | Minor Piece Coordination  | 3     | ❌ Disabled          | (combined) | (combined) | Disabled: see 3.1; code retained for Texel tuning  |
| 4.1 | Continuation History      | 4     | ⏸️ Deferred         | —          | —          | Phase 4 deferred — see phase section for rationale |
| 4.2 | Probcut                   | 4     | ⏸️ Deferred         | —          | —          | Phase 4 deferred                                   |
| 4.3 | SEE Pruning (main search) | 4     | ⏸️ Deferred         | —          | —          | Phase 4 deferred; previously tested as Elo-neutral |
| 5.1 | Texel Tuning              | 5     | 📋 Separate project | —          | —          | See `PLAN_Texel_Tuning.md`                         |

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


*Last updated: 2026-03-22*
