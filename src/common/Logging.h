// FrankyCPP
// Copyright (c) 2018-2026 Frank Kopp
//
// MIT License
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef FRANKYCPP_LOGGING_H
#define FRANKYCPP_LOGGING_H

//=============================================================================
// Logging.h - Centralized Logging Infrastructure
//=============================================================================
//
// Provides a singleton-based logging system using spdlog with compile-time
// and runtime log level control.
// Depends on: spdlog, version.h
//
// Log Levels (from most to least severe):
//   CRITICAL (1), ERROR (2), WARN (3), INFO (4), DEBUG (5), TRACE (6)
//   ZERO (0) disables all logging.
//
// Compile-Time Filtering:
//   Define SPDLOG_ACTIVE_LEVEL before including this header to strip
//   log calls at compile time:
//     -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO  // removes DEBUG & TRACE
//   Default is SPDLOG_LEVEL_DEBUG.
//
// Log Macros:
//   LOG__CRITICAL(logger, fmt, ...)
//   LOG__ERROR(logger, fmt, ...)
//   LOG__WARN(logger, fmt, ...)
//   LOG__INFO(logger, fmt, ...)
//   LOG__DEBUG(logger, fmt, ...)
//   LOG__TRACE(logger, fmt, ...)
//
// Available Loggers (via Logger::get()):
//   TEST_LOG, UCIHAND_LOG, UCI_LOG, BOOK_LOG, TT_LOG,
//   SEARCH_LOG, EVAL_LOG, TSUITE_LOG, CONFIG_LOG
//
// Usage:
//   auto& log = Logger::get().SEARCH_LOG;
//   LOG__INFO(log, "Search depth: {}", depth);
//   LOG__DEBUG(log, "Move: {} score: {}", move.str(), score);
//
// See also: docs/Logger.md for detailed documentation
//
//=============================================================================

// Compile-time log level gating (align with spdlog).
// You can strip debug/trace at compile time by defining SPDLOG_ACTIVE_LEVEL via your build, e.g.:
//   -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO   // removes debug & trace at compile time
// Default to DEBUG so runtime debug remains available unless the build overrides it.
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#endif

#include "version.h"

#include "common/ExePath.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <spdlog/spdlog.h>

#include <memory>
#include <string_view>

#ifdef NDEBUG
#define ASSERT_START while (0) {
#define ASSERT_END }
#else
#define ASSERT_START while (1) {
#define ASSERT_END \
  break;           \
  }
#endif

#define ZERO__LVL 0
#define CRITICAL__LVL 1
#define ERROR__LVL 2
#define WARN__LVL 3
#define INFO__LVL 4
#define DEBUG__LVL 5
#define TRACE__LVL 6

// If the build does not predefine LOG__LEVEL, map it from spdlog's compile-time active level
// so our local macros and spdlog's compile-time filtering align by default.
#ifndef LOG__LEVEL
// spdlog numeric levels: TRACE=0, DEBUG=1, INFO=2, WARN=3, ERR=4, CRITICAL=5, OFF=6
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
#define LOG__LEVEL TRACE__LVL
#elif SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
#define LOG__LEVEL DEBUG__LVL
#elif SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_INFO
#define LOG__LEVEL INFO__LVL
#elif SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_WARN
#define LOG__LEVEL WARN__LVL
#elif SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_ERROR
#define LOG__LEVEL ERROR__LVL
#elif SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_CRITICAL
#define LOG__LEVEL CRITICAL__LVL
#else
#define LOG__LEVEL ZERO__LVL
#endif
#endif

#if LOG__LEVEL > ZERO__LVL
#define LOG__CRITICAL(logger, ...)                         \
  do {                                                     \
    const auto& _lg = (logger);                            \
    if (_lg && _lg->should_log(spdlog::level::critical)) { \
      auto _msg = std::format(projectLocale, __VA_ARGS__); \
      _lg->log(spdlog::level::critical, _msg);             \
    }                                                      \
  } while (0)
#else
#define LOG__CRITICAL(logger, ...) void(0)
#endif

#if LOG__LEVEL > CRITICAL__LVL
#define LOG__ERROR(logger, ...)                            \
  do {                                                     \
    const auto& _lg = (logger);                            \
    if (_lg && _lg->should_log(spdlog::level::err)) {      \
      auto _msg = std::format(projectLocale, __VA_ARGS__); \
      _lg->log(spdlog::level::err, _msg);                  \
    }                                                      \
  } while (0)
#else
#define LOG__ERROR(logger, ...) void(0)
#endif

#if LOG__LEVEL > ERROR__LVL
#define LOG__WARN(logger, ...)                             \
  do {                                                     \
    const auto& _lg = (logger);                            \
    if (_lg && _lg->should_log(spdlog::level::warn)) {     \
      auto _msg = std::format(projectLocale, __VA_ARGS__); \
      _lg->log(spdlog::level::warn, _msg);                 \
    }                                                      \
  } while (0)
#else
#define LOG__WARN(logger, ...) void(0)
#endif

#if LOG__LEVEL > WARN__LVL
#define LOG__INFO(logger, ...)                             \
  do {                                                     \
    const auto& _lg = (logger);                            \
    if (_lg && _lg->should_log(spdlog::level::info)) {     \
      auto _msg = std::format(projectLocale, __VA_ARGS__); \
      _lg->log(spdlog::level::info, _msg);                 \
    }                                                      \
  } while (0)
#else
#define LOG__INFO(logger, ...) void(0)
#endif

#if LOG__LEVEL > INFO__LVL
#define LOG__DEBUG(logger, ...)                            \
  do {                                                     \
    const auto& _lg = (logger);                            \
    if (_lg && _lg->should_log(spdlog::level::debug)) {    \
      auto _msg = std::format(projectLocale, __VA_ARGS__); \
      _lg->log(spdlog::level::debug, _msg);                \
    }                                                      \
  } while (0)
