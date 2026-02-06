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

#ifndef FRANKYCPP_SEARCH_H
#define FRANKYCPP_SEARCH_H

//=============================================================================
// Search.h - Alpha-Beta Search Engine
//=============================================================================
//
// Search implements the core chess search algorithm using iterative deepening
// with alpha-beta pruning and various enhancements.
// Depends on: TT.h, Evaluator.h, MoveGenerator.h, Position.h, OpeningBook.h, etc.
//
// Search Algorithm:
//   - Iterative deepening with aspiration windows
//   - Principal Variation Search (PVS)
//   - Alpha-beta pruning with fail-soft
//
// Pruning Techniques:
//   - Null-move pruning (with verification)
//   - Late Move Reductions (LMR)
//   - Futility pruning
//   - Razoring
//   - Delta pruning (in quiescence)
//   - SEE pruning for captures
//
// Move Ordering:
//   - TT move (hash move)
//   - Captures (MVV-LVA / SEE ordered)
//   - Killer moves (2 per ply)
//   - Counter move heuristic
//   - History heuristic
//
// Components:
//   - TT (Transposition Table) for position caching
//   - Evaluator for leaf node evaluation
//   - OpeningBook for book moves
//   - History for move ordering heuristics
//
// Threading:
//   - Search runs in dedicated thread (searchThread)
//   - Stoppable via stopSearchFlag (atomic)
//   - Timer thread monitors time limits
//   - Semaphores manage init/running state
//
// Time Management:
//   - Configurable via SearchLimits
//   - Supports fixed time, increment, moves-to-go
//   - Dynamic time extension for complex positions
//
// Key Methods:
//   startSearch(position, limits)  - Begin search (async)
//   stopSearch()                   - Abort search
//   waitWhileSearching()           - Block until complete
//   getLastSearchResult()          - Retrieve result
//
// Usage:
//   Search search;
//   search.startSearch(position, limits);
//   search.waitWhileSearching();
//   SearchResult result = search.getLastSearchResult();
//
//=============================================================================

#include "SearchLimits.h"
#include "SearchResult.h"
#include "SearchStats.h"
#include "TT.h"
#include "chesscore/History.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"
#include "engine/UciHandler.h"
#include "openingbook/OpeningBook.h"
#include "types/types.h"

#include "common/gtest_friends.h"
#include "config/ConfigManager.h"

#include <atomic>
#include <semaphore>
#include <thread>

// forward declaration
class UciHandler;
class Evaluator;

class Search {

  // callback handler for UCI communication
  UciHandler* uciHandler{};

  // state management for the search
  mutable std::binary_semaphore initSemaphore{1};
  mutable std::binary_semaphore isRunningSemaphore{1};
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
  milliseconds timeLimit{};
  std::atomic<int64_t> extraTimeMs{0};
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
  // Size is DEPTH_MAX + 1 to safely handle ply values from 0 to DEPTH_MAX (127)
  // and ply+1 indexing (up to 128) in savePV() calls
  std::array<MoveList, DEPTH_MAX + 1> pv{};
  std::array<MoveGenerator, DEPTH_MAX + 1> mg{};

  // to mark the last move was a book move
  bool hadBookMove = false;

  // reference to the Search Config Data
  const engine::config::SearchConfigData& SearchConfig;

  // LMR reduction table pre-computed for depth 0..31 and moves searched 0..63
  static constexpr int lmr_reduction(const int depth, const int movesSearched) {
    // 1 + round(depth * movesSearched * 0.0035)
    // exact integer rounding of 35/10000
    return 1 + (depth * movesSearched * 35 + 5000) / 10000;
  }
  static constexpr std::array<std::array<int, 64>, 32> make_lmr_table() {
    std::array<std::array<int, 64>, 32> t{};
    for (std::size_t d = 0; d < 32; ++d)
      for (std::size_t m = 0; m < 64; ++m)
        t[d][m] = lmr_reduction(static_cast<int>(d), static_cast<int>(m));
    return t;
  }
  inline static auto LMR_REDUCTION = make_lmr_table();
  FRIEND_TEST(SearchTest, lmrReductionTable);

public:
  /// Node type for PVS: PV nodes search full window, NonPV nodes try zero window first.
  enum Node_Type : bool { NonPV = false,
                          PV    = true };

  /// Controls whether null-move pruning is allowed at this ply.
  /// Disabled to avoid recursive null-move searches.
  enum Do_Null : bool { No_Null_Move = false,
                        Do_Null_Move = true };

