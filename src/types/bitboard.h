/*
 * MIT License
 *
 * Copyright (c) 2020 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef FRANKYCPP_BITBOARD_H
#define FRANKYCPP_BITBOARD_H

#include "castlingrights.h"
#include "direction.h"
#include "orientation.h"
#include "piecetype.h"
#include "square.h"
#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
#include <iostream>
#endif

// PEXT BMI2 - add -mbmi2 to compiler options
#include <immintrin.h>
constexpr bool HasPext = false;

// 64 bit Bitboard type for storing boards as bits
typedef uint64_t Bitboard;

// //////////////////////////////////////////////////////////////////
// Bitboard constants

constexpr Bitboard BbZero = Bitboard(0);
constexpr Bitboard BbFull = ~BbZero;
constexpr Bitboard BbOne  = Bitboard(1);

constexpr Bitboard FileABB = 0x0101010101010101ULL;
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;
constexpr Bitboard FileHBB = FileABB << 7;

constexpr Bitboard Rank1BB = 0xFF;
constexpr Bitboard Rank2BB = Rank1BB << (8 * 1);
constexpr Bitboard Rank3BB = Rank1BB << (8 * 2);
constexpr Bitboard Rank4BB = Rank1BB << (8 * 3);
constexpr Bitboard Rank5BB = Rank1BB << (8 * 4);
constexpr Bitboard Rank6BB = Rank1BB << (8 * 5);
constexpr Bitboard Rank7BB = Rank1BB << (8 * 6);
constexpr Bitboard Rank8BB = Rank1BB << (8 * 7);

constexpr Bitboard DiagUpA1 = 0b1000000001000000001000000001000000001000000001000000001000000001;
constexpr Bitboard DiagUpB1 = (DiagUpA1 << 1) & ~FileABB;// shift EAST
constexpr Bitboard DiagUpC1 = (DiagUpB1 << 1) & ~FileABB;
constexpr Bitboard DiagUpD1 = (DiagUpC1 << 1) & ~FileABB;
constexpr Bitboard DiagUpE1 = (DiagUpD1 << 1) & ~FileABB;
constexpr Bitboard DiagUpF1 = (DiagUpE1 << 1) & ~FileABB;
constexpr Bitboard DiagUpG1 = (DiagUpF1 << 1) & ~FileABB;
constexpr Bitboard DiagUpH1 = (DiagUpG1 << 1) & ~FileABB;
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
constexpr Bitboard DiagDownG1 = (DiagDownH1 >> 1) & ~FileHBB;// shift WEST
constexpr Bitboard DiagDownF1 = (DiagDownG1 >> 1) & ~FileHBB;
constexpr Bitboard DiagDownE1 = (DiagDownF1 >> 1) & ~FileHBB;
constexpr Bitboard DiagDownD1 = (DiagDownE1 >> 1) & ~FileHBB;
constexpr Bitboard DiagDownC1 = (DiagDownD1 >> 1) & ~FileHBB;
constexpr Bitboard DiagDownB1 = (DiagDownC1 >> 1) & ~FileHBB;
constexpr Bitboard DiagDownA1 = (DiagDownB1 >> 1) & ~FileHBB;

constexpr Bitboard CENTER_FILES   = FileDBB | FileEBB;
constexpr Bitboard CENTER_RANKS   = Rank4BB | Rank5BB;
constexpr Bitboard CENTER_SQUARES = CENTER_FILES & CENTER_RANKS;

// pre computed arrays for various bitboards
// pre-computed in types::init()
namespace Bitboards {
  inline Bitboard sqBb[SQ_LENGTH];
  inline Bitboard fileBb[FILE_LENGTH];
  inline Bitboard rankBb[RANK_LENGTH];
  inline Bitboard sqToFileBb[SQ_LENGTH];
  inline Bitboard sqToRankBb[SQ_LENGTH];
  inline Bitboard squareDiagUpBb[SQ_LENGTH];
  inline Bitboard squareDiagDownBb[SQ_LENGTH];
  inline Bitboard pawnAttacks[COLOR_LENGTH][SQ_LENGTH];
  inline Bitboard nonSliderAttacks[PT_LENGTH][SQ_LENGTH];
  inline Bitboard filesWestMask[SQ_LENGTH];
  inline Bitboard filesEastMask[SQ_LENGTH];
  inline Bitboard ranksNorthMask[SQ_LENGTH];
  inline Bitboard ranksSouthMask[SQ_LENGTH];
  inline Bitboard fileWestMask[SQ_LENGTH];
  inline Bitboard fileEastMask[SQ_LENGTH];
  inline Bitboard neighbourFilesMask[SQ_LENGTH];
  inline Bitboard rays[OR_LENGTH][SQ_LENGTH];
  inline Bitboard intermediateBb[SQ_LENGTH][SQ_LENGTH];
  inline Bitboard passedPawnMask[COLOR_LENGTH][SQ_LENGTH];
  inline Bitboard kingSideCastleMask[COLOR_LENGTH];
  inline Bitboard queenSideCastleMask[COLOR_LENGTH];
  inline Bitboard colorBb[COLOR_LENGTH];
}// namespace Bitboards

// //////////////////////////////////////////////////////////////////
// Bitboard functions

// get all attacks from non pawn pieces
Bitboard getAttacksBb(PieceType pt, Square sq, Bitboard occupied);

/**
* Shifts a bitboard in the given direction.
* @param d Direction
* @param b Bitboard
* @return shifted bitboard
*/
inline Bitboard shiftBb(Direction d, Bitboard b) {
  // move the bits and clear the left our right file
  // after the shift to erase bit jumping over
  switch (d) {
    case NORTH:
      return (b << 8);
    case EAST:
      return (b << 1) & ~FileABB;
    case SOUTH:
      return (b >> 8);
    case WEST:
      return (b >> 1) & ~FileHBB;
    case NORTH_EAST:
      return (b << 9) & ~FileABB;
    case SOUTH_EAST:
      return (b >> 7) & ~FileABB;
    case SOUTH_WEST:
      return (b >> 9) & ~FileHBB;
    case NORTH_WEST:
      return (b << 7) & ~FileHBB;
  }
  return b;
}

