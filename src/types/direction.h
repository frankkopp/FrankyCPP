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

#ifndef FRANKYCPP_DIRECTION_H
#define FRANKYCPP_DIRECTION_H

#include "macros.h"
#include "square.h"

// Direction is a set of constants for moving squares within a Bb
//  NORTH      = 8,
//  EAST       = 1,
//  SOUTH      = -NORTH,
//  WEST       = -EAST,
//  NORTH_EAST = NORTH + EAST,
//  SOUTH_EAST = SOUTH + EAST,
//  SOUTH_WEST = SOUTH + WEST,
//  NORTH_WEST = NORTH + WEST
enum Direction : int_fast8_t {
  NORTH      = 8,
  EAST       = 1,
  SOUTH      = -NORTH,
  WEST       = -EAST,
  NORTH_EAST = NORTH + EAST,
  SOUTH_EAST = SOUTH + EAST,
  SOUTH_WEST = SOUTH + WEST,
  NORTH_WEST = NORTH + WEST
};

// return direction of pawns for the color
constexpr Direction pawnPush(Color c) { return c == WHITE ? NORTH : SOUTH; }

// Additional operators to add a Direction to a Square
// Could be invalid Square if int value of Direction + int value of Square are >63
constexpr Square operator+(Square s, Direction d) {
  return static_cast<Square>(int(s) + int(d));
}

// Additional operators to add a Direction to a Square
// Could be invalid Square if int value of Direction + int value of Square are >63
constexpr Square& operator+=(Square& s, Direction d) { return s = s + d; }

// Additional operators to subtract a Direction to a Square
// // Could be invalid Square if int value of Direction is > int value of Square
constexpr Square operator-(Square s, Direction d) {
  return static_cast<Square>(int(s) - int(d));
}

// Additional operators to subtract a Direction to a Square
// // Could be invalid Square if int value of Direction is > int value of Square
constexpr Square& operator-=(Square& s, Direction d) { return s = s - d; }

ENABLE_FULL_OPERATORS_ON (Direction)

#endif//FRANKYCPP_DIRECTION_H
