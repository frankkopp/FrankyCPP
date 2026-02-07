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

#ifndef FRANKYCPP_ENGINE_ARENA_UCIENGINE_H
#define FRANKYCPP_ENGINE_ARENA_UCIENGINE_H

//=============================================================================
// UCIEngine.h - External UCI Chess Engine Interface
//=============================================================================
//
// UCIEngine provides a wrapper for communicating with external UCI chess engines
// via subprocess. It handles engine initialization, position setup, and search
// execution for test suite evaluation.
//
// Implementation uses Boost.Asio async_pipe for reliable, cancellable I/O on
// Windows and Linux. This avoids the common problem of blocking reads that
// cannot be interrupted on process shutdown.
//
// Lifecycle:
//   - One UCIEngine instance is reused across multiple positions in a test suite
//   - Constructor starts engine process once
//   - newGame() clears state (TT, history) between positions for isolation
//   - Destructor sends quit and closes process once at end
//
// Usage (Test Suite Pattern):
//   UCIEngine engine("path/to/engine.exe");
//   for (auto& test : testPositions) {
//     engine.newGame();           // Clear state for position isolation
//     engine.setPosition(test.fen);
//     UCISearchResult result = engine.search(milliseconds{5000}, 30);
//     // ... evaluate result
//   }
//   // Destructor called once at end
//
// Protocol:
//   - Constructor: Sends "uci", waits for "uciok", captures "id name"
//   - newGame(): Sends "ucinewgame" to clear engine state (TT, history, etc.)
//   - setPosition(): Sends "position fen <fen>" to set board state
//   - search(): Sends "go movetime <ms> depth <depth>", parses "info" and "bestmove"
//   - Destructor: Sends "quit", closes process
//   - Uses "isready"/"readyok" for synchronization before operations
//
// Error Handling:
//   - Constructor throws if engine not found or won't start
//   - search() returns empty bestMove on timeout or error
//   - Logs all errors and warnings to console
//
// Thread Safety:
//   - Not thread-safe - use separate instances per thread
//
//=============================================================================

#include "types/types.h"

