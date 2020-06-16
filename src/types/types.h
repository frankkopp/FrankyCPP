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

#ifndef FRANKYCPP_TYPES_H
#define FRANKYCPP_TYPES_H

// include all type headers for convenience
#include "macros.h"
#include "init.h"
#include "globals.h"
#include "color.h"
#include "direction.h"
#include "orientation.h"
#include "depth.h"
#include "file.h"
#include "rank.h"
#include "square.h"
#include "castlingrights.h"
#include "value.h"
#include "valuetype.h"
#include "piecetype.h"
#include "piece.h"
#include "move.h"
#include "movelist.h"
#include "bitboard.h"
#include "zobristkey.h"

/** for time keeping */
typedef int64_t MilliSec;

#endif//FRANKYCPP_TYPES_H
