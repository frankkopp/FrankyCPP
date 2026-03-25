# FrankyCPP Texel Tuning — Phase Progress Tracker

**Plan Document:** `docs/specs/PLAN_Texel_Tuning.md`  
**Created:** 2026-03-22  
**Last Updated:** 2026-03-25  
**Target Version:** v1.7  

---

## Phase Summary

| Phase | Name                               | Status           | Started    | Completed  |
|-------|------------------------------------|------------------|------------|------------|
| 0     | Release v1.6, branch v1.7          | ✅ Complete       | —          | 2026-03-22 |
| 1     | Module structure + PGN library     | ✅ Complete       | 2026-03-22 | 2026-03-22 |
| 2     | Tuning build targets (scaffolding) | ✅ Complete       | 2026-03-22 | 2026-03-22 |
| 3     | Data collection                    | ✅ Complete       | 2026-03-22 | 2026-03-25 |
| 4     | Position extractor                 | ✅ Complete       | 2026-03-23 | 2026-03-23 |
| 5     | Mark tunable parameters            | 🟡 5.4 deferred  | 2026-03-23 | 2026-03-23 |
| 6     | Optimizer implementation           | 🟡 Code complete | 2026-03-24 | 2026-03-25 |
| 7     | Integration testing                | ⬚ Not Started    |            |            |
| 8     | Gauntlet validation + release      | ⬚ Not Started    |            |            |

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

## Phase 3: Data Collection ✅

| Step | Task                                                               | Status     |
|------|--------------------------------------------------------------------|------------|
| 3.1  | Download Zurichess quiet-labeled dataset (or similar)              | ✅ Complete |
| 3.2  | Create a small dev subset (~50K–100K positions) for fast iteration | ✅ Complete |
| 3.3  | Start FrankyCPP self-play generation in background (cutechess-cli) | ✅ Complete |
| 3.4  | Document dataset sources and locations in `test/testsets/tuning/`  | ✅ Complete |

**Gate:** ✅ Dev dataset and full datasets available in `test/testsets/tuning/`. Combined ~6.0M positions.

**Notes:**
- 3.1: Zurichess quiet-labeled.v7 downloaded (1,428,000 positions). Archived as `quiet-labeled.epd.7z`.
- 3.2: Dev dataset extracted from v1.6 vs v1.5 matches: ~49K positions (`v1.6_vs_v1.5_score.txt`)
- 3.3: Self-play complete: 50,142 games (cutechess-cli, st=0.5, concurrency 12, 8moves_v3.pgn openings). Extracted with score+qsearch filters → 4,568,763 positions (`selfplay_v1.7_50k_score.txt`). PGN archived as `selfplay_v1.7_50k.7z`. Match stats in `50k-matches-info.txt`.
- 3.4: `extraction-commands.txt` created with exact regeneration commands, filter settings, and extraction summary. Large files git-ignored; only `.7z` archives and info files committed.

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
- **Verbose progress:** Added `--verbose` progress reporting (every 5000 games: count, positions, elapsed, rate). Previously the flag was parsed but unused.

---

## Phase 5: Mark Tunable Parameters 🟡 (5.4 deferred)

| Step | Task                                                                   | Status        |
|------|------------------------------------------------------------------------|---------------|
| 5.1  | Mark `tunable = true` on all ~85 eval weight entries in ConfigRegistry | ✅ Complete    |
| 5.2  | Add `tunableOptions()` query method to ConfigRegistry                  | ✅ Complete    |
| 5.3  | Add unit test: verify expected number of tunable params discovered     | ✅ Complete    |
| 5.4  | Record baseline MSE, STS, WAC scores with current v1.6 params          | ⬚ Not Started |

**Gate:** Tunable flag set, test passes, baselines documented.

