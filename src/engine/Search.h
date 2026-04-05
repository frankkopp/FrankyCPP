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
//   - Syzygy tablebases for endgame positions
//
// Tablebase Integration (Root Probing):
//   - Probes Syzygy tablebases at root before search
//   - Filters root moves to only TB-optimal moves (maintains WDL)
//   - Uses DTZ-based scoring (shorter wins score higher)
//   - Returns TB move unless search finds proven shorter mate
//   - Configurable via USE_TB_PROBE_ROOT and TB_ROOT_IMMEDIATE
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
// ta
//=============================================================================

#include "PVTable.h"
#include "PawnTT.h"
#include "PlyInfo.h"
#include "SearchLimits.h"
#include "SearchResult.h"
#include "SearchStats.h"
#include "SearchThreadData.h"
#include "TT.h"
#include "chesscore/Position.h"
#include "engine/UciHandler.h"
#include "openingbook/OpeningBook.h"
#include "tablebase/Tablebase.h"
#include "types/types.h"

#include "common/gtest_friends.h"
#include "config/ConfigManager.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <semaphore>
#include <thread>
#include <vector>

// Forward-declare test classes at global scope so FRIEND_TEST inside namespace engine works
FRIEND_TEST_FWD_DECL(SearchTest, setupTime);
FRIEND_TEST_FWD_DECL(SearchTest, movesLeftBucketsOpeningVsQueenlessVsLowMaterial);
FRIEND_TEST_FWD_DECL(SearchTest, movesLeftRepetitionRiskIncreasesTime);
FRIEND_TEST_FWD_DECL(SearchTest, extraTime);
FRIEND_TEST_FWD_DECL(SearchTest, extraTimeCap);
FRIEND_TEST_FWD_DECL(SearchTest, extraTimeClockCap);
FRIEND_TEST_FWD_DECL(SearchTest, startTimer);
FRIEND_TEST_FWD_DECL(SearchTest, startTimerWithOverhead);
FRIEND_TEST_FWD_DECL(SearchSmpTest, selectBestThread);
FRIEND_TEST_FWD_DECL(SearchTest, drawScoreZeroContempt);
FRIEND_TEST_FWD_DECL(SearchTest, drawScorePositiveContempt);
FRIEND_TEST_FWD_DECL(SearchTest, drawScoreNegativeContempt);

namespace engine {
  using namespace chess;

  /// Tracks best-move stability across iterations for time management.
  /// Used to detect when the search is confident (stable) or uncertain (unstable).
  struct BestMoveStability {
    Move lastBestMove{MOVE_NONE};    // best move from previous iteration
    int stabilityCount{0};           // consecutive iterations with same best move
    int changeCount{0};              // number of times best move changed during search
    bool appliedStableFactor{false}; // guard: only apply stable time factor once
    bool appliedExtendFactor{false}; // guard: only apply extend time factor once

    void reset() {
      lastBestMove        = MOVE_NONE;
      stabilityCount      = 0;
      changeCount         = 0;
      appliedStableFactor = false;
      appliedExtendFactor = false;
    }
  };

  class Search {

    // callback handler for UCI communication
    UciHandler* uciHandler{};

    // state management for the search
    mutable std::binary_semaphore initSemaphore{1};
    mutable std::binary_semaphore isRunningSemaphore{1};
    std::thread searchThread{};

    std::unique_ptr<book::OpeningBook> book;
    std::unique_ptr<TT> tt;
    std::unique_ptr<PawnTT> pawnTT;                  // Shared pawn cache for all threads
    std::unique_ptr<tablebase::Tablebase> syzygy_tb; // Syzygy tablebase instance

    // MoveGenerator for PV extraction (reused to avoid allocation per call)
    // Mutable because validateMove() modifies internal lists but not observable state
    mutable MoveGenerator pvMoveGenerator{};

    /// TB root probe result (when TB_ROOT_IMMEDIATE=false, used to guide search).
    /// Groups all tablebase root-probe states into a single struct with reset().
    struct TBRootInfo {
      Move move{MOVE_NONE};                                 ///< Best move from TB at root
      Value value{VALUE_NONE};                              ///< TB score at root (DTZ-adjusted)
      tablebase::TBResult wdl{tablebase::TBResult::Failed}; ///< WDL result for filtering
      int dtz{0};                                           ///< DTZ value for scoring

