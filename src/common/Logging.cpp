// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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

#include <iostream>
#include <chrono>
#include "Logging.h"

// BOOST program options
#include "boost/program_options.hpp"
namespace po = boost::program_options;

inline po::variables_map programOptions{};

// Local helper: add sink to logger only if it's not already present.
// We compare the underlying pointer addresses to avoid duplicates when init() is called multiple times.
static void add_unique_sink(spdlog::logger &logger,
                            const std::shared_ptr<spdlog::sinks::sink> &sink) {
  auto &sinks = logger.sinks();
  for (const auto &s : sinks) {
    if (s.get() == sink.get()) return; // already present
  }
  sinks.push_back(sink);
}

// New local helper: consistently (re)configure a logger with sink, pattern, level, and flush policy.
// Centralizing this avoids drift across loggers and keeps init() concise. No behavior change intended.
static void configure_logger(const std::shared_ptr<spdlog::logger>& lg,
                             const spdlog::level::level_enum level,
                             const std::string& pattern,
                             const std::shared_ptr<spdlog::sinks::sink>& fileSink,
                             const spdlog::level::level_enum flushLevel) {
  if (!lg) return;
  if (fileSink) {
    add_unique_sink(*lg, fileSink);
  }
  lg->set_pattern(pattern);
  lg->set_level(level);
  lg->flush_on(flushLevel);
}

// Convert user text (e.g. "info", "warn", "trace") into a spdlog level.
// Uses spdlog::level::from_str to stay aligned with accepted aliases; falls back to 'warn' on unknown.
spdlog::level::level_enum Logger::parseLevel(const std::string_view s) {
  const auto lvl = spdlog::level::from_str(std::string{s});
  if (lvl == spdlog::level::off && s != "off") {
    return spdlog::level::warn;
  }
  return lvl;
}

// Set spdlog's global level. This affects default filtering for all loggers not having explicit levels.
// Optionally, we also make warn+ flush immediately to bound data loss on crashes.
void Logger::setGlobalLevel(const spdlog::level::level_enum lvl) {
  spdlog::set_level(lvl);
  spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& lg){
    if (lg) lg->flush_on(spdlog::level::warn);
  });
}

// Set a particular logger's level when you already have the pointer.
void Logger::setLoggerLevel(const std::shared_ptr<spdlog::logger>& lg, const spdlog::level::level_enum lvl) {
  if (lg) lg->set_level(lvl);
}

// Look up a logger by name (spdlog::get) and delegate to setLoggerLevel.
void Logger::setLoggerLevelByName(const std::string_view name, const spdlog::level::level_enum lvl) {
  if (name.empty()) return;
  if (const auto lg = spdlog::get(std::string{name})) {
    setLoggerLevel(lg, lvl);
  }
}

void Logger::init() const {

  // Parse requested log levels using spdlog's canonical parser.
  // Using from_str centralizes accepted names (e.g., "warn", "error") and keeps behavior in sync with spdlog.
  auto get_level = [](const char *opt_name, std::string &lvl_str) -> spdlog::level::level_enum {
    const auto lvl = spdlog::level::from_str(lvl_str);
    // If parsing returned 'off' but the user didn't explicitly request 'off', treat as invalid and fall back.
    // This produces a single, clear message and avoids surprising silent fallbacks.
    if (lvl == spdlog::level::off && lvl_str != "off") {
      std::cerr << "Invalid value for '" << opt_name << "' ('" << lvl_str << "') - falling back to 'warn'.\n";
      lvl_str = "warn";
      return spdlog::level::warn;
    }
    return lvl;
  };

  // Get level strings from programOptions; default to "warn" if missing.
  std::string logLvL = "warn";
  std::string searchLogLvL = "warn";
  if (!programOptions.empty()) {
    if (programOptions.contains("log_lvl")) {
      logLvL = programOptions["log_lvl"].as<std::string>();
    }
    if (programOptions.contains("search_log_lvl")) {
      searchLogLvL = programOptions["search_log_lvl"].as<std::string>();
    }
  }

  const auto logLevel = get_level("log_lvl", logLvL);
  const auto searchLogLevel = get_level("search_log_lvl", searchLogLvL);

  // Global log level and pattern once.
  spdlog::set_level(logLevel);
  spdlog::set_pattern(defaultPattern); // set the default pattern globally once

  // Shared file sink follows the global default log level
  sharedFileSink->set_level(logLevel);

  // Non-UCI loggers flush on warn to reduce I/O while still flushing important messages promptly.
  constexpr auto nonUciFlushLevel = spdlog::level::warn;

  // Configure repeated logger blocks via helper
  configure_logger(SEARCH_LOG,  searchLogLevel, defaultPattern, sharedFileSink, nonUciFlushLevel);
  configure_logger(TSUITE_LOG,  logLevel,       defaultPattern, sharedFileSink, nonUciFlushLevel);
  configure_logger(EVAL_LOG,    logLevel,       defaultPattern, sharedFileSink, nonUciFlushLevel);
  configure_logger(TT_LOG,      logLevel,       defaultPattern, sharedFileSink, nonUciFlushLevel);
  configure_logger(UCIHAND_LOG, logLevel,       defaultPattern, sharedFileSink, nonUciFlushLevel);
  configure_logger(BOOK_LOG,    logLevel,       defaultPattern, sharedFileSink, nonUciFlushLevel);

  // UCI logger keeps its dedicated console sink and its own simple pattern
  add_unique_sink(*UCI_LOG, uciOutSink);
  UCI_LOG->set_pattern("[%H:%M:%S:%f] %v");
  UCI_LOG->set_level(spdlog::level::trace); // keep as-is (trace) for UCI
  UCI_LOG->flush_on(spdlog::level::trace);

  // Logger for Unit Tests (stdout logger only, no file sink wiring)
  TEST_LOG->set_level(logLevel);
  TEST_LOG->flush_on(nonUciFlushLevel);

  // Proactive periodic flushing to bound data loss on crashes without flushing on every message.
  spdlog::flush_every(milliseconds(200));

  std::cout << "Logger initialized (" << logLvL << " / " << searchLogLvL << ")" << std::endl;
}
