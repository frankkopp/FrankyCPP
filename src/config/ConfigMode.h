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

#ifndef FRANKYCPP_CONFIGMODE_H
#define FRANKYCPP_CONFIGMODE_H

//=============================================================================
// ConfigMode.h - Conditional static constexpr for configuration values
//=============================================================================
//
// In PRODUCTION builds (-DFRANKYCPP_PRODUCTION):
//   - Non-essential config values become static constexpr (compile-time constants).
//   - Compiler eliminates dead branches and inlines values automatically.
//   - No runtime config changes possible for non-essential options.
//   - Non-essential options are not registered in UCI/YAML.
//   - CONFIG_OVERRIDE / applyOverrides() are not available; any usage causes
//     a compile error — intentional, catches accidental misuse.
//
// In DEVELOPMENT builds (default, no -DFRANKYCPP_PRODUCTION):
//   - Config values are non-static mutable instance members (runtime changeable).
//   - Full UCI/YAML/CONFIG_OVERRIDE support.
//   - All debugging/tuning features enabled.
//
// NOTE: C++ requires constexpr data members to be static. CONFIG_CONST expands
// to "static constexpr" in production and to nothing in development. Existing
// code that reads config values via an instance (e.g. SearchConfig.USE_LMR)
// works unchanged in both builds — C++ allows accessing static members through
// an instance, and the compiler inlines the constant value in production.
//
// Usage in structs:
//   struct SearchConfigData {
//       CONFIG_CONST bool   USE_LMR       = true;   // frozen in production
//       CONFIG_CONST int    LMR_MIN_DEPTH = 2;
//       CONFIG_ESSENTIAL int TT_SIZE_MB   = 64;     // always mutable
//   };
//
// Accidental misclassification is self-guarding:
//   If an essential config is wrongly marked CONFIG_CONST, its registry setter
//   (e.g. s.TT_SIZE_MB = parseInt(v)) will fail to compile in production because
//   you cannot assign to a static constexpr member.
//=============================================================================

#ifdef FRANKYCPP_PRODUCTION

// Production: non-essential config values become compile-time constants.
#define CONFIG_CONST static constexpr

// Code that must be compiled only in development builds.
#define DEV_ONLY(code)

// Code that must be compiled only in production builds.
#define PROD_ONLY(code) code

// Compile-time query: is config frozen?
#define IS_CONFIG_FROZEN true

#else // Development build

// Development: non-essential config values remain mutable instance members.
#define CONFIG_CONST

// Development-only code block.
#define DEV_ONLY(code) code

// Production-only code block (omitted in development).
#define PROD_ONLY(code)

// Compile-time query: is config frozen?
#define IS_CONFIG_FROZEN false

#endif // FRANKYCPP_PRODUCTION

// Essential configs — always non-static mutable instance members in all builds.
// This macro is purely documentary; it expands to nothing.
// Mark members that must stay runtime-mutable (e.g. TT size, file paths,
// pondering flag) with CONFIG_ESSENTIAL so their intent is clear in the struct.
#define CONFIG_ESSENTIAL

// Compile-time check: is a struct member mutable (i.e. CONFIG_ESSENTIAL)?
// Uses decltype to inspect the member's type via a default-constructed instance.
// - In production, CONFIG_CONST members are static constexpr → type is const → returns false.
// - In development, CONFIG_CONST members are plain mutable → type is non-const → returns true.
// - CONFIG_ESSENTIAL members are always mutable → always returns true.
// This allows ConfigRegistry to auto-derive UCI exposure from the struct declaration,
// avoiding a second source of truth.
// Usage: IS_MUTABLE(defaultSearch, USE_NMP)  → false in production, true in development
// NOLINTNEXTLINE(bugprone-macro-parentheses) — member access requires unparenthesized args
#define IS_MUTABLE(inst, member) (!std::is_const_v<decltype(inst.member)>)

//=============================================================================
// Statistics macros
//
// In PRODUCTION builds, non-essential stat operations compile to no-ops,
// eliminating all counter overhead from hot paths.
// Essential stats (nodes, depth, time — needed for UCI info output) use the
// ESSENTIAL_STAT_* variants, which are always active.
//=============================================================================

#ifdef FRANKYCPP_PRODUCTION
#define STAT_INC(counter) ((void) 0)
#define STAT_ADD(counter, val) ((void) 0)
#define STAT_SET(counter, val) ((void) 0)
#else
#define STAT_INC(counter) (++(counter))
#define STAT_ADD(counter, val) ((counter) += (val))
#define STAT_SET(counter, val) ((counter) = (val))
#endif

