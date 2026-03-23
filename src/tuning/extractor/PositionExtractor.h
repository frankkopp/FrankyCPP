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

#ifndef FRANKYCPP_POSITIONEXTRACTOR_H
#define FRANKYCPP_POSITIONEXTRACTOR_H

//=============================================================================
// PositionExtractor.h - PGN to Labeled Position Extractor
//=============================================================================
//
// Extracts labeled positions (FEN + game result) from PGN files for use in
// Texel tuning. Replays each game move-by-move and applies configurable
// filters to select only quiet, representative positions.
//
// Filters Applied (in order):
//   0. Game-level: skip unknown results, time forfeits, illegal moves, etc.
//   1. Early move: skip positions before a configurable half-move threshold
//   2. In check: skip positions where the side to move is in check
//   3. Capture/promotion: skip positions right after captures or promotions
//   4. Trivial endgame: skip positions with fewer than N pieces
//   5. Qsearch stability (optional): skip positions where the qsearch score
//      diverges from the static eval by more than a threshold
//
// Output Format:
//   One line per position: <FEN> [<result>]
//   Result is from White's perspective: 1.0 (win), 0.5 (draw), 0.0 (loss)
//
// Usage:
//   ExtractionConfig config;
//   PositionExtractor extractor;
//   extractor.extract("games.pgn", "positions.txt", config);
//   extractor.getStats().printSummary();
//
// Thread Safety:
//   PositionExtractor is NOT thread-safe. Use separate instances per thread.
//
//=============================================================================

#include "common/pgn/PgnGame.h"
#include "engine/Evaluator.h"
#include "types/types.h"

#include <optional>
#include <string>

namespace tuning {

  /// Configuration for position extraction filters.
  struct ExtractionConfig {
    int minHalfMove       = 16;    ///< Filter 1: skip first N half-moves (default: 8 full moves)
    int minPieces         = 6;     ///< Filter 4: skip positions with fewer than N pieces
    bool skipCaptures     = true;  ///< Filter 3: skip positions after captures
    bool skipPromotions   = true;  ///< Filter 3: skip positions after promotions
    bool skipTermination  = true;  ///< Filter 0: skip games with [Termination] header
    bool qsearchFilter    = false; ///< Filter 5: enable qsearch stability filter
    int qsearchThreshold  = 150;   ///< Filter 5: threshold in centipawns
    int qsearchMaxDepth   = 6;     ///< Filter 5: max depth for standalone qsearch
    bool scoreFilter      = false; ///< Filter 6: skip positions where search score contradicts result
    int scoreThreshold    = 200;   ///< Filter 6: threshold in centipawns for contradiction
  };

  /// Statistics collected during position extraction.
  /// All counters are cumulative across the entire extraction run.
  struct ExtractionStats {
    // Game-level counters
    int gamesTotal                = 0; ///< Total games seen in PGN file
    int gamesProcessed            = 0; ///< Games that passed game-level filters
    int gamesSkippedUnknownResult = 0; ///< Result is "*" (incomplete/unknown)
    int gamesSkippedTermination   = 0; ///< Has [Termination] header (time forfeit, illegal, etc.)
    int gamesSkippedReplayError   = 0; ///< SAN parse error during game replay
    int gamesWhiteWins            = 0; ///< Processed games won by White
    int gamesBlackWins            = 0; ///< Processed games won by Black
    int gamesDraws                = 0; ///< Processed games drawn

    // Position-level counters
    int totalPositionsSeen  = 0; ///< All positions across all processed games
    int filteredEarlyMove   = 0; ///< Filter 1: opening theory
    int filteredInCheck     = 0; ///< Filter 2: side to move in check
    int filteredCapture     = 0; ///< Filter 3: after capture or promotion
    int filteredEndgame     = 0; ///< Filter 4: trivial endgame (few pieces)
    int filteredQsearch              = 0; ///< Filter 5: qsearch instability
    bool qsearchEnabled             = false; ///< Whether qsearch filter was active
    int filteredScoreContradiction   = 0; ///< Filter 6: search score contradicts game result
    bool scoreFilterEnabled          = false; ///< Whether score contradiction filter was active
    int positionsExtracted           = 0; ///< Final output count

    /// Prints a formatted extraction summary to stdout and the tuning logger.
    /// Shows game-level and position-level statistics with percentages.
    /// @param inputPath   Input PGN file path (for display)
    /// @param outputPath  Output file path (for display)
    /// @param elapsedMs   Elapsed time in milliseconds
    void printSummary(const std::string& inputPath,
                      const std::string& outputPath,
                      int64_t elapsedMs) const;
  };

  /// Extracts labeled positions from PGN files for Texel tuning.
  class PositionExtractor {

    ExtractionStats stats{};
    engine::Evaluator evaluator{};

  public:
    PositionExtractor();

    /// Extracts positions from a PGN file and writes them to an output file.
    /// Each output line has the format: <FEN> [<result>]
    /// @param inputPgn   Path to the input PGN file
    /// @param outputFile Path to the output text file
    /// @param config     Extraction configuration (filters, thresholds)
    void extract(const std::string& inputPgn,
                 const std::string& outputFile,
                 const ExtractionConfig& config);

    /// Returns the extraction statistics from the last run.
    /// @return Const reference to stats
    [[nodiscard]] const ExtractionStats& getStats() const { return stats; }

  private:
    /// Processes a single PGN game: replays moves, applies filters, writes positions.
    /// @param game       Parsed PGN game
    /// @param config     Extraction configuration
    /// @param outStream  Output stream to write FEN+result lines
    void processGame(const common::pgn::PgnGame& game,
                     const ExtractionConfig& config,
                     std::ostream& outStream);

    /// Standalone quiescence search for position stability assessment.
    /// A simplified capture-only alpha-beta search without TT or threading.
    /// Used to detect tactically unstable positions where the static eval
    /// differs significantly from the resolved score.
    /// @param position  Position to search (modified via doMove/undoMove, restored on return)
    /// @param alpha     Alpha bound
    /// @param beta      Beta bound
    /// @param depth     Remaining depth (counts down to 0)
    /// @return          Quiescence score from side-to-move perspective
    chess::Value standaloneQsearch(chess::Position& position,
                                  chess::Value alpha,
                                  chess::Value beta,
                                  int depth);

  public:
    /// Parses a search score from a cutechess-cli PGN comment.
    /// Handles formats: "{+1.32/11 6.9s}", "{-M15/15 14s}", "{book}", "{0.00/19 0.008s}".
    /// The score is from the perspective of the side that played the move.
    /// @param comment  Raw comment text (content between { and }, without braces)
    /// @return Centipawn score, or std::nullopt if no valid score (book move, empty, etc.)
    [[nodiscard]] static std::optional<int> parseSearchScore(const std::string& comment);
  };

} // namespace tuning

#endif // FRANKYCPP_POSITIONEXTRACTOR_H
