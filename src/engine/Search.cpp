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

#include "Search.h"
#include "Evaluator.h"
#include "Handicap.h"
#include "See.h"
#include "common/Logging.h"
#include "common/misc.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

using namespace engine;
using namespace chess;
using namespace config;
using namespace common;

////////////////////////////////////////////////
///// CONSTRUCTORS

Search::Search() : Search(nullptr) {}

Search::Search(UciHandler* pUciHandler)
    : uciHandler(pUciHandler), SearchConfig(ConfigManager::instance().search()) {
  // Initialize Transposition Table with 0 MB (will be resized in initialize() based on config)
  this->tt = std::make_unique<TT>(0);
  // Initialize main search thread (thread ID 0)
  searchThreadData.push_back(std::make_unique<SearchThreadData>(0));
  // Bootstrap dynamic post-stop overhead with configured static overhead
  measuredPostStopOverheadMs = SearchConfig.MOVE_OVERHEAD_MS;
}

Search::~Search() {
  // Signal stop to all threads
  stopSearchFlag = true;
  stopConditionVar.notify_all();

  // First, join the main search thread. This is important because:
  // 1. The main search thread's run() method joins the helper threads
  // 2. If we try to join helpers here while run() is also trying to join them,
  //    we'd have a problem (double-join or race condition)
  // By joining the main thread first, we ensure run() has completed its cleanup.
  if (searchThread.joinable()) {
    searchThread.join();
  }

  // After the main thread is joined, run() should have already joined all helpers.
  // But as a safety net (e.g., if destruction happens during unusual circumstances),
  // we also attempt to join any remaining helper threads.
  joinHelperThreads();
}

////////////////////////////////////////////////
///// PUBLIC

void Search::newGame() {
  if (isSearching()) stopSearch();

  // Reset all thread data (not just main thread) for clean state
  // This ensures deterministic behavior when switching between thread counts
  for (const auto& threadData : searchThreadData) {
    threadData->reset();
  }

  if (tt) { tt->clear(); }
  if (pawnTT) { pawnTT->clear(); }
  bestMoveStability.reset();
  tbRoot.reset();
  hadBookMove = false;

  // Clear stale result from previous game
  lastSearchResult.reset();
  resultReady.store(false, std::memory_order_relaxed);

  // Reset dynamic overhead estimate — different games may have different time controls,
  // so the learned EMA from the previous game is not transferable.
  measuredPostStopOverheadMs = SearchConfig.MOVE_OVERHEAD_MS;
}

void Search::isReady() {
  initialize();
  sendReadyOk();
}

void Search::startSearch(const Position& p, const SearchLimits& sl) {
  // acquire init phase lock
  if (!initSemaphore.try_acquire()) {
    LOG__ERROR(Logger::get().SEARCH_LOG, "Search init failed as another initialization is ongoing.");
    return;
  }

  // start search time
  startTime       = currentTime();
  startSearchTime = startTime;

  // move the received copy of position and search limits to instance variables
  this->position = p;
  searchLimits   = sl;

  // join() previous search thread - this ensures run() has completed and
  // helper threads have been joined by run() before we start a new search
  if (searchThread.joinable()) { searchThread.join(); }

  // Safety: ensure no helper threads are lingering from a previous search
  // (should already be joined by run(), but defensive programming)
  joinHelperThreads();

  // ===========================================================================
  // start search in a separate thread
  // ===========================================================================
  LOG__DEBUG(Logger::get().SEARCH_LOG, "Starting search in separate thread.");
  searchThread = std::thread(&Search::run, this);

  // wait until search is running and initialization
  // is done before returning to caller
  initSemaphore.acquire();
  initSemaphore.release();
  LOG__INFO(Logger::get().SEARCH_LOG, "Search started.");
}

void Search::stopSearch() {
  if (!isSearching()) {
    LOG__WARN(Logger::get().SEARCH_LOG, "Stop search called when search was not running");
    return;
  }
  LOG__INFO(Logger::get().SEARCH_LOG, "Search stopped.");
  stopSearchFlag = true;
  stopConditionVar.notify_all();
  // Wait for the thread to die
  if (searchThread.joinable()) { searchThread.join(); }
  waitWhileSearching();
}

bool Search::isSearching() const { // NOLINT(*-convert-member-functions-to-static)
  // Try to get running semaphore.
  // If not available, the search is running
  if (isRunningSemaphore.try_acquire()) {
    isRunningSemaphore.release();
    return false;
  }
  return true;
}

void Search::waitWhileSearching() const {
  // get or wait for running semaphore
  isRunningSemaphore.acquire();
  isRunningSemaphore.release();
}

void Search::ponderhit() {
  if (isSearching() && searchLimits.ponder) {
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Ponderhit during search - activating time control");
    startTimer();
    return;
  }
  LOG__WARN(Logger::get().SEARCH_LOG, "Ponderhit received while not pondering");
}

void Search::clearTT() const {
  if (isSearching()) {
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string msg = "Can't clear hash while searching.";
    sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
    return;
  }
  tt->clear();
  // ReSharper disable once CppVariableCanBeMadeConstexpr
  const std::string msg = "Hash cleared.";
  sendString(msg);
  LOG__INFO(Logger::get().SEARCH_LOG, "{}", msg);
}

void Search::resizeTT() const {
  if (isSearching()) {
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    const std::string msg = "Can't resize hash while searching.";
    sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
    return;
  }
  // Resize the existing TT to the configured size and clear it
  tt->resize(SearchConfig.TT_SIZE_MB);
  sendString("Resized hash: " + tt->str());
}

uint64_t Search::getTotalNodes() const {
  uint64_t total = 0;
  // Only sum active threads [0..numHelperThreads], not the entire vector.
  // The vector grows but never shrinks, so stale entries beyond the active
  // thread count could contain non-zero nodesVisited from previous searches
  // with a higher thread count.
  const int activeThreads = numHelperThreads + 1;
  const auto count        = std::min(static_cast<int>(searchThreadData.size()), activeThreads);
  for (int i = 0; i < count; ++i) {
    total += searchThreadData[i]->nodesVisited;
  }
  return total;
}

////////////////////////////////////////////////
///// PRIVATE

void Search::resetSearchState() {
  stopSearchFlag    = false;
  stoppedByTimer    = false;
  timeLimit         = milliseconds{};
  extraTimeMs       = 0;
  lastUciUpdateTime = now();
  resultReady.store(false, std::memory_order_relaxed); // clear result flag
  lastSearchResult.reset();                            // clear previous result
}

void Search::run() {
  // check if there is already a search running
  // and if not, grab the isRunning semaphore
  if (!isRunningSemaphore.try_acquire()) {
    LOG__ERROR(Logger::get().SEARCH_LOG, "Search is already running");
    return;
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "Searching {}", position.strFen());

  // initialize search state (stop flag, result, time limits, UCI tracking)
  resetSearchState();

  // Store root color for contempt bias (used by drawScore())
  rootColor = position.getNextPlayer();

  // Note: npsTime and npsNodes are initialized later, right before iterative deepening,
  // to avoid including initialization overhead in NPS calculations
  initialize();

  // ===========================================================================
  // Initialize thread data for all search threads (main + helpers)
  // ===========================================================================

  // Determine number of helper threads from config (0 = single-threaded)
  numHelperThreads       = std::max(0, SearchConfig.THREADS - 1);
  const int totalThreads = numHelperThreads + 1;

  // Set SMP thread count for TT and PawnTT thread safety
  // When > 1: TT skips age-- to avoid bitfield race, PawnTT suppresses update warnings
  tt->setSmpThreads(totalThreads);
  pawnTT->setSmpThreads(totalThreads);

  // Ensure we have enough SearchThreadData instances allocated
  while (searchThreadData.size() < static_cast<size_t>(totalThreads)) {
    searchThreadData.push_back(std::make_unique<SearchThreadData>(static_cast<int>(searchThreadData.size())));
  }

  // Reset and initialize all thread data
  for (int t = 0; t < totalThreads; ++t) {
    searchThreadData[t]->resetForNewSearch(
      position, pawnTT.get(),
      SearchConfig.LMR_USE_LOG_FORMULA, SearchConfig.LMR_LOG_BASE_DIV,
      SearchConfig.USE_HISTORY_COUNTER || SearchConfig.USE_HISTORY_MOVES);
  }

  LOG__DEBUG(Logger::get().SEARCH_LOG, "Initialized {} search thread(s) ({} helper(s))", totalThreads, numHelperThreads);

  // ===========================================================================
  // End thread data initialization
  // ===========================================================================

  // set up and report search limits
  setupSearchLimits(position, searchLimits);

  // when not pondering and search is time controlled start timer
  if (searchLimits.timeControl && !searchLimits.ponder) { startTimer(); }

  // age tt entries (skip in SMP mode - age-- in probe() is disabled for thread safety)
  if (SearchConfig.USE_TT) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table: Using TT: {}", tt->str());
    if (numHelperThreads == 0) {
      tt->ageEntries();
    }
  }
  else { LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table: Not using TT."); }

  // release the init phase lock to signal the calling go routine
  // waiting in StartSearch() to return
  initSemaphore.release();

  // check for opening book move when we have a time-controlled game
  Move bookMove = MOVE_NONE;
  if (book && SearchConfig.USE_BOOK && searchLimits.timeControl) {
    bookMove = book->getBookMove(position.getZobristKey(), SearchConfig.BOOK_VARIETY);
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Opening Book: Choosing book move {}", bookMove.str());
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Opening Book: Not using book.");
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "Search using: PVS={} ASP={}", SearchConfig.USE_PVS, SearchConfig.USE_ASP);

  // Set thread-local pointer to main thread's data for search functions
  currentThreadData = &mainThread();

  // Reset helper thread state - helpers will be launched from iterativeDeepening()
  // after main thread has completed SMP_HELPER_START_DEPTH iterations (TT priming)
  helperThreads.clear();
  helpersLaunched = false;

  // Prepare search result with position info (FEN, etc.)
  // Will be updated with best move, score, PV, etc. after search
  SearchResult searchResult{position};

  // ===========================================================================
  // ITERATIVE DEEPENING
  // ===========================================================================
  if (!bookMove) {
    // Start search with iterative deepening if we did not find a book move.
    searchResult = iterativeDeepening(position);
  }
  else {
    // If we have found a book-move, update result and omit search.
    searchResult.bestMove = bookMove;
    searchResult.bookMove = true;
    searchResult.threads  = numHelperThreads + 1;
    hadBookMove           = true;
    if (isDebugMode()) {
      sendString(std::format("book move: {}", bookMove.str()));
    }
  }
  // ===========================================================================
  // /END ITERATIVE DEEPENING
  // ===========================================================================

  // If we arrive here during Ponder mode or Infinite mode and the search is not
  // stopped, it means that the search was finished before it has been stopped
  // by stopSearchFlag or ponderhit;
  // We wait here until the search has completed.
  if (!stopSearchFlag && (searchLimits.ponder || searchLimits.infinite)) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Search finished before stopped or ponderhit! Waiting for stop/ponderhit to send result");
    std::unique_lock lock(stopMutex);
    stopConditionVar.wait(lock, [this] {
      return stopSearchFlag.load() || !(searchLimits.ponder || searchLimits.infinite);
    });
  }

  // Clean up
  // make sure the timer stops as this could potentially still be running
  // when the search finished without any stop signal/limit
  stopSearchFlag = true;

  // ===========================================================================
  // Join all helper threads before extracting results
  // ===========================================================================
  joinHelperThreads();
  // ===========================================================================

  // Aggregate node count from all threads
  const uint64_t totalNodes = getTotalNodes();

  // Select the best thread (after all helpers have stopped)
  const SearchThreadData* bestThread = selectBestThread();

  // Override search result with best thread's data, but only if a real iteration completed.
  // Early exits (checkmate/stalemate/draw positions) return before any iteration runs,
  // so completedIterationDepth remains DEPTH_NONE and the PV is empty.
  // In that case, keep the searchResult as set by iterativeDeepening() (e.g., -VALUE_CHECKMATE).
  if (bestThread->completedIterationDepth != DEPTH_NONE) {
    searchResult.bestMove      = bestThread->pv.first().stripped();
    searchResult.bestMoveValue = bestThread->pv.first().value();
    searchResult.depth         = bestThread->completedIterationDepth;
    searchResult.extraDepth    = bestThread->statistics.currentExtraSearchDepth;
    searchResult.pv            = bestThread->pv.extract();
  }

  // Derive mateFound from the final result after best-thread selection.
  // This ensures consistency between mateFound and bestMoveValue regardless
  // of which thread was selected.
  if (searchLimits.mate) {
    const Value finalValue = searchResult.bestMoveValue;
    searchResult.mateFound = finalValue.isCheckMate()
                             && searchLimits.mate * 2 - 1
                                  == static_cast<int>(VALUE_CHECKMATE) - static_cast<int>(finalValue);
  }

  searchResult.time    = currentTime() - startSearchTime;
  searchResult.nodes   = totalNodes;
  searchResult.threads = numHelperThreads + 1;

  // Apply tablebase root override on top of best-thread selection
  applyTBRootOverride(searchResult);

  // Apply handicap move selection OR extract ponder move (mutually exclusive).
  // Handicap overrides the best move with a suboptimal pick from the candidate pool
  // and disables pondering. Must come after TB override (TB moves are not overridden).
  if (SearchConfig.HANDICAP > 0) {
    applyHandicap(searchResult);
  }
  else {
    // Extract and validate ponder move from PV or TT (normal play only)
    extractPonderMove(searchResult, *bestThread);
  }

  // Send final UCI info line if a non-main thread was selected.
  // This ensures the GUI shows depth/score/PV consistent with the final bestmove.
  // Suppressed when MultiPV > 1: sending a single "multipv 1" line here would
  // overwrite the GUI's complete multi-PV display from the last completed iteration
  // with a potentially different depth/score from a helper thread.
  if (bestThread->id != 0 && SearchConfig.MULTI_PV <= 1) {
    sendFinalUciInfo(*bestThread);
  }

  // ===========================================================================
  // Non-critical post-search work (logging, timer cleanup)
  // Moved BEFORE sendResult to minimize latency between result ready and bestmove output.
  // ===========================================================================

  // print stats to log
  LOG__INFO(Logger::get().SEARCH_LOG, "Search finished after {}", str(searchResult.time));
  LOG__INFO(Logger::get().SEARCH_LOG, "Search depth was {}({}) with {:L} nodes visited. NPS = {:L} nps with {} threads",
            bestThread->completedIterationDepth, bestThread->statistics.currentExtraSearchDepth,
            totalNodes, nps(totalNodes, searchResult.time), searchResult.threads);
  LOG__DEBUG(Logger::get().SEARCH_LOG, "Search stats: {}", thread().statistics.str());

  // print result to log
  if (searchLimits.mate && searchResult.mateFound) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Mate in {} found: {}", searchLimits.mate, bestThread->pv.first().str());
  }
  LOG__INFO(Logger::get().SEARCH_LOG, "Search result: {}", searchResult.str());

  // clean up timer thread if necessary
  joinTimerThread();

  // Reset thread-local pointer to prevent dangling reference if this thread
  // is reused by another Search instance
  currentThreadData = nullptr;

  // ===========================================================================
  // Dynamic post-stop overhead measurement
  // Measures wall time from timer-triggered stop to sendResult for adaptive time management.
  // Updates an EMA that is subtracted from the timer budget on the next search.
  // ===========================================================================
  if (stoppedByTimer) {
    const auto postStopNs           = (currentTime() - timerStopTime).count(); // nanoseconds
    const auto sampleMs             = static_cast<double>(postStopNs) / 1'000'000.0;
    constexpr int64_t maxOverheadMs = 200;
    const int64_t floorMs           = SearchConfig.MOVE_OVERHEAD_MS;
    // EMA: 30% new sample, 70% previous estimate
    const auto rawEmaMs        = 0.3 * sampleMs + 0.7 * static_cast<double>(measuredPostStopOverheadMs);
    measuredPostStopOverheadMs = std::clamp(static_cast<int64_t>(std::llround(rawEmaMs)), floorMs, maxOverheadMs);
    LOG__INFO(Logger::get().SEARCH_LOG,
              "Post-stop overhead: sample {:.3f} ms, EMA {} ms (floor {} ms, cap {} ms)",
              sampleMs, measuredPostStopOverheadMs, floorMs, maxOverheadMs);
  }

  // ===========================================================================
  // Send result and release semaphore
  // sendResult() MUST be the last significant action before isRunningSemaphore.release().
  // Once bestmove is sent, the GUI may immediately send the next go command.
  // The semaphore release gates isSearching() which gates startSearch().
  // ===========================================================================

  // save the result until overwritten by the next search
  lastSearchResult = searchResult;
  resultReady.store(true, std::memory_order_release); // signal result is ready

  // Send the result - this must be the last action before releasing the semaphore
  sendResult(searchResult);

  // release the running semaphore after the search has ended
  isRunningSemaphore.release();
}

void Search::launchHelperThreads() {
  // No-op if already launched or no helpers configured
  if (helpersLaunched || numHelperThreads == 0) {
    return;
  }

  // Position was already copied to each thread's SearchThreadData in run()
  // Launch all helper threads - they run full iterativeDeepening() just like main thread
  // Guards inside iterativeDeepening() handle main-thread-only logic (UCI output, time management, etc.)
  for (int i = 1; i <= numHelperThreads; ++i) {
    helperThreads.emplace_back([this, i] {
      // Set thread-local pointer so search functions use this thread's data
      currentThreadData = searchThreadData[i].get();

      LOG__DEBUG(Logger::get().SEARCH_LOG, "Helper thread {} starting iterative deepening", i);

      // Run full iterative deepening (same search quality as main thread)
      // Helpers benefit from: aspiration windows, proper move ordering, LMR, etc.
      (void) iterativeDeepening(searchThreadData[i]->position);

      LOG__DEBUG(Logger::get().SEARCH_LOG, "Helper thread {} finished, searched {:L} nodes",
                 i, searchThreadData[i]->nodesVisited);

      // Reset thread-local pointer
      currentThreadData = nullptr;
    });
  }

  helpersLaunched = true;
  LOG__INFO(Logger::get().SEARCH_LOG, "Launched {} helper thread(s) after depth {} (TT priming complete)",
            numHelperThreads, SearchConfig.SMP_HELPER_START_DEPTH);
}

void Search::joinHelperThreads() {
  if (helperThreads.empty()) return;
  const int count = static_cast<int>(helperThreads.size());
  for (auto& t : helperThreads) {
    if (t.joinable()) { t.join(); }
  }
  helperThreads.clear();
  LOG__DEBUG(Logger::get().SEARCH_LOG, "All {} helper thread(s) joined", count);
}

