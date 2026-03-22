# FrankyCPP Texel Tuning Plan

**Document Version:** 1.3  
**Created:** 2026-03-21  
**Last Updated:** 2026-03-22  
**Status:** 🚧 In Progress (Phases 0–2 ✅)  
**Target:** FrankyCPP v1.7  
**Priority:** High (Phase 5 of Eval & Strength Improvement Plan)  
**Predecessor:** `PLAN_Eval_and_Strength_Improvement.md`

**Companion Documents:**
- `docs/specs/PLAN_Texel_Tuning_Progress.md` — Phase progress tracker (created at implementation start)
- `docs/Texel_Tuning.md` — Feature documentation (created alongside implementation)

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
10. [Reproducibility and Persistence](#reproducibility-and-persistence)
11. [Project Phases and Implementation Order](#project-phases-and-implementation-order)
12. [Risks and Pitfalls](#risks-and-pitfalls)
13. [Estimated Effort](#estimated-effort)
14. [References](#references)

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
over 5 million positions with ~85 parameters completes in minutes on modern hardware.

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
- Engine static eval E_i(params) for position i (**from White's perspective**, in centipawns)
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

### ⚠️ Eval Perspective — Critical Implementation Detail

`Evaluator::evaluate()` returns a score from the **side-to-move perspective** (via `finalEval()`
which multiplies by `p.getNextPlayer().sign()`). The dataset labels are from **White's perspective**.

The tuner must convert back to White-relative before applying the sigmoid:

```cpp
Value rawEval = evaluator.evaluate(position);
// evaluate() returns side-to-move perspective; convert to White's perspective:
Value whiteRelativeEval = (position.getNextPlayer() == WHITE) ? rawEval : -rawEval;
```

Alternatively, call the internal scoring path up to `valueFromScore()` and skip `finalEval()`.
The first approach (negate when Black-to-move to undo the side-to-move conversion) is simpler
and doesn't require exposing internals.

Getting this wrong causes the tuner to produce garbage — it's the most common Texel implementation
bug.

### Scaling Constant K

K controls how steeply eval maps to win probability. Too small → all evals map near 0.5
(underfitting). Too large → small eval differences map to extreme outcomes (overfitting to material).

K is tuned once before parameter optimization using **ternary search**:
- Fix all eval params at current values
- Narrow the interval K ∈ [0.5, 2.0] by evaluating MSE at two interior points
- Typically converges to K ≈ 1.0–1.5 for classical evals
- K is then held constant during parameter tuning

```
K_low = 0.5, K_high = 2.0
For 50 iterations:
    K_left  = K_low  + (K_high - K_low) / 3
    K_right = K_high - (K_high - K_low) / 3
    if MSE(K_left) < MSE(K_right):
        K_high = K_right
    else:
        K_low = K_left
K = (K_low + K_high) / 2  // final scaling constant
```

---

## Data Requirements

### Volume

**Target: 5 million quiet, labeled positions** from at least 15,000 games.

More data is better for avoiding overfitting when tuning ~85+ parameters. 2M is the minimum
for stable results; 5–10M is the sweet spot.

### Sources — Practical Acquisition Strategy

Generating millions of positions from self-play requires significant compute infrastructure
that may not be readily available. Below are options ranked by **practicality and speed**:

#### Option 1: Download Pre-made Tuning Datasets (Fastest — Recommended Start)

Several chess engine developers publish ready-to-use labeled datasets:

| Source                              | Description                                                | Volume          | URL / Notes                                                   |
|-------------------------------------|------------------------------------------------------------|-----------------|---------------------------------------------------------------|
| **Zurichess quiet-labeled dataset** | Quiet positions extracted from CCRL games, labeled w/d/l   | ~7.2M positions | zurichess on GitHub, `quiet-labeled.epd.gz`                   |
| **Ethereal tuning data**            | Andy Grant's dataset used for Ethereal tuner               | ~8.5M positions | Available on request via TalkChess; sometimes mirrored        |
| **Lichess elite database**          | Games from 2400+ rated players, monthly dumps              | Millions/month  | `database.lichess.org` — requires extraction + filtering      |
| **CCRL PGN archives**               | Engine games at various time controls, rated ~2000–3500    | Thousands/games | `computerchess.org.uk/ccrl` — requires extraction + filtering |

**Recommendation:** Start with a downloaded dataset (e.g., Zurichess) for immediate tuning pipeline
development and validation. This provides fast iteration on the infrastructure without waiting for
self-play compute.

#### Option 2: Self-Play with FrankyCPP (Best Quality — Requires Compute)

Self-play data is ideal because eval biases match the engine's own play patterns. However:
- At 1s + 0.01s TC, a single FrankyCPP instance plays ~1 game/minute → ~1,400 games/day
- For 15,000 games: ~10 days on 1 core, or ~2.5 days on 4 cores running 4 concurrent matches
- **Requires cutechess-cli** for reliable game management

```powershell
# Example cutechess-cli self-play command
cutechess-cli.exe `
  -engine cmd=FrankyCPP.exe name="FrankyCPP" `
  -engine cmd=FrankyCPP.exe name="FrankyCPP" `
  -each proto=uci tc=1+0.01 `
  -rounds 15000 `
  -openings file=books/8moves_v3.pgn format=pgn order=random `
  -pgnout selfplay_output.pgn `
  -concurrency 4 `
  -recover
```

**Infrastructure note:** Running this on the dev machine ties up CPU for days. Consider:
- Running overnight / over weekends on the dev machine
- Using a cloud VM (Azure/AWS spot instance, ~$0.05/hr for 4-core)
- Running on a second machine if available

#### Option 3: Self-Play with Stockfish 18 (Diverse, High Quality)

Use Stockfish 18 at a **reduced depth or fast TC** to generate high-quality game data quickly:
- SF18 at depth 8 or 0.1s/move plays much faster than FrankyCPP
- Games are of higher quality (fewer blunders → cleaner position labels)
- Caveat: SF's NNUE play patterns differ from classical HCE. The positions reached may
  emphasize patterns that FrankyCPP's eval cannot distinguish.

**Best used as:** supplemental data mixed with FrankyCPP self-play or downloaded datasets.

#### Option 4: Mixed Dataset (Recommended Final Approach)

| Component                      | Volume | Purpose                                    |
|--------------------------------|--------|--------------------------------------------|
| Downloaded dataset (Zurichess) | 3M     | Broad coverage, immediate availability     |
| FrankyCPP self-play            | 2M     | Engine-specific patterns                   |
| **Total**                      | **5M** | Balanced, diverse                          |

Start with the downloaded dataset for pipeline development, then generate self-play data in
the background for the final tuning run.

#### Development Dataset (Phase 3)

During development (before real tuning), use a **small dataset** (~50K–100K positions) for fast
iteration. This can be a random subset of a downloaded dataset or extracted from PGN files in the
`books/` directory. The small dataset validates the pipeline end-to-end in seconds rather than
minutes.

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

Alternative: EPD format with `c9` tag for result. The loader should support both formats.

---

## Position Extraction Pipeline

### PGN Library — Foundation Step (in `common/`)

The existing PGN parser in `OpeningBook::readGamesPgn()` handles PGN parsing (headers, comments,
NAGs, variations, move replay via `Position::doMove()`), but is tightly coupled to the opening book
module and discards the `[Result]` tag.

**Step 1: Extract and generalize the PGN parser into `src/common/pgn/`.**

The PGN parser is general-purpose infrastructure — it belongs in `common/`, not in any specific
module. Both the opening book and the tuning tools need it. Following the project principle:
*shared code between modules belongs in `common/`.*

```
src/common/pgn/
├── PgnParser.h/.cpp        // Core parser: streaming, game-by-game
├── PgnGame.h               // Data struct: headers, moves, result
└── PgnTypes.h              // Shared types: GameResult enum, etc.
```

Key changes from the current `OpeningBook` PGN code:
- Extract the `[Result]` header (currently ignored by the book parser)
- Return structured `PgnGame` objects instead of directly adding to book
- Support streaming (game-by-game callback) for large files
- Maintain the existing `cleanUpPgnMoveSection()` logic for robustness
- **Thoroughly test** with diverse PGN files (the `books/` directory has good test data)

**Then refactor `OpeningBook` to use the new PGN library.** This is the validation step:
if the opening book passes all its existing tests with the extracted PGN parser, the library
is correct. Only then proceed to build the tuning tools on top of it.

### Position Extractor (Separate Tool)

The position extractor is a **standalone executable** (`FrankyCPP_v1.7_Extractor`), NOT a CLI
mode of the main FrankyCPP engine. The engine executable must not know that tuning exists.

Uses the PGN library from `common/pgn/` to extract labeled positions.

```
FrankyCPP_v1.7_Extractor <input.pgn> <output.txt> [--min-move 16] [--qsearch-filter]
```

### Extraction Steps

```
For each game in PGN:
    result = parse game Result header (1-0 / 0-1 / 1/2-1/2)
    if result is unknown (*): skip game
    position = starting position (or from [FEN] header if present)
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
        // NOTE: qsearch access requires friend declaration or a thin adapter in engine/.

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

**Accessing qsearch from the extractor:** The qsearch implementation lives in `engine/Search`.
Rather than adding a dependency from the extractor to the full Search class, create a thin
adapter or use a `friend` declaration to expose only the standalone qsearch functionality
needed. Alternatively, implement a minimal capture-only search in the extractor itself
(~50 lines of code, uses `MoveGenerator` + `Evaluator` + SEE, no TT needed).

**Recommendation:** Start with option 1 (simple capture filter). If results are noisy, add option 2
in a second iteration.

---

## Optimization Algorithm

### Phase A: Tune K (Ternary Search)

```
K_low = 0.5, K_high = 2.0
For 50 iterations:
    K_left  = K_low  + (K_high - K_low) / 3
    K_right = K_high - (K_high - K_low) / 3
    if MSE(K_left) < MSE(K_right):
        K_high = K_right
    else:
        K_low = K_left
K = (K_low + K_high) / 2  // final scaling constant
```

### Phase B: Coordinate Descent (Parameter Tuning)

Coordinate descent is the simplest and most common approach for Texel tuning. It works well
for ~85 parameters and is easy to implement and debug.

```
improved = true
passNumber = 0
while improved:
    improved = false
    passNumber++
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

    log(passNumber, current_mse, params_changed_this_pass)
    save_checkpoint(passNumber, params, mse)
```

Typically converges in 5–20 full passes over all parameters.

### Incremental MSE Optimization

When testing `P ± delta` for a single parameter, only positions where that parameter has
**nonzero influence** need re-evaluation. For example, changing `ROOK_OPEN_FILE_MID_BONUS` only
affects positions with rooks on open files.

**Implementation approach — feature activation flags:**

During the initial full MSE computation, for each position record a bitset of which parameter
groups contributed to its evaluation (i.e., which code paths were active). When testing a parameter
change, only re-evaluate positions whose bitset includes that parameter's group.

```cpp
struct TuningEntry {
    Position position;
    float result;
    std::bitset<MAX_PARAM_GROUPS> activeGroups;  // which param groups affect this position
};
```

**Building the activation bitset:** During the first eval pass, instrument `Evaluator::evaluate()`
(or a tuning-specific wrapper) to record which `EvalConfig.*` fields were read. This can be done
with a simple flag per parameter group (pawn structure, knight mobility, rook files, etc.) rather
than per individual parameter — ~15 groups covers all ~85 params.

**Expected speedup:** For any single parameter, typically only 30–70% of positions are affected
(e.g., knight outpost params affect only positions with knights on outpost squares). This yields
a ~2–3x speedup per pass, which compounds over 5–20 passes.

**Recommendation:** Build the activation-flag mechanism from the start. It's a modest
implementation cost (a few hours) for a significant runtime improvement.

### Speed Estimate

| Metric                       | Value                                |
|------------------------------|--------------------------------------|
| Positions in dataset         | 5,000,000                            |
| Parameters                   | ~85                                  |
| Eval speed                   | ~7M positions/sec (single thread)    |
| Evals per pass (naive)       | 85 params × 2 directions × 5M = 850M |
| Evals per pass (incremental) | ~850M × 0.5 average = ~425M          |
| Time per pass (4 threads)    | ~15–30 seconds                       |
| Passes to converge           | 5–20                                 |
| **Total tuning time**        | **~2–10 minutes (4 threads)**        |

The MSE computation over N positions is embarrassingly parallel — split positions across threads.
FrankyCPP already has a `ThreadPool` (in `common/`) that can be reused.

**⚠️ Thread safety:** `Evaluator` has mutable member state (`score`, `kingAttackCount`,
`attackedBy`, etc.). Each worker thread **must use its own `Evaluator` instance**. The
`EvalConfigData` struct is shared read-only across all threads (only modified between MSE
computations, never during).

```cpp
// Each worker thread gets its own Evaluator
std::vector<Evaluator> threadEvaluators(numThreads);
for (auto& eval : threadEvaluators) {
    eval.setPawnTT(nullptr);  // Pawn TT disabled during tuning
}
```

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

### Eval Weights (~85 params) — Primary Target

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

### Array Parameter Handling

Several parameters are arrays (e.g., `KING_SAFETY_TABLE[16]`, `PASSED_PAWN_RANK_MID_BONUS[6]`).
Each array element becomes a **separate `TuningParameter`** in the flat parameter vector:

```
KING_SAFETY_TABLE_0, KING_SAFETY_TABLE_1, ..., KING_SAFETY_TABLE_15  → 16 params
PASSED_PAWN_RANK_MID_BONUS_0, ..., PASSED_PAWN_RANK_MID_BONUS_5     → 6 params
```

#### Monotonicity Constraints

Certain array parameters must maintain ordering to produce sensible evaluation:

| Array                           | Constraint     | Rationale                           |
|---------------------------------|----------------|-------------------------------------|
| `KING_SAFETY_TABLE[16]`         | Non-decreasing | More attackers → more danger        |
| `PASSED_PAWN_RANK_MID_BONUS[6]` | Non-decreasing | Higher rank → closer to promotion   |
| `PASSED_PAWN_RANK_END_BONUS[6]` | Non-decreasing | Higher rank → closer to promotion   |
| `PAWN_ADVANCE_MID_BONUS[4]`     | Non-decreasing | Further advanced → more valuable    |
| `PAWN_ADVANCE_END_BONUS[4]`     | Non-decreasing | Further advanced → more valuable    |
| `PAWN_STORM_MID_PENALTY[4]`     | Non-decreasing | Closer storm pawns → more dangerous |

**Enforcement during coordinate descent:** After modifying an array element, clamp to maintain
ordering relative to neighbors:

```cpp
// After changing array[i]:
if (i > 0 && array[i] < array[i-1]) array[i] = array[i-1];       // floor
if (i < N-1 && array[i] > array[i+1]) array[i] = array[i+1];     // ceiling
```

### Piece Values — Not Tuned (Rationale)

The `constexpr pieceTypeValue[]` values (P=100, N=320, B=330, R=500, Q=900) are **standard chess
piece values** that define the centipawn scale. All other eval parameters are expressed relative to
these anchors. Tuning them would:

- Shift the centipawn scale, requiring re-interpretation of all other parameters
- Potentially destabilize SEE, MVV-LVA move ordering, and futility pruning thresholds
  (all of which use `pieceTypeValue[]` directly)
- Offer minimal benefit — these values are well-established by decades of engine development

Piece values are excluded from tuning. The eval weights already include sufficient flexibility
to express material imbalances (e.g., bishop pair bonus captures B > N preference).

### Piece-Square Tables (~768 params) — Optional Future Follow-up

Currently `constexpr` arrays in `Values.h` (6 piece types × 64 squares × 2 phases = 768 values).
Used incrementally by `Position::doMove()` via precomputed lookup tables.

**Requires:** Refactoring from `constexpr` to runtime-mutable storage. Significant effort because
`Position` uses these tables in the hot path (make/unmake move). Must verify no performance
regression.

**Symmetry constraint:** For non-pawn pieces, the tables should be horizontally symmetric
(a-file mirrors h-file). This halves the effective parameter count to ~448 and prevents the tuner
from learning file-specific noise.

**Decision point:** Whether to include PSTs will be made during Phase 6 (optimizer implementation)
based on initial tuning results with eval weights alone. If the ~85-param tuning shows clear MSE
improvement but match results plateau, PSTs become the natural next step.

### Exclude from Tuning

| Parameter type                   | Reason                                                     |
|----------------------------------|------------------------------------------------------------|
| `constexpr pieceTypeValue[]`     | Centipawn anchors; affect SEE, move ordering, futility too |
| `bool USE_*` toggles             | Binary on/off, not continuous weights                      |
| `CONFIG_ESSENTIAL` params        | Infrastructure (TT size, book paths, thread count)         |
| `USE_PAWN_TT`, `PAWN_TT_SIZE_MB` | Performance config, not eval                               |
| `USE_GAMEPHASE_VALUE`            | Structural switch                                          |
| Search parameters                | Require match-based tuning (SPSA), not position-based      |

---

## Implementation Plan

### Design Principles

1. **Complete separation from the engine.** The FrankyCPP engine executable must not know that
   tuning exists. No `--tune` or `--extract-positions` flags on the engine CLI. Tuning tools are
   **separate executables** that link against `FrankyCPPlib` (the static library) for access to
   `Position`, `Evaluator`, `ConfigManager`, etc.

2. **Shared code goes in `common/`.** Any code needed by multiple modules (engine, opening book,
   tuning) lives in `src/common/`. The PGN parser is the prime example.

3. **Tuning module is self-contained.** Like `engine_arena/`, the tuning code lives in its own
   directory with its own `main()`. It depends on `FrankyCPPlib` but nothing depends on it.

4. **Friend access for engine internals.** If the extractor or tuner needs access to engine
   internals (e.g., standalone qsearch), use `friend` declarations or thin adapter functions
   rather than making internals public.

### Module Architecture

```
src/
├── common/
│   ├── pgn/                       // ← NEW: Reusable PGN library (shared infrastructure)
│   │   ├── PgnParser.h/.cpp       // Core parser: streaming, game-by-game
│   │   ├── PgnGame.h              // Data struct: headers, moves, result
│   │   └── PgnTypes.h             // GameResult enum, shared types
│   ├── Logging.h/.cpp             // (existing)
│   ├── ThreadPool.h/.cpp          // (existing)
│   └── ...                        // (existing common utilities)
├── openingbook/
│   └── OpeningBook.cpp            // ← REFACTORED: uses common/pgn/ instead of inline parsing
├── tuning/                        // ← NEW: Tuning module (separate from engine)
│   ├── extractor/                 // Position extraction tool
│   │   ├── PositionExtractor.h/.cpp
│   │   └── ExtractorMain.cpp      // main() for FrankyCPP_v1.7_Extractor executable
│   ├── optimizer/                 // Texel tuning optimizer
│   │   ├── TuningParameter.h/.cpp
│   │   ├── TuningDataset.h/.cpp
│   │   ├── TuningEntry.h
│   │   ├── TexelTuner.h/.cpp
│   │   ├── TuningState.h/.cpp
│   │   └── TunerMain.cpp          // main() for FrankyCPP_v1.7_Tuner executable
│   └── README.md                  // Module documentation
└── CMakeLists.txt                 // Updated: adds Extractor + Tuner executables
```

### Build Targets

Following the pattern established by `engine_arena/`:

```cmake
# In src/CMakeLists.txt:

# Position Extractor executable
set(extractorExeName ${exeName}_Extractor)
file(GLOB SRCS_EXTRACTOR CONFIGURE_DEPENDS tuning/extractor/*.cpp tuning/extractor/*.h)
add_executable(${extractorExeName} ${SRCS_EXTRACTOR})
target_link_libraries(${extractorExeName} PRIVATE FrankyCPPlib Boost::program_options)

# Texel Tuner executable
set(tunerExeName ${exeName}_Tuner)
file(GLOB SRCS_TUNER CONFIGURE_DEPENDS tuning/optimizer/*.cpp tuning/optimizer/*.h)
add_executable(${tunerExeName} ${SRCS_TUNER})
target_link_libraries(${tunerExeName} PRIVATE FrankyCPPlib yaml-cpp::yaml-cpp Boost::program_options)
```

Both tools link against `FrankyCPPlib` for access to `Position`, `MoveGenerator`, `Evaluator`,
`ConfigManager`, etc. They do NOT link to each other — they are independent executables.

Production builds (`-DFRANKYCPP_PRODUCTION`) exclude the tuning targets entirely.

### Component Details

#### PGN Library (`src/common/pgn/`)

Extracted and generalized from `OpeningBook::readGamesPgn()`:

```cpp
namespace common::pgn {

  enum class GameResult { WHITE_WIN, DRAW, BLACK_WIN, UNKNOWN };

  struct PgnGame {
      std::unordered_map<std::string, std::string> headers;
      std::vector<std::string> moves;          // SAN move strings
      GameResult result = GameResult::UNKNOWN;
  };

  class PgnParser {
  public:
      // Stream-parse: calls callback for each complete game (memory-efficient for large files)
      void parse(const std::string& filePath, std::function<void(PgnGame&&)> gameCallback);

      // Batch-parse: returns all games (simpler API, higher memory)
      std::vector<PgnGame> parseAll(const std::string& filePath);
  };

} // namespace common::pgn
```

**Reuses:** The `cleanUpPgnMoveSection()` logic, comment/NAG stripping, game boundary detection
from the existing `OpeningBook` code.

**Adds:** `[Result]` header extraction, structured output, streaming API.

**Testing:** Thorough test suite using existing PGN files in `books/` directory (diverse formats,
edge cases). This is a critical foundation component — invest in comprehensive tests before building
on top of it.

**Validation:** Refactor `OpeningBook` to use the new PGN library. All existing `OpeningBookTest`
tests must pass unchanged. This proves the library is a correct extraction.

#### TuningEntry and TuningDataset

```cpp
struct TuningEntry {
    Position position;                                 // full Position object from FEN
    float result;                                      // 1.0, 0.5, 0.0
    std::bitset<NUM_PARAM_GROUPS> activeParamGroups;   // for incremental MSE optimization
};

class TuningDataset {
    std::vector<TuningEntry> entries;
public:
    void loadFromFile(const std::string& path);   // parse FEN + result per line
    void computeActivationFlags(Evaluator& eval); // one-time: record which params affect each pos
    std::pair<TuningDataset, TuningDataset> split(float trainFraction = 0.8f) const;
    size_t size() const;
    const TuningEntry& operator[](size_t i) const;
};
```

**Memory estimate:** `Position` is ~300–400 bytes (15 bitboards, piece array, state fields). 5M
entries × ~400 bytes ≈ **~2 GB**. This fits in memory on a 16 GB dev machine. If memory is tight,
use batch processing or a compact 32-byte piece-list representation with on-demand `Position`
reconstruction.

#### TuningParameter

```cpp
struct TuningParameter {
    std::string name;                    // e.g., "ISOLATED_PAWN_MID_WEIGHT"
    int* valuePtr;                       // direct pointer into EvalConfigData field
    int originalValue;                   // starting value, for comparison / reset
    int currentValue;
    int minValue;                        // clamp range
    int maxValue;
    int delta = 1;                       // step size for coordinate descent
    int paramGroup;                      // group index for activation bitset
    int arrayIndex = -1;                 // -1 for scalars, 0..N for array elements
    MonotonicityConstraint monotonicity = MonotonicityConstraint::NONE;
};

enum class MonotonicityConstraint { NONE, NON_DECREASING, NON_INCREASING };
```

Build the parameter list from `ConfigRegistry` by filtering entries with `exposure.tunable == true`.
For array parameters, create one `TuningParameter` per element with appropriate `arrayIndex` and
monotonicity constraints.

#### TexelTuner

```cpp
class TexelTuner {
    TuningDataset& trainSet;
    TuningDataset* testSet = nullptr;        // optional, for overfitting detection
    std::vector<TuningParameter> params;
    EvalConfigData& config;                  // mutable reference to live config
    std::vector<Evaluator> threadEvaluators;  // one per worker thread — mandatory
    double K;                                // scaling constant

    double computeMSE(const TuningDataset& dataset);   // parallel over all positions
    double computeMSEIncremental(                       // only positions affected by paramGroup
        const TuningDataset& dataset, int paramGroup);
    void tuneK();                            // ternary search for optimal K
    void tuneParameters();                   // coordinate descent main loop
    void enforceMonotonicity(TuningParameter& param); // array constraint enforcement
    void printProgress(int pass, double trainMSE, double testMSE);
    void saveCheckpoint(int pass);           // persist state for resumability
    void loadCheckpoint(const std::string& path); // resume from saved state
};
```

#### TuningState (Persistence)

```cpp
struct TuningState {
    int completedPasses;
    double bestMSE;
    double K;
    std::vector<std::pair<std::string, int>> paramValues;  // name → value pairs
    std::string datasetPath;
    std::string timestamp;

    void saveToYaml(const std::string& path) const;
    static TuningState loadFromYaml(const std::string& path);
};
```

### Standalone Executables — CLI Design

#### FrankyCPP_v1.7_Extractor

```
FrankyCPP_v1.7_Extractor <input.pgn> <output.txt> [options]

Options:
  --min-move <N>        Skip first N half-moves (default: 16)
  --min-pieces <N>      Skip positions with fewer than N pieces (default: 6)
  --qsearch-filter      Enable qsearch stability filter
  --qsearch-threshold   Threshold in cp for qsearch filter (default: 150)
  --help                Show usage
```

#### FrankyCPP_v1.7_Tuner

```
FrankyCPP_v1.7_Tuner <dataset.txt> [options]

Options:
  --threads <N>         Worker threads for parallel MSE (default: 4)
  --output <file>       Output YAML with tuned params (default: tuned_params.yaml)
  --resume <file>       Resume from checkpoint YAML
  --test-split <frac>   Fraction for test set (default: 0.2)
  --help                Show usage
```

---

## Integration with FrankyCPP

### Existing Infrastructure to Leverage

1. **`ConfigExposure.tunable` flag** — Already exists in `ConfigDef.h`, set to `false`
   by default. Mark all eval weight parameters `tunable = true` in `ConfigRegistry.cpp`. The tuner
   auto-discovers tunable params by querying the registry.

2. **`ConfigManager::applyOverrides()`** — Already supports runtime mutation of `EvalConfigData`.
   The tuner can modify parameters during optimization.

3. **`ConfigRegistry` getter/setter lambdas** — Provide type-safe string-based access to every
   parameter. Useful for serializing tuned values back to YAML.

4. **`ThreadPool`** — Existing thread pool in `common/` for parallelizing MSE computation.

### Required Changes to Existing Code

1. **Mark `tunable = true`** on all eval weight entries in `ConfigRegistry.cpp` (~85 entries).
   No functional change — just metadata.

2. **Extract PGN parser to `common/pgn/`** and refactor `OpeningBook` to use it. This is a
   refactoring of existing code, not a new feature. All existing tests must pass.

3. **Add tuning build targets** to `src/CMakeLists.txt` (Extractor + Tuner executables).
   Guarded by `if(NOT FRANKYCPP_PRODUCTION)`.

### Tuning Eval Mode

The tuner must adjust eval configuration before running. These overrides are applied within the
tuner executable itself, not in the engine:

| Setting                  | Tuning Value | Reason                                                           |
|--------------------------|--------------|------------------------------------------------------------------|
| `USE_LAZY_EVAL`          | `false`      | Lazy eval short-circuits, masking parameter effects              |
| `USE_PAWN_TT`            | `false`      | Cached pawn evals become stale when params change                |
| `USE_SPACE_EVAL`         | `true`       | Must enable to tune SPACE_BONUS weights                          |
| `USE_CONNECTED_ROOKS`    | `true`       | Must enable to tune CONNECTED_ROOKS weights                      |
| `USE_MINOR_CONNECTIVITY` | `true`       | Must enable to tune MINOR_CONNECTIVITY weights                   |

**All currently-disabled eval features must be re-enabled during tuning** so their weights can
be optimized. If the tuner drives a weight to zero, the feature can be disabled with confidence.
If it finds a nonzero optimum, the feature is worth keeping enabled.

### Output

After tuning, serialize optimized values to a YAML file that `ConfigManager` can load directly.
The existing YAML infrastructure handles this. The tuned `config/eval.yaml` can be dropped into
the engine's config directory and used immediately.

### What Does NOT Change in the Engine

- `Evaluator::evaluate()` — No changes. The tuner modifies `EvalConfigData` fields that the
  evaluator already reads through `EvalConfig.*` references.
- `Position` — No changes. (PST tuning would require changes — deferred.)
- `value.h` / `pieceTypeValue[]` — No changes. Piece values are not tuned.
- `main.cpp` — **No changes.** No tuning CLI flags added to the engine.
- `UciHandler` — No changes. The engine knows nothing about tuning.

---

## Validation Strategy

### Baseline Measurements (Before Tuning)

Before any tuning, record these baselines for comparison:

1. **MSE with current hand-tuned params** — This is the starting point the tuner must improve upon.
2. **MSE with all weights zeroed** — Sanity check (should be much higher than baseline; confirms
   the error function is working correctly).
3. **STS score with current params** — The benchmark to beat.
4. **WAC score with current params** — Must not regress.

### Versioning Strategy

**Finish v1.6 first, then do Texel tuning as v1.7.**

- Complete v1.6 with current hand-tuned parameters (establish the release baseline)
- Create v1.7 branch, bump version number
- v1.7 is **Texel tuning only** — no other feature work mixed in
- Keep v1.6 release binary as the reference opponent for gauntlet matches
- The v1.6 → v1.7 delta is the clean, measurable Texel tuning gain

This provides:
- A clean A/B comparison (v1.6 hand-tuned vs v1.7 Texel-tuned)
- A reliable fallback if tuning produces unexpected regressions
- Clear version history documenting the improvement source

### Before Accepting Tuned Parameters

1. **Train/test split** — Hold out 20% of positions as a test set. Report MSE on both sets.
   If test MSE is significantly higher than train MSE → overfitting. Re-tune with more data or
   fewer parameters.

2. **Sanity checks on tuned values:**
   - Pawn structure: isolated/doubled penalties are negative, passed pawn bonuses are positive
   - King safety weights increase with piece attacking power (Q > R > B ≈ N)
   - No parameter has flipped sign relative to hand-tuned values (red flag for overfitting)
   - Array parameters maintain monotonicity (rank bonuses increase with rank)

3. **STS regression test** — Run full STS suite (5s/move). Expect overall improvement; no
   category regression > 3 points.

4. **WAC check** — Must stay ≥ 95% (tactical sanity — eval tuning should not break tactics).

5. **Gauntlet matches** — The ultimate test. 500+ games at blitz TC via `cutechess-cli`:
   - vs FrankyCPP v1.6 (direct A/B test — this is the key comparison)
   - vs Stockfish classical @2700 (external reference)

6. **Incremental validation** — Tune a small subset first (e.g., just pawn structure, ~12 params).
   Validate. Then expand to the full parameter set. This catches integration bugs early and builds
   confidence in the pipeline.

### If Tuning Fails to Improve Match Strength

- Check dataset quality (enough games? good filtering? no tactical noise?)
- Try regenerating dataset with updated engine (self-play with latest params)
- Consider if some parameters interact negatively — try tuning subsets independently
- Verify lazy eval and pawn TT are actually disabled during tuning
- Verify disabled features (space, coordination) are re-enabled during tuning
- Check eval perspective handling (White-relative vs side-to-move — see Math section)

---

## Reproducibility and Persistence

### Logging

Every tuning run must produce a detailed log:

```
[2026-03-25 14:30:00] Tuning started
[2026-03-25 14:30:00] Dataset: selfplay_5M.txt (5,123,456 positions)
[2026-03-25 14:30:00] Train/test split: 4,098,765 / 1,024,691
[2026-03-25 14:30:00] Parameters: 85 tunable
[2026-03-25 14:30:02] K-tuning: K = 1.237 (MSE = 0.08234)
[2026-03-25 14:30:02] Baseline MSE (train): 0.08234
[2026-03-25 14:30:02] Baseline MSE (test):  0.08241
[2026-03-25 14:30:02] --- Pass 1 ---
[2026-03-25 14:30:32] Pass 1 complete: 43/85 params changed, train MSE: 0.08102, test MSE: 0.08115
[2026-03-25 14:30:32] Biggest movers: KNIGHT_OUTPOST_SUPPORTED_MID: 20→25, TEMPO: 34→30
[2026-03-25 14:30:32] Checkpoint saved: tuning_checkpoint_pass1.yaml
[2026-03-25 14:31:01] --- Pass 2 ---
...
[2026-03-25 14:35:12] Converged after 12 passes. Final train MSE: 0.07891, test MSE: 0.07903
[2026-03-25 14:35:12] Results saved: tuned_params.yaml
```

Use the existing `spdlog` logging infrastructure with a dedicated `TUNING_LOG` logger.

### Checkpoint / Resume

After each complete pass, save a checkpoint file containing:
- All current parameter values (name → value mapping)
- Pass number, current MSE (train and test)
- K value, dataset path, timestamp
- Format: YAML (consistent with config system)

This enables:
- **Resuming** interrupted tuning runs (e.g., overnight runs that fail mid-way)
- **Comparing** results across tuning runs with different datasets or settings
- **Iterating** by loading a previous result as the starting point for a new run

### Result Persistence

Tuning results are saved in a structured output directory:

```
test/testsets/tuning/
├── 2026-03-25_selfplay_5M/
│   ├── tuned_params.yaml            // final tuned parameters (loadable by ConfigManager)
│   ├── tuning_log.txt               // detailed run log
│   ├── tuning_checkpoint_pass12.yaml // final checkpoint
│   ├── tuning_config.yaml           // run configuration (dataset, threads, settings)
│   └── param_comparison.txt         // side-by-side: original vs tuned values
└── 2026-04-01_mixed_7M/
    └── ...
```

The `param_comparison.txt` is a human-readable summary:

```
Parameter                        | Original | Tuned | Delta | Change%
---------------------------------|----------|-------|-------|--------
ISOLATED_PAWN_MID_WEIGHT         |      -10 |   -12 |    -2 |    +20%
KNIGHT_OUTPOST_SUPPORTED_MID     |       20 |    25 |    +5 |    +25%
SPACE_BONUS_MID                  |        3 |     0 |    -3 |  -100%  ← candidate for removal
...
```

### Deterministic Behavior

- Parameter iteration order is fixed (registry order, deterministic)
- Dataset loading order is fixed (file order, no shuffling during coordinate descent)
- Floating-point MSE computation uses consistent reduction order across threads
  (sorted partial sums to minimize floating-point non-associativity effects)

---

## Project Phases and Implementation Order

The project is organized into **8 sequential phases**, each with a clear deliverable and
gate criteria before proceeding. Each phase should be merged/committed independently.

### Phase 0: Release v1.6 and Branch v1.7 *(prerequisite)* ✅

**Goal:** Establish the baseline.

| Step | Task                                                     | Days | Status |
|------|----------------------------------------------------------|------|--------|
| 0.1  | Complete any remaining v1.6 work                         | —    | ✅      |
| 0.2  | Tag v1.6 release, keep binary as reference opponent      | 0.5  | ✅      |
| 0.3  | Create v1.7 branch, bump version number                  | 0.5  | ✅      |
| 0.4  | Create `PLAN_Texel_Tuning_Progress.md` progress document | 0.5  | ✅      |

**Gate:** ✅ v1.6 released, v1.7 branch exists with bumped version (1.7.0).

---

### Phase 1: Module Structure and PGN Library *(foundation)*

**Goal:** Build shared infrastructure. OpeningBook works with new PGN library.

| Step | Task                                                                                            | Days    |
|------|-------------------------------------------------------------------------------------------------|---------|
| 1.1  | Create directory structure: `src/common/pgn/`, `src/tuning/extractor/`, `src/tuning/optimizer/` | 0.5     |
| 1.2  | Extract PGN parser from `OpeningBook` into `src/common/pgn/PgnParser.h/.cpp`                    | 2–3     |
| 1.3  | Create `PgnGame.h`, `PgnTypes.h` with structured output + Result extraction                     | (incl.) |
| 1.4  | Write comprehensive PGN parser unit tests (`test/common/PgnParserTest.cpp`)                     | 1–2     |
| 1.5  | Refactor `OpeningBook::readGamesPgn()` to use new `common::pgn::PgnParser`                      | 1       |
| 1.6  | Verify all existing `OpeningBookTest` tests pass unchanged                                      | (incl.) |
| 1.7  | Update `src/CMakeLists.txt` — `common/pgn/` auto-discovered by FrankyCPPlib glob                | 0.5     |

**Gate:** All `OpeningBookTest` tests pass. PGN parser tests pass with all files in `books/`.

**Deliverable:** `common/pgn/` library, refactored `OpeningBook`.

**Effort:** ~4–6 days

---

### Phase 2: Tuning Build Targets *(scaffolding)*

**Goal:** Extractor and Tuner executables exist (minimal stubs), build system configured.

| Step | Task                                                                       | Days    |
|------|----------------------------------------------------------------------------|---------|
| 2.1  | Create `ExtractorMain.cpp` with stub `main()` + CLI argument parsing       | 0.5     |
| 2.2  | Create `TunerMain.cpp` with stub `main()` + CLI argument parsing           | 0.5     |
| 2.3  | Add `FrankyCPP_v1.7_Extractor` and `FrankyCPP_v1.7_Tuner` targets to CMake | 0.5     |
| 2.4  | Guard tuning targets with `if(NOT FRANKYCPP_PRODUCTION)`                   | (incl.) |
| 2.5  | Verify both executables build, link, and print `--help`                    | 0.5     |
| 2.6  | Create `src/tuning/README.md` with module documentation                    | 0.5     |

**Gate:** Both executables compile and run `--help`. Engine executable unaffected.

**Deliverable:** Build system with 3 executables (engine, extractor, tuner).

**Effort:** ~2–3 days

---

### Phase 3: Data Collection *(development dataset)*

**Goal:** Have a dataset ready for development and testing (small + full).

| Step | Task                                                                         | Days |
|------|------------------------------------------------------------------------------|------|
| 3.1  | Download Zurichess quiet-labeled dataset (or similar)                        | 0.5  |
| 3.2  | Create a small dev subset (~50K–100K positions) for fast iteration           | 0.5  |
| 3.3  | Start FrankyCPP self-play generation in background (cutechess-cli script)    | 0.5  |
| 3.4  | Document dataset sources and locations in `test/testsets/tuning/`             | 0.5  |

**Gate:** Dev dataset and full downloaded dataset available in `test/testsets/tuning/`.

**Deliverable:** `test/testsets/tuning/dev_50k.txt`, `test/testsets/tuning/zurichess_7M.txt`

**Effort:** ~1–2 days (self-play runs in background, not blocking)

---

### Phase 4: Position Extractor *(complete tool)*

**Goal:** Working extractor that produces labeled datasets from PGN files.

| Step | Task                                                                        | Days |
|------|-----------------------------------------------------------------------------|------|
| 4.1  | Implement `PositionExtractor` class: PGN → FEN+result with filters 1–4      | 2–3  |
| 4.2  | Wire up `ExtractorMain.cpp` with full CLI (input PGN, output file, options) | 0.5  |
| 4.3  | Write extractor unit tests (filter behavior, edge cases, output format)     | 1    |
| 4.4  | *(Optional)* Add qsearch filter (Filter 5) — requires engine access         | 1–2  |
| 4.5  | Extract positions from `books/superbook.pgn` as validation                  | 0.5  |
| 4.6  | Compare extracted dataset quality with downloaded dataset (spot checks)     | 0.5  |

**Gate:** Extractor produces valid FEN+result files. Unit tests pass. Output matches expected format.

**Deliverable:** Working `FrankyCPP_v1.7_Extractor` executable.

**Effort:** ~4–6 days

---

### Phase 5: Mark Tunable Parameters *(engine-side prep)*

**Goal:** ConfigRegistry knows which parameters are tunable. Baselines recorded.

| Step | Task                                                                         | Days |
|------|------------------------------------------------------------------------------|------|
| 5.1  | Mark `tunable = true` on all ~85 eval weight entries in `ConfigRegistry.cpp` | 0.5  |
| 5.2  | Add unit test: verify expected number of tunable params discovered           | 0.5  |
| 5.3  | Record baseline MSE, STS, WAC scores with current v1.6 params                | 0.5  |

**Gate:** Tunable flag set, test passes, baselines documented.

**Deliverable:** Updated `ConfigRegistry.cpp`, baseline measurements.

**Effort:** ~1–2 days

---

### Phase 6: Optimizer Implementation *(core tuner)*

**Goal:** Working Texel tuner that can optimize parameters and produce output.

| Step | Task                                                                         | Days |
|------|------------------------------------------------------------------------------|------|
| 6.1  | Implement `TuningDataset` loader (FEN+result parsing, train/test split)      | 1    |
| 6.2  | Implement `TuningParameter` mapping (registry query → flat param vector)     | 1–2  |
| 6.3  | Implement `TexelTuner` core: sigmoid, MSE computation, K-tuning              | 1–2  |
| 6.4  | Implement coordinate descent loop with parallel MSE                          | 2–3  |
| 6.5  | Implement incremental MSE optimization (activation flags per param group)    | 1    |
| 6.6  | Implement monotonicity constraint enforcement for array parameters           | 0.5  |
| 6.7  | Implement `TuningState` checkpoint save/load (YAML)                          | 1    |
| 6.8  | Wire up `TunerMain.cpp` with full CLI (dataset, threads, output, resume)     | 0.5  |
| 6.9  | Implement output: tuned params YAML, comparison report                       | 0.5  |
| 6.10 | Write comprehensive unit tests for each component                            | 2–3  |
| 6.11 | **Decision point:** Evaluate initial results; decide on PST tuning scope     | —    |

**Gate:** Tuner runs end-to-end on dev dataset. Checkpoint save/resume works.
Output YAML loadable by `ConfigManager`.

**Deliverable:** Working `FrankyCPP_v1.7_Tuner` executable with full functionality.

**Effort:** ~10–14 days

---

### Phase 7: Integration Testing and Data Refinement

**Goal:** Full end-to-end validation. Dataset quality confirmed. Tuning produces real improvement.

| Step | Task                                                                         | Days |
|------|------------------------------------------------------------------------------|------|
| 7.1  | First real tuning run on full dataset (~5M positions)                        | 0.5  |
| 7.2  | Inspect tuned parameters: sanity checks, sign checks, magnitude review       | 0.5  |
| 7.3  | Load tuned params into engine, run STS + WAC regression tests                | 1    |
| 7.4  | Debug any issues (eval perspective, lazy eval, pawn TT, disabled features)   | 1–2  |
| 7.5  | Mix in self-play data (if generated by now), retune                          | 1    |
| 7.6  | Iterate: adjust filters, try subset tuning, compare datasets                 | 1–2  |
| 7.7  | Collect additional self-play data if needed                                  | (bg) |

**Gate:** Tuned params pass all sanity checks. STS improvement visible.

**Deliverable:** Validated set of tuned parameters.

**Effort:** ~4–6 days

---

### Phase 8: Gauntlet Validation and Release

**Goal:** Confirm ELO improvement via matches. Update config and documentation.

| Step | Task                                                             | Days |
|------|------------------------------------------------------------------|------|
| 8.1  | Gauntlet matches: 500+ games vs v1.6 via cutechess-cli           | 1–2  |
| 8.2  | Gauntlet matches: vs Stockfish classical @2700                   | 1    |
| 8.3  | If regression: debug, adjust dataset/params, repeat from Phase 7 | 1–2  |
| 8.4  | Update `config/eval.yaml` with final tuned parameters            | 0.5  |
| 8.5  | Update `docs/Texel_Tuning.md` documentation                      | 0.5  |
| 8.6  | Update `PLAN_Texel_Tuning_Progress.md` with final status         | 0.5  |
| 8.7  | Release v1.7                                                     | 0.5  |

**Gate:** Measurable ELO improvement over v1.6. No STS/WAC regressions.

**Deliverable:** v1.7 release with Texel-tuned eval parameters.

**Effort:** ~3–5 days

---

### Phase Summary

| Phase     | Name                               | Effort          | Cumulative | Status        |
|-----------|------------------------------------|-----------------|------------|---------------|
| 0         | Release v1.6, branch v1.7          | ~1 day          | 1 day      | ✅ Complete    |
| 1         | Module structure + PGN library     | ~4–6 days       | 5–7 days   | ✅ Complete    |
| 2         | Tuning build targets (scaffolding) | ~2–3 days       | 7–10 days  | ✅ Complete    |
| 3         | Data collection                    | ~1–2 days       | 8–12 days  | ⬚ Not Started |
| 4         | Position extractor                 | ~4–6 days       | 12–18 days | ⬚ Not Started |
| 5         | Mark tunable params                | ~1–2 days       | 13–20 days | ⬚ Not Started |
| 6         | Optimizer implementation           | ~10–14 days     | 23–34 days | ⬚ Not Started |
| 7         | Integration testing                | ~4–6 days       | 27–40 days | ⬚ Not Started |
| 8         | Gauntlet + release                 | ~3–5 days       | 30–45 days | ⬚ Not Started |
| **Total** |                                    | **~30–45 days** |            |               |

### Documentation Requirements

Throughout all phases:

1. **`docs/specs/PLAN_Texel_Tuning_Progress.md`** — Created at Phase 0. Updated at each phase
   completion with:
   - ✅ Completed phases/tasks with dates
   - Current status and next steps
   - Issues encountered and decisions made
   - Brief notes (1–2 lines per task) so another session can continue

2. **`docs/Texel_Tuning.md`** — Feature documentation. Created during Phase 6, finalized at
   Phase 8. Covers:
   - How to use the extractor and tuner executables
   - Dataset format and sources
   - Configuration for tuning runs
   - Interpreting results

3. **`src/tuning/README.md`** — Module-level documentation. Created at Phase 2.

4. **Unit tests** — Every component gets tests. Test files mirror source structure:
   - `test/common/PgnParserTest.cpp`
   - `test/tuning/PositionExtractorTest.cpp`
   - `test/tuning/TuningDatasetTest.cpp`
   - `test/tuning/TuningParameterTest.cpp`
   - `test/tuning/TexelTunerTest.cpp`

---

## Risks and Pitfalls

| Risk                                             | Severity | Likelihood | Mitigation                                                                               |
|--------------------------------------------------|----------|------------|------------------------------------------------------------------------------------------|
| **Overfitting** to dataset                       | High     | Medium     | Large dataset (5M+), train/test split, validate with matches                             |
| **Dataset bias** (non-representative games)      | Medium   | Medium     | Mix self-play with downloaded data; avoid single-source datasets                         |
| **Local minima** in optimization                 | Medium   | Low        | Multiple restarts from perturbed initial values; verify with different datasets          |
| **Quiet position filtering quality**             | High     | Medium     | Start with capture filter; add qsearch filter if results are noisy                       |
| **Lazy eval masking parameters**                 | High     | High       | Always disable lazy eval during tuning — non-negotiable                                  |
| **Disabled features not re-enabled**             | High     | Medium     | Tuning mode explicitly enables all eval features (space, coordination, etc.)             |
| **Pawn TT caching stale values**                 | Medium   | High       | Disable pawn TT during tuning                                                            |
| **Eval perspective bug** (STM vs White)          | High     | Medium     | Unit test: verify eval sign matches expected direction for known positions               |
| **Memory usage** (5M positions × ~400 bytes)     | Medium   | Medium     | ~2 GB; use batch processing or compact representation if memory-constrained              |
| **Array param monotonicity violated**            | Medium   | Medium     | Enforce constraints after each parameter update                                          |
| **Thread safety** (shared mutable Evaluator)     | High     | Medium     | One Evaluator instance per worker thread — non-negotiable                                |
| **PST refactoring performance regression**       | Medium   | Medium     | Defer PST tuning until eval weight tuning is validated; benchmark carefully              |
| **Tuned params don't transfer to different TCs** | Low      | Low        | Validate at multiple time controls; eval params are less TC-sensitive than search params |
| **Dataset too slow to generate via self-play**   | Medium   | High       | Start with downloaded dataset; generate self-play data in background                     |
| **PGN library refactor breaks OpeningBook**      | Medium   | Low        | Existing OpeningBook tests are the validation gate for Phase 1                           |

---

## Estimated Effort

### Summary

| Scope                              | Effort              | Expected Gain         |
|------------------------------------|---------------------|-----------------------|
| **Eval weights (~85 params)**      | **~30–45 days**     | **+20–50 ELO**        |
| PSTs (optional follow-up, Phase D) | ~2 weeks additional | +10–30 ELO additional |

### Optional Phase D: PST Tuning (separate follow-up)

If decided during Phase 6.11:

| Task         | Description                                                            |           Days |
|--------------|------------------------------------------------------------------------|---------------:|
| D.1          | Refactor Values.h from constexpr to runtime-mutable                    |            2–3 |
| D.2          | Add PST params to tuner with symmetry constraints                      |            1–2 |
| D.3          | Switch to Adam optimizer (coordinate descent too slow for ~850 params) |            2–3 |
| D.4          | Tuning run + validation                                                |            2–3 |
| **Subtotal** |                                                                        | **~7–11 days** |

---

## References

- [Texel's Tuning Method — Chessprogramming Wiki](https://www.chessprogramming.org/Texel%27s_Tuning_Method)
- [Peter Österlund's Original Post (TalkChess)](http://talkchess.com/forum3/viewtopic.php?f=7&t=50823)
- [Ethereal Tuner (src/tuner.c)](https://github.com/AndyGrant/Ethereal) — Gold standard reference implementation
- [Weiss Engine (tuner)](https://github.com/TerjeKir/weiss) — Clean, embedded C tuner
- [Zurichess Tuning Data](https://bitbucket.org/zurichess/tuner/src/master/) — Pre-made quiet-labeled dataset
- [Optimization Algorithms — Adam](https://arxiv.org/abs/1412.6980) — For PST tuning with many params
- [SPSA Tuning (for search params)](https://www.chessprogramming.org/SPSA) — Complementary approach for search-side tuning

---

*Last updated: 2026-03-22*
