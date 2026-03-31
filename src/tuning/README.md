# FrankyCPP Tuning Module

## Overview

The tuning module provides infrastructure for **Texel tuning** — an automated method
for optimizing evaluation parameters by minimizing the mean squared error between the
engine's static evaluation predictions and actual game outcomes from a large dataset of
labeled positions.

This module is **completely separate from the main engine executable**. The engine does
not know that tuning exists. The tuning tools are standalone executables that link against
`FrankyCPPlib` for access to `Position`, `Evaluator`, `ConfigManager`, etc.

## Architecture

```
src/tuning/
├── extractor/                  # Position extraction tool
│   ├── ExtractorMain.cpp       # main() for FrankyCPP_Extractor
│   ├── PositionExtractor.h     # Extraction engine with configurable filters
│   └── PositionExtractor.cpp
├── optimizer/                  # Texel tuning optimizer
│   ├── TunerMain.cpp           # main() for FrankyCPP_Tuner
│   ├── TexelTuner.h            # Core tuner: K-tuning, coordinate descent, MSE
│   ├── TexelTuner.cpp
│   ├── TuningDataset.h         # Dataset loader with train/test split
│   ├── TuningDataset.cpp
│   ├── TuningEntry.h           # Single (FEN, result) entry
│   ├── TuningParameter.h       # Parameter mapping from ConfigRegistry
│   ├── TuningParameter.cpp
│   ├── TuningState.h           # Checkpoint save/load (YAML)
│   ├── TuningState.cpp
│   ├── TuningOutput.h          # YAML output + comparison reports
│   └── TuningOutput.cpp
└── README.md                   # This file
```

### Position Extractor (`FrankyCPP_Extractor`)

Reads PGN files and produces labeled position datasets (FEN + game result per line).
Applies configurable filters to ensure position quality:

0. **Game-level filter** — skip unknown results, time forfeits, illegal moves
1. **Early move filter** — skip positions before a configurable half-move threshold
2. **Check filter** — skip positions where the side to move is in check
3. **Capture/promotion filter** — skip positions right after captures or promotions
4. **Endgame filter** — skip trivial endgames with fewer than N pieces
5. **Qsearch stability filter** *(optional)* — skip positions where qsearch score
   diverges from static eval by more than a threshold
6. **Score contradiction filter** *(optional)* — skip positions where search score
   contradicts the game result

Uses the shared PGN library from `src/common/pgn/`.

### Texel Tuner (`FrankyCPP_Tuner`)

Reads a labeled position dataset and optimizes evaluation parameters using coordinate
descent to minimize mean squared error. Features:

- **K-tuning** — ternary search for optimal sigmoid scaling constant
- **Coordinate descent** — iterative parameter optimization with per-pass checkpointing
- **Parallel MSE** — multi-threaded error computation across evaluator pool
- **Incremental MSE** — only re-evaluate positions affected by the changed parameter
  (activation flags per parameter group)
- **Monotonicity constraints** — enforce ordering for array parameters (e.g., king safety table)
- **Checkpoint/resume** — YAML-based save/load of full tuning state for long runs
- **Train/test split** — configurable holdout set for overfitting detection
- **Output** — tuned parameters YAML (loadable by ConfigManager), side-by-side comparison
  report with delta, change%, sign-flip and zero-out flags

## Build Targets

Both executables are CMake targets that link against `FrankyCPPlib`:

| Target                      | Description              |
|-----------------------------|--------------------------|
| `FrankyCPP_v1.7_Extractor`  | Position extraction tool |
| `FrankyCPP_v1.7_Tuner`      | Texel tuning optimizer   |

**Production builds** (`-DFRANKYCPP_PRODUCTION=ON`) exclude both tuning targets and
all tuning-related tests entirely. Tuning code assigns to `CONFIG_CONST` members which
become `static constexpr` in production.

## Usage

```powershell
# Extract positions from PGN
.\FrankyCPP_v1.7_Extractor --input games.pgn --output positions.txt

# Run tuner on extracted positions
.\FrankyCPP_v1.7_Tuner --dataset positions.txt --threads 8 --output results/tuning/run1

# Resume from checkpoint after interruption
.\FrankyCPP_v1.7_Tuner --dataset positions.txt --threads 8 --output results/tuning/run1 \
  --resume results/tuning/run1_checkpoint.yaml

# Show help
.\FrankyCPP_v1.7_Extractor --help
.\FrankyCPP_v1.7_Tuner --help
```

### Tuner Output Files

For `--output results/tuning/run1`, the tuner produces:

| File                              | Description                                        |
|-----------------------------------|----------------------------------------------------|
| `run1.yaml`                       | Tuned parameters in flat-key YAML format           |
| `run1_checkpoint.yaml`            | Checkpoint after each pass (for `--resume`)        |
| `run1_comparison.txt`             | Side-by-side original vs tuned with delta & flags  |

## Dataset Format

One position per line:
```
rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2 [1.0]
r1bqkbnr/pppppppp/2n5/8/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 2 2 [0.5]
```

Results from White's perspective: `1.0` (white win), `0.5` (draw), `0.0` (black win).

## Implementation Status

See `docs/archive/PLAN_Texel_Tuning.md` for the full plan and
`docs/archive/PLAN_Texel_Tuning_Progress.md` for phase-by-phase progress tracking.

| Phase | Description                             | Status        |
|-------|-----------------------------------------|---------------|
| 0     | Release v1.6, branch v1.7               | ✅ Complete    |
| 1     | Module structure + PGN library          | ✅ Complete    |
| 2     | Build targets (scaffolding)             | ✅ Complete    |
| 3     | Data collection                         | ✅ Complete    |
| 4     | Position extractor                      | ✅ Complete    |
| 5     | Mark tunable parameters                 | ✅ Complete    |
| 6     | Optimizer implementation                | ✅ Complete    |
| 7     | Full production tuning + gauntlet       | ✅ Complete    |
| 8     | Deactivate removal candidates + re-tune | ✅ Complete    |
| 9     | Full code cleanup of dead features      | ✅ Complete    |
| 10    | Final validation + release              | ✅ Complete    |

*Last updated: 2026-03-31*