      /// Resets all TB root state to defaults (no TB hit)
      void reset() {
        move  = MOVE_NONE;
        value = VALUE_NONE;
        wdl   = tablebase::TBResult::Failed;
        dtz   = 0;
      }
    } tbRoot;

    // result of previous search (empty until first search completes)
    std::optional<SearchResult> lastSearchResult{};

    // current position and search limits for the search
    Position position{};
    SearchLimits searchLimits{};

    // Side to move at the root of the search — used for contempt bias.
    // Contempt is positive from root player's perspective: when the side to move
    // at a draw node matches rootColor, drawScore() returns +contempt; otherwise −contempt.
    Color rootColor{WHITE};

    // manage running search
    std::atomic_bool stopSearchFlag = false;

    // Condition variable for efficient waiting when search finishes before stop/ponderhit
    std::condition_variable stopConditionVar{};
    std::mutex stopMutex{};

    // time management for the search
    TimePoint startTime{};     // when startSearch has been called
    TimePoint startSearchTime; // actual start time of search - only different from startTime after ponderhit()
    milliseconds timeLimit{};
    std::atomic<int64_t> extraTimeMs{0};
    std::thread timerThread{};
    mutable std::mutex timerMutex{}; // protects timerThread access from multiple threads

    // Dynamic post-stop overhead measurement for adaptive time management.
    // Measures the wall time between the timer setting stopSearchFlag and sendResult() completing.
    // This overhead (joining helpers, selecting best thread, building result) is subtracted from
    // the timer budget on the next search so bestmove arrives within the allocated time.
    // Initialized to MOVE_OVERHEAD_MS; updated via EMA after each timer-triggered search.
    int64_t measuredPostStopOverheadMs{0}; // initialized in constructor from MOVE_OVERHEAD_MS
    TimePoint timerStopTime{};             // timestamp when timer set stopSearchFlag
    bool stoppedByTimer = false;           // true when stop was triggered by the timer (not external stop/natural finish)

    // Atomic flag to indicate search result is ready (avoids race on lastSearchResult)
    std::atomic<bool> resultReady{false};

    // best-move instability tracking for dynamic time management
    BestMoveStability bestMoveStability{};

    // Control UCI updates to avoid flooding
    constexpr static uint64_t UCI_UPDATE_INTERVAL = nanoPerSec;
    uint64_t lastUciUpdateTime{};
    uint64_t lastUciUpdateNodes{};
    uint64_t npsTime{};
    uint64_t npsNodes{};

    // to mark the last move was a book move
    bool hadBookMove = false;

    // reference to the Search Config Data
    const config::SearchConfigData& SearchConfig;

    // ===========================================================================
    /// LazySMP per-thread search state (main thread + helpers)

    // Thread 0 is the main thread. Helper threads will be indices [1..N-1].
    std::vector<std::unique_ptr<SearchThreadData>> searchThreadData{};

    // Helper thread handles - empty when numHelperThreads == 0 (single-threaded)
    std::vector<std::thread> helperThreads{};

    // Number of helper threads (0 = single-threaded, N = N helpers + 1 main)
    // Set from SearchConfig.THREADS - 1 before search starts
    int numHelperThreads = 0;

    // Flag to track if helper threads have been launched for current search.
    // Helpers are launched after main thread completes SMP_HELPER_START_DEPTH iterations
    // to allow TT priming before helpers start contributing.
    bool helpersLaunched = false;

    // Thread-local pointer to current thread's SearchThreadData.
    // Set by run() for main thread, launchHelperThreads() lambda for helpers.
    // Enables search functions to access thread-local state without parameter passing.
    static inline thread_local SearchThreadData* currentThreadData = nullptr;

    // ===========================================================================

