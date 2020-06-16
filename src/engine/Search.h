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

#ifndef FRANKYCPP_SEARCH_H
#define FRANKYCPP_SEARCH_H


#include <thread>

#include "types/types.h"
#include "SearchLimits.h"
#include "SearchStats.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"
#include "common/Semaphore.h"
#include "openingbook/OpeningBook.h"

#include "gtest/gtest_prod.h"

// forward declaration
class UciHandler;

class Search {

  Position position{};
  SearchLimits searchLimits{};


public:
  ////////////////////////////////////////////////
  ///// CONSTRUCTORS

  Search(UciHandler* uciHandler);

  // disallow copies and moves
  Search (Search const&) = delete;
  Search& operator= (const Search&) = delete;
  Search (Search const&&)           = delete;
  Search& operator= (const Search&&) = delete;

  ////////////////////////////////////////////////
  ///// PUBLIC

  /** starts the search in a separate thread with the given search limits */
  void startSearch (const Position p, SearchLimits sl);

  /** Stops a running search gracefully - e.g. returns the best move found so far */
  void stopSearch ();

  /** checks if the search is already running */
  bool isRunning () const { return false; } // TODO

  /** signals if we have a result */
  bool hasResult () const { return false; } // TODO implement

  /** wait while searching */
  void waitWhileSearching ();

  /** to signal the search that pondering was successful */
  void ponderhit ();

  /** return current root pv list */
//  const MoveList& getPV () const { return pv[PLY_ROOT]; };    // TODO implement

  /** clears the hash table */
  void clearHash ();

  /** resize the hash to the given value in MB */
  void setHashSize (int sizeInMB);

  /** return search stats instance */
//  inline const SearchStats& getSearchStats () const { return searchStats; } // TODO implement

  /** return the last search result */
//  inline const SearchResult& getLastSearchResult () const { return lastSearchResult; }; // TODO implement

};

#endif//FRANKYCPP_SEARCH_H