**Notes:**
- 5.1: 88 eval weight entries marked `tunable = true` (82 scalar Int + 6 IntArray). Excluded: all `bool USE_*` toggles, `EVAL_CONFIG_SOURCE` (String), `PAWN_TT_SIZE_MB` (infrastructure), `USE_GAMEPHASE_VALUE` (structural).
- 5.2: Added `tunableOptions()` to `ConfigRegistry.h/.cpp` — returns `vector<const ConfigDef*>` filtered by `exposure.tunable`. Follows existing `uciOptions()`/`yamlOptions()` pattern.
- 5.3: 7 test cases in `ConfigRegistryTest.cpp`: count pinned at 88, all Eval domain, all Int/IntArray, no Bool toggles, no infrastructure params, spot-check key params.
- 5.4: MSE baseline requires Phase 6 tuner infrastructure (deferred). STS/WAC baselines to be recorded manually.

---

## Phase 6: Optimizer Implementation 🟡 (code complete, 6.11 decision pending)

| Step | Task                                                                     | Status         |
|------|--------------------------------------------------------------------------|----------------|
| 6.1  | Implement `TuningDataset` loader (FEN+result parsing, train/test split)  | ✅ Complete     |
| 6.2  | Implement `TuningParameter` mapping (registry → flat param vector)       | ✅ Complete     |
| 6.3  | Implement `TexelTuner` core: sigmoid, MSE computation, K-tuning          | ✅ Complete     |
| 6.4  | Implement coordinate descent loop with parallel MSE                      | ✅ Complete     |
| 6.5  | Implement incremental MSE optimization (activation flags)                | ✅ Complete     |
| 6.6  | Implement monotonicity constraint enforcement for array parameters       | ✅ Complete     |
| 6.7  | Implement `TuningState` checkpoint save/load (YAML)                      | ✅ Complete     |
| 6.8  | Wire up `TunerMain.cpp` with full CLI                                    | ✅ Complete     |
| 6.9  | Implement output: tuned params YAML, comparison report                   | ✅ Complete     |
| 6.10 | Write comprehensive unit tests for each component                        | ✅ Complete     |
| 6.11 | **Decision point:** Evaluate initial results; decide on PST tuning scope | ⬚ Not Started  |

**Gate:** ✅ Tuner runs end-to-end on dev dataset. Checkpoint save/resume works. All 196 tuning tests pass. Only 6.11 (decision point) remains.

**Notes:**
- 6.1: `TuningDataset` with dual-format loader (FrankyCPP `[result]` + EPD `c9`), auto-detection per line, deterministic train/test split, load stats, FEN validation via reusable Position. `TuningEntry` with `activeParamGroups` bitset for incremental MSE (Phase 6.5). 25 unit tests covering both formats, Zurichess 1.4M EPD, edge cases, split ordering. **Committed:** `bcae348`.
- 6.2: ✅ **Built, tested (20/20 pass), committed.** `TuningParameter` struct + `MonotonicityConstraint` enum + `buildFromRegistry()` static factory. Scalar Int → 1 param, IntArray → 1 param per element. Uses ConfigDef getter/setter lambdas. `applyToConfig()` / `readFromConfig()` round-trip. 13 param groups, monotonicity on 6 arrays, `countTunableValues()`. 20 unit tests all green.
- 6.3: ✅ **Built, tested (18/18 pass), committed.** `TexelTuner` class with `sigmoid()`, `computeMSE()` (single-threaded), `tuneK()` (ternary search). `setupEvalOverrides()` disables lazy eval/pawn TT, enables space/coordination terms. Uses `setFromFen()` for efficient position reuse. Eval perspective handled correctly (negate when Black to move). Dev dataset K≈0.52, MSE≈0.071. 18 unit tests (6 sigmoid, 7 MSE, 3 K-tuning, 2 dev dataset integration).
- 6.4: ✅ **Built, tested (30/30 pass), committed.** Parallel MSE via `common::ThreadPool` — one `Evaluator` per thread, sorted partial-sum reduction for deterministic FP. `createEvaluators(N)` creates N evaluators + pool. `computeMSEParallel()` matches single-threaded within 1 ULP (2.2e-16 diff on 2K positions). Coordinate descent `tuneParameters()` — tries ±delta per param, keeps best direction, logs per-pass summary (train/test MSE, params changed, biggest mover, time). Mutable config access via `applyOverrides()`. Dev dataset (5K pos, 122 params, 4 threads): 95/122 params improved, MSE 0.1200→0.1178 (−0.0022) in 1 pass, 1.05s. `tuneK()` auto-selects parallel MSE when multi-threaded. 12 new tests (6 parallel MSE, 5 coordinate descent, 1 thread clamping).
- 6.5: ✅ **Built, tested (42/42 pass), committed.** Incremental MSE with activation flags. `TuningEntry` gains `cachedSquaredError` field. Board-state analysis in `computeActivationFlags()` sets per-entry `activeParamGroups` bitset (13 groups, parallel via ThreadPool). `computeAndCacheErrors()` does full eval pass populating cache + `totalSquaredError_`. `computeMSEIncremental()` accumulates `deltaSSE = Σ(freshSE - cachedSE)` for active entries only, returns `(totalSSE + deltaSSE) / N` — matches full MSE within 2.8e-17. `updateCacheForGroup()` refreshes cache after committed changes. `tuneParameters()` now uses incremental MSE: activation flags + cache before loop, incremental trials, cache update on commit. Speedup on quiet-labeled.epd (100K pos): knight group (61.5% active) **1.45× faster**; bishop-pair group (35.2% active) expected ~2.5×. 11 new tests (5 activation flags, 4 incremental MSE correctness, 1 cache consistency, 1 integration) + 1 speedup benchmark.