  public:
    /// Node type classification for alpha-beta search.
    /// - PvNode: Principal Variation node, full window (alpha, beta), expected exact score
    /// - CutNode: Expected to fail high (beta cutoff), null window search
    /// - AllNode: Expected to fail low (no move raises alpha), null window search
    enum NodeType : uint8_t {
      PvNode,  // Principal Variation node (full window)
      CutNode, // Expected fail-high node (null window)
      AllNode  // Expected fail-low node (null window)
    };

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

    /// Stops any running search and resets all state for a new game.
    /// Clears: TT, PawnTT, all SearchThreadData (history, statistics, PV, plyStack),
    /// best-move stability tracker, TB root info, book move flag, last search result,
    /// result-ready flag, and dynamic post-stop overhead estimate.
    void newGame();

    /// Signals readiness to the UCI interface after initialization.
    /// Part of UCI protocol; may trigger time-consuming setup on first call.
    void isReady();

    /// Starts an asynchronous search in a separate thread.
    /// @param p   Position to search
    /// @param sl  Search limits (time, depth, nodes, etc.)
    void startSearch(const Position& p, const SearchLimits& sl);

    /// Stops a running search gracefully, returning the best move found so far.
    void stopSearch();

    /// Checks if the search is currently running.
    /// @return True if search is in progress
    bool isSearching() const;

    /// Blocks the calling thread until the search completes.
    void waitWhileSearching() const;

    /// Signals that pondering was successful (opponent played expected move).
    void ponderhit();

    /// Returns the principal variation from the current/last search.
    /// @return The PV move list (extracted from triangular table)
    MoveList getPV() const { return mainThread().pv.extract(); }

    /// Clears the transposition table.
    void clearTT() const;

    /// Resizes the transposition table according to SearchConfig::TT_SIZE_MB.
    void resizeTT() const;

    /// Returns the search statistics from the last search.
    /// @return Reference to SearchStats
    const SearchStats& getSearchStats() const { return mainThread().statistics; };

    /// Returns the main search thread state.
    /// Thread 0 is always the main thread; helper threads are indices [1..N-1].
    /// @return Reference to main SearchThreadData
    SearchThreadData& mainThread() { return *searchThreadData[0]; }

    /// Const version of mainThread() for read-only access to main thread state.
    /// @return Const reference to main SearchThreadData
    const SearchThreadData& mainThread() const { return *searchThreadData[0]; }

    /// Returns the total node count aggregated across all search threads.
    /// Used for UCI reporting (info nodes).
    /// @return Sum of nodesVisited from all SearchThreadData instances
    [[nodiscard]] uint64_t getTotalNodes() const;

    /// Returns the current thread's SearchThreadData.
    /// Uses thread-local storage set by run() or launchHelperThreads().
    /// Must only be called from within a search context (after currentThreadData is set).
    /// @return Reference to current thread's SearchThreadData
    static SearchThreadData& thread() {
      assert(currentThreadData != nullptr && "thread() called outside of search context");
      // ReSharper disable once CppDFANullDereference
      return *currentThreadData;
    }

    /// Returns true if the current thread is the main thread (thread ID 0).
    /// Used to guard main-thread-only logic (UCI output, time management, TB probing).
    /// @return True if current thread is main thread
    [[nodiscard]] static bool isMainThread() { return thread().id == 0; }

    /// Returns the result of the last completed search.
    /// @return Reference to SearchResult (undefined behavior if no search completed)
    /// @pre hasResult() returns true
    const SearchResult& getLastSearchResult() const { return *lastSearchResult; };

    /// Checks if a search result is available.
    /// Thread-safe: uses atomic flag to avoid race with search thread.
    /// @return True if result is ready
    [[nodiscard]] bool hasResult() const { return resultReady.load(std::memory_order_acquire); }

    /// Formats detailed search statistics as a string for debugging/logging.
    /// Static version that takes result and stats as parameters.
    /// @param result  Search result with best move, score, depth, FEN, etc.
    /// @param stats   Search statistics with pruning counts, TT hits, etc.
    /// @return        Formatted multi-line string with all statistics
    [[nodiscard]] std::string formatDetailedStats(const SearchResult& result, const SearchStats& stats) const;

