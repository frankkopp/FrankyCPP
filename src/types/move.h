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

#ifndef FRANKYCPP_MOVE_H
#define FRANKYCPP_MOVE_H

#include "movetype.h"
#include "piece.h"
#include "piecetype.h"
#include "value.h"

// Move is a 32bit unsigned int type for encoding chess moves as a primitive data type
// 16 bits for move encoding - 16 bits for sort value
//  MOVE_NONE = 0
//
//  BITMAP 32-bit
//  |-value ------------------------|-Move -------------------------|
//  3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 | 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
//  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 | 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  --------------------------------|--------------------------------
//                                  |                     1 1 1 1 1 1  to
//                                  |         1 1 1 1 1 1              from
//                                  |     1 1                          promotion piece type (pt-2 > 0-3)
//                                  | 1 1                              move type
//  1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 |                                  move sort value
enum Move : uint32_t {
  MOVE_NONE = 0
};

namespace MoveShifts {
  constexpr unsigned int FROM_SHIFT      = 6u;
  constexpr unsigned int PROM_TYPE_SHIFT = 12u;
  constexpr unsigned int MOVE_TYPE_SHIFT = 14u;
  constexpr unsigned int VALUE_SHIFT     = 16u;

  constexpr unsigned int SQUARE_MASK    = 0b111111u;
  constexpr unsigned int TO_MASK        = SQUARE_MASK;
  constexpr unsigned int FROM_MASK      = SQUARE_MASK << FROM_SHIFT;
  constexpr unsigned int PROM_TYPE_MASK = 0b11u << PROM_TYPE_SHIFT;
  constexpr unsigned int MOVE_TYPE_MASK = 0b11u << MOVE_TYPE_SHIFT;

  constexpr unsigned int MOVE_MASK  = 0xFFFFu;               // first 16-bit
  constexpr unsigned int VALUE_MASK = 0xFFFFu << VALUE_SHIFT;// second 16-bit
}// namespace MoveShifts

// creates a move
constexpr Move createMove(Square from, Square to, MoveType mt, PieceType promType = KNIGHT) {
  if (promType < KNIGHT) promType = KNIGHT;
  // promType will be reduced to 2 bits (4 values) Knight, Bishop, Rook, Queen
  // therefore we subtract the Knight value from the promType to get
  // value between 0 and 3 (0b00 - 0b11)
  return Move(to |
              from << MoveShifts::FROM_SHIFT |
              (promType - KNIGHT) << MoveShifts::PROM_TYPE_SHIFT |
              mt << MoveShifts::MOVE_TYPE_SHIFT);
}

// creates a move
constexpr Move createMove(Square from, Square to, MoveType mt, Value value) {
  return Move(to |
              from << MoveShifts::FROM_SHIFT |
              mt << MoveShifts::MOVE_TYPE_SHIFT |
              (value - VALUE_NONE) << MoveShifts::VALUE_SHIFT);
}

// creates a move
constexpr Move createMove(Square from, Square to, MoveType mt, PieceType promType, Value value) {
  if (promType < KNIGHT) promType = KNIGHT;
  // promType will be reduced to 2 bits (4 values) Knight, Bishop, Rook, Queen
  // therefore we subtract the Knight value from the promType to get
  // value between 0 and 3 (0b00 - 0b11)
  return Move(to |
              from << MoveShifts::FROM_SHIFT |
              (promType - KNIGHT) << MoveShifts::PROM_TYPE_SHIFT |
              mt << MoveShifts::MOVE_TYPE_SHIFT |
              (value - VALUE_NONE) << MoveShifts::VALUE_SHIFT);
}

// returns the from-Square of the move
inline Square fromSquare(Move m) {
  return Square((m & MoveShifts::FROM_MASK) >> MoveShifts::FROM_SHIFT);
}

// returns the to-Square of the move
inline Square toSquare(Move m) {
  return Square(m & MoveShifts::TO_MASK);
}

// PromotionType returns the PieceType considered for promotion when
// move type is also MoveType.Promotion.
// Must be ignored when move type is not MoveType.Promotion.
inline PieceType promotionTypeOf(Move m) {
  return PieceType(((m & MoveShifts::PROM_TYPE_MASK) >> MoveShifts::PROM_TYPE_SHIFT) + KNIGHT);
}

// MoveType returns the type of the move as defined in MoveType
inline MoveType moveTypeOf(Move m) {
  return MoveType((m & MoveShifts::MOVE_TYPE_MASK) >> MoveShifts::MOVE_TYPE_SHIFT);
}

// returns the move without any value (least 16-bits)
inline Move moveOf(Move m) {
  return Move(m & MoveShifts::MOVE_MASK);
}

// returns the sort value for the move used in the move generator
inline Value valueOf(Move m) {
  return Value(((m & MoveShifts::VALUE_MASK) >> MoveShifts::VALUE_SHIFT) + VALUE_NONE);
}

inline Move setValueOf(Move& m, Value v) {
  // can't store a value on MoveNone
  if (moveOf(m) == MOVE_NONE) return m;
  // when saving a value to a move we shift value to a positive integer
  // (0-VALUE_NONE) and encode it into the move. For retrieving we then shift
  // the value back to a range from VALUE_NONE to VALUE_INF
  return m = Move((m & MoveShifts::MOVE_MASK) | ((v - VALUE_NONE) << MoveShifts::VALUE_SHIFT));
}

constexpr bool validMove(Move m) {
  return m != MOVE_NONE &&
         validSquare(fromSquare(m)) &&
         validSquare(toSquare(m)) &&
         validPieceType(promotionTypeOf(m)) &&
         validMoveType(moveTypeOf(m)) &&
         (valueOf(m) == VALUE_NONE || validValue(valueOf(m)));
}

/** returns a short representation of the move as string (UCI protocol) */
inline std::string str(Move move) {
  if (moveOf(move) == MOVE_NONE) return "no move";
  std::string promotion = "";
  if ((moveTypeOf(move) == PROMOTION)) promotion = str(promotionTypeOf(move));
  return str(fromSquare(move)) + str(toSquare(move)) + promotion;
}

/** returns a verbose representation of the move as string */
inline std::string strVerbose(Move move) {
  if (!move) return "no move " + std::to_string(move);
  std::string tp;
  std::string promPt;
  switch (moveTypeOf(move)) {
    case NORMAL:
      tp = "NORMAL";
      break;
    case PROMOTION:
      promPt = str(promotionTypeOf(move));
      tp     = "PROMOTION";
      break;
    case ENPASSANT:
      tp = "ENPASSANT";
      break;
    case CASTLING:
      tp = "CASTLING";
      break;
  }
  return str(fromSquare(move)) + str(toSquare(move)) + promPt + " (" + tp + " " + std::to_string(valueOf(move)) + " " + std::to_string(move) + ")";
}

inline std::ostream& operator<<(std::ostream& os, const Move move) {
  os << str(move);
  return os;
}

#endif//FRANKYCPP_MOVE_H
