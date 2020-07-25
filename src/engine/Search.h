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

#include "SearchLimits.h"
#include "SearchResult.h"
#include "SearchStats.h"
#include "TT.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"
#include "common/Semaphore.h"
#include "engine/UciHandler.h"
#include "openingbook/OpeningBook.h"
#include "types/types.h"
#include "chesscore/History.h"

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
  TimePoint startTime{};    // when startSearch has been called
  TimePoint startSearchTime;// actual start time of search - only different from startTime after ponderhit()
  MilliSec timeLimit{};
  MilliSec extraTime{};
  std::thread timerThread{};

  // Control UCI updates to avoid flooding
  constexpr static uint64_t UCI_UPDATE_INTERVAL = nanoPerSec;
  uint64_t lastUciUpdateTime{};
  uint64_t lastUciUpdateNodes{};
  uint64_t npsTime{};
  uint64_t npsNodes{};

  // UCI relevant statistics
  uint64_t nodesVisited{};

  // Statistics
  SearchStats statistics{};

  // ply related data
  std::array<MoveList, DEPTH_MAX> pv{};
  std::array<MoveGenerator, DEPTH_MAX> mg{};

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

  // Creates a Search instance without an UciHandler. Prints uci
  // output to std::cout.
  Search();

  // Creates a Search instance and sends any uci protocol messages
  // to the UciHandler.
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
  void startSearch(const Position p, SearchLimits sl);

  // Stops a running search gracefully - e.g. returns the best move found so far
  void stopSearch();

  // checks if the search is already running
  bool isSearching() const;

  // signals if we have a result
  bool hasResult() const { return hasResultFlag; }

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
  inline const SearchStats& getSearchStats() const { return statistics; }// TODO implement

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
  SearchResult iterativeDeepening(Position& p);

  // Aspiration Search
  // AspirationSearch tries to achieve more beta cut offs by searching with a narrow
  // search window around an expected value for the search. We establish
  // a start value by doing a 3 ply normal search and expand the search window in
  // in several steps to the maximal window if search value returns outside of the window.
  Value aspirationSearch(Position& p, Depth depth, Value value);

  // rootSearch starts the actual recursive alpha beta search with the root moves for the first ply.
  // As root moves are treated a little different this separate function supports readability
  // as mixing it with the normal search would require quite some "if ply==0" statements.
  Value rootSearch(Position& p, Depth depth, Value alpha, Value beta);

  // search is the normal alpha beta search after the root move ply (ply > 0)
  // it will be called recursively until the remaining depth == 0 and we would
  // enter quiescence search. Search consumes about 60% of the search time and
  // all major prunings are done here. Quiescence search uses about 40% of the
  // search time and has less options for pruning as not all moves are searched.
  Value search(Position& p, Depth depth, Depth ply, Value alpha, Value beta, Node_Type isPv, Do_Null doNull);

  // qsearch is a simplified search to counter the horizon effect in depth based
  // searches. It continues the search into deeper branches as long as there are
  // so called non quiet moves (usually capture, checks, promotions). Only if the
  // position is relatively quiet we will compute an evaluation of the position
  // to return to the previous depth.
  // Look for non quiet moves is supported be the move generator which only
  // generates captures or promotions in qsearch (when not in check) and also
  // by SEE (Static Exchange Evaluation) to determine winning captured sequences.
  Value qsearch(Position& p, Depth ply, Value alpha, Value beta, Node_Type isPv);

  // After expanding the search to the required depth and all non quiet moves were
  // generated call the evaluation heuristic on the position.
  // This gives us a numerical value of this quiet position which we will return
  // back to the search.
  Value evaluate(Position& p);

  // reduce the number of moves searched in quiescence search by trying
  // to only look at good captures.
  bool goodCapture(Position& position, Move move);

  // storeTT stores a position into the TT
  void storeTt(Position& p, Depth depth, Depth ply, Move move, Value value, ValueType valueType, Value eval);

  // savePV adds the given move as first move to a dest move list and the appends
  // all src moves to dest. Dest will be cleared before the the append.
  void savePV(Move move, MoveList& src, MoveList& dest);

  // correct the value for mate distance when storing to TT
  static Value valueToTt(Value value, Depth ply);

  // correct the value for mate distance when reading from TT
  static Value valueFromTt(Value value, Depth ply);

  // getPVLine fills the given pv move list with the pv move starting from the given
  // depth as long as these position are in the TT
  // This is used when we retrieve a value and move from the TT and would not get a PV
  // line otherwise.
  void getPvLine(Position& p, MoveList& pvList, Depth depth);

  // stopConditions checks if stopFlag is set or if nodesVisited have
  // reached a potential maximum set in the search limits.
  bool stopConditions();

  // setupSearchLimits reports to log on search limits for the search
  // and sets up time control.
  void setupSearchLimits(Position& p, SearchLimits& sl);

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

  // checks repetitions and 50-moves rule. Returns true if the position
  // has repeated itself at least the given number of times.
  static bool checkDrawRepAnd50(Position& position, int numberOfRepetitions);

  // helper to send uci protocol messages.
  void sendReadyOk() const;

  // sends an info string to the uci handler if a handler is available.
  void sendString(const std::string& msg) const;

  // sends the search result to the uci handler if a handler is available.
  void sendResult(SearchResult& result);

  // send UCI information after each depth iteration.
  void sendIterationEndInfoToUci();

  // send UCI information about search - could be called each 500ms or so.
  void sendSearchUpdateToUci();

  // send UCI information after aspiration search.
  void sendAspirationResearchInfo(const std::string boundString);
};

#endif//FRANKYCPP_SEARCH_H
