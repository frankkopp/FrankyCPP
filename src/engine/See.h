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

#ifndef FRANKYCPP_SEE_H
#define FRANKYCPP_SEE_H


#include <types/types.h>

class Position;

namespace See {

  // SEE - static exchange evaluation function
  // Evaluates the SEE score for the given move which has not been made on the position
  // yet.
  // En-passant captures will always return a score of +100 and should therefore not be
  // cut-off.
  // Credit: https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
  //
  // Returns a value for the static exchange. The given move must be a capturing
  // move otherwise returns Value(0) .
  Value see(Position& p, Move move);

  // Returns a square with the least valuable attacker. When several of same
  // type are available it uses the least significant bit of the bitboard.
  Square getLeastValuablePiece(Position& p, Bitboard bitboard, Color color);

  // AttacksTo determines all attacks to the given square for the given color.
  Bitboard attacksTo(Position& p, Square square, Color color);

  // RevealedAttacks returns sliding attacks after a piece has been removed to reveal new attacks.
  // It is only necessary to look at slider pieces as only their attacks can be revealed.
  Bitboard revealedAttacks(Position& p, Square square, Bitboard occupied, Color color);

};


#endif//FRANKYCPP_SEE_H
