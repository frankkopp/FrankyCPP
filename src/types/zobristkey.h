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

#ifndef FRANKYCPP_ZOBRISTKEY_H
#define FRANKYCPP_ZOBRISTKEY_H

//=============================================================================
// zobristkey.h - Zobrist Hash Key Type
//=============================================================================
//
// ZobristKey is a 64-bit hash used for position identification in the
// transposition table and repetition detection.
// No internal dependencies.
//
// Type:
//   typedef uint64_t ZobristKey;
//
// Properties:
//   - Incrementally updated via XOR when position state changes
//   - Piece placement, side to move, castling rights, en passant all contribute
//   - Collision probability is ~1 in 2^64 per comparison
//
// Usage:
//   ZobristKey key = position.getZobristKey();
//   key ^= Zobrist::piece[piece][square];  // Toggle piece
//   key ^= Zobrist::side;                  // Toggle side to move
//
// See also: chesscore/Zobrist.h for the random number tables
//
//=============================================================================

#include <cstdint>

typedef uint64_t ZobristKey;

#endif//FRANKYCPP_ZOBRISTKEY_H
