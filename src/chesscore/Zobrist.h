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

#ifndef FRANKYCPP_ZOBRIST_H
#define FRANKYCPP_ZOBRIST_H

//=============================================================================
// Zobrist.h - Zobrist Hashing Tables and PRNG
//=============================================================================
//
// Provides Zobrist hash keys for position identification in the transposition
// table and repetition detection. Keys are pseudo-random 64-bit values
// generated at compile time.
// Depends on: types.h
//
// Zobrist Hashing:
//   Each position component (piece placement, castling, en passant, side)
//   has a unique random key. The position hash is computed by XORing all
//   applicable keys together. XOR allows incremental updates: toggling a
//   component twice returns to the original hash.
//
// Tables (in Zobrist:: namespace):
//   pieces[piece][square]       - Key for each piece on each square
//   castlingRights[cr]          - Key for each castling rights combination
//   enPassantFile[file]         - Key for en passant on each file
//   nextPlayer                  - Key XORed when Black to move
//
// PRNG:
//   Uses xorshift64* algorithm for high-quality pseudo-random numbers.
//   Algorithm by Sebastiano Vigna (2014), based on George Marsaglia's
//   xorshift generators (2003). Used for Zobrist table generation and
//   magic bitboard initialization.
//
// Usage:
//   ZobristKey key = 0;
//   key ^= Zobrist::pieces[WHITE_PAWN][SQ_E4];  // Add white pawn on e4
//   key ^= Zobrist::nextPlayer;                  // Toggle side to move
//   key ^= Zobrist::castlingRights[WHITE_OO];    // Add castling right
//
//=============================================================================

#include "types/types.h"

#include <array>
#include <cassert>
#include <cstdint>

/// Performs one step of xorshift64* PRNG algorithm.
/// Algorithm by Sebastiano Vigna (2014), based on Marsaglia's xorshift (2003).
/// Produces high-quality pseudo-random 64-bit values.
/// @param s  PRNG state (modified in place)
/// @return   Next pseudo-random value
constexpr uint64_t xorshift64star_step(uint64_t& s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 2685821657736338717ULL;
}

/// Pseudo-Random Number Generator using xorshift64* algorithm.
/// Algorithm by Sebastiano Vigna (2014). Used for magic number generation
/// and Zobrist key initialization.
class PRNG {
  uint64_t s;

  uint64_t rand64() {
    return xorshift64star_step(s);
  }

public:
  /// Creates a PRNG with the given seed.
  /// @param seed  Initial state (must be non-zero)
  explicit PRNG(const uint64_t seed) : s(seed) { assert(seed); }

  /// Generates a random value of type T.
  /// @tparam T  Return type (typically uint64_t or Bitboard)
  /// @return    Random value
  template <typename T>
  T rand() { return T(rand64()); }

  /// Generates a sparse random value with ~1/8 of bits set on average.
  /// Used for fast magic number initialization.
  /// @tparam T  Return type
  /// @return    Sparse random value
  template <typename T>
  T sparse_rand() { return T(rand64() & rand64() & rand64()); }
};

/// Zobrist hashing tables for position identification.
namespace Zobrist {
  /// Compile-time PRNG step (same algorithm as PRNG class).
  constexpr uint64_t xorshift64star_next(uint64_t& s) {
    return xorshift64star_step(s);
  }

  /// Compile-time generated Zobrist tables.
  struct Tables {
    std::array<std::array<ZobristKey, SQ_LENGTH>, PIECE_LENGTH> pieces{};
    std::array<ZobristKey, CR_LENGTH> castlingRights{};
    std::array<ZobristKey, FILE_LENGTH> enPassantFile{};
    ZobristKey nextPlayer{};

    /// Generates all Zobrist keys at compile time using a fixed seed.
    constexpr Tables() {
      uint64_t state = 1070372ULL; // fixed seed for reproducibility
      // pieces
      for (int pc = 0; pc < PIECE_LENGTH; ++pc) {
        for (int sq = 0; sq < SQ_LENGTH; ++sq) {
          pieces[pc][sq] = xorshift64star_next(state);
        }
      }
      // castling rights [0..15]
      for (int cr = 0; cr < CR_LENGTH; ++cr) {
        castlingRights[cr] = xorshift64star_next(state);
      }
      // en passant files A..H only; FILE_NONE remains default 0
      for (int f = FILE_A; f <= FILE_H; ++f) {
        enPassantFile[static_cast<std::size_t>(f)] = xorshift64star_next(state);
      }
      // next player
      nextPlayer = xorshift64star_next(state);
    }
  };

  /// Compile-time instantiated tables.
  inline constexpr Tables T{};

  /// Zobrist piece keys indexed by [piece][square].
  inline constexpr auto& pieces = T.pieces;

  /// Zobrist castling rights keys indexed by [castlingRights].
  inline constexpr auto& castlingRights = T.castlingRights;

  /// Zobrist en passant file keys indexed by [file].
  inline constexpr auto& enPassantFile = T.enPassantFile;

  /// Zobrist key XORed when Black is to move.
  inline constexpr ZobristKey nextPlayer = T.nextPlayer;
}// namespace Zobrist

#endif // FRANKYCPP_ZOBRIST_H
