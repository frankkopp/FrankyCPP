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
#include <vector>

namespace fs = std::filesystem;

using namespace config;
using namespace common;

namespace tablebase {

  namespace {

    /// Check if a file has a Syzygy tablebase extension
    bool isTBFile(const fs::path& path, bool& isWDL, bool& isDTZ) {
      const auto ext = path.extension().string();
      isWDL          = ext == ".rtbw";
      isDTZ          = ext == ".rtbz";
      return isWDL || isDTZ;
    }

    /// Count pieces in a tablebase filename (e.g., "KQvK" = 3, "KRBvKN" = 5)
    int countPiecesInFilename(const std::string& filename) {
      // Strip extension and path
      std::string name = filename;
      const size_t dot = name.find('.');
      if (dot != std::string::npos) {
        name = name.substr(0, dot);
      }

      int count = 0;
      for (const char c : name) {
        if (c == 'K' || c == 'Q' || c == 'R' || c == 'B' || c == 'N' || c == 'P') {
          count++;
        }
      }
      return count;
    }

  }// anonymous namespace

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

  /// Counts of files by piece count
  struct PieceCountStats {
    int wdl3{0}, wdl4{0}, wdl5{0}, wdl6{0};
    int dtz3{0}, dtz4{0}, dtz5{0}, dtz6{0};

    [[nodiscard]] int totalWdl() const { return wdl3 + wdl4 + wdl5 + wdl6; }
    [[nodiscard]] int totalDtz() const { return dtz3 + dtz4 + dtz5 + dtz6; }
    [[nodiscard]] bool has3piece() const { return wdl3 > 0 || dtz3 > 0; }
    [[nodiscard]] bool has4piece() const { return wdl4 > 0 || dtz4 > 0; }
    [[nodiscard]] bool has5piece() const { return wdl5 > 0 || dtz5 > 0; }
    [[nodiscard]] bool has6piece() const { return wdl6 > 0 || dtz6 > 0; }
  };

  /// Count tablebase files by piece count
  PieceCountStats countTablebaseFilesByPiece(const std::string& path) {
    PieceCountStats stats;

    if (path.empty()) {
      return stats;
    }

    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) {
      return stats;
    }

    for (const auto& entry : fs::directory_iterator(path, ec)) {
      if (ec) break;
      if (!entry.is_regular_file()) continue;

      bool isWDL = false;
      bool isDTZ = false;
      if (!isTBFile(entry.path(), isWDL, isDTZ)) continue;

      const int pieces = countPiecesInFilename(entry.path().filename().string());

      if (isWDL) {
        switch (pieces) {
          case 3:
            ++stats.wdl3;
            break;
          case 4:
            ++stats.wdl4;
            break;
          case 5:
            ++stats.wdl5;
            break;
          case 6:
            ++stats.wdl6;
            break;
          default:
            break;
        }
      }
      else if (isDTZ) {
        switch (pieces) {
          case 3:
            ++stats.dtz3;
            break;
          case 4:
            ++stats.dtz4;
            break;
          case 5:
            ++stats.dtz5;
            break;
          case 6:
            ++stats.dtz6;
            break;
          default:
            break;
        }
      }
    }

    return stats;
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

    // Count files by piece count for accurate reporting
    const auto stats   = countTablebaseFilesByPiece(tbPath);
    const int wdlCount = stats.totalWdl();
    const int dtzCount = stats.totalDtz();

    if (wdlCount == 0 && dtzCount == 0) {
      return "No tablebase files found in: " + tbPath;
    }

    // Build description based on actual piece counts found
    std::vector<int> pieceCounts;
    if (stats.has3piece()) pieceCounts.push_back(3);
    if (stats.has4piece()) pieceCounts.push_back(4);
    if (stats.has5piece()) pieceCounts.push_back(5);
    if (stats.has6piece()) pieceCounts.push_back(6);

    std::string pieceEstimate;
    if (pieceCounts.empty()) {
      pieceEstimate = "partial";
    }
    else if (pieceCounts.size() == 1) {
      pieceEstimate = std::to_string(pieceCounts[0]) + "-piece";
    }
    else {
      pieceEstimate = std::to_string(pieceCounts.front()) + "-" + std::to_string(pieceCounts.back()) + " piece";
    }

    return pieceEstimate + " tablebases available (" + std::to_string(wdlCount) + " WDL, " + std::to_string(dtzCount) + " DTZ files) in: " + tbPath;
  }

}// namespace tablebase