SearchResult Search::iterativeDeepening(Position& p) {
  SearchResult searchResult{p};

  // check repetition and 50-moves rule
  if (checkDrawRepAnd50(p, 2)) {
    const std::string msg = searchLimits.ponder
                              ? "Ponder called on DRAW by Repetition or 50-moves-rule"
                              : "Search called on DRAW by Repetition or 50-moves-rule";
    if (isMainThread()) sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
    searchResult.bestMoveValue = drawScore(p);
    return searchResult;
  }

  // generate all legal root moves for the position
  thread().rootMoves = *thread().plyStack[0].mg->generateLegalMoves(p, GenAll);

  // check if there are legal moves - if not, it's mate or stalemate
  if (thread().rootMoves.empty()) {
    if (p.hasCheck()) {
      STAT_INC(thread().statistics.checkmates);
      const std::string msg = searchLimits.ponder
                                ? "Ponder called on a check mate position"
                                : "Search called on a check mate position";
      if (isMainThread()) sendString(msg);
      LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
      searchResult.bestMoveValue = -VALUE_CHECKMATE;
    }
    else {
      STAT_INC(thread().statistics.stalemates);
      const std::string msg = searchLimits.ponder
                                ? "Ponder called on a stale mate position"
                                : "Search called on a stale mate position";
      if (isMainThread()) sendString(msg);
      LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
      searchResult.bestMoveValue = drawScore(p);
    }
    return searchResult;
  }

  // Reset TB root info from previous search (main thread only)
  // Helpers don't do TB probing - they benefit from main thread's TT entries
  if (isMainThread()) {
    tbRoot.reset();

    // Probe tablebase at root position
    // If TB_ROOT_IMMEDIATE=true and we get a hit, return TB move without searching
    // If TB_ROOT_IMMEDIATE=false, we store TB info and use it to guide the search
    if (SearchConfig.USE_TB_PROBE_ROOT
        && syzygy_tb
        && syzygy_tb->isAvailable()) {

      if (probeTablebaseAtRoot(p, searchResult)) {

        if (SearchConfig.TB_ROOT_IMMEDIATE) {
          // Return TB move immediately - skip search entirely
          return searchResult;
        }
        // TB_ROOT_IMMEDIATE=false: Store TB info for use during search
        // - Root moves will be filtered to only those maintaining TB result
        // - TB move will be prioritized in root move ordering
        // - Search produces proper PV while guaranteeing optimal play
        // tbRoot fields already set in probeTablebaseAtRoot
        LOG__INFO(Logger::get().SEARCH_LOG, "TB hit at root (non-immediate): move={} value={}, continuing search for PV",
                  tbRoot.move.str(), tbRoot.value.str());

        // Filter root moves to only those that maintain the TB result
        // This ensures we don't play a move that worsens our position
        filterRootMovesByTB(p);

        // Reset searchResult - we'll populate it from search
        searchResult = SearchResult{p};
      }
    }
  }

  // add some extra time for the move after the last book move (main thread only)
  // hadBookMove move will be true at his point if we ever had a book move.
  if (isMainThread() && hadBookMove && searchLimits.timeControl && searchLimits.moveTime.count() == 0) {
    LOG__DEBUG(Logger::get().SEARCH_LOG, "First non-book move to search. Adding extra time: Before: {}, after: {}",
               str(timeLimit + milliseconds(extraTimeMs.load())), str(2 * timeLimit + milliseconds(extraTimeMs.load())));
    addExtraTime(2.0);
    hadBookMove = false;
  }

  // Derive a root complexity factor for iteration gating using already generated rootMoves
  // This factor is used to scale the remaining time when checking if we have enough
  // time left to likely complete the next iteration.
  // Factors >1.0 indicate higher complexity and increase time budget; <1.0 decreases it.
  // This also checks for a single legal move and reduces time in this case.
  // Only main thread needs this - it's used for time management decisions.
  double rootComplexityFactor = 1.0;
  if (isMainThread()) {
    rootComplexityFactor = computeComplexityFactorFromMoves(p, thread().rootMoves);
    LOG__INFO(Logger::get().SEARCH_LOG, "Root complexity factor: {:.2f} (moves {}, inCheck {}, captures share ~)",
              rootComplexityFactor, thread().rootMoves.size(), p.hasCheck());

    // Debug: planned time budget before starting Iterative Deepening (no in-search extensions)
    LOG__DEBUG(Logger::get().SEARCH_LOG,
               "Planned time budget for this move (no in-search extensions): {}",
               str(timeLimit));
  }


  // Volatility tracking within this search
  Value prevBestRootValue       = VALUE_NONE; // best root eval from previous iteration
  bool addedVolatilityExtraTime = false;      // guard to add extra time due to eval swing at most once

  // Reset best-move instability tracking for this search (main thread only - shared state)
  if (isMainThread()) {
    bestMoveStability.reset();
  }

  // prepare max depth from search limits
  int maxDepth = searchLimits.depth ? searchLimits.depth : DEPTH_MAX;

  // Apply handicap depth cap (main thread only — helpers share the same stop flag)
  if (isMainThread() && SearchConfig.HANDICAP > 0) {
    const auto hParams = handicap::getHandicapParams(SearchConfig.HANDICAP);
    maxDepth           = std::min(maxDepth, hParams.depthCap);
    LOG__INFO(Logger::get().SEARCH_LOG, "Handicap {}: depth capped to {}", SearchConfig.HANDICAP, maxDepth);

    // Handicap time waste: sleep to consume a fraction of the time budget.
    // The timer is already running, so the sleep eats into search time while
    // consuming real clock time (prevents time banking that might help the engine).
    if (hParams.timeFraction < 100 && searchLimits.timeControl) {
      const auto wasteMs = timeLimit.count() * (100 - hParams.timeFraction) / 100;
      if (wasteMs > 0) {
        LOG__INFO(Logger::get().SEARCH_LOG, "Handicap {}: wasting {}ms of {}ms budget ({}% effective)",
                  SearchConfig.HANDICAP, wasteMs, timeLimit.count(), hParams.timeFraction);
        std::this_thread::sleep_for(milliseconds{wasteMs});
      }
    }
  }

  // If we have a TB root move (from non-immediate probe), give it a high sort value
  // so it's searched first. This ensures the TB move is the PV if it's truly best.
  // Both main thread and helpers benefit from this - tbRoot.move is set by main thread
  // before helpers are launched, so they can safely read it.
  if (tbRoot.move != MOVE_NONE) {
    for (Move& move : thread().rootMoves) {
      if (move == tbRoot.move) {
        // Give TB move a very high sort value to ensure it's searched first
        move.setValue(tbRoot.value);
        LOG__DEBUG(Logger::get().SEARCH_LOG, "Thread {}: TB move {} prioritized with value {}",
                   thread().id, move.str(), tbRoot.value.str());
        break;
      }
    }
    // Sort so TB move is first
    std::ranges::stable_sort(thread().rootMoves, moveValueGreaterComparator());
  }

  // Max window search in preparation for aspiration window
  // is not needed yet
  constexpr Value alpha = VALUE_MIN;
  constexpr Value beta  = VALUE_MAX;
  Value bestValue       = VALUE_NONE;

  // ===========================================================================
  // Iterative Deepening
  // ===========================================================================
  // Initialize NPS tracking right before starting search to exclude initialization overhead
  // (main thread only - these are shared state used for UCI reporting)
  if (isMainThread()) {
    npsTime  = now();
    npsNodes = thread().nodesVisited; // expected to be 0, but use actual value for robustness
  }
  milliseconds lastIterationMs{0};
  uint64_t lastIterationNodes = 0;
  uint64_t prevIterationNodes = 0;

  // Thread depth diversification for Lazy SMP.
  // Each helper thread searches a different subset of iteration depths to reduce redundant
  // work and produce more diverse TT entries at varied depth levels.
  //
  // USE_SMP_DEPTH_SKIP = true (default):
  //   Skip-table approach — each thread skips certain iteration depths based on its thread ID.
  //   Threads with higher IDs skip more aggressively (size 2-4) with interleaved phases,
  //   so at any given moment threads are spread across different depth levels.
  //   Main thread always searches every depth.
  //
  // USE_SMP_DEPTH_SKIP = false (legacy):
  //   Simple starting depth offset — helpers start at depth 1 + (id % 3), then search
  //   every depth from there. Threads converge to the same depth after a few iterations.

  // Skip tables: SkipSize controls how many depths to skip (1 = none),
  // SkipPhase offsets within the pattern so threads with the same size are interleaved.
  // Indexed by (threadId % TABLE_SIZE). 20 entries cover up to 20 helper threads;
  // higher IDs wrap around and still get good diversity.
  static constexpr int SKIP_TABLE_SIZE = 20;
  static constexpr std::array SkipSize  = {1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
  static constexpr std::array SkipPhase = {0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 7};

  const bool useDepthSkip = SearchConfig.USE_SMP_DEPTH_SKIP && !isMainThread();
  const int skipSize  = useDepthSkip ? SkipSize[thread().id % SKIP_TABLE_SIZE]  : 1;
  const int skipPhase = useDepthSkip ? SkipPhase[thread().id % SKIP_TABLE_SIZE] : 0;

  // Legacy fallback: simple starting depth offset when skip tables are disabled
  const Depth startingDepth = !SearchConfig.USE_SMP_DEPTH_SKIP && !isMainThread()
                                ? Depth{1 + thread().id % 3}
                                : Depth{1};

  for (auto iterationDepth = startingDepth; iterationDepth <= maxDepth; ++iterationDepth) {

    // Skip-table depth diversification: helpers skip certain iteration depths
    // so threads are spread across different depth levels at any given moment.
    if (useDepthSkip && (static_cast<int>(iterationDepth) + skipPhase) % skipSize != 0) {
      continue;
    }

    // Before starting a new iteration, check if we have enough time left to likely complete it.
    // (main thread only - helpers just check stopSearchFlag)
    if (isMainThread()
        && searchLimits.timeControl
        && !searchLimits.ponder
        && iterationDepth > 1) {
      const nanoseconds sinceNs = elapsedSince(startSearchTime);
      const auto elapsed        = MILLISECONDS(sinceNs);
      const milliseconds budget = timeLimit + milliseconds(extraTimeMs.load());
      if (elapsed >= budget) {
        LOG__DEBUG(Logger::get().SEARCH_LOG, "Stop before iteration {}: time budget exhausted (elapsed {} >= budget {})",
                   iterationDepth, str(elapsed), str(budget));
        break;
      }
      const milliseconds remaining = budget - elapsed;

      // Estimate the necessary time for the next iteration based on last iteration nodes,
      // observed node growth, and current NPS; keep a small safety buffer.
      constexpr milliseconds buffer{5};

      // Determine current NPS: prefer recent window (since last UCI update), fallback to average.
      const uint64_t nowTime      = now();
      uint64_t currentNps         = 0;
      const uint64_t currentNodes = thread().nodesVisited;
      if (nowTime > npsTime) { currentNps = nps(currentNodes - npsNodes, nowTime - npsTime); }
      if (currentNps == 0) { currentNps = nps(currentNodes, sinceNs); }
      if (currentNps == 0) { currentNps = 1; }

      // Predict the node count of the next iteration using observed growth.
      double growth = prevIterationNodes > 0
                        ? static_cast<double>(lastIterationNodes) / static_cast<double>(prevIterationNodes)
                        : 1.7; // default growth when we only have one observation
      if (growth < 1.2) growth = 1.2;
      if (growth > 3.0) growth = 3.0;
      const uint64_t predictedNodesNext = lastIterationNodes > 0
                                            ? static_cast<uint64_t>(std::llround(
                                                static_cast<long double>(lastIterationNodes)
                                                * static_cast<long double>(growth)))
                                            : 0ULL;

      milliseconds needed{0};
      if (predictedNodesNext > 0) {
        const uint64_t neededMsU64 = (predictedNodesNext * 1000ULL) / currentNps;
        needed                     = milliseconds{neededMsU64};
      }
      else if (lastIterationMs.count() > 0) {
        // fallback: use wall-time of last iteration
        needed = lastIterationMs;
      }

      // Complexity-aware gating: scale remaining by root complexity factor.
      // effectiveRemaining = remaining * rootComplexityFactor
      // This reduces effective time for simple positions (e.g., single legal move -> factor 0.1)
      const auto effectiveRemaining = milliseconds{
        std::llround(static_cast<long double>(remaining.count())
                     * static_cast<long double>(rootComplexityFactor))};

      if (effectiveRemaining <= buffer || (needed.count() > 0 && effectiveRemaining < needed)) {
        LOG__DEBUG(Logger::get().SEARCH_LOG,
                   "Stop before iteration {}: budget {}, remaining {} * complexity {:.2f} = effRemaining {} < needed {} (buffer {})",
                   iterationDepth, str(budget), str(remaining), rootComplexityFactor, str(effectiveRemaining), str(needed), str(buffer));
        break;
      }
    }

    // update search counter for the initial position of this iteration
    thread().nodesVisited++;

    // update depth statistics
    ESSENTIAL_STAT_SET(thread().statistics.currentIterationDepth, iterationDepth);
    ESSENTIAL_STAT_SET(thread().statistics.currentSearchDepth, thread().statistics.currentIterationDepth);
    if (thread().statistics.currentExtraSearchDepth < thread().statistics.currentIterationDepth) {
      ESSENTIAL_STAT_SET(thread().statistics.currentExtraSearchDepth, thread().statistics.currentIterationDepth);
    }

    // reset perft counter for last depth to
    ESSENTIAL_STAT_SET(thread().statistics.perftNodeCount, 0);

    // Measure iteration duration
    const TimePoint iterationStartTime = currentTime();
    const uint64_t iterStartNodes      = thread().nodesVisited;

    // =========================================================================
    // Start actual alpha beta search
    // MultiPV loop: find top N moves per iteration
    // When MULTI_PV == 1 (default), the loop runs once — zero overhead.
    // Helper threads always use effectiveMultiPV == 1 for efficiency.
    // When Handicap > 0, inflate to handicap multiPV to build a candidate pool
    // for suboptimal move selection (extra PVs not reported to UCI).
    // Note: multiPV may be < poolSize for gentle levels (1-2), where pool
    // candidates use approximate previous-iteration scores with tight thresholds.
    // =========================================================================
    int effectiveMultiPV = 1;
    if (!isMainThread()) {
      effectiveMultiPV = 1;
    }
    else {
      const int handicapMPV = SearchConfig.HANDICAP > 0
                                ? handicap::getHandicapParams(SearchConfig.HANDICAP).multiPV
                                : 1;
      effectiveMultiPV      = std::min(
        std::max(SearchConfig.MULTI_PV, handicapMPV),
        static_cast<int>(thread().rootMoves.size()));
    }

    // Number of PV lines to report to UCI (user-facing MultiPV, not inflated by handicap)
    const int reportedMultiPV = std::min(SearchConfig.MULTI_PV, effectiveMultiPV);

    // Collect PV data during the loop for deferred, sorted reporting (Stockfish-style).
    // After all PVs are searched, results are sorted by score (descending) so that
    // the output is always monotonically non-increasing, and rootMoves[0..N] are
    // re-ordered to match. This also produces batched UCI output with consistent
    // node counts across all PV lines.
    std::vector<MultiPvResult> multiPvResults;
    multiPvResults.reserve(effectiveMultiPV);

    for (int pvIdx = 0; pvIdx < effectiveMultiPV; ++pvIdx) {
      if (pvIdx == 0) {
        // First PV: use aspiration search as normal (or full window at shallow depths)
        // ASPIRATION SEARCH
        if (SearchConfig.USE_ASP && iterationDepth > 3) {
          bestValue = aspirationSearch(p, iterationDepth, bestValue);
        }
        // PVS SEARCH (or pure ALPHA BETA when PVS deactivated)
        else {
          bestValue = rootSearch(p, iterationDepth, alpha, beta);
        }
      }
      else {
        // Secondary PVs: full window search starting from pvIdx.
        // Moves [0..pvIdx-1] are already ranked and locked.
        bestValue = rootSearch(p, iterationDepth, VALUE_MIN, VALUE_MAX, pvIdx);
      }

      // Break from MultiPV loop if search was stopped
      if (stopConditions() && iterationDepth > 1) break;

      // Partial sort: bring the best remaining move to position pvIdx.
      // For pvIdx=0 this sorts the full list; for pvIdx>0 only [pvIdx..end].
      std::ranges::stable_sort(
        thread().rootMoves.begin() + pvIdx,
        thread().rootMoves.end(),
        moveValueGreaterComparator());

      // Collect PV data for deferred reporting.
      // Extract PV line now (before next pvIdx search overwrites the PV table).
      // Use thread-local position 'p' (not Search::position) to avoid data race
      // with the main thread which does doMove/undoMove on Search::position.
      Position tmpPos = p;
      multiPvResults.push_back({.pvLine   = extractPvWithTT(tmpPos),
                                .score    = thread().rootMoves[pvIdx].value(),
                                .seldepth = thread().statistics.currentExtraSearchDepth});
    }

    // Sort all completed PVs by score (descending) to guarantee monotonic output.
    // This handles search instability where a secondary PV might score higher than
    // pvIdx=0 due to richer TT state or different window dynamics.
    if (effectiveMultiPV > 1 && static_cast<int>(multiPvResults.size()) == effectiveMultiPV) {
      std::ranges::stable_sort(multiPvResults, [](const MultiPvResult& a, const MultiPvResult& b) {
        return a.score > b.score;
      });

      // Re-sort rootMoves[0..effectiveMultiPV] to match the sorted result order.
      // This ensures rootMoves[0] is the actual best move for post-iteration code
      // (stability tracking, mate detection, aspiration window centering, etc.).
      std::ranges::stable_sort(
        thread().rootMoves.begin(),
        thread().rootMoves.begin() + effectiveMultiPV,
        moveValueGreaterComparator());
    }

    // Send PV lines to UCI in batch (main thread only, not on stop).
    // Only send reportedMultiPV lines (user-facing), not handicap-inflated extras.
    // All lines share the same node count snapshot — consistent like Stockfish.
    if (isMainThread() && !multiPvResults.empty() && !stopConditions()) {
      if (reportedMultiPV < static_cast<int>(multiPvResults.size())) {
        // Trim to user-facing MultiPV count (handicap extras are internal only)
        const std::vector reportedResults(
          multiPvResults.begin(), multiPvResults.begin() + reportedMultiPV);
        sendMultiPvResultsToUci(reportedResults, iterationDepth);
      }
      else {
        sendMultiPvResultsToUci(multiPvResults, iterationDepth);
      }
    }

    // Restore the best PV to the PV table so post-iteration code
    // (assertions, volatility, stability, mate check, aspiration window)
    // sees the top move and its value.
    if (effectiveMultiPV > 1 && !multiPvResults.empty()) {
      bestValue              = multiPvResults[0].score;
      const auto& bestPvLine = multiPvResults[0].pvLine;
      for (int i = 0; i < static_cast<int>(bestPvLine.size()) && i < PVTable::MAX_PLY; ++i) {
        thread().pv(DEPTH_NONE, i) = bestPvLine[i];
      }
      if (static_cast<int>(bestPvLine.size()) < PVTable::MAX_PLY) {
        thread().pv(DEPTH_NONE, static_cast<int>(bestPvLine.size())) = MOVE_NONE;
      }
    }
    // =========================================================================
    // /End of alpha beta search for this iteration
    // =========================================================================

    // record iteration duration for next pre-check
    lastIterationMs = MILLISECONDS(currentTime() - iterationStartTime);

    // record node counts for growth prediction
    prevIterationNodes = lastIterationNodes;
    lastIterationNodes = thread().nodesVisited - iterStartNodes;

    // Launch helper threads after TT priming (delayed startup for better TT utilization)
    // Helpers start after main thread has completed SMP_HELPER_START_DEPTH iterations,
    // allowing them to benefit from TT entries written by the main thread.
    // (main thread only)
    if (isMainThread()
        && !helpersLaunched
        && iterationDepth >= SearchConfig.SMP_HELPER_START_DEPTH) {
      launchHelperThreads();
    }

    // These assertions only apply to main thread:
    // - Main thread always starts at depth 1 and completes at least one full iteration
    // - Helper threads may start at different depths or skip iterations, and can be stopped before populating PV
    assert(!isMainThread() || (!thread().pv.empty() && thread().pv.first() != MOVE_NONE && "pv must contain a valid first move"));
    assert(!isMainThread() || (bestValue == thread().pv.first().value() || stopSearchFlag) && "bestValue should be equal value of thread().pv.first()");

    // Conservative volatility detector
    // Add extra time when big evaluation swings happen between consecutive iterations
    // (main thread only - helpers don't manage time)
    if (isMainThread()
        && SearchConfig.USE_EVAL_VOLATILITY
        && searchLimits.timeControl
        && !addedVolatilityExtraTime
        && !isTimeAlmostUp()) {
      const Value currBest = thread().pv.first().value();
      // Only consider reasonably deep iterations to avoid noise from shallow depths
      const int volSwingMinDepth = SearchConfig.VOLATILITY_MIN_DEPTH;
      const auto volSwingThresh  = Value{SearchConfig.VOLATILITY_THRESHOLD};
      if (iterationDepth >= volSwingMinDepth
          && currBest.isValid()
          && prevBestRootValue.isValid()) {
        Value delta = currBest - prevBestRootValue;
        if (delta < VALUE_ZERO) delta = -delta;
        if (delta >= volSwingThresh) {
          const double factor = SearchConfig.VOLATILITY_FACTOR;
          addExtraTime(factor);
          addedVolatilityExtraTime = true;
          LOG__DEBUG(Logger::get().SEARCH_LOG, "Volatility: large eval swing at depth {} (Δ{} >= {}). Adding extra time (factor {:.2f}).",
                     iterationDepth, delta.str(), volSwingThresh.str(), factor);
        }
      }
      // remember current value for next iteration comparison
      if (currBest.isValid()) { prevBestRootValue = currBest; }
    }

    // Best-move instability tracking for dynamic time management
    // Tracks whether the best move is stable (same across iterations) or unstable (changing).
    // (main thread only - helpers don't manage time)
    if (isMainThread()
        && SearchConfig.USE_BESTMOVE_INSTABILITY
        && searchLimits.timeControl
        && !searchLimits.ponder
        && !isTimeAlmostUp()) {
      const Move currentBestMove = thread().pv.first().stripped();
      const int minDepth         = SearchConfig.INSTABILITY_MIN_DEPTH;

      if (iterationDepth >= minDepth) {
        // Update stability/change counters
        if (bestMoveStability.lastBestMove != MOVE_NONE) {
          if (currentBestMove == bestMoveStability.lastBestMove) {
            bestMoveStability.stabilityCount++;
          }
          else {
            bestMoveStability.changeCount++;
            bestMoveStability.stabilityCount = 0;
            LOG__DEBUG(Logger::get().SEARCH_LOG, "BestMove instability: move changed from {} to {} at depth {} (changeCount={})",
                       bestMoveStability.lastBestMove.str(), currentBestMove.str(), iterationDepth, bestMoveStability.changeCount);
          }
        }
        bestMoveStability.lastBestMove = currentBestMove;

        // Apply time adjustment based on stability
        // Stable: reduce time (move sooner, we're confident)
        if (!bestMoveStability.appliedStableFactor
            && bestMoveStability.stabilityCount >= SearchConfig.INSTABILITY_STABLE_COUNT) {
          const double factor = SearchConfig.INSTABILITY_STABLE_FACTOR;
          addExtraTime(factor);
          bestMoveStability.appliedStableFactor = true;
          LOG__DEBUG(Logger::get().SEARCH_LOG, "BestMove stability: {} stable for {} iterations at depth {}. Reducing time (factor {:.2f}).",
                     currentBestMove.str(), bestMoveStability.stabilityCount, iterationDepth, factor);
        }
        // Unstable: extend time (need more depth to find the true best move)
        else if (!bestMoveStability.appliedExtendFactor
                 && bestMoveStability.changeCount >= SearchConfig.INSTABILITY_CHANGE_THRESHOLD) {
          const double factor = SearchConfig.INSTABILITY_EXTEND_FACTOR;
          addExtraTime(factor);
          bestMoveStability.appliedExtendFactor = true;
          LOG__DEBUG(Logger::get().SEARCH_LOG, "BestMove instability: {} changes detected at depth {}. Extending time (factor {:.2f}).",
                     bestMoveStability.changeCount, iterationDepth, factor);
        }
      }
    }

    // if mate search check if we found a mate within the mate limit
    if (searchLimits.mate
        && abs(thread().pv.first().value()) >= VALUE_CHECKMATE_THRESHOLD
        && searchLimits.mate * 2 - 1 == VALUE_CHECKMATE - thread().pv.first().value()) {
      // Record completed iteration, so selectBestThread() sees this iteration's
      // data instead of stale values from the previous depth.
      thread().completedIterationDepth = iterationDepth;
      thread().lastIterationValue      = thread().pv.first().value();
      // mateFound is derived in run() after best-thread selection
      break;
    }

    // Check if we need to stop.
    // Doing this after the first iteration ensures that
    // we have done at least one complete search and have
    // a pv (best) move.
    // Note: Single legal move positions are handled via rootComplexityFactor (0.1),
    // which reduces effective time budget and causes early exit in timed games.
    if (!stopConditions()) {
      // sort root moves for the next iteration
      std::ranges::stable_sort(thread().rootMoves, moveValueGreaterComparator());
      // update stats (main thread only)
      // UCI info was already sent per-PV-line inside the MultiPV loop above.
      if (isMainThread()) {
        ESSENTIAL_STAT_SET(thread().statistics.currentBestRootMove, thread().pv.first());
        ESSENTIAL_STAT_SET(thread().statistics.currentBestRootMoveValue, thread().pv.first().value());
        // Send debug eval breakdown and iteration stats when debug mode is on
        if (isDebugMode()) {
          sendDebugEvalInfo();
        }
      }
      // Track completed iteration for best-thread selection (all threads)
      thread().completedIterationDepth = iterationDepth;
      thread().lastIterationValue      = thread().pv.first().value();
    }
    else {
      break;
    }
  }
  // ===========================================================================
  // /END: Iterative Deepening
  // ===========================================================================

  // update searchResult
  // the best move is pv(0,0) - we need to make sure this entry exists at this time
  // the best value is pv(0,0).value()
  searchResult.bestMove      = thread().pv.first().stripped();
  searchResult.bestMoveValue = thread().pv.first().value();
  searchResult.depth         = thread().statistics.currentIterationDepth;
  searchResult.extraDepth    = thread().statistics.currentExtraSearchDepth;
  searchResult.bookMove      = false;

  // Note: TB root override and ponder move logic are handled in run()
  // after best-thread selection, so they apply to the selected best thread's result.

  return searchResult;
}

Value Search::aspirationSearch(Position& p, const Depth depth, const Value bestValue) {
  // Mate bypass — skip aspiration entirely when previous iteration found a mate.
  // Aspiration around mate scores (e.g., ±12 around 9987cp) almost always fails,
  // wasting search effort and potentially losing confirmed mates through cascading
  // fail-lows. A full-window search guarantees the mate is confirmed or improved.
  if (bestValue.isCheckMate()) {
    return rootSearch(p, depth, VALUE_MIN, VALUE_MAX);
  }

  // Initialize aspiration window centered on previous iteration's value.
  // Uses exponential widening: delta grows by delta/divisor on each fail,
  // guaranteeing the window reaches [VALUE_MIN, VALUE_MAX] after ~8-10 fails.
  auto delta  = Value{SearchConfig.ASP_INITIAL_DELTA};
  Value alpha = std::max(bestValue - delta, VALUE_MIN);
  Value beta  = std::min(bestValue + delta, VALUE_MAX);
  Value value = VALUE_NONE;

  // Extreme score threshold for immediate full-window fallback.
  // TB win/loss scores (~9000 cp) would need ~20 incremental widenings from
  // a normal eval window. Detecting them early and opening to full window
  // saves ~19 redundant re-searches with negligible node cost difference.
  // 4000 cp is safely above any realistic evaluation but well below TB scores.
  static constexpr auto EXTREME_SCORE_THRESHOLD = Value{4000};

  while (true) {
    value = rootSearch(p, depth, alpha, beta);

    // If search has been stopped and the value missed the window, return
    // VALUE_NONE — root move values are invalid
    if (stopConditions() && (value <= alpha || value >= beta)) { return VALUE_NONE; }

    // If time is almost up, avoid further aspiration expansions and return current value
    if (isTimeAlmostUp()) { return value; }

    // Check if the value was within the window or expand the window
    if (value <= alpha) {
      // FAIL LOW — widen alpha (lower bound), keep beta unchanged.
      // Re-center on the actual search result (value-centered), not the stale
      // bestValue from the previous iteration.
      if (isMainThread()) {
        // Update displayed value to reflect current search result (not stale previous iteration)
        ESSENTIAL_STAT_SET(thread().statistics.currentBestRootMoveValue, thread().pv.first().value());
        sendAspirationResearchInfo("upperbound");
        // Add extra time because of fail low — we might have found a strong opponent's move
        addExtraTime(1.3);
      }
      // If time is almost up, don't expand; return current value
      if (isTimeAlmostUp()) { return value; }

      // Extreme score: TB loss or similar — skip incremental widening,
      // open to full window immediately for the next search.
      if (value <= -EXTREME_SCORE_THRESHOLD) {
        alpha = VALUE_MIN;
        beta  = VALUE_MAX;
      }
      else {
        alpha = std::max(value - delta, VALUE_MIN);
        delta = Value{static_cast<int>(delta) + static_cast<int>(delta) / SearchConfig.ASP_DELTA_GROWTH_DIVISOR};
      }
      STAT_INC(thread().statistics.aspirationResearches);
    }
    else if (value >= beta) {
      // FAIL HIGH — widen beta (upper bound), keep alpha unchanged.
      // Re-center on the actual search result (value-centered).
      if (isMainThread()) {
        ESSENTIAL_STAT_SET(thread().statistics.currentBestRootMoveValue, thread().pv.first().value());
        sendAspirationResearchInfo("lowerbound");
      }
      // If time is almost up, don't expand; return current value
      if (isTimeAlmostUp()) { return value; }

      // Extreme score: TB win or similar — skip incremental widening,
      // open to full window immediately for the next search.
      if (value >= EXTREME_SCORE_THRESHOLD) {
        alpha = VALUE_MIN;
        beta  = VALUE_MAX;
      }
      else {
        beta  = std::min(value + delta, VALUE_MAX);
        delta = Value{static_cast<int>(delta) + static_cast<int>(delta) / SearchConfig.ASP_DELTA_GROWTH_DIVISOR};
      }
      STAT_INC(thread().statistics.aspirationResearches);
    }
    else {
      // Value is within the window — aspiration succeeded
      break;
    }
  }
  return value;
}

Value Search::rootSearch(Position& p, const Depth depth, Value alpha, const Value beta, const int startIndex) {

  // In root search we search all moves and store the value
  // into the root moves themselves for sorting in the
  // next iteration
  // best move is stored in pv[0][0]
  // best value is stored in pv[0][0].value
  // The next iteration begins with the best move of the last
  // iteration, so we can be sure pv[0][0] will be set with the
  // last best move from the previous iteration independent of
  // the value. Any better move found is really better and will
  // replace pv[0][0] and also will be sorted first in the
  // next iteration

  // prepare root node search
  Value bestNodeValue = VALUE_NONE;
  Value value;

  // ///////////////////////////////////////////////////////
  // MOVE LOOP
  const size_t size = thread().rootMoves.size();
  for (size_t i = startIndex; i < size; i++) {
    Move& moveRef = thread().rootMoves.at(i);

    p.doMove(moveRef);
    thread().nodesVisited++;
    thread().statistics.currentVariation.push_back(moveRef);
    ESSENTIAL_STAT_SET(thread().statistics.currentRootMoveIndex, i);
    ESSENTIAL_STAT_SET(thread().statistics.currentRootMove, moveRef);

    if (checkDrawRepAnd50(p, 2)) {
      value = drawScore(p);
    }
    else {
      constexpr Depth ply{1};
      // ///////////////////////////////////////////////////////////////////
      // PVS
      // First move in a node is an assumed PV Move and searched with full search window (PV Node)
      // Root's first child is a PV node (full window search)
      // For MultiPV: the first move in the current sub-range (i == startIndex) is the PV candidate
      if (!SearchConfig.USE_PVS || i == static_cast<size_t>(startIndex)) {
        value = -search(p, depth - 1, ply, -beta, -alpha, PvNode, Do_Null_Move);
      }
      else {
        // Null window search after the initial PV search.
        // After first move, children are CUT nodes (expected to fail high)
        value = -search(p, depth - 1, ply, -alpha - 1, -alpha, CutNode, Do_Null_Move);
        // If this move improved alpha without exceeding beta we do a proper full window
        // search to get an accurate score.
        if (value > alpha && value < beta && !stopConditions() && !isTimeAlmostUp()) {
          STAT_INC(thread().statistics.rootPvsResearches);
          value = -search(p, depth - 1, ply, -beta, -alpha, PvNode, Do_Null_Move);
        }
      }
      // ///////////////////////////////////////////////////////////////////
    }

    thread().statistics.currentVariation.pop_back();
    p.undoMove();

    // set the value into the root move to later be able to sort
    // root moves according to value
    moveRef.setValue(value);

    // we want to do at least one complete search with depth 1
    // After that we can stop any time - any new best moves will
    // have been stored in pv
    if (stopConditions() && depth > 1) { return VALUE_NONE; }

    // Did we find a new best move?
    // For the first move with a full window (alpha=-inf)
    // this is always the case.
    if (value > bestNodeValue) {
      bestNodeValue = value;
      // we have a new best move - update triangular PV table
      thread().pv.update(moveRef, DEPTH_NONE);
      STAT_INC(thread().statistics.bestMoveChange);
      if (value > alpha) {
        // fail high in root only when using aspiration search
        if (value >= beta && SearchConfig.USE_ALPHABETA) {
          STAT_INC(thread().statistics.betaCuts);
          STAT_INC(thread().statistics.betaCutsByIndex[std::min(static_cast<int>(thread().statistics.currentRootMoveIndex), SearchStats::BETA_CUTS_INDEX_SIZE - 1)]);
          return value;
        }
        // value is < beta
        // always the case when not using aspiration search
        alpha = bestNodeValue;
      }
    }
  }
  // MOVE LOOP
  // ///////////////////////////////////////////////////////

  // only needed for aspiration and MTDf
  return bestNodeValue;
}

Value Search::search(Position& p, const Depth depth, const Depth ply, Value alpha, Value beta, const NodeType nodeType, const Do_Null doNull) {
  //  LOG__DEBUG(Logger::get().SEARCH_LOG, "Search {} {} {}", depth, ply, str(thread().statistics.currentVariation));

  // Clear PV for this node to prevent stale data from previous iterations/branches
  // from being propagated up via thread().pv.update(). Stale PV data can contain moves from
  // different positions that are illegal in the current position.
  thread().pv.clear(ply);

  // Enter quiescence search when depth == 0 or max ply has been reached
  // pvNodes/nonPvNodes are tracked inside qsearch() — no need to count here.
  if (depth == 0 || ply >= MAX_DEPTH) {
    const auto value = qsearch(p, ply, alpha, beta, nodeType);
    return value;
  }

  // Track PV vs non-PV node statistics (after qsearch drop-through to avoid double-counting
  // — qsearch() already tracks its own pvNodes/nonPvNodes at entry)
#ifndef FRANKYCPP_PRODUCTION
  if (nodeType == PvNode) { ++thread().statistics.pvNodes; }
  else { ++thread().statistics.nonPvNodes; }
  ++thread().statistics.searchNodes;
#endif

  // check if search should be stopped
  if (stopConditions() && depth > 1) { return VALUE_NONE; }

  // Mate Distance Pruning
  // Did we already find a shorter mate then ignore
  // this one.
  if (SearchConfig.USE_MDP) {
    alpha = std::max(alpha, -VALUE_CHECKMATE + static_cast<Value>(ply));
    beta  = std::min(beta, VALUE_CHECKMATE - static_cast<Value>(ply));
    if (alpha >= beta) {
      STAT_INC(thread().statistics.mdp);
      return alpha;
    }
  }

  // prepare node search
  const Color us      = p.getNextPlayer();
  Value bestNodeValue = VALUE_NONE;
  Move bestNodeMove   = MOVE_NONE; // used to store in the TT
  Move ttMove         = MOVE_NONE;
  ValueType ttType    = ALPHA;
  Value staticEval    = VALUE_NONE;
  bool matethreat     = false;

  // Variables for singular extension
  Value ttValue = VALUE_NONE;
  Depth ttDepth = DEPTH_NONE;

  // TT Lookup (before TB probe to avoid redundant TB probes for cached results)
  // Results of searches are stored in the TT to be used to
  // avoid searching positions several times. If a position
  // is stored in the TT, we retrieve a pointer to the entry.
  // We use the stored move as a best move from previous searches
  // and search it first (through setting PV move in move gen).
  // If we have a value from a similar or deeper search, we check
  // if the value is usable. Exact values mean that the previously
  // stored result already was a precise result, and we do not
  // need to search the position again. We can stop searching
  // this branch and return the value.
  // Alpha or Beta entries will only be used if they improve
  // the current values.
  if (SearchConfig.USE_TT) {
    STAT_INC(thread().statistics.ttProbes);
    if (const auto ttEntry = tt->probe(p.getZobristKey(), thread().id)) {
      // tt hit
      const auto probedMove = static_cast<Move>(ttEntry->move);
      // Validate TT move before use - corrupted/torn reads from lockless TT
      // could produce garbage moves that would corrupt position state in doMove()
      if (probedMove != MOVE_NONE && MoveGenerator::isPseudoLegal(p, probedMove)) {
        ttMove = probedMove;
      }
      ttValue = valueFromTt(ttEntry->value, ply);
      ttDepth = static_cast<Depth>(ttEntry->depth);

      // Track hit quality by depth (sufficient depth for cutoff vs move-only)
      if (ttDepth >= depth) { STAT_INC(thread().statistics.ttHitSufficientDepth); }
      else { STAT_INC(thread().statistics.ttHitInsufficientDepth); }

      // Track hit quality by bound type
      switch (ttEntry->type) {
        case NONE:
          STAT_INC(thread().statistics.ttHitNone);
          break;
        case EXACT:
          STAT_INC(thread().statistics.ttHitExact);
          break;
        case ALPHA:
          STAT_INC(thread().statistics.ttHitAlpha);
          break;
        case BETA:
          STAT_INC(thread().statistics.ttHitBeta);
          break;
      }

      // Never cutoff on PV nodes - this ensures we always build a complete PV line
      // Non-PV nodes can still use TT cutoffs as they don't contribute to the reported PV
      if (nodeType != PvNode && ttDepth >= depth) {
        if (SearchConfig.USE_TT_VALUE
            && ttValue.isValid()
            && (ttEntry->type == EXACT
                || (ttEntry->type == ALPHA && ttValue <= alpha)
                || (ttEntry->type == BETA && ttValue >= beta))) {
          STAT_INC(thread().statistics.TtCuts);
          STAT_INC(thread().statistics.ttCutsSearch);         // main search cut
          STAT_ADD(thread().statistics.ttCutDepthSum, depth); // track depth for avg calculation
          return ttValue;
        }
        STAT_INC(thread().statistics.TtNoCuts);
      }
      // if we have a static eval stored, we can reuse it
      if (SearchConfig.USE_EVAL_TT && ttEntry->eval != VALUE_NONE) {
        STAT_INC(thread().statistics.evalFromTT);
        staticEval = ttEntry->eval;
      }
    }
    else {
      STAT_INC(thread().statistics.ttMisses);
    }
  } // use TT

  // Tablebase probing in search (after TT lookup to use cached TB results)
  // Probe WDL for positions within TB piece limit. This can provide
  // early cutoffs for winning/losing positions or exact draws.
  // Only probe at sufficient depth to avoid overhead in shallow searches.
  // On PV nodes: only use TB to tighten bounds - don't cut off (need complete PV line).
  // If USE_TB_PROBE_PV is false, skip probing on PV nodes entirely (performance optimization).
  // Guard order optimized for fast rejection: cheap checks first, expensive popcount() last.
  if (SearchConfig.USE_TB_PROBE_SEARCH
      && p.getCastlingRights() == NO_CASTLING // CHEAP: Most positions have castling rights
      && depth >= SearchConfig.TB_PROBE_DEPTH // CHEAP: Fails at shallow depths
      && syzygy_tb                            // CHEAP: Null pointer check
      && syzygy_tb->isAvailable()             // CHEAP: Member access
      && (SearchConfig.USE_TB_PROBE_PV || nodeType != PvNode)
      && p.getOccupiedBb().popcount() <= SearchConfig.TB_PROBE_LIMIT) { // EXPENSIVE: Last!

    STAT_INC(thread().statistics.tbSearchProbes);
    const tablebase::TBResult wdl = syzygy_tb->probeWDL(p);

    if (wdl != tablebase::TBResult::Failed) {
      STAT_INC(thread().statistics.tbSearchHits);

      // Convert WDL to score with 50-move rule handling
      const Value tbScore = getTBScoreForSearch(wdl, p.getHalfMoveClock(), ply);

      // Use WDL as bound for alpha-beta
      // On PV nodes: only tighten bounds, never cut off (need to build PV)
      // On non-PV nodes: can cut off immediately
      if (wdl == tablebase::TBResult::Win || wdl == tablebase::TBResult::CursedWin) {
        // Position is winning - use as lower bound
        if (nodeType != PvNode && tbScore >= beta) {
          STAT_INC(thread().statistics.tbSearchCutoffs);
          // Store in TT for future lookups
          if (SearchConfig.USE_TT) {
            storeTt(p, depth, ply, MOVE_NONE, tbScore, BETA, VALUE_NONE);
          }
          return tbScore; // Fail high (beta cutoff)
        }
        // Tighten alpha if TB score is better
        alpha = std::max(alpha, tbScore);
      }
      else if (wdl == tablebase::TBResult::Loss || wdl == tablebase::TBResult::BlessedLoss) {
        // Position is losing - use as upper bound
        if (nodeType != PvNode && tbScore <= alpha) {
          STAT_INC(thread().statistics.tbSearchCutoffs);
          // Store in TT for future lookups
          if (SearchConfig.USE_TT) {
            storeTt(p, depth, ply, MOVE_NONE, tbScore, ALPHA, VALUE_NONE);
          }
          return tbScore; // Fail low (alpha cutoff)
        }
        // Tighten beta if TB score is worse
        beta = std::min(beta, tbScore);
      }
      else if (wdl == tablebase::TBResult::Draw) {
        // Exact draw - can return immediately on non-PV nodes
        if (nodeType != PvNode) {
          STAT_INC(thread().statistics.tbSearchCutoffs);
          if (SearchConfig.USE_TT) {
            storeTt(p, depth, ply, MOVE_NONE, VALUE_DRAW, EXACT, VALUE_NONE);
          }
          return VALUE_DRAW;
        }
        // On PV nodes, tighten both bounds to draw
        alpha = std::max(alpha, VALUE_DRAW);
        beta  = std::min(beta, VALUE_DRAW);
      }
    }
    else {
      STAT_INC(thread().statistics.tbSearchMisses);
    }
  }

  const bool hasCheck = p.hasCheck();

  // get an evaluation for the position
  if (!hasCheck && staticEval == VALUE_NONE) {
    staticEval = evaluate(p);
    // Storing this value might save us calls to eval on the same position.
    // Skip during singular verification to avoid unnecessary TT writes
    if (SearchConfig.USE_TT && SearchConfig.USE_EVAL_TT) {
      storeTt(p, DEPTH_NONE, DEPTH_NONE, MOVE_NONE, VALUE_NONE, NONE, staticEval);
    }
  }

  // Store static eval in PlyInfo for "improving" flag computation at deeper plies.
  // When in check we have no meaningful eval, so store VALUE_NONE.
  thread().plyStack[ply].staticEval = hasCheck ? VALUE_NONE : staticEval;

  // Compute "improving" flag: is our static eval better than 2 plies ago?
  // This is used to modulate pruning aggressiveness - when not improving,
  // we can prune more aggressively since our position isn't getting better.
  // Edge cases:
  //   - ply < 2: no prior data → assume false (conservative)
  //   - in check now: no eval → not improving
  //   - in check 2 plies ago (VALUE_NONE): fall back to ply-4, else assume improving
  const bool improving = [&]() {
    if (!SearchConfig.USE_IMPROVING) return false;
    if (hasCheck || staticEval == VALUE_NONE) return false;
    if (ply < 2) return false;
    const Value prevEval = thread().plyStack[ply - 2].staticEval;
    if (prevEval != VALUE_NONE) return staticEval > prevEval;
    // 2 plies ago was in check — try 4 plies ago
    if (ply >= 4) {
      const Value prevEval4 = thread().plyStack[ply - 4].staticEval;
      if (prevEval4 != VALUE_NONE) return staticEval > prevEval4;
    }
    return true; // Conservative: assume improving when no reference data
  }();

  // Track improving statistics
  if (SearchConfig.USE_IMPROVING && !hasCheck) {
    if (improving) { STAT_INC(thread().statistics.improvingTrue); }
    else { STAT_INC(thread().statistics.improvingFalse); }
  }

  // Reverse Futility Pruning, (RFP, Static Null Move Pruning)
  // https://www.chessprogramming.org/Reverse_Futility_Pruning
  // Anticipate a likely alpha low in the next ply by a beta cut
  // off before making and evaluating the move
  if (SearchConfig.USE_RFP
      && doNull
      && depth <= 3
      && nodeType != PvNode
      && !hasCheck
      && std::abs(beta) < VALUE_CHECKMATE_THRESHOLD) { // Don't prune when beta is a mate score
    auto margin = Value{SearchConfig.RFP_MARGIN[depth]};
    // Increase margin when not improving → prune less aggressively (Stockfish-style)
    // Rationale: "not improving" means eval may be unreliable, so search more carefully
    if (SearchConfig.USE_RFP_IMPROVING && !improving) {
      margin += Value{SearchConfig.RFP_IMPROVING_MARGIN};
    }
    if (staticEval - margin >= beta) {
      STAT_INC(thread().statistics.rfp_cuts);
      return staticEval - margin; // fail-hard: beta / fail-soft: staticEval - evalMargin;
    }
  }

  // Razoring
  // https://www.chessprogramming.org/Razoring
  // When static eval is well below alpha at the last node,
  // jump directly into qsearch.
  if (SearchConfig.USE_RAZORING
      && nodeType != PvNode // fix 19.2.2026 - only razor on non-PV nodes to avoid missing critical moves in PV line
      && depth == 1
      && staticEval != VALUE_NONE
      && staticEval <= alpha - SearchConfig.RAZOR_MARGIN) {
    STAT_INC(thread().statistics.razorings);
    // fix 19.2.2026 - use AllNode for razor to avoid missing critical moves in PV line; razor is a
    // heuristic that can afford to miss some moves, but we don't want it to miss critical moves in
    // the PV line. Use AllNode since we're expecting to fail low (that's why we're razoring).
    return qsearch(p, ply, alpha, beta, AllNode);
  }

  // NULL MOVE PRUNING
  // https://www.chessprogramming.org/Null_Move_Pruning
  // Under the assumption that in most chess position it would be better
  // do make a move than to not make a move we assume that if
  // our positional value after a null move is already above beta (>beta)
  // it would be above beta when doing a move in any case.
  // Certain situations need to be considered though:
  // - Zugzwang - it would be better not to move
  // - in check - this would lead to an illegal situation where the king is captured
  // - recursive null moves should be avoided
  if (SearchConfig.USE_NMP) {
    // Disable near mate bounds and in zugzwang-prone endgames
    const bool nearMateWindow = SearchConfig.USE_NMP_ZUG_GUARD
                                && (beta > VALUE_CHECKMATE_THRESHOLD - SearchConfig.NMP_NEAR_MATE_MARGIN
                                    || alpha < -VALUE_CHECKMATE_THRESHOLD + SearchConfig.NMP_NEAR_MATE_MARGIN);
    const bool zugProne = SearchConfig.USE_NMP_ZUG_GUARD
                          && p.getMaterialNonPawn(us) <= SearchConfig.NMP_ZUG_NONPAWN_THRESHOLD;

    if (doNull
        && nodeType != PvNode
        && depth >= SearchConfig.NMP_DEPTH
        && !hasCheck
        && !zugProne
        && !nearMateWindow) {
      // possible other criteria: eval > beta

      // determine depth reduction
      // ICCA Journal, Vol. 22, No. 3
      // Ernst A. Heinz, Adaptive Null-Move Pruning, postscript
      // http://people.csail.mit.edu/heinz/ps/adpt_null.ps.gz
      auto r = Depth{SearchConfig.NMP_REDUCTION};
      if (depth > 8 || (depth > 6 && p.getGamePhase() >= 3)) { ++r; }
      // Increase NMP reduction when position is not improving
      if (SearchConfig.USE_NMP_IMPROVING && !improving) {
        r += static_cast<Depth>(SearchConfig.NMP_IMPROVING_REDUCTION);
      }
      Depth newDepth = depth - r - 1;
      // double check that depth does not get negative
      if (newDepth < 0) { newDepth = DEPTH_NONE; }

      // do null move search
      // Null move child is a CUT node (we're trying to prove fail-high with null window)
      p.doNullMove();
      thread().nodesVisited++;
      Value nValue = -search(p, newDepth, ply + 1, -beta, -beta + 1, CutNode, No_Null_Move);
      p.undoNullMove();

      // Detect mate threats: if we get mated even without moving, there's a
      // serious threat that warrants search extensions later.
      if (nValue < -(VALUE_CHECKMATE - 2 * SearchConfig.THREAT_EXT_MATE_DEPTH)) { // configurable mate-in-N threshold
        matethreat = true;
      }

      // Fail-hard NMP: clamp to beta. NMP only proves value >= beta, not the
      // exact value. Fail-soft can return wildly inflated values (e.g., 8998 cp
      // in endgames) that contaminate the TT and cause bogus scores in aspiration
      // re-searches. The nearMateWindow guard already disables NMP when beta is
      // near checkmate range, so beta is always a normal score here.
      if (nValue > beta) {
        nValue = beta;
      }

      // if the value is higher than beta even after not making
      // a move, it is not worth searching as it will very likely
      // be above beta if we make a move
      if (nValue >= beta) {
        // Verification search to reduce tactical false positives
        if (SearchConfig.USE_NMP_VERIFY
            && depth >= SearchConfig.NMP_VERIFY_MIN_DEPTH) {
          Depth verifyDepth = depth - r - SearchConfig.NMP_VERIFY_MARGIN;
          if (verifyDepth < 0) { verifyDepth = DEPTH_NONE; }
          const auto do_null = matethreat ? No_Null_Move : Do_Null_Move;
          // Verification search inherits nodeType (verifying same node type)
          const Value v = search(p, verifyDepth, ply, beta - 1, beta, nodeType, do_null);
          if (v < beta) {
            STAT_INC(thread().statistics.nullMoveVerifications);
            // fall through: no cutoff
          }
          else {
            if (SearchConfig.USE_TT) { storeTt(p, depth, ply, MOVE_NONE, beta, BETA, staticEval); }
            STAT_INC(thread().statistics.nullMoveCuts);
            return beta;
          }
        }
        else {
          if (SearchConfig.USE_TT) { storeTt(p, depth, ply, MOVE_NONE, beta, BETA, staticEval); }
          STAT_INC(thread().statistics.nullMoveCuts);
          return beta;
        }
      }
    }
  }

  // Internal Iterative Reduction (IIR)
  // https://www.chessprogramming.org/Internal_Iterative_Reductions
  // Simply reduces depth when no TT move is available. The reduced search
  // will populate the TT, providing a move for future iterations.
  // Simpler and applies to all node types.
  // Note: Creates local mutable copy of depth since parameter is const.
  Depth searchDepth = depth;
  if (SearchConfig.USE_IIR
      && !ttMove
      && searchDepth >= SearchConfig.IIR_DEPTH
      && (SearchConfig.IIR_ALL_NODES || nodeType == PvNode)) {
    searchDepth = searchDepth - SearchConfig.IIR_REDUCTION;
    STAT_INC(thread().statistics.iirReductions);
  }

  // reset move generator for the actual search
  // Use mgSingular when in singular verification search (excludedMove is set)
  // to avoid corrupting the outer search's MoveGenerator state
  auto& info       = thread().plyStack[ply];
  auto* const myMg = info.excludedMove != MOVE_NONE ? info.mgSingular.get() : info.mg.get();
  myMg->resetOnDemand();

  // PV Move Sort
  // When we received a best move for the position from the
  // TT, we set it as PV move in the move-gen so it will
  // be searched first.
  if (SearchConfig.USE_TT_PV_MOVE_SORT && ttMove != MOVE_NONE) {
    STAT_INC(thread().statistics.TtMoveUsed);
    myMg->setPV(ttMove);
  }
  else { STAT_INC(thread().statistics.NoTtMove); }

  // prepare move loop
  Value value;
  Move move;
  int movesSearched = 0; // to detect mate situations

  // ///////////////////////////////////////////////////////
  // MOVE LOOP
  while ((move = myMg->getNextPseudoLegalMove(p, GenAll, hasCheck)) != MOVE_NONE) {
    // Skip excluded move (used for singular extension verification searches)
    if (move == info.excludedMove) { continue; }

    const Square from     = move.from();
    const Square to       = move.to();
    const bool givesCheck = p.givesCheck(move);

    // prepare newDepth
    const Depth newDepthFixed = searchDepth - DEPTH_ONE; // default depth reduction for the next ply
                                                         // might have already been reduced by IIR,
                                                         // but can be further reduced by LMR or
                                                         // extended by extensions
    Depth newDepth  = newDepthFixed;                     // default depth for the next ply - might be extended later
    Depth lmrDepth  = newDepthFixed;                     // default depth for LMR reductions - might be reduced later
    Depth extension = DEPTH_NONE;                        // default extension for the next ply - none, but might be set by extensions

    // Here we try some search extensions. This has to be done
    // very carefully as it usually is more effective to prune
    // than to extend.
    if (SearchConfig.USE_EXTENSIONS) {
      // Check extension: extend when a move gives check, but only for the
      // first few moves (which are the most promising due to move ordering).
      // This limits search explosion while focusing extensions on important checks.
      // The QS search already handles all check evasions, but this extension
      // allows the normal search pruning techniques to be applied.
      // Optional SEE filter: only extend checks that don't lose material.
      if (SearchConfig.USE_CHECK_EXT
          && depth >= SearchConfig.CHECK_EXT_MIN_DEPTH
          && givesCheck
          && movesSearched < SearchConfig.CHECK_EXT_EARLY_LIMIT
          && (!SearchConfig.USE_CHECK_EXT_SEE || See::see(p, move) >= 0)) {
        STAT_INC(thread().statistics.checkExtension);
        extension = DEPTH_ONE;
      }

      // If we have found a mate threat during Null Move Search
      // we extend normal search by one ply to try to find
      // a way out.
      // Deactivated in config as this grows the search tree
      // too much.
      if (SearchConfig.USE_THREAT_EXT
          && matethreat) {
        STAT_INC(thread().statistics.threatExtension);
        extension = DEPTH_ONE;
      }

      // Singular Extensions
      // https://www.chessprogramming.org/Singular_Extensions
      // When we have a TT move that appears significantly better than all alternatives,
      // extend its search to avoid missing critical tactical lines.
      // We do a reduced-depth null-window search excluding the TT move to verify
      // that no other move can reach close to the TT value.
      if (SearchConfig.USE_SINGULAR_EXT
          && extension == 0                                   // no other extension applied
          && move == ttMove                                   // this is the TT move
          && depth >= SearchConfig.SINGULAR_MIN_DEPTH         // sufficient depth
          && ttValue != VALUE_NONE                            // valid TT value
          && !hasCheck                                        // not in check (avoid instability)
          && ttDepth >= depth - 3                             // TT entry was from similar or deeper search
          && std::abs(ttValue) < VALUE_CHECKMATE_THRESHOLD) { // not a mate score

        // Reduced beta for the verification search
        const Value singularBeta = ttValue - Value{SearchConfig.SINGULAR_MARGIN};

        // Reduced depth for the verification search
        Depth singularDepth = (depth - SearchConfig.SINGULAR_REDUCTION) / 2;
        if (singularDepth < 1) { singularDepth = DEPTH_ONE; }

        // Set the excluded move for this ply so the verification search skips the TT move
        info.excludedMove = ttMove;

        STAT_INC(thread().statistics.singularSearches);

        // Do a null-window search to see if any other move can reach singularBeta
        // Uses mgSingular automatically because excludedMove is set
        // Singular verification is a CUT node search (looking for fail-high)
        const Value singularValue = search(p, singularDepth, ply, singularBeta - 1, singularBeta, CutNode, No_Null_Move);

        // Clear the excluded move
        info.excludedMove = MOVE_NONE;

        // If no other move reaches singularBeta, the TT move is singular - extend it
        if (singularValue < singularBeta) {
          STAT_INC(thread().statistics.singularExtension);
          extension = DEPTH_ONE;
        }
      }

      // With this turned off, we still can use extension to
      // at least avoid reductions for these moves by checking the extension variable
      // before applying reductions.
      if (SearchConfig.USE_EXT_ADD_DEPTH) {
        newDepth += extension;
      }
    }

    // ///////////////////////////////////////////////////////
    // Forward Pruning
    // FP will only be done when the move is not
    // interesting - no check, no capture, etc.
    if (nodeType != PvNode
        && extension == 0
        && move != ttMove
        && move != myMg->getKillerMoves()[0]
        && move != myMg->getKillerMoves()[1]
        && move.type() != PROMOTION
        && !p.isCapturingMove(move)
        && !hasCheck
        && !givesCheck
        && !matethreat) {

      // to check in futility pruning what material delta we have
      const auto moveGain = valueOf(p.getPiece(to));

      // Futility Pruning
      // Using an array of margin values for each depth
      // we try to prune moves if they seem not worth
      // searching any further. They are so far below
      // alpha that we can assume a beta cutoff in the
      // next iteration anyway.
      // This is a typical forward pruning technique
      // which might lead to errors.
      // Limited Razoring / Extended FP are covered by this.
      if (SearchConfig.USE_FP && depth < 7) {
        auto futilityMargin = SearchConfig.FP_MARGIN[depth];
        // Increase margin when not improving → prune less aggressively (Stockfish-style)
        // Rationale: "not improving" means eval may be unreliable, so search more carefully
        if (SearchConfig.USE_FP_IMPROVING && !improving) {
          // TODO: Test this with different values
          futilityMargin += SearchConfig.FP_IMPROVING_MARGIN;
        }
        if (staticEval + moveGain + futilityMargin <= alpha) {
          if (staticEval + moveGain > bestNodeValue) { bestNodeValue = staticEval + moveGain; }
          STAT_INC(thread().statistics.fpPrunings);
          continue;
        }
      }

      // LMP - Late Move Pruning
      // aka Move-Count-Based Pruning
      if (SearchConfig.USE_LMP) {
        const int lmpDepth = depth > 15 ? 15 : depth;
        int lmpThreshold   = SearchConfig.LMP_MOVES[lmpDepth];
        // When improving, allow searching more moves before pruning
        if (SearchConfig.USE_LMP_IMPROVING && improving) {
          lmpThreshold += lmpThreshold / 2; // 50% more moves when improving
        }
        if (movesSearched >= lmpThreshold) {
          STAT_INC(thread().statistics.lmpCuts);
          continue;
        }
      }
    }

    // LMR - Late Move Reduction
    // Applied more broadly than FP/LMP (outside the pruning guard block).
    // Uses reduction adjustments for special moves instead of excluding them.
    // Late Move Reduction assumes that later moves rarely exceed alpha, and
    // therefore the search is reduced in depth. This is, in effect, a soft
    // transition into quiescence search.
    if (SearchConfig.USE_LMR
        && depth >= SearchConfig.LMR_MIN_DEPTH
        && movesSearched >= SearchConfig.LMR_MIN_MOVES
        && extension == 0
        && nodeType != PvNode
        && !hasCheck
        && !matethreat) {

      const int d = std::min(depth, Depth{31});
      const int m = std::min(movesSearched, 63);
      lmrDepth -= static_cast<Depth>(thread().LMR_REDUCTION[d][m]);

      // Reduce more when position is NOT improving (eval not better than 2 plies ago)
      if (SearchConfig.USE_LMR_IMPROVING && !improving) {
        lmrDepth -= static_cast<Depth>(SearchConfig.LMR_IMPROVING_REDUCTION);
      }

      // Reduce more on expected cut nodes (expected to fail high)
      // Late moves on cut nodes are very unlikely to be the best move
      if (SearchConfig.USE_LMR_CUTNODE && nodeType == CutNode) {
        lmrDepth -= static_cast<Depth>(SearchConfig.LMR_CUTNODE_REDUCTION);
        STAT_INC(thread().statistics.lmrCutNodeReductions);
      }

      // Reduce less for moves with good history (frequently caused beta cutoffs)
      // histScore > 0 means good move -> negative reduction adjustment -> less reduction
      if (SearchConfig.USE_LMR_HISTORY) {
        const int histScore     = thread().history.historyCount[us][from][to];
        const int histReduction = -histScore / SearchConfig.LMR_HISTORY_DIVISOR;
        if (histReduction < 0) {
          // Positive history -> less reduction (histReduction is negative)
          STAT_INC(thread().statistics.lmrHistoryLessReduction);
          STAT_ADD(thread().statistics.lmrHistoryDepthSaved, -histReduction); // Convert to positive for tracking
        }
        lmrDepth -= static_cast<Depth>(histReduction);
      }

      // Reduce less for special moves (still search them, but with less reduction)
      // TT move: proven good in past searches
      // Killer moves: caused beta cutoffs at this ply
      // Checking moves: tactical, should be searched deeper
      if (move == ttMove
          || move == myMg->getKillerMoves()[0]
          || move == myMg->getKillerMoves()[1]
          || givesCheck) {
        lmrDepth += DEPTH_ONE;
      }

      // Don't reduce captures and promotions (they change material balance)
      if (p.isCapturingMove(move) || move.type() == PROMOTION) {
        lmrDepth = newDepth; // No reduction for captures/promotions
      }

      // Clamp: don't go below DEPTH_NONE and don't exceed newDepth (no extension via LMR!)
      lmrDepth = std::clamp(lmrDepth, DEPTH_NONE, newDepth);

      if (lmrDepth < newDepth) {
        STAT_INC(thread().statistics.lmrReductions);
      }
    }

    // ///////////////////////////////////////////////////////

    // ///////////////////////////////////////////////////////
    // DO MOVE
    p.doMove(move);

    // checking for legality is quite expensive, so we do it as late as possible
    // after we tried to prune the move already
    // if a move is illegal, we just undo and continue with the next move
    // Note: wasLegalMove does not check if the move was actually valid on the
    // position but only if a pseudoMove that was assumed valid was legal.
    if (!p.wasLegalMove()) {
      p.undoMove();
      continue;
    }

    // if available on platform tells the cpu to
    // prefetch the tt data into cpu caches
    TT_PREFETCH;
    // EVAL_PREFETCH;

    // we only count legal moves
    thread().nodesVisited++;
    thread().statistics.currentVariation.push_back(move);

    sendSearchUpdateToUci();

    // check repetition and 50 moves
    if (checkDrawRepAnd50(p, 2)) { value = drawScore(p); }
    else {

      const auto do_null = matethreat ? No_Null_Move : Do_Null_Move;

      // ///////////////////////////////////////////////////////////////////
      // PVS
      // First move in Node will be searched with the full window. Due to move
      // ordering we assume this is the PV. Every other move is searched with
      // a null window as we only try to prove that the move is bad (<alpha)
      // or that the move is too good (>beta). If this prove fails we need
      // to research the move again with a full window.
      // https://www.chessprogramming.org/Principal_Variation_Search
      //
      // Node type logic for children:
      // - PV node's first child: PvNode (inherits PV status)
      // - PV node's other children: CutNode (null window, expect fail-high)
      // - CutNode's children: AllNode (expect fail-low)
      // - AllNode's children: CutNode (expect fail-high)
      if (!SearchConfig.USE_PVS || movesSearched == 0) {
        // First move: PV node's child inherits PV, CutNode's child is AllNode, AllNode's child is CutNode
        const NodeType childType = (nodeType == PvNode) ? PvNode : (nodeType == CutNode ? AllNode : CutNode);
        value                    = -search(p, newDepth, ply + 1, -beta, -alpha, childType, do_null);
      }
      else {
        // Null window search after the initial PV search.
        // As depth we use a potentially reduced depth if Late Move Reduction
        // conditions have been met above.
        // Later moves with null window: child is CutNode (expect fail-high) or AllNode (if we're CutNode)
        const NodeType childType = (nodeType == CutNode) ? AllNode : CutNode;
        value                    = -search(p, lmrDepth, ply + 1, -alpha - 1, -alpha, childType, do_null);
        // If this move improved alpha without exceeding beta we do a proper full window
        // search to get an accurate score.
        // Without LMR we check for value > alpha && value < beta
        // With LMR we re-search when value > alpha
        if (value > alpha && !isTimeAlmostUp()) {
          // did we actually have a LMR reduction?
          if (lmrDepth < newDepthFixed) {
            STAT_INC(thread().statistics.lmrResearches);
            // Re-search with full depth: if we're PV, child becomes PV; otherwise same alternation
            const NodeType researchType = (nodeType == PvNode) ? PvNode : childType;
            value                       = -search(p, newDepth, ply + 1, -beta, -alpha, researchType, do_null);
          }
          else if (value < beta) {
            STAT_INC(thread().statistics.pvsResearches);
            // PVS re-search: if we're PV, child becomes PV; otherwise same alternation
            const NodeType researchType = (nodeType == PvNode) ? PvNode : childType;
            value                       = -search(p, newDepth, ply + 1, -beta, -alpha, researchType, do_null);
          }
        }
      }
      // ///////////////////////////////////////////////////////////////////
    }

    movesSearched++;
    thread().statistics.currentVariation.pop_back();
    p.undoMove();
    // UNDO MOVE
    // ///////////////////////////////////////////////////////


    // Did we find a better move for this node (not ply)?
    // For the first move this is always the case.
    if (value > bestNodeValue) {
      // These "best" values are only valid for this node
      // not for all the ply (not yet clear if >alpha)
      bestNodeValue = value;
      bestNodeMove  = move;

      // Did we find a better move than in previous nodes in ply
      // then this is our new PV and best move for this ply.
      // If we never find a better alpha this means all moves in
      // this node are worse than other moves in other nodes which
      // raised alpha - meaning we have a better move from another
      // node we would play. We will return alpha and store a alpha
      // node in TT.
      if (value > alpha) {
        // If we found a move that is better or equal than beta,
        // this means that the opponent can/will avoid this
        // position altogether, so we can stop to search this node.
        // We will not know if our best move is really the
        // best move or how good it really is (value is a lower bound)
        // as we cut off the rest of the search of the node here.
        // We will save the move as a killer to be able to search it
        // earlier in another node of the ply.
        if (value >= beta && SearchConfig.USE_ALPHABETA) {
          // Count beta cuts
          STAT_INC(thread().statistics.betaCuts);
          // Track beta cuts by move index (0-based, clamped to array size)
          STAT_INC(thread().statistics.betaCutsByIndex[std::min(movesSearched - 1, SearchStats::BETA_CUTS_INDEX_SIZE - 1)]);
          // Track if TT move caused the cutoff (first move and TT move was set)
          if (movesSearched == 1 && ttMove != MOVE_NONE && move == ttMove) {
            STAT_INC(thread().statistics.ttMoveBestMove);
          }
          // store move which caused a beta cutoff in this ply
          if (SearchConfig.USE_KILLER_MOVES && !p.isCapturingMove(move)) { myMg->storeKiller(move); }
          // Counter for moves which caused a beta cutoff
          // we use 1 << depth as an increment to favor deeper searches
          if (SearchConfig.USE_HISTORY_COUNTER && !p.isCapturingMove(move)) { thread().history.historyCount[us][from][to] += 1L << depth; }
          // store a successful counter-move to the previous opponent move
          if (SearchConfig.USE_HISTORY_MOVES) {
            const Move lastMove = p.getLastMove();
            if (lastMove != MOVE_NONE) { thread().history.counterMoves[lastMove.from()][lastMove.to()] = move; }
          }
          ttType = BETA;
          break;
        }
        // We found a move between alpha and beta which means we
        // really have found the best move so far in the ply which
        // can be forced (opponent can't avoid it).
        thread().pv.update(move, ply);

        // We raise alpha so the successive searches in this ply
        // need to find even better moves or dismiss the moves.
        alpha  = value;
        ttType = EXACT;
        // Skip history penalty for the move that raised alpha
        continue;
      }
    }
    // Decrease history only for quiet moves that failed to improve alpha
    if (SearchConfig.USE_HISTORY_COUNTER && !p.isCapturingMove(move)) {
      thread().history.historyCount[us][from][to] -= 1L << depth;
      if (thread().history.historyCount[us][from][to] < 0) { thread().history.historyCount[us][from][to] = 0; }
    }
  }
  // MOVE LOOP
  // ///////////////////////////////////////////////////////

  // If we did not have at least one legal move
  // then we might have a mate or stalemate
  if (movesSearched == 0 && !stopConditions()) {
    if (hasCheck) {
      // mate
      STAT_INC(thread().statistics.checkmates);
      bestNodeValue = -VALUE_CHECKMATE + static_cast<Value>(ply);
    }
    else {
      // stalemate
      STAT_INC(thread().statistics.stalemates);
      bestNodeValue = drawScore(p);
    }
    // this is in any case an exact value
    staticEval = bestNodeValue;
    ttType     = EXACT;
  }

  // Store TT
  // Store search result for this node into the transposition table
  // Use searchDepth (may be reduced by IIR) rather than original depth
  if (SearchConfig.USE_TT) { storeTt(p, searchDepth, ply, bestNodeMove, bestNodeValue, ttType, staticEval); }

  return bestNodeValue;
}

Value Search::qsearch(Position& p, const Depth ply, Value alpha, Value beta, const NodeType nodeType) {
  //  LOG__DEBUG(Logger::get().SEARCH_LOG, "QSearch {} {}", ply, str(thread().statistics.currentVariation));

  // Track PV vs non-PV node statistics
  // In qsearch, CutNode/AllNode are treated the same as non-PV
#ifndef FRANKYCPP_PRODUCTION
  if (nodeType == PvNode) { thread().statistics.pvNodes++; }
  else { thread().statistics.nonPvNodes++; }
  thread().statistics.qsearchNodes++;
#endif

  // Clear PV for this node (same reason as in search())
  thread().pv.clear(ply);

  if (thread().statistics.currentExtraSearchDepth < ply) { thread().statistics.currentExtraSearchDepth = ply; }

  // if we have deactivated qsearch or we have reached our maximum depth
  // we evaluate the position and return the value
  if (!SearchConfig.USE_QUIESCENCE || ply >= MAX_DEPTH || stopConditions()) {
    ESSENTIAL_STAT_INC(thread().statistics.perftNodeCount);
    return evaluate(p);
  }

  // Mate Distance Pruning
  // Did we already find a shorter mate then ignore this one.
  if (SearchConfig.USE_MDP) {
    alpha = std::max(alpha, -VALUE_CHECKMATE + static_cast<Value>(ply));
    beta  = std::min(beta, VALUE_CHECKMATE - static_cast<Value>(ply));
    if (alpha >= beta) {
      STAT_INC(thread().statistics.mdp);
      return alpha;
    }
  }

  // TT Lookup
  Move ttMove      = MOVE_NONE;
  Value staticEval = VALUE_NONE;
  if (SearchConfig.USE_TT && SearchConfig.USE_QS_TT) {
    STAT_INC(thread().statistics.ttProbes);
    if (const auto ttEntry = tt->probe(p.getZobristKey(), thread().id)) {
      // tt hit
      const auto probedMove = static_cast<Move>(ttEntry->move);
      // Validate TT move before use - corrupted/torn reads from lockless TT
      // could produce garbage moves that would corrupt position state in doMove()
      if (probedMove != MOVE_NONE && MoveGenerator::isPseudoLegal(p, probedMove)) {
        ttMove = probedMove;
      }

      // Track hit quality by bound type (qsearch has no depth requirement)
      // In qsearch, any hit is "sufficient depth" since we're at leaf nodes
      STAT_INC(thread().statistics.ttHitSufficientDepth);
      switch (ttEntry->type) {
        case NONE:
          STAT_INC(thread().statistics.ttHitNone);
          break;
        case EXACT:
          STAT_INC(thread().statistics.ttHitExact);
          break;
        case ALPHA:
          STAT_INC(thread().statistics.ttHitAlpha);
          break;
        case BETA:
          STAT_INC(thread().statistics.ttHitBeta);
          break;
      }

      const Value ttValue = valueFromTt(ttEntry->value, ply);
      if (SearchConfig.USE_TT_VALUE
          && nodeType != PvNode
          && ttValue.isValid()
          && (ttEntry->type == EXACT
              || (ttEntry->type == ALPHA && ttValue <= alpha)
              || (ttEntry->type == BETA && ttValue >= beta))) {
        STAT_INC(thread().statistics.TtCuts);
        STAT_INC(thread().statistics.ttCutsQsearch); // qsearch cut (depth 0)
        return ttValue;
      }
      // if we have a static eval stored we can reuse it
      if (SearchConfig.USE_EVAL_TT
          && ttEntry->eval != VALUE_NONE) {
        STAT_INC(thread().statistics.evalFromTT);
        staticEval = ttEntry->eval;
      }
    }
    else {
      STAT_INC(thread().statistics.ttMisses);
    }
  } // use TT

  // prepare node search
  Value bestNodeValue = VALUE_NONE;
  Move bestNodeMove   = MOVE_NONE; // used to store in the TT
  ValueType ttType    = ALPHA;
  const bool hasCheck = p.hasCheck();

  // if in check we simply do a normal search (all moves) in qsearch
  if (!hasCheck) {
    // get an evaluation for the position
    if (staticEval == VALUE_NONE) {
      staticEval = evaluate(p);
    }
    // Quiescence StandPat
    // Use evaluation as a standing pat (lower bound)
    // https://www.chessprogramming.org/Quiescence_Search#Standing_Pat
    // Assumption is that there is at least on move which would improve the
    // current position. So if we are already >beta we don't need to look at it.
    if (SearchConfig.USE_QS_STANDPAT_CUT && staticEval > alpha) {
      if (staticEval >= beta) {
        STAT_INC(thread().statistics.standpatCuts);
        // Storing this value might save us calls to eval on the same position.
        if (SearchConfig.USE_TT
            && SearchConfig.USE_QS_TT
            && SearchConfig.USE_EVAL_TT) {
          storeTt(p, DEPTH_NONE, ply, MOVE_NONE, VALUE_NONE, NONE, staticEval);
        }
        return staticEval;
      }
      alpha = staticEval;
    }
    bestNodeValue = staticEval;
  }

  // reset move generator for the move loop
  auto* const myMg = thread().plyStack[ply].mg.get();
  myMg->resetOnDemand();

  // PV Move Sort
  if (SearchConfig.USE_TT_PV_MOVE_SORT && ttMove != MOVE_NONE) {
    STAT_INC(thread().statistics.TtMoveUsed);
    myMg->setPV(ttMove);
  }
  else {
    STAT_INC(thread().statistics.NoTtMove);
  }

  // prepare move loop
  Value value;
  Move move;
  int movesSearched = 0; // to detect mate situations

  // when in check generate all moves
  const GenMode genMode = hasCheck ? GenAll : GenNonQuiet;

  // ///////////////////////////////////////////////////////
  // MOVE LOOP
  while ((move = myMg->getNextPseudoLegalMove(p, genMode, hasCheck)) != MOVE_NONE) {
    const Square to       = move.to();
    const bool givesCheck = p.givesCheck(move);

    // Forward Pruning
    // FP will only be done when the move is not
    // interesting - no check, no capture, etc.
    if (SearchConfig.USE_QFP
        && nodeType != PvNode
        && move != ttMove
        && move != myMg->getKillerMoves()[0]
        && move != myMg->getKillerMoves()[1]
        && move.type() != PROMOTION
        && !hasCheck
        && !givesCheck // post move
    ) {
      // to check in futility pruning what material delta we have
      const auto moveGain           = valueOf(p.getPiece(to));
      constexpr auto futilityMargin = Value{150};
      if (staticEval + moveGain + futilityMargin <= alpha) {
        if (staticEval + moveGain > bestNodeValue) { bestNodeValue = staticEval + moveGain; }
        STAT_INC(thread().statistics.qfpPrunings);
        continue;
      }
    }

    // reduce the number of moves searched in quiescence
    // by looking at good captures only
    if (!hasCheck && !goodCapture(p, move, givesCheck)) { continue; }

    // ///////////////////////////////////////////////////////
    // DO MOVE
    p.doMove(move);
    if (!p.wasLegalMove()) {
      p.undoMove();
      continue;
    }

    // if available on platform tells the cpu to
    // prefetch the data into cpu caches
    TT_PREFETCH;
    EVAL_PREFETCH;

    // we only count legal moves
    thread().nodesVisited++;
    thread().statistics.currentVariation.push_back(move);
    sendSearchUpdateToUci();

    // check repetition and 50 moves
    if (checkDrawRepAnd50(p, 2)) {
      value = drawScore(p);
    }
    else {
      // recursion into qsearch - inherit nodeType (PV nodes stay PV, non-PV stay non-PV)
      value = -qsearch(p, ply + 1, -beta, -alpha, nodeType);
    }

    movesSearched++;
    thread().statistics.currentVariation.pop_back();
    p.undoMove();
    // UNDO MOVE
    // ///////////////////////////////////////////////////////


    // See the search function above for documentation
    if (value > bestNodeValue) {
      bestNodeValue = value;
      bestNodeMove  = move;
      if (value > alpha) {
        if (value >= beta && SearchConfig.USE_ALPHABETA) {
          STAT_INC(thread().statistics.betaCuts);
          STAT_INC(thread().statistics.betaCutsByIndex[std::min(movesSearched - 1, SearchStats::BETA_CUTS_INDEX_SIZE - 1)]);
          // Track if TT move caused the cutoff
          if (movesSearched == 1 && ttMove != MOVE_NONE && move == ttMove) {
            STAT_INC(thread().statistics.ttMoveBestMove);
          }
          // Note: No killer/history updates in qsearch - we primarily search captures,
          // and history/killers are for quiet move ordering in main search.
          ttType = BETA;
          break;
        }
        thread().pv.update(move, ply);
        alpha  = value;
        ttType = EXACT;
      }
    }
    // Note: No history penalty in qsearch - see comment above at beta cutoff.
  }
  // MOVE LOOP
  // ///////////////////////////////////////////////////////

  // If we did not have at least one legal move,
  // then we might have a mate or stalemate
  if (movesSearched == 0 && !stopConditions()) {
    // if we have a mate, we had a check before and therefore
    // generated all moves. We can be sure this is a mate.
    if (hasCheck) {
      // mate
      STAT_INC(thread().statistics.checkmates);
      bestNodeValue = -VALUE_CHECKMATE + static_cast<Value>(ply);
      ttType        = EXACT;
    }
    // if we do not have a mate we had no check and
    // therefore might have only quiet moves which
    // we did not generate.
    // We return the standpat value in this case
    // which we have set to bestNodeValue in the
    // static eval earlier
  }

  // Store TT
  // Store search result for this node into the transposition table
  if (SearchConfig.USE_TT && SearchConfig.USE_QS_TT) {
    storeTt(p, DEPTH_NONE, ply, bestNodeMove, bestNodeValue, ttType, staticEval);
  }

  return bestNodeValue;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
inline Value Search::evaluate(const Position& p) { // NOLINT(*-convert-member-functions-to-static)
  STAT_INC(thread().statistics.leafPositionsEvaluated);
  STAT_INC(thread().statistics.evaluations);
  return thread().evaluator.evaluate(p);
}

bool Search::goodCapture(const Position& p, const Move move, const bool givesCheck) const {
  // Captures that give check are always good
  if (p.isCapturingMove(move) && givesCheck) {
    return true;
  }

  if (SearchConfig.USE_QS_SEE) {
    // Check SEE score of higher-value pieces to low-value pieces
    return See::see(p, move) >= 0;
  }
  return
    // all pawn captures - they never lose material
    // typeOf(position.getPiece(getFromSquare(move))) == PAWN

    // Lower value piece captures a higher value piece
    // With a margin to also look at Bishop x Knight
    valueOf(p.getPiece(move.from())) + 50 < valueOf(p.getPiece(move.to()))

    // all recaptures should be looked at
    || (p.getLastMove() != MOVE_NONE && p.getLastCapturedPiece() != PIECE_NONE && p.getLastMove().to() == move.to())

    // undefended pieces captures are good
    // If the defender is "behind" the attacker, this will not be recognized
    // here This is not too bad as it only adds a move to qsearch which we
    // could otherwise ignore
    || !p.isAttacked(move.to(), ~p.getNextPlayer());
}

void Search::storeTt(
  const Position& p,
  const Depth depth,
  const Depth ply,
  const Move move,
  const Value value,
  const ValueType valueType,
  const Value eval) const {
  tt->put(p.getZobristKey(), depth, move, valueToTt(value, ply), valueType, eval, thread().id);
}

Value Search::valueToTt(const Value value, const Depth ply) {
  if (value.isCheckMate()) {
    if (value > 0) { return value + static_cast<Value>(ply); }
    return value - static_cast<Value>(ply);
  }
  return value;
}

Value Search::valueFromTt(const Value value, const Depth ply) {
  if (value.isCheckMate()) {
    if (value > 0) { return value - static_cast<Value>(ply); }
    return value + static_cast<Value>(ply);
  }
  return value;
}


void Search::initialize() {
  LOG__INFO(Logger::get().SEARCH_LOG, "Search initialization.");
  // init opening book
  if (SearchConfig.USE_BOOK) {
    if (!book) {
      // only initialize once
      // Resolve book path relative to executable directory (not CWD)
      const auto bookPath = resolvePathRelativeToExe(SearchConfig.BOOK_PATH);
      if (!std::filesystem::exists(bookPath)) {
        const std::string message = std::format("Opening Book '{}' not found. Disabling book usage.", bookPath.string());
        LOG__ERROR(Logger::get().BOOK_LOG, "{}", message);
        ConfigManager::instance().applyOverrides([&](SearchConfigData& s, auto&) {
          s.USE_BOOK = false;
        });
      }
      else {
        book = std::make_unique<book::OpeningBook>(bookPath.string(), book::OpeningBook::fromString(SearchConfig.BOOK_TYPE));
        book->initialize();
      }
    }
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Opening Book disabled in configuration");
  }

  // init transposition table
  if (SearchConfig.USE_TT) {
    // When constructed with size 0 MB, TT has 1 cluster (4 entries); treat that as uninitialized sentinel
    if (tt->getMaxNumberOfClusters() == 1) {
      tt->resize(SearchConfig.TT_SIZE_MB);
    }
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table disabled in configuration");
    // Keep TT allocated but minimize its size to 0 MB (internally becomes 1 entry)
    tt->resize(0);
  }

  // init shared PawnTT (Evaluators are per-thread in SearchThreadData)
  if (!pawnTT) {
    pawnTT = std::make_unique<PawnTT>(0); // Start with 0, resize below if enabled
  }
  const auto& EvalConfig = ConfigManager::instance().eval();
  if (EvalConfig.USE_PAWN_TT && EvalConfig.PAWN_TT_SIZE_MB > 0) {
    if (pawnTT->getMaxNumberOfEntries() == 0) {
      pawnTT->resize(static_cast<uint64_t>(EvalConfig.PAWN_TT_SIZE_MB));
    }
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Pawn Cache disabled in configuration");
    pawnTT->resize(0);
  }

  // init tablebase
  initTablebase();
}

void Search::initTablebase() {
  // Only initialize once
  if (syzygy_tb) {
    return;
  }

  // Check if tablebase path is configured
  if (SearchConfig.TB_PATH.empty()) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Syzygy Tablebase: No path configured, tablebases disabled");
    return;
  }

  syzygy_tb = std::make_unique<tablebase::Tablebase>();
  if (syzygy_tb->initialize(SearchConfig.TB_PATH)) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Syzygy Tablebase: Initialized with {} pieces from '{}'",
              syzygy_tb->maxPieces(), SearchConfig.TB_PATH);

    // Pre-warm OS file cache to reduce latency on first in-game probes
    if (SearchConfig.TB_CACHE_PREWARM) {
      const int maxPieces = std::min(SearchConfig.TB_CACHE_PREWARM_PIECES, syzygy_tb->maxPieces());
      syzygy_tb->prewarmCache(maxPieces);
    }
  }
  else {
    LOG__WARN(Logger::get().SEARCH_LOG, "Syzygy Tablebase: Failed to initialize from '{}'", SearchConfig.TB_PATH);
    syzygy_tb.reset(); // Release failed instance
  }
}

