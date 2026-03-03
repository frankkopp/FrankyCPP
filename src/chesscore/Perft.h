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

#ifndef FRANKYCPP_PERFT_H
#define FRANKYCPP_PERFT_H

//=============================================================================
// Perft.h - Performance Test / Move Generation Verification
//=============================================================================
//
// Perft (Performance Test) counts the number of leaf nodes at a given depth
// to verify move generation correctness and measure performance.
// Depends on: types.h, Position, MoveGenerator
//
// Purpose:
//   - Verify move generator produces correct number of legal moves
//   - Benchmark move generation and position manipulation speed
//   - Debug move generation by comparing against known correct node counts
//
// Statistics Collected (when fullStats enabled):
//   - Total nodes (leaf positions)
//   - Captures, en passant captures, castling moves, promotions
//   - Checks, checkmates, stalemates
//
// Modes:
//   - Standard perft: generates all moves at once per ply
//   - On-demand perft: uses phased move generation (getNextPseudoLegalMove)
//   - Divide: shows node count per root move for debugging
//
// Usage:
//   Perft perft("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
//   perft.perft(6);  // Count nodes at depth 6
//   std::cout << "Nodes: " << perft.getNodes() << std::endl;
//
//   perft.perft_divide(5, false);  // Show per-move breakdown
//
//=============================================================================

#include "types/types.h"
#include <string>

namespace chess {

  // forward declared dependencies
  class Position;
  class MoveGenerator;

  class Perft {

    uint64_t nodes{};
    uint64_t checkCounter{};
    uint64_t checkMateCounter{};
    uint64_t staleMateCounter{};
    uint64_t captureCounter{};
    uint64_t enpassantCounter{};
    uint64_t castleCounter{};
    uint64_t promotionCounter{};
    std::string fen;
    bool stopFlag{};
    bool fullStats{true};

  public:
    /// Creates a Perft instance with the standard starting position.
    Perft();

    /// Creates a Perft instance with the given FEN position.
    /// @param f  FEN string describing the position to test
    explicit Perft(const std::string& f);

    /// Runs perft to the specified depth using standard move generation.
    /// @param maxDepth  Maximum depth to search (1 = count root moves)
    void perft(int maxDepth);

    /// Runs perft to the specified depth.
    /// @param maxDepth  Maximum depth to search
    /// @param onDemand  If true, uses phased move generation; if false, generates all moves at once
    void perft(int maxDepth, bool onDemand);

    /// Counts statistics for a leaf node (captures, checks, etc.).
    /// Called when we reach a leaf position to update counters.
    /// @param position  The leaf position
    /// @param move      The move that led to this position
    void leaf_node(const Position& position, Move move);

    /// Runs perft for a range of depths.
    /// Useful for running multiple depths in sequence and comparing results.
    /// @param startDepth  Starting depth (inclusive)
    /// @param endDepth    Ending depth (inclusive)
    /// @param onDemand    If true, uses phased move generation
    void perft(int startDepth, int endDepth, bool onDemand);

    /// Runs perft for a range of depths using the specified FEN position.
    /// Useful for running multiple depths in sequence and comparing results.
    /// @param fenString   FEN string describing the position to test
    /// @param startDepth  Starting depth (inclusive)
    /// @param endDepth    Ending depth (inclusive)
    /// @param onDemand    If true, uses phased move generation
    void perft(const std::string& fenString, int startDepth, int endDepth, bool onDemand);

    /// Runs perft with divide output, showing node count for each root move.
    /// Useful for debugging when perft count is wrong - compare per-move counts
    /// against a known-correct engine to find which move causes the discrepancy.
    /// @param maxDepth  Maximum depth to search
    /// @param onDemand  If true, uses phased move generation
    void perft_divide(int maxDepth, bool onDemand);

    /// Signals the perft to stop. Useful for interrupting long-running tests.
    void stop();

    /// Returns total leaf node count from last perft run.
    uint64_t getNodes() const { return nodes; }

    /// Returns number of capturing moves counted.
    uint64_t getCaptureCounter() const { return captureCounter; }

    /// Returns number of en passant captures counted.
    uint64_t getEnpassantCounter() const { return enpassantCounter; }

    /// Returns number of positions where side to move is in check.
    uint64_t getCheckCounter() const { return checkCounter; }

    /// Returns number of checkmate positions found.
    uint64_t getCheckMateCounter() const { return checkMateCounter; }

    /// Returns number of stalemate positions found.
    uint64_t getStaleMateCounter() const { return staleMateCounter; }

    /// Returns number of castling moves counted.
    uint64_t getCastleCounter() const { return castleCounter; }

    /// Returns number of promotion moves counted.
    uint64_t getPromotionCounter() const { return promotionCounter; }

    /// Returns whether full statistics collection is enabled.
    bool isFullStats() const { return fullStats; }

    /// Enables or disables full statistics collection.
    /// Disabling may improve performance slightly.
    /// @param fs  True to enable, false to disable
    void setFullStats(const bool fs) { fullStats = fs; }

  private:
    /// Resets all counters to zero before a new perft run.
    void resetCounter();

    /// Recursive minimax-style node counting using standard move generation.
    /// Generates all moves at once per ply.
    /// @param depth     Remaining depth
    /// @param position  Current position
    /// @param pMg       Move generator to use
    /// @return          Number of leaf nodes found
    uint64_t miniMax(int depth, Position& position, MoveGenerator* pMg);

    /// Recursive minimax-style node counting using on-demand move generation.
    /// Uses phased generation (getNextPseudoLegalMove) for testing that code path.
    /// @param depth     Remaining depth
    /// @param position  Current position
    /// @param pMg       Move generator to use
    /// @return          Number of leaf nodes found
    uint64_t miniMaxOD(int depth, Position& position, MoveGenerator* pMg);
  };

}// namespace chess

#endif// FRANKYCPP_PERFT_H
