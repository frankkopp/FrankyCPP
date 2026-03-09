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

#ifndef FRANKYCPP_SCORE_H
#define FRANKYCPP_SCORE_H

//=============================================================================
// score.h - Tapered Evaluation Score (Midgame + Endgame Pair)
//=============================================================================
//
// Score holds separate midgame and endgame evaluation values for tapered
// evaluation. The final score is interpolated based on game phase.
// Depends on: value.h
//
// Structure:
//   struct Score {
//     Value midgame;  - Evaluation in opening/middlegame
//     Value endgame;  - Evaluation in endgame
//   };
//
// Operators:
//   +, -, *, /        - Component-wise arithmetic
//   +=, -=, *=, /=    - Compound assignment
//
// Usage:
//   Score s = {Value{50}, Value{30}};  // +50cp midgame, +30cp endgame
//   s += Score{Value{10}, Value{20}};  // Now {60, 50}
//
//   // Tapered evaluation (in Evaluator):
//   double phase = position.getGamePhaseFactor();  // 0.0 (endgame) to 1.0 (midgame)
//   Value final = Value{int(s.midgame * phase + s.endgame * (1.0 - phase))};
//
//=============================================================================

#include "types/value.h"

namespace chess {

  struct Score {
    Value midgame;
    Value endgame;
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

} // namespace chess

#endif // FRANKYCPP_SCORE_H