    /// Formats detailed search statistics as a string for debugging/logging.
    /// Uses this search instance's last result and statistics.
    /// Aggregates stats from all search threads (main + helpers) for SMP.
    /// @return     Formatted multi-line string with all statistics
    [[nodiscard]] std::string formatDetailedStats() const;

    /// Aggregates search statistics from all threads (main + helpers).
    /// For SMP, each thread maintains its own SearchStats; this combines them.
    /// @return  Combined statistics from all search threads
    [[nodiscard]] SearchStats aggregateStats() const;

  private:
    ////////////////////////////////////////////////
    ///// PRIVATE

    /// Initializes opening book, transposition table, and other setup tasks.
    /// Idempotent: multiple calls have no additional effect.
    void initialize();

    /// Initializes Syzygy tablebases from configured path.
    /// Called during initialize(). Safe to call multiple times.
    void initTablebase();

    /// Probes tablebase at root position before iterative deepening.
    /// If successful and USE_TB_PROBE_ROOT is enabled, populates the search result.
    /// @param pos     Position to probe
    /// @param result  Search result to populate on successful probe
    /// @return true if TB hit occurred and result was populated
    bool probeTablebaseAtRoot(const Position& pos, SearchResult& result);

    /// Applies tablebase root override on top of best-thread selection.
    /// If a TB hit occurred at root, uses the TB-optimal (DTZ) move unless
    /// the search found a proven shorter mate.
    /// @param result  Search result to update with TB move/value
    void applyTBRootOverride(SearchResult& result) const;

    /// Extracts and validates the ponder move after search completes.
    /// Tries the best thread's PV first; falls back to TT probing.
    /// Validates legality and filters out moves leading to drawn/mated positions.
    /// @param result      Search result to update with ponder move
    /// @param bestThread  Best thread selected after search (for PV access)
    void extractPonderMove(SearchResult& result, const SearchThreadData& bestThread);

    /// Filters root moves to only those that maintain the TB result.
    /// Called when TB_ROOT_IMMEDIATE=false to ensure optimal play while searching.
    /// Removes moves that would worsen WDL (e.g., Win -> Draw or Draw -> Loss).
    /// @param pos  Current position (used to probe child positions)
    void filterRootMovesByTB(Position& pos) const;

    /// Converts TB WDL result to search score with 50-move rule handling.
    /// Uses TB_RULE50_THRESHOLD to decide if DTZ check is needed.
    /// @param wdl            WDL result from tablebase
    /// @param halfMoveClock  Current 50-move counter
    /// @param ply            Current search ply (for mate-distance scoring)
    /// @return               Score suitable for alpha-beta search
    [[nodiscard]] Value getTBScoreForSearch(tablebase::TBResult wdl, int halfMoveClock, Depth ply) const;

    /// Resets Search-owned state at the start of each search (before thread init).
    /// Clears stop flag, result, time limits, and UCI update tracking.
    /// Called from run() before initialize() and thread data setup.
    void resetSearchState();

    /// Called after starting search thread. Configures search, calls iterativeDeepening,
    /// and sends result to UCI.
    void run();

    /// Launches helper threads for Lazy SMP.
    /// Called from iterativeDeepening() after the main thread has completed a few iterations
    /// to allow TT priming before helpers start contributing.
    /// Helpers run full iterativeDeepening() with guards for main-thread-only logic.
    /// No-op if helpers already launched or numHelperThreads == 0.
    void launchHelperThreads();

    /// Joins all helper threads and clears the thread vector.
    /// Called from destructor, startSearch() (defensive), and run() (post-search cleanup).
    void joinHelperThreads();

    /// Performs iterative deepening search, incrementing depth until time expires.
    /// @param p  Position to search
    /// @return   Search result with best move and score
    SearchResult iterativeDeepening(Position& p);

    /// Aspiration window search with exponential widening.
    /// Searches with a narrow window around expected value, using exponential widening
    /// on fail-high/low (delta grows by delta/ASP_DELTA_GROWTH_DIVISOR each fail).
    /// Value-centered: re-search windows are centered on the actual search result,
    /// not the stale bestValue from the previous iteration.
    /// Mate bypass: if bestValue is a checkmate score, skips aspiration entirely
    /// and performs a full-window search to avoid losing confirmed mates.
    /// @param p          Position to search
    /// @param depth      Current search depth
    /// @param bestValue  Expected value from previous iteration
    /// @return           Search value
    Value aspirationSearch(Position& p, Depth depth, Value bestValue);

