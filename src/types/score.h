/*
 * MIT License
 *
 * Copyright (c) 2018 Frank Kopp
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

#ifndef FRANKYCPP_SCORE_H
#define FRANKYCPP_SCORE_H

// tuple to store two values during evaluation
struct Score {
  int midgame;
  int endgame;
};

constexpr Score operator+(const Score& lhs, const Score& rhs) {
  return Score{lhs.midgame + rhs.midgame, lhs.endgame + rhs.endgame};
}

constexpr Score operator-(const Score& lhs, const Score& rhs) {
  return Score{lhs.midgame - rhs.midgame, lhs.endgame - rhs.endgame};
}

constexpr Score operator*(const Score& lhs, const int i) {
  return Score{lhs.midgame * i, lhs.endgame * i};
}

constexpr Score operator/(const Score& lhs, const int i) {
  return Score{lhs.midgame / i, lhs.endgame / i};
}

constexpr Score& operator+=(Score& lhs, const Score& rhs) {
  lhs.midgame += rhs.midgame;
  lhs.endgame += rhs.endgame;
  return lhs;
}

constexpr Score& operator-=(Score& lhs, const Score& rhs) {
  lhs.midgame -= rhs.midgame;
  lhs.endgame -= rhs.endgame;
  return lhs;
}

constexpr Score& operator*=(Score& lhs, const int i) {
  lhs.midgame *= i;
  lhs.endgame *= i;
  return lhs;
}

constexpr Score& operator/=(Score& lhs, const int i) {
  lhs.midgame /= i;
  lhs.endgame /= i;
  return lhs;
}

#endif//FRANKYCPP_SCORE_H
