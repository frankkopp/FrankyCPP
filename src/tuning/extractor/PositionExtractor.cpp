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

#include "PositionExtractor.h"

#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"
#include "common/Logging.h"
#include "common/pgn/PgnParser.h"
#include "common/pgn/PgnTypes.h"

#include <cmath>
#include <format>
#include <fstream>
#include <iostream>

using namespace tuning;
using namespace chess;
using namespace common::pgn;
using namespace engine;

namespace {
  /// Formats a percentage safely (0.0 if denominator is zero).
  [[nodiscard]] std::string pct(const int numerator, const int denominator) {
    if (denominator == 0) return "  0.00%";
    return std::format("{:7.2f}%", 100.0 * numerator / denominator);
  }

  /// Formats a count with width and percentage.
  // ReSharper disable once CppDFAConstantParameter
  [[nodiscard]] std::string countAndPct(const int count, const int total, const int width = 12) {
    return std::format("{:>{}} ({})", count, width, pct(count, total));
  }
} // anonymous namespace

// ============================================================================
// ExtractionStats
// ============================================================================

void ExtractionStats::printSummary(const std::string& inputPath,
                                   const std::string& outputPath,
                                   const int64_t elapsedMs) const {
  const int totalFiltered = filteredEarlyMove + filteredInCheck + filteredCapture
                            + filteredEndgame + filteredQsearch + filteredScoreContradiction;
  const double elapsedSec = static_cast<double>(elapsedMs) / 1000.0;
  const double gamesPerSec = elapsedSec > 0 ? gamesProcessed / elapsedSec : 0;
  const double posPerSec = elapsedSec > 0 ? positionsExtracted / elapsedSec : 0;
  const double avgPosPerGame = gamesProcessed > 0
                                 ? static_cast<double>(positionsExtracted) / gamesProcessed
                                 : 0;

  // Build the summary report
  std::string report;
  report += "\n=== Extraction Summary ===\n";
  report += std::format("Input:     {}\n", inputPath);
  report += std::format("Output:    {}\n", outputPath);
  report += "\nGames:\n";
  report += std::format("  Total in PGN:         {}\n", gamesTotal);
  report += std::format("  Skipped (unknown):    {}\n", countAndPct(gamesSkippedUnknownResult, gamesTotal));
  report += std::format("  Skipped (terminated): {}\n", countAndPct(gamesSkippedTermination, gamesTotal));
  report += std::format("  Skipped (replay err): {}\n", countAndPct(gamesSkippedReplayError, gamesTotal));
  report += std::format("  Processed:            {}\n", countAndPct(gamesProcessed, gamesTotal));
  report += std::format("    White wins:         {}\n", countAndPct(gamesWhiteWins, gamesProcessed));
  report += std::format("    Black wins:         {}\n", countAndPct(gamesBlackWins, gamesProcessed));
  report += std::format("    Draws:              {}\n", countAndPct(gamesDraws, gamesProcessed));
  report += "\nPositions:\n";
  report += std::format("  Total seen:           {}\n", totalPositionsSeen);
  report += std::format("  Filter 1 (early):     {}\n", countAndPct(filteredEarlyMove, totalPositionsSeen));
  report += std::format("  Filter 2 (in check):  {}\n", countAndPct(filteredInCheck, totalPositionsSeen));
  report += std::format("  Filter 3 (capture):   {}\n", countAndPct(filteredCapture, totalPositionsSeen));
  report += std::format("  Filter 4 (endgame):   {}\n", countAndPct(filteredEndgame, totalPositionsSeen));
  if (qsearchEnabled) {
    report += std::format("  Filter 5 (qsearch):   {}\n", countAndPct(filteredQsearch, totalPositionsSeen));
  }
  else {
    report += "  Filter 5 (qsearch):   disabled\n";
  }
  if (scoreFilterEnabled) {
    report += std::format("  Filter 6 (score):     {}\n", countAndPct(filteredScoreContradiction, totalPositionsSeen));
  }
  else {
    report += "  Filter 6 (score):     disabled\n";
  }
  report += std::format("  -------------------------------------------\n");
  report += std::format("  Total filtered:       {}\n", countAndPct(totalFiltered, totalPositionsSeen));
  report += std::format("  Extracted:            {}\n", countAndPct(positionsExtracted, totalPositionsSeen));
  report += std::format("\nExtraction rate: {:.1f} positions per game\n", avgPosPerGame);
  report += std::format("Elapsed time: {:.1f}s ({:.0f} games/s, {:.0f} positions/s)\n",
                        elapsedSec, gamesPerSec, posPerSec);

  std::cout << report;
  LOG__INFO(common::Logger::get().TUNING_LOG, "{}", report);
}

