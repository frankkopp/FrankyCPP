// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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

//
// Created by frank on 26.09.2025.
//

#ifndef FRANKYCPP_BITBOARDS_H
#define FRANKYCPP_BITBOARDS_H

#include "bitboard.h"
#include "orientation.h"

/**
 * @file
 * @brief Central repository of precomputed chess bitboard lookup tables and helper utilities.
 *
 * This header builds (at compile time via constexpr) a rich set of bitboard arrays used
 * throughout move generation, evaluation, and search. All data are immutable and designed
 * to be inlined and optimized away by the compiler.
 *
 * Provided precomputations include:
 *  - rankBb / fileBb / sqBb: Basic per-rank, per-file, per-square masks.
 *  - sqToFileBb / sqToRankBb: Fast mapping from a square to its full file or rank mask.
 *  - squareDiagUpBb / squareDiagDownBb: Diagonal and anti-diagonal membership masks.
 *  - pawnAttacks[color][square]: Pawn attack bitboards (non-occupancy dependent).
 *  - nonSliderAttacks[PieceType][square]: King and knight attack patterns.
 *  - Directional masks (files/ranks north/south/east/west) and neighbor file helpers.
 *  - Rays[orientation][square]: Ray attacks (no blockers) in 8 directions.
 *  - intermediateBb[from][to]: Squares strictly between two line-aligned squares.
 *  - passedPawnMask[color][square]: Squares in front (same & adjacent files) relevant for passed pawn tests.
 *  - colorBb: Light vs. dark square masks.
 *  - castle masks for quick vacancy / attack tests.
 *
 * Conventions:
 *  - Square indexing assumed 0..63 (likely a1 = 0, h8 = 63) consistent with shift usage.
 *  - Bitboard LSB corresponds to square 0.
 *  - Orientation enum (N,S,E,W,NE,NW,SE,SW) indexes rays.
 *
 * Performance notes:
 *  - All data are constexpr so no runtime initialization cost.
 *  - Access is O(1) array indexing; prefer these tables over recomputation.
 *  - Keep this header lean: only definitions that must be globally visible should remain constexpr here.
 *
 * Usage:
 *  - Include this header wherever fast bitboard pattern retrieval is needed.
 *  - Avoid modifying the generation logic without ensuring invariants (square ordering, orientation enums) remain consistent.
 */

// //////////////////////////////////////////////////////////////////
// Bitboard constants and precomputed bitboards
// //////////////////////////////////////////////////////////////////

constexpr Bitboard BbZero = 0;
constexpr Bitboard BbFull = ~BbZero;
constexpr Bitboard BbOne  = 1;

constexpr Bitboard FileABB = 0x0101010101010101ULL;
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;
constexpr Bitboard FileHBB = FileABB << 7;

constexpr Bitboard Rank1BB = 0xFF;
constexpr Bitboard Rank2BB = Rank1BB << 8;
constexpr Bitboard Rank3BB = Rank1BB << 16;//(8 * 2)
constexpr Bitboard Rank4BB = Rank1BB << 24;//(8 * 3)
constexpr Bitboard Rank5BB = Rank1BB << 32;//(8 * 4)
constexpr Bitboard Rank6BB = Rank1BB << 40;//(8 * 5)
constexpr Bitboard Rank7BB = Rank1BB << 48;//(8 * 6)
constexpr Bitboard Rank8BB = Rank1BB << 56;//(8 * 7)

constexpr Bitboard DiagUpA1 = 0b1000000001000000001000000001000000001000000001000000001000000001;
constexpr Bitboard DiagUpB1 = DiagUpA1 << 1 & ~FileABB;// shift EAST
constexpr Bitboard DiagUpC1 = DiagUpB1 << 1 & ~FileABB;
constexpr Bitboard DiagUpD1 = DiagUpC1 << 1 & ~FileABB;
constexpr Bitboard DiagUpE1 = DiagUpD1 << 1 & ~FileABB;
constexpr Bitboard DiagUpF1 = DiagUpE1 << 1 & ~FileABB;
constexpr Bitboard DiagUpG1 = DiagUpF1 << 1 & ~FileABB;
constexpr Bitboard DiagUpH1 = DiagUpG1 << 1 & ~FileABB;
constexpr Bitboard DiagUpA2 = DiagUpA1 << 8;// shift NORTH
constexpr Bitboard DiagUpA3 = DiagUpA2 << 8;
constexpr Bitboard DiagUpA4 = DiagUpA3 << 8;
constexpr Bitboard DiagUpA5 = DiagUpA4 << 8;
constexpr Bitboard DiagUpA6 = DiagUpA5 << 8;
constexpr Bitboard DiagUpA7 = DiagUpA6 << 8;
constexpr Bitboard DiagUpA8 = DiagUpA7 << 8;

