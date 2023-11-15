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

Bitboard getAttacksBb(const PieceType pt, const Square sq, const Bitboard occupied) {
  switch (pt) {
    case BISHOP:
      return Bitboards::bishopMagics[sq].attacks[Bitboards::bishopMagics[sq].index(occupied)];
    case ROOK:
      return Bitboards::rookMagics[sq].attacks[Bitboards::rookMagics[sq].index(occupied)];
    case QUEEN:
      return Bitboards::bishopMagics[sq].attacks[Bitboards::bishopMagics[sq].index(occupied)] | Bitboards::rookMagics[sq].attacks[Bitboards::rookMagics[sq].index(occupied)];
    case KNIGHT:
      [[fallthrough]];
    case KING:
      return Bitboards::nonSliderAttacks[pt][sq];
    default:
      return BbZero;
  }
}

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

void Bitboards::rankFileBbPreCompute() {
  for (unsigned int i = RANK_1; i <= RANK_8; i++) {
    rankBb[i] = Rank1BB << (8 * i);
  }
  for (unsigned int i = FILE_A; i <= FILE_H; i++) {
    fileBb[i] = FileABB << i;
  }
}

void Bitboards::squareBitboardsPreCompute() {
  for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {

    // square bitboard
    // (cast to make clear that sq never is negative)
    sqBb[sq] = BbOne << static_cast<unsigned int>(sq);

    // file and rank bitboards
    sqToFileBb[sq] = fileBb[fileOf(sq)];
    sqToRankBb[sq] = rankBb[rankOf(sq)];

    // square diagonals
    // @formatter:off
    // clang-format off
    if (DiagUpA8 & sq) squareDiagUpBb[sq] = DiagUpA8;
    else if (DiagUpA7 & sq) squareDiagUpBb[sq] = DiagUpA7;
    else if (DiagUpA6 & sq) squareDiagUpBb[sq] = DiagUpA6;
    else if (DiagUpA5 & sq) squareDiagUpBb[sq] = DiagUpA5;
    else if (DiagUpA4 & sq) squareDiagUpBb[sq] = DiagUpA4;
    else if (DiagUpA3 & sq) squareDiagUpBb[sq] = DiagUpA3;
    else if (DiagUpA2 & sq) squareDiagUpBb[sq] = DiagUpA2;
    else if (DiagUpA1 & sq) squareDiagUpBb[sq] = DiagUpA1;
    else if (DiagUpB1 & sq) squareDiagUpBb[sq] = DiagUpB1;
    else if (DiagUpC1 & sq) squareDiagUpBb[sq] = DiagUpC1;
    else if (DiagUpD1 & sq) squareDiagUpBb[sq] = DiagUpD1;
    else if (DiagUpE1 & sq) squareDiagUpBb[sq] = DiagUpE1;
    else if (DiagUpF1 & sq) squareDiagUpBb[sq] = DiagUpF1;
    else if (DiagUpG1 & sq) squareDiagUpBb[sq] = DiagUpG1;
    else if (DiagUpH1 & sq) squareDiagUpBb[sq] = DiagUpH1;

    if (DiagDownH8 & sq) squareDiagDownBb[sq] = DiagDownH8;
    else if (DiagDownH7 & sq) squareDiagDownBb[sq] = DiagDownH7;
    else if (DiagDownH6 & sq) squareDiagDownBb[sq] = DiagDownH6;
    else if (DiagDownH5 & sq) squareDiagDownBb[sq] = DiagDownH5;
    else if (DiagDownH4 & sq) squareDiagDownBb[sq] = DiagDownH4;
    else if (DiagDownH3 & sq) squareDiagDownBb[sq] = DiagDownH3;
    else if (DiagDownH2 & sq) squareDiagDownBb[sq] = DiagDownH2;
    else if (DiagDownH1 & sq) squareDiagDownBb[sq] = DiagDownH1;
    else if (DiagDownG1 & sq) squareDiagDownBb[sq] = DiagDownG1;
    else if (DiagDownF1 & sq) squareDiagDownBb[sq] = DiagDownF1;
    else if (DiagDownE1 & sq) squareDiagDownBb[sq] = DiagDownE1;
    else if (DiagDownD1 & sq) squareDiagDownBb[sq] = DiagDownD1;
    else if (DiagDownC1 & sq) squareDiagDownBb[sq] = DiagDownC1;
    else if (DiagDownB1 & sq) squareDiagDownBb[sq] = DiagDownB1;
    else if (DiagDownA1 & sq) squareDiagDownBb[sq] = DiagDownA1;
    // @formatter:on
    // clang-format on
  }
}

