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

#ifndef FRANKYCPP_RANK_H
#define FRANKYCPP_RANK_H

enum Rank : int {
  RANK_1,
  RANK_2,
  RANK_3,
  RANK_4,
  RANK_5,
  RANK_6,
  RANK_7,
  RANK_8,
  RANK_NONE,
  RANK_LENGTH = 9
};

// checks if rank is a value of 0-7
constexpr bool validRank(Rank r) {
  return !(r < 0 || r >= 8);
}

// creates a rank from a char
constexpr Rank makeRank(char rankLabel) {
  const Rank r = static_cast<Rank>(rankLabel - '1');
  return validRank(r) ? r : RANK_NONE;
}

// returns a string representing the rank (e.g. 1 or 8)
constexpr char rankLabel(Rank r) {
  if (r < 0 || r >= 8) return '-';
  return '1' + r;
}

inline std::ostream& operator<< (std::ostream& os, const Rank r) {
  os << rankLabel(r);
  return os;
}
#endif//FRANKYCPP_RANK_H
