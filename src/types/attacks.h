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

// This is a refactored version of the original header which mixed data, helpers
// and public API. The public interface is now just Attacks::init() and
// Attacks::attacks(). Legacy functions getAttacksBb() and initMagicBitboards()
// remain as thin deprecated wrappers for existing call sites and tests.
//
// Design goals:
//  - Single obvious public API (Attacks::init, Attacks::attacks)
//  - Hide tables and generation details in an internal namespace (detail)
//  - Keep compile-time generation of masks/offsets; runtime fill of large tables
//  - PEXT-only path (HAS_PEXT required); unused magic/shift fields removed
//  - Idempotent initialization
//
#ifndef FRANKYCPP_ATTACKS_H
#define FRANKYCPP_ATTACKS_H

#include "bitboard.h"
#include "bitboards.h"
#include "orientation.h"

#include <array>

#ifdef HAS_PEXT
#include <immintrin.h>
#endif

// Attacks namespace contains functionality for fast lookup of sliding piece attacks
// using magic bitboards with PEXT (parallel bits extract) instruction if available.
// It provides initialization and attack lookup functions.
namespace Attacks {
  namespace detail {

    // ------------------------------------------------------------
    // constexpr helpers
    // ------------------------------------------------------------
    constexpr unsigned popcount_ce(Bitboard b) {
      unsigned c = 0;
      while (b) {
        b &= b - static_cast<Bitboard>(1);
        ++c;
      }
      return c;
    }

    // constexpr software pext (bit compress) for constant evaluation
    constexpr uint64_t pext_soft(const uint64_t src, uint64_t mask) {
      uint64_t res = 0, bit = 1;
      while (mask) {
        const uint64_t lsb = mask & -mask;
        if (src & lsb) res |= bit;
        mask ^= lsb;
        bit <<= 1;
      }
      return res;
    }

    struct Magic {
      Bitboard mask{};
      uint32_t offset{};// start index into global attack table
      [[nodiscard]] constexpr unsigned index(const Bitboard occupied) const {
#ifdef HAS_PEXT
        if (!std::is_constant_evaluated())
          return static_cast<unsigned>(_pext_u64(occupied, mask));
#endif
        return static_cast<unsigned>(pext_soft(occupied, mask));
      }
    };

    // Directions
    constexpr Direction RDirs[4] = {NORTH, EAST, SOUTH, WEST};
    constexpr Direction BDirs[4] = {NORTH_EAST, SOUTH_EAST, SOUTH_WEST, NORTH_WEST};

    // Edge / mask helpers
    constexpr Bitboard edgeMaskFor(const unsigned s) {
      return (Rank1BB | Rank8BB) & ~Bitboards::sqToRankBb[s] | ((FileABB | FileHBB) & ~Bitboards::sqToFileBb[s]);
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
        off[s + 1] = off[s] + (1u << popcount_ce(masks[s]));
      }
      return off;
    }
    constexpr std::array<Magic, SQ_LENGTH> makeMagics(const std::array<Bitboard, SQ_LENGTH>& masks,
                                                      const std::array<uint32_t, SQ_LENGTH + 1>& offsets) {
      std::array<Magic, SQ_LENGTH> m{};
      for (unsigned s = 0; s < SQ_LENGTH; ++s)
        m[s] = Magic{masks[s], offsets[s]};
      return m;
    }

    inline constexpr auto RookMasks     = makeRookMasks();
    inline constexpr auto BishopMasks   = makeBishopMasks();
    inline constexpr auto RookOffsets   = makeOffsets(RookMasks);
    inline constexpr auto BishopOffsets = makeOffsets(BishopMasks);

#ifndef __JETBRAINS_IDE__
    static_assert(RookOffsets.back() == 0x19000, "Unexpected rook table size");
    static_assert(BishopOffsets.back() == 0x1480, "Unexpected bishop table size");
#endif

    inline constexpr auto RookMagics   = makeMagics(RookMasks, RookOffsets);
    inline constexpr auto BishopMagics = makeMagics(BishopMasks, BishopOffsets);

    // Table size constants
    inline constexpr size_t RookTableSize   = 0x19000;
    inline constexpr size_t BishopTableSize = 0x1480;

    // Runtime attack tables (storage in attacks.cpp)
    extern Bitboard RookTable[RookTableSize];
    extern Bitboard BishopTable[BishopTableSize];
    extern bool Initialized;

    // Internal generation (implemented in attacks.cpp)
    void fill(Bitboard table[], const std::array<Magic, SQ_LENGTH>& magics, const Direction dirs[4]);
    Bitboard sliding_attack(const Direction dirs[4], Square sq, Bitboard occupied);

  }// namespace detail

  // ------------------------------------------------------------
  // Public API
  // ------------------------------------------------------------

  // Initialize (idempotent)
  void init();

  // Unified attack lookup for non-pawn pieces (QUEEN = ROOK | BISHOP)
  Bitboard attacks(PieceType pt, Square sq, Bitboard occupied);

}// namespace Attacks


#endif// FRANKYCPP_ATTACKS_H
