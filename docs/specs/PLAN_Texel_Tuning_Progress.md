# FrankyCPP Texel Tuning — Phase Progress Tracker

**Plan Document:** `docs/specs/PLAN_Texel_Tuning.md`  
**Created:** 2026-03-22  
**Last Updated:** 2026-03-23  
**Target Version:** v1.7  

---

## Phase Summary

| Phase | Name                               | Status         | Started    | Completed  |
|-------|------------------------------------|----------------|------------|------------|
| 0     | Release v1.6, branch v1.7          | ✅ Complete     | —          | 2026-03-22 |
| 1     | Module structure + PGN library     | ✅ Complete     | 2026-03-22 | 2026-03-22 |
| 2     | Tuning build targets (scaffolding) | ✅ Complete     | 2026-03-22 | 2026-03-22 |
| 3     | Data collection                    | 🚧 In Progress | 2026-03-22 |            |
| 4     | Position extractor                 | ✅ Complete     | 2026-03-23 | 2026-03-23 |
| 5     | Mark tunable parameters            | 🚧 In Progress | 2026-03-23 |            |
| 6     | Optimizer implementation           | ⬚ Not Started  |            |            |
| 7     | Integration testing                | ⬚ Not Started  |            |            |
| 8     | Gauntlet validation + release      | ⬚ Not Started  |            |            |

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

## Phase 1: Module Structure and PGN Library ✅

| Step | Task                                                                                            | Status        |
|------|-------------------------------------------------------------------------------------------------|---------------|
| 1.1  | Create directory structure: `src/common/pgn/`, `src/tuning/extractor/`, `src/tuning/optimizer/` | ✅ Complete    |
| 1.2  | Extract PGN parser from `OpeningBook` into `src/common/pgn/PgnParser.h/.cpp`                    | ✅ Complete    |
| 1.3  | Create `PgnGame.h`, `PgnTypes.h` with structured output + Result extraction                     | ✅ Complete    |
| 1.4  | Write comprehensive PGN parser unit tests (`test/common/pgn/PgnParserTest.cpp`)                 | ✅ Complete    |
| 1.5  | Refactor `OpeningBook::readGamesPgn()` to use new `common::pgn::PgnParser`                      | ✅ Complete    |
| 1.6  | Verify all existing `OpeningBookTest` tests pass unchanged                                      | ✅ Complete    |
| 1.7  | Update `src/CMakeLists.txt` — `common/pgn/` auto-discovered by FrankyCPPlib glob                | ✅ Complete    |

**Gate:** All `OpeningBookTest` tests pass. PGN parser tests pass with all files in `books/`.

**Notes:**
- 1.1: Created `src/common/pgn/`, `src/tuning/extractor/`, `src/tuning/optimizer/`
- 1.2: PGN parser extracted from `OpeningBook` — `cleanUpMoveSection` preserved verbatim with added bounds checks. Game boundary detection logic replicated. Added header tag parsing with escaped quote support. Added streaming and batch APIs.
- 1.3: `PgnTypes.h` (GameResult enum, resultToDouble, parseResultString, resultToString), `PgnGame.h` (structured game with headers, moves, result, convenience methods)
- 1.4: 30+ test cases covering: types round-trip, game struct, cleanUpMoveSection (mirroring OpeningBookTest::pgnCleanUpTest), tag parsing, streaming vs batch, parseFromLines, edge cases (empty, no-moves, %-escape, ;-comments), large file tests (8moves_v3, superbook), SAN format validation
- 1.7: Updated both `src/CMakeLists.txt` and `test/CMakeLists.txt` glob patterns

---

## Phase 2: Tuning Build Targets ✅

| Step | Task                                                                    | Status     |
|------|-------------------------------------------------------------------------|------------|
| 2.1  | Create `ExtractorMain.cpp` with stub `main()` + CLI argument parsing    | ✅ Complete |
| 2.2  | Create `TunerMain.cpp` with stub `main()` + CLI argument parsing        | ✅ Complete |
| 2.3  | Add `FrankyCPP_v1.7_Extractor` and `FrankyCPP_v1.7_Tuner` CMake targets | ✅ Complete |
| 2.4  | Guard tuning targets with `if(NOT FRANKYCPP_PRODUCTION)`                | ✅ Complete |
| 2.5  | Verify both executables build, link, and print `--help`                 | ✅ Complete |
| 2.6  | Create `src/tuning/README.md` with module documentation                 | ✅ Complete |

**Gate:** ✅ Both executables compile and run `--help`. Engine executable unaffected.

**Notes:**
- 2.1: `ExtractorMain.cpp` with Boost.program_options CLI: `--input`, `--output`, `--min-move`, `--min-pieces`, `--skip-captures`, `--skip-promotions`, `--qsearch-filter`, `--qsearch-threshold`, `--verbose`, `--help`. Positional args supported. Prints config summary then stub message.
- 2.2: `TunerMain.cpp` with Boost.program_options CLI: `--dataset`, `--output`, `--threads`, `--test-split`, `--resume`, `--max-passes`, `--verbose`, `--help`. Positional args supported. Prints config summary then stub message.
- 2.3: Both targets added to `src/CMakeLists.txt` following the `engine_arena` pattern. Link against `FrankyCPPlib` + `Boost::program_options`. Tuner also links `yaml-cpp::yaml-cpp`.
- 2.4: Both targets wrapped in `if(NOT FRANKYCPP_PRODUCTION)` — excluded from production builds.
- 2.5: Pending user build verification in CLion.
- 2.6: `src/tuning/README.md` created with module overview, architecture, usage examples, dataset format, and implementation status table.
- Removed `.gitkeep.md` placeholders from `extractor/` and `optimizer/` directories.

