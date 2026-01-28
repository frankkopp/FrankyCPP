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

#include "types/types.h"

#include <array>
#include <cassert>
#include <cstdint>

// PRNG moved from zobristkey.h
// from Stockfish: xorshift64star Pseudo-Random Number Generator
constexpr uint64_t xorshift64star_step(uint64_t& s) {
  s ^= s >> 12;
  s ^= s << 25;
  s ^= s >> 27;
  return s * 2685821657736338717ULL;
}

// PRNG moved from zobristkey.h
// from Stockfish: xorshift64star Pseudo-Random Number Generator
class PRNG {
  uint64_t s;

  uint64_t rand64() {
    return xorshift64star_step(s);
  }

public:
  explicit PRNG(const uint64_t seed) : s(seed) { assert(seed); }

  template <typename T>
  T rand() { return T(rand64()); }

  // Special generator used to fast init magic numbers.
  // Output values only have 1/8th of their bits set on average.
  template <typename T>
  T sparse_rand() { return T(rand64() & rand64() & rand64()); }
};

namespace Zobrist {
  // Compile-time generator reproducing the same PRNG stream as PRNG
  constexpr uint64_t xorshift64star_next(uint64_t& s) {
    return xorshift64star_step(s);
  }

  struct Tables {
    std::array<std::array<ZobristKey, SQ_LENGTH>, PIECE_LENGTH> pieces{};
    std::array<ZobristKey, CR_LENGTH> castlingRights{};
    std::array<ZobristKey, FILE_LENGTH> enPassantFile{};
    ZobristKey nextPlayer{};

    // Fill all tables from a single PRNG stream seeded like before
    constexpr Tables() {
      uint64_t state = 1070372ULL; // same seed as before
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

  inline constexpr Tables T{};

  // Expose tables with the same names used across the project

  // Zobrist piece keys [piece][square]
  inline constexpr auto& pieces = T.pieces;

  // Zobrist castling rights keys [castlingRights]
  inline constexpr auto& castlingRights = T.castlingRights;

  // Zobrist en passant file keys [file]
  inline constexpr auto& enPassantFile = T.enPassantFile;

  // Zobrist key for the side to move
  inline constexpr ZobristKey nextPlayer = T.nextPlayer;
}// namespace Zobrist

#endif // FRANKYCPP_ZOBRIST_H