  // //////////////////////////////////////////////
  // CONSTRUCTORS

  /// Creates a Search instance without an UciHandler.
  /// UCI output is printed to std::cout.
  Search();

  /// Creates a Search instance with a UciHandler for UCI protocol messages.
  /// @param pUciHandler  Pointer to the UCI handler (not owned)
  explicit Search(UciHandler* pUciHandler);

  ~Search();

  // disallow copies and moves
  Search(Search const&)             = delete;
  Search& operator=(const Search&)  = delete;
  Search(Search const&&)            = delete;
  Search& operator=(const Search&&) = delete;

  // ///////////////////////////////////////////
  // PUBLIC

  /// Stops any running search and resets state for a new game.
  /// Clears caches and history heuristics.
  void newGame();

  /// Signals readiness to the UCI interface after initialization.
  /// Part of UCI protocol; may trigger time-consuming setup on first call.
  void isReady();

  /// Starts an asynchronous search in a separate thread.
  /// @param p   Position to search
  /// @param sl  Search limits (time, depth, nodes, etc.)
  void startSearch(const Position& p, SearchLimits sl);

  /// Stops a running search gracefully, returning the best move found so far.
  void stopSearch();

  /// Checks if the search is currently running.
  /// @return True if search is in progress
  bool isSearching() const;

  /// Checks if a search result is available.
  /// @return True if result is ready
  bool hasResult() const { return hasResultFlag; }

  /// Blocks the calling thread until the search completes.
  void waitWhileSearching() const;

  /// Signals that pondering was successful (opponent played expected move).
  void ponderhit();

  /// Returns the principal variation from the current/last search.
  /// @return Reference to the PV move list
  const MoveList& getPV() const { return pv[0]; };

  /// Clears the transposition table.
  void clearTT() const;

  /// Resizes the transposition table according to SearchConfig::TT_SIZE_MB.
  void resizeTT() const;

  /// Returns the search statistics from the last search.
  /// @return Reference to SearchStats
  const SearchStats& getSearchStats() const { return statistics; };

  /// Returns the result of the last completed search.
  /// @return Reference to SearchResult
  const SearchResult& getLastSearchResult() const { return lastSearchResult; };

private:
  ////////////////////////////////////////////////
  ///// PRIVATE

  /// Initializes opening book, transposition table, and other setup tasks.
  /// Idempotent: multiple calls have no additional effect.
  void initialize();

  /// Called after starting search thread. Configures search, calls iterativeDeepening,
  /// and sends result to UCI.
  void run();

  /// Performs iterative deepening search, incrementing depth until time expires.
  /// @param p  Position to search
  /// @return   Search result with best move and score
  SearchResult iterativeDeepening(Position& p);

  /// Searches with a narrow window around expected value, widening on fail-high/low.
  /// @param p          Position to search
  /// @param depth      Current search depth
  /// @param bestValue  Expected value from previous iteration
  /// @return           Search value
  Value aspirationSearch(Position& p, Depth depth, Value bestValue);

  /// Searches root moves (ply 0) with special handling for root node.
  /// @param p      Position to search
  /// @param depth  Remaining depth
  /// @param alpha  Alpha bound
  /// @param beta   Beta bound
  /// @return       Best value found
  Value rootSearch(Position& p, Depth depth, Value alpha, Value beta);

  /// Recursive alpha-beta search for non-root plies (ply > 0).
  /// Handles all major pruning techniques.
  /// @param p       Position to search
  /// @param depth   Remaining depth
  /// @param ply     Current ply from root
  /// @param alpha   Alpha bound
  /// @param beta    Beta bound
  /// @param isPv    Whether this is a PV node
  /// @param doNull  Whether null-move pruning is allowed
  /// @return        Search value
  Value search(Position& p, Depth depth, Depth ply, Value alpha, Value beta, Node_Type isPv, Do_Null doNull);

  /// Quiescence search to resolve tactical sequences at leaf nodes.
  /// Only searches captures, promotions, and checks.
  /// @param p      Position to search
  /// @param ply    Current ply from root
  /// @param alpha  Alpha bound
  /// @param beta   Beta bound
  /// @param isPv   Whether this is a PV node
  /// @return       Quiescence value
  Value qsearch(Position& p, Depth ply, Value alpha, Value beta, Node_Type isPv);

  /// Evaluates a quiet position using the Evaluator.
  /// @param p  Position to evaluate
  /// @return   Evaluation score from side-to-move perspective
  Value evaluate(const Position& p);