void Bitboards::nonSlidingAttacksPreCompute() {
  // @formatter:off
  // steps for kings, pawns, knight for WHITE - negate to get BLACK
  int steps[][5] = {{},
                    {NORTH_WEST, NORTH, NORTH_EAST, EAST},// king
                    {NORTH_WEST, NORTH_EAST},             // pawn
                    {WEST + NORTH_WEST,                   // knight
                     EAST + NORTH_EAST,
                     NORTH + NORTH_WEST,
                     NORTH + NORTH_EAST}};
  // @formatter:on

  // knight and king attacks
  for (Color c = WHITE; c <= BLACK; ++c) {
    for (const PieceType pt : {KING, PAWN, KNIGHT}) {
      for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        for (int i = 0; steps[pt][i]; ++i) {
          const Square to = s + static_cast<Direction>(c == WHITE ? steps[pt][i] : -steps[pt][i]);
          if (validSquare(to) && distance(s, to) < 3) {
            if (pt == PAWN) { pawnAttacks[c][s] |= to; }
            else { nonSliderAttacks[pt][s] |= to; }
          }
        }
      }
    }
  }
}

void Bitboards::neighbourMasksPreCompute() {
  for (Square square = SQ_A1; square <= SQ_H8; ++square) {
    const auto f = static_cast<unsigned int>(fileOf(square));
    const auto r = static_cast<unsigned int>(rankOf(square));
    for (unsigned int j = 0; j <= 7; j++) {
      // file masks
      if (j < f) {
        filesWestMask[square] |= FileABB << j;
      }
      if (7 - j > f) {
        filesEastMask[square] |= FileABB << (7 - j);
      }
      // rank masks
      if (7 - j > r) {
        ranksNorthMask[square] |= Rank1BB << (8 * (7 - j));
      }
      if (j < r) {
        ranksSouthMask[square] |= Rank1BB << (8 * j);
      }
    }
    if (f > 0) {
      fileWestMask[square] = FileABB << (f - 1);
    }
    if (f < 7) {
      fileEastMask[square] = FileABB << (f + 1);
    }
    neighbourFilesMask[square] = fileEastMask[square] | fileWestMask[square];
  }
}

void Bitboards::raysPreCompute() {
  for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {
    rays[N][sq]  = getAttacksBb(ROOK, sq, BbZero) & ranksNorthMask[sq];
    rays[E][sq]  = getAttacksBb(ROOK, sq, BbZero) & filesEastMask[sq];
    rays[S][sq]  = getAttacksBb(ROOK, sq, BbZero) & ranksSouthMask[sq];
    rays[W][sq]  = getAttacksBb(ROOK, sq, BbZero) & filesWestMask[sq];
    rays[NW][sq] = getAttacksBb(BISHOP, sq, BbZero) & filesWestMask[sq] & ranksNorthMask[sq];
    rays[NE][sq] = getAttacksBb(BISHOP, sq, BbZero) & filesEastMask[sq] & ranksNorthMask[sq];
    rays[SE][sq] = getAttacksBb(BISHOP, sq, BbZero) & filesEastMask[sq] & ranksSouthMask[sq];
    rays[SW][sq] = getAttacksBb(BISHOP, sq, BbZero) & filesWestMask[sq] & ranksSouthMask[sq];
  }
}

void Bitboards::intermediatePreCompute() {
  for (Square from = SQ_A1; from <= SQ_H8; ++from) {
    for (Square to = SQ_A1; to <= SQ_H8; ++to) {
      const Bitboard toBB = sqBb[to];
      for (int o = 0; o < 8; o++) {
        if ((rays[o][from] & toBB) != BbZero) {
          intermediateBb[from][to] |= rays[Orientation(o)][from] & ~rays[o][to] & ~toBB;
        }
      }
    }
  }
}