// ============================================================================
// PositionExtractor
// ============================================================================

PositionExtractor::PositionExtractor() {
  // No PawnTT for the extractor — pawn cache disabled during extraction.
  evaluator.setPawnTT(nullptr);
}

void PositionExtractor::extract(const std::string& inputPgn,
                                const std::string& outputFile,
                                const ExtractionConfig& config) {
  // Reset stats for this extraction run
  stats = ExtractionStats{};
  stats.qsearchEnabled = config.qsearchFilter;
  stats.scoreFilterEnabled = config.scoreFilter;

  // Open output file
  std::ofstream outStream(outputFile);
  if (!outStream.is_open()) {
    const std::string msg = std::format("Failed to open output file: {}", outputFile);
    LOG__ERROR(common::Logger::get().TUNING_LOG, "{}", msg);
    throw std::runtime_error(msg);
  }

  // Parse PGN file using streaming callback
  PgnParser parser;
  parser.parseFile(inputPgn, [this, &config, &outStream](PgnGame&& game) {
    stats.gamesTotal++;

    // ========================================================================
    // Filter 0: Game-level filters
    // ========================================================================

    // Skip games with unknown result
    if (!game.hasKnownResult()) {
      stats.gamesSkippedUnknownResult++;
      return;
    }

    // Skip games with any [Termination] header (time forfeit, illegal move, unterminated, etc.)
    if (config.skipTermination && game.headers.contains("Termination")) {
      stats.gamesSkippedTermination++;
      return;
    }

    // Track result distribution
    switch (game.result) {
      case GameResult::WHITE_WIN: stats.gamesWhiteWins++; break;
      case GameResult::BLACK_WIN: stats.gamesBlackWins++; break;
      case GameResult::DRAW:      stats.gamesDraws++;     break;
      default: break;
    }
    stats.gamesProcessed++;

    // Process the game: replay moves, apply position-level filters, write output
    processGame(game, config, outStream);
  });

  outStream.close();
}

