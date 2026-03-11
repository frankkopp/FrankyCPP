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

#ifndef FRANKYCPP_PIECE_H
#define FRANKYCPP_PIECE_H

//=============================================================================
// piece.h - Chess Piece Type (Color + PieceType Combined)
//=============================================================================
//
// Piece represents a colored chess piece (e.g., WHITE_KING, BLACK_PAWN).
// Depends on: macros.h (implicitly uses Color, PieceType concepts)
//
// Encoding (4 bits):
//   Bits 0-2: PieceType (KING=1, PAWN=2, KNIGHT=3, BISHOP=4, ROOK=5, QUEEN=6)
//   Bit 3:    Color (0=WHITE, 1=BLACK)
//
//   PIECE_NONE   = 0b0000 (0)
//   WHITE_KING   = 0b0001 (1)  ...  WHITE_QUEEN = 0b0110 (6)
//   BLACK_KING   = 0b1001 (9)  ...  BLACK_QUEEN = 0b1110 (14)
//   PIECE_LENGTH = 16 (array sizing)
//
// Key Operations:
//   makePiece(color, pt)  - Create piece from color and type
//   makePiece('K')        - Create piece from FEN character
//   colorOf(piece)        - Extract color (WHITE/BLACK)
//   typeOf(piece)         - Extract piece type
//   str(piece)            - FEN character ('K', 'p', etc.)
//   validPiece(piece)     - True if valid piece value
//
// Usage:
//   Piece p = makePiece(WHITE, KNIGHT);  // WHITE_KNIGHT
//   Color c = colorOf(p);                // WHITE
//   PieceType pt = typeOf(p);            // KNIGHT
//   char ch = str(p);                    // 'N'
//
//=============================================================================

#include "macros.h"

namespace chess {

  enum Piece : int_fast8_t {
    // clang-format off
    PIECE_NONE,
    WHITE_KING = 1, WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN,
    BLACK_KING = 9, BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN,
    PIECE_LENGTH = 16
  }; // clang-format on

  // checks if piece type is a value of 0 - 6
  constexpr bool validPiece(const Piece p) { return p < 15 && p != 7 && p != 8; }

  // creates the piece given by color and piece type
  constexpr Piece makePiece(const Color c, const PieceType pt) { return static_cast<Piece>((c << 3) + pt); }

  // creates the piece based on the FEN char
  constexpr Piece makePiece(const unsigned char p) {
    switch (p) {
        // clang-format off
      case 'K': return WHITE_KING;
      case 'P': return WHITE_PAWN;
      case 'N': return WHITE_KNIGHT;
      case 'B': return WHITE_BISHOP;
      case 'R': return WHITE_ROOK;
      case 'Q': return WHITE_QUEEN;
      case 'k': return BLACK_KING;
      case 'p': return BLACK_PAWN;
      case 'n': return BLACK_KNIGHT;
      case 'b': return BLACK_BISHOP;
      case 'r': return BLACK_ROOK;
      case 'q': return BLACK_QUEEN;
      default: return PIECE_NONE;
    } // clang-format on
  }

  // returns the color of the given piece
  constexpr Color colorOf(const Piece p) { return static_cast<Color>(p >> 3); }

  // returns the piece type of the given piece
  constexpr PieceType typeOf(const Piece p) { return static_cast<PieceType>(p & 0b00000111u); }

  /** returns a char representing the piece. Upper case letters for white, lower case for black */
  constexpr auto pieceToChar = " KPNBRQ  kpnbrq   ";

  // single char label for the piece as used in a FEN (one of " KPNBRQ  kpnbrq")
  constexpr char str(const Piece p) {
    if (!validPiece(p)) return '-';
    return std::string(pieceToChar)[p];
  }

  inline std::ostream& operator<<(std::ostream& os, const Piece p) {
    os << str(p);
    return os;
  }

  ENABLE_INCR_OPERATORS_ON(Piece)

} // namespace chess

#endif // FRANKYCPP_PIECE_H
