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

#include "tablebase/TablebaseDownloader.h"
#include "common/Logging.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace tablebase {

namespace {

// Estimated sizes per piece count (approximate, for progress estimation)
constexpr size_t SIZE_3_PIECE = 7ULL * 1024 * 1024;         // ~7 MB
constexpr size_t SIZE_4_PIECE = 75ULL * 1024 * 1024;        // ~75 MB
constexpr size_t SIZE_5_PIECE = 1ULL * 1024 * 1024 * 1024;  // ~1 GB
constexpr size_t SIZE_6_PIECE = 150ULL * 1024 * 1024 * 1024;// ~150 GB

/// URL for the master file list from Lichess
constexpr auto FILE_LIST_URL = "https://tablebase.lichess.ovh/tables/standard/download.txt";

/// Download a text file and return its contents
std::string downloadTextFile(const std::string& url) {
  const std::string tempFile = fs::temp_directory_path().string() + "/syzygy_filelist.txt";

#ifdef _WIN32
  const std::string command = std::format(
    "powershell -NoProfile -Command \"Invoke-WebRequest -Uri '{}' -OutFile '{}' -UseBasicParsing\" 2>nul",
    url, tempFile);
#else
  const std::string command = std::format(
    "curl -sL -o '{}' '{}'",
    tempFile, url);
#endif

  const int result = std::system(command.c_str());
  if (result != 0) {
    return "";
  }

  std::ifstream file(tempFile);
  if (!file.is_open()) {
    return "";
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();

  std::error_code ec;
  fs::remove(tempFile, ec);

  return buffer.str();
}

/// Count pieces in a tablebase filename (e.g., "KQvK" = 3, "KRBvKN" = 5)
int countPiecesInFilename(const std::string& filename) {
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

/// Extract filename from URL
std::string getFilenameFromUrl(const std::string& url) {
  const size_t lastSlash = url.find_last_of('/');
  if (lastSlash != std::string::npos && lastSlash + 1 < url.size()) {
    return url.substr(lastSlash + 1);
  }
  return url;
}

/// Parse the download.txt file to get URLs for specific piece counts
/// Each line is a full URL like https://tablebase.lichess.ovh/tables/standard/3-4-5-wdl/KBvK.rtbw
std::vector<std::string> parseFileList(const std::string& content, const std::vector<int>& pieceCounts) {
  std::vector<std::string> urls;

  std::istringstream stream(content);
  std::string line;
  while (std::getline(stream, line)) {
    // Trim whitespace
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
      line.pop_back();
    }
    if (line.empty()) continue;

    // Extract filename and count pieces
    const std::string filename = getFilenameFromUrl(line);
    const int pieces = countPiecesInFilename(filename);

    // Check if this piece count is requested
    if (std::ranges::find(pieceCounts, pieces) != pieceCounts.end()) {
      urls.push_back(line);
    }
  }

  return urls;
}

} // anonymous namespace

//=============================================================================
// Public Methods
//=============================================================================

DownloadResult TablebaseDownloader::download(const DownloadConfig& config,
                                             const ProgressCallback& progress) {
  DownloadResult result;

  if (config.pieceCounts.empty()) {
    result.errors.emplace_back("No piece counts specified");
    return result;
  }

  if (config.targetPath.empty()) {
    result.errors.emplace_back("No target path specified");
    return result;
  }

  std::error_code ec;
  if (!fs::exists(config.targetPath, ec)) {
    if (!fs::create_directories(config.targetPath, ec)) {
      result.errors.emplace_back("Failed to create directory: " + config.targetPath);
      return result;
    }
  }

  // Fetch the master file list from server
  LOG__INFO(Logger::get().TB_LOG, "Fetching file list from server...");
  const std::string fileListContent = downloadTextFile(FILE_LIST_URL);
  if (fileListContent.empty()) {
    result.errors.emplace_back("Failed to download file list from " + std::string(FILE_LIST_URL));
    return result;
  }

  // Parse and filter URLs for requested piece counts
  const std::vector<std::string> urls = parseFileList(fileListContent, config.pieceCounts);

  if (urls.empty()) {
    result.errors.emplace_back("No files found for specified piece counts");
    return result;
  }

  DownloadProgress progressInfo;
  progressInfo.totalFiles = static_cast<int>(urls.size());

  LOG__INFO(Logger::get().TB_LOG, "Starting download of {} tablebase files to {}",
            progressInfo.totalFiles, config.targetPath);

  // Download each file
  for (const auto& url : urls) {
    const std::string filename = getFilenameFromUrl(url);
    const std::string destPath = config.targetPath + "/" + filename;

    progressInfo.currentFile = filename;

    // Skip if file already exists
    if (fs::exists(destPath, ec)) {
      result.filesSkipped++;
      progressInfo.filesCompleted++;
      if (progress) {
        progressInfo.success = true;
        progress(progressInfo);
      }
      continue;
    }

    // Download from the URL (already has full path from download.txt)
    const bool success = downloadFile(url, destPath, config.verbose);

    if (success) {
      result.filesDownloaded++;
      progressInfo.success = true;
    } else {
      result.filesFailed++;
      progressInfo.success = false;
      progressInfo.errorMessage = "Failed to download: " + filename;
      result.errors.push_back(progressInfo.errorMessage);
    }

    progressInfo.filesCompleted++;
    if (progress) {
      progress(progressInfo);
    }
  }

  result.success = (result.filesFailed == 0);
  return result;
}

std::vector<std::string> TablebaseDownloader::getRequiredFiles(const std::vector<int>& pieceCounts) {
  const std::string fileListContent = downloadTextFile(FILE_LIST_URL);
  if (fileListContent.empty()) {
    return {};
  }

  const auto urls = parseFileList(fileListContent, pieceCounts);

  std::vector<std::string> files;
  for (const auto& url : urls) {
    std::string filename = getFilenameFromUrl(url);
    const size_t dot = filename.find('.');
    if (dot != std::string::npos) {
      filename = filename.substr(0, dot);
    }
    if (std::ranges::find(files, filename) == files.end()) {
      files.push_back(filename);
    }
  }

  return files;
}

size_t TablebaseDownloader::estimateDownloadSize(const std::vector<int>& pieceCounts) {
  size_t total = 0;

  for (const int pieces : pieceCounts) {
    switch (pieces) {
      case 3: total += SIZE_3_PIECE; break;
      case 4: total += SIZE_4_PIECE; break;
      case 5: total += SIZE_5_PIECE; break;
      case 6: total += SIZE_6_PIECE; break;
      default: break;
    }
  }

  return total;
}

std::string TablebaseDownloader::formatSize(const size_t bytes) {
  constexpr size_t KBYTE = 1024;
  constexpr size_t MBYTE = KBYTE * 1024;
  constexpr size_t GBYTE = MBYTE * 1024;

  if (bytes >= GBYTE) {
    return std::format("{:.1f} GB", static_cast<double>(bytes) / static_cast<double>(GBYTE));
  }
  if (bytes >= MBYTE) {
    return std::format("{:.1f} MB", static_cast<double>(bytes) / static_cast<double>(MBYTE));
  }
  if (bytes >= KBYTE) {
    return std::format("{:.1f} KB", static_cast<double>(bytes) / static_cast<double>(KBYTE));
  }
  return std::format("{} bytes", bytes);
}

std::string TablebaseDownloader::checkStatus(const std::string& path,
                                             [[maybe_unused]] const std::vector<int>& pieceCounts) {
  if (path.empty()) {
    return "No tablebase path specified";
  }

  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return "Directory does not exist: " + path;
  }

