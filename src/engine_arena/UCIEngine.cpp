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

#include "UCIEngine.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace arena {

namespace bp = boost::process::v1;
using std::chrono::steady_clock;
using std::chrono::duration_cast;

//=============================================================================
// Constructors/Destructor
//=============================================================================

UCIEngine::UCIEngine(const std::string& enginePath)
    : enginePath(enginePath) {

  // Check if engine file exists
  if (!std::filesystem::exists(enginePath)) {
    throw std::runtime_error("UCI engine not found: " + enginePath);
  }

  // Create pipes and streams
  pipeIn = std::make_unique<bp::opstream>();
  pipeOut = std::make_unique<bp::ipstream>();

  // Start engine subprocess with pipe redirection
  // Note: Boost.Process will create the underlying pipes when we use the redirection operators
  try {
    childProcess = std::make_unique<bp::child>(
      enginePath,
      bp::std_in < *pipeIn,
      bp::std_out > *pipeOut
    );
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to start UCI engine: " + std::string(e.what()));
  }

  if (!childProcess || !childProcess->running()) {
    throw std::runtime_error("Failed to start UCI engine: " + enginePath);
  }

  // Initialize UCI protocol
  try {
    initializeUCI();
  } catch (const std::exception& e) {
    // Cleanup on initialization failure
    if (childProcess) {
      childProcess->terminate();
      childProcess->wait();
    }
    throw std::runtime_error("UCI initialization failed: " + std::string(e.what()));
  }

  std::cout << "UCI engine initialized: " << engineName << std::endl;
}

UCIEngine::~UCIEngine() {
  if (childProcess && childProcess->running()) {
    // Send quit command
    try {
      sendCommand("quit");
    } catch (...) {
      // Ignore errors during shutdown
    }

    // Give engine time to shutdown gracefully
    std::this_thread::sleep_for(milliseconds(100));

    // Terminate if still running
    if (childProcess->running()) {
      childProcess->terminate();
    }
    childProcess->wait();
  }
}

//=============================================================================
// Core functionality
//=============================================================================

void UCIEngine::newGame() {
  if (!childProcess || !childProcess->running()) {
    std::cerr << "ERROR: Engine process not running" << std::endl;
    return;
  }

  // Send ucinewgame to clear engine state (TT, history, etc.)
  sendCommand("ucinewgame");

  // Wait for engine to be ready
  waitUntilReady();
}

bool UCIEngine::setPosition(const std::string& fen) {
  if (!childProcess || !childProcess->running()) {
    std::cerr << "ERROR: Engine process not running" << std::endl;
    return false;
  }

  // Send position command
  std::string command = "position fen " + fen;
  sendCommand(command);

  // Wait for engine to be ready
  return waitUntilReady();
}

UCISearchResult UCIEngine::search(milliseconds timeMs, Depth maxDepth) {
  UCISearchResult result;

  if (!childProcess || !childProcess->running()) {
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

  // Send search command
  sendCommand(cmd.str());

  // Read response lines until we get "bestmove"
  const auto deadline = steady_clock::now() + searchTimeout;

  while (steady_clock::now() < deadline) {
    std::string line;
    const auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now());

    if (!readLine(line, remaining)) {
      std::cerr << "ERROR: Timeout waiting for engine response" << std::endl;
      break;
    }

    // Parse info lines
    if (line.rfind("info ", 0) == 0) {
      parseInfoLine(line, result);
      continue;
    }

    // Parse bestmove
    if (line.rfind("bestmove ", 0) == 0) {
      // Extract move (format: "bestmove e2e4" or "bestmove e2e4 ponder e7e5")
      std::istringstream iss(line);
      std::string keyword, move;
      iss >> keyword >> move;

      if (!move.empty()) {
        result.bestMove = move;
        return result;
      }
    }
  }

  // Timeout or error - return empty result
  if (result.bestMove.empty()) {
    std::cerr << "ERROR: Engine search failed or timed out" << std::endl;
  }

  return result;
}

//=============================================================================
// Private helper methods
//=============================================================================