bool Search::probeTablebaseAtRoot(const Position& pos, SearchResult& result) {

  // Check if position can be probed
  if (!syzygy_tb->canProbe(pos)) {
    LOG__DEBUG(Logger::get().SEARCH_LOG, "TB Root: Position not probeable (pieces={}, castling={})",
               pos.getOccupiedBb().popcount(), pos.getCastlingRights() != NO_CASTLING);
    return false;
  }

  // Probe the tablebase at root (includes DTZ and best move)
  const tablebase::TBProbeResult tbResult = syzygy_tb->probeRoot(pos);

  if (!tbResult.success()) {
    LOG__DEBUG(Logger::get().SEARCH_LOG, "TB Root: Probe failed for position {}", pos.strFen());
    return false;
  }

  // Store WDL and DTZ for later use (filtering, scoring)
  tbRoot.wdl   = tbResult.wdl;
  tbRoot.dtz   = tbResult.dtz;
  tbRoot.move  = tbResult.bestMove;
  tbRoot.value = tablebase::Tablebase::tbResultToScore(tbResult.wdl, tbResult.dtz);

  // Log the TB hit
  const std::string tbResultStr = tablebase::Tablebase::resultToString(tbResult.wdl);
  LOG__INFO(Logger::get().SEARCH_LOG, "TB Root: {} DTZ={} move={} value={} ",
            tbResultStr, tbResult.dtz, tbResult.bestMove.str(), tbRoot.value.str());

  // Send info string to UCI
  sendString(std::format("TB hit: {} DTZ={} move={} value={}", tbResultStr, tbResult.dtz, tbResult.bestMove.str(), tbRoot.value.str()));

  // Populate the search result with DTZ-based scoring
  result.bestMove      = tbRoot.move;
  result.bestMoveValue = tbRoot.value;
  result.tbHit         = true;

  // Update statistics
  STAT_INC(thread().statistics.tbRootHits);

  return true;
}

