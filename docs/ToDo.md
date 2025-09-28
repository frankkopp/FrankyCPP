Do not try to compile or run tests as you do not have the Clion environment and it would fail anyway. Ask me and I will run the task and provide the result.

Logging:
Here is the same small‑steps refactor plan, with updated prompts that explicitly state you will build and run tests and provide the results. I will not attempt to compile or run.

Plan overview
- Keep the Logger singleton and macro names.
- Make init idempotent to avoid duplicate sinks and racing reconfiguration.
- Make macros lazy and null‑safe to avoid formatting work when logs are disabled.
- Use spdlog features (`should_log`, `from_str`, `flush_every`, `flush_on`).
- Preserve strict FIFO within the file sink; default to synchronous loggers; add optional async mode behind a flag.
- Allow runtime log level changes (up to debug) via a small API; trace remains optional compile‑time.

    Step 1 — Make initialization idempotent, safe, and fast by default
    Goal
    - Avoid duplicated sinks on repeated init.
      - Use robust level parsing.
      - Improve flush policy: low overhead but still “follow the flow”.
    
    Files
    - `src/common/Logging.cpp`
      - `src/common/Logging.h`
    
    Tasks
    - Add a small helper `add_unique_sink(logger, sink)` that only adds a sink if it isn’t already present.
      - In `Logger::init()`:
          - Guard program option access with `count("key")` checks and fall back to defaults.
          - Use `spdlog::level::from_str` for level parsing with case insensitivity.
          - Use `add_unique_sink` for all logger/sink wiring to avoid duplicates on re‑init.
          - Change flush policy:
              - Set `flush_on(spdlog::level::warn)` for most loggers (keep `UCI_LOG` trace if needed).
              - Call `spdlog::flush_every(200ms)` once to keep latency low without flushing each message.
          - Keep loggers synchronous to preserve call order and minimize complexity.
      - Comment why we use these spdlog features and their effect on perf/ordering.
    
    Acceptance
    - Multiple inits do not duplicate log lines.
      - File log lines remain in call order; latency is typically under ~200ms; warnings/errors flush immediately.
    
    Prompt for step 1
    - Implement the following changes with minimal public surface changes. Only modify `src/common/Logging.cpp` and `src/common/Logging.h`.
      - Add a static/local helper in `src/common/Logging.cpp`:
          - `add_unique_sink(spdlog::logger\&, const std::shared_ptr<spdlog::sinks::sink>\&)` that pushes only if not already present (pointer compare).
      - In `Logger::init()`:
          - Use `programOptions.count("log_lvl")` and `programOptions.count("search_log_lvl")` to check presence; default to `"warn"` if absent.
          - Parse levels with `spdlog::level::from_str`; if invalid, fall back to `warn` and print a single clear message.
          - Replace all `sinks().push_back(...)` calls with `add_unique_sink(...)`.
          - Keep default pattern but set it once through `spdlog::set_pattern`.
          - Set `flush_on(spdlog::level::warn)` for all non‑`UCI_LOG` loggers; keep `UCI_LOG` as is or `trace` if necessary.
          - Add `spdlog::flush_every(std::chrono::milliseconds(200))` once during init.
      - Do not change the public API, macros, or logger names.
      - Add inline comments explaining why we switched to `from_str`, `add_unique_sink`, and `flush_every`.
      - Do not compile or run. I will build and run tests in CLion and provide the output.
    
    Step 2 — Make macros lazy, null‑safe, and if/else‑safe
    Goal
    - Zero formatting work when a level is disabled.
      - Preserve macro names and call sites.
      - Avoid macro pitfalls in control flow.
    
    Files
    - `src/common/Logging.h`
    
    Tasks
    - Wrap each `LOG__*` macro in `do { } while(0)`.
      - Check `logger && logger->should_log(level)` before calling `std::format(...)`.
      - Keep using `std::format(deLocale, ...)` to minimize call‑site changes.
      - Keep existing `LOG__LEVEL` ladder for now.
    
    Acceptance
    - No observable changes at call sites.
      - Disabled‑level logs do not evaluate formatting expressions.
    
    Prompt for step 2
    - In `src/common/Logging.h`, update all `LOG__*` macros:
        - Wrap each in `do { } while(0)`.
        - Inside, cache logger: `const auto\& _lg = (logger);` check `_lg && _lg->should_log(spdlog::level::<level>)`.
        - Only call `std::format(deLocale, __VA_ARGS__)` inside that branch, then `_lg->log(level, formatted)`.
        - Keep `LOG__LEVEL` gating as is; do not change levels or names.
      - Ensure macros compile in single‑statement contexts and `if/else` without braces.
      - Do not compile or run. I will build and run tests in CLion and provide the output.

    Step 3 — Align compile‑time level gating with spdlog
    Goal
    - Optional compile‑time removal of trace/debug to reduce overhead.
      - Keep runtime adjustability up to debug.
    
    Files
    - `src/common/Logging.h`
    
    Tasks
    - Before including spdlog headers, define `SPDLOG_ACTIVE_LEVEL` if not predefined by build flags.
        - Default to `SPDLOG_LEVEL_DEBUG` to keep runtime debug available.
        - Let users set `-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO` to strip debug/trace.
      - Optionally derive `LOG__LEVEL` default from `SPDLOG_ACTIVE_LEVEL` if `LOG__LEVEL` is not defined, so both are consistent.
    
    Acceptance
    - By default, behavior unchanged.
      - With `SPDLOG_ACTIVE_LEVEL` set to `INFO` or `WARN`, debug/trace macros compile out.
    
    Prompt for step 3
    - In `src/common/Logging.h`:
        - Add a small block at the top (before spdlog includes):
            - If `SPDLOG_ACTIVE_LEVEL` is not defined, `#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG`.
            - If `LOG__LEVEL` is not defined, map `SPDLOG_ACTIVE_LEVEL` to internal `LOG__LEVEL` so both align by default.
        - Add a brief comment showing how to set `SPDLOG_ACTIVE_LEVEL` at compile time to strip trace/debug.
      - Do not change existing default behavior unless the macro is provided.
      - Do not compile or run. I will build and run tests in CLion and provide the output.

    Step 4 — Add a minimal runtime log‑level control API
    Goal
    - Allow changing levels at runtime (global and per logger) up to debug.
      - Keep a compact API on the singleton.
    
    Files
    - `src/common/Logging.h`
      - `src/common/Logging.cpp`
    
    Tasks
    - In `Logger`, add:
        - `static spdlog::level::level_enum parseLevel(std::string_view)` using `spdlog::level::from_str` with fallback.
        - `void setGlobalLevel(spdlog::level::level_enum)` that updates `spdlog::set_level` and `flush_on` policy as needed.
        - `void setLoggerLevel(const std::shared_ptr<spdlog::logger>\&, spdlog::level::level_enum)` with null check.
        - Optional: `void setLoggerLevelByName(std::string_view name, spdlog::level::level_enum)` using `spdlog::get`.
    
    Acceptance
    - Can raise/lower levels at runtime to debug; trace behavior still controlled by compile‑time setting from Step 3.
    
    Prompt for step 4- In `src/common/Logging.h` add declarations to `Logger`:
    - `static spdlog::level::level_enum parseLevel(std::string_view);`
      - `void setGlobalLevel(spdlog::level::level_enum);`
      - `void setLoggerLevel(const std::shared_ptr<spdlog::logger>\&, spdlog::level::level_enum);`
      - `void setLoggerLevelByName(std::string_view, spdlog::level::level_enum);` optional but useful.
      - Implement in `src/common/Logging.cpp`:
          - `parseLevel` uses `spdlog::level::from_str` with fallback to `warn`.
          - `setGlobalLevel` calls `spdlog::set_level`, and optionally adjusts `flush_on` for all loggers if you want `warn+` to flush immediately.
          - `setLoggerLevel` sets the logger’s level if the pointer is valid.
          - `setLoggerLevelByName` uses `spdlog::get(name)` and delegates.
      - Add short comments on usage with spdlog.
      - Do not compile or run. I will build and run tests in CLion and provide the output.


    Step 5 — Improve sink wiring helpers and patterns (no behavioral change)
    Goal
    - Centralize repetitive per‑logger wiring and patterns; keep same result.
    
    Files
    - `src/common/Logging.cpp`
    
    Tasks
    - Add a small helper:
        - `void configure_logger(const std::shared_ptr<spdlog::logger>\& lg, spdlog::level::level_enum level, const std::string\& pattern, const std::shared_ptr<spdlog::sinks::sink>\& fileSink, spdlog::level::level_enum flushLevel)` that calls `add_unique_sink`, `set_pattern`, `set_level`, `flush_on`.
      - Replace current repeated blocks in `init()` with calls to this helper, preserving `UCI_LOG`’s special handling.
    
    Acceptance
    - No change in behavior; code is shorter and less error‑prone.
    
    Prompt for step 5
    - In `src/common/Logging.cpp`, introduce the internal helper `configure_logger(...)` as described above.
      - Replace repeated logger config blocks with `configure_logger` calls.
      - Keep `UCI_LOG`’s custom pattern and `uciOutSink` behavior; still use `add_unique_sink` for `uciOutSink`.
      - No change in public API or runtime behavior.
      - Do not compile or run. I will build and run tests in CLion and provide the output.

