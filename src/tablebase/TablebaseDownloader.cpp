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
#include "common/ThreadPool.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <openssl/evp.h>

namespace fs = std::filesystem;

using namespace common;

namespace tablebase {

  namespace {

    // Estimated sizes per piece count (approximate, for progress estimation)
    constexpr size_t SIZE_3_PIECE = 7ULL * 1024 * 1024;          // ~7 MB
    constexpr size_t SIZE_4_PIECE = 75ULL * 1024 * 1024;         // ~75 MB
    constexpr size_t SIZE_5_PIECE = 1ULL * 1024 * 1024 * 1024;   // ~1 GB
    constexpr size_t SIZE_6_PIECE = 150ULL * 1024 * 1024 * 1024; // ~150 GB

    /// URL for the master file list from Lichess
    constexpr auto FILE_LIST_URL = "https://tablebase.lichess.ovh/tables/standard/download.txt";

    /// URL for the MD5 checksums file from Lichess
    constexpr auto MD5_CHECKSUMS_URL = "https://tablebase.lichess.ovh/tables/standard/md5";

    /// CURL write callback for string data
    size_t curlWriteStringCallback(const char* ptr, const size_t size, const size_t nmemb, void* userdata) {
      const size_t totalSize = size * nmemb;
      auto* buffer           = static_cast<std::string*>(userdata);
      buffer->append(ptr, totalSize);
      return totalSize;
    }

    /// CURL write callback for file data
    size_t curlWriteFileCallback(const char* ptr, const size_t size, const size_t nmemb, void* userdata) {
      auto* file             = static_cast<std::ofstream*>(userdata);
      const size_t totalSize = size * nmemb;
      file->write(ptr, static_cast<std::streamsize>(totalSize));
      return file->good() ? totalSize : 0;
    }

    /// Download a text file and return its contents using libcurl
    std::string downloadTextFile(const std::string& url) {
      CURL* curl = curl_easy_init();
      if (!curl) {
        return "";
      }

      std::string response;
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteStringCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L); // 60 second timeout
      curl_easy_setopt(curl, CURLOPT_USERAGENT, "FrankyCPP/1.0");

      const CURLcode res = curl_easy_perform(curl);
      curl_easy_cleanup(curl);

      return res == CURLE_OK ? response : "";
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
        const int pieces           = countPiecesInFilename(filename);

        // Check if this piece count is requested
        if (std::ranges::find(pieceCounts, pieces) != pieceCounts.end()) {
          urls.push_back(line);
        }
      }