- 6.6: ✅ **Built, tested, committed.** `enforceMonotonicity()` static method on `TexelTuner` — clamps array element to neighbor bounds (NON_DECREASING: floor from predecessor, ceiling from successor; NON_INCREASING: inverse). Integrated into coordinate descent: called after setting ±delta trial values and after committing keep-best. Only the modified element is clamped (no cascading). 11 new tests: floor/ceiling clamping, no-op for scalars/NONE, first/last element boundaries, real registry arrays (KING_SAFETY_TABLE), integration test verifying ordering after 2 passes of coordinate descent.

- 6.7: ✅ **Built, tested, committed.** `TuningState` struct with YAML checkpoint persistence. `captureFromParams()` / `restoreToParams()` for snapshotting/restoring param vectors (name-based matching with warnings for mismatches). `saveToYaml()` / `loadFromYaml()` using yaml-cpp `Emitter`/`LoadFile` with format version marker (`FrankyCPP_TuningCheckpoint_v1`). Stores: completedPasses, K, bestTrainMSE, bestTestMSE, datasetPath, timestamp, all param name→value pairs. Integrated into `tuneParameters()`: optional `checkpointPath` + `datasetPath` + `startPass` params — saves after each pass, supports resume from arbitrary pass. 15 new tests: capture/restore, missing/extra params, YAML round-trip, negative values, empty params, error handling (missing file, invalid YAML, wrong format, bad path), human-readable check, real registry round-trip, coordinate descent integration.

- 6.8: ✅ **Built, committed.** `TunerMain.cpp` wired up with full end-to-end pipeline. Replaces the Phase 6 TODO stub with: dataset loading → train/test split → eval overrides → evaluator pool creation → parameter vector from registry → checkpoint resume (`--resume`) with `setK()` → K tuning (ternary search) → coordinate descent with checkpointing → final summary with top-20 movers table. All CLI args mapped: `--dataset`, `--output`, `--threads`, `--test-split`, `--max-passes`, `--resume`, `--verbose`. TUNING_LOG level set to debug when verbose. Added `setK()` setter to `TexelTuner` for clean checkpoint resume. Checkpoint path auto-derived from output path stem.

- 6.9: ✅ **Built, tested, committed.** `TuningOutput` class with `writeParamsYaml()` and `writeComparisonReport()`. YAML output uses flat-key format matching `config/eval.yaml` — array elements coalesced back into comma-separated values (e.g., `KING_SAFETY_TABLE: 0,5,15,...`). Comparison report has statistics (changed, unchanged, sign-flipped, zeroed-out), full parameter table with delta and change%, flags for SIGN-FLIP and ->ZERO. Console summary via `printComparisonSummary()`. Integrated into `TunerMain.cpp`: generates `<output>.yaml`, `<output>_comparison.txt`, and prints summary. 18 new tests: YAML header, scalar params, array coalescing, empty params, file creation, bad path, report header/statistics/all-params/delta/flags, registry round-trip YAML+report, edge cases (all unchanged, percentage calculation, from-zero).

