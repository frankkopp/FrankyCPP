/*
 * MIT License
 *
 * Copyright (c) 2018-2020 Frank Kopp
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

#ifndef FRANKYCPP_SEARCHCONFIG_H
#define FRANKYCPP_SEARCHCONFIG_H

#include "types/types.h"

namespace SearchConfig {

  // @formatter:off

  // opening book
  //  inline bool                    USE_BOOK  = true;
  //  inline std::string             BOOK_PATH = "./books/book.txt";
  //  inline OpeningBook::BookFormat BOOK_TYPE = OpeningBook::BookFormat::SIMPLE;

  // basic search strategies and features
  //  inline bool USE_ASPIRATION_WINDOW   = true;
  //  inline Depth ASPIRATION_START_DEPTH = Depth{4};
  //  inline bool USE_ALPHABETA           = true; // use ALPHA_BETA instead of MinMax
  //  inline bool USE_PVS                 = true; // use PVS null window search
  //  inline bool USE_QUIESCENCE          = true; // use quiescence search
  //  inline bool USE_QS_SEE              = true; // use SEE for goodCaptures

  //  inline Depth MAX_EXTRA_QDEPTH       = Depth{20};

  // Transposition Table
  inline bool USE_TT                  = true; // use transposition table
  inline int TT_SIZE_MB               = 64;   // size of TT in MB
  //  inline bool USE_TT_QSEARCH          = true; // use transposition table also in quiescence search

  //  // Move Sorting Features
//  inline bool USE_KILLER_MOVES        = true; // Store refutation moves (>beta) for move ordering
//  inline int NO_KILLER_MOVES          = 2;    // number of killers stored
//  inline bool USE_PV_MOVE_SORT        = true; // tell the move gen the current pv to return first

//  // Pruning features
//  inline bool USE_MDP                 = true; // mate distance pruning
//  inline bool USE_MPP                 = true; // minor promotion pruning
//  inline bool USE_QS_STANDPAT_CUT     = true; // RFP for quiescence
//
//  inline bool USE_RFP                 = true; // Reverse Futility Pruning
//  inline Value RFP_MARGIN             = Value{250}; // less than 3 pawns per depth
//
//  inline bool USE_NMP                 = true; // Null Move Pruning
//  inline Depth NMP_DEPTH              = Depth{3};
//  inline Depth NMP_REDUCTION          = Depth{3};
//  inline bool NMP_VERIFICATION        = true;
//  inline Depth NMP_V_REDUCTION        = Depth{3};
//
//  inline bool USE_EXTENSIONS          = true; // extensions
//
//  inline bool USE_FP                  = true; // futility pruning
//  inline Value FP_MARGIN              = 2 * valueOf(PAWN);
//
//  inline bool USE_EFP                 = true;
//  inline Value EFP_MARGIN             = valueOf(ROOK);
//
//  inline bool  USE_LMR                = true; // Late Move Reduction
//  inline Depth LMR_MIN_DEPTH          = Depth{3};
//  inline int   LMR_MIN_MOVES          = 3;
//  inline Depth LMR_REDUCTION          = Depth{1};
//
//  // not implemented
//  // vvvvvvvvvvvvvvv
//
//  inline bool USE_IID               = true; // Internal Iterative Deepening to find good first move
//  inline Depth IID_REDUCTION        = Depth{2};
//
//  inline bool USE_RAZOR_PRUNING     = true; // Razoring - bad move direct into qs
//  inline Depth RAZOR_DEPTH          = Depth{2};
//  inline Value RAZOR_MARGIN         = Value{600};
//
//  // tactical features
//  inline bool USE_LMP               = true; // Late Move Pruning
//  inline Depth LMP_MIN_DEPTH        = Depth{3};
//  inline int   LMP_MIN_MOVES        = 6;

  // @formatter:on

}

#endif //FRANKYCPP_SEARCHCONFIG_H
