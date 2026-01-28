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

// Compile-time log level gating (align with spdlog).
// You can strip debug/trace at compile time by defining SPDLOG_ACTIVE_LEVEL via your build, e.g.:
//   -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO   // removes debug & trace at compile time
// Default to DEBUG so runtime debug remains available unless the build overrides it.
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#endif

#include "version.h"

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
      auto _msg = std::format(deLocale, __VA_ARGS__);      \
      _lg->log(spdlog::level::critical, _msg);             \
    }                                                      \
  } while (0)
#else
#define LOG__CRITICAL(logger, ...) void(0)
#endif

#if LOG__LEVEL > CRITICAL__LVL
#define LOG__ERROR(logger, ...)                       \
  do {                                                \
    const auto& _lg = (logger);                       \
    if (_lg && _lg->should_log(spdlog::level::err)) { \
      auto _msg = std::format(deLocale, __VA_ARGS__); \
      _lg->log(spdlog::level::err, _msg);             \
    }                                                 \
  } while (0)
#else
#define LOG__ERROR(logger, ...) void(0)
#endif

#if LOG__LEVEL > ERROR__LVL
#define LOG__WARN(logger, ...)                         \
  do {                                                 \
    const auto& _lg = (logger);                        \
    if (_lg && _lg->should_log(spdlog::level::warn)) { \
      auto _msg = std::format(deLocale, __VA_ARGS__);  \
      _lg->log(spdlog::level::warn, _msg);             \
    }                                                  \
  } while (0)
#else
#define LOG__WARN(logger, ...) void(0)
#endif

#if LOG__LEVEL > WARN__LVL
#define LOG__INFO(logger, ...)                         \
  do {                                                 \
    const auto& _lg = (logger);                        \
    if (_lg && _lg->should_log(spdlog::level::info)) { \
      auto _msg = std::format(deLocale, __VA_ARGS__);  \
      _lg->log(spdlog::level::info, _msg);             \
    }                                                  \
  } while (0)
#else
#define LOG__INFO(logger, ...) void(0)
#endif

#if LOG__LEVEL > INFO__LVL
#define LOG__DEBUG(logger, ...)                         \
  do {                                                  \
    const auto& _lg = (logger);                         \
    if (_lg && _lg->should_log(spdlog::level::debug)) { \
      auto _msg = std::format(deLocale, __VA_ARGS__);   \
      _lg->log(spdlog::level::debug, _msg);             \
    }                                                   \
  } while (0)
#else
#define LOG__DEBUG(logger, ...) void(0)
#endif

#if LOG__LEVEL > DEBUG__LVL
#define LOG__TRACE(logger, ...)                         \
  do {                                                  \
    const auto& _lg = (logger);                         \
    if (_lg && _lg->should_log(spdlog::level::trace)) { \
      auto _msg = std::format(deLocale, __VA_ARGS__);   \
      _lg->log(spdlog::level::trace, _msg);             \
    }                                                   \
  } while (0)
#else
#define LOG__TRACE(logger, ...) void(0)
#endif

/** Singleton class for Logger */
class Logger {
  Logger() { init(); };
  ~Logger() = default;
  void init() const;

public:
  // disallow copies
  Logger(Logger const&)             = delete;// copy
  Logger& operator=(const Logger&)  = delete;// copy assignment
  Logger(Logger const&&)            = delete;// move
  Logger& operator=(const Logger&&) = delete;// move assignment

  /** get the singleton instance of Logger */
  static Logger& get() {
    static Logger instance;
    return instance;
  }

  // Convenience helpers for spdlog level handling and per-logger control.
  // - parseLevel: convert user-provided string to spdlog level, defaults to 'warn' on unknown.
  static spdlog::level::level_enum parseLevel(std::string_view);
  // - setGlobalLevel: set spdlog's global level (does not override individual logger levels).
  static void setGlobalLevel(spdlog::level::level_enum);
  // - setLoggerLevel: set a specific logger's level when you have the pointer.
  static void setLoggerLevel(const std::shared_ptr<spdlog::logger>&, spdlog::level::level_enum);
  // - setLoggerLevelByName: look up a logger by name and set its level.
  static void setLoggerLevelByName(std::string_view, spdlog::level::level_enum);

  const std::string defaultPattern = "[%H:%M:%S:%f] [t:%-10!t] [%-17n] [%-8l]: %v";

  const std::string logfile     = std::format("FrankyCPP_v{}.{}.log", FrankyCPP_VERSION_MAJOR, FrankyCPP_VERSION_MINOR);
  const std::string logfile_uci = std::format("FrankyCPP_v{}.{}_uci.log", FrankyCPP_VERSION_MAJOR, FrankyCPP_VERSION_MINOR);

  const std::shared_ptr<spdlog::sinks::basic_file_sink_mt> sharedFileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile);
  const std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> uciOutSink   = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

  // clang-format off
  //  const std::shared_ptr<spdlog::logger> MAIN_LOG    = spdlog::stdout_color_mt("Main_Logger");
  const std::shared_ptr<spdlog::logger> TEST_LOG    = spdlog::stdout_color_mt("Test_Logger");
  const std::shared_ptr<spdlog::logger> UCIHAND_LOG = spdlog::stdout_color_mt("UCIHandler_Logger");
  const std::shared_ptr<spdlog::logger> UCI_LOG     = spdlog::basic_logger_mt("UCI_Logger", logfile_uci);
  const std::shared_ptr<spdlog::logger> BOOK_LOG    = spdlog::stdout_color_mt("Book_Logger");
  const std::shared_ptr<spdlog::logger> TT_LOG      = spdlog::stdout_color_mt("TT_Logger");
  const std::shared_ptr<spdlog::logger> SEARCH_LOG  = spdlog::stdout_color_mt("Search_Logger");
  const std::shared_ptr<spdlog::logger> EVAL_LOG    = spdlog::stdout_color_mt("Eval_Logger");
  const std::shared_ptr<spdlog::logger> TSUITE_LOG  = spdlog::stdout_color_mt("TSuite_Logger");
  const std::shared_ptr<spdlog::logger> CONFIG_LOG  = spdlog::stdout_color_mt("Config_Logger");
  // clang-format on
};

#endif// FRANKYCPP_LOGGING_H
