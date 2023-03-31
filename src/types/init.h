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

#ifndef FRANKYCPP_TYPES_INIT_H
#define FRANKYCPP_TYPES_INIT_H

#include "bitboard.h"
#include "square.h"

namespace Types {

  // init() initializes all chess types by pre-computing arrays of
  // Bitboard or other data structures to avoid computation time
  // during runtime.
  // The order of the initialization is important as there are
  // dependencies.
  inline void init() {
    Squares::squareDistancePreCompute();
    Bitboards::rankFileBbPreCompute();
    Bitboards::squareBitboardsPreCompute();
    Bitboards::nonSlidingAttacksPreCompute();
    Bitboards::neighbourMasksPreCompute();
    Bitboards::initMagicBitboards();
    Bitboards::raysPreCompute();
    Bitboards::intermediatePreCompute();
    Bitboards::maskPassedPawnsPreCompute();
    Bitboards::castleMasksPreCompute();
    Bitboards::colorBitboardsPreCompute();
    Squares::centerDistancePreCompute();
    Squares::squareNamesPreCompute();
    Castling::initCastlingRights();
  }
}// namespace types

#endif//FRANKYCPP_TYPES_INIT_H