      return urls;
    }

    /// Parse the MD5 checksums file from the server
    /// Format: "<md5hash>  <filename>" (hash, two spaces, filename)
    /// Returns map of filename -> MD5 hash (lowercase)
    std::unordered_map<std::string, std::string> parseMD5Checksums(const std::string& content) {
      std::unordered_map<std::string, std::string> checksums;

      std::istringstream stream(content);
      std::string line;
      while (std::getline(stream, line)) {
        // Trim trailing whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
          line.pop_back();
        }
        if (line.empty()) continue;

        // Format: "<md5>  <filename>" - MD5 is 32 hex chars, then two spaces, then filename
        if (line.size() < 35) continue; // 32 + 2 + at least 1 char for filename

        const std::string hash = line.substr(0, 32);
        // Skip the two spaces separator
        if (line[32] != ' ' || line[33] != ' ') continue;
        const std::string filename = line.substr(34);

        // Validate hash is all hex digits
        bool validHash = true;
        for (const char c : hash) {
          if (!std::isxdigit(static_cast<unsigned char>(c))) {
            validHash = false;
            break;
          }
        }
        if (!validHash) continue;

        // Store with lowercase hash
        std::string lowerHash = hash;
        for (char& c : lowerHash) {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        checksums[filename] = lowerHash;
      }

      return checksums;
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
      if (downloadFile(url, destPath, config.verbose)) {
        result.filesDownloaded++;
        progressInfo.success = true;
      }
      else {
        result.filesFailed++;
        progressInfo.success      = false;
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

  VerifyResult TablebaseDownloader::verify(const std::string& path,
                                           const std::vector<int>& pieceCounts,
                                           const ProgressCallback& progress) {
    VerifyResult result;

    if (path.empty()) {
      result.errors.emplace_back("No path specified");
      return result;
    }

    std::error_code ec;
    if (!fs::exists(path, ec)) {
      result.errors.emplace_back("Directory does not exist: " + path);
      return result;
    }

    // Fetch MD5 checksums from server
    LOG__INFO(Logger::get().TB_LOG, "Fetching MD5 checksums from server...");
    const auto checksums = fetchMD5Checksums();
    if (checksums.empty()) {
      result.errors.emplace_back("Failed to fetch MD5 checksums from server");
      return result;
    }
    LOG__INFO(Logger::get().TB_LOG, "Loaded {} MD5 checksums from server", checksums.size());

    // Collect files to verify
    std::vector<std::string> filesToVerify;
    for (const auto& entry : fs::directory_iterator(path, ec)) {
      if (!entry.is_regular_file()) continue;

      const auto ext = entry.path().extension().string();
      if (ext != ".rtbw" && ext != ".rtbz") continue;

      const std::string filename = entry.path().filename().string();

      // Filter by piece count if specified
      if (!pieceCounts.empty()) {
        const int pieces = countPiecesInFilename(filename);
        if (std::ranges::find(pieceCounts, pieces) == pieceCounts.end()) {
          continue;
        }
      }

      filesToVerify.push_back(filename);
    }

    if (filesToVerify.empty()) {
      result.errors.emplace_back("No tablebase files found to verify");
      return result;
    }

    // Sort for consistent output
    std::ranges::sort(filesToVerify);

    const int totalFiles = static_cast<int>(filesToVerify.size());

    // Structure to hold per-file verification result
    struct FileVerifyResult {
      std::string filename;
      std::string expectedMD5;
      std::string computedMD5;
      bool hasChecksum{false};
      bool success{false};
      std::string errorMessage;
    };

    // Determine thread count - use hardware concurrency, but cap at reasonable number
    const unsigned int numThreads = std::max(1U, std::min(std::thread::hardware_concurrency(), 16U));
    ThreadPool pool(numThreads);

    LOG__INFO(Logger::get().TB_LOG, "Verifying {} tablebase files in {} ({} parallel)", totalFiles, path, numThreads);

    // Submit all verification tasks
    std::vector<std::future<FileVerifyResult>> futures;
    futures.reserve(filesToVerify.size());

    for (const auto& filename : filesToVerify) {
      const std::string filePath = std::format("{}/{}", path, filename);

      // Look up expected checksum
      const auto checksumIt         = checksums.find(filename);
      const bool hasChecksum        = checksumIt != checksums.end();
      const std::string expectedMD5 = hasChecksum ? checksumIt->second : "";

      // Submit task to thread pool
      futures.push_back(pool.enqueue([filePath, filename, expectedMD5, hasChecksum]() -> FileVerifyResult {
        FileVerifyResult fileResult;
        fileResult.filename    = filename;
        fileResult.expectedMD5 = expectedMD5;
        fileResult.hasChecksum = hasChecksum;

        if (!hasChecksum) {
          fileResult.success      = true; // No checksum = skip (not a failure)
          fileResult.errorMessage = "No reference checksum";
          return fileResult;
        }

        // Compute MD5
        fileResult.computedMD5 = computeMD5(filePath);
        if (fileResult.computedMD5.empty()) {
          fileResult.success      = false;
          fileResult.errorMessage = "Failed to compute MD5";
          return fileResult;
        }

        // Compare
        if (fileResult.computedMD5 == expectedMD5) {
          fileResult.success = true;
        }
        else {
          fileResult.success      = false;
          fileResult.errorMessage = std::format("MD5 mismatch (expected: {}, got: {})",
                                                expectedMD5, fileResult.computedMD5);
        }

        return fileResult;
      }));
    }

    // Collect results and report progress
    DownloadProgress progressInfo;
    progressInfo.totalFiles = totalFiles;

    for (auto& future : futures) {
      const FileVerifyResult fileResult = future.get();

      progressInfo.currentFile = fileResult.filename;
      progressInfo.filesCompleted++;

      if (!fileResult.hasChecksum) {
        result.filesNoChecksum++;
        progressInfo.success      = true;
        progressInfo.errorMessage = "No reference checksum: " + fileResult.filename;
      }
      else if (fileResult.success) {
        result.filesVerified++;
        progressInfo.success = true;
        progressInfo.errorMessage.clear();
      }
      else {
        result.filesFailed++;
        progressInfo.success      = false;
        progressInfo.errorMessage = fileResult.filename + ": " + fileResult.errorMessage;
        result.errors.push_back(progressInfo.errorMessage);
      }

      if (progress) {
        progress(progressInfo);
      }
    }

    result.success = (result.filesFailed == 0 && result.filesMissing == 0);
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
      const size_t dot     = filename.find('.');
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
        case 3:
          total += SIZE_3_PIECE;
          break;
        case 4:
          total += SIZE_4_PIECE;
          break;
        case 5:
          total += SIZE_5_PIECE;
          break;
        case 6:
          total += SIZE_6_PIECE;
          break;
        default:
          break;
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
        else if (ext == ".rtbz")
          ++dtzCount;
      }
    }

    std::string status;
    status += "Path: " + path + "\n";
    status += std::format("WDL files: {}\n", wdlCount);
    status += std::format("DTZ files: {}\n", dtzCount);

    if (wdlCount > 0 && dtzCount > 0) {
      status += "Status: Tablebases available";
    }
    else if (wdlCount > 0 || dtzCount > 0) {
      status += "Status: Partial (missing " + std::string(wdlCount == 0 ? "WDL" : "DTZ") + " files)";
    }
    else {
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
      CURL* curl = curl_easy_init();
      if (!curl) {
        LOG__ERROR(Logger::get().TB_LOG, "Failed to initialize curl");
        return false;
      }

      std::ofstream outFile(destPath, std::ios::binary);
      if (!outFile.is_open()) {
        LOG__ERROR(Logger::get().TB_LOG, "Failed to open output file: {}", destPath);
        curl_easy_cleanup(curl);
        return false;
      }

      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteFileCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);
      curl_easy_setopt(curl, CURLOPT_USERAGENT, "FrankyCPP/1.0");
      // No timeout for large files - they can take a while
      curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L); // 1KB/s minimum
      curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);    // for 60 seconds

      const CURLcode res = curl_easy_perform(curl);

      long httpCode = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
      curl_easy_cleanup(curl);
      outFile.close();

      if (res != CURLE_OK) {
        LOG__DEBUG(Logger::get().TB_LOG, "Download failed: {} (curl error: {})",
                   url, curl_easy_strerror(res));
        std::error_code ec;
        fs::remove(destPath, ec);
        return false;
      }

      if (httpCode != 200) {
        LOG__DEBUG(Logger::get().TB_LOG, "Download failed: {} (HTTP {})", url, httpCode);
        std::error_code ec;
        fs::remove(destPath, ec);
        return false;
      }

      std::error_code ec;
      bool exists = fs::exists(destPath, ec);
      if (!exists || ec) {
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

  std::string TablebaseDownloader::computeMD5(const std::string& filePath) {
    try {
      std::ifstream file(filePath, std::ios::binary);
      if (!file.is_open()) {
        LOG__ERROR(Logger::get().TB_LOG, "Failed to open file for MD5: {}", filePath);
        return "";
      }

      // Use EVP API (OpenSSL 3.0+)
      EVP_MD_CTX* ctx = EVP_MD_CTX_new();
      if (!ctx) {
        LOG__ERROR(Logger::get().TB_LOG, "Failed to create EVP_MD_CTX");
        return "";
      }

      if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        LOG__ERROR(Logger::get().TB_LOG, "Failed to initialize MD5 digest");
        return "";
      }

      // Read file in chunks for efficiency
      constexpr size_t BUFFER_SIZE = 64 * 1024; // 64KB buffer
      std::vector<char> buffer(BUFFER_SIZE);

      while (file.read(buffer.data(), static_cast<std::streamsize>(BUFFER_SIZE)) || file.gcount() > 0) {
        if (EVP_DigestUpdate(ctx, buffer.data(), static_cast<size_t>(file.gcount())) != 1) {
          EVP_MD_CTX_free(ctx);
          LOG__ERROR(Logger::get().TB_LOG, "Failed to update MD5 digest");
          return "";
        }
      }

      file.close();

      // Finalize and get the hash
      std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
      unsigned int digestLen = 0;
      if (EVP_DigestFinal_ex(ctx, digest.data(), &digestLen) != 1) {
        EVP_MD_CTX_free(ctx);
        LOG__ERROR(Logger::get().TB_LOG, "Failed to finalize MD5 digest");
        return "";
      }

      EVP_MD_CTX_free(ctx);

      // Convert to hex string (lowercase)
      std::string md5;
      md5.reserve(digestLen * 2);
      for (unsigned int i = 0; i < digestLen; ++i) {
        md5 += std::format("{:02x}", digest[i]);
      }

      return md5;

    } catch (const std::exception& e) {
      LOG__ERROR(Logger::get().TB_LOG, "MD5 computation exception: {}", e.what());
      return "";
    }
  }

  std::unordered_map<std::string, std::string> TablebaseDownloader::fetchMD5Checksums() {
    const std::string content = downloadTextFile(MD5_CHECKSUMS_URL);
    if (content.empty()) {
      LOG__ERROR(Logger::get().TB_LOG, "Failed to download MD5 checksums from {}", MD5_CHECKSUMS_URL);
      return {};
    }

    return parseMD5Checksums(content);
  }

  std::string TablebaseDownloader::getManualDownloadInstructions() {
    return R"(
================================================================================
MANUAL SYZYGY TABLEBASE DOWNLOAD INSTRUCTIONS
================================================================================

The automatic download failed. You can download tablebases manually from these sources:

DOWNLOAD SOURCES:
  1. Lichess (recommended):
     https://tablebase.lichess.ovh/tables/standard/
     - File list: https://tablebase.lichess.ovh/tables/standard/download.txt

  2. Sesse (original mirror):
     https://tablebase.sesse.net/syzygy/

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
  3. Verify with: FrankyCPP --syzygy verify --path D:\SYZYGY
  4. Check status: FrankyCPP --syzygy status --path D:\SYZYGY

FILE SIZES (approximate):
  3-piece:  ~7 MB    (5 positions)
  4-piece:  ~75 MB   (35 positions)
  5-piece:  ~1 GB    (145 positions)
  6-piece:  ~150 GB  (510 positions) - use torrent!

================================================================================
)";
  }

} // namespace tablebase