Step 6 — Optional: add async logging behind a compile‑time toggle
Goal
- Allow switching to async logging without changing call sites or macro usage.
- Preserve message order across producers while keeping low latency.

Files
- `src/common/Logging.h`
- `src/common/Logging.cpp`

Tasks
- Introduce a toggle e.g. `FRANKYCPP_LOG_ASYNC`.
- If defined:
    - Call `spdlog::init_thread_pool(queue_size, threads=1)` early in `init()` (or before creating loggers).
    - Create/convert loggers via `spdlog::create_async` and use a blocking `overflow_policy` to preserve order (block on overflow).
    - Keep `flush_every(200ms)` and `flush_on(warn)`.

Acceptance
- Default remains synchronous; defining async flag switches to async with preserved ordering and low visible latency.

Prompt for step 6
- Add a compile‑time flag `FRANKYCPP_LOG_ASYNC` guarded code:
    - If enabled, call `spdlog::init_thread_pool(1<<16, 1)` once in `init()` before creating loggers/sinks.
    - Create async loggers or switch factories accordingly; use `spdlog::async_overflow_policy::block`.
    - Keep the same macros and logger names; no call‑site changes.
- Comment on ordering guarantees (single consumer) and expected latency.
- Ensure graceful fallback to sync if the flag is not defined.
- Do not compile or run. I will build and run tests in CLion and provide the output.

