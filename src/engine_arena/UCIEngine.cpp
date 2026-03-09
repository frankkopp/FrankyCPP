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

//=============================================================================
// UCIEngine.cpp - External UCI Chess Engine Interface Implementation
//=============================================================================
//
// Uses Boost.Asio async_pipe for reliable, cancellable I/O.
// Key pattern:
//   - io_context runs in a dedicated thread
//   - async_read_until reads lines into a queue
//   - readLine() pulls from queue with timeout via condition_variable
//   - Shutdown: stop io_context, join thread, terminate process
//
//=============================================================================

#include "UCIEngine.h"
#include "common/stringutil.h"

#include <boost/process/v1/args.hpp>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace common;
using namespace chess;
namespace arena {

  namespace bp = boost::process::v1;
  using std::chrono::duration_cast;
  using std::chrono::steady_clock;

  //=============================================================================
  // Constructors/Destructor
  //=============================================================================

  UCIEngine::UCIEngine(const std::string& enginePath, const std::string& commandLineArgs,
                       const bool debugMode, const std::string& uciOptions)
      : enginePath_(enginePath), debugMode_(debugMode), pendingUciOptions_(uciOptions) {

    // Check if engine file exists
    if (!std::filesystem::exists(enginePath)) {
      throw std::runtime_error("UCI engine not found: " + enginePath);
    }

    if (debugMode_) {
      std::cout << "[UCIEngine] Starting engine: " << enginePath << std::endl;
      if (!commandLineArgs.empty()) {
        std::cout << "[UCIEngine] Command-line args: " << commandLineArgs << std::endl;
      }
      if (!uciOptions.empty()) {
        std::cout << "[UCIEngine] UCI options (before init): " << uciOptions << std::endl;
      }
    }

    // Create async pipe for stdout (using io_context)
    pipeOut_ = std::make_unique<bp::async_pipe>(ioContext_);

    // Create sync stream for stdin
    pipeIn_ = std::make_unique<bp::opstream>();

    // Start engine subprocess with pipe redirection
    // Pass enginePath separately (Boost.Process handles paths with spaces correctly)
    // Pass arguments via bp::args to avoid command-line injection issues
    try {
      if (commandLineArgs.empty()) {
        childProcess_ = std::make_unique<bp::child>(
          enginePath,
          bp::std_in<*pipeIn_,
                     bp::std_out>
            * pipeOut_);
      }
      else {
        // Split arguments on whitespace - users needing complex quoting should
        // use UCI setoption commands instead of command-line arguments
        std::vector<std::string> args;
        std::istringstream iss(commandLineArgs);
        std::string arg;
        while (iss >> arg) {
          args.push_back(arg);
        }

        childProcess_ = std::make_unique<bp::child>(
          enginePath,
          bp::args(args),
          bp::std_in<*pipeIn_,
                     bp::std_out>
            * pipeOut_);
      }
    } catch (const std::exception& e) {
      throw std::runtime_error("Failed to start UCI engine: " + std::string(e.what()));
    }

    if (!childProcess_ || !childProcess_->running()) {
      throw std::runtime_error("Failed to start UCI engine: " + enginePath);
    }

    // Start async reading
    startAsyncRead();

    // Start I/O thread to run the io_context
    ioThread_ = std::thread([this]() {
      // Use work guard to keep io_context running until we explicitly stop it
      auto workGuard = boost::asio::make_work_guard(ioContext_);
      ioContext_.run();
    });

    // Initialize UCI protocol
    try {
      initializeUCI();
    } catch (const std::exception& e) {
      // Cleanup on initialization failure
      stopping_.store(true);
      ioContext_.stop();
      if (ioThread_.joinable()) {
        ioThread_.join();
      }
      if (childProcess_) {
        childProcess_->terminate();
        childProcess_->wait();
      }
      throw std::runtime_error("UCI initialization failed: " + std::string(e.what()));
    }

    std::cout << "UCI engine initialized: " << engineName_ << std::endl;
  }

