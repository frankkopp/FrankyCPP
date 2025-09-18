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

#include "bitboard.h"

#include <bitset>
#include <sstream>

// //////////////////////////////////
// Bitboard functions
// //////////////////////////////////

std::string str(const Bitboard b) {
  std::ostringstream os;
  os << std::bitset<64>(b);
  return os.str();
}

std::string strBoard(const Bitboard b) {
  std::ostringstream os;
  os << "+---+---+---+---+---+---+---+---+\n";
  for (Rank r = RANK_8;; --r) {
    for (File f = FILE_A; f <= FILE_H; ++f) {
      os << ((b & squareOf(f, r)) ? "| X " : "|   ");
    }
    os << "|\n+---+---+---+---+---+---+---+---+\n";
    if (r == 0) break;
  }
  return os.str();
}

std::string strGrouped(const Bitboard b) {
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

// //////////////////////////////////
// Initialization amd pre-computing
// //////////////////////////////////

// Stockfish Magic bitboards - no need to reinvent the wheel
// Credits to Stockfish

constexpr Direction rookDirections[4]   = {NORTH, EAST, SOUTH, WEST};
constexpr Direction bishopDirections[4] = {NORTH_EAST, SOUTH_EAST, SOUTH_WEST, NORTH_WEST};

constexpr Bitboard sliding_attack(const Direction directions[], const Square sq, const Bitboard occupied) {
  Bitboard attack = 0;
  for (int i = 0; i < 4; ++i) {
    for (Square s = sq + directions[i];
         validSquare(s) && distance(s, s - directions[i]) == 1;
         s += directions[i]) {
      attack |= s;
      if (occupied & s)
        break;
    }
  }
  return attack;
}

// --- Compile-time sanity check ("proof") on empty board ---
// Rook from A1 must have 14 targets on an empty board (7 north + 7 east).
// Use your own popcount(Bitboard) if available; otherwise std::popcount (C++20).
static_assert(
  popcount(sliding_attack(rookDirections, SQ_A1, Bitboard{0})) == 14,
  "sliding_attack should yield 14 rook moves from A1 on an empty board"
);

void init_magics(Bitboard table[], Magic magics[], const Direction directions[]) {
  Bitboard occupancy[4096], reference[4096];
  int size = 0;

  for (Square s = SQ_A1; s <= SQ_H8; ++s) {

    // Board edges are not considered in the relevant occupancies
    const Bitboard edges = ((Rank1BB | Rank8BB) & ~Bitboards::sqToRankBb[s]) | ((FileABB | FileHBB) & ~Bitboards::sqToFileBb[s]);

    // Given a square 's', the mask is the bitboard of sliding attacks from
    // 's' computed on an empty board. The index must be big enough to contain
    // all the attacks for each possible subset of the mask and so is 2 power
    // the number of 1s of the mask. Hence we deduce the size of the shift to
    // apply to the 64 or 32 bits word to get the index.
    Magic& m = magics[s];
    m.mask   = sliding_attack(directions, s, 0) & ~edges;
    m.shift  = 64 - popcount(m.mask);

    // Set the offset for the attacks table of the square. We have individual
    // table sizes for each square with "Fancy Magic Bitboards".
    m.attacks = s == SQ_A1 ? table : magics[s - 1].attacks + size;

    // Use Carry-Rippler trick to enumerate all subsets of masks[s] and
    // store the corresponding sliding attack bitboard in reference[].
    Bitboard b = size = 0;
    do {
      occupancy[size] = b;
      reference[size] = sliding_attack(directions, s, b);

#ifdef HAS_PEXT// to be set as compiler option
      m.attacks[_pext_u64(b, m.mask)] = reference[size];
#endif

      size++;
      b = (b - m.mask) & m.mask;
    } while (b);

#ifndef HAS_PEXT
    // Manual mapping for magics when PEXT is not available

    // Optimal PRNG seeds to pick the correct magics in the shortest time
    const int seeds[RANK_LENGTH] = {728, 10316, 55013, 32803, 12281, 15100, 16645, 255};
    int epoch[4096] = {}, cnt = 0;
    PRNG rng(seeds[rankOf(s)]);

    // Find a magic for square 's' picking up an (almost) random number
    // until we find the one that passes the verification test.
    for (int i = 0; i < size;) {
      for (m.magic = 0; popcount((m.magic * m.mask) >> 56) < 6;)
        m.magic = rng.sparse_rand<Bitboard>();

      // A good magic must map every possible occupancy to an index that
      // looks up the correct sliding attack in the attacks[s] database.
      // Note that we build up the database for square 's' as a side
      // effect of verifying the magic. Keep track of the attempt count
      // and save it in epoch[], little speed-up trick to avoid resetting
      // m.attacks[] after every failed attempt.
      for (++cnt, i = 0; i < size; ++i) {
        unsigned idx = m.index(occupancy[i]);
        if (epoch[idx] < cnt) {
          epoch[idx]     = cnt;
          m.attacks[idx] = reference[i];
        }
        else if (m.attacks[idx] != reference[i])
          break;
      }
    }
#endif
  }
}

// init_magics() computes all rook and bishop attacks at startup. Magic
// bitboards are used to look up attacks of sliding pieces. As a reference see
// www.chessprogramming.org/Magic_Bitboards. In particular, here we use the so
// called "fancy" approach.
// Credits to Stockfish
void Bitboards::initMagicBitboards() {
  init_magics(rookTable, rookMagics, rookDirections);
  init_magics(bishopTable, bishopMagics, bishopDirections);
}