constexpr Bitboard DiagDownH1 = 0b0000000100000010000001000000100000010000001000000100000010000000;
constexpr Bitboard DiagDownH2 = DiagDownH1 << 8;// shift NORTH
constexpr Bitboard DiagDownH3 = DiagDownH2 << 8;
constexpr Bitboard DiagDownH4 = DiagDownH3 << 8;
constexpr Bitboard DiagDownH5 = DiagDownH4 << 8;
constexpr Bitboard DiagDownH6 = DiagDownH5 << 8;
constexpr Bitboard DiagDownH7 = DiagDownH6 << 8;
constexpr Bitboard DiagDownH8 = DiagDownH7 << 8;
constexpr Bitboard DiagDownG1 = DiagDownH1 >> 1 & ~FileHBB;// shift WEST
constexpr Bitboard DiagDownF1 = DiagDownG1 >> 1 & ~FileHBB;
constexpr Bitboard DiagDownE1 = DiagDownF1 >> 1 & ~FileHBB;
constexpr Bitboard DiagDownD1 = DiagDownE1 >> 1 & ~FileHBB;
constexpr Bitboard DiagDownC1 = DiagDownD1 >> 1 & ~FileHBB;
constexpr Bitboard DiagDownB1 = DiagDownC1 >> 1 & ~FileHBB;
constexpr Bitboard DiagDownA1 = DiagDownB1 >> 1 & ~FileHBB;

constexpr Bitboard CENTER_FILES   = FileDBB | FileEBB;
constexpr Bitboard CENTER_RANKS   = Rank4BB | Rank5BB;
constexpr Bitboard CENTER_SQUARES = CENTER_FILES & CENTER_RANKS;

namespace Bitboards {
  consteval std::array<Bitboard, 8> makeRankBb() {
    std::array<Bitboard, 8> a{};
    for (int i = 0; i < 8; ++i) a[i] = Rank1BB << (8 * i);
    return a;
  }
  // holds the corresponding bitboards for each rank
  inline constexpr std::array<Bitboard, 8> rankBb = makeRankBb();

  consteval std::array<Bitboard, 8> makeFileBb() {
    std::array<Bitboard, 8> a{};
    for (int i = 0; i < 8; ++i) a[i] = FileABB << i;
    return a;
  }
  // holds the corresponding bitboards for each file
  inline constexpr std::array<Bitboard, 8> fileBb = makeFileBb();