const SearchThreadData* Search::selectBestThread() const {
  const SearchThreadData* best = searchThreadData[0].get(); // main thread as baseline

  // Single-threaded or feature disabled: always use main thread
  if (numHelperThreads == 0 || !SearchConfig.USE_BEST_THREAD_SELECTION) {
    return best;
  }

  // Hard-limited searches (depth/nodes): only the main thread enforces
  // these limits, so helpers may not have reached the same depth or node
  // count.  Picking an incomplete helper would violate the limit contract.
  if (searchLimits.depth || searchLimits.nodes) {
    return best;
  }

  const auto scoreMargin = Value{SearchConfig.BEST_THREAD_SCORE_MARGIN};

  const int totalThreads = numHelperThreads + 1;
  for (int t = 1; t < totalThreads; ++t) {
    const auto& candidate = *searchThreadData[t];

    // Skip threads that never completed an iteration
    if (candidate.completedIterationDepth == DEPTH_NONE) {
      continue;
    }

    const Depth bestDepth = best->completedIterationDepth;
    const Depth candDepth = candidate.completedIterationDepth;
    const Value bestValue = best->lastIterationValue;
    const Value candValue = candidate.lastIterationValue;

    bool candidateIsBetter = false;

    // Mate scores: depth is irrelevant — only mate distance matters.
    // Higher value = shorter delivering mate or longer delay before being mated.
    if (bestValue.isCheckMate() && candValue.isCheckMate()) { // NOLINT(*-branch-clone)
      candidateIsBetter = (candValue > bestValue);
    }
    else if (candDepth > bestDepth) {
      // Candidate searched deeper: accept unless score is much worse
      candidateIsBetter = (candValue >= bestValue - scoreMargin);
    }
    else if (candDepth == bestDepth) {
      // Same depth: prefer higher score
      candidateIsBetter = (candValue > bestValue);
    }
    else {
      // Candidate shallower: only accept if score is much better
      candidateIsBetter = (candValue > bestValue + scoreMargin);
    }

    if (candidateIsBetter) {
      best = searchThreadData[t].get();
    }
  }

  if (best->id != 0) {
    LOG__INFO(Logger::get().SEARCH_LOG,
              "Best thread selection: helper {} (depth {} value {}) over main (depth {} value {})",
              best->id, best->completedIterationDepth, best->lastIterationValue.str(),
              searchThreadData[0]->completedIterationDepth, searchThreadData[0]->lastIterationValue.str());
  }
  else {
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Best thread selection: main thread retained (depth {} value {})",
               best->completedIterationDepth, best->lastIterationValue.str());
  }

  return best;
}