  UCIEngine::~UCIEngine() {
    // Send quit command first (if process is running)
    if (childProcess_ && childProcess_->running()) {
      try {
        sendCommand("quit");
      } catch (...) {
        // Ignore errors during shutdown
      }
      // Give engine time to shutdown gracefully
      std::this_thread::sleep_for(milliseconds(100));
    }

    // Signal stopping
    stopping_.store(true);

    // Wake up any waiters
    queueCV_.notify_all();

    // Cancel pending async operations on the pipe FIRST
    // This is critical - on Windows, io_context.stop() alone doesn't cancel in-flight reads
    if (pipeOut_) {
      boost::system::error_code ec;
      pipeOut_->cancel();  // Cancel pending async operations (no argument on Windows)
      pipeOut_->close(ec); // Close the pipe
    }

    // Now stop the io_context
    ioContext_.stop();

    // Wait for I/O thread to finish
    if (ioThread_.joinable()) {
      ioThread_.join();
    }


    // Terminate process if still running
    if (childProcess_) {
      if (childProcess_->running()) {
        childProcess_->terminate();
      }
      childProcess_->wait();
    }
  }

  //=============================================================================
  // Core functionality
  //=============================================================================

  void UCIEngine::newGame() {
    if (!childProcess_ || !childProcess_->running()) {
      std::cerr << "ERROR: Engine process not running" << std::endl;
      return;
    }

    sendCommand("ucinewgame");
    waitUntilReady();
  }

  bool UCIEngine::setPosition(const std::string& fen) {
    if (!childProcess_ || !childProcess_->running()) {
      std::cerr << "ERROR: Engine process not running" << std::endl;
      return false;
    }

    const std::string command = "position fen " + fen;
    sendCommand(command);
    return waitUntilReady();
  }

  UCISearchResult UCIEngine::search(milliseconds timeMs, Depth maxDepth) {
    UCISearchResult result;

    if (!childProcess_ || !childProcess_->running()) {
      std::cerr << "ERROR: Engine process not running" << std::endl;
      return result;
    }

    // Build go command
    std::ostringstream cmd;
    cmd << "go";
    if (timeMs.count() > 0) {
      cmd << " movetime " << timeMs.count();
    }
    if (maxDepth > 0) {
      cmd << " depth " << maxDepth;
    }

    sendCommand(cmd.str());

    // Scale timeout based on requested move time
    milliseconds effectiveTimeout = searchTimeout_;
    if (timeMs.count() > 0) {
      constexpr milliseconds::rep timeoutFactor = 3;
      constexpr auto maxRep                     = std::numeric_limits<milliseconds::rep>::max();
      const auto baseMs                         = timeMs.count();
      const auto scaledMs                       = baseMs > maxRep / timeoutFactor
                                                    ? maxRep
                                                    : baseMs * timeoutFactor;
      effectiveTimeout                          = milliseconds(scaledMs);
    }

    // Read response lines until we get "bestmove"
    const auto deadline = steady_clock::now() + effectiveTimeout;

    while (steady_clock::now() < deadline) {
      std::string line;
      const auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now());

      if (!readLine(line, remaining)) {
        std::cerr << "ERROR: Timeout waiting for engine response" << std::endl;
        break;
      }

      if (line.rfind("info ", 0) == 0) {
        parseInfoLine(line, result);
        continue;
      }

      if (line.rfind("bestmove ", 0) == 0) {
        std::istringstream iss(line);
        std::string keyword, move;
        iss >> keyword >> move;

        if (!move.empty()) {
          result.bestMove = move;
          return result;
        }
      }
    }

    if (result.bestMove.empty()) {
      std::cerr << "ERROR: Engine search failed or timed out" << std::endl;
    }

