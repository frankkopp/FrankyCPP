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

#ifndef FRANKYCPP_MAGICS_H
#define FRANKYCPP_MAGICS_H

#include "bitboard.h"

#ifndef HAS_PEXT
#error "PEXT-only path requested, but HAS_PEXT is not defined. Enable BMI2 (/arch:AVX2 or -mbmi2) and add -DHAS_PEXT."
#endif

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

constexpr Bitboard edgeMaskFor(const unsigned s) {
  return (Rank1BB | Rank8BB) & ~Bitboards::sqToRankBb[s] | (FileABB | FileHBB) & ~Bitboards::sqToFileBb[s];
}

constexpr Bitboard rookMaskFor(const unsigned s) {
  return (Bitboards::rays[N][s] | Bitboards::rays[S][s] | Bitboards::rays[E][s] | Bitboards::rays[W][s]) & ~edgeMaskFor(s);
}

constexpr Bitboard bishopMaskFor(const unsigned s) {
  return (Bitboards::rays[NE][s] | Bitboards::rays[NW][s] | Bitboards::rays[SE][s] | Bitboards::rays[SW][s]) & ~edgeMaskFor(s);
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
    off[s + 1]          = off[s] + (1u << bits);
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

// Fill a table using precomputed masks+offsets; list all subsets via carry-rippler
inline void init_one(Bitboard table[], const std::array<Magic, SQ_LENGTH>& magics,
                     const Direction dirs[4]) {
  for (Square s : Square::all()) {
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

// Attack lookup
// gets all attacks from non-pawn pieces on a given square considering the occupied squares
inline Bitboard getAttacksBb(const PieceType pt, const Square sq, const Bitboard occupied) {
  switch (pt) {
    case BISHOP: {
      const auto& m = bishopMagics[sq];
      return bishopTable[m.offset + m.index(occupied)];
    }
    case ROOK: {
      const auto& m = rookMagics[sq];
      return rookTable[m.offset + m.index(occupied)];
    }
    case QUEEN: {
      const auto& rb = bishopMagics[sq];
      const auto& rr = rookMagics[sq];
      return bishopTable[rb.offset + rb.index(occupied)]
             | rookTable[rr.offset + rr.index(occupied)];
    }
    case KNIGHT:
      [[fallthrough]];
    case KING:
      return Bitboards::nonSliderAttacks[pt][sq];
    default:
      return BbZero;
  }
}

#endif// FRANKYCPP_MAGICS_H