void Search::sendFinalUciInfo(const SearchThreadData& bestThread) const {
  const nanoseconds& since  = elapsedSince(startSearchTime);
  const uint64_t totalNodes = getTotalNodes();

  // Use the best thread's PV directly (extractPvWithTT uses thread-local state)
  const MoveList pvLine = bestThread.pv.extract();

  if (uciHandler) {
    uciHandler->sendIterationEndInfo(
      bestThread.completedIterationDepth,
      bestThread.statistics.currentExtraSearchDepth,
      bestThread.lastIterationValue,
      totalNodes,
      nps(totalNodes, since),
      MILLISECONDS(since),
      pvLine);
    return;
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "depth {} seldepth {} value {} nodes {:L} nps {:L} time {:L} pv {}",
            bestThread.completedIterationDepth,
            bestThread.statistics.currentExtraSearchDepth,
            bestThread.lastIterationValue.str(),
            totalNodes,
            nps(totalNodes, since),
            MILLISECONDS(since).count(),
            pvLine.str());
}


void Search::applyTBRootOverride(SearchResult& result) const {
  if (tbRoot.wdl == tablebase::TBResult::Failed) {
    return;
  }

  result.tbHit = true;

  // Check if search (best thread) found a proven mate
  const Value searchValue    = result.bestMoveValue;
  const bool searchFoundMate = searchValue >= VALUE_CHECKMATE_THRESHOLD;

  // Calculate mate depth if search found mate (in plies/half-moves)
  const int searchMateDepth = searchFoundMate
                                ? static_cast<int>(VALUE_CHECKMATE) - static_cast<int>(searchValue)
                                : INT_MAX;

  // TB DTZ is distance to zeroing (capture/pawn move), NOT necessarily mate.
  // A proven mate is always preferred over a TB "Win" because:
  // 1. Mate is concrete and forced
  // 2. TB DTZ=1 might be a capture leading to a longer mate sequence
  if (searchFoundMate && searchMateDepth <= tbRoot.dtz) {
    LOG__INFO(Logger::get().SEARCH_LOG,
              "Search found mate in {} (TB DTZ={}), using search move {}",
              searchMateDepth, tbRoot.dtz, result.bestMove.str());
    return;
  }

  // Use TB move — it's DTZ-optimal (shortest path to conversion)
  const Move searchMove = result.bestMove;
  result.bestMove       = tbRoot.move;
  result.bestMoveValue  = tbRoot.value;
  if (searchMove == tbRoot.move) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Search confirmed TB-optimal move {}", tbRoot.move.str());
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG,
              "Using TB-optimal move {} (DTZ={}), search suggested {}",
              tbRoot.move.str(), tbRoot.dtz, searchMove.str());
  }
}

