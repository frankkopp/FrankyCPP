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
#include "See.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

////////////////////////////////////////////////
///// CONSTRUCTORS

Search::Search() : Search(nullptr) {}

Search::Search(UciHandler* pUciHandler)
    : uciHandler(pUciHandler), SearchConfig(ConfigManager::instance().search()) {
  this->tt = std::make_unique<TT>(0);
}

Search::~Search() {
  // necessary to avoid an error message:
  // terminate called without an active exception
  if (searchThread.joinable()) { searchThread.join(); }
}

////////////////////////////////////////////////
///// PUBLIC

void Search::newGame() {
  if (isSearching()) stopSearch();

  for (auto& plyInfo : plyStack) {
    plyInfo.resetSearchState();
  }
  rootMoves.clear();
  tt->clear();
  if (evaluator) { evaluator->reset(); }
  bestMoveStability.reset();
  history.reset();

  tbRootMove  = MOVE_NONE;
  tbRootValue = VALUE_NONE;
  tbRootWdl   = tablebase::TBResult::Failed;
  tbRootDtz   = 0;
  hadBookMove = false;
}

void Search::isReady() {
  initialize();
  sendReadyOk();
}

void Search::startSearch(const Position& p, SearchLimits sl) {
  // acquire init phase lock
  if (!initSemaphore.try_acquire()) {
    LOG__WARN(Logger::get().SEARCH_LOG, "Search init failed as another initialization is ongoing.");
  }

  // DEBUG to test the new config approach
  LOG__INFO(Logger::get().SEARCH_LOG, "DEBUG: CONFIG_SOURCE: {}", SearchConfig.CONFIG_SOURCE);

  // start search time
  startTime       = currentTime();
  startSearchTime = startTime;

  // move the received copy of position and search limits to instance variables
  this->position = p;
  searchLimits   = sl;

  // join() previous thread
  if (searchThread.joinable()) { searchThread.join(); }

  // start search in a separate thread
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
  // Wait for the thread to die
  if (searchThread.joinable()) { searchThread.join(); }
  waitWhileSearching();
}

bool Search::isSearching() const {// NOLINT(*-convert-member-functions-to-static)
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
    const std::string msg = "Can't clear hash while searching.";
    sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
    return;
  }
  tt->clear();
  const std::string msg = "Hash cleared.";
  sendString(msg);
  LOG__INFO(Logger::get().SEARCH_LOG, "{}", msg);
}

void Search::resizeTT() const {
  if (isSearching()) {
    const std::string msg = "Can't resize hash while searching.";
    sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
    return;
  }
  // Resize the existing TT to the configured size and clear it
  tt->resize(SearchConfig.TT_SIZE_MB);
  sendString("Resized hash: " + tt->str());
}

////////////////////////////////////////////////
///// PRIVATE

void Search::run() {
  // check if there is already a search running
  // and if not, grab the isRunning semaphore
  if (!isRunningSemaphore.try_acquire()) {
    LOG__ERROR(Logger::get().SEARCH_LOG, "Search is already running");
    return;
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "Searching {}", position.strFen());

  // initialize search
  stopSearchFlag = false;
  lastSearchResult.reset();// clear previous result
  timeLimit         = milliseconds{};
  extraTimeMs       = 0;
  nodesVisited      = 0;
  statistics        = SearchStats{};
  lastUciUpdateTime = nowFast();
  // Note: npsTime and npsNodes are initialized later, right before iterative deepening,
  // to avoid including initialization overhead in NPS calculations
  initialize();

  // Regenerate LMR table based on current config (linear vs logarithmic formula)
  regenerateLmrTable();

  // set up and report search limits
  setupSearchLimits(position, searchLimits);

  // when not pondering and search is time controlled start timer
  if (searchLimits.timeControl && !searchLimits.ponder) { startTimer(); }

  // age tt entries
  if (SearchConfig.USE_TT) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table: Using TT: {}", tt->str());
    tt->ageEntries();
  }
  else { LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table: Not using TT."); }

  // Initialize ply-based data
  // Clear entire PV table once (uses memset internally, zero heap allocations)
  pv.clearAll();

  // Initialize per-ply search state (MoveGenerators, history pointers, etc.)
  for (int i = DEPTH_NONE; i < DEPTH_MAX; i++) {
    plyStack[i].resetSearchState();
    if (SearchConfig.USE_HISTORY_COUNTER || SearchConfig.USE_HISTORY_MOVES) { plyStack[i].mg->setHistoryData(&history); }
  }

  // release the init phase lock to signal the calling go routine
  // waiting in StartSearch() to return
  initSemaphore.release();

  // check for opening book move when we have a time-controlled game
  Move bookMove = MOVE_NONE;
  if (book && SearchConfig.USE_BOOK && searchLimits.timeControl) {
    // TODO: instead of a random book move we could select a book move based on
    //  some score and some variation (randomness)
    bookMove = book->getRandomMove(position.getZobristKey());
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Opening Book: Choosing book move {}", bookMove.str());
  }
  else { LOG__INFO(Logger::get().SEARCH_LOG, "Opening Book: Not using book."); }

  LOG__INFO(Logger::get().SEARCH_LOG, "Search using: PVS={} ASP={}", SearchConfig.USE_PVS, SearchConfig.USE_ASP);

  // If we have found a book-move an update result and omit search.
  // Otherwise, start search with iterative deepening.
  SearchResult searchResult{position};
  if (!bookMove) { searchResult = iterativeDeepening(position); }
  else {
    searchResult.bestMove = bookMove;
    searchResult.bookMove = true;
    hadBookMove           = true;
  }

  // If we arrive here during Ponder mode or Infinite mode and the search is not
  // stopped, it means that the search was finished before it has been stopped
  // by stopSearchFlag or ponderhit;
  // We wait here until the search has completed.
  if (!stopSearchFlag && (searchLimits.ponder || searchLimits.infinite)) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Search finished before stopped or ponderhit! Waiting for stop/ponderhit to send result");
    // relaxed busy wait
    while (!stopSearchFlag && (searchLimits.ponder || searchLimits.infinite)) {
      std::this_thread::sleep_for(milliseconds(5));
    }
  }

  // Clean up
  // make sure the timer stops as this could potentially still be running
  // when the search finished without any stop signal/limit
  stopSearchFlag = true;

  // update the search result with search time and pv
  searchResult.time  = currentTime() - startSearchTime;
  searchResult.pv    = pv.extract();
  searchResult.nodes = nodesVisited;

  // print stats to log
  LOG__INFO(Logger::get().SEARCH_LOG, "Search finished after {}", str(searchResult.time));
  LOG__INFO(Logger::get().SEARCH_LOG, "Search depth was {}({}) with {:L} nodes visited. NPS = {:L} nps", statistics.currentSearchDepth, statistics.currentExtraSearchDepth, nodesVisited, nps(nodesVisited, searchResult.time));
  LOG__DEBUG(Logger::get().SEARCH_LOG, "Search stats: {}", statistics.str());

  // print result to log
  if (searchLimits.mate && searchResult.mateFound) { LOG__INFO(Logger::get().SEARCH_LOG, "Mate in {} found: {}", searchLimits.mate, pv.first().str()); }
  LOG__INFO(Logger::get().SEARCH_LOG, "Search result: {}", searchResult.str());

  // save the result until overwritten by the next search
  lastSearchResult = searchResult;

  // At the end of a search we send the result in any case even if
  // searched has been stopped.
  sendResult(searchResult);

  // clean up timer thread if necessary
  if (timerThread.joinable()) timerThread.join();

  // release the running semaphore after the search has ended
  isRunningSemaphore.release();
}

