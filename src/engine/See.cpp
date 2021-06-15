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

#include "See.h"
#include "chesscore/Position.h"

Value See::see(Position& p, Move move) {

  // enpassant moves are ignored in a sense that it will be winning
  // capture and therefore should lead to no cut-offs when using see()
  if (typeOf(move) == ENPASSANT) {
    return Value{100};
  }

  // prepare short array to store the captures - max 32 pieces
  std::array<Value, 32> gain{};

  int ply               = 0;
  const Square toSquare = ::toSquare(move);
  Square fromSquare     = ::fromSquare(move);
  Piece movedPiece      = p.getPiece(fromSquare);
  Color nextPlayer      = p.getNextPlayer();

  // get a bitboard of all occupied squares to remove single pieces later
  // to reveal hidden attacks (x-ray)
  Bitboard occupiedBitboard = p.getOccupiedBb();

  // get all attacks to the square as a bitboard
  Bitboard remainingAttacks = attacksTo(p, toSquare, WHITE) | attacksTo(p, toSquare, BLACK);

  // initial value of the first capture
  Value capturedValue = valueOf(p.getPiece(toSquare));
  gain[ply]           = capturedValue;

  // loop through all remaining attacks/captures
  do {
    ply++;                   // next depth
    nextPlayer = ~nextPlayer;// change side

    // speculative store, if defended
    gain[ply] = (typeOf(move) == PROMOTION
                   ? valueOf(promotionTypeOf(move)) - valueOf(PAWN)
                   : valueOf(movedPiece)) -
                gain[ply - 1];

    // pruning if defended - will not change final see score
    if (std::max(-gain[ply - 1], gain[ply]) < 0) break;

    remainingAttacks ^= fromSquare;// reset bit in set to traverse
    occupiedBitboard ^= fromSquare;// reset bit in temporary occupancy (for x-Rays)

    // reevaluate attacks to reveal attacks after removing the moving piece
    remainingAttacks |= revealedAttacks(p, toSquare, occupiedBitboard, WHITE) | revealedAttacks(p, toSquare, occupiedBitboard, BLACK);

    // determine next capture
    fromSquare = getLeastValuablePiece(p, remainingAttacks, nextPlayer);
    movedPiece = p.getPiece(fromSquare);

  } while (fromSquare != SQ_NONE);

  while (--ply) {
    gain[ply - 1] = -std::max(-gain[ply - 1], gain[ply]);
  }

  return gain[0];
}

Square See::getLeastValuablePiece(Position& p, Bitboard bitboard, Color color) {
  // check all piece types with increasing value
  if ((bitboard & p.getPieceBb(color, PAWN)) != 0)
    return lsb(bitboard & p.getPieceBb(color, PAWN));

  if ((bitboard & p.getPieceBb(color, KNIGHT)) != 0)
    return lsb(bitboard & p.getPieceBb(color, KNIGHT));

  if ((bitboard & p.getPieceBb(color, BISHOP)) != 0)
    return lsb(bitboard & p.getPieceBb(color, BISHOP));

  if ((bitboard & p.getPieceBb(color, ROOK)) != 0)
    return lsb(bitboard & p.getPieceBb(color, ROOK));

  if ((bitboard & p.getPieceBb(color, QUEEN)) != 0)
    return lsb(bitboard & p.getPieceBb(color, QUEEN));

  if ((bitboard & p.getPieceBb(color, KING)) != 0)
    return lsb(bitboard & p.getPieceBb(color, KING));

  return SQ_NONE;
}

Bitboard See::attacksTo(Position& p, Square square, Color color) {
  // prepare en passant attacks
  Bitboard epAttacks     = BbZero;
  Square enPassantSquare = p.getEnPassantSquare();
  if (enPassantSquare != SQ_NONE && enPassantSquare == square) {
    const Square pawnSquare   = pawnPush(enPassantSquare, ~color);
    if (Bitboards::neighbourFilesMask[pawnSquare] &
        Bitboards::sqToRankBb[pawnSquare] &
        p.getPieceBb(color, PAWN)) {
      epAttacks |= Bitboards::sqBb[pawnSquare];
    }
  }

  Bitboard occupiedAll = p.getOccupiedBb();

  // this uses a reverse approach - it uses the target square as from square
  // to generate attacks for each type and then intersects the result with
  // the piece bitboard.

  //      Pawns
  return (Bitboards::pawnAttacks[~color][square] & p.getPieceBb(color, PAWN)) |
         // Knight
         (getAttacksBb(KNIGHT, square, occupiedAll) & p.getPieceBb(color, KNIGHT)) |
         // King
         (getAttacksBb(KING, square, occupiedAll) & p.getPieceBb(color, KING)) |
         // Sliding rooks and queens
         (getAttacksBb(ROOK, square, occupiedAll) & (p.getPieceBb(color, ROOK) | p.getPieceBb(color, QUEEN))) |
         // Sliding bishops and queens
         (getAttacksBb(BISHOP, square, occupiedAll) & (p.getPieceBb(color, BISHOP) | p.getPieceBb(color, QUEEN))) |
         // consider en passant attacks
         epAttacks;
}

Bitboard See::revealedAttacks(Position& p, Square square, Bitboard occupied, Color color) {
  // Sliding rooks and queens
  return (getAttacksBb(ROOK, square, occupied) & (p.getPieceBb(color, ROOK) | p.getPieceBb(color, QUEEN)) & occupied) |
         // Sliding bishops and queens
         (getAttacksBb(BISHOP, square, occupied) & (p.getPieceBb(color, BISHOP) | p.getPieceBb(color, QUEEN)) & occupied);
}
