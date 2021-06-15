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

#ifndef FRANKYCPP_PIECE_H
#define FRANKYCPP_PIECE_H

#include "macros.h"

// Piece is a set of constants for pieces in chess
// Can be used with masks:
//  No Piece = 0
//  White Piece is a non zero value with piece & 0b1000 == 0
//  Black Piece is a non zero value with piece & 0b1000 == 1
//  PieceNone  = 0b00000
//  WhiteKing  = 0b00001
//  WhitePawn  = 0b00010
//  WhiteKnight= 0b00011
//  WhiteBishop= 0b00100
//  WhiteRook  = 0b00101
//  WhiteQueen = 0b00110
//  BlackKing  = 0b01001
//  BlackPawn  = 0b01010
//  BlackKnight= 0b01011
//  BlackBishop= 0b01100
//  BlackRook  = 0b01101
//  BlackQueen = 0b01110
//  PieceLength= 0b10000
enum Piece : int_fast8_t { // @formatter:off
  PIECE_NONE,
  WHITE_KING = 1, WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN,
  BLACK_KING = 9, BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN,
  PIECE_LENGTH = 16
};// @formatter:on

// checks if piece type is a value of 0 - 6
constexpr bool validPiece(Piece p) {
  return p < 15 && p != 7 && p != 8;
}

// creates the piece given by color and piece type
constexpr Piece makePiece(Color c, PieceType pt) { return Piece((c << 3) + pt); }

// creates the piece based on the FEN char
constexpr Piece makePiece(unsigned char p) {
  switch (p) { // @formatter:off
    case 'K': return WHITE_KING;
    case 'P': return WHITE_PAWN;
    case 'N': return WHITE_KNIGHT;
    case 'B': return WHITE_BISHOP;
    case 'R': return WHITE_ROOK;;
    case 'Q': return WHITE_QUEEN;
    case 'k': return BLACK_KING;
    case 'p': return BLACK_PAWN;
    case 'n': return BLACK_KNIGHT;
    case 'b': return BLACK_BISHOP;
    case 'r': return BLACK_ROOK;;
    case 'q': return BLACK_QUEEN;
    default: return PIECE_NONE;
  }// @formatter:on
}

// returns the color of the given piece
constexpr Color colorOf(Piece p) { return static_cast<Color>(p >> 3); }

// returns the piece type of the given piece
constexpr PieceType typeOf(Piece p) { return static_cast<PieceType>(p & 7); }

namespace {
  /** returns a char representing the piece. Upper case letters for white, lower case for black */
  constexpr const char* pieceToChar = " KPNBRQ  kpnbrq   ";
}// namespace

// single char label for the piece as used in a FEN (one of " KPNBRQ  kpnbrq")
constexpr char str(Piece p) {
  if (!validPiece(p)) return '-';
  return std::string(pieceToChar)[p];
}

inline std::ostream& operator<<(std::ostream& os, const Piece p) {
  os << str(p);
  return os;
}

ENABLE_INCR_OPERATORS_ON(Piece)

#endif//FRANKYCPP_PIECE_H
