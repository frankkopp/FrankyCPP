# FrankyCPP Texel Tuning — Phase Progress Tracker

**Plan Document:** `docs/specs/PLAN_Texel_Tuning.md`  
**Created:** 2026-03-22  
**Last Updated:** 2026-03-22  
**Target Version:** v1.7  

---

## Phase Summary

| Phase | Name                               | Status        | Started | Completed  |
|-------|------------------------------------|---------------|---------|------------|
| 0     | Release v1.6, branch v1.7          | ✅ Complete    | —       | 2026-03-22 |
| 1     | Module structure + PGN library     | ⬚ Not Started |         |            |
| 2     | Tuning build targets (scaffolding) | ⬚ Not Started |         |            |
| 3     | Data collection                    | ⬚ Not Started |         |            |
| 4     | Position extractor                 | ⬚ Not Started |         |            |
| 5     | Mark tunable parameters            | ⬚ Not Started |         |            |
| 6     | Optimizer implementation           | ⬚ Not Started |         |            |
| 7     | Integration testing                | ⬚ Not Started |         |            |
| 8     | Gauntlet validation + release      | ⬚ Not Started |         |            |

---

## Phase 0: Release v1.6 and Branch v1.7 ✅

| Step | Task                                                     | Status     |
|------|----------------------------------------------------------|------------|
| 0.1  | Complete any remaining v1.6 work                         | ✅ Complete |
| 0.2  | Tag v1.6 release, keep binary as reference opponent      | ✅ Complete |
| 0.3  | Create v1.7 branch, bump version number                  | ✅ Complete |
| 0.4  | Create `PLAN_Texel_Tuning_Progress.md` progress document | ✅ Complete |

**Gate:** ✅ v1.6 released, v1.7 branch exists with version 1.7.0.

**Notes:**
- v1.6 release complete, binary available as reference opponent for gauntlet matches
- v1.7 branch created, `CMakeLists.txt` version set to 1.7.0
- Progress tracker created (this document)

---

## Phase 1: Module Structure and PGN Library ⬚

| Step | Task                                                                                            | Status        |
|------|-------------------------------------------------------------------------------------------------|---------------|
| 1.1  | Create directory structure: `src/common/pgn/`, `src/tuning/extractor/`, `src/tuning/optimizer/` | ⬚ Not Started |
| 1.2  | Extract PGN parser from `OpeningBook` into `src/common/pgn/PgnParser.h/.cpp`                    | ⬚ Not Started |
| 1.3  | Create `PgnGame.h`, `PgnTypes.h` with structured output + Result extraction                     | ⬚ Not Started |
| 1.4  | Write comprehensive PGN parser unit tests (`test/common/PgnParserTest.cpp`)                     | ⬚ Not Started |
| 1.5  | Refactor `OpeningBook::readGamesPgn()` to use new `common::pgn::PgnParser`                      | ⬚ Not Started |
| 1.6  | Verify all existing `OpeningBookTest` tests pass unchanged                                      | ⬚ Not Started |
| 1.7  | Update `src/CMakeLists.txt` — `common/pgn/` auto-discovered by FrankyCPPlib glob                | ⬚ Not Started |

**Gate:** All `OpeningBookTest` tests pass. PGN parser tests pass with all files in `books/`.

---

## Phase 2: Tuning Build Targets ⬚

| Step | Task                                                                    | Status        |
|------|-------------------------------------------------------------------------|---------------|
| 2.1  | Create `ExtractorMain.cpp` with stub `main()` + CLI argument parsing    | ⬚ Not Started |
| 2.2  | Create `TunerMain.cpp` with stub `main()` + CLI argument parsing        | ⬚ Not Started |
| 2.3  | Add `FrankyCPP_v1.7_Extractor` and `FrankyCPP_v1.7_Tuner` CMake targets | ⬚ Not Started |
| 2.4  | Guard tuning targets with `if(NOT FRANKYCPP_PRODUCTION)`                | ⬚ Not Started |
| 2.5  | Verify both executables build, link, and print `--help`                 | ⬚ Not Started |
| 2.6  | Create `src/tuning/README.md` with module documentation                 | ⬚ Not Started |

**Gate:** Both executables compile and run `--help`. Engine executable unaffected.

---

## Phase 3: Data Collection ⬚

| Step | Task                                                                 | Status        |
|------|----------------------------------------------------------------------|---------------|
| 3.1  | Download Zurichess quiet-labeled dataset (or similar)                | ⬚ Not Started |
| 3.2  | Create a small dev subset (~50K–100K positions) for fast iteration   | ⬚ Not Started |
| 3.3  | Start FrankyCPP self-play generation in background (cutechess-cli)   | ⬚ Not Started |
| 3.4  | Document dataset sources and locations in `results/tuning/README.md` | ⬚ Not Started |

**Gate:** Dev dataset and full downloaded dataset available in `results/tuning/`.

---

## Phase 4: Position Extractor ⬚

