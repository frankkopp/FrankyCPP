// FrankyCPP
// Copyright (c) 2018-2021 Frank Kopp
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
#include "SearchConfig.h"
#include "See.h"

#include <algorithm>
#include <chrono>

////////////////////////////////////////////////
///// CONSTRUCTORS

Search::Search() : Search(nullptr) {}

Search::Search(UciHandler* pUciHandler) {
  this->uciHandler = pUciHandler;
  this->tt         = std::make_unique<TT>(0);
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
  tt->clear();
  evaluator = std::make_unique<Evaluator>();
  history   = History{};
}

void Search::isReady() {
  initialize();
  sendReadyOk();
}

void Search::startSearch(const Position& p, SearchLimits sl) {
  // acquire init phase lock
  if (!initSemaphore.try_acquire()) { LOG__WARN(Logger::get().SEARCH_LOG, "Search init failed as another initialization is ongoing."); }

  // start search time
  startTime       = currentTime();
  startSearchTime = startTime;

  // move the received copy of position and search limits to instance variables
  this->position     = p;
  this->searchLimits = std::move(sl);

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

bool Search::isSearching() const {
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

void Search::resizeTT() {
  if (isSearching()) {
    const std::string msg = "Can't resize hash while searching.";
    sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
    return;
  }
  tt = std::make_unique<TT>(0);// clear the old TT (is a smart pointer and memory is freed)
  initialize();                // re-initialize
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
  stopSearchFlag    = false;
  hasResultFlag     = false;
  timeLimit         = milliseconds{};
  extraTimeMs       = 0;
  nodesVisited      = 0;
  statistics        = SearchStats{};
  lastUciUpdateTime = nowFast();
  npsTime           = lastUciUpdateTime;
  initialize();

  // set up and report search limits
  setupSearchLimits(position, searchLimits);

  // when not pondering and search is time controlled start timer
  if (searchLimits.timeControl && !searchLimits.ponder) { startTimer(); }

  // age tt entries
  if (tt->getMaxNumberOfEntries()) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table: Using TT: {}", tt->str());
    tt->ageEntries();
  }
  else { LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table: Not using TT."); }

  // Initialize ply-based data
  // move generators for each ply
  // pv move list for each ply
  // Each depth in search gets its own global
  // field to avoid object creation during search.
  for (int i = DEPTH_NONE; i < DEPTH_MAX; i++) {
    this->mg[i] = MoveGenerator{};
    if (SearchConfig::USE_HISTORY_COUNTER || SearchConfig::USE_HISTORY_MOVES) { this->mg[i].setHistoryData(&history); }
    pv[i].clear();
  }

  // release the init phase lock to signal the calling go routine
  // waiting in StartSearch() to return
  initSemaphore.release();

  // check for opening book move when we have a time-controlled game
  Move bookMove = MOVE_NONE;
  if (book && SearchConfig::USE_BOOK && searchLimits.timeControl) {
    // TODO: instead of a random book move we could select a book move based on
    //  some score and some variation (randomness)
    bookMove = book->getRandomMove(position.getZobristKey());
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Opening Book: Choosing book move {}", bookMove.str());
  }
  else { LOG__INFO(Logger::get().SEARCH_LOG, "Opening Book: Not using book."); }

  LOG__INFO(Logger::get().SEARCH_LOG, "Search using: PVS={} ASP={}", SearchConfig::USE_PVS, SearchConfig::USE_ASP);

  // If we have found a book-move an update result and omit search.
  // Otherwise, start search with iterative deepening.
  SearchResult searchResult{};
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
  searchResult.pv    = pv[0];
  searchResult.nodes = nodesVisited;

  // print stats to log
  LOG__INFO(Logger::get().SEARCH_LOG, "Search finished after {}", str(searchResult.time));
  LOG__INFO(Logger::get().SEARCH_LOG, "Search depth was {}({}) with {:L} nodes visited. NPS = {:L} nps", statistics.currentSearchDepth, statistics.currentExtraSearchDepth, nodesVisited, nps(nodesVisited, searchResult.time));
  LOG__DEBUG(Logger::get().SEARCH_LOG, "Search stats: {}", statistics.str());

  // print result to log
  if (searchLimits.mate && searchResult.mateFound) { LOG__INFO(Logger::get().SEARCH_LOG, "Mate in {} found: {}", searchLimits.mate, pv[0].at(0).str()); }
  LOG__INFO(Logger::get().SEARCH_LOG, "Search result: {}", searchResult.str());

  // save the result until overwritten by the next search
  lastSearchResult = searchResult;
  hasResultFlag    = true;

  // At the end of a search we send the result in any case even if
  // searched has been stopped.
  sendResult(searchResult);

  // clean up timer thread if necessary
  if (timerThread.joinable()) timerThread.join();

  // release the running semaphore after the search has ended
  isRunningSemaphore.release();
}

SearchResult Search::iterativeDeepening(Position& p) {
  SearchResult searchResult{};

  // check repetition and 50-moves rule
  if (checkDrawRepAnd50(p, 2)) {
    const std::string msg = this->searchLimits.ponder
                              ? "Ponder called on DRAW by Repetition or 50-moves-rule"
                              : "Search called on DRAW by Repetition or 50-moves-rule";
    sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
    searchResult.bestMoveValue = VALUE_DRAW;
    return searchResult;
  }

  // generate all legal root moves for the position
  rootMoves = *mg[0].generateLegalMoves(p, GenAll);

  // check if there are legal moves - if not, it's mate or stalemate
  if (rootMoves.empty()) {
    if (p.hasCheck()) {
      statistics.checkmates++;
      const std::string msg = this->searchLimits.ponder
                                ? "Ponder called on a check mate position"
                                : "Search called on a check mate position";
      sendString(msg);
      LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
      searchResult.bestMoveValue = -VALUE_CHECKMATE;
    }
    else {
      statistics.stalemates++;
      const std::string msg = this->searchLimits.ponder
                                ? "Ponder called on a stale mate position"
                                : "Search called on a stale mate position";
      sendString(msg);
      LOG__WARN(Logger::get().SEARCH_LOG, "{}", msg);
      searchResult.bestMoveValue = VALUE_DRAW;
    }
    return searchResult;
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

  // prepare max depth from search limits
  const int maxDepth = searchLimits.depth ? searchLimits.depth : DEPTH_MAX;

  // Max window search in preparation for aspiration window
  // is not needed yet
  constexpr Value alpha = VALUE_MIN;
  constexpr Value beta  = VALUE_MAX;
  Value bestValue       = VALUE_NONE;

  // ###########################################
  // ### BEGIN Iterative Deepening
  milliseconds lastIterationMs{0};
  uint64_t lastIterationNodes = 0;
  uint64_t prevIterationNodes = 0;
  for (auto iterationDepth = Depth{1}; iterationDepth <= maxDepth; ++iterationDepth) {

    // ===========================================
    // Before starting a new iteration, check if we have enough time left to likely complete it.
    if (searchLimits.timeControl && !searchLimits.ponder && iterationDepth > 1) {
      const nanoseconds sinceNs  = elapsedSince(startSearchTime);
      const milliseconds elapsed = MILLISECONDS(sinceNs);
      const milliseconds budget  = timeLimit + milliseconds(extraTimeMs.load());
      if (elapsed >= budget) {
        LOG__DEBUG(Logger::get().SEARCH_LOG, "Stop before iteration {}: time budget exhausted (elapsed {} >= budget {})",
                   iterationDepth, str(elapsed), str(budget));
        break;
      }
      const milliseconds remaining = budget - elapsed;

      // Estimate needed time for the next iteration based on last iteration nodes,
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
                                            ? static_cast<uint64_t>(lastIterationNodes * growth)
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
      const auto effectiveRemaining = milliseconds{
        static_cast<int64_t>(remaining.count() * rootComplexityFactor)};

      if (effectiveRemaining <= buffer || (needed.count() > 0 && effectiveRemaining < needed)) {
        LOG__DEBUG(Logger::get().SEARCH_LOG,
                   "Stop before iteration {}: effRemaining {} < needed {} (buffer {}, rootFactor {:.2f})",
                   iterationDepth, str(effectiveRemaining), str(needed), str(buffer), rootComplexityFactor);
        break;
      }
    }
    // ===========================================

    // update search counter
    nodesVisited++;

    // update depth statistics
    statistics.currentIterationDepth = iterationDepth;
    statistics.currentSearchDepth    = statistics.currentIterationDepth;
    if (statistics.currentExtraSearchDepth < statistics.currentIterationDepth) { statistics.currentExtraSearchDepth = statistics.currentIterationDepth; }

    // reset perft counter for last depth to
    statistics.perftNodeCount = 0;

    // Measure iteration duration
    const TimePoint iterationStartTime = currentTime();
    const uint64_t iterStartNodes      = nodesVisited;

    // ###########################################
    // Start actual alpha beta search
    // ASPIRATION SEARCH
    if (SearchConfig::USE_ASP && iterationDepth > 3) { bestValue = aspirationSearch(p, iterationDepth, bestValue); }
    // PVS SEARCH (or pure ALPHA BETA when PVS deactivated)
    else { bestValue = rootSearch(p, iterationDepth, alpha, beta); }
    // ###########################################

    // record iteration duration for next pre-check
    lastIterationMs = MILLISECONDS(currentTime() - iterationStartTime);

    // record node counts for growth prediction
    prevIterationNodes = lastIterationNodes;
    lastIterationNodes = nodesVisited - iterStartNodes;

    assert((bestValue == pv[0].at(0).value() || stopSearchFlag) && "bestValue should be equal value of pv[0].at(0)");

    // Conservative volatility detector: big evaluation swings between consecutive iterations
    if (!addedVolatilityExtraTime && !isTimeAlmostUp()) {
      const Value currBest = pv[0].at(0).value();
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

    // if mate search check if we found a mate within the mate limit
    if (searchLimits.mate && abs(pv[0].at(0).value()) >= VALUE_CHECKMATE_THRESHOLD && searchLimits.mate * 2 - 1 == VALUE_CHECKMATE - pv[0].at(0).value()) {
      searchResult.mateFound = true;
      break;
    }

    // Check if we need to stop.
    // Doing this after the first iteration ensures that
    // we have done at least one complete search and have
    // a pv (best) move.
    // If we only have one move to play also stop the search
    if (!stopConditions() && rootMoves.size() > 1) {
      // sort root moves for the next iteration
      std::ranges::stable_sort(rootMoves, moveValueGreaterComparator());
      statistics.currentBestRootMove      = pv[0].at(0);
      statistics.currentBestRootMoveValue = pv[0].at(0).value();
      assert(pv[0].at(0) == rootMoves.at(0) && "Best root move should be equal to pv[0].at(0)");
      // update UCI GUI
      sendIterationEndInfoToUci();
    }
    else { break; }
  }
  // ### END OF Iterative Deepening
  // ###########################################

  // update searchResult
  // the best move is pv[0][0] - we need to make sure this array entry exists at this time
  // the best value is pv[0][0].valueOf
  searchResult.bestMove      = pv[0].at(0).stripped();
  searchResult.bestMoveValue = pv[0].at(0).value();
  searchResult.depth         = statistics.currentIterationDepth;
  searchResult.extraDepth    = statistics.currentExtraSearchDepth;
  searchResult.bookMove      = false;

  // see if we have a move we could ponder on
  if (pv[0].size() > 1) { searchResult.ponderMove = pv[0].at(1).stripped(); }
  else {
    // we do not have a ponder-move in the pv-list,
    // so let's check the TT
    if (SearchConfig::USE_TT) {
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
    if (checkDrawRepAnd50(p, 2) || mg[0].generateLegalMoves(p, GenAll)->empty()) {
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
      // FAIL LOW - decrease upper bound
      sendAspirationResearchInfo("upperbound");
      // add some extra time because of fail low
      // we might have found a strong opponent's move
      addExtraTime(1.3);
      // if we fail low tests, it is best to immediately open up the window full
      // If time is almost up, don't expand; return current value
      if (isTimeAlmostUp()) { return value; }
      alpha = VALUE_MIN;
      // Alternatively we could do steps as well
      // alpha = Max(bestValue-aspirationSteps[i], ValueMin)
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

    if (checkDrawRepAnd50(p, 2)) { value = VALUE_DRAW; }
    else {
      constexpr Depth ply{1};
      // ///////////////////////////////////////////////////////////////////
      // PVS
      // First move in a node is an assumed PV and searched with full search window
      if (!SearchConfig::USE_PVS || i == 0) {
        value = -search(p, depth - 1, ply, -beta, -alpha, PV, Do_Null_Move);
      }
      else {
        // Null window search after the initial PV search.
        value = -search(p, depth - 1, ply, -alpha - 1, -alpha, NonPV, Do_Null_Move);
        // If this move improved alpha without exceeding beta we do a proper full window
        // search to get an accurate score.
        if (value > alpha && value < beta && !stopConditions() && !isTimeAlmostUp()) {
          statistics.rootPvsResearches++;
          value = -search(p, depth - 1, ply, -beta, -alpha, PV, Do_Null_Move);
        }
      }
      // ///////////////////////////////////////////////////////////////////
    }

    statistics.currentVariation.pop_back();
    p.undoMove();

    // we want to do at least one complete search with depth 1
    // After that we can stop any time - any new best moves will
    // have been stored in pv[0]
    if (stopConditions() && depth > 1) { return VALUE_NONE; }

    // set the value into he root move to later be able to sort
    // root moves according to value
    moveRef.setValue(value);

    // Did we find a new best move?
    // For the first move with a full window (alpha=-inf)
    // this is always the case.
    if (value > bestNodeValue) {
      bestNodeValue = value;
      // we have a new best move and pv[0][0] - store pv+1 to pv
      savePV(moveRef, pv[1], pv[0]);
      statistics.bestMoveChange++;
      if (value > alpha) {
        // fail high in root only when using aspiration search
        if (value >= beta && SearchConfig::USE_ALPHABETA) {
          statistics.betaCuts++;
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

Value Search::search(Position& p, const Depth depth, const Depth ply, Value alpha, Value beta, const Node_Type isPv, const Do_Null doNull) {
  //  LOG__DEBUG(Logger::get().SEARCH_LOG, "Search {} {} {}", depth, ply, str(statistics.currentVariation));

  // Enter quiescence search when depth == 0 or max ply has been reached
  if (depth == 0 || ply >= MAX_DEPTH) { return qsearch(p, ply, alpha, beta, isPv); }

  // check if search should be stopped
  if (stopConditions()) { return VALUE_NONE; }

  // Mate Distance Pruning
  // Did we already find a shorter mate then ignore
  // this one.
  if (SearchConfig::USE_MDP) {
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

  // TT Lookup
  // Results of searches are stored in the TT to be used to
  // avoid searching positions several times. If a position
  // is stored in the TT we retrieve a pointer to the entry.
  // We use the stored move as a best move from previous searches
  // and search it first (through setting PV move in move gen).
  // If we have a value from a similar or deeper search we check
  // if the value is usable. Exact values mean that the previously
  // stored result already was a precise result, and we do not
  // need to search the position again. We can stop searching
  // this branch and return the value.
  // Alpha or Beta entries will only be used if they improve
  // the current values.
  if (SearchConfig::USE_TT) {
    if (const TT::Entry* ttEntryPtr = tt->probe(p.getZobristKey())) {
      // tt hit
      statistics.ttHit++;
      ttMove = static_cast<Move>(ttEntryPtr->move);
      if (ttEntryPtr->depth >= depth) {
        const Value ttValue = valueFromTt(ttEntryPtr->value, ply);
        if (SearchConfig::USE_TT_VALUE
            && ttValue.isValid()
            && (ttEntryPtr->type == EXACT
                || (ttEntryPtr->type == ALPHA && ttValue <= alpha)
                || (ttEntryPtr->type == BETA && ttValue >= beta))) {
          // get PV line from tt as we prune here
          // and wouldn't have one otherwise
          getPvLine(p, pv[ply], depth);
          statistics.TtCuts++;
          return ttValue;
        }
        statistics.TtNoCuts++;
      }
      // if we have a static eval stored, we can reuse it
      if (SearchConfig::USE_EVAL_TT && ttEntryPtr->eval != VALUE_NONE) {
        statistics.evalFromTT++;
        staticEval = ttEntryPtr->eval;
      }
    }
    else { statistics.ttMiss++; }
  }// use TT

  const bool hasCheck = p.hasCheck();

  // get an evaluation for the position
  if (!hasCheck && staticEval == VALUE_NONE) {
    staticEval = evaluate(p);
    // Storing this value might save us calls to eval on the same position.
    if (SearchConfig::USE_TT && SearchConfig::USE_EVAL_TT) {
      storeTt(p, DEPTH_NONE, DEPTH_NONE, MOVE_NONE, VALUE_NONE, NONE, staticEval);
    }
  }

  // Razoring from Stockfish
  // When static eval is well below alpha at the last node
  // jump directly into qsearch
  if (SearchConfig::USE_RAZORING
      && depth == 1
      && staticEval != VALUE_NONE
      && staticEval <= alpha - SearchConfig::RAZOR_MARGIN) {
    statistics.razorings++;
    return qsearch(p, ply, alpha, beta, PV);
  }

  // Reverse Futility Pruning, (RFP, Static Null Move Pruning)
  // https://www.chessprogramming.org/Reverse_Futility_Pruning
  // Anticipate likely alpha low in the next ply by a beta cut
  // off before making and evaluating the move
  if (SearchConfig::USE_RFP
      && doNull
      && depth <= 3
      && !isPv
      && !hasCheck) {
    const Value margin = SearchConfig::RFP_MARGIN[depth];
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
  if (SearchConfig::USE_NMP) {
    if (doNull
        && !isPv
        && depth >= SearchConfig::NMP_DEPTH
        && p.getMaterialNonPawn(us) > 0// to reduce risk of zugzwang
        && !hasCheck) {
      // possible other criteria: eval > beta

      // determine depth reduction
      // ICCA Journal, Vol. 22, No. 3
      // Ernst A. Heinz, Adaptive Null-Move Pruning, postscript
      // http://people.csail.mit.edu/heinz/ps/adpt_null.ps.gz
      Depth r = SearchConfig::NMP_REDUCTION;
      if (depth > 8 || (depth > 6 && p.getGamePhase() >= 3)) { ++r; }
      Depth newDepth = depth - r - 1;
      // double check that depth does not get negative
      if (newDepth < 0) { newDepth = DEPTH_NONE; }

      // do null move search
      p.doNullMove();
      nodesVisited++;
      Value nValue = -search(p, newDepth, ply + 1, -beta, -beta + 1, NonPV, No_Null_Move);
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
      else if (nValue < -VALUE_CHECKMATE_THRESHOLD) {
        // the player did not move and got mated ==> mate threat
        matethreat = true;
      }

      // if the value is higher than beta even after not making
      // a move it is not worth searching as it will very likely
      // be above beta if we make a move
      if (nValue >= beta) {
        statistics.nullMoveCuts++;
        // Store TT
        if (SearchConfig::USE_TT) { storeTt(p, depth, ply, ttMove, nValue, BETA, staticEval); }
        return nValue;
      }
    }
  }

  // Internal Iterative Deepening (IID)
  // https://www.chessprogramming.org/Internal_Iterative_Deepening
  // Used when no best move from the tt is available from a previous
  // searches. IID is used to find a good move to search first by
  // searching the current position to a reduced depth, and using
  // the best move of that search as the first move at the real depth.
  // Does not make a big difference in search tree size when move
  // order already is good.
  if (SearchConfig::USE_IID) {
    if (depth >= SearchConfig::IID_DEPTH
        && !ttMove// no move from TT
        && doNull
        && isPv) {// avoid in null move search

      // get the new depth and make sure it is >0
      auto newDepthIid = depth - SearchConfig::IID_REDUCTION;
      if (newDepthIid < 0) { newDepthIid = DEPTH_NONE; }

      // do the actual reduced search only if we have time left
      if (!isTimeAlmostUp()) {
        search(p, newDepthIid, ply, alpha, beta, isPv, doNull);
        statistics.iidSearches++;

        // check if we should stop the search
        if (stopConditions()) { return VALUE_NONE; }

        // get the best move from the reduced search if available
        if (!pv[ply].empty()) {
          statistics.iidMoves++;
          ttMove = pv[ply][0].stripped();
        }
      }
    }
  }

  // reset search
  // !important to do this after IID!
  const auto myMg = &mg[ply];
  myMg->resetOnDemand();
  pv[ply].clear();

  // PV Move Sort
  // When we received a best move for the position from the
  // TT or IID we set it as PV move in the move-gen so it will
  // be searched first.
  if (SearchConfig::USE_TT_PV_MOVE_SORT && ttMove != MOVE_NONE) {
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
    const Square from     = move.from();
    const Square to       = move.to();
    const bool givesCheck = p.givesCheck(move);

    // prepare newDepth
    Depth newDepth  = depth - DEPTH_ONE;
    Depth lmrDepth  = newDepth;
    Depth extension = DEPTH_NONE;

    // Here we try some search extensions. This has to be done
    // very carefully as it usually is more effective to prune
    // than to extend.
    if (SearchConfig::USE_EXTENSIONS) {
      // The check extensions is a bit redundant as our QS search
      // searches all moves anyway when in check. But with this
      // extension we hope to profit from using the pruning
      // of the normal search which are not available in
      // qsearch.
      if (SearchConfig::USE_CHECK_EXT && givesCheck) {
        statistics.checkExtension++;
        extension = DEPTH_ONE;
      }

      // If we have found a mate threat during Null Move Search
      // we extend normal search by one ply to try to find
      // a way out.
      // Deactivated in config as this grows the search tree
      // too much.
      if (SearchConfig::USE_THREAT_EXT && matethreat) {
        statistics.threatExtension++;
        extension = DEPTH_ONE;
      }

      // With this turned off we still can use extension to
      // at least avoid reductions for these moves.
      if (SearchConfig::USE_EXT_ADD_DEPTH) {
        newDepth += extension;
      }
    }

    // ///////////////////////////////////////////////////////
    // Forward Pruning
    // FP will only be done when the move is not
    // interesting - no check, no capture, etc.
    if (!isPv
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
      if (SearchConfig::USE_FP && depth < 7) {
        const auto futilityMargin = SearchConfig::FP_MARGIN[depth];
        if (staticEval + moveGain + futilityMargin <= alpha) {
          if (staticEval + moveGain > bestNodeValue) { bestNodeValue = staticEval + moveGain; }
          statistics.fpPrunings++;
          continue;
        }
      }

      // LMP - Late Move Pruning
      // aka Move Count Based Pruning
      if (SearchConfig::USE_LMP) {
        if (movesSearched >= SearchConfig::LMP_MOVES[(depth > 15 ? 15 : depth)]) {
          statistics.lmpCuts++;
          continue;
        }
      }

      // LMR
      // Late Move Reduction assumes that later moves a rarely
      // exceeding alpha and therefore the search is reduced in
      // depth. This is in effect a soft transition into
      // quiescence search as we usually try the pv move and
      // capturing moves first. In quiescence only capturing
      // moves are searched anyway.
      // newDepth is the "standard" new depth (depth - 1)
      // lmrDepth is set to newDepth and only reduced
      // if conditions apply.
      if (SearchConfig::USE_LMR) {
        // compute reduction from depth and move searched
        if (depth >= SearchConfig::LMR_MIN_DEPTH && movesSearched >= SearchConfig::LMR_MIN_MOVES) {
          if (depth >= 32 || movesSearched >= 64) {
            lmrDepth -= static_cast<Depth>(SearchConfig::LMR_REDUCTION[31][63]);
          }
          else {
            lmrDepth -= static_cast<Depth>(SearchConfig::LMR_REDUCTION[depth][movesSearched]);
          }
          statistics.lmrReductions++;
        }
        // make sure not to become negative
        if (lmrDepth < 0) { lmrDepth = DEPTH_NONE; }
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
      // ///////////////////////////////////////////////////////////////////
      // PVS
      // First move in Node will be searched with the full window. Due to move
      // ordering we assume this is the PV. Every other move is searched with
      // a null window as we only try to prove that the move is bad (<alpha)
      // or that the move is too good (>beta). If this prove fails we need
      // to research the move again with a full window.
      // https://www.chessprogramming.org/Principal_Variation_Search
      if (!SearchConfig::USE_PVS || movesSearched == 0) {
        value = -search(p, newDepth, ply + 1, -beta, -alpha, PV, Do_Null_Move);
      }
      else {
        // Null window search after the initial PV search.
        // As depth we use a potentially reduced depth if Late Move Reduction
        // conditions have been met above.
        value = -search(p, lmrDepth, ply + 1, -alpha - 1, -alpha, NonPV, Do_Null_Move);
        // If this move improved alpha without exceeding beta we do a proper full window
        // search to get an accurate score.
        // Without LMR we check for value > alpha && value < beta
        // With LMR we re-search when value > alpha
        if (value > alpha && !stopConditions() && !isTimeAlmostUp()) {
          // did we actually have a LMR reduction?
          if (lmrDepth < newDepth) {
            statistics.lmrResearches++;
            value = -search(p, newDepth, ply + 1, -beta, -alpha, PV, Do_Null_Move);
          }
          else if (value < beta) {
            statistics.pvsResearches++;
            value = -search(p, newDepth, ply + 1, -beta, -alpha, PV, Do_Null_Move);
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
    if (stopConditions()) { return VALUE_NONE; }

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
        // If we found a move that is better or equal than beta
        // this means that the opponent can/will avoid this
        // position altogether, so we can stop search this node.
        // We will not know if our best move is really the
        // best move or how good it really is (value is a lower bound)
        // as we cut off the rest of the search of the node here.
        // We will save the move as a killer to be able to search it
        // earlier in another node of the ply.
        if (value >= beta && SearchConfig::USE_ALPHABETA) {
          // Count beta cuts
          statistics.betaCuts++;
          // Count beta cuts on first move
          if (movesSearched == 1) { statistics.betaCuts1st++; }
          // store move which caused a beta cut off in this ply
          if (SearchConfig::USE_KILLER_MOVES && !p.isCapturingMove(move)) { myMg->storeKiller(move); }
          // counter for moves which caused a beta cut off
          // we use 1 << depth as an increment to favor deeper searches
          // a more repetitions
          if (SearchConfig::USE_HISTORY_COUNTER) { history.historyCount[us][from][to] += 1LL << depth; }
          // store a successful counter move to the previous opponent move
          if (SearchConfig::USE_HISTORY_MOVES) {
            const Move lastMove = p.getLastMove();
            if (lastMove != MOVE_NONE) { history.counterMoves[lastMove.from()][lastMove.to()] = move; }
          }
          ttType = BETA;
          break;
        }
        // We found a move between alpha and beta which means we
        // really have found the best move so far in the ply which
        // can be forced (opponent can't avoid it).
        savePV(move, pv[ply + 1], pv[ply]);

        // We raise alpha so the successive searches in this ply
        // need to find even better moves or dismiss the moves.
        alpha  = value;
        ttType = EXACT;
      }
    }
    // no beta cutoff - decrease historyCounter for the move
    // we decrease it by only half the increase amount
    if (SearchConfig::USE_HISTORY_COUNTER) {
      history.historyCount[us][from][to] -= 1LL << depth;
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
  if (SearchConfig::USE_TT) { storeTt(p, depth, ply, bestNodeMove, bestNodeValue, ttType, staticEval); }

  return bestNodeValue;
}

Value Search::qsearch(Position& p, const Depth ply, Value alpha, Value beta, const Node_Type isPv) {
  //  LOG__DEBUG(Logger::get().SEARCH_LOG, "QSearch {} {}", ply, str(statistics.currentVariation));

  if (statistics.currentExtraSearchDepth < ply) { statistics.currentExtraSearchDepth = ply; }

  // if we have deactivated qsearch or we have reached our maximum depth
  // we evaluate the position and return the value
  if (!SearchConfig::USE_QUIESCENCE || ply >= MAX_DEPTH) {
    statistics.perftNodeCount++;
    return evaluate(p);
  }

  // check if search should be stopped
  if (stopConditions()) { return VALUE_NONE; }

  // Mate Distance Pruning
  if (SearchConfig::USE_MDP) {
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

  // TT Lookup
  if (SearchConfig::USE_TT && SearchConfig::USE_QS_TT) {
    if (const TT::Entry* ttEntryPtr = tt->probe(p.getZobristKey())) {
      // tt hit
      statistics.ttHit++;
      ttMove              = static_cast<Move>(ttEntryPtr->move);
      const Value ttValue = valueFromTt(ttEntryPtr->value, ply);
      if (ttValue.isValid() && (ttEntryPtr->type == EXACT || (ttEntryPtr->type == ALPHA && ttValue <= alpha) || (ttEntryPtr->type == BETA && ttValue >= beta)) && SearchConfig::USE_TT_VALUE) {
        statistics.TtCuts++;
        return ttValue;
      }
      // if we have a static eval stored we can reuse it
      if (SearchConfig::USE_EVAL_TT && ttEntryPtr->eval != VALUE_NONE) {
        statistics.evalFromTT++;
        staticEval = ttEntryPtr->eval;
      }
    }
    else { statistics.ttMiss++; }
  }// use TT

  const bool hasCheck = p.hasCheck();

  // if in check we simply do a normal search (all moves) in qsearch
  if (!hasCheck) {
    // get an evaluation for the position
    if (staticEval == VALUE_NONE) { staticEval = evaluate(p); }
    // Quiescence StandPat
    // Use evaluation as a standing pat (lower bound)
    // https://www.chessprogramming.org/Quiescence_Search#Standing_Pat
    // Assumption is that there is at least on move which would improve the
    // current position. So if we are already >beta we don't need to look at it.
    if (SearchConfig::USE_QS_STANDPAT_CUT && staticEval > alpha) {
      if (staticEval >= beta) {
        statistics.standpatCuts++;
        // Storing this value might save us calls to eval on the same position.
        if (SearchConfig::USE_TT && SearchConfig::USE_QS_TT && SearchConfig::USE_EVAL_TT) { storeTt(p, DEPTH_NONE, ply, MOVE_NONE, VALUE_NONE, NONE, staticEval); }
        return staticEval;
      }
      alpha = staticEval;
    }
    bestNodeValue = staticEval;
  }

  // reset search
  const auto myMg = &mg[ply];
  myMg->resetOnDemand();
  pv[ply].clear();

  // PV Move Sort
  if (SearchConfig::USE_TT_PV_MOVE_SORT && ttMove != MOVE_NONE) {
    statistics.TtMoveUsed++;
    myMg->setPV(ttMove);
  }
  else { statistics.NoTtMove++; }

  // prepare move loop
  Value value;
  Move move;
  int movesSearched = 0;// to detect mate situations

  // when in check generate all moves
  const GenMode genMode = hasCheck ? GenAll : GenNonQuiet;

  // ///////////////////////////////////////////////////////
  // MOVE LOOP
  while ((move = myMg->getNextPseudoLegalMove(p, genMode, hasCheck)) != MOVE_NONE) {
    const Square from     = move.from();
    const Square to       = move.to();
    const bool givesCheck = p.givesCheck(move);

    // Forward Pruning
    // FP will only be done when the move is not
    // interesting - no check, no capture, etc.
    if (SearchConfig::USE_QFP
        && !isPv
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

    // reduce number of moves searched in quiescence
    // by looking at good captures only
    if (!hasCheck && !goodCapture(p, move)) { continue; }

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
    if (checkDrawRepAnd50(p, 2)) { value = VALUE_DRAW; }
    else { value = -qsearch(p, ply + 1, -beta, -alpha, isPv); }

    movesSearched++;
    statistics.currentVariation.pop_back();
    p.undoMove();
    // UNDO MOVE
    // ///////////////////////////////////////////////////////

    // check if we should stop the search
    if (stopConditions()) { return VALUE_NONE; }

    // See search function above for documentation
    if (value > bestNodeValue) {
      bestNodeValue = value;
      bestNodeMove  = move;
      if (value > alpha) {
        if (value >= beta && SearchConfig::USE_ALPHABETA) {
          statistics.betaCuts++;
          if (movesSearched == 1) { statistics.betaCuts1st++; }
          if (SearchConfig::USE_KILLER_MOVES && !p.isCapturingMove(move)) { myMg->storeKiller(move); }
          if (SearchConfig::USE_HISTORY_COUNTER) { history.historyCount[us][from][to] += 1 << 1; }
          if (SearchConfig::USE_HISTORY_MOVES) {
            const Move lastMove = p.getLastMove();
            if (lastMove != MOVE_NONE) { history.counterMoves[lastMove.from()][lastMove.to()] = move; }
          }
          ttType = BETA;
          break;
        }
        savePV(move, pv[ply + 1], pv[ply]);
        alpha  = value;
        ttType = EXACT;
      }
    }
    if (SearchConfig::USE_HISTORY_COUNTER) {
      history.historyCount[us][from][to] -= 1 << 1;
      if (history.historyCount[us][from][to] < 0) { history.historyCount[us][from][to] = 0; }
    }
  }
  // MOVE LOOP
  // ///////////////////////////////////////////////////////

  // If we did not have at least one legal move
  // then we might have a mate or stalemate
  if (movesSearched == 0 && !stopConditions()) {
    // if we have a mate we had a check before and therefore
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
  if (SearchConfig::USE_TT && SearchConfig::USE_QS_TT) { storeTt(p, DEPTH_ONE, ply, bestNodeMove, bestNodeValue, ttType, staticEval); }

  return bestNodeValue;
}

inline Value Search::evaluate(const Position& p) {
  statistics.leafPositionsEvaluated++;
  statistics.evaluations++;
  return evaluator->evaluate(p);
}

bool Search::goodCapture(Position& p, const Move move) const {
  if (SearchConfig::USE_QS_SEE) {
    // Check SEE score of higher-value pieces to low-value pieces
    return See::see(p, move) > 0;
  }
  return
    // all pawn captures - they never loose material
    // typeOf(position.getPiece(getFromSquare(move))) == PAWN

    // Lower value piece captures higher value piece
    // With a margin to also look at Bishop x Knight
    (valueOf(position.getPiece(move.from())) + 50) < valueOf(position.getPiece(move.to()))

    // all recaptures should be looked at
    || (position.getLastMove() != MOVE_NONE && position.getLastCapturedPiece() != PIECE_NONE && position.getLastMove().to() == move.to())

    // undefended pieces captures are good
    // If the defender is "behind" the attacker this will not be recognized
    // here This is not too bad as it only adds a move to qsearch which we
    // could otherwise ignore
    || !position.isAttacked(move.to(), ~position.getNextPlayer());
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

void Search::savePV(const Move move, MoveList& src, MoveList& dest) {
  dest.clear();
  dest.push_back(move);
  dest.insert(dest.end(), src.begin(), src.end());
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


void Search::getPvLine(Position& p, MoveList& pvList, const Depth depth) const {
  // Recursion-less reading the chain of pv moves
  pvList.clear();
  int counter  = 0;
  auto ttMatch = tt->getMatch(p.getZobristKey());
  while (ttMatch != nullptr && ttMatch->move != MOVE_NONE && counter < depth) {
    pvList.push_back(static_cast<Move>(ttMatch->move));
    p.doMove(static_cast<Move>(ttMatch->move));
    counter++;
    ttMatch = tt->getMatch(p.getZobristKey());
  }
  for (int i = 0; i < counter; ++i) { p.undoMove(); }
}

void Search::initialize() {
  LOG__INFO(Logger::get().SEARCH_LOG, "Search initialization.");
  // init opening book
  if (SearchConfig::USE_BOOK) {
    if (!book) {
      // only initialize once
      if (!std::filesystem::exists(SearchConfig::BOOK_PATH)) {
        const std::string message = std::format("Opening Book '{}' not found. Disabling book usage.", SearchConfig::BOOK_PATH);
        LOG__ERROR(Logger::get().BOOK_LOG, "{}", message);
        SearchConfig::USE_BOOK = false;
      }
      else {
        book = std::make_unique<OpeningBook>(SearchConfig::BOOK_PATH, SearchConfig::BOOK_TYPE);
        book->initialize();
      }
    }
  }
  else { LOG__INFO(Logger::get().SEARCH_LOG, "Opening Book disabled in configuration"); }

  // init transposition table
  if (SearchConfig::USE_TT) {
    if (tt->getMaxNumberOfEntries() == 0) {
      // only initialize once
      tt = std::make_unique<TT>(SearchConfig::TT_SIZE_MB);
    }
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table disabled in configuration");
    tt = std::make_unique<TT>(0);
  }

  // init evaluator
  if (!evaluator) {
    // only initialize once
    evaluator = std::make_unique<Evaluator>();
  }
}

bool Search::stopConditions() {
  if (stopSearchFlag) return true;
  if (searchLimits.nodes > 0 && nodesVisited >= searchLimits.nodes) { stopSearchFlag = true; }
  return stopSearchFlag;
}

bool Search::checkDrawRepAnd50(const Position& p, const int numberOfRepetitions) {
  return p.checkRepetitions(numberOfRepetitions) || p.getHalfMoveClock() >= 100;
}

void Search::setupSearchLimits(const Position& p, SearchLimits& sl) {
  if (sl.infinite) { LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Infinite"); }
  if (sl.ponder) { LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Ponder"); }
  if (sl.mate > 0) { LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Mate in {}", sl.mate); }
  if (sl.timeControl) {
    timeLimit   = setupTimeControl(p, sl);
    extraTimeMs = 0;
    if (sl.moveTime.count()) { LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Time Controlled: Time per Move {}", str(sl.moveTime)); }
    else {
      LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Time Controlled: White = {} (inc {}) Black = {} (inc {}) Moves to go: {}",
                str(sl.whiteTime), str(sl.whiteInc), str(sl.blackTime), str(sl.blackInc), sl.movesToGo);
      LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Time limit: {}", str(timeLimit));
    }
    if (sl.ponder) { LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Ponder - time control postponed until ponderhit received"); }
  }
  else { LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: No time control"); }
  if (sl.depth) { LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Depth limited  : {}", sl.depth); }
  if (sl.nodes) { LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Nodes limited  : {}", sl.nodes); }
  if (!sl.moves.empty()) { LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Moves limited  : {}", sl.moves.str()); }
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

milliseconds Search::setupTimeControl(const Position& position, const SearchLimits& limits) {
  if (limits.moveTime.count()) {
    // Search mode time per move

    // we need a little room for executing the code
    const milliseconds duration = limits.moveTime - milliseconds{SearchConfig::MOVE_OVERHEAD_MS};
    // if the duration is now negative return the original value and issue a warning
    if (duration.count() < 0) {
      LOG__WARN(Logger::get().SEARCH_LOG, "Very short move time: {} ms", limits.moveTime.count());
      return limits.moveTime;
    }
    // In fixed movetime mode do not scale by complexity; just use the adjusted duration.
    return duration;
  }

  // Search mode is remaining time - estimated time per move

  // Improved moves-left model using phase/material buckets and repetition risk.
  int movesLeft = limits.movesToGo;
  if (!movesLeft) {
    // Derive game phase and material features
    const double phase = position.getGamePhaseFactor();// ~1.0 opening/mid, ~0.0 endgame

    // Count non-pawn pieces across both sides (KNIGHT/BISHOP/ROOK/QUEEN)
    auto countPieces = [&](const PieceType pt) -> int {
      return position.getPieceBb(WHITE, pt).popcount() + position.getPieceBb(BLACK, pt).popcount();
    };
    const int knights = countPieces(KNIGHT);
    const int bishops = countPieces(BISHOP);
    const int rooks   = countPieces(ROOK);
    const int queensW = position.getPieceBb(WHITE, QUEEN).popcount();
    const int queensB = position.getPieceBb(BLACK, QUEEN).popcount();
    const int queens  = queensW + queensB;
    const int npp     = knights + bishops + rooks + queens;// non-pawn piece count (kings excluded)

    // Select a base bucket
    int base;
    if (npp <= SearchConfig::NPP_LIGHT_THRESHOLD) {
      base = SearchConfig::MOVES_LEFT_LOW_MAT;// very low material
    }
    else if (queens == 0) {
      // Queenless middlegames/endgames tend to resolve faster
      base = npp <= SearchConfig::NPP_LIGHT_THRESHOLD + 2
               ? SearchConfig::MOVES_LEFT_LOW_MAT
               : SearchConfig::MOVES_LEFT_QUEENLESS;
    }
    else if (phase >= 0.66
             || npp >= SearchConfig::NPP_HEAVY_THRESHOLD) {
      base = SearchConfig::MOVES_LEFT_OPENING;
    }
    else if (phase <= 0.33) {
      base = SearchConfig::MOVES_LEFT_ENDGAME;
    }
    else {
      base = SearchConfig::MOVES_LEFT_MIDGAME;
    }

    // Adjust for repetition/50-move risk
    if (position.getHalfMoveClock() >= SearchConfig::REPETITION_HMC_HIGH) {
      base -= SearchConfig::REPETITION_RISK_PENALTY;
    }

    // Clamp
    base = std::clamp(base, SearchConfig::MOVES_LEFT_MIN_CLAMP, SearchConfig::MOVES_LEFT_MAX_CLAMP);

    movesLeft = base;
    LOG__DEBUG(Logger::get().SEARCH_LOG,
               "TimeCtl: movesLeft={} (phase {:.2f}, npp {}, queens {}), hmc {}",
               movesLeft, phase, npp, queens, position.getHalfMoveClock());
  }

  // time left for current player
  milliseconds timeLeft;
  if (position.getNextPlayer()) { timeLeft = limits.blackTime + (movesLeft * limits.blackInc); }
  else { timeLeft = limits.whiteTime + (movesLeft * limits.whiteInc); }
  // estimate time per move
  const auto tl = static_cast<milliseconds>(timeLeft.count() / movesLeft);
  // tiny fixed reserve to reduce micro overshoots (remaining-time mode only)
  const milliseconds reserve{SearchConfig::MOVE_OVERHEAD_MS};
  // account for runtime of our code
  milliseconds base;
  if (tl.count() < 100) {
    // limits for very short available time reduced by another 20%
    base = static_cast<milliseconds>(static_cast<uint64_t>(0.8 * tl.count()));
  }
  else {
    // reduced by 10%
    base = static_cast<milliseconds>(static_cast<uint64_t>(0.9 * tl.count()));
  }
  // apply reserve
  base = base > reserve ? (base - reserve) : base;

  // Complexity-aware weighting
  const double factor = computeComplexityFactorQuick(position);
  const auto weighted = milliseconds{static_cast<int64_t>(base.count() * factor)};
  return weighted;
}

void Search::addExtraTime(const double f) {
  if (searchLimits.timeControl && !searchLimits.moveTime.count()) {
    const auto deltaMs = static_cast<int64_t>(timeLimit.count() * (f - 1.0));
    (void) extraTimeMs.fetch_add(deltaMs, std::memory_order_relaxed);
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Time added/reduced by {} to {} ", str(milliseconds(deltaMs)), str(timeLimit + milliseconds(extraTimeMs.load(std::memory_order_relaxed))));
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
  if (total <= 0) return 1.0;

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
  if (uciHandler) {
    uciHandler->sendIterationEndInfo(
      statistics.currentSearchDepth,
      statistics.currentExtraSearchDepth,
      statistics.currentBestRootMoveValue,
      nodesVisited,
      nps(nodesVisited, since),
      MILLISECONDS(since),
      pv[0]);
    return;
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "depth {} seldepth {} value {} nodes {:L} nps {:L} time {:L} pv {}",
            statistics.currentSearchDepth,
            statistics.currentExtraSearchDepth,
            statistics.currentBestRootMoveValue.str(),
            nodesVisited,
            nps(nodesVisited, since),
            MILLISECONDS(since).count(),
            pv[0].str());
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
  if (uciHandler) {
    uciHandler->sendAspirationResearchInfo(
      statistics.currentSearchDepth,
      statistics.currentExtraSearchDepth,
      statistics.currentBestRootMoveValue,
      boundString,
      nodesVisited,
      nps(nodesVisited, since),
      MILLISECONDS(since),
      pv[0]);
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
            pv[0].str());
}