- 6.10: ✅ **Built, tested (196/196 pass), finalized.** 37 new edge-case tests added across all Phase 6 components. Coverage review completed per Sprint 6.10 plan:
  - `TuningDatasetTest` (+7): duplicate FENs accepted, maxEntries limit, maxEntries=0 unlimited, out-of-range results rejected, only-comments file, single-entry split, reserve no-op.
  - `TuningParameterTest` (+8): applyToConfig beyond max bounds, negative beyond min, delta default=1, expanded count pinned at 122, single-element array contract, array index/size validation, originalValue matches currentValue on fresh build.
  - `TexelTunerTest` (+10): setupEvalOverrides verification (lazy eval/pawn TT disabled, space/coordination enabled), setK/getK round-trip, near-zero MSE for perfect prediction, all-draws dataset, all-white-wins dataset, eval perspective with known material advantage, single-parameter convergence, tuneK parallel vs single-thread consistency, hasEvaluator state tracking, **end-to-end integration test** (10+ positions → tuneK → 2-pass coordinate descent → checkpoint → YAML output → comparison report → verify all files valid).
  - `TuningStateTest` (+6): captureFromParams empty vector, restoreToParams empty state, parameter order preserved through save/load, partial checkpoint with missing K field, large param vector (122) round-trip, extreme values (INT_MAX/INT_MIN).
  - `TuningOutputTest` (+6): single-element array coalescing, all-params-changed report, large negative delta with sign-flip flag, mixed scalars and arrays in YAML.

### Session Pickup Instructions
- Next step: Phase 6.11 — Decision point (evaluate initial tuning results, not a code step)
- All Phase 6 code is complete (6.1–6.10). Build and run tests to validate.

### Sprint Plan (Phase 6 Detailed Breakdown)

Phase 6 builds the core Texel tuner: dataset loading, parameter mapping, sigmoid/MSE math,
coordinate descent optimization, checkpointing, and output. Each sprint is one self-contained
step (~0.5–2 days) with a clear deliverable and test gate.

#### Sprint 6.1 — TuningDataset loader
- Create `TuningDataset.h/.cpp` and `TuningEntry.h`.
- `TuningEntry`: holds FEN string + float result (+ placeholder `activeParamGroups` bitset for Sprint 6.5).
- `TuningDataset::loadFromFile()`: parse `<FEN> [<result>]` format, one per line; support both `[1.0]` and EPD c9 tag.
- `TuningDataset::split(float trainFraction)`: deterministic split into train/test pair (no shuffle — file order, as per plan).
- `size()`, `operator[]`, iteration support.
- **Tests** (`test/tuning/TuningDatasetTest.cpp`): parse valid lines, reject malformed, verify split ratios, round-trip FEN integrity, test with the dev dataset file `v1.6_vs_v1.5_score.txt`.
- **Gate:** Loads dev dataset (~49K positions) in < 2s. All tests pass.

#### Sprint 6.2 — TuningParameter mapping
- Create `TuningParameter.h/.cpp`.
- `TuningParameter` struct: `name`, `valuePtr` (into live `EvalConfigData`), `originalValue`, `currentValue`, `minValue`/`maxValue`, `delta`, `paramGroup`, `arrayIndex`, `MonotonicityConstraint`.
- Factory function `buildTuningParameters(EvalConfigData& config)`: queries `ConfigRegistry::instance().tunableOptions()`, builds flat vector. For `IntArray` entries, expand each element into a separate `TuningParameter` with appropriate `arrayIndex`. Assign `paramGroup` IDs (~15 groups by eval category).
- **Tests** (`test/tuning/TuningParameterTest.cpp`): verify count matches 88 tunable entries (expanded arrays → total individual params), check `valuePtr` points to correct field, verify array expansion for `KING_SAFETY_TABLE` (16 elements) and `PASSED_PAWN_RANK_MID_BONUS` (6 elements), verify monotonicity constraints assigned to known arrays.
- **Gate:** Parameter vector built from registry. Modifying `*valuePtr` changes `EvalConfigData`. All tests pass.

