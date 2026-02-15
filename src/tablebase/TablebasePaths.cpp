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

#include "tablebase/TablebasePaths.h"
#include "common/Logging.h"
#include "common/misc.h"
#include "config/ConfigManager.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace tablebase {

namespace {

/// Check if a file has a Syzygy tablebase extension
bool isTBFile(const fs::path& path, bool& isWDL, bool& isDTZ) {
  const auto ext = path.extension().string();
  isWDL = ext == ".rtbw";
  isDTZ = ext == ".rtbz";
  return isWDL || isDTZ;
}

} // anonymous namespace

//=============================================================================
// Path Resolution
//=============================================================================

std::string findTablebasePath() {
  return findTablebasePath("");
}

std::string findTablebasePath(const std::string& explicitPath) {
  // Priority 1: Explicit path parameter
  if (!explicitPath.empty()) {
    if (validateTablebasePath(explicitPath)) {
      LOG__DEBUG(Logger::get().TB_LOG, "TB path from explicit parameter: {}", explicitPath);
      return explicitPath;
    }
    LOG__DEBUG(Logger::get().TB_LOG, "Explicit TB path invalid or empty: {}", explicitPath);
  }

  // Priority 2: Environment variable
  std::string envPath = getEnvironmentPath();
  if (!envPath.empty()) {
    if (validateTablebasePath(envPath)) {
      LOG__DEBUG(Logger::get().TB_LOG, "TB path from TB_PATH environment: {}", envPath);
      return envPath;
    }
    LOG__DEBUG(Logger::get().TB_LOG, "TB_PATH environment path invalid: {}", envPath);
  }

  // Priority 3: ConfigManager (search.yaml)
  std::string configPath = getConfiguredPath();
  if (!configPath.empty()) {
    if (validateTablebasePath(configPath)) {
      LOG__DEBUG(Logger::get().TB_LOG, "TB path from config: {}", configPath);
      return configPath;
    }
    LOG__DEBUG(Logger::get().TB_LOG, "Config TB path invalid: {}", configPath);
  }

  // Priority 4: Platform default
  std::string defaultPath = getDefaultTablebasePath();
  if (!defaultPath.empty() && validateTablebasePath(defaultPath)) {
    LOG__DEBUG(Logger::get().TB_LOG, "TB path from platform default: {}", defaultPath);
    return defaultPath;
  }

  LOG__DEBUG(Logger::get().TB_LOG, "No valid tablebase path found");
  return "";
}

std::string getDefaultTablebasePath() {
#ifdef _WIN32
  // Windows: "%LOCALAPPDATA%\FrankyCPP\syzygy\"
  const std::string localAppData = getEnv("LOCALAPPDATA");
  if (!localAppData.empty()) {
    return localAppData + "\\FrankyCPP\\syzygy";
  }
  return "";
#else
  // Linux/Unix: ~/.local/share/frankycpp/syzygy/
  const std::string home = getEnv("HOME");
  if (!home.empty()) {
    return home + "/.local/share/frankycpp/syzygy";
  }
  return "";
#endif
}

std::string getEnvironmentPath() {
  // Check SYZYGY_PATH first (standard name used by Stockfish and other engines)
  std::string path = getEnv("SYZYGY_PATH");
  if (!path.empty()) {
    return path;
  }
  // Fall back to TB_PATH for backwards compatibility
  return getEnv("TB_PATH");
}

std::string getConfiguredPath() {
  return ConfigManager::instance().search().TB_PATH;
}

//=============================================================================
// Validation
//=============================================================================

bool validateTablebasePath(const std::string& path) {
  if (path.empty()) {
    return false;
  }

  std::error_code ec;
  if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) {
    return false;
  }

  // Look for at least one .rtbw or .rtbz file
  for (const auto& entry : fs::directory_iterator(path, ec)) {
    if (ec) break;
    if (entry.is_regular_file()) {
      bool isWDL = false;
      bool isDTZ = false;
      if (isTBFile(entry.path(), isWDL, isDTZ)) {
        return true;
      }
    }
  }

  return false;
}

std::pair<int, int> countTablebaseFiles(const std::string& path) {
  int wdlCount = 0;
  int dtzCount = 0;

  if (path.empty()) {
    return {0, 0};
  }

  std::error_code ec;
  if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) {
    return {0, 0};
  }

  for (const auto& entry : fs::directory_iterator(path, ec)) {
    if (ec) break;
    if (entry.is_regular_file()) {
      bool isWDL = false;
      bool isDTZ = false;
      if (isTBFile(entry.path(), isWDL, isDTZ)) {
        if (isWDL) ++wdlCount;
        if (isDTZ) ++dtzCount;
      }
    }
  }

  return {wdlCount, dtzCount};
}

//=============================================================================
// Status Reporting
//=============================================================================

std::string getTablebaseStatus(const std::string& path) {
  const std::string tbPath = path.empty() ? findTablebasePath() : path;

  if (tbPath.empty()) {
    return "No tablebases found";
  }

  if (!validateTablebasePath(tbPath)) {
    return "Invalid tablebase path: " + tbPath;
  }

  const auto [wdlCount, dtzCount] = countTablebaseFiles(tbPath);

  if (wdlCount == 0 && dtzCount == 0) {
    return "No tablebase files found in: " + tbPath;
  }

  // Estimate piece count based on file count
  // 3-piece: 6 files, 4-piece: 35 files, 5-piece: 290 files, 6-piece: ~2000 files
  std::string pieceEstimate;
  const int totalFiles = wdlCount + dtzCount;
  if (totalFiles >= 3000) {
    pieceEstimate = "6+ piece";
  } else if (totalFiles >= 400) {
    pieceEstimate = "5-6 piece";
  } else if (totalFiles >= 50) {
    pieceEstimate = "4-5 piece";
  } else if (totalFiles >= 10) {
    pieceEstimate = "3-4 piece";
  } else {
    pieceEstimate = "partial";
  }

  return pieceEstimate + " tablebases available (" +
         std::to_string(wdlCount) + " WDL, " +
         std::to_string(dtzCount) + " DTZ files) in: " + tbPath;
}

} // namespace tablebase