// popcount() counts the number of non-zero bits in a bitboard
inline int popcount(Bitboard b) {
#if defined(__GNUC__)// GCC, Clang, ICC
  return __builtin_popcountll(b);
#elif defined(_MSC_VER)
  return static_cast<int>(__popcnt64(b));
#else// Compiler is not GCC
  // pre-computed table of population counter for 16-bit
  extern uint8_t PopCnt16[1 << 16];
  // nice trick to address 16-bit groups in a 64-bit int
  // @formatter:off
  union {
    Bitboard bb;
    uint16_t u[4];
  } v = {b};
  // @formatter:on
  // adding all 16-bit population counters referenced in the 64-bit union
  return PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]];
#endif
}

// Used when no build-in popcount is available for compiler.
// @return popcount16() counts the non-zero bits using SWAR-Popcount algorithm
inline unsigned popcount16(unsigned u) {
  u -= (u >> 1U) & 0x5555U;
  u = ((u >> 2U) & 0x3333U) + (u & 0x3333U);
  u = ((u >> 4U) + u) & 0x0F0FU;
  return (u * 0x0101U) >> 8U;
}

// lsb() and msb() return the least/most significant bit in a non-zero
// bitboard
inline Square lsb(Bitboard b) {
  if (!b) return SQ_NONE;
#ifdef __GNUC__ // GCC, Clang, ICC
  return static_cast<Square>(__builtin_ctzll(b));
#elif defined(_MSC_VER)
  unsigned long index;
    if (_BitScanForward64(&index, b)) {
      return static_cast<Square>(index);
    }
    else {
      return SQ_NONE;
    }
#else // Compiler is not GCC
#error "Compiler not yet supported."
#endif
}

// lsb() and msb() return the least/most significant bit in a non-zero
// bitboard
inline Square msb(Bitboard b) {
  if (!b) return SQ_NONE;
#if defined(__GNUC__) // GCC, Clang, ICC
  return static_cast<Square>(63 ^ __builtin_clzll(b));
#elif defined(_MSC_VER)
  unsigned long index;
    if (_BitScanReverse64(&index, b)) {
      return static_cast<Square>(index);
  }
    else {
      return SQ_NONE;
    }
#else // Compiler is not GCC
#error "Compiler not yet supported."
#endif
}