void PositionExtractor::processGame(const PgnGame& game,
                                    const ExtractionConfig& config,
                                    std::ostream& outStream) {
  // Set up position: use FEN from header if present, otherwise standard start position
  Position position;
  if (game.hasCustomFen()) {
    position.setFromFen(game.getHeader("FEN"));
  }

  MoveGenerator mg;
  const double resultLabel = resultToDouble(game.result);
  int halfMoveCounter = 0;
  int moveIndex = 0;

  // Replay each move in the game
  for (const auto& sanMove : game.moves) {
    // Parse SAN move
    const Move move = mg.getMoveFromSan(position, sanMove);
    if (move == MOVE_NONE) {
      LOG__WARN(common::Logger::get().TUNING_LOG,
                "Invalid SAN move '{}' at half-move {} in game (White: {}, Black: {}). Skipping rest of game.",
                sanMove, halfMoveCounter,
                game.getHeader("White"), game.getHeader("Black"));
      stats.gamesSkippedReplayError++;
      stats.gamesProcessed--; // Undo the increment since we're abandoning this game
      // Also undo the result counter
      switch (game.result) {
        case GameResult::WHITE_WIN: stats.gamesWhiteWins--; break;
        case GameResult::BLACK_WIN: stats.gamesBlackWins--; break;
        case GameResult::DRAW:      stats.gamesDraws--;     break;
        default: break;
      }
      return;
    }

    // Execute the move
    position.doMove(move);
    halfMoveCounter++;
    const int currentMoveIndex = moveIndex++;
    stats.totalPositionsSeen++;

    // ========================================================================
    // Position-level filters (applied in order for efficiency)
    // ========================================================================

    // Filter 1: Skip early moves (opening theory)
    if (halfMoveCounter < config.minHalfMove) {
      stats.filteredEarlyMove++;
      continue;
    }

    // Filter 2: Skip positions where the side to move is in check
    if (position.hasCheck()) {
      stats.filteredInCheck++;
      continue;
    }

    // Filter 3: Skip positions after captures or promotions (not quiet)
    if (config.skipCaptures || config.skipPromotions) {
      const bool wasCapture = position.getLastCapturedPiece() != PIECE_NONE
                              || move.type() == ENPASSANT;
      const bool wasPromotion = move.type() == PROMOTION;
      if ((config.skipCaptures && wasCapture) || (config.skipPromotions && wasPromotion)) {
        stats.filteredCapture++;
        continue;
      }
    }

    // Filter 4: Skip trivial endgames (few pieces)
    if (position.getOccupiedBb().popcount() < config.minPieces) {
      stats.filteredEndgame++;
      continue;
    }

    // Filter 5: Qsearch stability (optional)
    if (config.qsearchFilter) {
      const Value staticEval = evaluator.evaluate(position);
      const Value qsearchScore = standaloneQsearch(
        position, -VALUE_CHECKMATE, VALUE_CHECKMATE, config.qsearchMaxDepth);
      const int diff = std::abs(static_cast<int>(qsearchScore) - static_cast<int>(staticEval));
      if (diff > config.qsearchThreshold) {
        stats.filteredQsearch++;
        continue;
      }
    }

    // Filter 6: Score contradiction (optional)
    // Skip positions where the engine's search score strongly contradicts the game result.
    // E.g., result is White win (1.0) but engine scored the position as strongly negative.
    // This removes noisy labels where the result was decided by a later blunder.
    if (config.scoreFilter
        && currentMoveIndex < static_cast<int>(game.moveComments.size())) {
      const auto scoreCp = parseSearchScore(game.moveComments[currentMoveIndex]);
      if (scoreCp.has_value()) {
        // Convert score to White's perspective.
        // After doMove(), getNextPlayer() is the side that will move NEXT.
        // If next is BLACK → White just moved → score is from White's perspective (as-is)
        // If next is WHITE → Black just moved → negate for White's perspective
        const int whiteScore = (position.getNextPlayer() == BLACK)
                                 ? scoreCp.value()
                                 : -scoreCp.value();
        const bool contradiction =
          (resultLabel == 1.0 && whiteScore < -config.scoreThreshold)  // White won but score says Black is winning
          || (resultLabel == 0.0 && whiteScore > config.scoreThreshold); // Black won but score says White is winning
        if (contradiction) {
          stats.filteredScoreContradiction++;
          continue;
        }
      }
    }

    // ========================================================================
    // Position passed all filters — write to output
    // ========================================================================
    outStream << position.strFen() << " [" << std::format("{:.1f}", resultLabel) << "]\n";
    stats.positionsExtracted++;
  }
}

// ============================================================================
// Standalone Qsearch
// ============================================================================

