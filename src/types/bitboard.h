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

#ifndef FRANKYCPP_BITBOARD_H
#define FRANKYCPP_BITBOARD_H

#include "direction.h"
#include "orientation.h"
#include "piecetype.h"
#include "square.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <immintrin.h>

#ifndef HAS_PEXT
#error "PEXT-only path requested, but HAS_PEXT is not defined. Enable BMI2 (/arch:AVX2 or -mbmi2) and add -DHAS_PEXT."
#endif

// 64-bit Bitboard type for storing boards as bits
typedef uint64_t Bitboard;

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
  constexpr std::array<Bitboard, 8> makeRankBb() {
    std::array<Bitboard, 8> a{};
    for (unsigned i = 0; i < 8; ++i) a[i] = Rank1BB << (8 * i);
    return a;
  }
  // holds the corresponding bitboards for each rank
  inline constexpr std::array<Bitboard, 8> rankBb = makeRankBb();

  constexpr std::array<Bitboard, 8> makeFileBb() {
    std::array<Bitboard, 8> a{};
    for (unsigned i = 0; i < 8; ++i) a[i] = FileABB << i;
    return a;
  }
  // holds the corresponding bitboards for each file
  inline constexpr std::array<Bitboard, 8> fileBb = makeFileBb();

  constexpr std::array<Bitboard, SQ_LENGTH> makeSqBb() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) a[s] = BbOne << s;
    return a;
  }
  // holds a bitboard with only one bit set at the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> sqBb = makeSqBb();

  constexpr unsigned fileIdx(const unsigned s) { return s & 7U; }
  constexpr std::array<Bitboard, SQ_LENGTH> makeSqToFileBb() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s = SQ_A1; s <= SQ_H8; ++s) a[s] = fileBb[fileIdx(s)];
    return a;
  }
  // holds bitboards for each square's file
  inline constexpr std::array<Bitboard, SQ_LENGTH> sqToFileBb       = makeSqToFileBb();


  constexpr unsigned rankIdx(const unsigned s) { return s >> 3U; }
  constexpr std::array<Bitboard, SQ_LENGTH> makeSqToRankBb() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s = SQ_A1; s <= SQ_H8; ++s) a[s] = rankBb[rankIdx(s)];
    return a;
  }
  // holds bitboards for each square's rank
  inline constexpr std::array<Bitboard, SQ_LENGTH> sqToRankBb       = makeSqToRankBb();

  constexpr Bitboard diagUpForIdx(const Square s) {
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
  constexpr std::array<Bitboard, SQ_LENGTH> makeSquareDiagUpBb() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s = SQ_A1; s <= SQ_H8; ++s) a[s] = diagUpForIdx(s);
    return a;
  }
  // holds precomputed bitboards for each square's diagonal (A1-H8)
  inline constexpr std::array<Bitboard, SQ_LENGTH> squareDiagUpBb   = makeSquareDiagUpBb();

  constexpr Bitboard diagDownForIdx(const Square s) {
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
  constexpr std::array<Bitboard, SQ_LENGTH> makeSquareDiagDownBb() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (Square s = SQ_A1; s <= SQ_H8; ++s) a[s] = diagDownForIdx(s);
    return a;
  }
  // holds precomputed bitboards for each square's anti-diagonal (H1-A8)
  inline constexpr std::array<Bitboard, SQ_LENGTH> squareDiagDownBb = makeSquareDiagDownBb();

  // holds bitboards for the attacked squares if a pawn of the given color is on the given square
  constexpr std::array<std::array<Bitboard, SQ_LENGTH>, COLOR_LENGTH> makePawnAttacks() {
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

  constexpr std::array<std::array<Bitboard, SQ_LENGTH>, PT_LENGTH> makeNonSliderAttacks() {
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

  constexpr std::array<Bitboard, SQ_LENGTH> makeFilesWestMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const unsigned f = fileIdx(s);
      Bitboard m       = 0;
      for (unsigned k = 0; k < f; ++k) m |= FileABB << k;
      a[s] = m;
    }
    return a;
  }
  // holds precomputed bitboards for all the files to the left of the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> filesWestMask = makeFilesWestMask();

  constexpr std::array<Bitboard, SQ_LENGTH> makeFilesEastMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const unsigned f = fileIdx(s);
      Bitboard m       = 0;
      for (unsigned k = f + 1; k < 8; ++k) m |= FileABB << k;
      a[s] = m;
    }
    return a;
  }
  // holds precomputed bitboards for all the files to the right of the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> filesEastMask = makeFilesEastMask();

  constexpr std::array<Bitboard, SQ_LENGTH> makeRanksNorthMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const unsigned r = rankIdx(s);
      Bitboard m       = 0;
      for (unsigned k = r + 1; k < 8; ++k) m |= Rank1BB << (8 * k);
      a[s] = m;
    }
    return a;
  }
  // holds precomputed bitboards for all the ranks above the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> ranksNorthMask = makeRanksNorthMask();

  constexpr std::array<Bitboard, SQ_LENGTH> makeRanksSouthMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const unsigned r = rankIdx(s);
      Bitboard m       = 0;
      for (unsigned k = 0; k < r; ++k) m |= Rank1BB << (8 * k);
      a[s] = m;
    }
    return a;
  }
  // holds precomputed bitboards for all the ranks below the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> ranksSouthMask = makeRanksSouthMask();

  constexpr std::array<Bitboard, SQ_LENGTH> makeFileWestMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const unsigned f = fileIdx(s);
      a[s]             = f > 0 ? FileABB << (f - 1) : BbZero;
    }
    return a;
  }
  // holds precomputed bitboards for the file to the left of the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> fileWestMask = makeFileWestMask();

  constexpr std::array<Bitboard, SQ_LENGTH> makeFileEastMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const unsigned f = fileIdx(s);
      a[s]             = f < 7 ? FileABB << (f + 1) : BbZero;
    }
    return a;
  }
  // holds precomputed bitboards for the file to the right of the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> fileEastMask = makeFileEastMask();

  constexpr std::array<Bitboard, SQ_LENGTH> makeNeighbourFilesMask() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const unsigned f = fileIdx(s);
      Bitboard m       = 0;
      if (f > 0) m |= FileABB << (f - 1);
      if (f < 7) m |= FileABB << (f + 1);
      a[s] = m;
    }
    return a;
  }
  // holds precomputed bitboards for the files neighboring the given square
  inline constexpr std::array<Bitboard, SQ_LENGTH> neighbourFilesMask = makeNeighbourFilesMask();

  constexpr std::array<Bitboard, COLOR_LENGTH> makeColorBb() {
    std::array<Bitboard, COLOR_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const unsigned f   = fileIdx(s);
      const unsigned r   = rankIdx(s);
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
  constexpr std::array kingSideCastleMask = {
    sqBb[SQ_F1] | sqBb[SQ_G1] | sqBb[SQ_H1],
    sqBb[SQ_F8] | sqBb[SQ_G8] | sqBb[SQ_H8]};
  // holds precomputed bitboards for squares involved in the corresponding castle move
  constexpr std::array queenSideCastleMask = {
    sqBb[SQ_D1] | sqBb[SQ_C1] | sqBb[SQ_B1] | sqBb[SQ_A1],
    sqBb[SQ_D8] | sqBb[SQ_C8] | sqBb[SQ_B8] | sqBb[SQ_A8]};


  constexpr std::array<std::array<Bitboard, SQ_LENGTH>, OR_LENGTH> makeRays() {
    std::array<std::array<Bitboard, SQ_LENGTH>, OR_LENGTH> a{};

    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
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

  constexpr std::array<std::array<Bitboard, SQ_LENGTH>, SQ_LENGTH> makeIntermediateBb() {
    std::array<std::array<Bitboard, SQ_LENGTH>, SQ_LENGTH> a{};
    for (unsigned from = 0; from < SQ_LENGTH; ++from) {
      for (unsigned to = 0; to < SQ_LENGTH; ++to) {
        const Bitboard toBB = sqBb[to];
        Bitboard acc        = 0;

        for (unsigned o = 0; o < OR_LENGTH; ++o) {
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

  constexpr std::array<std::array<Bitboard, SQ_LENGTH>, COLOR_LENGTH> makePassedPawnMask() {
    std::array<std::array<Bitboard, SQ_LENGTH>, COLOR_LENGTH> a{};

    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const int f = static_cast<int>(fileIdx(s));
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

// //////////////////////////////////////////////////////////////////
// Bitboard functions
// //////////////////////////////////////////////////////////////////

/**
 * Shifts a bitboard in the given direction.
 * @param d Direction
 * @param b Bitboard
 * @return shifted bitboard
 */
inline Bitboard shiftBb(const Direction d, const Bitboard b) {
  // move the bits and clear the left our right file
  // after the shift to erase bit jumping over
  switch (d) {
    case NORTH:
      return b << 8;
    case EAST:
      return b << 1 & ~FileABB;
    case SOUTH:
      return b >> 8;
    case WEST:
      return b >> 1 & ~FileHBB;
    case NORTH_EAST:
      return b << 9 & ~FileABB;
    case SOUTH_EAST:
      return b >> 7 & ~FileABB;
    case SOUTH_WEST:
      return b >> 9 & ~FileHBB;
    case NORTH_WEST:
      return b << 7 & ~FileHBB;
  }
  return b;
}

// if C++20 feature library <bit> is available, use the new bit operations
#if __cpp_lib_bitops >= 201907L
#include <bit>
#endif

// popcount() counts the number of non-zero bits in a bitboard
inline int popcount(Bitboard b) {
#if __cpp_lib_bitops >= 201907L
  return std::popcount(b);
#else
#if defined(__GNUC__)// GCC, Clang, ICC
  return __builtin_popcountll(b);
#elif defined(_MSC_VER)
  return static_cast<int>(_mm_popcnt_u64(b));
#else// Compiler is not GCC
  // pre-computed table of population counter for 16-bit
  extern uint8_t PopCnt16[1 << 16];
  // nice trick to address 16-bit groups in a 64-bit int
  union {
    Bitboard bb;
    uint16_t u[4];
  } v = {b};
  // adding all 16-bit population counters referenced in the 64-bit union
  return PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]];
#endif
#endif// _cpp_bit
}

// Used when no build-in popcount is available for compiler.
// @return popcount16() counts the non-zero bits using SWAR-Popcount algorithm
[[maybe_unused]] inline unsigned popcount16(unsigned u) {
  u -= u >> 1U & 0x5555U;
  u = (u >> 2U & 0x3333U) + (u & 0x3333U);
  u = (u >> 4U) + u & 0x0F0FU;
  return (u * 0x0101U) >> 8U;
}

// lsb() and msb() return the least/most significant bit in a non-zero
// bitboard
inline Square lsb(Bitboard b) {
  if (!b) return SQ_NONE;
#if __cpp_lib_bitops >= 201907L
  return static_cast<Square>(std::countr_zero(b));
#else
#ifdef __GNUC__// GCC, Clang, ICC
  return static_cast<Square>(__builtin_ctzll(b));
#elif defined(_MSC_VER)
  unsigned long index;
  if (_BitScanForward64(&index, b)) { return static_cast<Square>(index); }
  else { return SQ_NONE; }
#else// Compiler is not GCC
#error "Compiler not yet supported."
#endif
#endif
}

// lsb() and msb() return the least/most significant bit in a non-zero
// bitboard
inline Square msb(Bitboard b) {
  if (!b) return SQ_NONE;
#if __cpp_lib_bitops >= 201907L
  return static_cast<Square>(63 - std::countl_zero(b));
#else
#if defined(__GNUC__)// GCC, Clang, ICC
  return static_cast<Square>(63 - __builtin_clzll(b));
#elif defined(_MSC_VER)
  unsigned long index;
  if (_BitScanReverse64(&index, b)) { return static_cast<Square>(index); }
  else { return SQ_NONE; }
#else// Compiler is not GCC
#error "Compiler not yet supported."
#endif
#endif
}

// pop_lsb() finds and clears the least significant bit in a non-zero
// bitboard. Returns the cleared bit as a Square or SQ_NONE if
// bitboard was zero. The given Bitboard is changed in-place.
// Example:
// Bitboard b = ...;
// while (b) {
//   Square s = pop_lsb(b);
//   ...
// }
inline Square popLSB(Bitboard& b) {
  if (!b) return SQ_NONE;
  const Square s = lsb(b);
  b &= b - 1;
  return s;
}

// //////////////////////////////////////////////////////////////////
// Bitboard operators
// //////////////////////////////////////////////////////////////////

// Operators for Squares as Bitboards
constexpr Bitboard operator&(const Square lhs, const Square rhs) { return Bitboards::sqBb[lhs] & Bitboards::sqBb[rhs]; }
constexpr Bitboard operator|(const Square lhs, const Square rhs) { return Bitboards::sqBb[lhs] | Bitboards::sqBb[rhs]; }

// Operators for Squares on Bitboards
constexpr Bitboard operator&(const Bitboard b, const Square s) { return b & Bitboards::sqBb[s]; }
constexpr Bitboard operator|(const Bitboard b, const Square s) { return b | Bitboards::sqBb[s]; }
constexpr Bitboard operator^(const Bitboard b, const Square s) { return b ^ Bitboards::sqBb[s]; }
constexpr Bitboard& operator|=(Bitboard& b, const Square s) { return b |= Bitboards::sqBb[s]; }
constexpr Bitboard& operator^=(Bitboard& b, const Square s) { return b ^= Bitboards::sqBb[s]; }

// //////////////////////////////////////////////////////////////////
// Bitboard print functions
// //////////////////////////////////////////////////////////////////

// Prints a bitboard as a bitset
inline std::string str(const Bitboard b) {
  std::ostringstream os;
  os << std::bitset<64>(b);
  return os.str();
}

// Prints a bitboard in an 8x8 matrix for output on a console
inline std::string strBoard(const Bitboard b) {
  std::ostringstream os;
  os << "+---+---+---+---+---+---+---+---+\n";
  for (Rank r = RANK_8;; --r) {
    for (File f = FILE_A; f <= FILE_H; ++f) {
      os << (b & Square::of(f, r) ? "| X " : "|   ");
    }
    os << "|\n+---+---+---+---+---+---+---+---+\n";
    if (r == 0) break;
  }
  return os.str();
}

// StringGrouped returns a string representation of the 64 bits grouped in 8.
// Order is LSB to msb ==> A1 B1 ... G8 H8
inline std::string strGrouped(const Bitboard b) {
  std::ostringstream os;
  for (uint16_t i = 0; i < 64; i++) {
    if (i > 0 && i % 8 == 0) {
      os << ".";
    }
    os << ((b & (BbOne << i)) ? "1" : "0");
  }
  os << " (" + std::to_string(b) + ")";
  return os.str();
}

// //////////////////////////////////////////////////////////////////
// Magic bitboards
// Bitboard initialization and pre-computation
// //////////////////////////////////////////////////////////////////

// constexpr popcount to avoid non-constexpr intrinsics on some compilers
constexpr unsigned popcount_ce(Bitboard b) {
  unsigned c = 0;
  while (b) {
    b &= b - 1;
    ++c;
  }
  return c;
}

// constexpr software pext (bit compress)
constexpr uint64_t pext_soft(const uint64_t src, uint64_t mask) {
  uint64_t res = 0;
  uint64_t bit = 1;
  while (mask) {
    const uint64_t lsb = mask & -mask;
    if (src & lsb) res |= bit;
    mask ^= lsb;
    bit <<= 1;
  }
  return res;
}

// Magic holds all magic bitboards relevant for a single square
// Ideas taken from Stockfish
// License see https://stockfishchess.org/about/
struct Magic {
  Bitboard mask{};
  Bitboard magic{}; // unused on PEXT path
  unsigned shift{}; // unused on PEXT path
  uint32_t offset{};// start index into the global table

  [[nodiscard]] constexpr unsigned index(const Bitboard occupied) const {
    if (std::is_constant_evaluated())
      return static_cast<unsigned>(pext_soft(occupied, mask));
    else
      return static_cast<unsigned>(_pext_u64(occupied, mask));
  }
};

constexpr Direction rookDirections[4]   = {NORTH, EAST, SOUTH, WEST};
constexpr Direction bishopDirections[4] = {NORTH_EAST, SOUTH_EAST, SOUTH_WEST, NORTH_WEST};

constexpr Bitboard sliding_attack(const Direction directions[], const Square sq, const Bitboard occupied) {
  Bitboard attack = 0;
  for (int i = 0; i < 4; ++i) {
    for (Square s = sq + directions[i];; s += directions[i]) {
      if (!s.isValid()) break;
      // ensure we don't wrap around across files/ranks; guard before distance()
      if (s.distanceTo(s - directions[i]) != 1) break;
      attack |= s;
      if (occupied & s)
        break;
    }
  }
  return attack;
}

// --- Compile-time sanity check ("proof") on an empty board ---
// Rook from A1 must have 14 targets on an empty board (7 north + 7 east).
// Use your own popcount(Bitboard) if available; otherwise std::popcount (C++20).
#ifndef __JETBRAINS_IDE__
static_assert(
  popcount_ce(sliding_attack(rookDirections, SQ_A1, Bitboard{0})) == 14,
  "sliding_attack should yield 14 rook moves from A1 on an empty board");
#endif

namespace Bitboards {
  constexpr Bitboard edgeMaskFor(const unsigned s) {
    return (Rank1BB | Rank8BB) & ~sqToRankBb[s] | (FileABB | FileHBB) & ~sqToFileBb[s];
  }

  constexpr Bitboard rookMaskFor(const unsigned s) {
    return (rays[N][s] | rays[S][s] | rays[E][s] | rays[W][s]) & ~edgeMaskFor(s);
  }

  constexpr Bitboard bishopMaskFor(const unsigned s) {
    return (rays[NE][s] | rays[NW][s] | rays[SE][s] | rays[SW][s]) & ~edgeMaskFor(s);
  }

  constexpr std::array<Bitboard, SQ_LENGTH> makeRookMasks() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) a[s] = rookMaskFor(s);
    return a;
  }

  constexpr std::array<Bitboard, SQ_LENGTH> makeBishopMasks() {
    std::array<Bitboard, SQ_LENGTH> a{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s) a[s] = bishopMaskFor(s);
    return a;
  }

  constexpr std::array<uint32_t, SQ_LENGTH + 1> makeOffsets(const std::array<Bitboard, SQ_LENGTH>& masks) {
    std::array<uint32_t, SQ_LENGTH + 1> off{};
    off[0] = 0;
    for (unsigned s = 0; s < SQ_LENGTH; ++s) {
      const unsigned bits = popcount_ce(masks[s]);
      off[s + 1]          = static_cast<uint32_t>(off[s] + (1u << bits));
    }
    return off;
  }

  constexpr std::array<Magic, SQ_LENGTH> makeMagics(const std::array<Bitboard, SQ_LENGTH>& masks,
                                                    const std::array<uint32_t, SQ_LENGTH + 1>& offsets) {
    std::array<Magic, SQ_LENGTH> m{};
    for (unsigned s = 0; s < SQ_LENGTH; ++s)
      m[s] = Magic{masks[s], 0, 0, offsets[s]};
    return m;
  }

  inline constexpr auto rookMasks     = makeRookMasks();
  inline constexpr auto bishopMasks   = makeBishopMasks();
  inline constexpr auto rookOffsets   = makeOffsets(rookMasks);
  inline constexpr auto bishopOffsets = makeOffsets(bishopMasks);

  // Clion has a problem with static_assert on large constexpr arrays and shows these lines as errors,
  // so we disable these checks when building in the IDE
#ifndef __JETBRAINS_IDE__
  static_assert(rookOffsets.back() == 0x19000, "Unexpected rookTable size");
  static_assert(bishopOffsets.back() == 0x1480, "Unexpected bishopTable size");
#endif

  inline constexpr std::array<Magic, SQ_LENGTH> rookMagics   = makeMagics(rookMasks, rookOffsets);
  inline constexpr std::array<Magic, SQ_LENGTH> bishopMagics = makeMagics(bishopMasks, bishopOffsets);

  // Global tables (runtime-filled to avoid MSVC constexpr step limit)
  // Trying to also make these constexpr leads to "error C1061: compiler limit : blocks nested too deeply"
  inline Bitboard rookTable[0x19000];
  inline Bitboard bishopTable[0x1480];

  // Fill a table using precomputed masks+offsets; enumerate all subsets via carry-rippler
  inline void init_one(Bitboard table[], const std::array<Magic, SQ_LENGTH>& magics,
                       const Direction dirs[4]) {
    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
      const auto& m    = magics[s];
      const Bitboard M = m.mask;

      Bitboard b = 0;
      do {
        const unsigned idx    = static_cast<unsigned>(_pext_u64(b, M));
        table[m.offset + idx] = sliding_attack(dirs, s, b);
        b                     = b - M & M;
      } while (b);
    }
  }

  // Initialize both rook and bishop magic bitboards
  inline void initMagicBitboards() {
    init_one(rookTable, rookMagics, rookDirections);
    init_one(bishopTable, bishopMagics, bishopDirections);
  }

}// namespace Bitboards

// Attack lookup
// gets all attacks from non-pawn pieces on a given square considering the occupied squares
inline Bitboard getAttacksBb(const PieceType pt, const Square sq, const Bitboard occupied) {
  switch (pt) {
    case BISHOP: {
      const auto& m = Bitboards::bishopMagics[sq];
      return Bitboards::bishopTable[m.offset + m.index(occupied)];
    }
    case ROOK: {
      const auto& m = Bitboards::rookMagics[sq];
      return Bitboards::rookTable[m.offset + m.index(occupied)];
    }
    case QUEEN: {
      const auto& rb = Bitboards::bishopMagics[sq];
      const auto& rr = Bitboards::rookMagics[sq];
      return Bitboards::bishopTable[rb.offset + rb.index(occupied)]
             | Bitboards::rookTable[rr.offset + rr.index(occupied)];
    }
    case KNIGHT:
      [[fallthrough]];
    case KING:
      return Bitboards::nonSliderAttacks[pt][sq];
    default:
      return BbZero;
  }
}


#endif// FRANKYCPP_BITBOARD_H
