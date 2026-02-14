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

#ifndef FRANKYCPP_TABLEBASEPATHS_H
#define FRANKYCPP_TABLEBASEPATHS_H

//=============================================================================
// TablebasePaths.h - Tablebase Path Resolution Utilities
//=============================================================================
//
// Provides utilities for locating Syzygy tablebase files on disk.
// Supports multiple path sources with priority-based resolution.
//
// Path Resolution Order (highest to lowest priority):
//   1. Explicit path parameter (if provided)
//   2. Environment variable TB_PATH
//   3. ConfigManager TB_PATH setting (from search.yaml)
//   4. Platform-specific default directories
//
// Validation:
//   - Checks for presence of .rtbw and/or .rtbz files
//   - Validates directory accessibility
//
// Platform Defaults:
//   - Windows: %LOCALAPPDATA%\FrankyCPP\syzygy\
//   - Linux:   ~/.local/share/frankycpp/syzygy/
//
// Usage:
//   std::string path = tablebase::findTablebasePath();
//   if (!path.empty()) {
//     tb.initialize(path);
//   }
//
//=============================================================================

#include <string>
#include <vector>

namespace tablebase {

/// Find the first valid tablebase path from all sources.
/// Checks in order: environment variable, config, defaults.
/// @return Valid path containing TB files, or empty string if none found
[[nodiscard]] std::string findTablebasePath();

/// Find tablebase path, with explicit override taking the highest priority.
/// @param explicitPath  If non-empty, checked first before other sources
/// @return Valid path containing TB files, or empty string if none found
[[nodiscard]] std::string findTablebasePath(const std::string& explicitPath);

/// Get platform-specific default tablebase directory.
/// @return Default path (may not exist)
[[nodiscard]] std::string getDefaultTablebasePath();

/// Check if a path contains valid Syzygy tablebase files.
/// Looks for .rtbw (WDL) or .rtbz (DTZ) files.
/// @param path  Directory path to check
/// @return true if at least one valid TB file was found
[[nodiscard]] bool validateTablebasePath(const std::string& path);

/// Get the TB_PATH environment variable value.
/// @return Environment variable value, or empty string if not set
[[nodiscard]] std::string getEnvironmentPath();

/// Get the configured TB_PATH from ConfigManager.
/// @return Configured path, or empty string if not set
[[nodiscard]] std::string getConfiguredPath();

/// Count tablebase files in a directory.
/// @param path  Directory path to scan
/// @return Pair of (wdl_count, dtz_count)
[[nodiscard]] std::pair<int, int> countTablebaseFiles(const std::string& path);

/// Get human-readable status string for tablebase availability.
/// @param path  Path to check (or empty to use findTablebasePath)
/// @return Status string like "6-piece tablebases available (150 WDL, 150 DTZ files)"
[[nodiscard]] std::string getTablebaseStatus(const std::string& path = "");

} // namespace tablebase

#endif // FRANKYCPP_TABLEBASEPATHS_H
