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

#ifndef FRANKYCPP_TABLEBASEDOWNLOADER_H
#define FRANKYCPP_TABLEBASEDOWNLOADER_H

//=============================================================================
// TablebaseDownloader.h - Syzygy Tablebase Download Management
//=============================================================================
//
// Provides functionality to download Syzygy tablebase files from Lichess mirror.
// Uses libcurl for HTTP/HTTPS downloads (cross-platform).
//
// Features:
//   - Download by piece count (3, 4, 5, 6)
//   - Progress reporting via callback
//   - Fetches file list from server to ensure correct URLs
//   - File verification after download using MD5 checksums
//
// Usage:
//   TablebaseDownloader downloader;
//   DownloadConfig config{.pieceCounts = {3, 4, 5}, .targetPath = "D:/SYZYGY"};
//   downloader.download(config, [](const DownloadProgress& p) {
//     std::cout << p.currentFile << " " << p.percentComplete() << "%" << std::endl;
//   });
//
//=============================================================================

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tablebase {

  /// Configuration for tablebase download operation
  struct DownloadConfig {
    std::vector<int> pieceCounts;   ///< Piece counts to download (e.g., {3, 4, 5})
    std::string targetPath;         ///< Directory to save files
    bool verbose{false};            ///< Print detailed progress
    bool verifyAfterDownload{true}; ///< Verify files exist after download
  };

  /// Progress information during download
  struct DownloadProgress {
    std::string currentFile;  ///< Current file being downloaded
    int filesCompleted{0};    ///< Number of files completed
    int totalFiles{0};        ///< Total number of files to download
    bool success{true};       ///< Whether current operation succeeded
    std::string errorMessage; ///< Error message if success is false

    /// Returns completion percentage (0-100)
    [[nodiscard]] int percentComplete() const {
      return totalFiles > 0 ? filesCompleted * 100 / totalFiles : 0;
    }
  };

  /// Callback function for progress updates
  using ProgressCallback = std::function<void(const DownloadProgress&)>;

  /// Result of a download operation
  struct DownloadResult {
    bool success{false};             ///< Overall success
    int filesDownloaded{0};          ///< Number of files successfully downloaded
    int filesFailed{0};              ///< Number of files that failed
    int filesSkipped{0};             ///< Number of files already present (skipped)
    std::vector<std::string> errors; ///< Error messages for failed files
  };

  /// Result of a verification operation
  struct VerifyResult {
    bool success{false};             ///< Overall success (all files valid)
    int filesVerified{0};            ///< Number of files with matching MD5
    int filesFailed{0};              ///< Number of files with MD5 mismatch
    int filesMissing{0};             ///< Number of files not found
    int filesNoChecksum{0};          ///< Number of files with no reference MD5
    std::vector<std::string> errors; ///< Error messages for failed/missing files
  };

  /// Syzygy tablebase download manager
  class TablebaseDownloader {
  public:
    /// Download tablebases for specified piece counts
    /// @param config   Download configuration
    /// @param progress Optional progress callback
    /// @return Result with success/failure counts
    [[nodiscard]] static DownloadResult download(const DownloadConfig& config,
                                                 const ProgressCallback& progress = nullptr);

    /// Verify tablebases using MD5 checksums from server
    /// @param path         Directory containing tablebase files
    /// @param pieceCounts  Piece counts to verify (empty = verify all found files)
    /// @param progress     Optional progress callback
    /// @return Result with verification counts
    [[nodiscard]] static VerifyResult verify(const std::string& path,
                                             const std::vector<int>& pieceCounts = {},
                                             const ProgressCallback& progress    = nullptr);

    /// Get list of files needed for given piece counts
    /// @param pieceCounts  Vector of piece counts (3-6)
    /// @return List of filenames (without path)
    [[nodiscard]] static std::vector<std::string> getRequiredFiles(const std::vector<int>& pieceCounts);

    /// Get estimated download size in bytes for given piece counts
    /// @param pieceCounts  Vector of piece counts (3-6)
    /// @return Estimated total size in bytes
    [[nodiscard]] static size_t estimateDownloadSize(const std::vector<int>& pieceCounts);

    /// Get human-readable size string
    /// @param bytes  Size in bytes
    /// @return String like "1.5 GB" or "150 MB"
    [[nodiscard]] static std::string formatSize(size_t bytes);

    /// Check status of tablebases in a directory
    /// @param path  Directory to check
    /// @param pieceCounts  Piece counts to check for
    /// @return Status message
    [[nodiscard]] static std::string checkStatus(const std::string& path,
                                                 const std::vector<int>& pieceCounts);

    /// Get manual download instructions (shown when automatic download fails)
    [[nodiscard]] static std::string getManualDownloadInstructions();

  private:
    /// Download a single file from URL
    /// @param url       Full URL to download
    /// @param destPath  Full path to save file
    /// @param verbose   Print progress
    /// @return true if download succeeded
    [[nodiscard]] static bool downloadFile(const std::string& url,
                                           const std::string& destPath,
                                           bool verbose);

    /// Compute MD5 checksum of a file
    /// @param filePath  Path to file
    /// @return MD5 hash as lowercase hex string, or empty string on error
    [[nodiscard]] static std::string computeMD5(const std::string& filePath);

    /// Fetch and parse MD5 checksums from server
    /// @return Map of filename -> MD5 hash
    [[nodiscard]] static std::unordered_map<std::string, std::string> fetchMD5Checksums();
  };

} // namespace tablebase

#endif // FRANKYCPP_TABLEBASEDOWNLOADER_H
