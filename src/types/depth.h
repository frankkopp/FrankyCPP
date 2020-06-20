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

#ifndef FRANKYCPP_DEPTH_H
#define FRANKYCPP_DEPTH_H

#include <cstdint>
#include "macros.h"

///////////////////////////////////
//// DEPTH
enum Depth : int {
  DEPTH_NONE  = 0,
  DEPTH_ONE   = 1,
  DEPTH_TWO   = 2,
  DEPTH_THREE = 3,
  DEPTH_FOUR  = 4,

  DEPTH_ZERO            = DEPTH_NONE,
  DEPTH_FRONTIER        = DEPTH_ONE,
  DEPTH_PRE_FRONTIER    = DEPTH_TWO,
  DEPTH_PREPRE_FRONTIER = DEPTH_THREE,

  DEPTH_MAX = 127
};

ENABLE_FULL_OPERATORS_ON (Depth)


#endif//FRANKYCPP_DEPTH_H