void Search::applyHandicap(SearchResult& result) const {
  // No-op when handicap is disabled
  if (SearchConfig.HANDICAP <= 0) {
    return;
  }

  // Don't override book or TB moves
  if (result.bookMove || result.tbHit) {
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Handicap: skipping — {} move",
               result.bookMove ? "book" : "TB");
    return;
  }

  // Need a valid search result with root moves to pick from
  if (result.bestMove.isNone()) {
    LOG__WARN(Logger::get().SEARCH_LOG, "Handicap: bestMove is MOVE_NONE — this should not happen");
    return;
  }

  // Use main thread's root moves (full pool from inflated MultiPV)
  const auto& rootMoves = mainThread().rootMoves;
  if (rootMoves.empty()) {
    LOG__WARN(Logger::get().SEARCH_LOG, "Handicap: rootMoves is empty — this should not happen");
    return;
  }

  const auto params       = handicap::getHandicapParams(SearchConfig.HANDICAP);
  const Move originalBest = result.bestMove;

  const Move selected = handicap::selectHandicapMove(
    rootMoves,
    params.poolSize,
    params.scoreThreshold,
    position.getZobristKey());

  if (selected.isNone()) {
    return;
  }

  // Override result with handicap-selected move
  result.bestMove      = selected.stripped();
  result.bestMoveValue = selected.value();
  result.ponderMove    = MOVE_NONE; // pondering disabled with handicap
  result.pv.clear();
  result.pv.push_back(result.bestMove);

  if (result.bestMove == originalBest.stripped()) {
    LOG__INFO(Logger::get().SEARCH_LOG,
              "Handicap {}: selected best move {} (score {})",
              SearchConfig.HANDICAP, result.bestMove.str(), result.bestMoveValue.str());
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG,
              "Handicap {}: selected {} (score {}) instead of best move {} (pool={}, threshold={}cp)",
              SearchConfig.HANDICAP, result.bestMove.str(), result.bestMoveValue.str(),
              originalBest.str(), params.poolSize, params.scoreThreshold);
  }
}

void Search::extractPonderMove(SearchResult& result, const SearchThreadData& bestThread) {
  // Try to get ponder move from best thread's PV
  if (bestThread.pv.hasLength(DEPTH_NONE, 2)) {
    result.ponderMove = bestThread.pv(DEPTH_NONE, 1).stripped();
  }
  else {
    // No ponder move in PV — try the TT
    if (SearchConfig.USE_TT && !result.bestMove.isNone()) {
      position.doMove(result.bestMove);
      STAT_INC(mainThread().statistics.ttProbes);
      if (const auto ttEntry = tt->probe(position.getZobristKey())) {
        STAT_INC(mainThread().statistics.ttHitSufficientDepth);
        switch (ttEntry->type) {
          case NONE:
            STAT_INC(mainThread().statistics.ttHitNone);
            break;
          case EXACT:
            STAT_INC(mainThread().statistics.ttHitExact);
            break;
          case ALPHA:
            STAT_INC(mainThread().statistics.ttHitAlpha);
            break;
          case BETA:
            STAT_INC(mainThread().statistics.ttHitBeta);
            break;
        }
        const auto probedMove = static_cast<Move>(ttEntry->move);
        if (probedMove != MOVE_NONE && MoveGenerator::isPseudoLegal(position, probedMove)) {
          result.ponderMove = probedMove;
          STAT_INC(mainThread().statistics.TtMoveUsed);
          LOG__DEBUG(Logger::get().SEARCH_LOG, "Using ponder move from hash table: {}", result.ponderMove.str());
        }
        else {
          STAT_INC(mainThread().statistics.NoTtMove);
        }
      }
      else {
        STAT_INC(mainThread().statistics.ttMisses);
      }
      position.undoMove();
    }
  }

  // Validate ponder move — avoid pondering on mate/draw/illegal positions
  if (result.ponderMove != MOVE_NONE && !result.bestMove.isNone()) {
    position.doMove(result.bestMove);
    if (!position.isLegalMove(result.ponderMove)) {
      LOG__DEBUG(Logger::get().SEARCH_LOG, "ponder move {} omitted as illegal", result.ponderMove.str());
      result.ponderMove = MOVE_NONE;
      position.undoMove();
    }
    else {
      position.doMove(result.ponderMove);
      if (checkDrawRepAnd50(position, 2) || mainThread().plyStack[0].mg->generateLegalMoves(position, GenAll)->empty()) {
        LOG__DEBUG(Logger::get().SEARCH_LOG, "ponder move omitted as game finished");
        result.ponderMove = MOVE_NONE;
      }
      position.undoMove();
      position.undoMove();
    }
  }
}

void Search::filterRootMovesByTB(Position& pos) const {
  // Filter root moves to only those that maintain the TB result.
  // For a winning position, keep only moves where opponent is losing.
  // For a drawn position, keep only moves where opponent is not winning.
  // For a losing position, keep all moves (we're lost anyway).

  if (tbRoot.wdl == tablebase::TBResult::Failed) {
    return; // No TB result, nothing to filter
  }

  // Skip filtering when near the 50-move limit.
  // Root probe uses DTZ (rule50-aware) but child probes use WDL (pure theory).
  // Near the 50-move limit, this mismatch can cause incorrect filtering:
  // - CursedWin/BlessedLoss at root may not match pure WDL in children
  // - Moves that look suboptimal in pure WDL may be optimal given rule50
  if (pos.getHalfMoveClock() >= SearchConfig.TB_RULE50_THRESHOLD) {
    LOG__DEBUG(Logger::get().SEARCH_LOG,
               "TB filter skipped: halfMoveClock {} >= threshold {}",
               pos.getHalfMoveClock(), SearchConfig.TB_RULE50_THRESHOLD);
    return;
  }

  const size_t originalCount = thread().rootMoves.size();

  // Determine what result we need from opponent's perspective after our move
  auto shouldKeepMove = [&](const Move& move) -> bool {
    pos.doMove(move);

    // If child position can't be probed, keep the move (safe default)
    if (!syzygy_tb->canProbe(pos)) {
      pos.undoMove();
      return true;
    }

    const tablebase::TBResult childWdl = syzygy_tb->probeWDL(pos);
    pos.undoMove();

    if (childWdl == tablebase::TBResult::Failed) {
      return true; // Probe failed, keep the move
    }

    // After our move, it's opponent's turn. WDL is from opponent's perspective.
    // If we're winning, opponent should be losing (WDL = Loss or BlessedLoss)
    // If we're drawing, opponent should be drawing (WDL = Draw)
    // If we're losing, any move is acceptable

    switch (tbRoot.wdl) {
      case tablebase::TBResult::Win:
      case tablebase::TBResult::CursedWin:
        // We're winning - opponent must be losing
        return childWdl == tablebase::TBResult::Loss || childWdl == tablebase::TBResult::BlessedLoss;

      case tablebase::TBResult::Draw:
        // We're drawing - opponent must not be winning
        return childWdl != tablebase::TBResult::Win && childWdl != tablebase::TBResult::CursedWin;

      case tablebase::TBResult::BlessedLoss: /* fallthrough */
      case tablebase::TBResult::Loss:        /* fallthrough */
      default:
        // We're losing - keep all moves (try to find the best losing move)
        return true;
    }
  };

  // Filter in-place by compacting kept moves to the front
  size_t writeIdx = 0;
  for (size_t readIdx = 0; readIdx < thread().rootMoves.size(); ++readIdx) {
    if (shouldKeepMove(thread().rootMoves[readIdx])) {
      if (writeIdx != readIdx) {
        thread().rootMoves[writeIdx] = thread().rootMoves[readIdx];
      }
      ++writeIdx;
    }
  }

  // Truncate the list to the number of kept moves
  thread().rootMoves.resize(writeIdx);

  const size_t filteredCount = thread().rootMoves.size();

  if (filteredCount < originalCount) {
    LOG__INFO(Logger::get().SEARCH_LOG, "TB filter: {} -> {} root moves (removed {} suboptimal)",
              originalCount, filteredCount, originalCount - filteredCount);
  }

  // Safety check: if all moves were filtered (shouldn't happen), restore TB move
  if (thread().rootMoves.empty() && tbRoot.move != MOVE_NONE) {
    LOG__WARN(Logger::get().SEARCH_LOG, "TB filter removed all moves! Restoring TB move {}", tbRoot.move.str());
    thread().rootMoves.push_back(tbRoot.move);
  }
}


Value Search::getTBScoreForSearch(const tablebase::TBResult wdl, const int halfMoveClock, const Depth ply) const {
  // Fast path: far from 50-move limit, no special handling needed
  if (halfMoveClock < SearchConfig.TB_RULE50_THRESHOLD) {
    return tablebase::Tablebase::tbValueToScore(wdl, ply);
  }

  // Slow path: near 50-move limit, need to be careful about unreachable wins/losses
  // This is only triggered when halfMoveClock >= TB_RULE50_THRESHOLD (default 80)
  // Setting TB_RULE50_THRESHOLD >= 100 effectively disables this path

  switch (wdl) {
    case tablebase::TBResult::Win:
    case tablebase::TBResult::Loss:
      // Near the 50-move limit with a decisive result
      // We don't have DTZ in search (only WDL), so be conservative:
      // - Very close to 50-move (>=90): treat as draw to avoid claiming unreachable wins
      // - Moderately close (threshold to 90): use WDL but it might be inaccurate
      if (halfMoveClock >= 90) {
        // Very close to 50-move rule - be conservative, treat as draw
        // This prevents claiming wins that can't be achieved before the rule kicks in
        return VALUE_DRAW;
      }
      // Between threshold and 90: use WDL score but acknowledge potential inaccuracy
      return tablebase::Tablebase::tbValueToScore(wdl, ply);

    case tablebase::TBResult::CursedWin:
    case tablebase::TBResult::BlessedLoss:
      // These already account for 50-move rule complications
      // CursedWin: would be win but 50-move may draw - score as slight advantage
      // BlessedLoss: would be loss but 50-move may save - score as slight disadvantage
      return tablebase::Tablebase::tbValueToScore(wdl, ply);

    case tablebase::TBResult::Draw:
      return VALUE_DRAW;

    case tablebase::TBResult::Failed:
    default:
      return VALUE_NONE;
  }
}

bool Search::stopConditions() { // NOLINT(*-make-member-function-const)
  if (stopSearchFlag) return true;
  // Node limit is checked against main thread only (hot path - can't aggregate all threads)
  // Helper thread nodes are not counted toward the limit for performance reasons.
  if (searchLimits.nodes > 0 && thread().nodesVisited >= searchLimits.nodes) { stopSearchFlag = true; }
  return stopSearchFlag;
}

bool Search::checkDrawRepAnd50(const Position& p, const int numberOfRepetitions) {
  return p.checkRepetitions(numberOfRepetitions) || p.getHalfMoveClock() >= 100;
}

Value Search::drawScore(const Position& p) const {
  // When contempt is 0, return VALUE_DRAW (== 0) — no bias.
  // Positive contempt: root player gets +contempt at draw nodes (avoids draws).
  // The sign flips when the side to move is the opponent, so the opponent
  // sees −contempt (draws are attractive for them from our perspective).
  const int contempt = SearchConfig.CONTEMPT;
  if (contempt == 0) return VALUE_DRAW;
  return p.getNextPlayer() == rootColor ? Value{contempt} : Value{-contempt};
}

