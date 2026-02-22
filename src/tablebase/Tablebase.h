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

#ifndef FRANKYCPP_TABLEBASE_H
#define FRANKYCPP_TABLEBASE_H

//=============================================================================
// Tablebase.h - Syzygy Endgame Tablebase Probing
//=============================================================================
//
// Provides interface to Syzygy endgame tablebases via the Fathom library.
// Depends on: Position.h, types.h, Fathom (external C library)
//
// Tablebase Types:
//   - WDL (Win/Draw/Loss): Fast probe for search decisions
//   - DTZ (Distance To Zeroing): For optimal root move selection
//
// WDL Results:
//   - Win: Position is won with perfect play
//   - CursedWin: Would be won but may draw due to 50-move rule
//   - Draw: Position is drawn with perfect play
//   - BlessedLoss: Would be lost but may draw due to 50-move rule
//   - Loss: Position is lost with perfect play
//
// Limitations:
//   - Only positions without castling rights
//   - Piece count must be within available tablebase range (typically 6-7)
//   - Requires tablebase files on disk (~150GB for 7-piece)
//
// Threading:
//   - Fathom library is thread-safe when built with TB_NO_THREADS=0
//   - Concurrent probing from multiple search threads is supported
//
// Key Methods:
//   initialize(path)   - Load tablebases from directory
//   probeWDL(pos)      - Fast WDL probe for search
//   probeRoot(pos)     - Full probe with DTZ and best move
//   canProbe(pos)      - Check if position is probeable
//
// Usage:
//   Tablebase tb;
//   tb.initialize("C:/syzygy/345;C:/syzygy/6");
//   if (tb.canProbe(pos)) {
//     TBResult result = tb.probeWDL(pos);
//     if (result == TBResult::Win) { ... }
//   }
//
//=============================================================================

#include "types/types.h"
#include "chesscore/Position.h"
#include <string>

namespace tablebase {

/// WDL (Win/Draw/Loss) result from tablebase probe.
enum class TBResult : int8_t {
  Loss        = -2,  ///< Position is lost with perfect play
  BlessedLoss = -1,  ///< Would be lost but may draw due to 50-move rule
  Draw        = 0,   ///< Position is drawn with perfect play
  CursedWin   = 1,   ///< Would be won but may draw due to 50-move rule
  Win         = 2,   ///< Position is won with perfect play
  Failed      = 3    ///< Probe failed (position not in TB or not probeable)
};

/// Full tablebase probe result with WDL, DTZ (distance to zeroing), and best move.
struct TBProbeResult {
  TBResult wdl{TBResult::Failed};  ///< Win/Draw/Loss result
  int dtz{0};                      ///< Distance to zeroing move (capture or pawn move)
  Move bestMove{MOVE_NONE};        ///< Best move from tablebase (MOVE_NONE if unavailable)

  /// Returns true if the probe succeeded.
  [[nodiscard]] bool success() const { return wdl != TBResult::Failed; }
  /// Returns true if the position is winning (Win or CursedWin).
  [[nodiscard]] bool isWin() const { return wdl == TBResult::Win || wdl == TBResult::CursedWin; }
  /// Returns true if the position is drawn.
  [[nodiscard]] bool isDraw() const { return wdl == TBResult::Draw; }
  /// Returns true if the position is losing (Loss or BlessedLoss).
  [[nodiscard]] bool isLoss() const { return wdl == TBResult::Loss || wdl == TBResult::BlessedLoss; }
};

/// Syzygy tablebase probing interface using the Fathom library.
/// Thread safety:
///   - probeWDL(): Thread-safe for concurrent probing from multiple search threads.
///   - probeRoot(): NOT thread-safe. Call only once at root per search.
class Tablebase {
  bool initialized_{false};
  int maxPieces_{0};        ///< Maximum pieces available in loaded tablebases (e.g., 6 or 7)
  std::string tbPath_;      ///< Path(s) used to initialize tablebases

public:
  Tablebase() = default;
  ~Tablebase();

  // Disallow copies (manages global Fathom state)
  Tablebase(const Tablebase&) = delete;
  Tablebase& operator=(const Tablebase&) = delete;

  /// Initialize tablebases from path(s).
  /// @param path  Semicolon-separated list of directories (Windows: semicolon, Linux: colon)
  /// @return true if at least one tablebase file was found
  bool initialize(const std::string& path);

  /// Shut down and release tablebase resources.
  void shutdown();

  /// Returns true if tablebases are available for probing.
  [[nodiscard]] bool isAvailable() const { return initialized_ && maxPieces_ > 0; }

  /// Returns the maximum number of pieces supported by loaded tablebases.
  [[nodiscard]] int maxPieces() const { return maxPieces_; }

  /// Returns the path(s) used to initialize tablebases.
  [[nodiscard]] const std::string& getPath() const { return tbPath_; }

  /// Probe WDL only (faster, suitable for search nodes).
  /// Returns the "pure" theoretical WDL result without 50-move rule considerations.
  /// For positions near the 50-move limit, use probeRoot, which respects halfmove clock.
  /// @param pos  Position to probe
  /// @return WDL result or Failed if probe unsuccessful
  [[nodiscard]] TBResult probeWDL(const Position& pos) const;

  /// Probe WDL and DTZ with best move (slower, suitable for root).
  /// Unlike probeWDL, this function uses the position's halfmove clock for
  /// accurate cursed win / blessed loss detection near the 50-move limit.
  /// @param pos  Position to probe
  /// @return Full probe result including DTZ and best move
  [[nodiscard]] TBProbeResult probeRoot(const Position& pos) const;

  /// Check if position can be probed (piece count within limit, no castling rights).
  /// @param pos  Position to check
  /// @return true if position is probeable
  [[nodiscard]] bool canProbe(const Position& pos) const;

  /// Pre-warm OS file cache by probing representative endgame positions.
  /// Call once at startup after tablebase initialization to reduce latency
  /// on first in-game probes. Forces OS to load TB files into page cache.
  /// @param maxPieces Maximum piece count to warm (3 to maxPieces_)
  void prewarmCache(int maxPieces = 5) const;

  /// Convert TBResult and DTZ to centipawn value for search scoring.
  /// Uses DTZ to prefer shorter wins (smaller DTZ = higher score).
  /// @param result  WDL result
  /// @param dtz     Distance to zeroing move (from probeRoot)
  /// @return Value in centipawns
  [[nodiscard]] static Value tbResultToScore(TBResult result, int dtz);

  /// Convert TBResult to centipawn value (legacy, no DTZ).
  /// @param result  WDL result
  /// @param ply     Current ply (for mate-distance-like scoring)
  /// @return Value in centipawns
  [[nodiscard]] static Value tbValueToScore(TBResult result, Depth ply);

  /// Get human-readable string representation of result.
  /// @param result  WDL result
  /// @return String like "Win", "Draw", "Loss", etc.
  [[nodiscard]] static std::string resultToString(TBResult result);
};

} // namespace tablebase

#endif // FRANKYCPP_TABLEBASE_H