// pop_lsb() finds and clears the least significant bit in a non-zero
// bitboard
inline Square popLSB(Bitboard &b) {
  if (!b) return SQ_NONE;
  const Square s = lsb(b);
  b &= b - 1;
  return s;
}

// //////////////////////////////////////////////////////////////////
// Bitboard operators

// Operators for Squares as Bitboards
inline Bitboard operator&(const Square lhs, const Square rhs) {
  return Bitboards::sqBb[lhs] & Bitboards::sqBb[rhs];
}

inline Bitboard operator|(const Square lhs, const Square rhs) {
  return Bitboards::sqBb[lhs] | Bitboards::sqBb[rhs];
}

// Operators for Squares on Bitboards
inline Bitboard operator&(const Bitboard b, const Square s) {
  return b & Bitboards::sqBb[s];
}

inline Bitboard operator|(const Bitboard b, const Square s) {
  return b | Bitboards::sqBb[s];
}

inline Bitboard operator^(const Bitboard b, const Square s) {
  return b ^ Bitboards::sqBb[s];
}

inline Bitboard &operator|=(Bitboard &b, const Square s) {
  return b |= Bitboards::sqBb[s];
}

inline Bitboard &operator^=(Bitboard &b, const Square s) {
  return b ^= Bitboards::sqBb[s];
}

// //////////////////////////////////////////////////////////////////
// Bitboard print functions

// Prints a bitboard as a bitset
std::string str(Bitboard b);

// Prints a bitboard in an 8x8 matrix for output on a console
std::string strBoard(Bitboard b);

// StringGrouped returns a string representation of the 64 bits grouped in 8.
// Order is LSB to msb ==> A1 B1 ... G8 H8
std::string strGrouped(Bitboard b);

// //////////////////////////////////////////////////////////////////
// Bitboard initialization and pre-computation

namespace Bitboards {
  void rankFileBbPreCompute();
  void squareBitboardsPreCompute();
  void nonSlidingAttacksPreCompute();
  void initMagicBitboards();
  void neighbourMasksPreCompute();
  void raysPreCompute();
  void intermediatePreCompute();
  void maskPassedPawnsPreCompute();
  void castleMasksPreCompute();
  void colorBitboardsPreCompute();
}

// //////////////////////////////////////////////////////////////////
// Magic bitboards

// Magic holds all magic bitboards relevant for a single square
// Taken from Stockfish
// License see https://stockfishchess.org/about/
struct Magic {
  Bitboard  mask;
  Bitboard  magic;
  Bitboard* attacks;
  unsigned  shift;

  // Compute the attack's index using the 'magic bitboards' approach
  inline unsigned index(Bitboard occupied) const {
    if (HasPext) {
      return unsigned(_pext_u64(occupied, mask));
    }
    return unsigned(((occupied & mask) * magic) >> shift);
  }
};

namespace Bitboards {
  inline Bitboard rookTable[0x19000];  // To store rook attacks
  inline Bitboard bishopTable[0x1480]; // To store bishop attacks
  inline Magic rookMagics[SQ_LENGTH];
  inline Magic bishopMagics[SQ_LENGTH];
}

/// xorshift64star Pseudo-Random Number Generator
/// This class is based on original code written and dedicated
/// to the public domain by Sebastiano Vigna (2014).
/// It has the following characteristics:
///
///  -  Outputs 64-bit numbers
///  -  Passes Dieharder and SmallCrush test batteries
///  -  Does not require warm-up, no zeroland to escape
///  -  Internal state is a single 64-bit integer
///  -  Period is 2^64 - 1
///  -  Speed: 1.60 ns/call (Core i7 @3.40GHz)
///
/// For further analysis see
///   <http://vigna.di.unimi.it/ftp/papers/xorshift.pdf>
class PRNG {
  uint64_t s;
  uint64_t rand64() {
    s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
    return s * 2685821657736338717LL;
  }
public:
  PRNG(uint64_t seed) : s(seed) { assert(seed); }
  template<typename T> T rand() { return T(rand64()); }
  // Special generator used to fast init magic numbers.
  // Output values only have 1/8th of their bits set on average.
  template<typename T> T sparse_rand() { return T(rand64() & rand64() & rand64()); }
};

#endif//FRANKYCPP_BITBOARD_H