#else
#define LOG__DEBUG(logger, ...) void(0)
#endif

#if LOG__LEVEL > DEBUG__LVL
#define LOG__TRACE(logger, ...)                            \
  do {                                                     \
    const auto& _lg = (logger);                            \
    if (_lg && _lg->should_log(spdlog::level::trace)) {    \
      auto _msg = std::format(projectLocale, __VA_ARGS__); \
      _lg->log(spdlog::level::trace, _msg);                \
    }                                                      \
  } while (0)
#else
#define LOG__TRACE(logger, ...) void(0)
#endif

namespace common {

  /// Singleton class providing centralized logging for FrankyCPP.
  /// Access via Logger::get() to get the singleton instance, then use
  /// the appropriate logger (e.g., Logger::get().SEARCH_LOG) with LOG__* macros.
  class Logger {
    Logger() { init(); };
    ~Logger() = default;

    /// Initializes all loggers with default patterns and levels.
    void init() const;

  public:
    // disallow copies and moves
    Logger(Logger const&)             = delete;
    Logger& operator=(const Logger&)  = delete;
    Logger(Logger const&&)            = delete;
    Logger& operator=(const Logger&&) = delete;

    /// Returns the singleton Logger instance.
    /// @return Reference to the Logger singleton
    static Logger& get() {
      static Logger instance;
      return instance;
    }

    /// Parses a log level string to spdlog level enum.
    /// Valid strings: "trace", "debug", "info", "warn", "error", "critical", "off".
    /// Defaults to 'warn' on unknown input.
    /// @param level  Log level name (case-insensitive)
    /// @return       Corresponding spdlog level enum
    static spdlog::level::level_enum parseLevel(std::string_view level);

    /// Sets the global spdlog log level.
    /// Does not override individual logger levels that were set explicitly.
    /// @param level  Log level to set globally
    static void setGlobalLevel(spdlog::level::level_enum level);

    /// Sets a specific logger's level.
    /// @param logger  Shared pointer to the logger
    /// @param level   Log level to set
    static void setLoggerLevel(const std::shared_ptr<spdlog::logger>& logger, spdlog::level::level_enum level);

    /// Looks up a logger by name and sets its level.
    /// @param name   Logger name (e.g., "Search_Logger")
    /// @param level  Log level to set
    static void setLoggerLevelByName(std::string_view name, spdlog::level::level_enum level);

    /// Default log pattern: timestamp, thread, logger name, level, message.
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string defaultPattern = "[%H:%M:%S:%f] [t:%-10!t] [%-17n] [%-8l]: %v";

    /// Log file path for general logging (resolved next to the executable).
    const std::string logfile = common::resolvePathRelativeToExe(
      std::format("FrankyCPP_v{}.{}.log", FrankyCPP_VERSION_MAJOR, FrankyCPP_VERSION_MINOR)).string();

    /// Log file path for UCI protocol logging (resolved next to the executable).
    const std::string logfile_uci = common::resolvePathRelativeToExe(
      std::format("FrankyCPP_v{}.{}_uci.log", FrankyCPP_VERSION_MAJOR, FrankyCPP_VERSION_MINOR)).string();

    /// Shared file sink for loggers that write to the main log file.
    const std::shared_ptr<spdlog::sinks::basic_file_sink_mt> sharedFileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile);

    /// Sink for UCI output to stdout with color support.
    const std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> uciOutSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    // clang-format off
  /// Logger for unit tests.
  const std::shared_ptr<spdlog::logger> TEST_LOG    = spdlog::stdout_color_mt("Test_Logger");
  /// Logger for UCI handler operations.
  const std::shared_ptr<spdlog::logger> UCIHAND_LOG = spdlog::stdout_color_mt("UCIHandler_Logger");
  /// Logger for raw UCI protocol messages (file output).
  const std::shared_ptr<spdlog::logger> UCI_LOG     = spdlog::basic_logger_mt("UCI_Logger", logfile_uci);
  /// Logger for opening book operations.
  const std::shared_ptr<spdlog::logger> BOOK_LOG    = spdlog::stdout_color_mt("Book_Logger");
  /// Logger for transposition table operations.
  const std::shared_ptr<spdlog::logger> TT_LOG      = spdlog::stdout_color_mt("TT_Logger");
  /// Logger for search operations.
  const std::shared_ptr<spdlog::logger> SEARCH_LOG  = spdlog::stdout_color_mt("Search_Logger");
  /// Logger for evaluation operations.
  const std::shared_ptr<spdlog::logger> EVAL_LOG    = spdlog::stdout_color_mt("Eval_Logger");
  /// Logger for test suite operations.
  const std::shared_ptr<spdlog::logger> TSUITE_LOG  = spdlog::stdout_color_mt("TSuite_Logger");
  /// Logger for configuration operations.
  const std::shared_ptr<spdlog::logger> CONFIG_LOG  = spdlog::stdout_color_mt("Config_Logger");
  /// Logger for tablebase operations.
  const std::shared_ptr<spdlog::logger> TB_LOG      = spdlog::stdout_color_mt("Tablebase_Logger");
  /// Logger for general application errors and critical issues.
  const std::shared_ptr<spdlog::logger> APP_LOG     = spdlog::stdout_color_mt("App_Logger");
  /// Logger for tuning tools (extractor, optimizer).
  const std::shared_ptr<spdlog::logger> TUNING_LOG  = spdlog::stdout_color_mt("Tuning_Logger");
    // clang-format on
  };

} // namespace common

#endif // FRANKYCPP_LOGGING_H
