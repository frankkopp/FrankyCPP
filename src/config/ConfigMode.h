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

//=============================================================================
// Statistics macros
//
// In PRODUCTION builds, non-essential stat operations compile to no-ops,
// eliminating all counter overhead from hot paths.
// Essential stats (nodes, depth, time — needed for UCI info output) use the
// ESSENTIAL_STAT_* variants, which are always active.
//=============================================================================

#ifdef FRANKYCPP_PRODUCTION
  #define STAT_INC(counter)         ((void)0)
  #define STAT_ADD(counter, val)    ((void)0)
  #define STAT_SET(counter, val)    ((void)0)
#else
  #define STAT_INC(counter)         (++(counter))
  #define STAT_ADD(counter, val)    ((counter) += (val))
  #define STAT_SET(counter, val)    ((counter) = (val))
#endif

// Essential stats — always collected regardless of build mode.
#define ESSENTIAL_STAT_INC(counter)         (++(counter))
#define ESSENTIAL_STAT_ADD(counter, val)    ((counter) += (val))
#define ESSENTIAL_STAT_SET(counter, val)    ((counter) = (val))

#endif // FRANKYCPP_CONFIGMODE_H