SearchResult Search::iterativeDeepening(Position& p) {
  SearchResult searchResult{p};

  // check repetition and 50-moves rule
  if (checkDrawRepAnd50(p, 2)) {
    const std::string msg = searchLimits.ponder
                              ? "Ponder called on DRAW by Repetition or 50-moves-rule"
                              : "Search called on DRAW by Repetition or 50-moves-rule";
    sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
    searchResult.bestMoveValue = VALUE_DRAW;
    return searchResult;
  }

  // generate all legal root moves for the position
  rootMoves = *plyStack[0].mg->generateLegalMoves(p, GenAll);

  // check if there are legal moves - if not, it's mate or stalemate
  if (rootMoves.empty()) {
    if (p.hasCheck()) {
      statistics.checkmates++;
      const std::string msg = searchLimits.ponder
                                ? "Ponder called on a check mate position"
                                : "Search called on a check mate position";
      sendString(msg);
      LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
      searchResult.bestMoveValue = -VALUE_CHECKMATE;
    }
    else {
      statistics.stalemates++;
      const std::string msg = searchLimits.ponder
                                ? "Ponder called on a stale mate position"
                                : "Search called on a stale mate position";
      sendString(msg);
      LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
      searchResult.bestMoveValue = VALUE_DRAW;
    }
    return searchResult;
  }

  // Reset TB root info from previous search
  tbRootMove  = MOVE_NONE;
  tbRootValue = VALUE_NONE;
  tbRootWdl   = tablebase::TBResult::Failed;
  tbRootDtz   = 0;

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
      tbRootMove  = searchResult.bestMove;
      tbRootValue = searchResult.bestMoveValue;
      // tbRootWdl and tbRootDtz already set in probeTablebaseAtRoot
      LOG__INFO(Logger::get().SEARCH_LOG, "TB hit at root (non-immediate): move={} value={}, continuing search for PV",
                tbRootMove.str(), tbRootValue.str());

      // Filter root moves to only those that maintain the TB result
      // This ensures we don't play a move that worsens our position
      filterRootMovesByTB(p);

      // Reset searchResult - we'll populate it from search
      searchResult = SearchResult{p};
    }
  }

  // add some extra time for the move after the last book move
  // hadBookMove move will be true at his point if we ever had
  // a book move.
  if (hadBookMove && searchLimits.timeControl && searchLimits.moveTime.count() == 0) {
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
  const double rootComplexityFactor = computeComplexityFactorFromMoves(p, rootMoves);
  LOG__INFO(Logger::get().SEARCH_LOG, "Root complexity factor: {:.2f} (moves {}, inCheck {}, captures share ~)",
            rootComplexityFactor, rootMoves.size(), p.hasCheck());

  // Debug: planned time budget before starting Iterative Deepening (no in-search extensions)
  LOG__DEBUG(Logger::get().SEARCH_LOG,
             "Planned time budget for this move (no in-search extensions): {}",
             str(timeLimit));


  // Volatility tracking within this search
  Value prevBestRootValue       = VALUE_NONE;// best root eval from previous iteration
  bool addedVolatilityExtraTime = false;     // guard to add extra time due to eval swing at most once

  // Reset best-move instability tracking for this search
  bestMoveStability.reset();

  // prepare max depth from search limits
  const int maxDepth = searchLimits.depth ? searchLimits.depth : DEPTH_MAX;

  // If we have a TB root move (from non-immediate probe), give it a high sort value
  // so it's searched first. This ensures the TB move is the PV if it's truly best.
  if (tbRootMove != MOVE_NONE) {
    for (Move& move : rootMoves) {
      if (move == tbRootMove) {
        // Give TB move a very high sort value to ensure it's searched first
        move.setValue(tbRootValue);
        LOG__DEBUG(Logger::get().SEARCH_LOG, "TB move {} prioritized with value {}", move.str(), tbRootValue.str());
        break;
      }
    }
    // Sort so TB move is first
    std::ranges::stable_sort(rootMoves, moveValueGreaterComparator());
  }

  // Max window search in preparation for aspiration window
  // is not needed yet
  constexpr Value alpha = VALUE_MIN;
  constexpr Value beta  = VALUE_MAX;
  Value bestValue       = VALUE_NONE;

  // ###########################################
  // ### BEGIN Iterative Deepening
  // Initialize NPS tracking right before starting search to exclude initialization overhead
  npsTime  = nowFast();
  npsNodes = nodesVisited;
  milliseconds lastIterationMs{0};
  uint64_t lastIterationNodes = 0;
  uint64_t prevIterationNodes = 0;
  for (auto iterationDepth = Depth{1}; iterationDepth <= maxDepth; ++iterationDepth) {

    // ===========================================
    // Before starting a new iteration, check if we have enough time left to likely complete it.
    if (searchLimits.timeControl && !searchLimits.ponder && iterationDepth > 1) {
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
      const uint64_t nowTimeFast = nowFast();
      uint64_t currentNps        = 0;
      if (nowTimeFast > npsTime) { currentNps = nps(nodesVisited - npsNodes, nowTimeFast - npsTime); }
      if (currentNps == 0) { currentNps = nps(nodesVisited, sinceNs); }
      if (currentNps == 0) { currentNps = 1; }

      // Predict the node count of the next iteration using observed growth.
      double growth = prevIterationNodes > 0
                        ? static_cast<double>(lastIterationNodes) / static_cast<double>(prevIterationNodes)
                        : 1.7;// default growth when we only have one observation
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
    // ===========================================

    // update search counter
    nodesVisited++;

    // update depth statistics
    statistics.currentIterationDepth = iterationDepth;
    statistics.currentSearchDepth    = statistics.currentIterationDepth;
    if (statistics.currentExtraSearchDepth < statistics.currentIterationDepth) {
      statistics.currentExtraSearchDepth = statistics.currentIterationDepth;
    }

    // reset perft counter for last depth to
    statistics.perftNodeCount = 0;

    // Measure iteration duration
    const TimePoint iterationStartTime = currentTime();
    const uint64_t iterStartNodes      = nodesVisited;

    // ###########################################
    // Start actual alpha beta search
    // ASPIRATION SEARCH
    if (SearchConfig.USE_ASP && iterationDepth > 3) {
      bestValue = aspirationSearch(p, iterationDepth, bestValue);
    }
    // PVS SEARCH (or pure ALPHA BETA when PVS deactivated)
    else {
      bestValue = rootSearch(p, iterationDepth, alpha, beta);
    }
    // ###########################################

    // record iteration duration for next pre-check
    lastIterationMs = MILLISECONDS(currentTime() - iterationStartTime);

    // record node counts for growth prediction
    prevIterationNodes = lastIterationNodes;
    lastIterationNodes = nodesVisited - iterStartNodes;

    assert(!pv.empty() && pv.first() != MOVE_NONE && "pv must contain a valid first move");
    assert((bestValue == pv.first().value() || stopSearchFlag) && "bestValue should be equal value of pv.first()");

    // Conservative volatility detector: big evaluation swings between consecutive iterations
    if (!addedVolatilityExtraTime && !isTimeAlmostUp()) {
      const Value currBest = pv.first().value();
      // Only consider reasonably deep iterations to avoid noise from shallow depths
      constexpr int VOL_SWING_MIN_DEPTH = 6;
      constexpr auto VOL_SWING_THRESH   = Value{150};// ~1.5 pawns
      if (iterationDepth >= VOL_SWING_MIN_DEPTH && currBest.isValid() && prevBestRootValue.isValid()) {
        Value delta = currBest - prevBestRootValue;
        if (delta < VALUE_ZERO) delta = -delta;
        if (delta >= VOL_SWING_THRESH) {
          addExtraTime(1.15);// +15%
          addedVolatilityExtraTime = true;
          LOG__DEBUG(Logger::get().SEARCH_LOG, "Volatility: large eval swing at depth {} (Δ{} >= {}). Adding small extra time (15%).",
                     iterationDepth, delta.str(), VOL_SWING_THRESH.str());
        }
      }
      // remember current value for next iteration comparison
      if (currBest.isValid()) { prevBestRootValue = currBest; }
    }

    // Best-move instability tracking for dynamic time management
    // Tracks whether the best move is stable (same across iterations) or unstable (changing).
    if (SearchConfig.USE_BESTMOVE_INSTABILITY && searchLimits.timeControl && !searchLimits.ponder && !isTimeAlmostUp()) {
      const Move currentBestMove = pv.first().stripped();
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
        && abs(pv.first().value()) >= VALUE_CHECKMATE_THRESHOLD
        && searchLimits.mate * 2 - 1 == VALUE_CHECKMATE - pv.first().value()) {
      searchResult.mateFound = true;
      break;
    }

    // Check if we need to stop.
    // Doing this after the first iteration ensures that
    // we have done at least one complete search and have
    // a pv (best) move.
    // If we only have one move to play also stop the search
    if (!stopConditions()) {
      // sort root moves for the next iteration
      std::ranges::stable_sort(rootMoves, moveValueGreaterComparator());
      statistics.currentBestRootMove      = pv.first();
      statistics.currentBestRootMoveValue = pv.first().value();
      assert(pv.first() == rootMoves.at(0) && "Best root move should be equal to pv.first()");
      // update UCI GUI
      sendIterationEndInfoToUci();
    }
    else {
      break;
    }
  }
  // ### END OF Iterative Deepening
  // ###########################################

  // update searchResult
  // the best move is pv(0,0) - we need to make sure this entry exists at this time
  // the best value is pv(0,0).value()
  searchResult.bestMove      = pv.first().stripped();
  searchResult.bestMoveValue = pv.first().value();
  searchResult.depth         = statistics.currentIterationDepth;
  searchResult.extraDepth    = statistics.currentExtraSearchDepth;
  searchResult.bookMove      = false;

  // If we had a TB probe at root (non-immediate mode), decide which move to use.
  // TB move is DTZ-optimal (shortest path to zeroing move / conversion).
  // Only override TB move if search found a provable shorter mate.
  if (tbRootWdl != tablebase::TBResult::Failed) {
    searchResult.tbHit = true;

    // Check if search found a proven mate
    const Value searchValue    = pv.first().value();
    const bool searchFoundMate = searchValue >= VALUE_CHECKMATE_THRESHOLD;

    // Calculate mate depth if search found mate (in plies/half-moves)
    // VALUE_CHECKMATE - searchValue gives the mate distance
    const int searchMateDepth = searchFoundMate
                                  ? static_cast<int>(VALUE_CHECKMATE) - static_cast<int>(searchValue)
                                  : INT_MAX;

    // TB DTZ is distance to zeroing (capture/pawn move), NOT necessarily mate.
    // A proven mate is always preferred over a TB "Win" because:
    // 1. Mate is concrete and forced
    // 2. TB DTZ=1 might be a capture leading to a longer mate sequence
    // Use search result if it found a mate at least as short as TB's DTZ
    if (searchFoundMate && searchMateDepth <= tbRootDtz) {
      // Search found a proven mate - keep search's move and score
      LOG__INFO(Logger::get().SEARCH_LOG,
                "Search found mate in {} (TB DTZ={}), using search move {}",
                searchMateDepth, tbRootDtz, searchResult.bestMove.str());
      // Keep searchResult.bestMove and searchResult.bestMoveValue from search
      // But still mark as TB-backed since we verified with TB
    }
    else {
      // Use TB move - it's DTZ-optimal (shortest path to conversion)
      // Search's move might delay the win or risk 50-move rule
      const Move searchMove      = searchResult.bestMove;
      searchResult.bestMove      = tbRootMove;
      searchResult.bestMoveValue = tbRootValue;
      if (searchMove == tbRootMove) {
        LOG__INFO(Logger::get().SEARCH_LOG, "Search confirmed TB-optimal move {}", tbRootMove.str());
      }
      else {
        LOG__INFO(Logger::get().SEARCH_LOG,
                  "Using TB-optimal move {} (DTZ={}), search suggested {}",
                  tbRootMove.str(), tbRootDtz, searchMove.str());
      }
    }
  }

  // see if we have a move we could ponder on
  if (pv.hasLength(DEPTH_NONE, 2)) {
    searchResult.ponderMove = pv(DEPTH_NONE, 1).stripped();
  }
  else {
    // we do not have a ponder-move in the pv-list,
    // so let's check the TT
    if (SearchConfig.USE_TT) {
      p.doMove(searchResult.bestMove);
      if (const auto* ttEntryPtr = tt->probe(p.getZobristKey())) {
        statistics.ttHit++;
        searchResult.ponderMove = static_cast<Move>(ttEntryPtr->move);
        LOG__DEBUG(Logger::get().SEARCH_LOG, "Using ponder move from hash table: {}", searchResult.ponderMove.str());
      }
      p.undoMove();
    }
  }

  // Double-check the ponder move to avoid a ponder search on mate or draw position.
  // If position after ponder move is a final position do not even send a ponder move.
  // This is necessary as arena sends a go ponder command although the position is final.
  if (searchResult.ponderMove != MOVE_NONE) {
    p.doMove(searchResult.bestMove);
    p.doMove(searchResult.ponderMove);
    // check repetition and 50-moves rule or if there are legal moves when using ponder move
    if (checkDrawRepAnd50(p, 2) || plyStack[0].mg->generateLegalMoves(p, GenAll)->empty()) {
      LOG__DEBUG(Logger::get().SEARCH_LOG, "ponder move omitted as game finished");
      searchResult.ponderMove = MOVE_NONE;
    }
    p.undoMove();
    p.undoMove();
  }

  return searchResult;
}

Value Search::aspirationSearch(Position& p, const Depth depth, const Value bestValue) {
  // aspiration steps
  constexpr std::array aspirationSteps = {Value{50}, Value{200}, VALUE_MAX};

  constexpr auto steps = aspirationSteps.size();
  Value value          = VALUE_NONE;

  // new search window
  Value alpha = std::max(bestValue - aspirationSteps[0], VALUE_MIN);
  Value beta  = std::min(bestValue + aspirationSteps[0], VALUE_MAX);

  for (auto i = 1; i < steps; ++i) {
    // search with the reduced window or last with the full window
    value = rootSearch(p, depth, alpha, beta);
    // if search has been stopped and the value has missed the window, return
    // the value and the values of the root moves are invalid
    if (stopConditions() && (value <= alpha || value >= beta)) { return VALUE_NONE; }

    // If time is almost up, avoid further aspiration expansions and return current value
    if (isTimeAlmostUp()) { return value; }

    // check if the value was within the window or expand the window
    if (value <= alpha) {
      // FAIL LOW - widen alpha (lower bound)
      sendAspirationResearchInfo("upperbound");
      // add some extra time because of fail low
      // we might have found a strong opponent's move
      addExtraTime(1.3);
      // If time is almost up, don't expand; return current value
      if (isTimeAlmostUp()) { return value; }
      // if we fail low tests, it is best to immediately open up the window full
      // Alternatively, we could do steps as well
      // alpha = VALUE_MIN;
      alpha = std::max(bestValue - aspirationSteps[i], VALUE_MIN);
      statistics.aspirationResearches++;
    }
    else if (value >= beta) {
      // FAIL HIGH - increase upper bound
      sendAspirationResearchInfo("lowerbound");
      // If time is almost up, don't expand; return current value
      if (isTimeAlmostUp()) { return value; }
      beta = std::min(bestValue + aspirationSteps[i], VALUE_MAX);
      statistics.aspirationResearches++;
    }
    else { break; }
  }

  // With a fully open search window of the last step, we can accept
  // partial searches as well. Root move values are usable and can
  // be sorted to find the best move.
  return value;
}

Value Search::rootSearch(Position& p, const Depth depth, Value alpha, const Value beta) {

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
  const size_t size = rootMoves.size();
  for (size_t i = 0; i < size; i++) {
    Move& moveRef = rootMoves.at(i);

    p.doMove(moveRef);
    nodesVisited++;
    statistics.currentVariation.push_back(moveRef);
    statistics.currentRootMoveIndex = i;
    statistics.currentRootMove      = moveRef;

    if (checkDrawRepAnd50(p, 2)) {
      value = VALUE_DRAW;
    }
    else {
      constexpr Depth ply{1};
      // ///////////////////////////////////////////////////////////////////
      // PVS
      // First move in a node is an assumed PV Move and searched with full search window (PV Node)
      // Root's first child is a PV node (full window search)
      if (!SearchConfig.USE_PVS || i == 0) {
        value = -search(p, depth - 1, ply, -beta, -alpha, PvNode, Do_Null_Move);
      }
      else {
        // Null window search after the initial PV search.
        // After first move, children are CUT nodes (expected to fail high)
        value = -search(p, depth - 1, ply, -alpha - 1, -alpha, CutNode, Do_Null_Move);
        // If this move improved alpha without exceeding beta we do a proper full window
        // search to get an accurate score.
        if (value > alpha && value < beta && !stopConditions() && !isTimeAlmostUp()) {
          statistics.rootPvsResearches++;
          value = -search(p, depth - 1, ply, -beta, -alpha, PvNode, Do_Null_Move);
        }
      }
      // ///////////////////////////////////////////////////////////////////
    }

    statistics.currentVariation.pop_back();
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
      pv.update(moveRef, DEPTH_NONE);
      statistics.bestMoveChange++;
      if (value > alpha) {
        // fail high in root only when using aspiration search
        if (value >= beta && SearchConfig.USE_ALPHABETA) {
          statistics.betaCuts++;
          statistics.betaCutsByIndex[std::min(static_cast<int>(statistics.currentRootMoveIndex), SearchStats::BETA_CUTS_INDEX_SIZE - 1)]++;
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
  //  LOG__DEBUG(Logger::get().SEARCH_LOG, "Search {} {} {}", depth, ply, str(statistics.currentVariation));

  // Clear PV for this node to prevent stale data from previous iterations/branches
  // from being propagated up via pv.update(). Stale PV data can contain moves from
  // different positions that are illegal in the current position.
  // Note: IID may write to pv but clears it after extracting the ttMove.
  pv.clear(ply);

  // Enter quiescence search when depth == 0 or max ply has been reached
  // pvNodes/nonPvNodes are tracked inside qsearch() — no need to count here.
  if (depth == 0 || ply >= MAX_DEPTH) {
    const auto value = qsearch(p, ply, alpha, beta, nodeType);
    return value;
  }

  // Track PV vs non-PV node statistics (after qsearch drop-through to avoid double-counting
  // — qsearch() already tracks its own pvNodes/nonPvNodes at entry)
  if (nodeType == PvNode) { statistics.pvNodes++; }
  else { statistics.nonPvNodes++; }
  statistics.searchNodes++;

  // check if search should be stopped
  if (stopConditions() && depth > 1) { return VALUE_NONE; }

  // Mate Distance Pruning
  // Did we already find a shorter mate then ignore
  // this one.
  if (SearchConfig.USE_MDP) {
    alpha = std::max(alpha, -VALUE_CHECKMATE + static_cast<Value>(ply));
    beta  = std::min(beta, VALUE_CHECKMATE - static_cast<Value>(ply));
    if (alpha >= beta) {
      statistics.mdp++;
      return alpha;
    }
  }

  // prepare node search
  const Color us      = p.getNextPlayer();
  Value bestNodeValue = VALUE_NONE;
  Move bestNodeMove   = MOVE_NONE;// used to store in the TT
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
    if (const TT::Entry* ttEntryPtr = tt->probe(p.getZobristKey())) {
      // tt hit
      statistics.ttHit++;
      ttMove  = static_cast<Move>(ttEntryPtr->move);
      ttValue = valueFromTt(ttEntryPtr->value, ply);
      ttDepth = static_cast<Depth>(ttEntryPtr->depth);
      // Never cutoff on PV nodes - this ensures we always build a complete PV line
      // Non-PV nodes can still use TT cutoffs as they don't contribute to the reported PV
      if (nodeType != PvNode && ttDepth >= depth) {
        if (SearchConfig.USE_TT_VALUE
            && ttValue.isValid()
            && (ttEntryPtr->type == EXACT
                || (ttEntryPtr->type == ALPHA && ttValue <= alpha)
                || (ttEntryPtr->type == BETA && ttValue >= beta))) {
          statistics.TtCuts++;
          return ttValue;
        }
        statistics.TtNoCuts++;
      }
      // if we have a static eval stored, we can reuse it
      if (SearchConfig.USE_EVAL_TT && ttEntryPtr->eval != VALUE_NONE) {
        statistics.evalFromTT++;
        staticEval = ttEntryPtr->eval;
      }
    }
    else { statistics.ttMiss++; }
  }// use TT

  // Tablebase probing in search (after TT lookup to use cached TB results)
  // Probe WDL for positions within TB piece limit. This can provide
  // early cutoffs for winning/losing positions or exact draws.
  // Only probe at sufficient depth to avoid overhead in shallow searches.
  // On PV nodes: only use TB to tighten bounds - don't cut off (need complete PV line).
  // If USE_TB_PROBE_PV is false, skip probing on PV nodes entirely (performance optimization).
  if (SearchConfig.USE_TB_PROBE_SEARCH
      && (SearchConfig.USE_TB_PROBE_PV || nodeType != PvNode)
      && syzygy_tb
      && syzygy_tb->isAvailable()
      && depth >= SearchConfig.TB_PROBE_DEPTH
      && p.getOccupiedBb().popcount() <= SearchConfig.TB_PROBE_LIMIT
      && p.getCastlingRights() == NO_CASTLING) {

    const tablebase::TBResult wdl = syzygy_tb->probeWDL(p);

    if (wdl != tablebase::TBResult::Failed) {
      statistics.tbSearchHits++;

      // Convert WDL to score with 50-move rule handling
      const Value tbScore = getTBScoreForSearch(wdl, p.getHalfMoveClock(), ply);

      // Use WDL as bound for alpha-beta
      // On PV nodes: only tighten bounds, never cut off (need to build PV)
      // On non-PV nodes: can cut off immediately
      if (wdl == tablebase::TBResult::Win || wdl == tablebase::TBResult::CursedWin) {
        // Position is winning - use as lower bound
        if (nodeType != PvNode && tbScore >= beta) {
          statistics.tbSearchCutoffs++;
          // Store in TT for future lookups
          if (SearchConfig.USE_TT) {
            storeTt(p, depth, ply, MOVE_NONE, tbScore, BETA, VALUE_NONE);
          }
          return tbScore;// Fail high (beta cutoff)
        }
        // Tighten alpha if TB score is better
        alpha = std::max(alpha, tbScore);
      }
      else if (wdl == tablebase::TBResult::Loss || wdl == tablebase::TBResult::BlessedLoss) {
        // Position is losing - use as upper bound
        if (nodeType != PvNode && tbScore <= alpha) {
          statistics.tbSearchCutoffs++;
          // Store in TT for future lookups
          if (SearchConfig.USE_TT) {
            storeTt(p, depth, ply, MOVE_NONE, tbScore, ALPHA, VALUE_NONE);
          }
          return tbScore;// Fail low (alpha cutoff)
        }
        // Tighten beta if TB score is worse
        beta = std::min(beta, tbScore);
      }
      else if (wdl == tablebase::TBResult::Draw) {
        // Exact draw - can return immediately on non-PV nodes
        if (nodeType != PvNode) {
          statistics.tbSearchCutoffs++;
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
      statistics.tbSearchMisses++;
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
  plyStack[ply].staticEval = hasCheck ? VALUE_NONE : staticEval;

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
    const Value prevEval = plyStack[ply - 2].staticEval;
    if (prevEval != VALUE_NONE) return staticEval > prevEval;
    // 2 plies ago was in check — try 4 plies ago
    if (ply >= 4) {
      const Value prevEval4 = plyStack[ply - 4].staticEval;
      if (prevEval4 != VALUE_NONE) return staticEval > prevEval4;
    }
    return true;// Conservative: assume improving when no reference data
  }();

  // Track improving statistics
  if (SearchConfig.USE_IMPROVING && !hasCheck) {
    if (improving) { statistics.improvingTrue++; }
    else { statistics.improvingFalse++; }
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
    statistics.razorings++;
    // fix 19.2.2026 - use AllNode for razor to avoid missing critical moves in PV line; razor is a
    // heuristic that can afford to miss some moves, but we don't want it to miss critical moves in
    // the PV line. Use AllNode since we're expecting to fail low (that's why we're razoring).
    return qsearch(p, ply, alpha, beta, AllNode);
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
      && std::abs(beta) < VALUE_CHECKMATE_THRESHOLD) {  // Don't prune when beta is a mate score
    auto margin = Value{SearchConfig.RFP_MARGIN[depth]};
    // Increase margin when not improving → prune less aggressively (Stockfish-style)
    // Rationale: "not improving" means eval may be unreliable, so search more carefully
    if (SearchConfig.USE_RFP_IMPROVING && !improving) {
      // TODO: Test this with different values
      margin += Value{SearchConfig.RFP_IMPROVING_MARGIN};
    }
    if (staticEval - margin >= beta) {
      statistics.rfp_cuts++;
      return staticEval - margin;// fail-hard: beta / fail-soft: staticEval - evalMargin;
    }
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
      nodesVisited++;
      Value nValue = -search(p, newDepth, ply + 1, -beta, -beta + 1, CutNode, No_Null_Move);
      p.undoNullMove();

      // check if we should stop the search
      if (stopConditions()) { return VALUE_NONE; }

      // flag for mate threats
      if (nValue > VALUE_CHECKMATE_THRESHOLD) {
        // although this player did not make a move the value still is
        // a mate - very good! Just adjust the value to not return an
        // unproven mate
        nValue = VALUE_CHECKMATE_THRESHOLD;
      }
      else if (nValue < -(VALUE_CHECKMATE - 6)) {// limit for mate in 3 or less
        // the player did not move and got mated ==> mate threat
        matethreat = true;
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
          const Value v      = search(p, verifyDepth, ply, beta - 1, beta, nodeType, do_null);
          if (stopConditions()) { return VALUE_NONE; }
          if (v < beta) {
            statistics.nullMoveVerifications++;
            // fall through: no cutoff
          }
          else {
            if (SearchConfig.USE_TT) { storeTt(p, depth, ply, MOVE_NONE, nValue, BETA, staticEval); }
            statistics.nullMoveCuts++;
            return nValue;
          }
        }
        else {
          if (SearchConfig.USE_TT) { storeTt(p, depth, ply, MOVE_NONE, nValue, BETA, staticEval); }
          statistics.nullMoveCuts++;
          return nValue;
        }
      }
    }
  }

  // Internal Iterative Deepening (IID)
  // https://www.chessprogramming.org/Internal_Iterative_Deepening
  // Used when no best move from the tt is available from previous
  // searches. IID is used to find a good move to search first by
  // searching the current position to a reduced depth and using
  // the best move of that search as the first move at the real depth.
  // Does not make a big difference in search tree size when move
  // order already is good.
  if (SearchConfig.USE_IID) {
    if (depth >= SearchConfig.IID_DEPTH
        && !ttMove// no move from TT
        && doNull
        && nodeType == PvNode) {// avoid in null move search

      // get the new depth and make sure it is >0
      auto newDepthIid = depth - SearchConfig.IID_REDUCTION;
      if (newDepthIid < 0) { newDepthIid = DEPTH_NONE; }

      // do the actual reduced search only if we have time left
      if (!isTimeAlmostUp()) {
        // IID search inherits nodeType (searching same node at reduced depth)
        search(p, newDepthIid, ply, alpha, beta, nodeType, doNull);
        statistics.iidSearches++;

        // check if we should stop the search
        if (stopConditions()) { return VALUE_NONE; }

        // get the best move from the reduced search if available
        if (!pv.empty(ply)) {
          statistics.iidMoves++;
          ttMove = pv.first(ply).stripped();
          // Clear pv after extracting the move - IID polluted it
          pv.clear(ply);
        }
      }
    }
  }

  // reset move generator for the actual search
  // Use mgSingular when in singular verification search (excludedMove is set)
  // to avoid corrupting the outer search's MoveGenerator state
  auto& info       = plyStack[ply];
  auto* const myMg = info.excludedMove != MOVE_NONE ? info.mgSingular.get() : info.mg.get();
  myMg->resetOnDemand();

  // PV Move Sort
  // When we received a best move for the position from the
  // TT or IID, we set it as PV move in the move-gen so it will
  // be searched first.
  if (SearchConfig.USE_TT_PV_MOVE_SORT && ttMove != MOVE_NONE) {
    statistics.TtMoveUsed++;
    myMg->setPV(ttMove);
  }
  else { statistics.NoTtMove++; }

  // prepare move loop
  Value value;
  Move move;
  int movesSearched = 0;// to detect mate situations

  // ///////////////////////////////////////////////////////
  // MOVE LOOP
  while ((move = myMg->getNextPseudoLegalMove(p, GenAll, hasCheck)) != MOVE_NONE) {
    // Skip excluded move (used for singular extension verification searches)
    if (move == info.excludedMove) { continue; }

    const Square from     = move.from();
    const Square to       = move.to();
    const bool givesCheck = p.givesCheck(move);

    // prepare newDepth
    const Depth newDepthFixed = depth - DEPTH_ONE;// default depth reduction for the next ply
    Depth newDepth            = newDepthFixed;    // default depth for the next ply - might be extended later
    Depth lmrDepth            = newDepthFixed;    // default depth for LMR reductions - might be reduced later
    Depth extension           = DEPTH_NONE;

    // Here we try some search extensions. This has to be done
    // very carefully as it usually is more effective to prune
    // than to extend.
    if (SearchConfig.USE_EXTENSIONS) {
      // Check extension: extend when a move gives check, but only for the
      // first few moves (which are the most promising due to move ordering).
      // This limits search explosion while focusing extensions on important checks.
      // The QS search already handles all check evasions, but this extension
      // allows the normal search pruning techniques to be applied.
      if (SearchConfig.USE_CHECK_EXT
          && givesCheck
          && movesSearched < SearchConfig.CHECK_EXT_EARLY_LIMIT) {
        statistics.checkExtension++;
        extension = DEPTH_ONE;
      }

      // If we have found a mate threat during Null Move Search
      // we extend normal search by one ply to try to find
      // a way out.
      // Deactivated in config as this grows the search tree
      // too much.
      if (SearchConfig.USE_THREAT_EXT
          && matethreat) {
        statistics.threatExtension++;
        extension = DEPTH_ONE;
      }

      // Singular Extensions
      // https://www.chessprogramming.org/Singular_Extensions
      // When we have a TT move that appears significantly better than all alternatives,
      // extend its search to avoid missing critical tactical lines.
      // We do a reduced-depth null-window search excluding the TT move to verify
      // that no other move can reach close to the TT value.
      if (SearchConfig.USE_SINGULAR_EXT
          && extension == 0                                  // no other extension applied
          && move == ttMove                                  // this is the TT move
          && depth >= SearchConfig.SINGULAR_MIN_DEPTH        // sufficient depth
          && ttValue != VALUE_NONE                           // valid TT value
          && ttDepth >= depth - 3                            // TT entry was from similar or deeper search
          && !hasCheck                                       // not in check (avoid instability)
          && std::abs(ttValue) < VALUE_CHECKMATE_THRESHOLD) {// not a mate score

        // Reduced beta for the verification search
        const Value singularBeta = ttValue - Value{SearchConfig.SINGULAR_MARGIN};

        // Reduced depth for the verification search
        Depth singularDepth = (depth - SearchConfig.SINGULAR_REDUCTION) / 2;
        if (singularDepth < 1) { singularDepth = DEPTH_ONE; }

        // Set the excluded move for this ply so the verification search skips the TT move
        info.excludedMove = ttMove;

        statistics.singularSearches++;

        // Do a null-window search to see if any other move can reach singularBeta
        // Uses mgSingular automatically because excludedMove is set
        // Singular verification is a CUT node search (looking for fail-high)
        const Value singularValue = search(p, singularDepth, ply, singularBeta - 1, singularBeta, CutNode, No_Null_Move);

        // Clear the excluded move
        info.excludedMove = MOVE_NONE;

        // check if we should stop the search
        if (stopConditions()) { return VALUE_NONE; }

        // If no other move reaches singularBeta, the TT move is singular - extend it
        if (singularValue < singularBeta) {
          statistics.singularExtension++;
          extension = DEPTH_ONE;
        }
      }

      // With this turned off, we still can use extension to
      // at least avoid reductions for these moves.
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
          statistics.fpPrunings++;
          continue;
        }
      }

      // LMP - Late Move Pruning
      // aka Move-Count-Based Pruning
      if (SearchConfig.USE_LMP) {
        const int lmpDepth = depth > 15 ? 15 : depth;
        int lmpThreshold = SearchConfig.LMP_MOVES[lmpDepth];
        // When improving, allow searching more moves before pruning
        if (SearchConfig.USE_LMP_IMPROVING && improving) {
          lmpThreshold += lmpThreshold / 2; // 50% more moves when improving
        }
        if (movesSearched >= lmpThreshold) {
          statistics.lmpCuts++;
          continue;
        }
      }

      // LMR
      // Late Move Reduction assumes that later moves a rarely
      // exceeding alpha and therefore the search is reduced in
      // depth. This is, in effect, a soft transition into
      // quiescence search as we usually try the pv move and
      // capturing moves first. In quiescence only capturing
      // moves are searched anyway.
      // newDepth is the "standard" new depth (depth - 1)
      // lmrDepth is set to newDepth and only reduced
      // if conditions apply.
      if (SearchConfig.USE_LMR
          && depth >= SearchConfig.LMR_MIN_DEPTH
          && movesSearched >= SearchConfig.LMR_MIN_MOVES
          && nodeType != PvNode
          && !givesCheck
          && !p.isCapturingMove(move)
          && move.type() != PROMOTION
          && !matethreat) {
        // fprintln("DEBUG: considering LMR for move {} at depth {} and move count {}", move.str(), depth, movesSearched);
        const int d = std::min(depth, Depth{31});
        const int m = std::min(movesSearched, 63);
        lmrDepth -= static_cast<Depth>(LMR_REDUCTION[d][m]);
        // Reduce more when position is NOT improving (eval not better than 2 plies ago)
        if (SearchConfig.USE_LMR_IMPROVING && !improving) {
          lmrDepth -= static_cast<Depth>(SearchConfig.LMR_IMPROVING_REDUCTION);
        }
        // Reduce more on expected cut nodes (expected to fail high)
        // Late moves on cut nodes are very unlikely to be the best move
        if (SearchConfig.USE_LMR_CUTNODE && nodeType == CutNode) {
          lmrDepth -= static_cast<Depth>(SearchConfig.LMR_CUTNODE_REDUCTION);
          statistics.lmrCutNodeReductions++;
        }
        // Reduce less for moves with good history (frequently caused beta cutoffs)
        // histScore > 0 means good move -> negative reduction adjustment -> less reduction
        if (SearchConfig.USE_LMR_HISTORY) {
          const int histScore = history.historyCount[us][from][to];
          const int histReduction = -histScore / SearchConfig.LMR_HISTORY_DIVISOR;
          if (histReduction < 0) {
            // Positive history -> less reduction (histReduction is negative)
            statistics.lmrHistoryLessReduction++;
            statistics.lmrHistoryDepthSaved -= histReduction; // Convert to positive for tracking
          }
          lmrDepth -= static_cast<Depth>(histReduction);
        }
        // Clamp: don't go below DEPTH_NONE and don't exceed newDepth (no extension via LMR!)
        lmrDepth = std::clamp(lmrDepth, DEPTH_NONE, newDepth);
        statistics.lmrReductions++;
      }
    }

    // ///////////////////////////////////////////////////////

    // ///////////////////////////////////////////////////////
    // DO MOVE
    p.doMove(move);

    // checking for legality is quite expensive so we do it as late as possible
    // after we tried to prune the move already
    // if a move is illegal we just undo and continue with the next move
    if (!p.wasLegalMove()) {
      p.undoMove();
      continue;
    }

    // if available on platform tells the cpu to
    // prefetch the tt data into cpu caches
    TT_PREFETCH;
    // EVAL_PREFETCH;

    // we only count legal moves
    nodesVisited++;
    statistics.currentVariation.push_back(move);

    sendSearchUpdateToUci();

    // check repetition and 50 moves
    if (checkDrawRepAnd50(p, 2)) { value = VALUE_DRAW; }
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
        value = -search(p, newDepth, ply + 1, -beta, -alpha, childType, do_null);
      }
      else {
        // Null window search after the initial PV search.
        // As depth we use a potentially reduced depth if Late Move Reduction
        // conditions have been met above.
        // Later moves with null window: child is CutNode (expect fail-high) or AllNode (if we're CutNode)
        const NodeType childType = (nodeType == CutNode) ? AllNode : CutNode;
        value = -search(p, lmrDepth, ply + 1, -alpha - 1, -alpha, childType, do_null);
        // If this move improved alpha without exceeding beta we do a proper full window
        // search to get an accurate score.
        // Without LMR we check for value > alpha && value < beta
        // With LMR we re-search when value > alpha
        if (value > alpha && !stopConditions() && !isTimeAlmostUp()) {
          // did we actually have a LMR reduction?
          if (lmrDepth < newDepthFixed) {
            statistics.lmrResearches++;
            // Re-search with full depth: if we're PV, child becomes PV; otherwise same alternation
            const NodeType researchType = (nodeType == PvNode) ? PvNode : childType;
            value = -search(p, newDepth, ply + 1, -beta, -alpha, researchType, do_null);
          }
          else if (value < beta) {
            statistics.pvsResearches++;
            // PVS re-search: if we're PV, child becomes PV; otherwise same alternation
            const NodeType researchType = (nodeType == PvNode) ? PvNode : childType;
            value = -search(p, newDepth, ply + 1, -beta, -alpha, researchType, do_null);
          }
        }
      }
      // ///////////////////////////////////////////////////////////////////
    }

    movesSearched++;
    statistics.currentVariation.pop_back();
    p.undoMove();
    // UNDO MOVE
    // ///////////////////////////////////////////////////////


    // check if we should stop the search
    // We want to guarantee at least one complete depth-1 root search.
    // Do not abort mid-loop at depth==1 to keep results deterministic under time pressure.
    if (stopConditions() && depth > 1) { return VALUE_NONE; }

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
          statistics.betaCuts++;
          // Track beta cuts by move index (0-based, clamped to array size)
          statistics.betaCutsByIndex[std::min(movesSearched - 1, SearchStats::BETA_CUTS_INDEX_SIZE - 1)]++;
          // store move which caused a beta cutoff in this ply
          if (SearchConfig.USE_KILLER_MOVES && !p.isCapturingMove(move)) { myMg->storeKiller(move); }
          // Counter for moves which caused a beta cutoff
          // we use 1 << depth as an increment to favor deeper searches
          if (SearchConfig.USE_HISTORY_COUNTER && !p.isCapturingMove(move)) { history.historyCount[us][from][to] += 1L << depth; }
          // store a successful counter-move to the previous opponent move
          if (SearchConfig.USE_HISTORY_MOVES) {
            const Move lastMove = p.getLastMove();
            if (lastMove != MOVE_NONE) { history.counterMoves[lastMove.from()][lastMove.to()] = move; }
          }
          ttType = BETA;
          break;
        }
        // We found a move between alpha and beta which means we
        // really have found the best move so far in the ply which
        // can be forced (opponent can't avoid it).
        pv.update(move, ply);

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
      history.historyCount[us][from][to] -= 1L << depth;
      if (history.historyCount[us][from][to] < 0) { history.historyCount[us][from][to] = 0; }
    }
  }
  // MOVE LOOP
  // ///////////////////////////////////////////////////////

  // If we did not have at least one legal move
  // then we might have a mate or stalemate
  if (movesSearched == 0 && !stopConditions()) {
    if (hasCheck) {
      // mate
      statistics.checkmates++;
      bestNodeValue = -VALUE_CHECKMATE + static_cast<Value>(ply);
    }
    else {
      // stalemate
      statistics.stalemates++;
      bestNodeValue = VALUE_DRAW;
    }
    // this is in any case an exact value
    staticEval = bestNodeValue;
    ttType     = EXACT;
  }

  // Store TT
  // Store search result for this node into the transposition table
  if (SearchConfig.USE_TT) { storeTt(p, depth, ply, bestNodeMove, bestNodeValue, ttType, staticEval); }

  return bestNodeValue;
}

