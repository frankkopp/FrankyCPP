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

#include "init.h"
#include "version.h"
#include <algorithm>
#include <chesscore/Perft.h>
#include <config/ConfigGenerators.h>
#include <config/ConfigManager.h>
#include <engine/Benchmark.h>
#include <engine/UciHandler.h>
#include <engine/UciOptions.h>
#include <enginetest/TestSuite.h>
#include <fstream>
#include <iostream>
#include <tablebase/TablebaseDownloader.h>
#include <tablebase/TablebasePaths.h>

// BOOST program options
#include "boost/program_options.hpp"
#include "common/CrashHandler.h"
#include "common/Logging.h"
namespace po = boost::program_options;

using namespace engine;
using namespace chess;
using namespace config;
using namespace common;
using namespace enginetest;

// global variable for program options
inline po::variables_map programOptions; // NOLINT(*-err58-cpp)

int main(int argc, char* argv[]) {

  // Version comes from CMAKE template version.h.in
  std::string appName = "FrankyCPP";
#ifdef FRANKYCPP_PRODUCTION
  appName.append(" (stripped)");
#endif
  appName
    .append(" v")
    .append(std::to_string(FrankyCPP_VERSION_MAJOR))
    .append(".")
    .append(std::to_string(FrankyCPP_VERSION_MINOR));
  std::cout << appName << std::endl;

  std::string config_file, book_file, book_type, testsuite_file;
  std::string show_config_format, show_config_domain;
  std::string syzygy_command, syzygy_path, syzygy_pieces;
  std::string crash_dump_path;
  int testsuite_time  = 0;
  int testsuite_depth = 0;
  int perftStart      = 0;
  int perftEnd        = 0;
  bool perftOnDemand  = false;

  // Command line options
  try {
    // clang-format off

    // =========================================================================
    // Declare a group of options that will be allowed only on command line
    // =========================================================================
    po::options_description generic("Generic options");
    generic.add_options()
      ("help,?", "produce help message")
      ("config,c", po::value<std::string>(&config_file)->default_value("./config/FrankyCPP.cfg"), "configuration file name")
      ("ucioptions,u", "print UCI options as if 'uci' command was sent")
      ("version,v", "print version string")
      // Crash handler options
      ("crash-dumps", po::value<std::string>(&crash_dump_path)->default_value("./crash_dumps"),
        "directory for crash dumps/minidumps (default: ./crash_dumps)")
      // Show config options
      ("show-config", "show all available configuration settings")
      ("format", po::value<std::string>(&show_config_format)->default_value("table"), "output format for --show-config: table, yaml, json")
      ("domain", po::value<std::string>(&show_config_domain)->default_value("all"), "filter by domain: general, search, eval, tuning, debug, all")
      // Perft options
      ("perft", "run perft test")
      ("startDepth", po::value<int>(&perftStart)->default_value(1), "start depth for perft test")
      ("endDepth", po::value<int>(&perftEnd)->default_value(5), "end depth for perft test")
      ("onDemand", po::value<bool>(&perftOnDemand)->default_value(false), "use on demand move generation for perft test")
      // Benchmark options
      ("bench", "run benchmark to measure NPS")
      ("benchDepth", po::value<int>()->default_value(12), "search depth for benchmark (1-127)")
      ("benchHash", po::value<int>()->default_value(128), "hash size in MB for benchmark (1-65536)")
      // Search thread options
      ("threads", po::value<int>()->default_value(0), "number of search threads (1-64, 0=use config default)")
      // Testsuite options
      ("testsuite", po::value<std::string>(&testsuite_file), "run testsuite in given file")
      ("tsDepth", po::value<int>(&testsuite_depth)->default_value(0), "max search depth per test in testsuite")
      ("tsTime", po::value<int>(&testsuite_time)->default_value(1'000), "time in ms per test in testsuite")
      // Syzygy tablebase options
      ("syzygy", po::value<std::string>(&syzygy_command),
        "syzygy tablebase command:\n"
        "  help     - show detailed syzygy help\n"
        "  status   - show local tablebase status\n"
        "  verify   - verify files using MD5 checksums\n"
        "  download - download tablebase files")
      ("pieces", po::value<std::string>(&syzygy_pieces)->default_value(""),
        "piece counts to process (e.g., 3-4-5 or 3,4,5,6)\n"
        "  download: defaults to 3-4-5\n"
        "  verify/status: defaults to all (3-6)")
      ("path", po::value<std::string>(&syzygy_path),
        "target path for tablebase files\n"
        "(default: from config or platform default)");

    // =========================================================================
    // Declare a group of options that will be allowed both on command line
    // and in config file
    // =========================================================================
    po::options_description config("Configuration");
    config.add_options()
      ("log_lvl,l", po::value<std::string>()->default_value("warn"), "set general log level <critical|error|warn|info|debug|trace>")
      ("search_log_lvl,s", po::value<std::string>()->default_value("warn"), "set search log level <critical|error|warn|info|debug|trace>")
      ("nobook", "do not use opening book")
      ("book,b", po::value<std::string>(&book_file), "opening book to use")
      ("booktype,t", po::value<std::string>(&book_type), "type of opening book <simple|san|pgn>");

    // =========================================================================
    // Hidden options will be allowed both on command line and in config file,
    // but will not be shown to the user when printing help.
    // =========================================================================
    po::options_description hidden("Hidden options");
    hidden.add_options()
      ("test", po::value<std::string>(), "test_hidden");

    // clang-format on

    // Consolidate
    po::options_description cmdline_options;
    cmdline_options.add(generic).add(config).add(hidden);
    po::options_description config_file_options;
    config_file_options.add(config).add(hidden);
    po::options_description visible("Allowed options");
    visible.add(generic).add(config);
    po::positional_options_description p;
    p.add("input-file", -1);
    store(po::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), programOptions);
    notify(programOptions);

    // Install crash handler early to catch any crashes during initialization or runtime
    // This generates minidumps (Windows) or stack traces (Linux) on unhandled exceptions
    crashhandler::install(crash_dump_path);

    if (programOptions.contains("help")) {
      std::cout << visible << "\n";
      return 0;
    }

    if (programOptions.contains("version")) {
      std::cout << "Version: " << appName << "\n";
      return 0;
    }

    if (programOptions.contains("ucioptions")) {
      // Initialize to ensure all static data is ready
      init::init();
      // Print UCI options exactly like the "uci" command would
      std::cout << "id name FrankyCPP v" << FrankyCPP_VERSION_MAJOR << "." << FrankyCPP_VERSION_MINOR << "\n";
      std::cout << "id author Frank Kopp, Germany\n";
      std::cout << UciOptions::getInstance()->str() << "\n";
      return 0;
    }

    if (programOptions.contains("show-config")) {
      // Initialize to ensure all static data is ready
      init::init();

      // Parse domain filter
      const std::optional<ConfigDomain> domainFilter = parseDomainName(show_config_domain);

      // Get current config values
      const auto& searchConfig = ConfigManager::instance().search();
      const auto& evalConfig   = ConfigManager::instance().eval();

      // Generate output based on format
      if (show_config_format == "yaml") {
        std::cout << generateYamlTemplate(domainFilter);
      }
      else if (show_config_format == "json") {
        std::cout << generateConfigJson(searchConfig, evalConfig, domainFilter);
      }
      else {
        // Default: table format
        std::cout << "FrankyCPP v" << FrankyCPP_VERSION_MAJOR << "." << FrankyCPP_VERSION_MINOR
                  << " - Configuration Settings\n\n";
        std::cout << generateConfigTable(searchConfig, evalConfig, domainFilter);
      }
      return 0;
    }

    // read the config file
    std::ifstream ifs(config_file.c_str());
    if (!ifs) {
      std::cerr << "could not open the config file: " << config_file << "\n";
    }
    else {
      store(parse_config_file(ifs, config_file_options), programOptions);
      notify(programOptions);
    }

    // opening book
    if (programOptions.contains("nobook")) {
      CONFIG_OVERRIDE(s.USE_BOOK = false;);
      LOG__INFO(Logger::get().BOOK_LOG, "Not using opening book.");
    }
    else if (programOptions.contains("book")) {
      if (!programOptions.contains("booktype")) {
        LOG__ERROR(Logger::get().BOOK_LOG, "Opening book type is missing (use --help for details). Using default book.");
      }
      else {
        const auto& bookPath = programOptions["book"].as<std::string>();
        if (!std::filesystem::exists(bookPath)) {
          LOG__ERROR(Logger::get().BOOK_LOG, "Opening book '{}' not found. Using default {}",
                     bookPath, SEARCH_CONFIG.BOOK_PATH);
        }
        else {
          CONFIG_OVERRIDE(s.BOOK_PATH = bookPath;);
        }
        const auto& bookType = programOptions["booktype"].as<std::string>();
        if (bookType == "simple" || bookType == "SIMPLE") {
          CONFIG_OVERRIDE(s.BOOK_TYPE = "SIMPLE";);
        }
        else if (bookType == "san" || bookType == "SAN") {
          CONFIG_OVERRIDE(s.BOOK_TYPE = "SIMPLE";);
        }
        else if (bookType == "pgn" || bookType == "PGN") {
          CONFIG_OVERRIDE(s.BOOK_TYPE = "PGN";);
        }
      }
    }

    // Search threads configuration
    const int threadsArg = programOptions["threads"].as<int>();
    if (threadsArg > 0) {
      const int clampedThreads = std::clamp(threadsArg, 1, 64);
      CONFIG_OVERRIDE(s.THREADS = clampedThreads;);
      LOG__INFO(Logger::get().APP_LOG, "Search threads set to {} via command line.", clampedThreads);
    }

    // Testsuite run from cmd line
    if (programOptions.contains("testsuite")) {
      init::init();
      std::cout << "RUNNING TEST SUITE\n";
      std::cout << "########################################################\n";
      std::cout << "Version: " << appName << "\n";
      std::ifstream file(testsuite_file);
      if (file.is_open()) {
        std::cout << "Running Testsuite:  " << testsuite_file << "\n";
        file.close();
      }
      else {
        std::cerr << "Could not read file: " << testsuite_file << "\n";
        return 1;
      }
      std::cout << "Time per Test:      " << std::format(projectLocale, "{:L}", testsuite_time) << "\n";
      std::cout << "Max depth per Test: " << std::format(projectLocale, "{:L}", testsuite_depth) << "\n";
      TestSuite testSuite{milliseconds{testsuite_time}, Depth{testsuite_depth}, testsuite_file};
      testSuite.runTestSuite();
      return 0;
    }

    // Perft run from cmd line
    if (programOptions.contains("perft")) {
      init::init();
      std::cout << std::endl;
      std::cout << "RUNNING PERFT TEST\n";
      std::cout << "########################################################\n";
      std::cout << "Version: " << appName << "\n";
      std::cout << "Start depth: " << std::format(projectLocale, "{:L}", perftStart) << "\n";
      std::cout << "End depth  : " << std::format(projectLocale, "{:L}", perftEnd) << "\n";
      std::cout << "On Demand  : " << (perftOnDemand ? "true" : "false") << "\n";
      std::cout << std::endl;
      Perft perft{};
      perft.perft(perftStart, perftEnd, perftOnDemand);
      return 0;
    }

    // Benchmark run from cmd line
    if (programOptions.contains("bench")) {
      init::init();
      const int benchDepth = programOptions["benchDepth"].as<int>();
      const int benchHash  = programOptions["benchHash"].as<int>();
      std::cout << std::endl;
      std::cout << "RUNNING BENCHMARK\n";
      std::cout << "########################################################\n";
      std::cout << "Version: " << appName << "\n";
      std::cout << "Depth:   " << benchDepth << "\n";
      std::cout << "Hash:    " << benchHash << " MB\n";
      std::cout << "Threads: " << SEARCH_CONFIG.THREADS << "\n";
      std::cout << std::endl;
      engine::BenchConfig benchConfig;
      benchConfig.depth      = benchDepth;
      benchConfig.hashSizeMB = benchHash;
      benchConfig.threads    = SEARCH_CONFIG.THREADS;
      const auto result      = engine::Benchmark::run(benchConfig);
      engine::Benchmark::printResults(result);
      return 0;
    }

    // Syzygy tablebase commands
    if (programOptions.contains("syzygy")) {
      init::init();
      std::cout << std::endl;
      std::cout << "SYZYGY TABLEBASE UTILITY\n";
      std::cout << "########################################################\n";

      // Parse piece counts from string like "3-4-5" or "3,4,5"
      auto parsePieceCounts = [](const std::string& str) -> std::vector<int> {
        std::vector<int> counts;
        std::string s = str;
        // Replace dashes with commas for uniform parsing
        std::ranges::replace(s, '-', ',');
        size_t pos = 0;
        while ((pos = s.find(',')) != std::string::npos) {
          const int val = std::stoi(s.substr(0, pos));
          if (val >= 3 && val <= 6) counts.push_back(val);
          s.erase(0, pos + 1);
        }
        if (!s.empty()) {
          const int val = std::stoi(s);
          if (val >= 3 && val <= 6) counts.push_back(val);
        }
        return counts;
      };

      if (syzygy_command == "help") {
        std::cout << R"(
Syzygy Tablebase Commands
=========================

COMMANDS:
  --syzygy help      Show this help message
  --syzygy status    Show status of local tablebases
  --syzygy verify    Verify tablebase files using MD5 checksums
  --syzygy download  Download tablebase files from online mirrors

OPTIONS:
  --pieces <list>    Piece counts to download/verify (default: 3-4-5)
                     Format: 3-4-5 or 3,4,5
                     Valid values: 3, 4, 5, 6
  --path <dir>       Target directory for tablebase files
                     Default: from TB_PATH config or platform default

EXAMPLES:
  # Check local tablebase status
  FrankyCPP --syzygy status

  # Check status of specific directory
  FrankyCPP --syzygy status --path D:\SYZYGY

  # Verify tablebase files against server checksums
  FrankyCPP --syzygy verify --path D:\SYZYGY

  # Verify only 3 and 4-piece tablebases
  FrankyCPP --syzygy verify --path D:\SYZYGY --pieces 3-4

  # Download 3, 4, 5-piece tablebases to default location
  FrankyCPP --syzygy download

  # Download only 3 and 4-piece (smallest, ~80MB)
  FrankyCPP --syzygy download --pieces 3-4

  # Download to specific directory
  FrankyCPP --syzygy download --pieces 3-4-5 --path D:\Chess\Syzygy

TABLEBASE SIZES (approximate):
  3-piece:  ~7 MB
  4-piece:  ~75 MB
  5-piece:  ~1 GB
  6-piece:  ~150 GB (recommend torrent download)

DOWNLOAD SOURCES:
  Primary:  https://tablebase.lichess.ovh/tables/standard/
  Backup:   https://tablebase.sesse.net/syzygy/

CONFIGURATION:
  Set TB_PATH in config/search.yaml to auto-detect tablebases at startup.
  Or set TB_PATH environment variable.

)";
        return 0;
      }

      if (syzygy_command == "status") {
        // Show status of local tablebases
        std::string tbPath = syzygy_path;
        if (tbPath.empty()) {
          tbPath = tablebase::findTablebasePath();
        }

        std::cout << "\nTablebase Status:\n";
        std::cout << std::string(40, '-') << "\n";

        if (tbPath.empty()) {
          std::cout << "No tablebase path configured or found.\n";
          std::cout << "\nTo configure:\n";
          std::cout << "  - Set TB_PATH in config/search.yaml\n";
          std::cout << "  - Or set TB_PATH environment variable\n";
          std::cout << "  - Or use --path option\n";
        }
        else {
          std::cout << tablebase::getTablebaseStatus(tbPath) << "\n";
        }
        return 0;
      }

      if (syzygy_command == "verify") {
        // Verify tablebases using MD5 checksums from server
        std::string tbPath = syzygy_path;
        if (tbPath.empty()) {
          tbPath = tablebase::findTablebasePath();
        }

        if (tbPath.empty()) {
          std::cerr << "Error: No tablebase path specified. Use --path option.\n";
          return 1;
        }

        // Empty string = verify all files, otherwise parse specified pieces
        const auto pieceCounts = parsePieceCounts(syzygy_pieces);

        std::cout << "\nVerifying Tablebase Files:\n";
        std::cout << std::string(40, '-') << "\n";
        std::cout << "Path: " << tbPath << "\n";
        if (pieceCounts.empty()) {
          std::cout << "Piece counts: all (3-6)\n";
        }
        else {
          std::cout << "Piece counts: ";
          for (size_t i = 0; i < pieceCounts.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << pieceCounts[i];
          }
          std::cout << "\n";
        }
        std::cout << std::string(40, '-') << "\n\n";

        const auto verifyResult = tablebase::TablebaseDownloader::verify(tbPath, pieceCounts,
                                                                         [](const tablebase::DownloadProgress& progress) {
                                                                           std::cout << "\r[" << progress.percentComplete() << "%] "
                                                                                     << progress.filesCompleted << "/" << progress.totalFiles
                                                                                     << " - " << progress.currentFile;
                                                                           if (!progress.success) {
                                                                             std::cout << " (FAILED)";
                                                                           }
                                                                           else {
                                                                             std::cout << " (OK)    ";
                                                                           }
                                                                           std::cout << std::string(10, ' ') << std::flush;
                                                                         });

        std::cout << "\n\n";
        std::cout << "Verification Summary:\n";
        std::cout << std::string(40, '-') << "\n";
        std::cout << "Verified (OK):        " << verifyResult.filesVerified << " files\n";
        std::cout << "Failed (MD5 mismatch):" << verifyResult.filesFailed << " files\n";
        std::cout << "Missing:              " << verifyResult.filesMissing << " files\n";
        std::cout << "No reference checksum:" << verifyResult.filesNoChecksum << " files\n";

        if (!verifyResult.errors.empty()) {
          std::cout << "\nErrors:\n";
          for (const auto& err : verifyResult.errors) {
            std::cout << "  - " << err << "\n";
          }
        }

        if (verifyResult.success) {
          std::cout << "\nAll files verified successfully!\n";
        }
        else {
          std::cout << "\nVerification failed. Some files may be corrupted.\n";
          std::cout << "Consider re-downloading the failed files.\n";
        }

        return verifyResult.success ? 0 : 1;
      }

      if (syzygy_command == "download") {
        // Download tablebases
        std::string targetPath = syzygy_path;
        if (targetPath.empty()) {
          targetPath = tablebase::getDefaultTablebasePath();
          if (targetPath.empty()) {
            std::cerr << "Error: No target path specified. Use --path option.\n";
            return 1;
          }
        }

        // Default to 3-4-5 for download if not specified
        const std::string piecesStr = syzygy_pieces.empty() ? "3-4-5" : syzygy_pieces;
        const auto pieceCounts      = parsePieceCounts(piecesStr);
        if (pieceCounts.empty()) {
          std::cerr << "Error: No valid piece counts specified. Use --pieces 3-4-5\n";
          return 1;
        }

        std::cout << "Target path: " << targetPath << "\n";
        std::cout << "Piece counts: ";
        for (size_t i = 0; i < pieceCounts.size(); ++i) {
          if (i > 0) std::cout << ", ";
          std::cout << pieceCounts[i];
        }
        std::cout << "\n";

        const size_t estimatedSize = tablebase::TablebaseDownloader::estimateDownloadSize(pieceCounts);
        std::cout << "Estimated download size: " << tablebase::TablebaseDownloader::formatSize(estimatedSize) << "\n";
        std::cout << std::string(40, '-') << "\n";

        // Check for 6-piece warning
        if (std::ranges::find(pieceCounts, 6) != pieceCounts.end()) {
          std::cout << "\nWARNING: 6-piece tablebases are ~150GB.\n";
          std::cout << "Consider using torrent download for better reliability.\n";
          std::cout << "Download sources:\n";
          std::cout << "  - https://tablebase.sesse.net/syzygy/\n";
          std::cout << "  - https://tablebase.lichess.ovh/tables/standard/\n\n";
        }

        tablebase::DownloadConfig downloadConfig;
        downloadConfig.pieceCounts = pieceCounts;
        downloadConfig.targetPath  = targetPath;
        downloadConfig.verbose     = true;

        const auto downloadResult = tablebase::TablebaseDownloader::download(downloadConfig,
                                                                             [](const tablebase::DownloadProgress& progress) {
                                                                               std::cout << "\r[" << progress.percentComplete() << "%] "
                                                                                         << progress.filesCompleted << "/" << progress.totalFiles
                                                                                         << " - " << progress.currentFile;
                                                                               if (!progress.success && !progress.errorMessage.empty()) {
                                                                                 std::cout << " (FAILED)";
                                                                               }
                                                                               std::cout << std::string(20, ' ') << std::flush;
                                                                             });

        std::cout << "\n\n";
        std::cout << "Download Summary:\n";
        std::cout << std::string(40, '-') << "\n";
        std::cout << "Downloaded: " << downloadResult.filesDownloaded << " files\n";
        std::cout << "Skipped (existing): " << downloadResult.filesSkipped << " files\n";
        std::cout << "Failed: " << downloadResult.filesFailed << " files\n";

        if (!downloadResult.errors.empty()) {
          std::cout << "\nErrors:\n";
          for (const auto& err : downloadResult.errors) {
            std::cout << "  - " << err << "\n";
          }
          // Show manual download instructions on failure
          std::cout << tablebase::TablebaseDownloader::getManualDownloadInstructions();
        }

        return downloadResult.success ? 0 : 1;
      }

      // Unknown command
      std::cerr << "Unknown syzygy command: " << syzygy_command << "\n";
      std::cerr << "Available commands: help, status, verify, download\n";
      std::cerr << "Use --syzygy help for detailed usage information.\n";
      return 1;
    }

    // just a test - does nothing
    if (programOptions.contains("test")) {
      std::cout << "Test of hidden command line option." << std::endl;
      std::cout << programOptions["test"].as<std::string>() << std::endl;
    }

  } catch (std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  } catch (...) {
    std::cerr << "Exception of unknown type!\n";
    return 1;
  }

  // Init all pre calculated data structures
  init::init();

  // Create engine and start UCI loop
  UciHandler uci{};
  uci.loop();

  return 0;
}
