#include "init.h"
#include "version.h"
#include <chesscore/Perft.h>
#include <engine/SearchConfig.h>
#include <engine/UciHandler.h>
#include <enginetest/TestSuite.h>
#include <fstream>
#include <iostream>

// BOOST program options
#include "boost/program_options.hpp"
namespace po = boost::program_options;

// global variable for program options
inline po::variables_map programOptions;

int main(int argc, char* argv[]) {

  // Version comes from CMAKE template version.h.in
  std::string appName = "FrankyCPP_NewGen";
  appName
    .append(" v")
    .append(std::to_string(FrankyCPP_VERSION_MAJOR))
    .append(".")
    .append(std::to_string(FrankyCPP_VERSION_MINOR));
  std::cout << appName << std::endl;

  std::string config_file;
  std::string book_file;
  std::string book_type;
  std::string testsuite_file;
  int testsuite_time;
  int testsuite_depth;
  int perftStart;
  int perftEnd;

  // Command line options
  try {
    // @formatter:off
    
    // Declare a group of options that will be allowed only on command line
    po::options_description generic("Generic options");
    generic.add_options()
      ("help,?", "produce help message")("version,v", "print version string")
      ("config,c", po::value<std::string>(&config_file)->default_value("./config/FrankyCPP.cfg"), "name of a file of a configuration.");

    // Declare a group of options that will be allowed both on command line
    // and in config file
    po::options_description config("Configuration");
    config.add_options()
      ("log_lvl,l", po::value<std::string>()->default_value("warn"), "set general log level <critical|error|warn|info|debug|trace>")
      ("search_log_lvl,s", po::value<std::string>()->default_value("warn"), "set search log level <critical|error|warn|info|debug|trace>")
      ("nobook", "do not use opening book")
      ("book,b", po::value<std::string>(&book_file), "opening book to use")
      ("booktype,t", po::value<std::string>(&book_type), "type of opening book <simple|san|pgn>")
      ("testsuite", po::value<std::string>(&testsuite_file), "run testsuite in given file")
      ("tsTime", po::value<int>(&testsuite_time)->default_value(1'000), "time in ms per test in testsuite")
      ("tsDepth", po::value<int>(&testsuite_depth)->default_value(0), "max search depth per test in testsuite")
      ("perft", "run perft test")
      ("startDepth", po::value<int>(&perftStart)->default_value(1), "start depth for perft test")
      ("endDepth", po::value<int>(&perftEnd)->default_value(5), "end depth for perft test");

    // Hidden options, will be allowed both on command line and in config file,
    // but will not be shown to the user when printing help.
    po::options_description hidden("Hidden options");
    hidden.add_options()
      ("test,t", po::value<std::string>(), "test_hidden");
    // @formatter:on

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

    if (programOptions.count("help")) {
      std::cout << visible << "\n";
      return 0;
    }

    if (programOptions.count("version")) {
      std::cout << "Version: " << appName << "\n";
      return 0;
    }

    //     read config file
    std::ifstream ifs(config_file.c_str());
    if (!ifs) {
      std::cerr << "could not open config file: " << config_file << "\n";
    }
    else {
      store(parse_config_file(ifs, config_file_options), programOptions);
      notify(programOptions);
    }

    // opening book
    if (programOptions.count("nobook")) {
      SearchConfig::USE_BOOK = false;
      LOG__INFO(Logger::get().BOOK_LOG, "Not using opening book.");
    }
    else if (programOptions.count("book")) {
      if (!programOptions.count("booktype")) {
        LOG__ERROR(Logger::get().BOOK_LOG, "Opening book type is missing (use --help for details). Using default book.");
      }
      else {
        const auto& bookPath = programOptions["book"].as<std::string>();
        if (!OpeningBook::fileExists(bookPath)) {
          LOG__ERROR(Logger::get().BOOK_LOG, "Open book '{}' not found. Using default {}", bookPath, SearchConfig::BOOK_PATH);
        }
        else {
          SearchConfig::BOOK_PATH = bookPath;
        }
        const auto& bookType = programOptions["booktype"].as<std::string>();
        if (bookType == "simple" || bookType == "SIMPLE") {
          SearchConfig::BOOK_TYPE = OpeningBook::BookFormat::SIMPLE;
        }
        else if (bookType == "san" || bookType == "SAN") {
          SearchConfig::BOOK_TYPE = OpeningBook::BookFormat::SAN;
        }
        else if (bookType == "pgn" || bookType == "PGN") {
          SearchConfig::BOOK_TYPE = OpeningBook::BookFormat::PGN;
        }
      }
    }

    // Testsuite run from cmd line
    if (programOptions.count("testsuite")) {
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
      std::cout << "Time per Test:      " << fmt::format("{:n}", testsuite_time) << "\n";
      std::cout << "Max depth per Test: " << fmt::format("{:n}", testsuite_depth) << "\n";
      TestSuite testSuite{MilliSec{testsuite_time}, Depth{testsuite_depth}, testsuite_file};
      testSuite.runTestSuite();
      return 0;
    }

    if (programOptions.count("perft")) {
      init::init();
      std::cout << std::endl;
      std::cout << "RUNNING PERFT TEST\n";
      std::cout << "########################################################\n";
      std::cout << "Version: " << appName << "\n";
      std::cout << "Start depth: " << fmt::format("{:n}", perftStart) << "\n";
      std::cout << "End depth  : " << fmt::format("{:n}", perftEnd) << "\n";
      std::cout << std::endl;
      Perft perft{};
      perft.perft(perftStart, perftEnd, true);
      return 0;
    }

    // just a test - does nothing
    if (programOptions.count("test")) {
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