    /// Searches root moves (ply 0) with special handling for root node.
    /// @param p           Position to search
    /// @param depth       Remaining depth
    /// @param alpha       Alpha bound
    /// @param beta        Beta bound
    /// @param startIndex  First index in rootMoves to search (0..N-1, for MultiPV)
    /// @return            Best value found
    Value rootSearch(Position& p, Depth depth, Value alpha, Value beta, int startIndex = 0);

    /// Recursive alpha-beta search for non-root plies (ply > 0).
    /// Handles all major pruning techniques.
    /// @param p        Position to search
    /// @param depth    Remaining depth
    /// @param ply      Current ply from root
    /// @param alpha    Alpha bound
    /// @param beta     Beta bound
    /// @param nodeType Node type: PvNode (full window), CutNode (expect fail-high), AllNode (expect fail-low)
    /// @param doNull   Whether null-move pruning is allowed
    /// @return         Search value
    Value search(Position& p, Depth depth, Depth ply, Value alpha, Value beta, NodeType nodeType, Do_Null doNull);

    /// Quiescence search to resolve tactical sequences at leaf nodes.
    /// Only searches captures, promotions, and checks.
    /// @param p        Position to search
    /// @param ply      Current ply from root
    /// @param alpha    Alpha bound
    /// @param beta     Beta bound
    /// @param nodeType Node type: PvNode or non-PV (AllNode/CutNode treated same in qsearch)
    /// @return       Quiescence value
    Value qsearch(Position& p, Depth ply, Value alpha, Value beta, NodeType nodeType);

    /// Evaluates a quiet position using the Evaluator.
    /// @param p  Position to evaluate
    /// @return   Evaluation score from side-to-move perspective
    Value evaluate(const Position& p);

    /// Selects the best thread based on completed depth and score after search ends.
    /// Uses depth + score heuristic: deeper thread wins unless its score is worse
    /// by more than BEST_THREAD_SCORE_MARGIN centipawns.
    /// Called after all helper threads have joined.
    /// @return Pointer to the SearchThreadData with the best result
    [[nodiscard]] const SearchThreadData* selectBestThread() const;
    FRIEND_TEST_NS(SearchSmpTest, selectBestThread);

    /// Sends a final UCI info line with the best thread's result before bestmove.
    /// Ensures the GUI shows depth/score/PV consistent with the final bestmove.
    /// Only called when a non-main thread is selected.
    /// @param bestThread  The selected best thread's data
    void sendFinalUciInfo(const SearchThreadData& bestThread) const;

    /// Determines if a capture is likely good enough to search in quiescence.
    /// @param p     Position
    /// @param move  Capture move to evaluate
    /// @param givesCheck if the move on the position gives check (captures that give check are more likely to be good)
    /// @return      True if capture should be searched
    /// @note        As givesCheck is already available at the time we call this function, we pass it
    ///              as an argument to avoid redundant calls to p.givesCheck() inside this function, which can be costly.
    bool goodCapture(const Position& p, Move move, bool givesCheck) const;

    /// Stores a position entry in the transposition table.
    /// @param p          Position
    /// @param depth      Search depth
    /// @param ply        Current ply (for mate score adjustment)
    /// @param move       Best move found
    /// @param value      Search value
    /// @param valueType  Bound type (EXACT, ALPHA, BETA)
    /// @param eval       Static evaluation
    void storeTt(const Position& p, Depth depth, Depth ply, Move move, Value value, ValueType valueType, Value eval) const;

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
    FRIEND_TEST_NS(SearchTest, setupTime);
    FRIEND_TEST_NS(SearchTest, movesLeftBucketsOpeningVsQueenlessVsLowMaterial);
    FRIEND_TEST_NS(SearchTest, movesLeftRepetitionRiskIncreasesTime);

