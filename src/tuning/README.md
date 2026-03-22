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
├── extractor/              # Position extraction tool
│   └── ExtractorMain.cpp   # main() for FrankyCPP_Extractor
├── optimizer/              # Texel tuning optimizer
│   └── TunerMain.cpp       # main() for FrankyCPP_Tuner
└── README.md               # This file
```

### Position Extractor (`FrankyCPP_Extractor`)

Reads PGN files and produces labeled position datasets (FEN + game result per line).
Applies configurable filters to ensure position quality:

1. **Opening filter** — skip early moves (configurable half-move threshold)
2. **Check filter** — skip positions where side to move is in check
3. **Capture/promotion filter** — skip non-quiet positions
4. **Endgame filter** — skip trivial endgames (too few pieces)
5. **Qsearch filter** *(optional)* — skip tactically unstable positions

Uses the shared PGN library from `src/common/pgn/`.

### Texel Tuner (`FrankyCPP_Tuner`)

Reads a labeled position dataset and optimizes evaluation parameters using coordinate
descent to minimize mean squared error. Features:

- **K-tuning** — ternary search for optimal sigmoid scaling constant
- **Coordinate descent** — iterative parameter optimization
- **Parallel MSE** — multi-threaded error computation
- **Incremental MSE** — only re-evaluate positions affected by the changed parameter
- **Monotonicity constraints** — enforce ordering for array parameters
- **Checkpoint/resume** — save/load tuning state for long runs
- **Train/test split** — overfitting detection

## Build Targets

Both executables are CMake targets that link against `FrankyCPPlib`:

| Target                      | Description              |
|-----------------------------|--------------------------|
| `FrankyCPP_v1.7_Extractor`  | Position extraction tool |
| `FrankyCPP_v1.7_Tuner`      | Texel tuning optimizer   |

**Production builds** (`-DFRANKYCPP_PRODUCTION=ON`) exclude both tuning targets entirely.

## Usage

```powershell
# Extract positions from PGN
.\FrankyCPP_v1.7_Extractor --input games.pgn --output positions.txt

# Run tuner on extracted positions
.\FrankyCPP_v1.7_Tuner --dataset positions.txt --threads 4

# Show help
.\FrankyCPP_v1.7_Extractor --help
.\FrankyCPP_v1.7_Tuner --help
```

## Dataset Format

One position per line:
```
rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2 [1.0]
r1bqkbnr/pppppppp/2n5/8/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 2 2 [0.5]
```

Results from White's perspective: `1.0` (white win), `0.5` (draw), `0.0` (black win).

## Implementation Status

See `docs/specs/PLAN_Texel_Tuning.md` for the full plan and
`docs/specs/PLAN_Texel_Tuning_Progress.md` for phase-by-phase progress tracking.

| Phase | Description              | Status         |
|-------|--------------------------|----------------|
| 0     | Release v1.6, branch v1.7 | ✅ Complete     |
| 1     | PGN library              | ✅ Complete     |
| 2     | Build targets            | ✅ Complete     |
| 3     | Data collection          | ⬚ Not Started |
| 4     | Position extractor       | ⬚ Not Started |
| 5     | Mark tunable params      | ⬚ Not Started |
| 6     | Optimizer implementation | ⬚ Not Started |
| 7     | Integration testing      | ⬚ Not Started |
| 8     | Gauntlet + release       | ⬚ Not Started |
