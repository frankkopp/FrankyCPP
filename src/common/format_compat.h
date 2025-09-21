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

// FrankyCPP
// std::format compatibility shim to migrate away from external fmt
// - Provides fmt::format mapped to std::format when external fmt is absent
// - Provides fmt::format(locale, ...) via std::vformat when external fmt is absent
// - Provides fmt::localtime(time_t) helper compatible with existing call sites

#ifndef FRANKYCPP_FORMAT_COMPAT_H
#define FRANKYCPP_FORMAT_COMPAT_H

#include <ctime>
#include <format>
#include <locale>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <type_traits>

// If external fmt is available (e.g., via spdlog with SPDLOG_FMT_EXTERNAL),
// fmt headers define FMT_VERSION. In that case, do not inject fmt:: symbols.
#ifndef FMT_VERSION
namespace fmt {

// Wrapper for std::vformat (no locale); supports runtime format strings
template <typename... Args>
std::string format(std::string_view fmt_str, Args&&... args) {
    auto tup = std::make_tuple(std::forward<Args>(args)...);
    auto fargs = std::apply([](auto&... elems) {
        return std::make_format_args(elems...);
    }, tup);
    return std::vformat(fmt_str, fargs);
}

// Locale-aware overload using std::vformat
template <typename... Args>
std::string format(const std::locale& loc, std::string_view fmt_str, Args&&... args) {
    auto tup = std::make_tuple(std::forward<Args>(args)...);
    auto fargs = std::apply([](auto&... elems) {
        return std::make_format_args(elems...);
    }, tup);
    return std::vformat(loc, fmt_str, fargs);
}

// Minimal replacement for fmt::localtime(time_t) returning thread-safe std::tm copy
inline std::tm localtime(std::time_t t) {
#if defined(_WIN32)
    std::tm tm{};
    localtime_s(&tm, &t);
    return tm;
#else
    std::tm tm{};
    ::localtime_r(&t, &tm);
    return tm;
#endif
}

} // namespace fmt
#endif // !FMT_VERSION

// Formatting helpers that always use std::vformat
/**
 * \brief Formatting helpers built on top of \c std::vformat.
 *
 * These utilities accept runtime format strings and forward all arguments to
 * \c std::vformat, mirroring \c std::format placeholder semantics.
 *
 * - Placeholders follow the C++20 \c std::format syntax.
 * - On errors, \c std::format_error is thrown.
 * - \c sformat uses locale-independent formatting (like \c std::format).
 * - \c lformat applies the provided \c std::locale.
 */
namespace fstr {

  /**
   * \brief Formats a string using \c std::vformat with the global locale.
   *
   * \tparam Args Variadic argument types.
   * \param fmt_str Runtime format string using \c std::format syntax.
   * \param args Arguments to be formatted.
   * \return The formatted \c std::string.
   * \throws std::format_error If the format string is invalid or formatting fails.
   *
   * \code
   * auto s = fstr::sformat("value = {}, hex = {:#x}", 42, 42);
   * \endcode
   */
  template <typename... Args>
  std::string sformat(std::string_view fmt_str, Args&&... args) {
    auto tup = std::make_tuple(std::forward<Args>(args)...);
    auto fargs = std::apply([](auto&... elems) {
      return std::make_format_args(elems...);
    }, tup);
    return std::vformat(fmt_str, fargs);
  }

  /**
   * \brief Formats a string using \c std::vformat with an explicit locale.
   *
   * \tparam Args Variadic argument types.
   * \param loc The \c std::locale to use for locale-sensitive formatting.
   * \param fmt_str Runtime format string using \c std::format syntax.
   * \param args Arguments to be formatted.
   * \return The formatted \c std::string.
   * \throws std::format_error If the format string is invalid or formatting fails.
   *
   * \code
   * std::locale loc{"C"};
   * auto s = fstr::lformat(loc, "number: {:L}", 1234567);
   * \endcode
   */
  template <typename... Args>
  std::string lformat(const std::locale& loc, std::string_view fmt_str, Args&&... args) {
    auto tup = std::make_tuple(std::forward<Args>(args)...);
    auto fargs = std::apply([](auto&... elems) {
      return std::make_format_args(elems...);
    }, tup);
    return std::vformat(loc, fmt_str, fargs);
  }
}

#endif // FRANKYCPP_FORMAT_COMPAT_H