// Essential stats — always collected regardless of build mode.
#define ESSENTIAL_STAT_INC(counter) (++(counter))
#define ESSENTIAL_STAT_ADD(counter, val) ((counter) += (val))
#define ESSENTIAL_STAT_SET(counter, val) ((counter) = (val))

//=============================================================================
// TT/PawnTT hot-path statistics macros  (R5 — SMP false sharing investigation)
//
// TT::probe() and TT::put() increment shared mutable counters (numberOfProbes,
// numberOfPuts, numberOfHits, etc.) on every call from every thread. These
// counters share cache lines with critical read-only fields (_data pointer,
// clusterMask, hashKeyMask), causing false sharing under SMP.
//
// Set TT_STATS_ENABLED to 0 and rebuild to compile out ALL TT/PawnTT
// diagnostic counters. Then re-run bench_thread_scaling to measure the
// NPS impact of false sharing.
//
// When disabled:
//   - TT::str() / PawnTT::str() will report all zeros
//   - hashFull() returns 0 (numberOfEntries not tracked)
//   - No functional impact on search correctness
//=============================================================================
#define TT_STATS_ENABLED 1

#if TT_STATS_ENABLED
#define TT_STAT_INC(counter) (++(counter))
#else
#define TT_STAT_INC(counter) ((void) 0)
#endif

//=============================================================================
// CONFIG SETTER macros for ConfigRegistry setter lambdas
//
// SEARCH_CONFIG_SETTER(member, parser) / EVAL_CONFIG_SETTER(member, parser)
//   Expands to a complete setter lambda that works in both builds:
//   - CONFIG_ESSENTIAL members (plain mutable instance members): assigned in all builds.
//   - CONFIG_CONST members (static constexpr in production): assignment silently
//     skipped via the assignIfMutable<> template helper below.
//
//   The key insight: `if constexpr` only suppresses instantiation inside templates.
//   By routing the assignment through a templated helper, GCC/Clang no longer
//   type-check the discarded branch, so assigning to a static constexpr member
//   is never seen by the compiler.
//
// SEARCH_CONFIG_ARRAY_SETTER(member)
//   Same idea but for std::array members that use parseArray(v, member) in-place.
//
// Usage:
//   .setter = SEARCH_CONFIG_SETTER(USE_LMR, parseBool)
//   .setter = EVAL_CONFIG_SETTER(TEMPO, parseInt)
//   .setter = SEARCH_CONFIG_ARRAY_SETTER(RFP_MARGIN)
//=============================================================================

#include <string>
#include <type_traits>

namespace config::config_detail {

  // Assigns `value` to `dest` only if T is non-const (i.e. CONFIG_ESSENTIAL).
  // When T is const (CONFIG_CONST = static constexpr in production), the discarded
  // branch is never instantiated because this is a template — GCC/Clang won't
  // type-check the assignment, avoiding the "read-only variable" error.
  template<typename T, typename U>
  void assignIfMutable(T& dest, U&& value) {
    if constexpr (!std::is_const_v<T>) {
      dest = std::forward<U>(value);
    }
  }

  // Array variant: calls parseArray(v, dest) only if T is non-const.
  template<typename T>
  void assignArrayIfMutable(T& dest, const std::string& v) {
    if constexpr (!std::is_const_v<T>) {
      parseArray(v, dest);
    }
  }

} // namespace config::config_detail

#define SEARCH_CONFIG_SETTER(member, parser)                       \
  [](SearchConfigData& s, EvalConfigData&, const std::string& v) { \
    config::config_detail::assignIfMutable(s.member, parser(v));   \
  }

#define EVAL_CONFIG_SETTER(member, parser)                         \
  [](SearchConfigData&, EvalConfigData& e, const std::string& v) { \
    config::config_detail::assignIfMutable(e.member, parser(v));   \
  }

#define SEARCH_CONFIG_ARRAY_SETTER(member)                         \
  [](SearchConfigData& s, EvalConfigData&, const std::string& v) { \
    config::config_detail::assignArrayIfMutable(s.member, v);      \
  }

#define EVAL_CONFIG_ARRAY_SETTER(member)                           \
  [](SearchConfigData&, EvalConfigData& e, const std::string& v) { \
    config::config_detail::assignArrayIfMutable(e.member, v);      \
  }

#endif // FRANKYCPP_CONFIGMODE_H
