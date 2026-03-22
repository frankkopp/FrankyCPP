# FrankyCPP Texel Tuning Plan

**Document Version:** 1.0  
**Created:** 2026-03-21  
**Status:** 📋 Planning  
**Target:** FrankyCPP v2.0  
**Priority:** High (Phase 5 of Eval & Strength Improvement Plan)  
**Predecessor:** `PLAN_Eval_and_Strength_Improvement.md`

---

## Motivation

Phase 1 of the eval improvement plan gained +81 ELO with hand-picked parameter values. Phases 2–3
added more eval terms but failed to deliver additional match strength — the gains were within noise,
and Phase 3all actually regressed (−26 test suite positions, −9.7% NPS).

The engine now has ~85 numeric eval parameters, every one of them a hand-tuned guess. Texel tuning
optimizes all parameters simultaneously against millions of labeled positions. It adds **zero NPS
cost** (no new code in the eval hot path) and is the standard approach used by virtually every
competitive classical HCE engine (Ethereal, Weiss, Demolito, etc.), typically attributed to
+50–100 ELO over hand-tuned values.

Additionally, Texel tuning can answer the Phase 3 question objectively: if the tuner finds nonzero
optimal weights for space/coordination features, they're worth keeping. If it zeros them out,
remove them with confidence.

---

## Table of Contents

