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

#include <boost/process/v1/child.hpp>
#include <boost/process/v1/pipe.hpp>
#include <boost/process/v1/io.hpp>
#include <string>
#include <memory>
#include <map>

namespace arena {

/// Result of a UCI engine search
struct UCISearchResult {
  std::string bestMove;       ///< UCI long algebraic (e.g., "e2e4"), empty on error
  uint64_t nodes = 0;         ///< Total nodes searched
  Depth depth = DEPTH_ZERO;   ///< Search depth reached
  Value score = VALUE_NONE;   ///< Centipawn score from engine's perspective
  milliseconds time{0};   ///< Time spent searching
};

/// External UCI chess engine interface
class UCIEngine {
  // Member fields
  std::string enginePath;                                     ///< Path to engine executable
  std::string engineName;                                     ///< Engine name from "id name"
  std::unique_ptr<boost::process::v1::child> childProcess;    ///< Engine subprocess
  std::unique_ptr<boost::process::v1::opstream> pipeIn;       ///< Input stream to engine
  std::unique_ptr<boost::process::v1::ipstream> pipeOut;      ///< Output stream from engine
  milliseconds searchTimeout{30000};                      ///< Default 30 second timeout
  bool debugMode{false};                                      ///< Debug mode: print all UCI communication

public:
  // Constructors/Destructor
  /// Construct and initialize UCI engine
  /// @param enginePath Path to UCI engine executable
  /// @param commandLineArgs Command-line arguments to pass to engine (e.g., "--nobook -hash 128")
  /// @throws std::runtime_error if engine not found or initialization fails
  explicit UCIEngine(const std::string& enginePath, const std::string& commandLineArgs = "");

  /// Destructor - stops engine
  ~UCIEngine();

  // Non-copyable
  UCIEngine(const UCIEngine&) = delete;
  UCIEngine& operator=(const UCIEngine&) = delete;

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

  /// Set absolute timeout for search operations
  /// @param timeout Maximum time to wait for engine response
  void setSearchTimeout(milliseconds timeout) { searchTimeout = timeout; }

  /// Enable/disable debug mode (prints all UCI communication)
  /// @param debug True to enable debug output, false to disable
  void setDebugMode(bool debug) { debugMode = debug; }

  /// Send UCI option to engine
  /// @param name Option name
  /// @param value Option value
  void setOption(const std::string& name, const std::string& value);

  /// Set multiple UCI options from string
  /// Format: "Hash=256; Threads=4" or "Hash=256 Threads=4"
  /// @param options Semicolon or space-separated "name=value" pairs
  ///
  /// Note: To verify options were applied, you can send "uci" command again
  /// and parse the "option" lines in the response. The engine will report
  /// current values in the format: "option name <name> type <type> default <value> ..."
  void setUciOptions(const std::string& options);

  /// Get current option values from engine
  /// Sends "getoptions" command (FrankyCPP extension) and parses response.
  ///
  /// FrankyCPP Extension: Uses "getoptions" command which reports current values
  /// Format: option name <name> type <type> current <value>
  ///
  /// Note: This only works with FrankyCPP. For standard UCI engines, you would
  ///       need to send "uci" and parse "default" values, which may not reflect
  ///       changes made via setoption commands.
  ///
  /// @return Map of option names to current values
  std::map<std::string, std::string> getOptions();

private:
  // Private helper methods
  /// Send command to engine
  void sendCommand(const std::string& command);

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

  /// Send "isready" and wait for "readyok"
  /// @return True if engine ready, false on timeout
  bool waitUntilReady();

  /// Parse info line for search statistics
  void parseInfoLine(const std::string& line, UCISearchResult& result);

public:
  // Getters
  /// Get engine name from "id name" response
  [[nodiscard]] const std::string& getEngineName() const { return engineName; }
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_UCIENGINE_H
