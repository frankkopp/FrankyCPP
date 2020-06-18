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


#include <chesscore/History.h>
#include <thread>

#include "SearchLimits.h"
#include "SearchStats.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"
#include "common/Semaphore.h"
#include "engine/UciHandler.h"
#include "openingbook/OpeningBook.h"
#include "types/types.h"

#include "SearchResult.h"
#include "TT.h"
#include "gtest/gtest_prod.h"

// forward declaration
class UciHandler;
class Evaluator;

class Search {

  // callback handler for UCI communication
  UciHandler* uciHandler{};

  // state management for the search
  mutable Semaphore initSemaphore{1};
  mutable Semaphore isRunningSemaphore{1};
  std::thread searchThread{};
  std::thread timerThread{};


  std::unique_ptr<OpeningBook> book;
  std::unique_ptr<TT> tt;
  std::unique_ptr<Evaluator> evaluator;

  // history heuristics
  History history{};

  // result of previous search
  SearchResult lastSearchResult{};

  // current position and search limits for the search
  Position position{};
  SearchLimits searchLimits{};
  MoveList rootMoves{};

  // manage running search
  std::atomic_bool stopSearchFlag = false;
  std::atomic_bool hasResultFlag  = false;

  // time management for the search
  TimePoint startTime{};
  MilliSec timeLimit{};
  MilliSec extraTime{};

  // Control UCI updates to avoid flooding
  constexpr static MilliSec UCI_UPDATE_INTERVAL = MilliSec{500};
  TimePoint lastUciUpdateTime{};

  // UCI relevant statistics
  uint64_t nodesVisited{0};

  // Statistics
  SearchStats searchStats{};

  // ply related data
  MoveList pv[DEPTH_MAX];
  MoveGenerator mg[DEPTH_MAX];

  // to mark the last move was a book move
  bool hadBookMove = false;

public:
  // in PV we search the full window in NonPV we try a zero window first
  enum Node_Type : bool { NonPV = false,
                          PV    = true };

  // If this is true we are allowed to do a NULL move in this ply. This is to
  // avoid recursive null move searches or other prunings which do not make sense
  // in a null move search
  enum Do_Null : bool { No_Null_Move = false,
                        Do_Null_Move = true };

  // //////////////////////////////////////////////
  // CONSTRUCTORS

  Search();
  explicit Search(UciHandler* pUciHandler);
  ~Search();

  // disallow copies and moves
  Search(Search const&) = delete;
  Search& operator=(const Search&) = delete;
  Search(Search const&&)           = delete;
  Search& operator=(const Search&&) = delete;

  // ///////////////////////////////////////////
  // PUBLIC

  // NewGame stops any running searches and resets the search state
  // to be ready for a different game. Any caches or states will be reset.
  void newGame();

  // IsReady signals the uciHandler that the search is ready.
  // This is part if the UCI protocol to make sure a chess
  // engine is initialized and ready to receive commands.
  // When called this will initialize the search which might
  // take a while. When finished this will call the uciHandler
  // set in SetUciHandler to send "readyok" to the UCI user interface.
  void isReady();

  // starts the search in a separate thread with the given search limits
  void startSearch(const Position p, SearchLimits sl);// TODO implement

  // Stops a running search gracefully - e.g. returns the best move found so far
  void stopSearch();

  // checks if the search is already running
  bool isSearching() const;

  // signals if we have a result
  bool hasResult() const { return false; }// TODO implement

  // wait while searching blocks execution of the current thread until
  // the search has finished
  void waitWhileSearching() const;

  // to signal the search that pondering was successful
  void ponderhit();
  // return current root pv list

  const MoveList& getPV() const { return pv[0]; };// TODO implement

  // clears the hash table
  void clearTT();

  // resize the hash to the value in the global config SearchConfig::TT_SIZE_MB
  void resizeTT();

  // return search stats instance
  inline const SearchStats& getSearchStats() const { return searchStats; }// TODO implement

  // return the last search result
  inline const SearchResult& getLastSearchResult() const { return lastSearchResult; };// TODO implement

private:
  ////////////////////////////////////////////////
  ///// PRIVATE

  // Initialize sets up opening book, transposition table
  // and other potentially time consuming setup tasks
  // This can be called several times without doing
  // initialization again.
  void initialize();

  // Called after starting the search in a new thread. Configures the search
  // and eventually calls iterativeDeepening. After the search it takes the
  // result to sends it to the UCI engine.
  void run();

  // Iterative Deepening:
  // It works as follows: the program starts with a one ply search,
  // then increments the search depth and does another search. This
  // process is repeated until the time allocated for the search is
  // exhausted. In case of an unfinished search, the current best
  // move is returned by the search. The current best move is
  // guaranteed to by at least as good as the best move from the
  // last finished iteration as we sorted the root moves before
  // the start of the new iteration and started with this best
  // move. This way, also the results from the partial search
  // can be accepted
  SearchResult iterativeDeepening(Position& position);

  // stopConditions checks if stopFlag is set or if nodesVisited have
  // reached a potential maximum set in the search limits.
  bool stopConditions();

  // setupSearchLimits reports to log on search limits for the search
  // and sets up time control.
  void setupSearchLimits(Position& position, SearchLimits& sl);

  // setupTimeControl sets up time control according to the given search limits
  // and returns a limit on the duration for the current search.
  static MilliSec setupTimeControl(Position& position, SearchLimits& limits);
  FRIEND_TEST(SearchTest, setupTime);

  // addExtraTime certain situations might call for a extension or reduction
  // of the given time limit for the search. This function add/subtracts
  // a portion (%) of the current time limit.
  //  Example:
  //  f = 1.0 --> no change in search time
  //  f = 0.9 --> reduction by 10%
  //  f = 1.1 --> extension by 10%
  void addExtraTime(double f);
  FRIEND_TEST(SearchTest, extraTime);

  // startTimer starts a thread which regularly checks the elapsed time against
  // the time limit and extra time given. If time limit is reached this will set
  // the stopFlag to true and terminate itself.
  void startTimer();
  FRIEND_TEST(SearchTest, startTimer);

  void sendReadyOk() const;
  void sendString(const std::string& msg) const;
  void sendResult(SearchResult& result);
};

#endif//FRANKYCPP_SEARCH_H