  int wdlCount = 0;
  int dtzCount = 0;

  for (const auto& entry : fs::directory_iterator(path, ec)) {
    if (entry.is_regular_file()) {
      const auto ext = entry.path().extension().string();
      if (ext == ".rtbw") ++wdlCount;
      else if (ext == ".rtbz") ++dtzCount;
    }
  }

  std::string status;
  status += "Path: " + path + "\n";
  status += std::format("WDL files: {}\n", wdlCount);
  status += std::format("DTZ files: {}\n", dtzCount);

  if (wdlCount > 0 && dtzCount > 0) {
    status += "Status: Tablebases available";
  } else if (wdlCount > 0 || dtzCount > 0) {
    status += "Status: Partial (missing " + std::string(wdlCount == 0 ? "WDL" : "DTZ") + " files)";
  } else {
    status += "Status: No tablebase files found";
  }

  return status;
}

//=============================================================================
// Private Methods
//=============================================================================

bool TablebaseDownloader::downloadFile(const std::string& url,
                                       const std::string& destPath,
                                       const bool verbose) {
  if (verbose) {
    LOG__INFO(Logger::get().TB_LOG, "Downloading: {} -> {}", url, destPath);
  }

  try {
#ifdef _WIN32
    const std::string command = std::format(
      "powershell -NoProfile -Command \"Invoke-WebRequest -Uri '{}' -OutFile '{}' -UseBasicParsing\" 2>nul",
      url, destPath);
#else
    const std::string command = std::format(
      "curl -sL -o '{}' '{}'",
      destPath, url);
#endif

    const int result = std::system(command.c_str());

    if (result != 0) {
      LOG__DEBUG(Logger::get().TB_LOG, "Download command failed with code: {}", result);
      std::error_code ec;
      fs::remove(destPath, ec);
      return false;
    }

    std::error_code ec;
    if (!fs::exists(destPath, ec) || ec) {
      fs::remove(destPath, ec);
      return false;
    }

    const auto fileSize = fs::file_size(destPath, ec);
    if (ec || fileSize == 0) {
      fs::remove(destPath, ec);
      return false;
    }

    return true;

  } catch (const std::exception& e) {
    LOG__ERROR(Logger::get().TB_LOG, "Download exception: {}", e.what());
    return false;
  }
}