Step 7 — Add an explicit shutdown/flush hook
Goal
- Ensure logs are flushed on exit, especially with async.

Files
- `src/common/Logging.h`
- `src/common/Logging.cpp`

Tasks
- Add `void shutdown()` on `Logger` that calls `spdlog::shutdown()`.
- Document calling it at program shutdown.

Acceptance
- On exit, logs are fully flushed and files closed.

Prompt for step 7
- In `Logger`, add `void shutdown();` and implement in `src/common/Logging.cpp` calling `spdlog::shutdown()`.
- Keep current singleton semantics.
- Optionally add a comment indicating where to call this in app shutdown.
- Do not compile or run. I will build and run tests in CLion and provide the output.


Time Management:
done: Add a configurable Move Overhead option (already proposed as Step 1)
done: Rationale: replace hardcoded 20ms/5ms with a UCI option used in both movetime and remaining‑time modes.
done: Prompt: Please implement Step 1: add a UCI Move Overhead option and use it in setupTimeControl across movetime and remaining‑time modes, touching src/engine/SearchConfig.h, src/engine/UciOptions.cpp, and src/engine/Search.cpp. I will build and run.

done: Soft time guard before expensive re‑searches (Step 2)
done: Rationale: avoid starting aspiration expansions, PV re‑searches, IID, or full re‑searches when time is almost up.
done: Prompt: Please add an isTimeAlmostUp() helper and call it at re‑search trigger points in src/engine/Search.cpp (root PVS re‑search, LMR re‑search, aspiration expansion, IID). I will build and run.

done: Adaptive iteration duration predictor
done: Rationale: replace fixed 1.5× last iteration heuristic with a predictor using node growth factor and current NPS to estimate next iteration cost.
done: Prompt: Please replace the 1.5× heuristic in iterativeDeepening with an ETA based on last iteration nodes and current NPS in src/engine/Search.cpp. I will build and run.

done: Panic time extension on volatility
done: Rationale: add extra time when fail‑low, big eval swings, or checks at root indicate tactical complexity.
done: Prompt: Please add a volatility detector (fail‑low, |Δeval| threshold, root in‑check) and call addExtraTime() conservatively in src/engine/Search.cpp. I will build and run.

