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

#ifndef FRANKYCPP_TEST_ENGINE_PATH_H
#define FRANKYCPP_TEST_ENGINE_PATH_H

#include "version.h"

#include <filesystem>
#include <string>
#include <vector>

/**
 * @brief Get the path to the FrankyCPP test engine executable.
 *
 * Searches for the engine executable in common build output locations.
 * The executable name is derived from FrankyCPP_EXE_NAME (defined in version.h),
 * ensuring the version is always in sync with CMake configuration.
 *
 * @return Path to the engine executable, or empty string if not found.
 */
inline std::string getTestEnginePath() {
  // Build the exe name from version macro (defined in version.h via CMake)
  std::string exeName = FrankyCPP_EXE_NAME;

#ifdef _WIN32
  const std::string ext = ".exe";
#else
  const std::string ext = "";
#endif

  // Search paths in priority order
  std::vector<std::string> searchPaths = {
    "cmake-build-win-release/src/" + exeName + ext,
    "cmake-build-wsl-release/src/" + exeName + ext,
    "../src/" + exeName + ext,
    exeName + ext
  };

  for (const auto& path : searchPaths) {
    if (std::filesystem::exists(path)) {
      return path;
    }
  }
  return "";
}

#endif // FRANKYCPP_TEST_ENGINE_PATH_H