    /// Adds/subtracts time to the current search limit.
    /// @param f  Factor: 1.0 = no change, 0.9 = -10%, 1.1 = +10%
    void addExtraTime(double f);
    FRIEND_TEST_NS(SearchTest, extraTime);
    FRIEND_TEST_NS(SearchTest, extraTimeCap);
    FRIEND_TEST_NS(SearchTest, extraTimeClockCap);

    /// Checks if time is almost exhausted (soft guard for re-searches).
    /// @return True if remaining time is below safety margin
    bool isTimeAlmostUp() const;

    /// Starts timer thread that monitors time limit and sets stop flag.
    /// This does not set the startSearchTime - it only starts the timer thread that
    /// will monitor the time and set the stop flag when time is up. The actual
    /// startSearchTime is set in run() when the search thread starts, to ensure
    /// accurate timing from the moment search begins (after ponderhit).
    void startTimer();
    FRIEND_TEST_NS(SearchTest, startTimer);
    FRIEND_TEST_NS(SearchTest, startTimerWithOverhead);

    /// Joins the timer thread if it is still running (protected by timerMutex).
    /// Called from run() post-search cleanup.
    void joinTimerThread();

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

    /// Returns the contempt-biased draw score for the current position.
    /// When CONTEMPT is 0, returns VALUE_DRAW (== 0). Otherwise returns
    /// +CONTEMPT when the side to move at the draw node is the root player
    /// (engine avoids draws), or −CONTEMPT when it's the opponent's turn
    /// (engine steers opponent toward draws).
    /// @param p  Position at the draw node (side to move is inspected)
    /// @return   Contempt-biased draw value
    [[nodiscard]] Value drawScore(const Position& p) const;
    FRIEND_TEST_NS(SearchTest, drawScoreZeroContempt);
    FRIEND_TEST_NS(SearchTest, drawScorePositiveContempt);
    FRIEND_TEST_NS(SearchTest, drawScoreNegativeContempt);

    /// Sends "readyok" to UCI handler.
    void sendReadyOk() const;

    /// Sends info string to UCI handler if available.
    /// @param msg  Message to send
    void sendString(const std::string& msg) const;

    /// Returns true if the UCI handler has debug mode enabled.
    /// Used to guard additional diagnostic info string output.
    [[nodiscard]] bool isDebugMode() const { return uciHandler && uciHandler->isDebugMode(); }

    /// Sends debug eval breakdown for the current root position via info string.
    /// Only called when debug mode is on and main thread completes an iteration.
    void sendDebugEvalInfo() const;

    /// Sends search result to UCI handler if available.
    /// @param result  Search result to send
    void sendResult(const SearchResult& result) const;

    /// Sends iteration-end info (depth, score, PV, etc.) to UCI.
    void sendIterationEndInfoToUci();

    /// Holds collected PV data for deferred, sorted MultiPV reporting.
    /// Collected during the MultiPV loop, sorted by score, then reported in batch.
    struct MultiPvResult {
      MoveList pvLine;  ///< PV line extracted via extractPvWithTT
      Value score;      ///< Score from rootSearch for this PV
      int seldepth;     ///< Selective depth at time of search
    };

    /// Sends all collected MultiPV results to UCI in a single batch.
    /// Results must already be sorted by score (descending).
    /// @param results        Collected PV results (sorted by score descending)
    /// @param iterationDepth Current iteration depth
    void sendMultiPvResultsToUci(const std::vector<MultiPvResult>& results, Depth iterationDepth);

    /// Sends periodic search update (nodes, nps, time, hashfull) to UCI.
    void sendSearchUpdateToUci();

    /// Sends aspiration window research info to UCI.
    /// @param boundString  Bound type ("upperbound" or "lowerbound")
    void sendAspirationResearchInfo(const std::string& boundString) const;

    /// Extracts PV from triangular table, extending it using TT lookups.
    /// This ensures full PV lines are available even after TT cutoffs.
    /// @param p  Position to use for TT probing (will be modified and restored)
    /// @return   Extended PV as MoveList
    MoveList extractPvWithTT(Position& p) const;
  };

} // namespace engine

#endif // FRANKYCPP_SEARCH_H