1. [What Is Texel Tuning](#what-is-texel-tuning)
2. [Mathematical Formulation](#mathematical-formulation)
3. [Data Requirements](#data-requirements)
4. [Position Extraction Pipeline](#position-extraction-pipeline)
5. [Optimization Algorithm](#optimization-algorithm)
6. [Parameters to Tune](#parameters-to-tune)
7. [Implementation Plan](#implementation-plan)
8. [Integration with FrankyCPP](#integration-with-frankycpp)
9. [Validation Strategy](#validation-strategy)
10. [Risks and Pitfalls](#risks-and-pitfalls)
11. [Estimated Effort](#estimated-effort)
12. [References](#references)

---

## What Is Texel Tuning

Texel's Tuning Method (Peter Österlund, 2014) treats eval parameter optimization as **supervised
regression**. The core idea:

1. Collect millions of positions from real games, each labeled with the game outcome (win/draw/loss)
2. For each position, compute the engine's **static evaluation** (no search — just `evaluate()`)
3. Map the eval score to an expected outcome via a sigmoid function
4. Find parameter values that minimize the mean squared error between predicted and actual outcomes

The key insight: **no engine search is needed**. Only the fast static `evaluate()` function is called
per position, making it orders of magnitude faster than match-based tuning (SPSA). A full tuning run
over 5 million positions with ~85 parameters completes in 30–60 minutes on modern hardware.

### Why This Works

Game outcomes encode the ground truth about what "good" positions look like. If a position was
reached in a game that White won, the eval should lean positive for White. A systematic deviation
between eval predictions and game outcomes reveals parameter miscalibration. The sigmoid mapping
connects centipawn eval scores to win probability on the same scale as Elo ratings.

### What It Cannot Do

- Texel tuning only optimizes **eval parameters**, not search parameters (LMR tables, NMP depth,
  etc.). Search params require match-based tuning (SPSA) because their effect depends on the
  entire search tree, not individual positions.
- It cannot add new eval features — only optimize weights of existing ones.
- It assumes the dataset is representative of real play. Dataset bias → parameter bias.

---

## Mathematical Formulation

### Error Function

Given:
- N labeled positions, each with a game result R_i ∈ {1.0, 0.5, 0.0} (white win / draw / white loss)
- Engine static eval E_i(params) for position i (from White's perspective, in centipawns)
- Scaling constant K

The objective is to minimize:

```
Error(K, params) = (1/N) × Σᵢ (Rᵢ − σ(K, Eᵢ))²
```

where the sigmoid function maps eval to expected outcome:

```
σ(K, e) = 1 / (1 + 10^(−K × e / 400))
```

This sigmoid has the same shape as the Elo expected-score formula:
- eval = 0 cp  → σ = 0.5 (equal chance)
- eval = +100 cp → σ ≈ 0.64 (slightly favors White)
- eval = +400 cp → σ ≈ 0.91 (strongly favors White)
- eval = −∞     → σ = 0.0 (Black wins)

### Scaling Constant K

K controls how steeply eval maps to win probability. Too small → all evals map near 0.5
(underfitting). Too large → small eval differences map to extreme outcomes (overfitting to material).

K is tuned once before parameter optimization:
- Fix all eval params at current values
- Binary search K ∈ [0.5, 2.0] to minimize MSE
- Typically converges to K ≈ 1.0–1.5 for classical evals
- K is then held constant during parameter tuning

---

## Data Requirements

### Volume

**Target: 5 million quiet, labeled positions** from at least 15,000 games.

More data is better for avoiding overfitting when tuning ~85+ parameters. 2M is the minimum
for stable results; 5–10M is the sweet spot.

### Sources (ranked by preference)

1. **FrankyCPP self-play** (best option)
   - Fast time control: 1s + 0.01s increment per move
   - ~10,000–15,000 games → ~3–5M positions after filtering
   - Best because eval biases match the engine's own play patterns
   - Use current best parameter set (Phase 1 values) as the starting point

2. **CCRL / CEGT PGN archives**
   - Games from engines in the ~2000–2500 Elo range
   - Avoid Stockfish/Leela games (NNUE-dominated play patterns may bias classical eval tuning)
   - Good for supplementing self-play data

3. **Lichess open database**
   - Filter for 2000+ rated players, standard time controls
   - Large volume available, but human play patterns differ from engine play
   - Use as a fallback only

### Label Format

Each position gets the game result from **White's perspective**:

| PGN Result | Label |
|------------|-------|
| `1-0`      | 1.0   |
| `1/2-1/2`  | 0.5   |
| `0-1`      | 0.0   |

### File Format

Simple text, one position per line:

```
rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2 [1.0]
r1bqkbnr/pppppppp/2n5/8/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 2 2 [0.5]
```

Alternative: EPD format with `c9` tag for result.

---

## Position Extraction Pipeline

A tool (new CLI mode in FrankyCPP or standalone utility) that reads PGN files and outputs the
labeled position dataset.

### Steps

```
For each game in PGN:
    result = parse game result header (1-0 / 0-1 / 1/2-1/2)
    position = starting position
    moveNumber = 0

    For each move in the game:
        position.doMove(move)
        moveNumber++

        // Filter 1: Skip early moves (opening theory)
        if moveNumber < 16 (8 full moves):
            continue

        // Filter 2: Skip positions in check
        if position.isInCheck():
            continue

        // Filter 3: Skip positions right after captures/promotions (not quiet)
        if move.isCapture() or move.isPromotion():
            continue

        // Filter 4: Skip trivial endgames
        if position.pieceCount() < 6:
            continue

        // Filter 5 (recommended): Quiescence resolution
        // Run qsearch from this position. If |qsearch_score - static_eval| > 150cp,
        // the position is tactically unstable — skip it.
        // This is the most impactful filter.

        // Output
        write(position.toFen(), resultLabel)
```

### Quiescence Filtering — Why It Matters

Without quiescence filtering, the dataset contains positions where pieces are hanging or exchanges
are in progress. The static eval for these positions is misleading (it doesn't account for the
imminent captures). Training on these positions teaches the tuner wrong lessons — it's like trying
to learn positional judgment from positions where tactics dominate.

**Options (from simple to thorough):**

1. **Skip post-capture positions** (Filter 3 above) — simple, catches most cases
2. **Static eval vs qsearch check** — run qsearch, skip if scores diverge significantly
3. **Use qsearch score as the eval** — most accurate but requires running qsearch for every
   position during tuning, which is much slower

**Recommendation:** Start with option 1 (simple capture filter). If results are noisy, add option 2
in a second iteration.

---

## Optimization Algorithm

### Phase A: Tune K

```
K_low = 0.5, K_high = 2.0
For 30 iterations:
    K_mid = (K_low + K_high) / 2
    K_left = (K_low + K_mid) / 2
    K_right = (K_mid + K_high) / 2
    if MSE(K_left) < MSE(K_right):
        K_high = K_mid
    else:
        K_low = K_mid
K = K_mid  // final scaling constant
```

### Phase B: Coordinate Descent (Parameter Tuning)

Coordinate descent is the simplest and most common approach for Texel tuning. It works well
for ~85 parameters and is easy to implement and debug.

```
improved = true
while improved:
    improved = false
    for each parameter P in parameter_vector:
        current_mse = compute_mse(all_positions)

        P += delta  // delta = 1 for int params
        mse_plus = compute_mse(all_positions)

        P -= 2 * delta  // try the other direction
        mse_minus = compute_mse(all_positions)

        if mse_plus < current_mse and mse_plus <= mse_minus:
            P += 2 * delta  // keep P + delta
            improved = true
        elif mse_minus < current_mse:
            // keep P - delta (already set)
            improved = true
        else:
            P += delta  // revert to original
```

Typically converges in 5–20 full passes over all parameters.

### Speed Estimate

| Metric                    | Value                                |
|---------------------------|--------------------------------------|
| Positions in dataset      | 5,000,000                            |
| Parameters                | ~85                                  |
| Eval speed                | ~7M positions/sec (single thread)    |
| Evals per pass            | 85 params × 2 directions × 5M = 850M |
| Time per pass (1 thread)  | ~120 seconds                         |
| Time per pass (4 threads) | ~30 seconds                          |
| Passes to converge        | 5–20                                 |
| **Total tuning time**     | **~3–10 minutes (4 threads)**        |

The MSE computation over N positions is embarrassingly parallel — split positions across threads.
FrankyCPP already has a `ThreadPool` that can be reused.

### Alternative: Adam Optimizer

For larger parameter sets (>200 params, e.g., when including PSTs), coordinate descent becomes slow.
Adam optimizer with numerical gradients converges faster:

```
For each mini-batch of positions:
    For each parameter P:
        gradient ≈ (MSE(P+ε) - MSE(P-ε)) / (2ε)   // numerical gradient
    Update all params using Adam update rule (momentum + adaptive learning rate)
```

**Recommendation:** Start with coordinate descent (simpler, sufficient for ~85 params). Switch to
Adam only if PST tuning is added later (~850 params).

---

## Parameters to Tune

### Tier 1: EvalConfigData Weights (~85 params) — Primary Target

All `CONFIG_CONST int` fields in `EvalConfigData.h` that represent eval weights:

| Category                   | Parameters                                                                                                                                                |   Count |
|----------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------|--------:|
| **Pawn structure**         | ISOLATED_PAWN_MID/END, DOUBLED_PAWN_MID/END, PASSED_PAWN_MID/END, BLOCKED_PAWN_MID/END, PHALANX_PAWN_MID/END, SUPPORTED_PAWN_MID/END                      |      12 |
| **Passed pawn rank bonus** | PASSED_PAWN_RANK_MID_BONUS[6], PASSED_PAWN_RANK_END_BONUS[6]                                                                                              |      12 |
| **Pawn advance**           | PAWN_ADVANCE_MID_BONUS[4], PAWN_ADVANCE_END_BONUS[4]                                                                                                      |       8 |
| **Bishop pair**            | BISHOP_PAIR_MID/END_BONUS                                                                                                                                 |       2 |
| **Knight**                 | KNIGHT_MOBILITY_MID/END_PER_MOVE, LOW_MOBILITY (×2 tiers ×2 phases), OUTPOST (×2 types ×2 phases)                                                         |      12 |
| **Bishop**                 | BISHOP_MOBILITY_MID/END_PER_MOVE, LOW_MOBILITY (×2 phases), BAD_BISHOP_PER_PAWN (×2 phases)                                                               |       6 |
| **Rook**                   | ROOK_MOBILITY (×2 phases), LOW_MOBILITY (×2 phases), OPEN/SEMIOPEN FILE (×2×2 phases), 7TH_RANK (×2 phases), BEHIND_PASSER (×2 types ×2 phases)           |      14 |
| **Queen**                  | QUEEN_MOBILITY (×2 phases), TROPISM (×2 phases)                                                                                                           |       4 |
| **King safety**            | SHIELD (×2 phases), PROXIMITY (×2 types), ATTACK_WEIGHT (×4 piece types), SAFETY_TABLE[16], PAWN_STORM[4], KING_OPEN/SEMIOPEN_FILE, SAFE_CHECK (×4 types) |     ~33 |
| **Threats**                | BY_PAWN (×3 victim types ×2 phases), BY_MINOR (×2 types ×2 phases), HANGING (×2 phases)                                                                   |      12 |
| **Space**                  | SPACE_BONUS_MID/END                                                                                                                                       |       2 |
| **Coordination**           | CONNECTED_ROOKS (×2 phases), MINOR_CONNECTIVITY (×2 phases)                                                                                               |       4 |
| **Misc**                   | TEMPO, LAZY_THRESHOLD                                                                                                                                     |       2 |
| **Total**                  |                                                                                                                                                           | **~85** |

### Tier 2: Piece Values (5 params) — High Impact, Small Effort

Currently `constexpr` in `value.h`:

```cpp
constexpr Value pieceTypeValue[] = {
    0,     // no type
    2000,  // king
    100,   // pawn (anchor — do not tune)
    320,   // knight
    330,   // bishop
    500,   // rook
    900,   // queen
};
```

Pawn value (100) is the anchor — all other values are relative to it. Tune: knight, bishop, rook,
queen (4 params). King value (2000) is for SEE/MVV-LVA ordering, not eval — exclude.

**Requires:** Adding 4 fields to `EvalConfigData` and using them in `evaluate()` instead of the
`constexpr` array. Small refactor.

### Tier 3: Piece-Square Tables (~768 params) — Largest Potential, Most Effort

Currently `constexpr` arrays in `Values.h` (6 piece types × 64 squares × 2 phases = 768 values).
Used incrementally by `Position::doMove()` via precomputed lookup tables.

**Requires:** Refactoring from `constexpr` to runtime-mutable storage. Significant effort because
`Position` uses these tables in the hot path (make/unmake move). Must verify no performance
regression.

**Symmetry constraint:** For non-pawn pieces, the tables should be horizontally symmetric
(a-file mirrors h-file). This halves the effective parameter count to ~448 and prevents the tuner
from learning file-specific noise.

**Recommendation:** Defer to a separate follow-up effort after Tier 1+2 tuning is validated.

### Exclude from Tuning

| Parameter type                   | Reason                                                |
|----------------------------------|-------------------------------------------------------|
| `bool USE_*` toggles             | Binary on/off, not continuous weights                 |
| `CONFIG_ESSENTIAL` params        | Infrastructure (TT size, book paths, thread count)    |
| `USE_PAWN_TT`, `PAWN_TT_SIZE_MB` | Performance config, not eval                          |
| `USE_GAMEPHASE_VALUE`            | Structural switch                                     |
| Search parameters                | Require match-based tuning (SPSA), not position-based |

---

## Implementation Plan

### Architecture

New module: `src/tuning/` with the following components:

```
src/tuning/
├── TuningDataset.h/.cpp      // Loads FEN+result file, stores positions in memory
├── TuningParameter.h          // Maps flat vector index ↔ EvalConfigData field
├── TexelTuner.h/.cpp          // Core optimization loop (K-tuning + coordinate descent)
└── PositionExtractor.h/.cpp   // PGN → FEN+result extraction tool
```

### Component Details

#### TuningDataset

```cpp
struct TuningEntry {
    Position position;   // or: a compact representation (piece list + state flags)
    float result;        // 1.0, 0.5, 0.0
};

class TuningDataset {
    std::vector<TuningEntry> entries;
public:
    void loadFromFile(const std::string& path);  // parse FEN + result per line
    size_t size() const;
    const TuningEntry& operator[](size_t i) const;
};
```

**Memory consideration:** 5M `Position` objects × ~200 bytes ≈ 1 GB. If too large, use a compact
representation (piece list, 32 bytes per position) and reconstruct `Position` on the fly during
eval. Alternatively, process in batches.

#### TuningParameter

```cpp
struct TuningParameter {
    std::string name;                    // e.g., "ISOLATED_PAWN_MID_WEIGHT"
    int* valuePtr;                       // direct pointer into EvalConfigData field
    int currentValue;
    int minValue;                        // clamp range (optional)
    int maxValue;
    int delta = 1;                       // step size for coordinate descent
};
```

Build the parameter list from `ConfigRegistry` by filtering entries with `exposure.tunable == true`.
The existing `getter`/`setter` lambdas provide type-safe access. Alternatively, since all tunable
params are `int` fields in `EvalConfigData`, build direct pointers for speed.

#### TexelTuner

```cpp
class TexelTuner {
    TuningDataset& dataset;
    std::vector<TuningParameter> params;
    EvalConfigData& config;              // mutable reference to live config
    Evaluator evaluator;                 // reused for all evals
    double K;                            // scaling constant

    double computeMSE();                 // evaluate all positions, compute error
    void tuneK();                        // binary search for optimal K
    void tuneParameters();               // coordinate descent main loop
    void printProgress(int pass, double mse);
};
```

#### PositionExtractor

Reads PGN files, replays games using `Position::doMove()`, applies filters, outputs dataset file.
Can be a standalone CLI mode: `FrankyCPP --extract-positions input.pgn output.txt`.

### CLI Integration

```
FrankyCPP --tune <dataset.txt> [--threads 4] [--output tuned_params.yaml]
FrankyCPP --extract-positions <input.pgn> <output.txt> [--min-move 16] [--qsearch-filter]
FrankyCPP --self-play <num_games> <output.pgn> [--tc 1+0.01] [--threads 4]
```

---

## Integration with FrankyCPP

### Existing Infrastructure to Leverage

1. **`ConfigExposure.tunable` flag** — Already exists in `ConfigDef.h` (line 142), set to `false`
   by default. Mark all eval weight parameters `tunable = true` in `ConfigRegistry.cpp`. The tuner
   auto-discovers tunable params by querying the registry.

2. **`ConfigManager::applyOverrides()`** — Already supports runtime mutation of `EvalConfigData`.
   The tuner can modify parameters during optimization.

3. **`ConfigRegistry` getter/setter lambdas** — Provide type-safe string-based access to every
   parameter. Useful for serializing tuned values back to YAML.

4. **`ThreadPool`** — Existing thread pool for parallelizing MSE computation.

### Required Changes

1. **Mark `tunable = true`** on all eval weight entries in `ConfigRegistry.cpp` (~85 entries).
   No functional change — just metadata.

2. **Tuning eval mode** — During tuning, certain features must be disabled:
   - **Lazy eval** (`USE_LAZY_EVAL = false`): Lazy eval short-circuits evaluation when the score
     exceeds a threshold. This masks the effect of many parameters on many positions. Must be
     disabled during tuning.
   - **Pawn TT** (`USE_PAWN_TT = false`): The pawn TT caches pawn structure eval. When parameters
     change between evaluations, cached values become stale. Disable during tuning.
   - Both can be set via `applyOverrides()` before the tuning loop.

3. **Piece values (Tier 2)** — Add 4 fields to `EvalConfigData`:
   ```cpp
   CONFIG_CONST int PIECE_VALUE_KNIGHT = 320;
   CONFIG_CONST int PIECE_VALUE_BISHOP = 330;
   CONFIG_CONST int PIECE_VALUE_ROOK   = 500;
   CONFIG_CONST int PIECE_VALUE_QUEEN  = 900;
   ```
   In `evaluate()`, use these instead of `constexpr pieceTypeValue[]` for the material score.
   In production builds, `CONFIG_CONST` makes them `constexpr` again — zero cost.

4. **Output** — After tuning, serialize optimized values to a YAML file that `ConfigManager` can
   load directly. The existing YAML infrastructure handles this.

### What Does NOT Need to Change

- `Evaluator::evaluate()` — No changes needed. The tuner modifies `EvalConfigData` fields that
  the evaluator already reads through `EvalConfig.*` references.
- `Position` — No changes for Tier 1+2 tuning. (Tier 3 PST tuning would require changes.)
- Build system — The tuning module is compiled only in non-production builds (guarded by
  `#ifndef FRANKYCPP_PRODUCTION`, same pattern as test config overrides).

---

## Validation Strategy

### Before Accepting Tuned Parameters

1. **Train/test split** — Hold out 20% of positions as a test set. Report MSE on both sets.
   If test MSE is significantly higher than train MSE → overfitting. Re-tune with more data or
   fewer parameters.

2. **Sanity checks on tuned values:**
   - Piece values reasonable: N ≈ B (within 20), R > B, Q > R + minor
   - Pawn structure: isolated/doubled penalties are negative, passed pawn bonuses are positive
   - King safety weights increase with piece attacking power (Q > R > B ≈ N)
   - No parameter has flipped sign relative to hand-tuned values (red flag for overfitting)

3. **STS regression test** — Run full STS suite (5s/move). Expect overall improvement; no
   category regression > 3 points.

4. **WAC check** — Must stay ≥ 95% (tactical sanity — eval tuning should not break tactics).

5. **Gauntlet matches** — The ultimate test. 500+ games at blitz TC via `cutechess-cli`:
   - vs FrankyCPP with original params (direct A/B test)
   - vs v1.5 baseline (absolute strength)
   - vs Stockfish classical @2700 (external reference)

6. **Incremental validation** — Tune a small subset first (e.g., just pawn structure, ~12 params).
   Validate. Then expand to the full parameter set. This catches integration bugs early and builds
   confidence in the pipeline.

### If Tuning Fails to Improve Match Strength

- Check dataset quality (enough games? good filtering? no tactical noise?)
- Try regenerating dataset with updated engine (self-play with latest params)
- Consider if some parameters interact negatively — try tuning subsets independently
- Verify lazy eval is actually disabled during tuning

---

## Risks and Pitfalls

| Risk                                             | Severity | Likelihood | Mitigation                                                                               |
|--------------------------------------------------|----------|------------|------------------------------------------------------------------------------------------|
| **Overfitting** to dataset                       | High     | Medium     | Large dataset (5M+), train/test split, validate with matches                             |
| **Dataset bias** (non-representative games)      | Medium   | Medium     | Use self-play as primary source; supplement with diverse external games                  |
| **Local minima** in optimization                 | Medium   | Low        | Multiple restarts from perturbed initial values; verify with different datasets          |
| **Quiet position filtering quality**             | High     | Medium     | Start with capture filter; add qsearch filter if results are noisy                       |
| **Lazy eval masking parameters**                 | High     | High       | Always disable lazy eval during tuning — non-negotiable                                  |
| **Pawn TT caching stale values**                 | Medium   | High       | Disable pawn TT during tuning                                                            |
| **Memory usage** (5M positions)                  | Low      | Medium     | Use compact position representation or batch processing                                  |
| **Piece value refactoring breaks things**        | Medium   | Low        | Tier 2 is a small, testable change — add unit tests                                      |
| **PST refactoring performance regression**       | Medium   | Medium     | Defer Tier 3 until Tier 1+2 is validated; benchmark carefully                            |
| **Tuned params don't transfer to different TCs** | Low      | Low        | Validate at multiple time controls; eval params are less TC-sensitive than search params |

---

## Estimated Effort

### Phase A: Infrastructure (one-time)

| Task         | Description                                                       |            Days |
|--------------|-------------------------------------------------------------------|----------------:|
| A.1          | Position extractor tool (PGN → FEN+result, with filters)          |             3–4 |
| A.2          | Self-play game generation (CLI mode or script with cutechess-cli) |             1–2 |
| A.3          | TuningDataset loader (parse FEN+result file into memory)          |               1 |
| A.4          | TuningParameter mapping (registry query → flat param vector)      |             1–2 |
| A.5          | TexelTuner core (K-tuning + coordinate descent + parallel MSE)    |             3–4 |
| A.6          | CLI integration (`--tune`, `--extract-positions`)                 |               1 |
| A.7          | Output serialization (tuned params → YAML)                        |             0.5 |
| **Subtotal** |                                                                   | **~11–14 days** |

### Phase B: Tuning Runs

| Task         | Description                                               |                                  Days |
|--------------|-----------------------------------------------------------|--------------------------------------:|
| B.1          | Generate dataset (10K+ self-play games → 5M positions)    | 1–2 (compute time, mostly unattended) |
| B.2          | Mark `tunable` flags in ConfigRegistry (~85 entries)      |                                   0.5 |
| B.3          | Initial tuning run (Tier 1: ~85 EvalConfigData params)    |                                     1 |
| B.4          | Add piece values to EvalConfigData (Tier 2: 4 params)     |                                     1 |
| B.5          | Second tuning run (Tier 1 + Tier 2: ~89 params)           |                                     1 |
| B.6          | Iterate: inspect results, adjust filters/dataset, re-tune |                                   1–2 |
| **Subtotal** |                                                           |                         **~5–7 days** |

### Phase C: Validation

| Task         | Description                                       |                             Days |
|--------------|---------------------------------------------------|---------------------------------:|
| C.1          | STS + WAC + test suite regression                 |                                1 |
| C.2          | Gauntlet matches (500+ games, multiple opponents) | 1–2 (compute, mostly unattended) |
| C.3          | Analysis, parameter review, iterate if needed     |                              1–2 |
| **Subtotal** |                                                   |                    **~3–5 days** |

### Phase D: PST Tuning (optional, separate follow-up)

| Task         | Description                                                            |           Days |
|--------------|------------------------------------------------------------------------|---------------:|
| D.1          | Refactor Values.h from constexpr to runtime-mutable                    |            2–3 |
| D.2          | Add PST params to tuner with symmetry constraints                      |            1–2 |
| D.3          | Switch to Adam optimizer (coordinate descent too slow for ~850 params) |            2–3 |
| D.4          | Tuning run + validation                                                |            2–3 |
| **Subtotal** |                                                                        | **~7–11 days** |

### Summary

| Scope                                      | Effort       | Expected Gain         |
|--------------------------------------------|--------------|-----------------------|
| **Tier 1+2 (eval weights + piece values)** | **~3 weeks** | **+20–50 ELO**        |
| Tier 3 (PSTs, optional follow-up)          | ~2 weeks     | +10–30 ELO additional |

---

## Implementation Order

Recommended sequence for the first session:

```
1. Mark tunable flags in ConfigRegistry          (B.2 — 0.5 day, sets up infrastructure)
2. Position extractor tool                       (A.1 — 3-4 days, needed for everything)
3. Self-play dataset generation                  (A.2 + B.1 — start compute, runs overnight)
4. TuningDataset + TuningParameter + TexelTuner  (A.3–A.5 — 5-6 days, core implementation)
5. CLI integration + output                      (A.6–A.7 — 1.5 days)
6. First tuning run + validation                 (B.3 + C.1 — 2 days)
7. Add piece values + retune                     (B.4–B.5 — 2 days)
8. Gauntlet matches                              (C.2–C.3 — 2-3 days)
```

---

## References

- [Texel's Tuning Method — Chessprogramming Wiki](https://www.chessprogramming.org/Texel%27s_Tuning_Method)
- [Peter Österlund's Original Post (TalkChess)](http://talkchess.com/forum3/viewtopic.php?f=7&t=50823)
- [Ethereal Tuner (src/tuner.c)](https://github.com/AndyGrant/Ethereal) — Gold standard reference implementation
- [Weiss Engine (tuner)](https://github.com/TerjeKir/weiss) — Clean, embedded C tuner
- [Optimization Algorithms — Adam](https://arxiv.org/abs/1412.6980) — For PST tuning with many params
- [SPSA Tuning (for search params)](https://www.chessprogramming.org/SPSA) — Complementary approach for search-side tuning

---

*Last updated: 2026-03-21*