void Search::setupSearchLimits(const Position& p, SearchLimits& sl) {
  if (sl.infinite) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Infinite");
  }
  if (sl.ponder) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Ponder");
  }
  if (sl.mate > 0) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Mate in {}", sl.mate);
  }
  if (sl.timeControl) {
    timeLimit   = setupTimeControl(p, sl);
    extraTimeMs = 0;

    if (sl.moveTime.count()) {
      LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Time Controlled: Time per Move {}", str(sl.moveTime));
    }
    else {
      LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Time Controlled: White = {} (inc {}) Black = {} (inc {}) Moves to go: {}",
                str(sl.whiteTime), str(sl.whiteInc), str(sl.blackTime), str(sl.blackInc), sl.movesToGo);
      LOG__INFO(Logger::get().SEARCH_LOG, "Time limit for move: {}", str(timeLimit));
    }
    if (sl.ponder) {
      LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Ponder - time control postponed until ponderhit received");
    }
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: No time control");
  }
  if (sl.depth) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Depth limited  : {}", sl.depth);
  }
  if (sl.nodes) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Nodes limited  : {}", sl.nodes);
  }
  if (!sl.moves.empty()) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Moves limited  : {}", sl.moves.str());
  }
}

bool Search::isTimeAlmostUp() const {
  if (!searchLimits.timeControl || searchLimits.ponder) return false;
  const auto budget  = timeLimit + milliseconds(extraTimeMs.load(std::memory_order_relaxed));
  const auto elapsed = currentTime() - startSearchTime;
  if (elapsed >= budget) return true;
  const auto remaining = std::chrono::duration_cast<milliseconds>(budget - elapsed);
  // Define a threshold: max(5ms, 2% of the original budget)
  constexpr milliseconds minBuffer{5};
  const milliseconds relBuffer{budget.count() > 0 ? milliseconds{budget.count() / 50} : milliseconds{0}}; // ~2%
  const milliseconds threshold = relBuffer > minBuffer ? relBuffer : minBuffer;
  return remaining <= threshold;
}

milliseconds Search::setupTimeControl(const Position& p, const SearchLimits& limits) const {
  // Search mode time per move
  if (limits.moveTime.count()) {
    // we need a little room for executing the code
    const milliseconds duration = limits.moveTime - milliseconds{SearchConfig.MOVE_OVERHEAD_MS};
    // if the duration is now negative, return the original value and issue a warning
    if (duration.count() < 0) {
      LOG__WARN(Logger::get().SEARCH_LOG, "Very short move time: {} ms", limits.moveTime.count());
      return limits.moveTime;
    }
    // In fixed movetime mode do not scale by complexity; just use the adjusted duration.
    return duration;
  }

  // Search mode is remaining time - estimated time per move

  // Estimate moves left
  // Improved moves-left model using phase/material buckets and repetition risk.
  int movesLeft = limits.movesToGo;
  if (!movesLeft) {
    // Derive game phase and material features
    const double phase = p.getGamePhaseFactor(); // ~1.0 opening/mid, ~0.0 endgame

    // Count non-pawn pieces across both sides (KNIGHT/BISHOP/ROOK/QUEEN)
    auto countPieces = [&](const PieceType pt) -> int {
      return p.getPieceBb(WHITE, pt).popcount() + p.getPieceBb(BLACK, pt).popcount();
    };
    const int knights = countPieces(KNIGHT);
    const int bishops = countPieces(BISHOP);
    const int rooks   = countPieces(ROOK);
    const int queensW = p.getPieceBb(WHITE, QUEEN).popcount();
    const int queensB = p.getPieceBb(BLACK, QUEEN).popcount();
    const int queens  = queensW + queensB;
    const int npp     = knights + bishops + rooks + queens; // non-pawn piece count (kings excluded)

    // Select a base bucket
    int base = 0;
    if (npp <= SearchConfig.NPP_LIGHT_THRESHOLD) {
      base = SearchConfig.MOVES_LEFT_LOW_MAT; // very low material
    }
    else if (queens == 0) {
      // Queenless middlegames/endgames tend to resolve faster
      base = npp <= SearchConfig.NPP_LIGHT_THRESHOLD + 2
               ? SearchConfig.MOVES_LEFT_LOW_MAT
               : SearchConfig.MOVES_LEFT_QUEENLESS;
    }
    else if (phase >= 0.66 || npp >= SearchConfig.NPP_HEAVY_THRESHOLD) {
      base = SearchConfig.MOVES_LEFT_OPENING;
    }
    else if (phase <= 0.33) {
      base = SearchConfig.MOVES_LEFT_ENDGAME;
    }
    else {
      base = SearchConfig.MOVES_LEFT_MIDGAME;
    }

    // Adjust for repetition/50-move risk
    if (p.getHalfMoveClock() >= SearchConfig.REPETITION_HMC_HIGH) {
      base -= SearchConfig.REPETITION_RISK_PENALTY;
    }

    // Clamp
    base = std::clamp(base, SearchConfig.MOVES_LEFT_MIN_CLAMP, SearchConfig.MOVES_LEFT_MAX_CLAMP);

    movesLeft = base;
    LOG__DEBUG(Logger::get().SEARCH_LOG,
               "TimeCtl: Estimated movesLeft={} (phase {:.2f}, npp {}, queens {}), hmc {}",
               movesLeft, phase, npp, queens, p.getHalfMoveClock());
  } // if (!movesLeft)

  // Estimate time left for current player
  milliseconds timeLeft;
  if (p.getNextPlayer()) { timeLeft = limits.blackTime + (movesLeft * limits.blackInc); }
  else { timeLeft = limits.whiteTime + (movesLeft * limits.whiteInc); }
  // estimate time per move
  const auto tl = static_cast<milliseconds>(timeLeft.count() / movesLeft);
  // tiny fixed reserve to reduce micro overshoots (remaining-time mode only)
  const milliseconds reserve{SearchConfig.MOVE_OVERHEAD_MS};
  // account for the runtime of our code
  milliseconds base;
  if (tl.count() < 100) {
    // limits for a very short available time reduced by another 20%
    base = tl - tl / 5; // ~80% without floating-point (avoids narrowing)
  }
  else {
    // reduced by 10%
    base = tl - tl / 10; // ~90% without floating-point
  }
  // apply reserve
  base = base > reserve ? base - reserve : base;

  // Complexity-aware weighting
  const double factor = computeComplexityFactorQuick(p);
  const auto weighted = milliseconds{
    (std::llround(static_cast<long double>(base.count()) * static_cast<long double>(factor)))};
  LOG__DEBUG(Logger::get().SEARCH_LOG, "TimeCtl: Estimated time left: base: {:L} ms factor: {:L} ms weighted: {:L}",
             base.count(), factor, weighted.count());
  return weighted;
}

void Search::addExtraTime(const double f) {
  if (searchLimits.timeControl && !searchLimits.moveTime.count()) {
    const auto deltaMs      = static_cast<int64_t>(std::llround(static_cast<long double>(timeLimit.count()) * (static_cast<long double>(f) - 1.0L)));
    const auto currentExtra = extraTimeMs.load(std::memory_order_relaxed);
    const auto maxExtraMs   = static_cast<int64_t>(std::llround(static_cast<long double>(timeLimit.count()) * SearchConfig.MAX_EXTRA_TIME_FACTOR));

    // Hard cap: total budget (timeLimit + extra) must not exceed remaining clock time
    // minus a safety margin (post-stop overhead). This prevents overshooting in fast games
    // where MAX_EXTRA_TIME_FACTOR * base could exceed the actual remaining time.
    const auto playerTimeMs = (position.getNextPlayer()
                                 ? searchLimits.blackTime
                                 : searchLimits.whiteTime)
                                .count();
    const auto clockCapMs = std::max(int64_t{0}, playerTimeMs - measuredPostStopOverheadMs - timeLimit.count());

    // Use the tighter of the two caps
    const auto effectiveCapMs = std::min(maxExtraMs, clockCapMs);

    // Cap extra time to effective cap
    if (currentExtra >= effectiveCapMs) {
      LOG__DEBUG(Logger::get().SEARCH_LOG, "Time adjustment: {} ignored - already at max extra time {} (cap {}, clock cap {})",
                 str(milliseconds(deltaMs)),
                 str(milliseconds(currentExtra)),
                 str(milliseconds(maxExtraMs)),
                 str(milliseconds(clockCapMs)));
      return; // Already at cap
    }

    // Apply delta but don't exceed cap
    auto newExtra = currentExtra + deltaMs;
    if (newExtra > effectiveCapMs) {
      newExtra = effectiveCapMs;
    }
    extraTimeMs.store(newExtra, std::memory_order_relaxed);

    LOG__DEBUG(Logger::get().SEARCH_LOG, "Time adjustment: {} -> total budget {} (base {} + extra {}, cap {}, clock cap {})",
               str(milliseconds(deltaMs)),
               str(timeLimit + milliseconds(newExtra)),
               str(timeLimit),
               str(milliseconds(newExtra)),
               str(milliseconds(maxExtraMs)),
               str(milliseconds(clockCapMs)));
  }
}

void Search::startTimer() {
  std::lock_guard lock(timerMutex);
  // Don't start another timer if one is already running
  if (timerThread.joinable()) {
    return;
  }

  // Start a new timer thread that will set stopSearchFlag when time is up
  timerThread = std::thread([this] {
    // Note: startSearchTime is set in startSearch() before the search thread starts.
    // Do NOT reset it here - that would race with early-exit paths (e.g., book moves).
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Timer started with time limit of {} ms (post-stop overhead reserve: {} ms)", str(timeLimit), measuredPostStopOverheadMs);
    // Busy-wait threshold for higher-precision tail (2-3ms)
    constexpr milliseconds busyWaitThreshold{3};
    while (true) {
      const auto now     = currentTime();
      const auto elapsed = now - startSearchTime;
      // Subtract measured post-stop overhead so the timer fires early enough
      // for post-stop work (join helpers, select best, send bestmove) to complete
      // within the original time budget.
      const auto overhead  = milliseconds(measuredPostStopOverheadMs);
      const auto rawBudget = timeLimit + milliseconds(extraTimeMs.load());
      const auto budget    = rawBudget > overhead ? rawBudget - overhead : milliseconds{0};
      if (elapsed >= budget || stopSearchFlag) { break; }
      const auto remaining_ms = std::chrono::duration_cast<milliseconds>(budget - elapsed);
      if (remaining_ms > busyWaitThreshold) {
        // Sleep for a fraction of the remaining time (1-5ms)
        milliseconds sleepFor = remaining_ms / 5;
        if (sleepFor < milliseconds(1)) sleepFor = milliseconds(1);
        if (sleepFor > milliseconds(5)) sleepFor = milliseconds(5);
        std::this_thread::sleep_for(sleepFor);
      }
      else {
        // Busy-wait for the final few ms
        while (currentTime() - startSearchTime < budget && !stopSearchFlag) {
          // tight loop, no sleep
          std::this_thread::yield();
        }
        break;
      }
    }
    if (!this->stopSearchFlag) {
      this->timerStopTime  = currentTime();
      this->stoppedByTimer = true;
      this->stopSearchFlag = true;
      this->stopConditionVar.notify_all();
      LOG__INFO(Logger::get().SEARCH_LOG, "Stop search by Timer after wall time: {} (time limit {} and extra time {})", str(currentTime() - startSearchTime), str(timeLimit), str(milliseconds(extraTimeMs.load())));
    }
  });
}

void Search::joinTimerThread() {
  std::lock_guard lock(timerMutex);
  if (timerThread.joinable()) { timerThread.join(); }
}

double Search::computeComplexityFactorFromMoves(const Position& p, const MoveList& legalMoves) {
  // Defaults chosen conservatively to avoid large swings.
  constexpr int pivotMoves = 30;   // neutral pivot
  constexpr double slope   = 0.01; // +/-1% per move relative to pivot
  constexpr double baseCap = 0.25; // baseline capture ratio
  constexpr double capW    = 0.50; // weight for (ratio - baseCap)
  constexpr double inChkB  = 0.10; // +10% when in check
  constexpr double minF    = 0.85; // min factor
  constexpr double maxF    = 1.30; // max factor

  const int total = static_cast<int>(legalMoves.size());

  // This should not happen, as this is checked before calling this function.
  // But just in case, return a neutral factor.
  if (total <= 0) return 1.0;

  // Special-case: if there is only one legal move, scale time way down
  if (total == 1) return 0.1;

  int captures = 0;
  for (const Move& m : legalMoves) {
    if (p.isCapturingMove(m)) ++captures;
  }
  const double ratio = static_cast<double>(captures) / static_cast<double>(total);

  double f = 1.0;
  // Move-count component around pivot
  f += slope * static_cast<double>(total - pivotMoves);
  // Captures component relative to baseline
  f += capW * (ratio - baseCap);
  // In-check bonus
  if (p.hasCheck()) f += inChkB;

  return std::clamp(f, minF, maxF);
}

double Search::computeComplexityFactorQuick(const Position& p) {
  // Generate once for root quickly; we only need counts
  MoveGenerator mg;
  const MoveList legal = *mg.generateLegalMoves(p, GenAll);
  return computeComplexityFactorFromMoves(p, legal);
}

void Search::sendReadyOk() const {
  if (uciHandler) {
    uciHandler->sendReadyOk();
    return;
  }
  LOG__INFO(Logger::get().SEARCH_LOG, "uci >> readyok");
}

void Search::sendString(const std::string& msg) const {
  if (uciHandler) {
    uciHandler->sendString(msg);
    return;
  }
  LOG__INFO(Logger::get().SEARCH_LOG, "uci >> {}", msg);
}

void Search::sendDebugEvalInfo() const {
  // Evaluate the position at the end of the PV (after applying all PV moves).
  // This is far more informative than the root position, which is the same every
  // iteration and often symmetric (e.g., startpos → all components are zero).
  Position pvPos        = position;
  const MoveList pvLine = thread().pv.extract();
  for (const auto& move : pvLine) {
    if (!move) break;
    pvPos.doMove(move);
  }
  const EvalTrace trace = thread().evaluator.evaluateTrace(pvPos);
  sendString(std::format("pv-leaf {} | {}", pvLine.str(), trace.str()));

  // Iteration stats: TT hit-rate and beta-cut-1st-move %
  const auto& stats       = thread().statistics;
  const uint64_t ttHits   = stats.ttHitSufficientDepth + stats.ttHitInsufficientDepth;
  const uint64_t ttTotal  = stats.ttProbes;
  const double ttHitRate  = ttTotal > 0
                              ? 100.0 * static_cast<double>(ttHits) / static_cast<double>(ttTotal)
                              : 0.0;
  const double betaCut1st = stats.betaCuts > 0
                              ? 100.0 * static_cast<double>(stats.betaCutsByIndex[0]) / static_cast<double>(stats.betaCuts)
                              : 0.0;
  sendString(std::format("depth {} nodes {:L} tt-hitrate {:.1f}% beta-cut-1st {:.1f}%",
                         stats.currentSearchDepth,
                         thread().nodesVisited,
                         ttHitRate,
                         betaCut1st));
}

void Search::sendResult(const SearchResult& result) const {
  if (uciHandler) { uciHandler->sendResult(result.bestMove, result.ponderMove); }
}

void Search::sendIterationEndInfoToUci() {
  const nanoseconds& since = elapsedSince(startSearchTime);
  lastUciUpdateTime        = now();

  // Use a copy of the initial position to extract PV with TT extension
  Position p            = position;
  const MoveList pvLine = extractPvWithTT(p);

  // Aggregate node count from all threads for UCI reporting
  const uint64_t totalNodes = getTotalNodes();

  if (uciHandler) {
    uciHandler->sendIterationEndInfo(
      thread().statistics.currentSearchDepth,
      thread().statistics.currentExtraSearchDepth,
      thread().statistics.currentBestRootMoveValue,
      totalNodes,
      nps(totalNodes, since),
      MILLISECONDS(since),
      pvLine);
    return;
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "depth {} seldepth {} value {} nodes {:L} nps {:L} time {:L} pv {}",
            thread().statistics.currentSearchDepth,
            thread().statistics.currentExtraSearchDepth,
            thread().statistics.currentBestRootMoveValue.str(),
            totalNodes,
            nps(totalNodes, since),
            MILLISECONDS(since).count(),
            pvLine.str());
}

void Search::sendMultiPvResultsToUci(const std::vector<MultiPvResult>& results, const Depth iterationDepth) {
  const nanoseconds& since = elapsedSince(startSearchTime);
  lastUciUpdateTime        = now();

  // Snapshot node count once — all PV lines share the same value (Stockfish-style).
  const uint64_t totalNodes  = getTotalNodes();
  const uint64_t nodesPerSec = nps(totalNodes, since);
  const auto elapsed         = MILLISECONDS(since);

  for (int i = 0; i < static_cast<int>(results.size()); ++i) {
    const auto& [pvLine, score, seldepth] = results[i];
    const int multipvIndex                = i + 1; // UCI multipv is 1-based

    if (uciHandler) {
      uciHandler->sendIterationEndInfo(
        iterationDepth,
        seldepth,
        score,
        totalNodes,
        nodesPerSec,
        elapsed,
        pvLine,
        multipvIndex);
    }
    else {
      LOG__INFO(Logger::get().SEARCH_LOG, "depth {} seldepth {} multipv {} value {} nodes {:L} nps {:L} time {:L} pv {}",
                iterationDepth,
                seldepth,
                multipvIndex,
                score.str(),
                totalNodes,
                nodesPerSec,
                elapsed.count(),
                pvLine.str());
    }
  }
}

void Search::sendSearchUpdateToUci() {

  // Only main thread sends UCI updates - helpers contribute via TT only
  if (thread().id != 0) { return; }

  // to minimize performance impact we only check time every 1M nodes
  // Note: uses main thread nodes only to avoid aggregation overhead on hot path
  if (thread().nodesVisited - lastUciUpdateNodes < 1'000'000) { return; }
  lastUciUpdateNodes = thread().nodesVisited;

  // we only update every UCI_UPDATE_INTERVAL ns
  const uint64_t nowTime = now();
  if (nowTime - lastUciUpdateTime < UCI_UPDATE_INTERVAL) { return; }
  lastUciUpdateTime = nowTime;

  // Aggregate node count from all threads for UCI reporting
  const uint64_t totalNodes = getTotalNodes();

  // nps is calculated from the total nodes and time since last update.
  // This might not be the same as the over all avg. nps which is shown
  // at the end of a search.
  const uint64_t nodesPerSec = nps(totalNodes - npsNodes, nowTime - npsTime);
  npsTime                    = nowTime;
  npsNodes                   = totalNodes;

  const int hashfull = tt->hashFull();

  const nanoseconds& since = elapsedSince(startSearchTime);

  if (uciHandler) {
    uciHandler->sendSearchUpdate(
      thread().statistics.currentSearchDepth,
      thread().statistics.currentExtraSearchDepth,
      totalNodes,
      nodesPerSec,
      MILLISECONDS(since),
      hashfull);
    uciHandler->sendCurrentRootMove(thread().statistics.currentRootMove, thread().statistics.currentRootMoveIndex);
    uciHandler->sendCurrentLine(thread().statistics.currentVariation);
    return;
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "depth {} seldepth {} nodes {:L} nps {:L} time {:L} hashful {:L}",
            thread().statistics.currentSearchDepth,
            thread().statistics.currentExtraSearchDepth,
            totalNodes,
            nodesPerSec,
            MILLISECONDS(since).count(),
            hashfull);
}

void Search::sendAspirationResearchInfo(const std::string& boundString) const {
  const nanoseconds& since = elapsedSince(startSearchTime);

  // Time gate for UCI output — suppress aspiration bound info during the first
  // 3 seconds of search. This avoids flooding the GUI with
  // intermediate lowerbound/upperbound scores that resolve in milliseconds
  // (e.g., TB win scores causing visible score swings in Arena/CuteChess).
  // Aspiration re-searches at shallow depths are transient noise; only report
  // them when the search is taking long enough that the user wants feedback.
  // Note: LOG output is always emitted regardless (for debugging/analysis).
  static constexpr auto ASP_UCI_INFO_DELAY = milliseconds{3000};

  // Use a copy of the initial position to extract PV with TT extension
  Position p            = position;
  const MoveList pvLine = extractPvWithTT(p);

  // Aggregate node count from all threads for UCI reporting
  const uint64_t totalNodes = getTotalNodes();

  if (uciHandler) {
    // Suppress aspiration bound UCI output when MultiPV > 1. Sending a single
    // "multipv 1" lowerbound/upperbound line while the GUI still displays the
    // previous depth's complete multi-PV set causes the GUI to show a stale
    // multipv 2..N alongside the updated multipv 1, creating a non-monotonic display.
    // LOG output is still emitted for debugging.
    if (since >= ASP_UCI_INFO_DELAY && SearchConfig.MULTI_PV <= 1) {
      uciHandler->sendAspirationResearchInfo(
        thread().statistics.currentSearchDepth,
        thread().statistics.currentExtraSearchDepth,
        thread().statistics.currentBestRootMoveValue,
        boundString,
        totalNodes,
        nps(totalNodes, since),
        MILLISECONDS(since),
        pvLine);
    }
    // Always log for debugging, even when UCI output is suppressed
    LOG__DEBUG(Logger::get().SEARCH_LOG, "depth {} seldepth {} value {} {} nodes {:L} nps {:L} time {:L} pv {}",
               thread().statistics.currentSearchDepth,
               thread().statistics.currentExtraSearchDepth,
               thread().statistics.currentBestRootMoveValue.str(),
               boundString,
               totalNodes,
               nps(totalNodes, since),
               MILLISECONDS(since).count(),
               pvLine.str());
    return;
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "depth {} seldepth {} value {} {} nodes {:L} nps {:L} time {:L} pv {}",
            thread().statistics.currentSearchDepth,
            thread().statistics.currentExtraSearchDepth,
            thread().statistics.currentBestRootMoveValue.str(),
            boundString,
            totalNodes,
            nps(totalNodes, since),
            MILLISECONDS(since).count(),
            pvLine.str());
}