std::string TablebaseDownloader::getManualDownloadInstructions() {
  return R"(
================================================================================
MANUAL SYZYGY TABLEBASE DOWNLOAD INSTRUCTIONS
================================================================================

The spdlog::color_mode::automatic download failed. You can download tablebases manually from these sources:

DOWNLOAD SOURCES:
  1. Lichess (recommended):
     https://tablebase.lichess.ovh/tables/standard/
     - File list: https://tablebase.lichess.ovh/tables/standard/download.txt

  2. Sesse (original mirror):
     http://tablebase.sesse.net/syzygy/

  3. Torrent (best for 6-piece, ~150GB):
     Search for "Syzygy tablebases torrent" - widely available

DIRECTORY STRUCTURE:
  - 3-4-5 piece files: ~1.1 GB total
  - 6 piece files: ~150 GB total (WDL: ~68 GB, DTZ: ~82 GB)

HOW TO DOWNLOAD:

  Option A - Browser download:
    1. Go to https://tablebase.lichess.ovh/tables/standard/
    2. Download files from 3-4-5-wdl/ and 3-4-5-dtz/ folders
    3. Save ALL files to a SINGLE folder (e.g., D:\SYZYGY)

  Option B - Command line (Windows PowerShell):
    # Download file list
    Invoke-WebRequest -Uri "https://tablebase.lichess.ovh/tables/standard/download.txt" -OutFile "download.txt"

    # Download each file (example for one file)
    Invoke-WebRequest -Uri "https://tablebase.lichess.ovh/tables/standard/3-4-5-wdl/KQvK.rtbw" -OutFile "D:\SYZYGY\KQvK.rtbw"

  Option C - Command line (Linux/Mac):
    # Download all 3-4-5 piece files
    wget -r -np -nH --cut-dirs=3 -P ./syzygy https://tablebase.lichess.ovh/tables/standard/3-4-5-wdl/
    wget -r -np -nH --cut-dirs=3 -P ./syzygy https://tablebase.lichess.ovh/tables/standard/3-4-5-dtz/

AFTER DOWNLOAD:
  1. Ensure all .rtbw (WDL) and .rtbz (DTZ) files are in ONE folder
  2. Configure FrankyCPP:
     - Set TB_PATH in config/search.yaml: TB_PATH: "D:/SYZYGY"
     - Or set environment variable: TB_PATH=D:\SYZYGY
  3. Verify with: FrankyCPP --syzygy status --path D:\SYZYGY

FILE SIZES (approximate):
  3-piece:  ~7 MB    (5 positions)
  4-piece:  ~75 MB   (35 positions)
  5-piece:  ~1 GB    (145 positions)
  6-piece:  ~150 GB  (510 positions) - use torrent!

================================================================================
)";
}

} // namespace tablebase
