# Logger: changing log levels at runtime

This project uses spdlog for logging. The `Logger` singleton exposes a few small helpers to make
runtime level changes easy and consistent:

- Logger::parseLevel(std::string_view)
  Convert common strings (e.g., "trace", "debug", "info", "warn", "error", "critical", "off") to a spdlog level.
  Unknown inputs fall back to `warn`.

- Logger::setGlobalLevel(spdlog::level::level_enum)
  Set spdlog’s global level. This affects the default filtering for loggers that don’t have an explicit
  level set. Additionally, all registered loggers are configured to `flush_on(warn)` to ensure warnings
  and above are flushed promptly.

- Logger::setLoggerLevel(const std::shared_ptr<spdlog::logger>&, spdlog::level::level_enum)
  Set the level of a specific logger (if the pointer is valid).

- Logger::setLoggerLevelByName(std::string_view, spdlog::level::level_enum)
  Look up a logger by name via `spdlog::get(name)` and set its level if found.

## Quick examples

- Set the global level to info:

```cpp
Logger::setGlobalLevel(spdlog::level::info);
```

- Parse a user-provided string and set a named logger:

```cpp
const auto lvl = Logger::parseLevel(user_input);   // e.g. "debug"
Logger::setLoggerLevelByName("Search_Logger", lvl);
```

- Directly adjust a known logger pointer (e.g., TEST_LOG):

```cpp
Logger::setLoggerLevel(Logger::get().TEST_LOG, spdlog::level::trace);
```

- Using both: from string to specific logger pointer:

```cpp
Logger::setLoggerLevel(Logger::get().SEARCH_LOG, Logger::parseLevel("warn"));
```

## Notes and pitfalls

- Compile-time gating: If the build defines `SPDLOG_ACTIVE_LEVEL` (or the project’s `LOG__LEVEL`), some
  messages (like TRACE/DEBUG) may be compiled out. Raising the level at runtime won’t resurrect messages
  removed at compile time. Example: with `-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO`, `LOG__TRACE` and
  `LOG__DEBUG` are compiled out.

- Named logger lookups: `setLoggerLevelByName` relies on the logger being registered with spdlog’s registry.
  The loggers created in `Logger` (e.g., `"Search_Logger"`, `"TT_Logger"`, etc.) are already named and
  registered through spdlog’s factory helpers used during initialization.

- Flushing: The helpers default to flushing warnings and above immediately. You can call `logger->flush_on(level)`
  on individual loggers to change this behavior.

- Patterns: `Logger::init()` sets a global pattern and a slimmer pattern for the UCI logger. Changing the level
  does not change patterns.

## Unit test reference

See `test/common/LoggingTest.cpp` test `LoggerRuntime.LevelChangeByPtrAndName` for a minimal example that:

- sets a logger to `warn` and verifies `info` is hidden while `warn` is emitted,
- raises the logger to `info` by pointer and checks `info` output appears,
- raises the logger to `debug` using `setLoggerLevelByName` combined with `parseLevel("debug")`.

This is a good template for validating changes to logging behavior in future refactors.
