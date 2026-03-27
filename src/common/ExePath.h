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

#ifndef FRANKYCPP_EXEPATH_H
#define FRANKYCPP_EXEPATH_H

//=============================================================================
// ExePath.h - Executable Path Utilities
//=============================================================================
//
// Provides utilities for resolving paths relative to the running executable's
// directory rather than the current working directory. This ensures the engine
// loads its own config/books/logs regardless of how it is launched (via
// cutechess-cli, a GUI, or from a different shell directory).
//
// The implementation uses platform-specific APIs:
//   - Windows: GetModuleFileNameW()
//   - Linux:   readlink("/proc/self/exe")
//
// Falls back to std::filesystem::current_path() if the platform API fails.
//
// Functions:
//   getExecutableDir()         - Cached directory of the running executable
//   resolvePathRelativeToExe() - Resolve a relative path against the exe dir
//
//=============================================================================

#include <filesystem>

namespace common {

  /// Returns the directory containing the running executable.
  /// Uses GetModuleFileNameW (Windows) or /proc/self/exe (Linux).
  /// Falls back to current_path() if the platform API fails.
  /// Result is cached after the first call.
  [[nodiscard]] const std::filesystem::path& getExecutableDir();

  /// Resolves a potentially relative path against the executable's directory.
  /// If the path is already absolute it is returned unchanged.
  /// @param relPath  Relative or absolute path
  /// @return         Absolute path resolved against getExecutableDir()
  [[nodiscard]] std::filesystem::path resolvePathRelativeToExe(const std::filesystem::path& relPath);

} // namespace common

#endif // FRANKYCPP_EXEPATH_H
