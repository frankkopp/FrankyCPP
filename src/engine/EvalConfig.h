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

#ifndef FRANKYCPP_EVALCONFIG_H
#define FRANKYCPP_EVALCONFIG_H

#include "types/types.h"

namespace EvalConfig {

  inline bool USE_MATERIAL   = true;
  inline bool USE_POSITIONAL = true;

  inline int TEMPO = 34;

  inline bool USE_LAZY_EVAL   = true;
  inline Value LAZY_THRESHOLD = Value{700};

  inline bool USE_PAWN_EVAL       = false;
  inline bool USE_PAWN_TT         = false;
  inline int PAWN_TT_SIZE_MB = 64;

  inline int ISOLATED_PAWN_MID_WEIGHT  = -10;
  inline int ISOLATED_PAWN_END_WEIGHT  = -20;
  inline int DOUBLED_PAWN_MID_WEIGHT   = -10;
  inline int DOUBLED_PAWN_END_WEIGHT   = -30;
  inline int PASSED_PAWN_MID_WEIGHT    = 20;
  inline int PASSED_PAWN_END_WEIGHT    = 40;
  inline int BLOCKED_PAWN_MID_WEIGHT   = -2;
  inline int BLOCKED_PAWN_END_WEIGHT   = -20;
  inline int PHALANX_PAWN_MID_WEIGHT   = 4;
  inline int PHALANX_PAWN_END_WEIGHT   = 4;
  inline int SUPPORTED_PAWN_MID_WEIGHT = 10;
  inline int SUPPORTED_PAWN_END_WEIGHT = 15;
}// namespace EvalConfig

#endif//FRANKYCPP_EVALCONFIG_H