void UCIEngine::sendCommand(const std::string& command) {
  if (!pipeIn || !childProcess || !childProcess->running()) {
    throw std::runtime_error("Engine process not running");
  }
  *pipeIn << command << std::endl;
  pipeIn->flush();
}

bool UCIEngine::readLine(std::string& line, milliseconds timeoutMs) {
  if (!pipeOut || !childProcess || !childProcess->running()) {
    return false;
  }

  line.clear();

  // Boost.Process pipes are blocking, so we need to use a thread with timeout
  // to avoid hanging forever if the engine doesn't respond
  std::atomic<bool> completed{false};
  std::atomic<bool> success{false};

  std::thread readerThread([&]() {
    if (std::getline(*pipeOut, line)) {
      success = true;
    }
    completed = true;
  });

  // Wait for completion or timeout
  const auto deadline = steady_clock::now() + timeoutMs;
  while (!completed && steady_clock::now() < deadline) {
    std::this_thread::sleep_for(milliseconds(10));
  }

  if (!completed) {
    // Timeout - detach thread (it will complete eventually but we move on)
    readerThread.detach();
    return false;
  }

  readerThread.join();
  return success;
}

bool UCIEngine::waitForResponse(const std::string& expectedResponse, milliseconds timeoutMs) {
  const auto deadline = steady_clock::now() + timeoutMs;

  while (steady_clock::now() < deadline) {
    std::string line;
    const auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now());

    if (readLine(line, remaining)) {
      // Search for response within line to handle log prefixes
      if (line.find(expectedResponse) != std::string::npos) {
        return true;
      }
      // Keep reading until we find the expected response or timeout
    } else {
      break; // Timeout or error
    }
  }

  return false;
}

void UCIEngine::initializeUCI() {
  // Send "uci" command
  sendCommand("uci");

  // Read response lines until we get "uciok"
  const auto deadline = steady_clock::now() + milliseconds(5000);
  bool receivedUciOk = false;

  while (steady_clock::now() < deadline) {
    std::string line;
    const auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now());

    if (!readLine(line, remaining)) {
      break; // Timeout or error
    }

    // Parse "id name" for engine name
    if (line.rfind("id name ", 0) == 0) {
      engineName = line.substr(8); // Skip "id name "
    }

    // Check for uciok (search within line to handle log prefixes)
    if (line.find("uciok") != std::string::npos) {
      receivedUciOk = true;
      break;
    }
  }

  if (!receivedUciOk) {
    throw std::runtime_error("Engine did not respond with 'uciok'");
  }

  if (engineName.empty()) {
    engineName = "Unknown Engine";
  }

  // Send "isready" and wait for "readyok"
  if (!waitUntilReady()) {
    throw std::runtime_error("Engine did not respond to 'isready'");
  }
}

bool UCIEngine::waitUntilReady() {
  sendCommand("isready");
  return waitForResponse("readyok", milliseconds(5000));
}

void UCIEngine::parseInfoLine(const std::string& line, UCISearchResult& result) {
  std::istringstream iss(line);
  std::string token;

  // Skip "info" keyword
  iss >> token;

  // Parse key-value pairs
  while (iss >> token) {
    if (token == "depth") {
      int depth;
      if (iss >> depth) {
        result.depth = static_cast<Depth>(depth);
      }
    } else if (token == "nodes") {
      iss >> result.nodes;
    } else if (token == "time") {
      int timeMs;
      if (iss >> timeMs) {
        result.time = milliseconds(timeMs);
      }
    } else if (token == "score") {
      std::string scoreType;
      if (iss >> scoreType) {
        if (scoreType == "cp") {
          int centipawns;
          if (iss >> centipawns) {
            result.score = static_cast<Value>(centipawns);
          }
        } else if (scoreType == "mate") {
          int mateIn;
          if (iss >> mateIn) {
            // Convert mate score to value (positive = winning, negative = losing)
            result.score = mateIn > 0 ? VALUE_CHECKMATE - mateIn : -VALUE_CHECKMATE + mateIn;
          }
        }
      }
    }
  }
}

} // namespace arena