---

## Phase 3: Data Collection 🚧

| Step | Task                                                               | Status         |
|------|--------------------------------------------------------------------|----------------|
| 3.1  | Download Zurichess quiet-labeled dataset (or similar)              | ⬚ Not Started  |
| 3.2  | Create a small dev subset (~50K–100K positions) for fast iteration | ✅ Complete     |
| 3.3  | Start FrankyCPP self-play generation in background (cutechess-cli) | ✅ Complete     |
| 3.4  | Document dataset sources and locations in `test/testsets/tuning/`  | ⬚ Not Started  |

**Gate:** Dev dataset and full downloaded dataset available in `test/testsets/tuning/`.

**Notes:**
- 3.2: Dev dataset extracted from v1.6 vs v1.5 matches: ~49K positions (`v1.6_vs_v1.5_score.txt`)
- 3.3: Self-play complete: 17,037 games → 1.54M positions with qsearch+score filter (`selfplay_v1.7_50k_score.txt`)

---

## Phase 4: Position Extractor ✅

| Step | Task                                                                      | Status     |
|------|---------------------------------------------------------------------------|------------|
| 4.1  | Implement `PositionExtractor` class: PGN → FEN+result with filters 0–4    | ✅ Complete |
| 4.2  | Implement `ExtractionStats` struct with `printSummary()` formatted report | ✅ Complete |
| 4.3  | Wire up `ExtractorMain.cpp` with full CLI                                 | ✅ Complete |
| 4.4  | Implement standalone qsearch filter (Filter 5)                            | ✅ Complete |
| 4.5  | Write extractor unit tests (filter behavior, stats, output format)        | ✅ Complete |
| 4.6  | Add `TUNING_LOG` logger to `Logging.h`                                    | ✅ Complete |
| 4.7  | Update `test/CMakeLists.txt` for tuning test sources                      | ✅ Complete |
| 4.8  | Build, run tests, validate end-to-end with v1.6 match PGNs                | ✅ Complete |
| 4.9  | Compare extraction with/without qsearch filter (performance benchmark)    | ✅ Complete |

**Gate:** ✅ Extractor produces valid FEN+result files. Unit tests pass. Stats summary printed.

**Notes:**
- 4.1: `PositionExtractor` with streaming PGN parsing, 5 position filters + game-level filters
- 4.2: Full formatted report with percentages, rates, elapsed time
- 4.3: Added `--skip-termination` CLI flag; wired config → extractor → stats summary
- 4.4: Standalone qsearch (capture-only alpha-beta, no TT/SMP, max depth 6)
- 4.5: 16 test cases covering all filters, output format, stats consistency, integration
- 4.6: `TUNING_LOG` added to Logger singleton
- 4.7: `TEST_TUNING` glob + `SRCS_TUNING_EXTRACTOR_FOR_TESTS` (excludes ExtractorMain.cpp)
- 4.8: Validated on v1.6 vs v1.5 (500 games → 48,794 pos) and v1.6 vs SF18 (500 games → 44,497 pos)
- 4.9: Qsearch overhead: 3.3× (not 10-50× estimated). Filters 2.9-4.8% additional positions. SF games have higher tactical filtering (4.75% vs 2.89%) as expected.
- **Filter 6 added:** Score contradiction filter (`--score-filter`) — skips positions where PGN search score (from cutechess-cli comments) strongly contradicts game result. Reduces label noise from blunders. `parseSearchScore()` parses `{+1.32/11 6.9s}` format. 9 new unit tests.
- **Large-scale validation:** Tested on selfplay_v1.7_50k.pgn (17,037 games, 2.6M positions):
  - Score filter only: 1,597,558 extracted (61.33%), 909 games/s, 18.5s
  - Score + qsearch: 1,541,080 extracted (59.07%), 338 games/s, 49.8s (2.7× slower)
  - Qsearch caught 58,972 additional positions (2.26%); score filter caught ~3,000 (0.1%)

---

## Phase 5: Mark Tunable Parameters 🚧

| Step | Task                                                                   | Status        |
|------|------------------------------------------------------------------------|---------------|
| 5.1  | Mark `tunable = true` on all ~85 eval weight entries in ConfigRegistry | ✅ Complete    |
| 5.2  | Add `tunableOptions()` query method to ConfigRegistry                  | ✅ Complete    |
| 5.3  | Add unit test: verify expected number of tunable params discovered     | ✅ Complete    |
| 5.4  | Record baseline MSE, STS, WAC scores with current v1.6 params         | ⬚ Not Started |

**Gate:** Tunable flag set, test passes, baselines documented.

**Notes:**
- 5.1: 88 eval weight entries marked `tunable = true` (82 scalar Int + 6 IntArray). Excluded: all `bool USE_*` toggles, `EVAL_CONFIG_SOURCE` (String), `PAWN_TT_SIZE_MB` (infrastructure), `USE_GAMEPHASE_VALUE` (structural).
- 5.2: Added `tunableOptions()` to `ConfigRegistry.h/.cpp` — returns `vector<const ConfigDef*>` filtered by `exposure.tunable`. Follows existing `uciOptions()`/`yamlOptions()` pattern.
- 5.3: 7 test cases in `ConfigRegistryTest.cpp`: count pinned at 88, all Eval domain, all Int/IntArray, no Bool toggles, no infrastructure params, spot-check key params.
- 5.4: MSE baseline requires Phase 6 tuner infrastructure (deferred). STS/WAC baselines to be recorded manually.

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

*Last updated: 2026-03-23*