done: Complexity‑aware time allocation
done: Rationale: spend more time when root move count is high, position is in check, or many legal captures exist; spend less on trivial or forced positions.
done: Prompt: Please weight per‑move budget by root complexity indicators (legal move count, in‑check, captures ratio) in setupTimeControl and root iteration gating in src/engine/Search.cpp. I will build and run.

done: Higher‑precision timer tail
done: Rationale: reduce overshoot on Windows by busy‑waiting the last few milliseconds instead of sleeping.
done: Prompt: Please modify startTimer() in src/engine/Search.cpp to switch from sleep to a short busy‑wait for the final ~2–3ms before deadline. I will build and run.

done: Improved movesLeft model
done: Rationale: estimate moves to go using game phase, material, and repetition risk rather than a linear factor.
done: Prompt: Please refactor movesLeft estimation in setupTimeControl in src/engine/Search.cpp to use phase/material buckets with tunables in src/engine/SearchConfig.h. I will build and run.

done: Root single‑move fast path
done: Rationale: if only one legal root move, skip deep search or cap depth/time.
done: Prompt: Please add a single‑move early exit in iterativeDeepening with minimal verification in src/engine/Search.cpp. I will build and run.

Time‑aware feature shedding
Rationale: disable or reduce costly features (IID, large LMR re‑search, history updates) under time pressure.
Prompt: Please conditionally skip IID and limit LMR re‑search when isTimeAlmostUp() in src/engine/Search.cpp. I will build and run.

Quiescence bailout under time pressure
Rationale: cap qsearch depth or switch to stand‑pat only when time is almost up.
Prompt: Please add a time‑pressure guard in qsearch to early‑return (stand‑pat) when isTimeAlmostUp() in src/engine/Search.cpp. I will build and run.



NPS‑based dynamic budget tracking
Rationale: track NPS during the search and estimate whether the next iteration (or re‑search) can finish within remaining time.
Prompt: Please track rolling NPS and use it to gate starting the next iteration and re‑searches in src/engine/Search.cpp. I will build and run.

Increment‑aware spend policy
Rationale: always leave a reserve, spend a fraction of increment, and avoid burning base time when increment is high.
Prompt: Please modify remaining‑time budgeting in setupTimeControl to allocate baseShare + k * increment with a fixed reserve; add tunables in src/engine/SearchConfig.h. I will build and run.

Ponder credit budgeting
Rationale: if the pondered move is played, reuse part of ponder time as credit for the move; otherwise decay.
Prompt: Please track ponderCreditMs and, on ponderhit() in src/engine/Search.cpp, add a bounded credit to extraTimeMs. I will build and run.

Decimated time checks inside hot loops
Rationale: add a very cheap periodic time check (e.g., every N nodes) to stopConditions() using an atomic deadline to minimize overhead.
Prompt: Please add an atomic deadlineNs, update it when time changes, and check it every N nodes in stopConditions() and key loops in src/engine/Search.cpp. I will build and run.

Emergency move mode
Rationale: when remaining time below a threshold, enforce a very fast, shallow search to avoid flagging.
Prompt: Please add a UCI Emergency Move Time threshold and, when triggered, cap depth and avoid re‑search paths in src/engine/Search.cpp. I will build and run.

Enhanced timing telemetry
Rationale: log computed budgets, reserves, extra time changes, iteration ETAs, and overruns for tuning.
Prompt: Please enrich timing logs in src/engine/Search.cpp and add a SearchConfig flag to toggle verbose timing telemetry. I will build and run.

Ponder time cap option
Rationale: avoid runaway ponder by capping max ponder time per move.
Prompt: Please add UCI Max Ponder Time and enforce it in startTimer()/ponderhit() paths in src/engine/Search.cpp. I will build and run.

Min/Max per‑move clamps (UCI options)
Rationale: prevent pathological allocations by clamping computed budget.
Prompt: Please add UCI options Min Move Time and Max Move Time and clamp the computed timeLimit in src/engine/Search.cpp and wire in src/engine/UciOptions.cpp. I will build and run.

Files you’ll likely touch for these steps:
src/engine/Search.cpp
src/engine/SearchConfig.h
src/engine/UciOptions.cpp
src/engine/SearchLimits.h