| Step | Task                                                                    | Status        |
|------|-------------------------------------------------------------------------|---------------|
| 4.1  | Implement `PositionExtractor` class: PGN → FEN+result with filters 1–4  | ⬚ Not Started |
| 4.2  | Wire up `ExtractorMain.cpp` with full CLI                               | ⬚ Not Started |
| 4.3  | Write extractor unit tests (filter behavior, edge cases, output format) | ⬚ Not Started |
| 4.4  | *(Optional)* Add qsearch filter (Filter 5)                              | ⬚ Not Started |
| 4.5  | Extract positions from `books/superbook.pgn` as validation              | ⬚ Not Started |
| 4.6  | Compare extracted dataset quality with downloaded dataset (spot checks) | ⬚ Not Started |

**Gate:** Extractor produces valid FEN+result files. Unit tests pass.

---

## Phase 5: Mark Tunable Parameters ⬚

| Step | Task                                                                   | Status        |
|------|------------------------------------------------------------------------|---------------|
| 5.1  | Mark `tunable = true` on all ~85 eval weight entries in ConfigRegistry | ⬚ Not Started |
| 5.2  | Add unit test: verify expected number of tunable params discovered     | ⬚ Not Started |
| 5.3  | Record baseline MSE, STS, WAC scores with current v1.6 params          | ⬚ Not Started |

**Gate:** Tunable flag set, test passes, baselines documented.

---

## Phase 6: Optimizer Implementation ⬚

| Step | Task                                                                     | Status        |
|------|--------------------------------------------------------------------------|---------------|
| 6.1  | Implement `TuningDataset` loader (FEN+result parsing, train/test split)  | ⬚ Not Started |
| 6.2  | Implement `TuningParameter` mapping (registry → flat param vector)       | ⬚ Not Started |
| 6.3  | Implement `TexelTuner` core: sigmoid, MSE computation, K-tuning          | ⬚ Not Started |
| 6.4  | Implement coordinate descent loop with parallel MSE                      | ⬚ Not Started |
| 6.5  | Implement incremental MSE optimization (activation flags)                | ⬚ Not Started |
| 6.6  | Implement monotonicity constraint enforcement for array parameters       | ⬚ Not Started |
| 6.7  | Implement `TuningState` checkpoint save/load (YAML)                      | ⬚ Not Started |
| 6.8  | Wire up `TunerMain.cpp` with full CLI                                    | ⬚ Not Started |
| 6.9  | Implement output: tuned params YAML, comparison report                   | ⬚ Not Started |
| 6.10 | Write comprehensive unit tests for each component                        | ⬚ Not Started |
| 6.11 | **Decision point:** Evaluate initial results; decide on PST tuning scope | ⬚ Not Started |

**Gate:** Tuner runs end-to-end on dev dataset. Checkpoint save/resume works.

---

## Phase 7: Integration Testing ⬚

| Step | Task                                                                   | Status        |
|------|------------------------------------------------------------------------|---------------|
| 7.1  | First real tuning run on full dataset (~5M positions)                  | ⬚ Not Started |
| 7.2  | Inspect tuned parameters: sanity checks, sign checks, magnitude review | ⬚ Not Started |
| 7.3  | Load tuned params into engine, run STS + WAC regression tests          | ⬚ Not Started |
| 7.4  | Debug any issues (eval perspective, lazy eval, pawn TT, etc.)          | ⬚ Not Started |
| 7.5  | Mix in self-play data (if generated by now), retune                    | ⬚ Not Started |
| 7.6  | Iterate: adjust filters, try subset tuning, compare datasets           | ⬚ Not Started |
| 7.7  | Collect additional self-play data if needed                            | ⬚ Not Started |

**Gate:** Tuned params pass all sanity checks. STS improvement visible.

---

## Phase 8: Gauntlet Validation and Release ⬚

| Step | Task                                                             | Status        |
|------|------------------------------------------------------------------|---------------|
| 8.1  | Gauntlet matches: 500+ games vs v1.6 via cutechess-cli           | ⬚ Not Started |
| 8.2  | Gauntlet matches: vs Stockfish classical @2700                   | ⬚ Not Started |
| 8.3  | If regression: debug, adjust dataset/params, repeat from Phase 7 | ⬚ Not Started |
| 8.4  | Update `config/eval.yaml` with final tuned parameters            | ⬚ Not Started |
| 8.5  | Update `docs/Texel_Tuning.md` documentation                      | ⬚ Not Started |
| 8.6  | Update this progress document with final status                  | ⬚ Not Started |
| 8.7  | Release v1.7                                                     | ⬚ Not Started |

**Gate:** Measurable ELO improvement over v1.6. No STS/WAC regressions.

---

## Decisions Log

| Date       | Decision                                                  | Rationale                          |
|------------|-----------------------------------------------------------|------------------------------------|
| 2026-03-22 | Start Texel tuning as v1.7 (clean separation from v1.6)   | Clean A/B comparison for ELO gain  |

## Issues Log

| Date       | Issue                                                    | Resolution         |
|------------|----------------------------------------------------------|--------------------|
| 2026-03-22 | Eval perspective code snippet in plan had inverted logic | Fixed in plan v1.3 |

---

*Last updated: 2026-03-22*