Value Search::qsearch(Position& p, const Depth ply, Value alpha, Value beta, const NodeType nodeType) {
  //  LOG__DEBUG(Logger::get().SEARCH_LOG, "QSearch {} {}", ply, str(statistics.currentVariation));

  // Track PV vs non-PV node statistics
  // In qsearch, CutNode/AllNode are treated the same as non-PV
  if (nodeType == PvNode) { statistics.pvNodes++; }
  else { statistics.nonPvNodes++; }
  statistics.qsearchNodes++;

  // Clear PV for this node (same reason as in search())
  pv.clear(ply);

  if (statistics.currentExtraSearchDepth < ply) { statistics.currentExtraSearchDepth = ply; }

  // if we have deactivated qsearch or we have reached our maximum depth
  // we evaluate the position and return the value
  if (!SearchConfig.USE_QUIESCENCE || ply >= MAX_DEPTH || stopConditions()) {
    statistics.perftNodeCount++;
    return evaluate(p);
  }

  // Mate Distance Pruning
  // Did we already find a shorter mate then ignore this one.
  if (SearchConfig.USE_MDP) {
    alpha = std::max(alpha, -VALUE_CHECKMATE + static_cast<Value>(ply));
    beta  = std::min(beta, VALUE_CHECKMATE - static_cast<Value>(ply));
    if (alpha >= beta) {
      statistics.mdp++;
      return alpha;
    }
  }

  // TT Lookup
  Move ttMove      = MOVE_NONE;
  Value staticEval = VALUE_NONE;
  if (SearchConfig.USE_TT && SearchConfig.USE_QS_TT) {
    if (const TT::Entry* ttEntryPtr = tt->probe(p.getZobristKey())) {
      // tt hit
      statistics.ttHit++;
      ttMove              = static_cast<Move>(ttEntryPtr->move);
      const Value ttValue = valueFromTt(ttEntryPtr->value, ply);
      if (SearchConfig.USE_TT_VALUE
          && nodeType != PvNode
          && ttValue.isValid()
          && (ttEntryPtr->type == EXACT
              || (ttEntryPtr->type == ALPHA && ttValue <= alpha)
              || (ttEntryPtr->type == BETA && ttValue >= beta))) {
        statistics.TtCuts++;
        return ttValue;
      }
      // if we have a static eval stored we can reuse it
      if (SearchConfig.USE_EVAL_TT
          && ttEntryPtr->eval != VALUE_NONE) {
        statistics.evalFromTT++;
        staticEval = ttEntryPtr->eval;
      }
    }
    else { statistics.ttMiss++; }
  }// use TT

  // prepare node search
  Value bestNodeValue = VALUE_NONE;
  Move bestNodeMove   = MOVE_NONE;// used to store in the TT
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
        statistics.standpatCuts++;
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
  auto* const myMg = plyStack[ply].mg.get();
  myMg->resetOnDemand();

  // PV Move Sort
  if (SearchConfig.USE_TT_PV_MOVE_SORT && ttMove != MOVE_NONE) {
    statistics.TtMoveUsed++;
    myMg->setPV(ttMove);
  }
  else {
    statistics.NoTtMove++;
  }

  // prepare move loop
  Value value;
  Move move;
  int movesSearched = 0;// to detect mate situations

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
        && !givesCheck// post move
    ) {
      // to check in futility pruning what material delta we have
      const auto moveGain           = valueOf(p.getPiece(to));
      constexpr auto futilityMargin = Value{150};
      if (staticEval + moveGain + futilityMargin <= alpha) {
        if (staticEval + moveGain > bestNodeValue) { bestNodeValue = staticEval + moveGain; }
        statistics.qfpPrunings++;
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
    nodesVisited++;
    statistics.currentVariation.push_back(move);
    sendSearchUpdateToUci();

    // check repetition and 50 moves
    if (checkDrawRepAnd50(p, 2)) {
      value = VALUE_DRAW;
    }
    else {
      // recursion into qsearch - inherit nodeType (PV nodes stay PV, non-PV stay non-PV)
      value = -qsearch(p, ply + 1, -beta, -alpha, nodeType);
    }

    movesSearched++;
    statistics.currentVariation.pop_back();
    p.undoMove();
    // UNDO MOVE
    // ///////////////////////////////////////////////////////

    // check if we should stop the search
    if (stopConditions()) { return VALUE_NONE; }

    // See the search function above for documentation
    if (value > bestNodeValue) {
      bestNodeValue = value;
      bestNodeMove  = move;
      if (value > alpha) {
        if (value >= beta && SearchConfig.USE_ALPHABETA) {
          statistics.betaCuts++;
          statistics.betaCutsByIndex[std::min(movesSearched - 1, SearchStats::BETA_CUTS_INDEX_SIZE - 1)]++;
          // Note: No killer/history updates in qsearch - we primarily search captures,
          // and history/killers are for quiet move ordering in main search.
          ttType = BETA;
          break;
        }
        pv.update(move, ply);
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
      statistics.checkmates++;
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

inline Value Search::evaluate(const Position& p) {
  statistics.leafPositionsEvaluated++;
  statistics.evaluations++;
  return evaluator->evaluate(p);
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
  tt->put(p.getZobristKey(), depth, move, valueToTt(value, ply), valueType, eval);
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
      if (!std::filesystem::exists(SearchConfig.BOOK_PATH)) {
        const std::string message = std::format("Opening Book '{}' not found. Disabling book usage.", SearchConfig.BOOK_PATH);
        LOG__ERROR(Logger::get().BOOK_LOG, "{}", message);
        ConfigManager::instance().applyOverrides([&](SearchConfigData& s, auto&) {
          s.USE_BOOK = false;
        });
      }
      else {
        book = std::make_unique<OpeningBook>(SearchConfig.BOOK_PATH, OpeningBook::fromString(SearchConfig.BOOK_TYPE));
        book->initialize();
      }
    }
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Opening Book disabled in configuration");
  }

  // init transposition table
  if (SearchConfig.USE_TT) {
    // When constructed with size 0 MB, TT ensures at least 1 entry; treat that as uninitialized sentinel
    if (tt->getMaxNumberOfEntries() == 1) {
      tt->resize(SearchConfig.TT_SIZE_MB);
    }
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table disabled in configuration");
    // Keep TT allocated but minimize its size to 0 MB (internally becomes 1 entry)
    tt->resize(0);
  }

  // init evaluator
  if (!evaluator) {
    // only initialize once
    evaluator = std::make_unique<Evaluator>();
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
  }
  else {
    LOG__WARN(Logger::get().SEARCH_LOG, "Syzygy Tablebase: Failed to initialize from '{}'", SearchConfig.TB_PATH);
    syzygy_tb.reset();// Release failed instance
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
  tbRootWdl = tbResult.wdl;
  tbRootDtz = tbResult.dtz;

  // Log the TB hit
  const std::string tbResultStr = tablebase::Tablebase::resultToString(tbResult.wdl);
  LOG__INFO(Logger::get().SEARCH_LOG, "TB Root: {} DTZ={} move={}",
            tbResultStr, tbResult.dtz, tbResult.bestMove.str());

  // Send info string to UCI
  sendString(std::format("TB hit: {} DTZ={} move={}", tbResultStr, tbResult.dtz, tbResult.bestMove.str()));

  // Populate the search result with DTZ-based scoring
  result.bestMove      = tbResult.bestMove;
  result.bestMoveValue = tablebase::Tablebase::tbResultToScore(tbResult.wdl, tbResult.dtz);
  result.tbHit         = true;

  // Update statistics
  statistics.tbRootHits++;

  return true;
}

void Search::filterRootMovesByTB(Position& pos) {
  // Filter root moves to only those that maintain the TB result.
  // For a winning position, keep only moves where opponent is losing.
  // For a drawn position, keep only moves where opponent is not winning.
  // For a losing position, keep all moves (we're lost anyway).

  if (tbRootWdl == tablebase::TBResult::Failed) {
    return;// No TB result, nothing to filter
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

  const size_t originalCount = rootMoves.size();

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
      return true;// Probe failed, keep the move
    }

    // After our move, it's opponent's turn. WDL is from opponent's perspective.
    // If we're winning, opponent should be losing (WDL = Loss or BlessedLoss)
    // If we're drawing, opponent should be drawing (WDL = Draw)
    // If we're losing, any move is acceptable

    switch (tbRootWdl) {
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
  for (size_t readIdx = 0; readIdx < rootMoves.size(); ++readIdx) {
    if (shouldKeepMove(rootMoves[readIdx])) {
      if (writeIdx != readIdx) {
        rootMoves[writeIdx] = rootMoves[readIdx];
      }
      ++writeIdx;
    }
  }

  // Truncate the list to the number of kept moves
  rootMoves.resize(writeIdx);

  const size_t filteredCount = rootMoves.size();

  if (filteredCount < originalCount) {
    LOG__INFO(Logger::get().SEARCH_LOG, "TB filter: {} -> {} root moves (removed {} suboptimal)",
              originalCount, filteredCount, originalCount - filteredCount);
  }

  // Safety check: if all moves were filtered (shouldn't happen), restore TB move
  if (rootMoves.empty() && tbRootMove != MOVE_NONE) {
    LOG__WARN(Logger::get().SEARCH_LOG, "TB filter removed all moves! Restoring TB move {}", tbRootMove.str());
    rootMoves.push_back(tbRootMove);
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

bool Search::stopConditions() {// NOLINT(*-make-member-function-const)
  if (stopSearchFlag) return true;
  if (searchLimits.nodes > 0 && nodesVisited >= searchLimits.nodes) { stopSearchFlag = true; }
  return stopSearchFlag;
}

bool Search::checkDrawRepAnd50(const Position& p, const int numberOfRepetitions) {
  return p.checkRepetitions(numberOfRepetitions) || p.getHalfMoveClock() >= 100;
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
  const milliseconds relBuffer{budget.count() > 0 ? milliseconds{budget.count() / 50} : milliseconds{0}};// ~2%
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
    const double phase = p.getGamePhaseFactor();// ~1.0 opening/mid, ~0.0 endgame

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
    const int npp     = knights + bishops + rooks + queens;// non-pawn piece count (kings excluded)

    // Select a base bucket
    int base = 0;
    if (npp <= SearchConfig.NPP_LIGHT_THRESHOLD) {
      base = SearchConfig.MOVES_LEFT_LOW_MAT;// very low material
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
  }// if (!movesLeft)

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
    base = tl - tl / 5;// ~80% without floating-point (avoids narrowing)
  }
  else {
    // reduced by 10%
    base = tl - tl / 10;// ~90% without floating-point
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
    const auto deltaMs = std::llround( static_cast<long double>(timeLimit.count()) * (static_cast<long double>(f) - 1.0L));
    (void) extraTimeMs.fetch_add(deltaMs, std::memory_order_relaxed);
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Time adjustment: {} -> total budget {} (base {} + extra {})",
               str(milliseconds(deltaMs)),
               str(timeLimit + milliseconds(extraTimeMs.load(std::memory_order_relaxed))),
               str(timeLimit),
               str(milliseconds(extraTimeMs.load(std::memory_order_relaxed))));
  }
}

void Search::startTimer() {
  this->timerThread = std::thread([&] {
    startSearchTime = currentTime();
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Timer started with time limit of {} ms", str(timeLimit));
    // Busy-wait threshold for higher-precision tail (2-3ms)
    constexpr milliseconds busyWaitThreshold{3};
    while (true) {
      const auto now     = currentTime();
      const auto elapsed = now - startSearchTime;
      const auto budget  = timeLimit + milliseconds(extraTimeMs.load());
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
      this->stopSearchFlag = true;
      LOG__INFO(Logger::get().SEARCH_LOG, "Stop search by Timer after wall time: {} (time limit {} and extra time {})", str(currentTime() - startSearchTime), str(timeLimit), str(milliseconds(extraTimeMs.load())));
    }
  });
}

double Search::computeComplexityFactorFromMoves(const Position& p, const MoveList& legalMoves) {
  // Defaults chosen conservatively to avoid large swings.
  constexpr int pivotMoves = 30;  // neutral pivot
  constexpr double slope   = 0.01;// +/-1% per move relative to pivot
  constexpr double baseCap = 0.25;// baseline capture ratio
  constexpr double capW    = 0.50;// weight for (ratio - baseCap)
  constexpr double inChkB  = 0.10;// +10% when in check
  constexpr double minF    = 0.85;// min factor
  constexpr double maxF    = 1.30;// max factor


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

void Search::sendResult(const SearchResult& result) const {
  if (uciHandler) { uciHandler->sendResult(result.bestMove, result.ponderMove); }
}

void Search::sendIterationEndInfoToUci() {
  const nanoseconds& since = elapsedSince(startSearchTime);
  lastUciUpdateTime        = nowFast();

  // Use a copy of the initial position to extract PV with TT extension
  Position p            = position;
  const MoveList pvLine = extractPvWithTT(p);

  if (uciHandler) {
    uciHandler->sendIterationEndInfo(
      statistics.currentSearchDepth,
      statistics.currentExtraSearchDepth,
      statistics.currentBestRootMoveValue,
      nodesVisited,
      nps(nodesVisited, since),
      MILLISECONDS(since),
      pvLine);
    return;
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "depth {} seldepth {} value {} nodes {:L} nps {:L} time {:L} pv {}",
            statistics.currentSearchDepth,
            statistics.currentExtraSearchDepth,
            statistics.currentBestRootMoveValue.str(),
            nodesVisited,
            nps(nodesVisited, since),
            MILLISECONDS(since).count(),
            pvLine.str());
}

void Search::sendSearchUpdateToUci() {

  // to minimize performance impact we only check time every 1M nodes
  if (nodesVisited - lastUciUpdateNodes < 1'000'000) { return; }
  lastUciUpdateNodes = nodesVisited;

  // we only update every UCI_UPDATE_INTERVAL ns
  const uint64_t nowTime = nowFast();
  if (nowTime - lastUciUpdateTime < UCI_UPDATE_INTERVAL) { return; }
  lastUciUpdateTime = nowTime;

  // nps is calculated from the nodes and time since last update.
  // This might not be the same as the over all avg. nps which is shown
  // at the end of a search.
  const uint64_t nodesPerSec = nps(nodesVisited - npsNodes, nowTime - npsTime);
  npsTime                    = nowTime;
  npsNodes                   = nodesVisited;

  const int hashfull = tt->hashFull();

  const nanoseconds& since = elapsedSince(startSearchTime);

  if (uciHandler) {
    uciHandler->sendSearchUpdate(
      statistics.currentSearchDepth,
      statistics.currentExtraSearchDepth,
      nodesVisited,
      nodesPerSec,
      MILLISECONDS(since),
      hashfull);
    uciHandler->sendCurrentRootMove(statistics.currentRootMove, statistics.currentRootMoveIndex);
    uciHandler->sendCurrentLine(statistics.currentVariation);
    return;
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "depth {} seldepth {} nodes {:L} nps {:L} time {:L} hashful {:L}",
            statistics.currentSearchDepth,
            statistics.currentExtraSearchDepth,
            nodesVisited,
            nodesPerSec,
            MILLISECONDS(since).count(),
            hashfull);
}

void Search::sendAspirationResearchInfo(const std::string& boundString) {
  const nanoseconds& since = elapsedSince(startSearchTime);

  // Use a copy of the initial position to extract PV with TT extension
  Position p            = position;
  const MoveList pvLine = extractPvWithTT(p);

  if (uciHandler) {
    uciHandler->sendAspirationResearchInfo(
      statistics.currentSearchDepth,
      statistics.currentExtraSearchDepth,
      statistics.currentBestRootMoveValue,
      boundString,
      nodesVisited,
      nps(nodesVisited, since),
      MILLISECONDS(since),
      pvLine);
    return;
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "depth {} seldepth {} value {} {} nodes {:L} nps {:L} time {:L} pv {}",
            statistics.currentSearchDepth,
            statistics.currentExtraSearchDepth,
            statistics.currentBestRootMoveValue.str(),
            boundString,
            nodesVisited,
            nps(nodesVisited, since),
            MILLISECONDS(since).count(),
            pvLine.str());
}

MoveList Search::extractPvWithTT(Position& p) {
  MoveList result;

  // First, copy moves from the triangular PV table
  const int pvLen = pv.length();
  for (int i = 0; i < pvLen; ++i) {
    const Move move = pv(DEPTH_NONE, i);
    if (move == MOVE_NONE) break;
    result.push_back(move);
    p.doMove(move);
  }

  // Now extend using TT lookups
  // Limit to prevent infinite loops (e.g., from TT collisions)
  constexpr int maxExtension = MAX_DEPTH;
  int extended               = 0;

  while (extended < maxExtension) {
    const TT::Entry* entry = tt->probe(p.getZobristKey());
    if (!entry) break;

    const auto ttMove = static_cast<Move>(entry->move);
    if (ttMove == MOVE_NONE) break;

    // Verify the move is legal in current position
    // (TT entries can be stale due to collisions)
    if (!p.isLegalMove(ttMove)) break;

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
  const int totalMoves = pvLen + extended;
  for (int i = 0; i < totalMoves; ++i) {
    p.undoMove();
  }

  return result;
}

std::string Search::formatDetailedStats(
  const SearchResult& result,
  const SearchStats& stats) {

  std::ostringstream os;
  os.imbue(deLocale);

  const auto timeMs  = duration_cast<milliseconds>(result.time).count();
  const uint64_t nps = timeMs > 0 ? (result.nodes * 1000) / static_cast<uint64_t>(timeMs) : 0;

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
  os << "Book Move      : " << (result.bookMove ? "yes" : "no") << "\n";
  os << "TB Hit         : " << (result.tbHit ? "yes" : "no") << "\n";
  os << "Mate Found     : " << (result.mateFound ? "yes" : "no") << "\n";
  os << "PV             : " << result.pv.str() << "\n";

  os << "\n------------------- Terminal Nodes --------------------\n";
  os << "Checkmates     : " << stats.checkmates << "\n";
  os << "Stalemates     : " << stats.stalemates << "\n";
  os << "Leaf Positions : " << stats.leafPositionsEvaluated << "\n";
  os << "Evaluations    : " << stats.evaluations << "\n";

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
  os << "TT Hits        : " << stats.ttHit << "\n";
  os << "TT Misses      : " << stats.ttMiss << "\n";
  os << "TT Cuts        : " << stats.TtCuts << "\n";
  os << "TT No Cuts     : " << stats.TtNoCuts << "\n";
  os << "TT Move Used   : " << stats.TtMoveUsed << "\n";
  os << "No TT Move     : " << stats.NoTtMove << "\n";
  os << "Eval from TT   : " << stats.evalFromTT << "\n";

  os << "\n------------------- IID Stats -------------------------\n";
  os << "IID Searches   : " << stats.iidSearches << "\n";
  os << "IID Moves      : " << stats.iidMoves << "\n";

  os << "\n------------------- Re-search Stats -------------------\n";
  os << "Root PVS Re    : " << stats.rootPvsResearches << "\n";
  os << "PVS Researches : " << stats.pvsResearches << "\n";
  os << "ASP Researches : " << stats.aspirationResearches << "\n";
  os << "Best Move Chg  : " << stats.bestMoveChange << "\n";

  os << "\n------------------- Tablebase Stats -------------------\n";
  os << "TB Root Hits   : " << stats.tbRootHits << "\n";
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
  return formatDetailedStats(*lastSearchResult, statistics);
}
