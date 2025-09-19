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
  "sliding_attack should yield 14 rook moves from A1 on an empty board");

// Initializes magic bitboards for all squares and both piece types
constexpr void init_magics(Bitboard table[], Magic magics[], const Direction directions[]) {
  size_t offset = 0;

  for (Square s = SQ_A1; s <= SQ_H8; ++s) {
    // Exclude board edges from relevant occupancy
    const Bitboard edges =
        ((Rank1BB | Rank8BB) & ~Bitboards::sqToRankBb[s]) |
        ((FileABB | FileHBB) & ~Bitboards::sqToFileBb[s]);

    Magic& m = magics[s];

    // Relevant blocker mask for this square
    m.mask   = sliding_attack(directions, s, 0) & ~edges;
    m.shift  = 0; // unused
    m.magic  = 0; // unused
    m.offset = static_cast<uint32_t>(offset);

    const unsigned bits  = static_cast<unsigned>(popcount(m.mask));
    const unsigned count = 1u << bits;

    // Enumerate all subsets of m.mask via carry-rippler
    Bitboard b = 0;
    do {
      const unsigned idx = static_cast<unsigned>(_pext_u64(b, m.mask));
      table[m.offset + idx] = sliding_attack(directions, s, b);
      b = (b - m.mask) & m.mask;
    } while (b);

    offset += count;
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
