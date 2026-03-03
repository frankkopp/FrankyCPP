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

#ifndef FRANKYCPP_BENCHMARKPOSITIONS_H
#define FRANKYCPP_BENCHMARKPOSITIONS_H

//=============================================================================
// BenchmarkPositions.h - Curated positions for engine benchmarking
//=============================================================================
//
// Contains a fixed set of positions for the `bench` command to measure NPS.
// Positions are sourced from public domain test suites:
//   - WAC (Win at Chess) - tactical positions
//   - Kaufman CCR Test - balanced tactical/positional
//   - Eigenmann Rapid Engine Test - modern diverse positions
//   - Standard positions - starting position, known test positions
//
// Selection criteria:
//   - Mix of opening, middlegame, and endgame
//   - Variety of piece configurations
//   - Different branching factors (tactical vs quiet)
//   - Positions that take reasonable time at depth 13
//
//=============================================================================

#include <array>
#include <string_view>

namespace engine::benchmark {

  /// Curated benchmark positions from public domain test sets (50 positions)
  /// Used by the `bench` command for standardized NPS measurement
  constexpr std::array<std::string_view, 50> BENCH_FENS = {
    // =========================================================================
    // Standard positions (5)
    // =========================================================================
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",             // Starting position
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",// Kiwi (position 2)
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11",                           // Endgame
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",     // Complex tactics
    "8/8/4kpp1/3p1b2/p6P/2B5/6P1/6K1 w - - 0 1",                            // Bishop endgame

    // =========================================================================
    // WAC (Win at Chess) - tactical positions (20)
    // Source: https://www.chessprogramming.org/Win_at_Chess
    // =========================================================================
    "5rk1/1ppb3p/p1pb4/6q1/3P1p1r/2P1R2P/PP1BQ1P1/5RKN w - - 0 1",       // WAC.003
    "r1bq2rk/pp3pbp/2p1p1pQ/7P/3P4/2PB1N2/PP3PPR/2KR4 w - - 0 1",        // WAC.004
    "5k2/6pp/p1qN4/1p1p4/3P4/2PKP2Q/PP3r2/3R4 b - - 0 1",                // WAC.005
    "rnbqkb1r/pppp1ppp/8/4P3/6n1/7P/PPPNPPP1/R1BQKBNR b KQkq - 0 1",     // WAC.007
    "r4q1k/p2bR1rp/2p2Q1N/5p2/5p2/2P5/PP3PPP/R5K1 w - - 0 1",            // WAC.008
    "2br2k1/2q3rn/p2NppQ1/2p1P3/Pp5R/4P3/1P3PPP/3R2K1 w - - 0 1",        // WAC.010
    "r1b1kb1r/3q1ppp/pBp1pn2/8/Np3P2/5B2/PPP3PP/R2Q1RK1 w kq - 0 1",     // WAC.011
    "4k1r1/2p3r1/1pR1p3/3pP2p/3P2qP/P4N2/1PQ4P/5R1K b - - 0 1",          // WAC.012
    "5rk1/pp4p1/2n1p2p/2Npq3/2p5/6P1/P3P1BP/R4Q1K w - - 0 1",            // WAC.013
    "r2rb1k1/pp1q1p1p/2n1p1p1/2bp4/5P2/PP1BPR1Q/1BPN2PP/R5K1 w - - 0 1", // WAC.014
    "r1bqk2r/ppp1nppp/4p3/n5N1/2BPp3/P1P5/2P2PPP/R1BQK2R w KQkq - 0 1",  // WAC.022
    "r3nrk1/2p2p1p/p1p1b1p1/2NpPq2/3R4/P1N1Q3/1PP2PPP/4R1K1 w - - 0 1",  // WAC.023
    "6k1/1b1nqpbp/pp4p1/5P2/1PN5/4Q3/P5PP/1B2B1K1 b - - 0 1",            // WAC.024
    "7k/pp4np/2p3p1/3pN1q1/3P4/Q7/1r3rPP/2R2RK1 w - - 0 1",              // WAC.027
    "r2q2k1/pp1rbppp/4pn2/2P5/1P3B2/6P1/P3QPBP/1R3RK1 w - - 0 1",        // WAC.029
    "1r3r2/4q1kp/b1pp2p1/5p2/pPn1N3/6P1/P3PPBP/2QRR1K1 w - - 0 1",       // WAC.030
    "3r2k1/p2q4/1p4p1/3rRp1p/5P1P/6PK/P3R3/3Q4 w - - 0 1",               // WAC.067
    "2kr3r/pppq1ppp/3p1n2/bQ2p3/1n1PP3/1PN1BN1P/1PP2PP1/2KR3R b - - 0 1",// WAC.070
    "r1q3rk/1ppbb1p1/4Np1p/p3pP2/P3P3/2N4R/1PP1Q1PP/3R2K1 w - - 0 1",    // WAC.073

    // =========================================================================
    // Kaufman CCR Test - balanced tactical/positional (10)
    // Source: Larry Kaufman's Computer Chess Rating Test
    // =========================================================================
    "1rbq1rk1/p1b1nppp/1p2p3/8/1B1pN3/P2B4/1P3PPP/2RQ1R1K w - - 0 1",      // Kaufman 1
    "3r2k1/p2r1p1p/1p2p1p1/q4n2/3P4/PQ5P/1P1RNPP1/3R2K1 b - - 0 1",        // Kaufman 2
    "r1b1r1k1/1ppn1p1p/3pnqp1/8/p1P1P3/5P2/PbNQNBPP/1R2RB1K w - - 0 1",    // Kaufman 4
    "2r4k/pB4bp/1p4p1/6q1/1P1n4/2N5/P4PPP/2R1Q1K1 b - - 0 1",              // Kaufman 5
    "2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - 0 1",// Kaufman 7
    "r3k2r/pbn2ppp/8/1P1pP3/P1qP4/5B2/3Q1PPP/R3K2R w KQkq - 0 1",          // Kaufman 9
    "3r2k1/ppq2pp1/4p2p/3n3P/3N2P1/2P5/PP2QP2/K2R4 b - - 0 1",             // Kaufman 10
    "6k1/p3q2p/1nr3pB/8/3Q1P2/6P1/PP5P/3R2K1 b - - 0 1",                   // Kaufman 12
    "1r4k1/7p/5np1/3p3n/8/2NB4/7P/3N1RK1 w - - 0 1",                       // Kaufman 13
    "8/4p3/p2p4/2pP4/2P1P3/1P4k1/1P1K4/8 w - - 0 1",                       // Kaufman 16 (pawn endgame)

    // =========================================================================
    // Eigenmann Rapid Engine Test - modern diverse positions (16)
    // Source: Public domain engine test suite
    // =========================================================================
    "r1bqk1r1/1p1p1n2/p1n2pN1/2p1b2Q/2P1Pp2/1PN5/PB4PP/R4RK1 w q - 0 1",    // ERET 001 - Relief
    "r1n2N1k/2n2K1p/3pp3/5Pp1/b5R1/8/1PPP4/8 w - - 0 1",                    // ERET 002 - Zugzwang
    "r1b1r1k1/1pqn1pbp/p2pp1p1/P7/1n1NPP1Q/2NBBR2/1PP3PP/R6K w - - 0 1",    // ERET 003 - Open Line
    "5b2/p2k1p2/P3pP1p/n2pP1p1/1p1P2P1/1P1KBN2/7P/8 w - - 0 1",             // ERET 004 - Endgame
    "r3kbnr/1b3ppp/pqn5/1pp1P3/3p4/1BN2N2/PP2QPPP/R1BR2K1 w kq - 0 1",      // ERET 005 - Bishop Sac f7
    "2rq1rk1/pb1n1ppN/4p3/1pb5/3P1Pn1/P1N5/1PQ1B1PP/R1B2RK1 b - - 0 1",     // ERET 007 - Bishop Pair
    "r2qk2r/ppp1bppp/2n5/3p1b2/3P1Bn1/1QN1P3/PP3P1P/R3KBNR w KQkq - 0 1",   // ERET 008 - Center
    "rnb1kb1r/p4p2/1qp1pn2/1p2N2p/2p1P1p1/2N3B1/PPQ1BPPP/3RK2R w Kkq - 0 1",// ERET 009 - Knight Sac
    "5rk1/pp1b4/4pqp1/2Ppb2p/1P2p3/4Q2P/P3BPP1/1R3R1K b - - 0 1",           // ERET 010 - Passed Pawn
    "r1b2rk1/p2nqppp/1ppbpn2/3p4/2P5/1PN1PN2/PBQPBPPP/R4RK1 w - - 0 1",     // ERET 048 - Strong Squares
    "r1bq1rk1/p4ppp/3p2n1/1PpPp2n/4P2P/P1PB1PP1/2Q1N3/R1B1K2R b KQ - 0 1",  // ERET 053 - Pos. Sacrifice
    "r2qr1k1/pp1bbp2/n5p1/2pPp2p/8/P2PP1PP/1P2N1BK/R1BQ1R2 w - - 0 1",      // ERET 057 - Exchange
    "8/8/R7/1b4k1/5p2/1B3r2/7P/7K w - - 0 1",                               // ERET 058 - Endgame
    "rq6/5k2/p3pP1p/3p2p1/6PP/1PB1Q3/2P5/1K6 w - - 0 1",                    // ERET 059 - Endgame
    "q2B2k1/pb4bp/4p1p1/2p1N3/2PnpP2/PP3B2/6PP/2RQ2K1 b - - 0 1",           // ERET 060 - King Attack
    "r1bq1r1k/1pp1n1pp/p1np4/4p2Q/2B1PP2/2NB4/PPP3PP/R4RK1 w - - 0 1",      // ERET 061 - Attack
  };

  /// Number of benchmark positions
  constexpr std::size_t BENCH_POSITION_COUNT = BENCH_FENS.size();

}// namespace engine::benchmark

#endif// FRANKYCPP_BENCHMARKPOSITIONS_H
