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

#ifndef FRANKYCPP_ENGINE_ARENA_ARENACONFIG_H
#define FRANKYCPP_ENGINE_ARENA_ARENACONFIG_H

#include "types/types.h"
#include "types/timeunits.h"

#include <string>
#include <vector>

namespace arena {

/// Configuration for a single EPD test suite
struct TestSuiteConfig {
  std::string name;           ///< Test suite name (e.g., "WAC", "STS")
  std::string epdPath;        ///< Path to EPD file
  milliseconds timePerMove;   ///< Time limit per move
  Depth maxDepth;            ///< Maximum search depth
};

/// Configuration for a single engine match
struct MatchConfig {
  std::string name;          ///< Match name (e.g., "v1.1 vs v1.0")
  std::string engine1Path;   ///< Path to first engine executable
  std::string engine2Path;   ///< Path to second engine executable
  std::string cutechessPath; ///< Path to cutechess-cli executable
  std::string openingBook;   ///< Path to opening book (PGN format)
  std::string timeControl;   ///< Time control (e.g., "10+0.1")
  int rounds;                ///< Number of rounds to play
  std::string outputPgn;     ///< Path to save PGN games
};

/// Main arena configuration
struct ArenaConfig {
  std::string version;       ///< Engine version being tested
  std::string resultsDir;    ///< Root directory for results
  std::vector<TestSuiteConfig> testSuites; ///< Test suite configurations
  std::vector<MatchConfig> matches;        ///< Match configurations

  /// Load configuration from YAML file
  /// @param configPath Path to arena.yaml configuration file
  /// @throws std::runtime_error if file not found or invalid YAML
  static ArenaConfig loadFromYaml(const std::string& configPath);

  /// Validate configuration (check paths exist, values are sensible)
  /// @return True if valid, false otherwise
  bool validate() const;
};

} // namespace arena

#endif // FRANKYCPP_ENGINE_ARENA_ARENACONFIG_H
