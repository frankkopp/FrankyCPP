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

#ifndef FRANKYCPP_MISC_H
#define FRANKYCPP_MISC_H

//=============================================================================
// misc.h - Miscellaneous Utility Functions
//=============================================================================
//
// Contains various utility functions that don't fit into other categories.
//
// Functions:
//   getEnv()         - Safe, cross-platform environment variable access
//   printProgress()  - Generate ASCII progress bar string
//
//=============================================================================

#include <format>
#include <optional>
#include <string>

#ifdef _WIN32
#include <cstdlib> // For _dupenv_s
#else
#include <cstdlib> // For getenv
#include <mutex>
#endif

namespace common {

  /// Safe, thread-safe, cross-platform environment variable access.
  /// On Windows, uses _dupenv_s (thread-safe).
  /// On POSIX, uses getenv with mutex protection.
  /// @param name  Environment variable name
  /// @return      Value if set, empty string if not set or on error
  [[nodiscard]] inline std::string getEnv(const char* name) {
    if (name == nullptr) {
      return "";
    }

#ifdef _WIN32
    // Windows: Use _dupenv_s which is thread-safe and allocates a copy
    char* buffer      = nullptr;
    size_t size       = 0;
    const errno_t err = _dupenv_s(&buffer, &size, name);
    if (err != 0 || buffer == nullptr) {
      return "";
    }
    std::string result(buffer);
    free(buffer);
    return result;
#else
    // POSIX: getenv is not guaranteed thread-safe, so protect with mutex
    // Note: This only protects against concurrent getenv calls within this process,
    // not against external environment modifications (which are inherently unsafe)
    static std::mutex envMutex;
    std::lock_guard<std::mutex> lock(envMutex);
    const char* value = std::getenv(name);
    return (value != nullptr) ? std::string(value) : "";
#endif
  }

  /// Safe environment variable access with optional return.
  /// @param name  Environment variable name
  /// @return      std::optional containing value if set, std::nullopt otherwise
  [[nodiscard]] inline std::optional<std::string> getEnvOpt(const char* name) {
    if (name == nullptr) {
      return std::nullopt;
    }

#ifdef _WIN32
    char* buffer      = nullptr;
    size_t size       = 0;
    const errno_t err = _dupenv_s(&buffer, &size, name);
    if (err != 0 || buffer == nullptr) {
      return std::nullopt;
    }
    std::string result(buffer);
    free(buffer);
    return result;
#else
    static std::mutex envMutex;
    std::lock_guard<std::mutex> lock(envMutex);
    const char* value = std::getenv(name);
    return (value != nullptr) ? std::optional<std::string>(value) : std::nullopt;
#endif
  }

  /// Generates an ASCII progress bar string.
  /// @param percentage  Progress value from 0.0 to 1.0
  /// @return            Formatted string like "50% [||||||||||          ]"
  inline std::string printProgress(const double percentage) {
    constexpr int pbarw = 60;
    constexpr auto pbar = "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||";

    const int val  = static_cast<int>(percentage * 100);
    const int lpad = static_cast<int>(percentage * pbarw);
    const int rpad = pbarw - lpad;

    return std::format("%3d%% [%.*s%*s]", val, lpad, pbar, rpad, "");
  }

} // namespace common

#endif // FRANKYCPP_MISC_H