  consteval std::array<Bitboard, SQ_LENGTH> makeSqBb() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) a[s] = BbOne << static_cast<int>(s);
    return a;
  }
  // holds a bitboard with only one bit set at the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> sqBb = makeSqBb();

  consteval int fileIdx(const unsigned s) { return s & 7U; }
  consteval std::array<Bitboard, SQ_LENGTH> makeSqToFileBb() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) a[s] = fileBb[fileIdx(s)];
    return a;
  }
  // holds bitboards for each square's file
  inline constexpr std::array<Bitboard, SQ_LENGTH> sqToFileBb = makeSqToFileBb();


  consteval unsigned rankIdx(const unsigned s) { return s >> 3U; }
  consteval std::array<Bitboard, SQ_LENGTH> makeSqToRankBb() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) a[s] = rankBb[rankIdx(s)];
    return a;
  }
  // holds bitboards for each square's rank
  inline constexpr std::array<Bitboard, SQ_LENGTH> sqToRankBb = makeSqToRankBb();

  consteval Bitboard diagUpForIdx(const Square s) {
    const Bitboard one = sqBb[s];
    if (DiagUpA8 & one) return DiagUpA8;
    if (DiagUpA7 & one) return DiagUpA7;
    if (DiagUpA6 & one) return DiagUpA6;
    if (DiagUpA5 & one) return DiagUpA5;
    if (DiagUpA4 & one) return DiagUpA4;
    if (DiagUpA3 & one) return DiagUpA3;
    if (DiagUpA2 & one) return DiagUpA2;
    if (DiagUpA1 & one) return DiagUpA1;
    if (DiagUpB1 & one) return DiagUpB1;
    if (DiagUpC1 & one) return DiagUpC1;
    if (DiagUpD1 & one) return DiagUpD1;
    if (DiagUpE1 & one) return DiagUpE1;
    if (DiagUpF1 & one) return DiagUpF1;
    if (DiagUpG1 & one) return DiagUpG1;
    if (DiagUpH1 & one) return DiagUpH1;
    return BbZero;
  }
  consteval std::array<Bitboard, SQ_LENGTH> makeSquareDiagUpBb() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) a[s] = diagUpForIdx(s);
    return a;
  }
  // holds precomputed bitboards for each square's diagonal (A1-H8)
  inline constexpr std::array<Bitboard, SQ_LENGTH> squareDiagUpBb = makeSquareDiagUpBb();

  consteval Bitboard diagDownForIdx(const Square s) {
    const Bitboard one = sqBb[s];
    if (DiagDownH8 & one) return DiagDownH8;
    if (DiagDownH7 & one) return DiagDownH7;
    if (DiagDownH6 & one) return DiagDownH6;
    if (DiagDownH5 & one) return DiagDownH5;
    if (DiagDownH4 & one) return DiagDownH4;
    if (DiagDownH3 & one) return DiagDownH3;
    if (DiagDownH2 & one) return DiagDownH2;
    if (DiagDownH1 & one) return DiagDownH1;
    if (DiagDownG1 & one) return DiagDownG1;
    if (DiagDownF1 & one) return DiagDownF1;
    if (DiagDownE1 & one) return DiagDownE1;
    if (DiagDownD1 & one) return DiagDownD1;
    if (DiagDownC1 & one) return DiagDownC1;
    if (DiagDownB1 & one) return DiagDownB1;
    if (DiagDownA1 & one) return DiagDownA1;
    return BbZero;
  }
  consteval std::array<Bitboard, SQ_LENGTH> makeSquareDiagDownBb() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) a[s] = diagDownForIdx(s);
    return a;
  }
  // holds precomputed bitboards for each square's anti-diagonal (H1-A8)
  inline constexpr std::array<Bitboard, SQ_LENGTH> squareDiagDownBb = makeSquareDiagDownBb();

  // holds bitboards for the attacked squares if a pawn of the given color is on the given square
  consteval std::array<std::array<Bitboard, SQ_LENGTH>, COLOR_LENGTH> makePawnAttacks() {
    std::array<std::array<Bitboard, SQ_LENGTH>, COLOR_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const int f = static_cast<int>(s & 7);
      const int r = static_cast<int>(s >> 3);
      // consolidated loop over colors to avoid redundancy
      for (unsigned c = 0; c < COLOR_LENGTH; ++c) {
        const int dir = Color{c}.sign();
        Bitboard m    = 0;
        const int nr  = r + dir;
        if (0 <= nr && nr < 8) {
          if (f - 1 >= 0) m |= sqBb[static_cast<unsigned>(nr * 8 + (f - 1))];
          if (f + 1 < 8) m |= sqBb[static_cast<unsigned>(nr * 8 + (f + 1))];
        }
        a[c][s] = m;
      }
    }
    return a;
  }
  // holds precomputed pawnAttacks[Color][Square]
  inline constexpr std::array<std::array<Bitboard, SQ_LENGTH>, COLOR_LENGTH> pawnAttacks = makePawnAttacks();

  consteval std::array<std::array<Bitboard, SQ_LENGTH>, PT_LENGTH> makeNonSliderAttacks() {
    std::array<std::array<Bitboard, SQ_LENGTH>, PT_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const int f = static_cast<int>(s & 7);
      const int r = static_cast<int>(s >> 3);
      // KING (8 neighbours)
      {
        Bitboard m = 0;
        for (int df = -1; df <= 1; ++df) {
          for (int dr = -1; dr <= 1; ++dr) {
            if (df == 0 && dr == 0) continue;
            const int nf = f + df, nr = r + dr;
            if (0 <= nf && nf < 8 && 0 <= nr && nr < 8) m |= sqBb[static_cast<unsigned>(nr * 8 + nf)];
          }
        }
        a[KING][s] = m;
      }
      // KNIGHT (8 L-moves)
      {
        Bitboard m = 0;
        for (int i = 0; i < 8; ++i) {
          constexpr int kdr[8] = {-1, -2, -2, -1, 1, 2, 2, 1};
          constexpr int kdf[8] = {-2, -1, 1, 2, -2, -1, 1, 2};
          const int nf = f + kdf[i], nr = r + kdr[i];
          if (0 <= nf && nf < 8 && 0 <= nr && nr < 8) m |= sqBb[static_cast<unsigned>(nr * 8 + nf)];
        }
        a[KNIGHT][s] = m;
      }
    }
    return a;
  }
  // holds precomputed nonSliderAttacks[PieceType][Square]
  inline constexpr std::array<std::array<Bitboard, SQ_LENGTH>, PT_LENGTH> nonSliderAttacks = makeNonSliderAttacks();

  consteval std::array<Bitboard, SQ_LENGTH> makeFilesWestMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) {
      const int f = fileIdx(s);
      Bitboard m       = 0;
      for (int k = 0; k < f; ++k) m |= FileABB << k;
      a[s] = m;
    }
    return a;
  }
  // holds precomputed bitboards for all the files to the left of the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> filesWestMask = makeFilesWestMask();

  consteval std::array<Bitboard, SQ_LENGTH> makeFilesEastMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) {
      const int f = fileIdx(s);
      Bitboard m       = 0;
      for (int k = f + 1; k < 8; ++k) m |= FileABB << k;
      a[s] = m;
    }
    return a;
  }
  // holds precomputed bitboards for all the files to the right of the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> filesEastMask = makeFilesEastMask();

  consteval std::array<Bitboard, SQ_LENGTH> makeRanksNorthMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) {
      const int r = rankIdx(s);
      Bitboard m       = 0;
      for (int k = r + 1; k < 8; ++k) m |= Rank1BB << (8 * k);
      a[s] = m;
    }
    return a;
  }
  // holds precomputed bitboards for all the ranks above the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> ranksNorthMask = makeRanksNorthMask();

  consteval std::array<Bitboard, SQ_LENGTH> makeRanksSouthMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) {
      const int r = rankIdx(s);
      Bitboard m       = 0;
      for (int k = 0; k < r; ++k) m |= Rank1BB << (8 * k);
      a[s] = m;
    }
    return a;
  }
  // holds precomputed bitboards for all the ranks below the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> ranksSouthMask = makeRanksSouthMask();

  consteval std::array<Bitboard, SQ_LENGTH> makeFileWestMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) {
      const int f = fileIdx(s);
      a[s]             = f > 0 ? Bitboard(FileABB << (f - 1)) : BbZero;
    }
    return a;
  }
  // holds precomputed bitboards for the file to the left of the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> fileWestMask = makeFileWestMask();

  consteval std::array<Bitboard, SQ_LENGTH> makeFileEastMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) {
      const int f = fileIdx(s);
      a[s]             = f < 7 ? Bitboard(FileABB << (f + 1)) : BbZero;
    }
    return a;
  }
  // holds precomputed bitboards for the file to the right of the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> fileEastMask = makeFileEastMask();

  consteval std::array<Bitboard, SQ_LENGTH> makeNeighbourFilesMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s : Square::all()) {
      const int f = fileIdx(s);
      Bitboard m       = 0;
      if (f > 0) m |= FileABB << (f - 1);
      if (f < 7) m |= FileABB << (f + 1);
      a[s] = m;
    }
    return a;
  }
  // holds precomputed bitboards for the files neighboring the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> neighbourFilesMask = makeNeighbourFilesMask();

  consteval std::array<Bitboard, COLOR_LENGTH> makeColorBb() {
    std::array<Bitboard, COLOR_LENGTH> a{};
    for (Square s : Square::all()) {
      const int f   = fileIdx(s);
      const int r   = rankIdx(s);
      const Bitboard bit = sqBb[s];
      if ((f + r & 1U) == 0) a[BLACK] |= bit;
      else
        a[WHITE] |= bit;
    }
    return a;
  }
  // holds precomputed bitboards for all black and white squares
  inline constexpr std::array<Bitboard, COLOR_LENGTH> colorBb = makeColorBb();

  // holds precomputed bitboards for squares involved in the corresponding castle move
  inline constexpr std::array kingSideCastleMask = {
    sqBb[SQ_F1] | sqBb[SQ_G1] | sqBb[SQ_H1],
    sqBb[SQ_F8] | sqBb[SQ_G8] | sqBb[SQ_H8]};
  // holds precomputed bitboards for squares involved in the corresponding castle move
  inline constexpr std::array queenSideCastleMask = {
    sqBb[SQ_D1] | sqBb[SQ_C1] | sqBb[SQ_B1] | sqBb[SQ_A1],
    sqBb[SQ_D8] | sqBb[SQ_C8] | sqBb[SQ_B8] | sqBb[SQ_A8]};


  consteval std::array<std::array<Bitboard, SQ_LENGTH>, OR_LENGTH> makeRays() {
    std::array<std::array<Bitboard, SQ_LENGTH>, OR_LENGTH> a{};

    for (Square s : Square::all()) {
      const Bitboard file     = sqToFileBb[s];
      const Bitboard rank     = sqToRankBb[s];
      const Bitboard diagUp   = squareDiagUpBb[s];  // NE-SW diagonal
      const Bitboard diagDown = squareDiagDownBb[s];// NW-SE diagonal
      // Orthogonal rays
      a[N][s] = file & ranksNorthMask[s];
      a[S][s] = file & ranksSouthMask[s];
      a[E][s] = rank & filesEastMask[s];
      a[W][s] = rank & filesWestMask[s];
      // Diagonal rays
      a[NE][s] = diagUp & ranksNorthMask[s] & filesEastMask[s];
      a[SW][s] = diagUp & ranksSouthMask[s] & filesWestMask[s];
      a[NW][s] = diagDown & ranksNorthMask[s] & filesWestMask[s];
      a[SE][s] = diagDown & ranksSouthMask[s] & filesEastMask[s];
    }
    return a;
  }
  // holds precomputed ray bitboards  for all orientations and squares
  inline constexpr std::array<std::array<Bitboard, SQ_LENGTH>, OR_LENGTH> rays = makeRays();

  consteval std::array<std::array<Bitboard, SQ_LENGTH>, SQ_LENGTH> makeIntermediateBb() {
    std::array<std::array<Bitboard, SQ_LENGTH>, SQ_LENGTH> a{};
    for (int from = 0; from < SQ_LENGTH; ++from) {
      for (int to = 0; to < SQ_LENGTH; ++to) {
        const Bitboard toBB = sqBb[to];
        Bitboard acc        = 0;

        for (int o = 0; o < OR_LENGTH; ++o) {
          if ((rays[o][from] & toBB) != BbZero) {

            acc |= rays[o][from] & ~rays[o][to] & ~toBB;

          }
        }
        a[from][to] = acc;
      }
    }
    return a;
  }
  // holds precomputed bitboards for the squares between the two given squares or BbZero if the squares are not
  // on a straight line to each other or if they are direct neighbors
  inline constexpr std::array<std::array<Bitboard, SQ_LENGTH>, SQ_LENGTH> intermediateBb = makeIntermediateBb();

  consteval std::array<std::array<Bitboard, SQ_LENGTH>, COLOR_LENGTH> makePassedPawnMask() {
    std::array<std::array<Bitboard, SQ_LENGTH>, COLOR_LENGTH> a{};

    for (int s = 0; s < SQ_LENGTH; ++s) {
      const int f = fileIdx(s);
      const int r = static_cast<int>(rankIdx(s));

      // white pawn
      Bitboard w = rays[N][s];
      if (f < 7 && r < 7) w |= rays[N][s + 1U];// east neighbor
      if (f > 0 && r < 7) w |= rays[N][s - 1U];// west neighbor
      a[WHITE][s] = w;

      // black pawn
      Bitboard b = rays[S][s];
      if (f < 7 && r > 0) b |= rays[S][s + 1U];// east neighbor
      if (f > 0 && r > 0) b |= rays[S][s - 1U];// west neighbor
      a[BLACK][s] = b;
    }

    return a;
  }
  // holds precomputed bitboards for the squares in front of the given pawn on the same or neighboring files
  inline constexpr std::array<std::array<Bitboard, SQ_LENGTH>, COLOR_LENGTH> passedPawnMask = makePassedPawnMask();

}// namespace Bitboards

// Operators for Squares as Bitboards
constexpr Bitboard operator&(const Square lhs, const Square rhs) { return Bitboards::sqBb[lhs] & Bitboards::sqBb[rhs]; }
constexpr Bitboard operator|(const Square lhs, const Square rhs) { return Bitboards::sqBb[lhs] | Bitboards::sqBb[rhs]; }

// Operators for Squares on Bitboards
constexpr Bitboard operator&(const Bitboard b, const Square s) { return b & Bitboards::sqBb[s]; }
constexpr Bitboard operator|(const Bitboard b, const Square s) { return b | Bitboards::sqBb[s]; }
constexpr Bitboard operator^(const Bitboard b, const Square s) { return b ^ Bitboards::sqBb[s]; }
constexpr Bitboard& operator|=(Bitboard& b, const Square s) { return b |= Bitboards::sqBb[s]; }
constexpr Bitboard& operator^=(Bitboard& b, const Square s) { return b ^= Bitboards::sqBb[s]; }


#endif// FRANKYCPP_BITBOARDS_H