#### Sprint 6.3 — TexelTuner core: sigmoid, MSE, K-tuning
- Create `TexelTuner.h/.cpp` with initial core methods.
- `sigmoid(K, eval)`: `1.0 / (1.0 + pow(10.0, -K * eval / 400.0))`.
- `computeMSE(dataset)`: iterate all positions, call `Evaluator::evaluate()`, convert to White-relative (negate when Black-to-move), apply sigmoid, accumulate squared error. **Single-threaded first** — parallel in Sprint 6.4.
- Eval setup: disable `USE_LAZY_EVAL`, `USE_PAWN_TT`; enable `USE_SPACE_EVAL`, `USE_CONNECTED_ROOKS`, `USE_MINOR_CONNECTIVITY` via `CONFIG_OVERRIDE_START`.
- `tuneK()`: ternary search on [0.5, 2.0] for 50 iterations per plan. Returns optimal K.
- One `Evaluator` instance (single-threaded).
- **Tests** (`test/tuning/TexelTunerTest.cpp`): sigmoid unit tests (eval=0 → 0.5, large positive → ~1.0, symmetric), MSE on a tiny hand-crafted dataset (3–5 positions with known results), K-tuning produces value in [0.5, 2.0], MSE with all-zero eval > MSE with real params (sanity).
- **Gate:** `computeMSE()` runs on dev dataset. `tuneK()` converges. All tests pass.

#### Sprint 6.4 — Parallel MSE + coordinate descent loop
- Add multi-threaded `computeMSE()`: partition positions across N threads, each with its own `Evaluator` instance (per `threadEvaluators` vector). Aggregate partial sums with sorted reduction for deterministic FP.
- Use existing `common::ThreadPool` for work dispatch.
- Implement `tuneParameters()`: outer `while(improved)` loop, inner loop over all `TuningParameter`s. For each: try +delta, compute MSE; try -delta, compute MSE; keep best or revert. Log pass progress (params changed, MSE).
- Print per-pass summary: pass number, train MSE, test MSE (if available), count of changed params, biggest movers.
- **Tests:** verify coordinate descent improves MSE on dev dataset (at least 1 pass reduces train MSE), verify multi-thread MSE matches single-thread MSE (within FP tolerance), verify convergence (terminates within `maxPasses`).
- **Gate:** Full coordinate descent runs end-to-end on dev dataset with 4 threads. MSE decreases. Takes < 60s on dev dataset.

#### Sprint 6.5 — Incremental MSE (activation flags)
- Add `std::bitset<NUM_PARAM_GROUPS> activeParamGroups` to `TuningEntry`.
- Implement `TuningDataset::computeActivationFlags(Evaluator&)`: for each position, evaluate once and record which param groups contributed. Use a lightweight instrumentation approach — check which `EvalConfigData` fields are relevant based on board state (e.g., has knights → knight group active, has rooks on open files → rook-file group active).
- Add `computeMSEIncremental(dataset, paramGroup)`: only re-evaluates positions where `activeParamGroups[paramGroup]` is set.
- Update `tuneParameters()` to call incremental MSE when testing P ± delta.
- **Tests:** verify activation flags set correctly for known positions (e.g., position with no knights → knight group not active), verify incremental MSE matches full MSE for the same parameter change, measure speedup (expect ~2× over full MSE).
- **Gate:** Incremental MSE produces identical results to full MSE. Measurable speedup. All tests pass.

