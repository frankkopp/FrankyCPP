# Engine Arena Review

Date: 2026-02-06

Scope: `src/engine_arena/*`, `src/engine_arena_main.cpp`, `docs/arena/*.md`, `test/engine_arena/*`

## Findings (ordered by severity)

- High: JSON output is built via raw string concatenation without escaping string fields, so Windows-style backslashes and any quotes in engine names/paths or FENs can produce invalid JSON and break reporting/automation. Evidence in `src/engine_arena/ResultWriter.cpp:65` and `src/engine_arena/ResultWriter.cpp:137`.
- High: `readLine` spawns a new thread per read and detaches it on timeout; repeated timeouts can leak threads and cause concurrent reads from the same pipe, corrupting UCI output parsing. Evidence in `src/engine_arena/UCIEngine.cpp:384` and `src/engine_arena/UCIEngine.cpp:411`.
- Medium: Engine startup with command-line args concatenates `enginePath + " " + commandLineArgs` and passes it as a single program string; this is fragile for paths with spaces or complex quoting, especially on Windows, and can prevent the engine from launching. Evidence in `src/engine_arena/UCIEngine.cpp:60`.
- Medium: `searchTimeout` defaults to 30s and is not tied to `timePerMove`; long per-move limits (docs mention up to 1 hour) can time out early and mark positions as failed. Evidence in `src/engine_arena/UCIEngine.h:93` and `src/engine_arena/UCIEngine.cpp:170`.
- Medium: Parallel test execution does not honor `debugMode` (no `setDebugMode(true)` in the worker initialization), so users cannot get UCI I/O logging in parallel mode despite docs implying the flag applies to all runs. Evidence in `src/engine_arena/TestSuiteRunner.cpp:455`.
- Medium: Configuration validation omits checks for `timePerMove > 0`, `concurrency > 0`, and `resultsDir` writability, but the documentation claims numeric range checks and writable directory validation. Evidence in `src/engine_arena/ArenaConfig.cpp:177` and doc claim in `docs/arena/Configuration.md:972`.
- Medium: Documentation is inconsistent with current CLI and result formats; this will mislead users and break copy/paste workflows.
  - `docs/arena/External_Engine_Testing.md:288` and `docs/arena/Results.md:349` still use `--compare`, but the CLI uses `--cmp` (`src/engine_arena_main.cpp:63`).
  - Test suite JSON examples and scripts in `docs/arena/External_Engine_Testing.md:394` and `docs/arena/Results.md:71` use the old schema (`version`, `suiteName`, `results`), while current output uses `arenaVersion`, nested `testSuite`/`engine`, and `summary` (`src/engine_arena/ResultWriter.cpp:65`).
  - `docs/arena/Configuration.md:936` "Complete Configuration Example" omits `enginePath`, which is required and enforced (`src/engine_arena/ArenaConfig.cpp:210`).
  - `docs/arena/Development.md:81` describes TestSuiteRunner using the built-in engine and lists external engine support as future work, but the code already uses external UCI engines.
- Low: Test coverage is strong for `UCIEngine` and `TestSuiteRunner`, but there are no tests covering `MatchRunner::parseOutput`, `ResultWriter` JSON validity (escaping), or report generation in `ArenaRunner`. Evidence: only UCI/TestSuite tests exist in `test/engine_arena/TestSuiteRunner_IntegrationTest.cpp:1` and `test/engine_arena/UCIEngine_ErrorHandlingTest.cpp:1`.

## Open Questions / Assumptions

- Are Windows paths expected to always use forward slashes in configs (to avoid JSON escaping issues), or should the writer escape backslashes regardless?
- Do you want to support reading legacy result files, or is skipping old schemas acceptable?
- Should `timePerMove` implicitly raise `searchTimeout`, or should a new YAML field control the timeout explicitly?

## Change Summary

- Review only; no code changes made.