void Bitboards::maskPassedPawnsPreCompute() {
  for (Square square = SQ_A1; square <= SQ_H8; ++square) {
    const int f = int(fileOf(square));
    const int r = int(rankOf(square));
    // white pawn - ignore that pawns can't be on all squares
    passedPawnMask[WHITE][square] |= rays[N][square];
    if (f < 7 && r < 7) {
      passedPawnMask[WHITE][square] |= rays[N][square + EAST];
    }
    if (f > 0 && r < 7) {
      passedPawnMask[WHITE][square] |= rays[N][square + WEST];
    }
    // black pawn - ignore that pawns can'*t be on all squares
    passedPawnMask[BLACK][square] |= rays[S][square];
    if (f < 7 && r > 0) {
      passedPawnMask[BLACK][square] |= rays[S][square + EAST];
    }
    if (f > 0 && r > 0) {
      passedPawnMask[BLACK][square] |= rays[S][square + WEST];
    }
  }
}

void Bitboards::castleMasksPreCompute() {
  // castle masks
  kingSideCastleMask[WHITE]  = sqBb[SQ_F1] | sqBb[SQ_G1] | sqBb[SQ_H1];
  kingSideCastleMask[BLACK]  = sqBb[SQ_F8] | sqBb[SQ_G8] | sqBb[SQ_H8];
  queenSideCastleMask[WHITE] = sqBb[SQ_D1] | sqBb[SQ_C1] | sqBb[SQ_B1] | sqBb[SQ_A1];
  queenSideCastleMask[BLACK] = sqBb[SQ_D8] | sqBb[SQ_C8] | sqBb[SQ_B8] | sqBb[SQ_A8];
}

void Bitboards::colorBitboardsPreCompute() {
  for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {
    if ((static_cast<int>(fileOf(sq)) + static_cast<int>(rankOf(sq))) % 2 == 0) {
      colorBb[BLACK] = colorBb[BLACK] | sq;
    }
    else {
      colorBb[WHITE] = colorBb[WHITE] | Bitboards::sqBb[sq];// squaresBb[WHITE] | sq;
    }
  }
}

// Stockfish Magic bitboards - no need to reinvent the wheel
// Credits to Stockfish

Bitboard sliding_attack(Direction directions[], const Square sq, const Bitboard occupied) {
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

void init_magics(Bitboard table[], Magic magics[], Direction directions[]) {

  Bitboard occupancy[4096], reference[4096];
  int size = 0;

  for (Square s = SQ_A1; s <= SQ_H8; ++s) {
    Bitboard edges, b;

    // Board edges are not considered in the relevant occupancies
    edges = ((Rank1BB | Rank8BB) & ~Bitboards::sqToRankBb[s]) | ((FileABB | FileHBB) & ~Bitboards::sqToFileBb[s]);

    // Given a square 's', the mask is the bitboard of sliding attacks from
    // 's' computed on an empty board. The index must be big enough to contain
    // all the attacks for each possible subset of the mask and so is 2 power
    // the number of 1s of the mask. Hence we deduce the size of the shift to
    // apply to the 64 or 32 bits word to get the index.
    Magic& m   = magics[s];
    m.mask     = sliding_attack(directions, s, 0) & ~edges;
    m.shift    = 64 - popcount(m.mask);

    // Set the offset for the attacks table of the square. We have individual
    // table sizes for each square with "Fancy Magic Bitboards".
    m.attacks  = s == SQ_A1 ? table : magics[s - 1].attacks + size;

    // Use Carry-Rippler trick to enumerate all subsets of masks[s] and
    // store the corresponding sliding attack bitboard in reference[].
    b = size = 0;
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
  Direction rookDirections[]   = {NORTH, EAST, SOUTH, WEST};
  Direction bishopDirections[] = {NORTH_EAST, SOUTH_EAST, SOUTH_WEST, NORTH_WEST};
  init_magics(rookTable, rookMagics, rookDirections);
  init_magics(bishopTable, bishopMagics, bishopDirections);
}