#include <boost/asio.hpp>
#include <boost/process/v1/async_pipe.hpp>
#include <boost/process/v1/child.hpp>
#include <boost/process/v1/io.hpp>
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace arena {

/// Result of a UCI engine search
struct UCISearchResult {
  std::string bestMove;       ///< UCI long algebraic (e.g., "e2e4"), empty on error
  uint64_t nodes = 0;         ///< Total nodes searched
  Depth depth = DEPTH_ZERO;   ///< Search depth reached
  Value score = VALUE_NONE;   ///< Centipawn score from engine's perspective
  milliseconds time{0};       ///< Time spent searching
};

/// External UCI chess engine interface
class UCIEngine {
  // Member fields
  std::string enginePath_;                                    ///< Path to engine executable
  std::string engineName_;                                    ///< Engine name from "id name"

  // Boost.Asio for async I/O
  boost::asio::io_context ioContext_;                         ///< Asio I/O context
  std::unique_ptr<boost::process::v1::async_pipe> pipeOut_;   ///< Async pipe from engine stdout
  std::unique_ptr<boost::process::v1::opstream> pipeIn_;      ///< Sync stream to engine stdin
  std::unique_ptr<boost::process::v1::child> childProcess_;   ///< Engine subprocess

  // Async reader state
  boost::asio::streambuf readBuffer_;                         ///< Buffer for async reads
  std::thread ioThread_;                                      ///< Thread running io_context
  std::queue<std::string> lineQueue_;                         ///< Queue of lines read from engine
  std::mutex queueMutex_;                                     ///< Protects lineQueue_
  std::condition_variable queueCV_;                           ///< Signals when line available
  std::atomic<bool> stopping_{false};                         ///< Signal to stop I/O

  // Configuration
  milliseconds initTimeout_{120000};                          ///< Default 2 minute timeout for initialization (book loading, etc.)
  milliseconds searchTimeout_{30000};                         ///< Default 30 second timeout for search operations
  bool debugMode_{false};                                     ///< Debug mode: print all UCI communication
  std::string pendingUciOptions_;                             ///< UCI options to send before first isready

public:
  // Constructors/Destructor
  /// Construct and initialize UCI engine
  /// @param enginePath Path to UCI engine executable
  /// @param commandLineArgs Command-line arguments to pass to engine (e.g., "--nobook -hash 128")
  /// @param debugMode Enable debug output (prints all UCI communication)
  /// @param uciOptions UCI options to send before initialization (e.g., "OwnBook=false; Hash=128")
  /// @throws std::runtime_error if engine not found or initialization fails
  explicit UCIEngine(const std::string& enginePath, const std::string& commandLineArgs = "",
                     bool debugMode = false, const std::string& uciOptions = "");

  /// Destructor - stops engine
  ~UCIEngine();

  // Non-copyable, non-movable (owns threads and I/O resources)
  UCIEngine(const UCIEngine&) = delete;
  UCIEngine& operator=(const UCIEngine&) = delete;
  UCIEngine(UCIEngine&&) = delete;
  UCIEngine& operator=(UCIEngine&&) = delete;

  // Core functionality
  /// Send ucinewgame to clear engine state (TT, history, etc.)
  /// Call this between positions in a test suite to ensure clean state.
  /// This is essential for fair, comparable test results.
  void newGame();

  /// Set position for next search
  /// @param fen FEN string representing position
  /// @return True if successful, false on error
  bool setPosition(const std::string& fen);

  /// Search current position
  /// @param timeMs Time limit in milliseconds
  /// @param maxDepth Maximum search depth
  /// @return Search result with best move (empty on error)
  UCISearchResult search(milliseconds timeMs, Depth maxDepth);

  /// Set timeout for initialization operations (uci, isready after start)
  /// @param timeout Maximum time to wait for engine to initialize (default: 120s for book loading)
  void setInitTimeout(const milliseconds timeout) { initTimeout_ = timeout; }

  /// Set absolute timeout for search operations
  /// @param timeout Maximum time to wait for engine response
  void setSearchTimeout(const milliseconds timeout) { searchTimeout_ = timeout; }

  /// Enable/disable debug mode (prints all UCI communication)
  /// @param debug True to enable debug output, false to disable
  void setDebugMode(const bool debug) { debugMode_ = debug; }

  /// Send UCI option to engine
  /// @param name Option name
  /// @param value Option value
  void setOption(const std::string& name, const std::string& value);

  /// Set multiple UCI options from string
  /// Format: "Hash=256; Threads=4" or "Hash=256 Threads=4"
  /// @param options Semicolon or space-separated "name=value" pairs
  void setUciOptions(const std::string& options);

  /// Get option values from engine by re-sending "uci" and parsing option lines.
  /// - For FrankyCPP: Returns actual current values via non-standard "current" field
  /// - For other engines: Returns initial defaults only ("default" field is a fallback
  ///   that does NOT reflect values changed via setoption - UCI has no standard query mechanism)
  /// @return Map of option names to values (current for FrankyCPP, defaults for others)
  std::map<std::string, std::string> getOptions();

private:
  // Private helper methods
  /// Send command to engine
  void sendCommand(const std::string& command) const;

  /// Read line from engine (with timeout)
  /// @param line Output buffer for line
  /// @param timeoutMs Maximum time to wait
  /// @return True if line read successfully, false on timeout/error
  bool readLine(std::string& line, milliseconds timeoutMs);

  /// Wait for specific response from engine
  /// @param expectedResponse Response to wait for
  /// @param timeoutMs Maximum time to wait
  /// @return True if response received, false on timeout
  bool waitForResponse(const std::string& expectedResponse, milliseconds timeoutMs);

  /// Initialize UCI protocol
  void initializeUCI();

  /// Send pending UCI options (called during initialization, before isready)
  void sendPendingOptions() const;

  /// Send "isready" and wait for "readyok"
  /// @return True if engine ready, false on timeout
  bool waitUntilReady();

  /// Parse info line for search statistics
  static void parseInfoLine(const std::string& line, UCISearchResult& result);

  /// Start async reading from pipe
  void startAsyncRead();

  /// Handle completion of async read
  void handleRead(const boost::system::error_code& ec, std::size_t bytesTransferred);

public:
  // Getters
  /// Get engine name from "id name" response
  [[nodiscard]] const std::string& getEngineName() const { return engineName_; }
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_UCIENGINE_H
