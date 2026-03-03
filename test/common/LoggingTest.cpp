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

// FrankyCPP - Logging macro behavior tests
// Ensures macros are single-statement safe, lazy, and respect should_log/null loggers.

#include <gtest/gtest.h>

#include "common/Logging.h"
#include "types/globals.h"// for deLocale used by Logging macros

#include <memory>
#include <string>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

using namespace common;

namespace {

  // Simple sink that captures message payloads for assertions.
  template<typename Mutex>
  class vector_sink final : public spdlog::sinks::base_sink<Mutex> {
  public:
    std::vector<std::string> messages;

  protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
      spdlog::memory_buf_t buf;
      this->formatter_->format(msg, buf);
      messages.emplace_back(buf.data(), buf.size());
    }
    void flush_() override {}
  };

  using vector_sink_mt = vector_sink<spdlog::details::null_mutex>;

  std::shared_ptr<spdlog::logger> make_test_logger(const std::string& name,
                                                   std::shared_ptr<vector_sink_mt>& out_sink) {
    out_sink    = std::make_shared<vector_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>(name, out_sink);
    // Keep default pattern at logger level; payload we capture is message content only.
    logger->set_level(spdlog::level::trace);
    return logger;
  }

}// namespace

TEST(LoggingMacros, IfElseWithoutBracesIsSafe) {
  std::shared_ptr<vector_sink_mt> sink;
  const auto logger = make_test_logger("IfElse", sink);

  bool cond = true;
  if (cond)
    LOG__INFO(logger, "hello {}", 42);
  else
    LOG__INFO(logger, "else path");

  ASSERT_EQ(sink->messages.size(), 1u);
  EXPECT_NE(sink->messages[0].find("hello 42"), std::string::npos);
}

TEST(LoggingMacros, NullLoggerIsNoop) {
  // Ensure calling macros with a null logger doesn't crash and does nothing.
  std::shared_ptr<spdlog::logger> null_logger;

  // These should all be no-ops.
  LOG__TRACE(null_logger, "trace {}", 1);
  LOG__DEBUG(null_logger, "debug {}", 2);
  LOG__INFO(null_logger, "info {}", 3);
  LOG__WARN(null_logger, "warn {}", 4);
  LOG__ERROR(null_logger, "error {}", 5);
  LOG__CRITICAL(null_logger, "critical {}", 6);

  SUCCEED();// If we got here without crashing, the test passes.
}

TEST(LoggingMacros, ShouldLogGatingPreventsLowerLevels) {
  std::shared_ptr<vector_sink_mt> sink;
  const auto logger = make_test_logger("Gating", sink);

  // Set logger to warn; info should not be logged, warn should be.
  logger->set_level(spdlog::level::warn);

  LOG__INFO(logger, "info message");
  LOG__WARN(logger, "warn message {}", 7);

  ASSERT_EQ(sink->messages.size(), 1u);
  EXPECT_NE(sink->messages[0].find("warn message 7"), std::string::npos);
}

// Optional compile-time gating test: only builds when TRACE is compiled out by default gating
#if LOG__LEVEL <= DEBUG__LVL
TEST(LoggingMacros, CompileTimeGatingDiscardsArgsWhenTraceDisabled) {
  // Deliberately non-constructible type: if the macro attempted to format this,
  // it would not compile. Because TRACE is compiled out, this code still compiles.
  struct Explode {
    Explode() = delete;
  };

  // If LOG__TRACE were active, attempting to format Explode{} would be a compile error.
  LOG__TRACE(nullptr, "won't be formatted: {}", Explode{});

  SUCCEED();
}
#endif// LOG__LEVEL <= DEBUG__LVL

TEST(LoggerRuntime, LevelChangeByPtrAndName) {
  std::shared_ptr<vector_sink_mt> sink;
  const std::string name = "RuntimeLevelTest";
  auto logger            = make_test_logger(name, sink);

  // Register so setLoggerLevelByName can find it.
  spdlog::register_logger(logger);

  // Start with WARN: info should be filtered, warn should pass.
  Logger::setLoggerLevel(logger, spdlog::level::warn);
  sink->messages.clear();
  LOG__INFO(logger, "info-should-not-appear");
  LOG__WARN(logger, "warn-should-appear");
  ASSERT_EQ(sink->messages.size(), 1u);
  EXPECT_NE(sink->messages[0].find("warn-should-appear"), std::string::npos);

  // Raise to INFO via pointer: info should now be logged.
  Logger::setLoggerLevel(logger, spdlog::level::info);
  sink->messages.clear();
  LOG__INFO(logger, "info-now-visible");
  ASSERT_EQ(sink->messages.size(), 1u);
  EXPECT_NE(sink->messages[0].find("info-now-visible"), std::string::npos);

  // Raise to DEBUG via name lookup and parseLevel("debug").
  const auto parsed = Logger::parseLevel("debug");
  EXPECT_EQ(parsed, spdlog::level::debug);
  Logger::setLoggerLevelByName(name, parsed);
  sink->messages.clear();
  LOG__DEBUG(logger, "debug-now-visible");
  ASSERT_EQ(sink->messages.size(), 1u);
  EXPECT_NE(sink->messages[0].find("debug-now-visible"), std::string::npos);

  // Invalid parseLevel input should fall back to 'warn'.
  EXPECT_EQ(Logger::parseLevel("definitely-not-a-level"), spdlog::level::warn);

  // Cleanup registry.
  spdlog::drop(name);
}