Value PositionExtractor::standaloneQsearch(Position& position,
                                           Value alpha,
                                           const Value beta,
                                           const int depth) {
  // Terminal: max depth reached — return static eval
  if (depth <= 0) {
    return evaluator.evaluate(position);
  }

  const bool inCheck = position.hasCheck();

  // If not in check, use standpat (static eval as lower bound)
  if (!inCheck) {
    const Value staticEval = evaluator.evaluate(position);
    if (staticEval >= beta) {
      return staticEval; // beta cutoff
    }
    if (staticEval > alpha) {
      alpha = staticEval;
    }
  }

  // Generate captures (or all moves if in check)
  MoveGenerator mg;
  const GenMode genMode = inCheck ? GenAll : GenNonQuiet;
  const auto* moves = mg.generateLegalMoves(position, genMode);

  // If in check and no legal moves: checkmate
  if (inCheck && moves->empty()) {
    return -VALUE_CHECKMATE + (6 - depth); // distance-adjusted mate score
  }

  Value bestValue = inCheck ? -VALUE_CHECKMATE : alpha;

  for (const auto move : *moves) {
    position.doMove(move);

    // Verify legality was already handled by generateLegalMoves
    const Value score = -standaloneQsearch(position, -beta, -alpha, depth - 1);

    position.undoMove();

    if (score > bestValue) {
      bestValue = score;
      if (score > alpha) {
        alpha = score;
        if (score >= beta) {
          break; // beta cutoff
        }
      }
    }
  }

  return bestValue;
}

// ============================================================================
// Score Parsing
// ============================================================================

std::optional<int> PositionExtractor::parseSearchScore(const std::string& comment) {
  // Cutechess-cli PGN comment formats:
  //   "book"                         → no score (book move)
  //   "+1.32/11 6.9s"               → +132 cp
  //   "-1.28/12 6.8s"               → -128 cp
  //   "0.00/19 0.008s"              → 0 cp (no sign prefix)
  //   "+M15/15 14s"                 → mate score (+10000)
  //   "-M16/20 7.8s"               → mate score (-10000)
  //   "+M1/127 0.009s, White mates" → mate with extra text
  //   "+98.71/22 27s"              → +9871 cp

  if (comment.empty()) return std::nullopt;

  // Skip leading whitespace
  size_t pos = 0;
  while (pos < comment.size() && comment[pos] == ' ') pos++;
  if (pos >= comment.size()) return std::nullopt;

  // "book" or other non-score annotations
  if (comment[pos] == 'b' || comment[pos] == 'B') return std::nullopt;
  // "White mates", "Black mates", etc.
  if (comment[pos] == 'W' || comment[pos] == 'w') return std::nullopt;

  // Parse sign
  int sign = 1;
  if (comment[pos] == '+') { sign = 1; pos++; }
  else if (comment[pos] == '-') { sign = -1; pos++; }

  if (pos >= comment.size()) return std::nullopt;

  // Check for mate score (M or m followed by digits)
  if (comment[pos] == 'M' || comment[pos] == 'm') {
    return sign * 10000; // Treat any mate as ±10000 cp
  }

  // Parse decimal score (in pawns) up to the '/' depth separator
  const auto slashPos = comment.find('/', pos);
  if (slashPos == std::string::npos || slashPos <= pos) return std::nullopt;

  // Parse integer and fractional parts separately (locale-independent)
  const std::string scoreStr = comment.substr(pos, slashPos - pos);
  const auto dotPos = scoreStr.find('.');

  try {
    if (dotPos == std::string::npos) {
      // No decimal point — just integer pawns
      const int wholePawns = std::stoi(scoreStr);
      return sign * wholePawns * 100;
    }

    // Parse whole and fractional parts separately
    const int wholePawns = (dotPos > 0) ? std::stoi(scoreStr.substr(0, dotPos)) : 0;
    const std::string fracStr = scoreStr.substr(dotPos + 1);

    // Convert fractional part to centipawns (handle 1 or 2 digits)
    int fracCp = 0;
    if (!fracStr.empty()) {
      fracCp = std::stoi(fracStr);
      if (fracStr.size() == 1) fracCp *= 10; // ".3" → 30 cp
      // If more than 2 digits, truncate (shouldn't happen in practice)
      if (fracStr.size() > 2) fracCp /= static_cast<int>(std::pow(10, fracStr.size() - 2));
    }

    return sign * (wholePawns * 100 + fracCp);
  }
  catch (...) {
    return std::nullopt; // Unparseable score
  }
}