  /// Determines if a capture is likely good enough to search in quiescence.
  /// @param p     Position
  /// @param move  Capture move to evaluate
  /// @return      True if capture should be searched
  bool goodCapture(Position& p, Move move) const;

  /// Stores a position entry in the transposition table.
  /// @param p          Position
  /// @param depth      Search depth
  /// @param ply        Current ply (for mate score adjustment)
  /// @param move       Best move found
  /// @param value      Search value
  /// @param valueType  Bound type (EXACT, ALPHA, BETA)
  /// @param eval       Static evaluation
  void storeTt(const Position& p, Depth depth, Depth ply, Move move, Value value, ValueType valueType, Value eval) const;

  /// Saves a move and appends source PV to destination PV.
  /// @param move  Move to prepend
  /// @param src   Source PV to append
  /// @param dest  Destination PV (cleared first)
  static void savePV(Move move, MoveList& src, MoveList& dest);

  /// Adjusts mate score for TT storage (distance from root).
  /// @param value  Score to adjust
  /// @param ply    Current ply
  /// @return       Adjusted score
  static Value valueToTt(Value value, Depth ply);

  /// Adjusts mate score when reading from TT (distance from root).
  /// @param value  Score from TT
  /// @param ply    Current ply
  /// @return       Adjusted score
  static Value valueFromTt(Value value, Depth ply);

  /// Reconstructs PV line from TT entries.
  /// @param p       Position at start of PV
  /// @param pvList  List to fill with PV moves
  /// @param depth   Maximum depth to probe
  void getPvLine(Position& p, MoveList& pvList, Depth depth) const;

  /// Checks if search should stop (stop flag or node limit reached).
  /// @return True if search should terminate
  bool stopConditions();

  /// Logs search limits and sets up time control.
  /// @param p   Current position
  /// @param sl  Search limits to configure
  void setupSearchLimits(const Position& p, SearchLimits& sl);

  /// Calculates time limit for current search based on limits.
  /// @param p       Current position
  /// @param limits  Search limits
  /// @return        Time limit for this search
  milliseconds setupTimeControl(const Position& p, const SearchLimits& limits) const;
  FRIEND_TEST(SearchTest, setupTime);
  FRIEND_TEST(SearchTest, movesLeftBucketsOpeningVsQueenlessVsLowMaterial);
  FRIEND_TEST(SearchTest, movesLeftRepetitionRiskIncreasesTime);

  /// Adds/subtracts time to the current search limit.
  /// @param f  Factor: 1.0 = no change, 0.9 = -10%, 1.1 = +10%
  void addExtraTime(double f);
  FRIEND_TEST(SearchTest, extraTime);

  /// Checks if time is almost exhausted (soft guard for re-searches).
  /// @return True if remaining time is below safety margin
  bool isTimeAlmostUp() const;

  /// Starts timer thread that monitors time limit and sets stop flag.
  void startTimer();
  FRIEND_TEST(SearchTest, startTimer);

  /// Computes position complexity factor (quick version).
  /// @param p  Position to analyze
  /// @return   Factor: >1.0 = more complex, <1.0 = simpler
  static double computeComplexityFactorQuick(const Position& p);

  /// Computes position complexity factor from legal moves.
  /// @param p           Position to analyze
  /// @param legalMoves  List of legal moves
  /// @return            Factor: >1.0 = more complex, <1.0 = simpler
  static double computeComplexityFactorFromMoves(const Position& p, const MoveList& legalMoves);

  /// Checks for draw by repetition or 50-move rule.
  /// @param p                    Position to check
  /// @param numberOfRepetitions  Required repetition count
  /// @return                     True if position is drawn
  static bool checkDrawRepAnd50(const Position& p, int numberOfRepetitions);

  /// Sends "readyok" to UCI handler.
  void sendReadyOk() const;

  /// Sends info string to UCI handler if available.
  /// @param msg  Message to send
  void sendString(const std::string& msg) const;

  /// Sends search result to UCI handler if available.
  /// @param result  Search result to send
  void sendResult(const SearchResult& result) const;

  /// Sends iteration-end info (depth, score, PV, etc.) to UCI.
  void sendIterationEndInfoToUci();

  /// Sends periodic search update (nodes, nps, time, hashfull) to UCI.
  void sendSearchUpdateToUci();

  /// Sends aspiration window research info to UCI.
  /// @param boundString  Bound type ("upperbound" or "lowerbound")
  void sendAspirationResearchInfo(const std::string& boundString);
};

#endif// FRANKYCPP_SEARCH_H
