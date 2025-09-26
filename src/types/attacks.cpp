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

#include "attacks.h"

namespace Attacks {
  namespace detail {

    Bitboard RookTable[RookTableSize];
    Bitboard BishopTable[BishopTableSize];
    bool Initialized = false;

    // Compute sliding attack for a given occupancy (used during table fill)
    Bitboard sliding_attack(const Direction dirs[4], const Square sq, const Bitboard occupied) {
      Bitboard attack = 0;
      for (int i = 0; i < 4; ++i) {
        for (Square s = sq + dirs[i]; s.isValid() && s.distanceTo(s - dirs[i]) == 1; s += dirs[i]) {
          attack |= s;
          if (occupied & s) break;
        }
      }
      return attack;
    }

    void fill(Bitboard table[], const std::array<Magic, SQ_LENGTH>& magics, const Direction dirs[4]) {
      for (Square sq : Square::all()) {
        const Magic& m      = magics[sq];
        const Bitboard mask = m.mask;
        Bitboard subset     = 0;
        // Carry-rippler enumeration of all subsets of mask
        do {
          const unsigned idx    = m.index(subset);
          table[m.offset + idx] = sliding_attack(dirs, sq, subset);
          subset                = (subset - mask) & mask;
        } while (subset);
      }
    }

  }// namespace detail

  // Public API ---------------------------------------------------------

  void init() {
    if (detail::Initialized) return;
    detail::fill(detail::RookTable, detail::RookMagics, detail::RDirs);
    detail::fill(detail::BishopTable, detail::BishopMagics, detail::BDirs);
    detail::Initialized = true;
  }

  namespace {
    Bitboard sliderLookup(const PieceType pt, const Square sq, const Bitboard occ) {
      if (pt == ROOK) {
        const auto& m = detail::RookMagics[sq];
        return detail::RookTable[m.offset + m.index(occ)];
      }
      const auto& m = detail::BishopMagics[sq];
      return detail::BishopTable[m.offset + m.index(occ)];
    }
  }// namespace

  Bitboard attacks(const PieceType pt, const Square sq, const Bitboard occupied) {
    switch (pt) {
      case BISHOP:
        return sliderLookup(BISHOP, sq, occupied);
      case ROOK:
        return sliderLookup(ROOK, sq, occupied);
      case QUEEN:
        return sliderLookup(ROOK, sq, occupied) | sliderLookup(BISHOP, sq, occupied);
      case KNIGHT:
      case KING:
        return Bitboards::nonSliderAttacks[pt][sq];
      default:
        return BbZero;
    }
  }

}// namespace Attacks