MoveList Search::extractPvWithTT(Position& p) const {
  MoveList result;

  // First, copy moves from the triangular PV table
  // Validate each move - PV table can contain stale data from previous
  // search branches or iterations that are illegal in the current position
  const int pvLen  = thread().pv.length();
  int validPvMoves = 0;
  for (int i = 0; i < pvLen; ++i) {
    const Move move = thread().pv(DEPTH_NONE, i);
    if (move == MOVE_NONE) break;

    // Verify the move is fully legal in current position
    // validateMove() generates all legal moves and checks if this move is among them
    // This catches stale moves where the piece no longer exists on the from-square
    if (!thread().pvMoveGenerator.validateMove(p, move)) break;

    result.push_back(move);
    p.doMove(move);
    ++validPvMoves;
  }

  // Now extend using TT lookups
  // Limit to prevent infinite loops (e.g., from TT collisions)
  constexpr int maxExtension = MAX_DEPTH;
  int extended               = 0;

  while (extended < maxExtension) {
    const auto ttEntry = tt->probe(p.getZobristKey());
    if (!ttEntry) break;

    const auto ttMove = static_cast<Move>(ttEntry->move);
    if (ttMove == MOVE_NONE) break;

    // Verify the move is fully legal in current position
    // validateMove() generates all legal moves and checks if this move is among them
    // This catches TT hash collisions where the stored move is invalid for this position
    if (!thread().pvMoveGenerator.validateMove(p, ttMove)) break;

    // Check for repetition - don't extend into a repeated position
    // Use 2 to check for threefold repetition (same as search uses)
    p.doMove(ttMove);
    if (p.checkRepetitions(2)) {
      p.undoMove();
      break;
    }

    result.push_back(ttMove);
    ++extended;
  }

  // Undo all moves to restore position
  const int totalMoves = validPvMoves + extended;
  for (int i = 0; i < totalMoves; ++i) {
    p.undoMove();
  }

  return result;
}

std::string Search::formatDetailedStats(
  const SearchResult& result,
  const SearchStats& stats) const {

  std::ostringstream os;
  os.imbue(projectLocale);

  const auto timeMs  = duration_cast<milliseconds>(result.time).count();
  const uint64_t nps = timeMs > 0 ? (result.nodes * 1000) / static_cast<uint64_t>(timeMs) : 0;

  // Calculate Effective Branching Factor: EBF = nodes^(1/depth)
  // This measures search efficiency - lower is better (perfect ordering = 1.0)
  const double ebf = (result.depth > 0 && result.nodes > 0)
                       ? std::pow(static_cast<double>(result.nodes), 1.0 / static_cast<double>(result.depth))
                       : 0.0;

  os << "\n==================== Search Results ====================\n";
  if (!result.fen.empty()) {
    os << "Position       : " << result.fen << "\n";
  }
  os << "Best Move      : " << result.bestMove.str() << "\n";
  os << "Score          : " << result.bestMoveValue.str() << "\n";
  os << "Ponder Move    : " << result.ponderMove.str() << "\n";
  os << "Depth          : " << result.depth << "/" << result.extraDepth << " (regular/selective)\n";
  os << "Time           : " << timeMs << " ms\n";
  os << "Nodes          : " << result.nodes << "\n";
  os << "NPS            : " << nps << "\n";
  os << "EBF            : " << std::fixed << std::setprecision(2) << ebf << "\n";
  os << "Book Move      : " << (result.bookMove ? "yes" : "no") << "\n";
  os << "TB Hit         : " << (result.tbHit ? "yes" : "no") << "\n";
  os << "Mate Found     : " << (result.mateFound ? "yes" : "no") << "\n";
  os << "PV             : " << result.pv.str() << "\n";

  os << "\n------------------- Terminal Nodes --------------------\n";
  os << "Checkmates     : " << stats.checkmates << "\n";
  os << "Stalemates     : " << stats.stalemates << "\n";
  os << "Leaf Positions : " << stats.leafPositionsEvaluated << "\n";
  os << "Evaluations    : " << stats.evaluations << "\n";
  os << "Perft Nodes    : " << stats.perftNodeCount << "\n";

  os << "\n------------------- Node Type Stats -------------------\n";
  const uint64_t totalNodes = stats.pvNodes + stats.nonPvNodes;
  os << "PV Nodes       : " << stats.pvNodes;
  if (totalNodes > 0) {
    const double pvPct = 100.0 * static_cast<double>(stats.pvNodes) / static_cast<double>(totalNodes);
    os << " (" << std::fixed << std::setprecision(2) << pvPct << "%)";
  }
  os << "\n";
  os << "Non-PV Nodes   : " << stats.nonPvNodes;
  if (totalNodes > 0) {
    const double nonPvPct = 100.0 * static_cast<double>(stats.nonPvNodes) / static_cast<double>(totalNodes);
    os << " (" << std::fixed << std::setprecision(2) << nonPvPct << "%)";
  }
  os << "\n";
  os << "Search Nodes   : " << stats.searchNodes;
  if (totalNodes > 0) {
    const double searchPct = 100.0 * static_cast<double>(stats.searchNodes) / static_cast<double>(totalNodes);
    os << " (" << std::fixed << std::setprecision(2) << searchPct << "%)";
  }
  os << "\n";
  os << "QSearch Nodes  : " << stats.qsearchNodes;
  if (totalNodes > 0) {
    const double qsPct = 100.0 * static_cast<double>(stats.qsearchNodes) / static_cast<double>(totalNodes);
    os << " (" << std::fixed << std::setprecision(2) << qsPct << "%)";
  }
  os << "\n";

  os << "\n------------------- Pruning Stats ---------------------\n";
  os << "Beta Cuts      : " << stats.betaCuts << "\n";
  os << "MDP Cuts       : " << stats.mdp << "\n";
  os << "Razorings      : " << stats.razorings << "\n";
  os << "RFP Cuts       : " << stats.rfp_cuts << "\n";
  os << "NMP Cuts       : " << stats.nullMoveCuts << "\n";
  os << "NMP Verifies   : " << stats.nullMoveVerifications << "\n";
  os << "FP Prunings    : " << stats.fpPrunings << "\n";
  os << "QFP Prunings   : " << stats.qfpPrunings << "\n";
  os << "Standpat Cuts  : " << stats.standpatCuts << "\n";

  os << "\n------------------- LMR/LMP Stats ---------------------\n";
  os << "LMR Reductions : " << stats.lmrReductions << "\n";
  os << "LMR Researches : " << stats.lmrResearches << "\n";
  os << "LMR CutNode    : " << stats.lmrCutNodeReductions;
  if (stats.lmrReductions > 0) {
    const double pct = 100.0 * static_cast<double>(stats.lmrCutNodeReductions) / static_cast<double>(stats.lmrReductions);
    os << " (" << std::fixed << std::setprecision(1) << pct << "% of LMR)";
  }
  os << "\n";
  os << "LMR Hist Less  : " << stats.lmrHistoryLessReduction;
  if (stats.lmrReductions > 0) {
    const double pct = 100.0 * static_cast<double>(stats.lmrHistoryLessReduction) / static_cast<double>(stats.lmrReductions);
    os << " (" << std::fixed << std::setprecision(1) << pct << "% of LMR)";
  }
  os << "\n";
  os << "LMR Hist Saved : " << stats.lmrHistoryDepthSaved << " plies";
  if (stats.lmrHistoryLessReduction > 0) {
    const double avg = static_cast<double>(stats.lmrHistoryDepthSaved) / static_cast<double>(stats.lmrHistoryLessReduction);
    os << " (avg " << std::fixed << std::setprecision(2) << avg << " per move)";
  }
  os << "\n";
  os << "LMP Cuts       : " << stats.lmpCuts << "\n";

  os << "\n------------------- Improving Stats -------------------\n";
  os << "Improving True : " << stats.improvingTrue;
  {
    const uint64_t improvingTotal = stats.improvingTrue + stats.improvingFalse;
    if (improvingTotal > 0) {
      const double pct = 100.0 * static_cast<double>(stats.improvingTrue) / static_cast<double>(improvingTotal);
      os << " (" << std::fixed << std::setprecision(1) << pct << "%)";
    }
  }
  os << "\n";
  os << "Improving False: " << stats.improvingFalse << "\n";

  os << "\n------------------- Extension Stats -------------------\n";
  os << "Check Ext      : " << stats.checkExtension << "\n";
  os << "Threat Ext     : " << stats.threatExtension << "\n";
  os << "Singular Srch  : " << stats.singularSearches << "\n";
  os << "Singular Ext   : " << stats.singularExtension << "\n";

  os << "\n------------------- TT Stats --------------------------\n";
  os << "TT Size (MB)   : " << tt->getSizeInByte() / (1024 * 1024) << "\n";
  os << "TT Max Entries : " << tt->getMaxNumberOfEntries() << "\n";
  os << "TT Entries     : " << tt->getNumberOfEntries() << "\n";
  {
    const double ttFillPct = tt->getMaxNumberOfEntries() > 0
                               ? 100.0 * static_cast<double>(tt->getNumberOfEntries()) / static_cast<double>(tt->getMaxNumberOfEntries())
                               : 0.0;
    os << "TT Fill        : " << std::fixed << std::setprecision(2) << ttFillPct << "%\n";

    // TT internal counters have data races in SMP - show them but mark as approximate
    os << "TT Puts        : " << tt->getNumberOfPuts() << " (approx)\n";
    os << "TT Updates     : " << tt->getNumberOfUpdates() << " (approx)\n";
    os << "TT Collisions  : " << tt->getNumberOfCollisions() << " (approx)\n";
    os << "TT Overwrites  : " << tt->getNumberOfOverwrites() << " (approx)\n";

    // Use SearchStats for accurate probe/hit/miss tracking (per-thread, aggregated)
    const uint64_t trackedHits   = stats.ttHitSufficientDepth + stats.ttHitInsufficientDepth;
    const uint64_t trackedProbes = stats.ttProbes;
    const uint64_t trackedMisses = stats.ttMisses;

    os << "--- Probe Stats (from SearchStats - accurate) ---\n";
    os << "TT Probes      : " << trackedProbes << "\n";
    if (trackedProbes > 0) {
      os << "TT Hits        : " << trackedHits << " (" << std::fixed << std::setprecision(1)
         << (100.0 * static_cast<double>(trackedHits) / static_cast<double>(trackedProbes)) << "%)\n";
      os << "TT Misses      : " << trackedMisses << " (" << std::fixed << std::setprecision(1)
         << (100.0 * static_cast<double>(trackedMisses) / static_cast<double>(trackedProbes)) << "%)\n";
    }
    else {
      os << "TT Hits        : " << trackedHits << "\n";
      os << "TT Misses      : " << trackedMisses << "\n";
    }

    os << "--- Hit Quality (Depth) ---\n";
    if (trackedHits > 0) {
      os << "Sufficient Dep : " << stats.ttHitSufficientDepth << " (" << std::fixed << std::setprecision(1)
         << (100.0 * static_cast<double>(stats.ttHitSufficientDepth) / static_cast<double>(trackedHits)) << "%)\n";
      os << "Insuffic. Dep  : " << stats.ttHitInsufficientDepth << " (" << std::fixed << std::setprecision(1)
         << (100.0 * static_cast<double>(stats.ttHitInsufficientDepth) / static_cast<double>(trackedHits)) << "%)\n";
    }
    else {
      os << "Sufficient Dep : 0\n";
      os << "Insuffic. Dep  : 0\n";
    }

    // Hit quality by bound type
    const uint64_t boundTracked = stats.ttHitNone + stats.ttHitExact + stats.ttHitAlpha + stats.ttHitBeta;
    os << "--- Hit Quality (Bound) ---\n";
    if (boundTracked > 0) {
      os << "NONE Hits      : " << stats.ttHitNone << " (" << std::fixed << std::setprecision(1)
         << (100.0 * static_cast<double>(stats.ttHitNone) / static_cast<double>(boundTracked)) << "%) [eval-only]\n";
      os << "EXACT Hits     : " << stats.ttHitExact << " (" << std::fixed << std::setprecision(1)
         << (100.0 * static_cast<double>(stats.ttHitExact) / static_cast<double>(boundTracked)) << "%)\n";
      os << "ALPHA Hits     : " << stats.ttHitAlpha << " (" << std::fixed << std::setprecision(1)
         << (100.0 * static_cast<double>(stats.ttHitAlpha) / static_cast<double>(boundTracked)) << "%)\n";
      os << "BETA Hits      : " << stats.ttHitBeta << " (" << std::fixed << std::setprecision(1)
         << (100.0 * static_cast<double>(stats.ttHitBeta) / static_cast<double>(boundTracked)) << "%)\n";
    }
    else {
      os << "NONE Hits      : 0\n";
      os << "EXACT Hits     : 0\n";
      os << "ALPHA Hits     : 0\n";
      os << "BETA Hits      : 0\n";
    }

    // TT effectiveness metrics
    os << "--- TT Effectiveness ---\n";
    os << "TT Cuts        : " << stats.TtCuts << "\n";
    if (stats.ttCutsSearch > 0 || stats.ttCutsQsearch > 0) {
      os << "  Search Cuts  : " << stats.ttCutsSearch;
      if (stats.ttCutsSearch > 0) {
        const double avgCutDepth = static_cast<double>(stats.ttCutDepthSum) / static_cast<double>(stats.ttCutsSearch);
        os << " (avg depth " << std::fixed << std::setprecision(1) << avgCutDepth << ")";
      }
      os << "\n";
      os << "  Qsearch Cuts : " << stats.ttCutsQsearch << "\n";
    }
    os << "TT No Cuts     : " << stats.TtNoCuts << "\n";
    os << "TT Move Used   : " << stats.TtMoveUsed;
    if (stats.TtMoveUsed > 0) {
      const double bestMoveRate = 100.0 * static_cast<double>(stats.ttMoveBestMove) / static_cast<double>(stats.TtMoveUsed);
      os << " (" << stats.ttMoveBestMove << " = " << std::fixed << std::setprecision(1) << bestMoveRate << "% best move)";
    }
    os << "\n";
    os << "No TT Move     : " << stats.NoTtMove << "\n";
    os << "Eval from TT   : " << stats.evalFromTT << "\n";

    // Value breakdown - note: these can overlap (a hit can provide cut AND move ordering)
    if (trackedHits > 0) {
      os << "--- Value Breakdown (% of hits, overlapping) ---\n";
      os << "  Cutoffs      : " << stats.TtCuts << " (" << std::fixed << std::setprecision(1)
         << (100.0 * static_cast<double>(stats.TtCuts) / static_cast<double>(trackedHits)) << "%)\n";
      os << "  Eval Reused  : " << stats.evalFromTT << " (" << std::fixed << std::setprecision(1)
         << (100.0 * static_cast<double>(stats.evalFromTT) / static_cast<double>(trackedHits)) << "%)\n";
      os << "  Move Ordering: " << stats.TtMoveUsed << " (" << std::fixed << std::setprecision(1)
         << (100.0 * static_cast<double>(stats.TtMoveUsed) / static_cast<double>(trackedHits)) << "%)\n";
    }
  }

  os << "\n----------------- PawnTT Stats ------------------------\n";
  {
    const uint64_t pttQueries = pawnTT->getNumberOfHits() + pawnTT->getNumberOfMisses();
    const uint64_t pttHitPct  = pttQueries ? (pawnTT->getNumberOfHits() * 100) / pttQueries : 0;
    const uint64_t pttMissPct = pttQueries ? (pawnTT->getNumberOfMisses() * 100) / pttQueries : 0;
    const double pttFillPct   = pawnTT->getMaxNumberOfEntries() > 0
                                  ? 100.0 * static_cast<double>(pawnTT->getNumberOfEntries()) / static_cast<double>(pawnTT->getMaxNumberOfEntries())
                                  : 0.0;
    os << "PTT Size (MB)  : " << pawnTT->getSizeInByte() / (1024 * 1024) << "\n";
    os << "PTT Max Entries: " << pawnTT->getMaxNumberOfEntries() << "\n";
    os << "PTT Entries    : " << pawnTT->getNumberOfEntries() << "\n";
    os << "PTT Fill       : " << std::fixed << std::setprecision(2) << pttFillPct << "%\n";
    os << "PTT Puts       : " << pawnTT->getNumberOfPuts() << "\n";
    os << "PTT Updates    : " << pawnTT->getNumberOfUpdates() << "\n";
    os << "PTT Collisions : " << pawnTT->getNumberOfCollisions() << "\n";
    os << "PTT Hits       : " << pawnTT->getNumberOfHits() << " (" << pttHitPct << "%)\n";
    os << "PTT Misses     : " << pawnTT->getNumberOfMisses() << " (" << pttMissPct << "%)\n";
  }

  os << "\n------------------- IIR Stats -------------------------\n";
  if (ConfigManager::instance().search().USE_IIR) {
    os << "IIR Reductions : " << stats.iirReductions << "\n";
  }

  os << "\n------------------- Re-search Stats -------------------\n";
  os << "Root PVS Re    : " << stats.rootPvsResearches << "\n";
  os << "PVS Researches : " << stats.pvsResearches << "\n";
  os << "ASP Researches : " << stats.aspirationResearches << "\n";
  os << "Best Move Chg  : " << stats.bestMoveChange << "\n";

  os << "\n------------------- Tablebase Stats -------------------\n";
  os << "TB Root Hits   : " << stats.tbRootHits << "\n";
  os << "TB Search Prbs : " << stats.tbSearchProbes << "\n";
  os << "TB Search Hits : " << stats.tbSearchHits << "\n";
  os << "TB Search Miss : " << stats.tbSearchMisses << "\n";
  os << "TB Cutoffs     : " << stats.tbSearchCutoffs << "\n";

  // Beta cuts distribution (move ordering quality)
  if (stats.betaCuts > 0) {
    os << "\n------------------- Beta Cuts Distribution ------------\n";
    os << "(Shows which move index caused cutoff - lower index = better ordering)\n";
    for (int i = 0; i < SearchStats::BETA_CUTS_INDEX_SIZE; ++i) {
      const double pct = 100.0 * static_cast<double>(stats.betaCutsByIndex[i]) / static_cast<double>(stats.betaCuts);
      os << "  Move " << std::setw(2) << i << (i == 9 ? "+" : " ")
         << "   : " << std::fixed << std::setprecision(2) << std::setw(6) << pct << "% ("
         << stats.betaCutsByIndex[i] << ")\n";
    }
  }
  os << "========================================================\n";

  return os.str();
}

std::string Search::formatDetailedStats() const {
  if (!lastSearchResult.has_value()) {
    return "\n==================== No Search Result Available ====================\n";
  }
  // Aggregate stats from all threads (main + helpers) for SMP
  const SearchStats aggregated = aggregateStats();
  return formatDetailedStats(*lastSearchResult, aggregated);
}

SearchStats Search::aggregateStats() const {
  // Start with main thread's stats
  SearchStats result = mainThread().statistics;

  // Add stats from active helper threads only [1..numHelperThreads].
  // The vector grows but never shrinks, so entries beyond the active
  // thread count could contain stale statistics from previous searches.
  const int activeThreads = numHelperThreads + 1;
  const auto count        = std::min(static_cast<int>(searchThreadData.size()), activeThreads);
  for (int i = 1; i < count; ++i) {
    if (searchThreadData[i]) {
      result += searchThreadData[i]->statistics;
    }
  }

  return result;
}
