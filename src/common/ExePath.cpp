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

#include "ExePath.h"

#include <filesystem>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <climits>
#include <unistd.h>
#endif

namespace common {

  const std::filesystem::path& getExecutableDir() {
    static const std::filesystem::path dir = []() -> std::filesystem::path {
#ifdef _WIN32
      std::wstring buf(MAX_PATH, L'\0');
      DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
      // Handle paths longer than MAX_PATH
      while (len >= buf.size()) {
        buf.resize(buf.size() * 2);
        len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
      }
      if (len > 0) {
        buf.resize(len);
        return std::filesystem::path(buf).parent_path();
      }
#else
      char buf[PATH_MAX];
      const ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
      if (len > 0) {
        buf[len] = '\0';
        return std::filesystem::path(buf).parent_path();
      }
#endif
      // Fallback: current working directory (legacy behaviour)
      return std::filesystem::current_path();
    }();
    return dir;
  }

  std::filesystem::path resolvePathRelativeToExe(const std::filesystem::path& relPath) {
    if (relPath.is_absolute()) {
      return relPath;
    }
    return getExecutableDir() / relPath;
  }

} // namespace common