    return result;
  }

  void UCIEngine::setOption(const std::string& name, const std::string& value) {
    if (!childProcess_ || !childProcess_->running()) {
      std::cerr << "ERROR: Engine process not running" << std::endl;
      return;
    }

    std::ostringstream cmd;
    cmd << "setoption name " << name << " value " << value;
    sendCommand(cmd.str());
    waitUntilReady();
  }

  void UCIEngine::setUciOptions(const std::string& options) {
    if (options.empty()) {
      return;
    }

    const auto pairs = parseOptionPairs(options);
    for (const auto& [name, value] : pairs) {
      setOption(name, value);
    }
  }

  std::map<std::string, std::string> UCIEngine::getOptions() {
    std::map<std::string, std::string> options;

    if (!childProcess_ || !childProcess_->running()) {
      std::cerr << "ERROR: Engine process not running" << std::endl;
      return options;
    }

    sendCommand("getoptions");

    const auto deadline = steady_clock::now() + milliseconds(5000);

    while (steady_clock::now() < deadline) {
      std::string line;
      const auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now());

      if (!readLine(line, remaining)) {
        break;
      }

      if (line.find("optionsok") != std::string::npos) {
        break;
      }

      const size_t optionNamePos = line.find("option name ");
      if (optionNamePos != std::string::npos) {
        const size_t nameStart = optionNamePos + 12;
        const size_t typePos   = line.find(" type ", nameStart);

        if (typePos == std::string::npos) {
          continue;
        }

        std::string name        = line.substr(nameStart, typePos - nameStart);
        const size_t currentPos = line.find(" current ", typePos);

        if (currentPos != std::string::npos) {
          const size_t valueStart = currentPos + 9;
          const size_t valueEnd   = line.find_first_of(" \t\r\n", valueStart);

          std::string value;
          if (valueEnd != std::string::npos) {
            value = line.substr(valueStart, valueEnd - valueStart);
          }
          else {
            value = line.substr(valueStart);
          }

          value.erase(value.find_last_not_of(" \t\r\n") + 1);
          options[name] = value;
        }
      }
    }

    return options;
  }

  //=============================================================================
  // Private helper methods
  //=============================================================================

  void UCIEngine::sendCommand(const std::string& command) const {
    if (!pipeIn_ || !childProcess_ || !childProcess_->running()) {
      throw std::runtime_error("Engine process not running");
    }

    if (debugMode_) {
      std::cout << "[UCIEngine] >>> " << command << std::endl;
    }

    *pipeIn_ << command << std::endl;
    pipeIn_->flush();
  }

  void UCIEngine::startAsyncRead() {
    if (stopping_.load()) {
      return;
    }

    // Async read until newline
    boost::asio::async_read_until(
      *pipeOut_,
      readBuffer_,
      '\n',
      [this](const boost::system::error_code& ec, const std::size_t bytesTransferred) {
        handleRead(ec, bytesTransferred);
      });
  }

  void UCIEngine::handleRead(const boost::system::error_code& ec, const std::size_t bytesTransferred) {
    (void) bytesTransferred; // Unused - we read lines from the streambuf directly

    if (stopping_.load()) {
      return;
    }

    if (ec) {
      // Error or EOF - signal to consumers
      {
        std::lock_guard lock(queueMutex_);
        // Push empty string to signal EOF
      }
      queueCV_.notify_all();
      return;
    }

    // Extract line from buffer
    std::istream is(&readBuffer_);
    std::string line;
    std::getline(is, line);

    // Trim whitespace (including Windows CRLF)
    line = trimFast(line);

    if (debugMode_) {
      std::cout << "[UCIEngine] <<< " << line << std::endl;
    }

    // Skip empty lines and engine log lines (lines starting with '[' are typically timestamps/log output)
    // UCI protocol responses never start with '[', so this filters out noise
    if (line.empty() || line[0] == '[') {
      // Continue reading without queuing this line
      startAsyncRead();
      return;
    }

    // Push to queue and notify waiters
    {
      std::lock_guard lock(queueMutex_);
      lineQueue_.push(std::move(line));
    }
    queueCV_.notify_one();

    // Continue reading
    startAsyncRead();
  }

  bool UCIEngine::readLine(std::string& line, const milliseconds timeoutMs) {
    line.clear();

    std::unique_lock lock(queueMutex_);
    const bool gotLine = queueCV_.wait_for(lock, timeoutMs, [this]() {
      return !lineQueue_.empty() || stopping_.load();
    });

    if (!gotLine || lineQueue_.empty()) {
      return false;
    }

    line = std::move(lineQueue_.front());
    lineQueue_.pop();

    return true;
  }

  bool UCIEngine::waitForResponse(const std::string& expectedResponse, const milliseconds timeoutMs) {
    const auto deadline = steady_clock::now() + timeoutMs;

    while (steady_clock::now() < deadline) {
      std::string line;
      const auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now());

      if (readLine(line, remaining)) {
        if (line.find(expectedResponse) != std::string::npos) {
          return true;
        }
      }
      else {
        break;
      }
    }

    return false;
  }

  void UCIEngine::initializeUCI() {
    if (debugMode_) {
      std::cout << "[UCIEngine] Initializing UCI protocol (timeout: " << initTimeout_.count() / 1000 << "s)..." << std::endl;
    }

    sendCommand("uci");

    const auto deadline = steady_clock::now() + initTimeout_;
    bool receivedUciOk  = false;

    while (steady_clock::now() < deadline) {
      std::string line;
      const auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now());

      if (!readLine(line, remaining)) {
        break;
      }

      if (line.rfind("id name ", 0) == 0) {
        engineName_ = line.substr(8);
      }

      if (line.find("uciok") != std::string::npos) {
        receivedUciOk = true;
        break;
      }
    }

    if (!receivedUciOk) {
      throw std::runtime_error("Engine did not respond with 'uciok'");
    }

    if (engineName_.empty()) {
      engineName_ = "Unknown Engine";
    }

    // Send pending UCI options BEFORE isready
    // This ensures options like OwnBook=false take effect before engine initializes
    if (!pendingUciOptions_.empty()) {
      if (debugMode_) {
        std::cout << "[UCIEngine] Sending UCI options before initialization..." << std::endl;
      }
      // Parse and send each option without waitUntilReady() between them
      // (engine hasn't finished init yet — we send all options then wait once)
      const auto pairs = parseOptionPairs(pendingUciOptions_);
      for (const auto& [name, value] : pairs) {
        std::ostringstream cmd;
        cmd << "setoption name " << name << " value " << value;
        sendCommand(cmd.str());
      }
    }

    if (!waitUntilReady()) {
      throw std::runtime_error("Engine did not respond to 'isready'");
    }
  }

  bool UCIEngine::waitUntilReady() {
    sendCommand("isready");
    return waitForResponse("readyok", initTimeout_);
  }

  std::vector<std::pair<std::string, std::string>> UCIEngine::parseOptionPairs(const std::string& options) {
    std::vector<std::pair<std::string, std::string>> result;

    if (options.empty()) {
      return result;
    }

    // Split into raw tokens
    std::vector<std::string> rawPairs;
    if (options.find(';') != std::string::npos) {
      // Semicolon-separated: "Hash=256; Threads=4"
      std::istringstream iss(options);
      std::string pair;
      while (std::getline(iss, pair, ';')) {
        pair.erase(0, pair.find_first_not_of(" \t\r\n"));
        pair.erase(pair.find_last_not_of(" \t\r\n") + 1);
        if (!pair.empty()) {
          rawPairs.push_back(pair);
        }
      }
    }
    else if (options.find('=') != std::string::npos) {
      // Single option or space-separated: "Hash=256" or "Hash=256 Threads=4"
      std::istringstream iss(options);
      std::string pair;
      while (iss >> pair) {
        if (pair.find('=') != std::string::npos) {
          rawPairs.push_back(pair);
        }
      }
    }

    // Parse each "name=value" pair
    for (const auto& pair : rawPairs) {
      const size_t eqPos = pair.find('=');
      if (eqPos == std::string::npos) {
        std::cerr << "WARNING: Invalid UCI option format (missing '='): " << pair << std::endl;
        continue;
      }

      std::string name  = pair.substr(0, eqPos);
      std::string value = pair.substr(eqPos + 1);

      // Trim whitespace
      name.erase(0, name.find_first_not_of(" \t"));
      name.erase(name.find_last_not_of(" \t") + 1);
      value.erase(0, value.find_first_not_of(" \t"));
      value.erase(value.find_last_not_of(" \t") + 1);

      if (name.empty() || value.empty()) {
        std::cerr << "WARNING: Invalid UCI option (empty name or value): " << pair << std::endl;
        continue;
      }

      result.emplace_back(name, value);
    }

    return result;
  }

  void UCIEngine::parseInfoLine(const std::string& line, UCISearchResult& result) {
    std::istringstream iss(line);
    std::string token;

    iss >> token; // Skip "info"

    while (iss >> token) {
      if (token == "depth") {
        int depth = 0;
        if (iss >> depth) {
          result.depth = static_cast<Depth>(depth);
        }
      }
      else if (token == "nodes") {
        iss >> result.nodes;
      }
      else if (token == "time") {
        int timeMs = 0;
        if (iss >> timeMs) {
          result.time = milliseconds(timeMs);
        }
      }
      else if (token == "score") {
        std::string scoreType;
        if (iss >> scoreType) {
          if (scoreType == "cp") {
            int centipawns = 0;
            if (iss >> centipawns) {
              result.score = static_cast<Value>(centipawns);
            }
          }
          else if (scoreType == "mate") {
            int mateIn = 0;
            if (iss >> mateIn) {
              result.score = mateIn > 0 ? VALUE_CHECKMATE - mateIn : -VALUE_CHECKMATE + mateIn;
            }
          }
        }
      }
    }
  }

  std::string UCIEngine::queryEngineConfig(const std::string& enginePath, const std::string& commandLineArgs) {
    std::ostringstream report;

    try {
      // Start engine temporarily to query configuration
      UCIEngine engine(enginePath, commandLineArgs, false, "");

      // Check if this is a FrankyCPP engine (has getoptions support)
      const std::string& engineName = engine.getEngineName();
      const bool isFrankyCPP        = engineName.find("FrankyCPP") != std::string::npos;

      if (!isFrankyCPP) {
        report << "Note: Engine '" << engineName << "' is not FrankyCPP.\n";
        report << "      UCI has no standard mechanism to query current option values.\n";
        report << "      Only FrankyCPP supports the 'getoptions' command.\n";
        return report.str();
      }

      // Query options using FrankyCPP's getoptions command
      const auto options = engine.getOptions();

      if (options.empty()) {
        report << "Note: Could not retrieve configuration from engine.\n";
        report << "      The 'getoptions' command may not be supported by this version.\n";
        return report.str();
      }

      report << "=== Engine Configuration: " << engineName << " ===\n";

      // Group options by category (Search, Eval, etc.)
      std::map<std::string, std::vector<std::pair<std::string, std::string>>> grouped;
      for (const auto& [name, value] : options) {
        // Simple categorization based on common prefixes
        std::string category = "General";
        if (name.find("USE_") == 0 || name.find("MAX_") == 0 || name.find("MIN_") == 0) {
          category = "Search";
        }
        else if (name.find("EVAL_") == 0 || name.find("PST_") == 0) {
          category = "Evaluation";
        }
        else if (name.find("BOOK") == 0 || name.find("TB") == 0) {
          category = "Books/Tables";
        }
        grouped[category].emplace_back(name, value);
      }

      // Print grouped options
      for (const auto& [category, opts] : grouped) {
        report << "\n[" << category << "]\n";
        for (const auto& [name, value] : opts) {
          report << "  " << std::left << std::setw(30) << name << " = " << value << "\n";
        }
      }

      report << "\n";

    } catch (const std::exception& e) {
      report << "Error querying engine configuration: " << e.what() << "\n";
    }

    return report.str();
  }

} // namespace arena