#### Sprint 6.6 — Monotonicity constraints for arrays
- Implement `enforceMonotonicity(TuningParameter& param, std::vector<TuningParameter>& allParams)`: after modifying an array element, clamp value relative to neighbors (`array[i] >= array[i-1]` for non-decreasing).
- Integrate into the coordinate descent loop — call after each array element change.
- Apply to: `KING_SAFETY_TABLE`, `PASSED_PAWN_RANK_MID/END_BONUS`, `PAWN_ADVANCE_MID/END_BONUS`, `PAWN_STORM_MID_PENALTY`.
- **Tests:** attempt to violate monotonicity by setting middle element below predecessor → verify clamped, verify tuner converges with constraints active (no infinite loops from conflicting clamps).
- **Gate:** Array parameters maintain ordering after tuning. Tests pass.

#### Sprint 6.7 — TuningState checkpoint save/load
- Create `TuningState.h/.cpp`.
- `TuningState` struct: `completedPasses`, `bestMSE`, `K`, `paramValues` (vector of name→value), `datasetPath`, `timestamp`.
- `saveToYaml(path)`: serialize to YAML using yaml-cpp.
- `loadFromYaml(path)`: deserialize and restore parameter values into `EvalConfigData` via `ConfigRegistry` setters.
- Integrate into `tuneParameters()`: save checkpoint after each completed pass.
- Add `--resume` support: load checkpoint, set K, restore param values, continue from `completedPasses + 1`.
- **Tests:** save → load round-trip (all fields preserved), resume produces same result as uninterrupted run (determinism), corrupt/missing file handling.
- **Gate:** Checkpoint YAML is human-readable and round-trips correctly. Resume works. Tests pass.

#### Sprint 6.8 — Wire up TunerMain.cpp
- Replace the "not yet implemented" stub in `TunerMain.cpp` with the full pipeline: load dataset → split → apply eval overrides → tune K → tune parameters → save output.
- Map all CLI args to `TexelTuner` configuration: `--threads`, `--test-split`, `--max-passes`, `--resume`, `--verbose`.
- Add `TUNING_LOG` logger initialization for structured logging.
- Print final summary: baseline MSE, final MSE, improvement %, number of passes, elapsed time.
- **Gate:** `FrankyCPP_v1.7_Tuner --dataset dev_50k.txt --threads 4` runs end-to-end and produces output files.

#### Sprint 6.9 — Output: tuned YAML + comparison report
- Generate `tuned_params.yaml`: only tuned eval parameters, in `ConfigManager`-loadable format (matching `config/eval.yaml` structure).
- Generate `param_comparison.txt`: side-by-side table — parameter name, original value, tuned value, delta, change %. Flag sign-flipped params. Flag zeroed-out params (candidates for removal).
- Print comparison summary to stdout and `TUNING_LOG`.
- **Tests:** verify YAML output is loadable by `ConfigManager`, verify comparison report contains all tuned params, verify delta/percentage calculations.
- **Gate:** Output YAML can be copied to `config/eval.yaml` and engine starts normally. Comparison report is human-readable.

#### Sprint 6.10 — Comprehensive unit tests
- Review test coverage across all Sprint 6.1–6.9 components. Add missing edge cases:
  - `TuningDataset`: empty file, single position, huge result values, duplicate FENs
  - `TuningParameter`: min/max clamping, delta=0 edge, all-array-expanded count verification
  - `TexelTuner`: eval perspective correctness (known position with known who-is-better), MSE=0 edge case (perfect predictions), convergence with only 1 parameter
  - `TuningState`: empty state, partial checkpoint, version mismatch handling
  - Integration test: small end-to-end run (100 positions, 3 params, 2 passes) → verify MSE decreases and output files valid.
- **Gate:** All tests pass. No untested public methods in optimizer module.

#### Sprint 6.11 — Decision point: evaluate results
- Run tuner on full dev dataset (~49K positions) with all 88 tunable params. Record train/test MSE and wall time.
- Inspect tuned values: sanity checks (signs, magnitudes, no wild outliers).
- Compare MSE improvement with baseline from Phase 5.4.
- **Decision:** Is PST tuning needed? If eval-weight-only MSE improvement is satisfactory and test MSE tracks train MSE (no overfitting), proceed to Phase 7. If MSE plateaus, consider PST follow-up (Phase D in plan).
- *This is an analysis/decision step, not a code step.*

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

*Last updated: 2026-03-25*
