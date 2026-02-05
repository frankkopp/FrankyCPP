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

#ifndef FRANKYCPP_ENGINE_ARENA_CONSOLECOLORS_H
#define FRANKYCPP_ENGINE_ARENA_CONSOLECOLORS_H

//=============================================================================
// ConsoleColors.h - ANSI Color Support for Console Output
//=============================================================================
//
// Provides ANSI escape codes for colorful console output in reports.
// Colors are automatically disabled if output is not to a terminal.
//
// Usage:
//   std::cout << Color::GREEN << "Success!" << Color::RESET << std::endl;
//   std::cout << Color::colorize("Error!", Color::RED) << std::endl;
//
//=============================================================================

#include <string>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace arena {

/// ANSI color codes for console output
namespace Color {

// Check if stdout is a terminal (enables colors)
inline bool isTerminal() {
  static bool checked = false;
  static bool isTTY = false;
  if (!checked) {
    isTTY = isatty(fileno(stdout)) != 0;
    checked = true;
  }
  return isTTY;
}

// ANSI escape codes (empty if not terminal)
inline const char* RESET   = "\033[0m";
inline const char* BOLD    = "\033[1m";
inline const char* DIM     = "\033[2m";

// Foreground colors
inline const char* RED     = "\033[31m";
inline const char* GREEN   = "\033[32m";
inline const char* YELLOW  = "\033[33m";
inline const char* BLUE    = "\033[34m";
inline const char* MAGENTA = "\033[35m";
inline const char* CYAN    = "\033[36m";
inline const char* WHITE   = "\033[37m";

// Bright foreground colors
inline const char* BRIGHT_RED     = "\033[91m";
inline const char* BRIGHT_GREEN   = "\033[92m";
inline const char* BRIGHT_YELLOW  = "\033[93m";
inline const char* BRIGHT_BLUE    = "\033[94m";
inline const char* BRIGHT_MAGENTA = "\033[95m";
inline const char* BRIGHT_CYAN    = "\033[96m";
inline const char* BRIGHT_WHITE   = "\033[97m";

/// Returns color code if terminal, empty string otherwise
inline std::string color(const char* code) {
  return isTerminal() ? code : "";
}

/// Wraps text with color code and reset
inline std::string colorize(const std::string& text, const char* colorCode) {
  if (!isTerminal()) return text;
  return std::string(colorCode) + text + RESET;
}

/// Returns green text for positive values, red for negative, yellow for zero
inline std::string colorDelta(const std::string& text, int delta) {
  if (!isTerminal()) return text;
  if (delta > 0) return std::string(GREEN) + text + RESET;
  if (delta < 0) return std::string(RED) + text + RESET;
  return std::string(YELLOW) + text + RESET;
}

/// Returns green text for positive values, red for negative, yellow for zero
inline std::string colorDelta(const std::string& text, double delta, double threshold = 0.001) {
  if (!isTerminal()) return text;
  if (delta > threshold) return std::string(GREEN) + text + RESET;
  if (delta < -threshold) return std::string(RED) + text + RESET;
  return std::string(YELLOW) + text + RESET;
}

} // namespace Color

/// Unicode symbols for quick visual indicators
namespace Symbol {
  inline const char* CHECK     = "✅";  // Improvement/pass
  inline const char* CROSS     = "❌";  // Regression/fail
  inline const char* EQUAL     = "⚖️";   // Equal/no change
  inline const char* WARNING   = "⚠️";   // Warning
  inline const char* ARROW_UP  = "▲";   // Improvement
  inline const char* ARROW_DOWN= "▼";   // Regression
  inline const char* DASH      = "—";   // N/A or missing
}

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_CONSOLECOLORS_H
