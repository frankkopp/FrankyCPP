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

#include <chrono>

#include "Evaluator.h"
#include "Search.h"
#include "SearchConfig.h"
#include "See.h"
#include "chesscore/Position.h"

////////////////////////////////////////////////
///// CONSTRUCTORS

Search::Search() : Search(nullptr) {}

Search::Search(UciHandler* uciHandler) {
  this->uciHandler = uciHandler;
  this->tt         = std::make_unique<TT>(0);
}

Search::~Search() {
  // necessary to avoid err message:
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

void Search::startSearch(const Position p, SearchLimits sl) {
  // acquire init phase lock
  if (!initSemaphore.get()) {
    LOG__WARN(Logger::get().SEARCH_LOG, "Search init failed as another initialization is ongoing.");
  }

  // start search time
  startTime       = now();
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
  initSemaphore.getOrWait();
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
  if (searchThread.joinable()) {
    searchThread.join();
  }
  waitWhileSearching();
}

bool Search::isSearching() const {
  // Try to get running semaphore.
  // If not available the search is running
  if (isRunningSemaphore.get()) {
    isRunningSemaphore.release();
    return false;
  }
  return true;
}

void Search::waitWhileSearching() const {
  // get or wait for running semaphore
  isRunningSemaphore.getOrWait();
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

void Search::clearTT() {
  if (isSearching()) {
    const std::string msg = "Can't clear hash while searching.";
    sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, msg);
    return;
  }
  tt->clear();
  const std::string msg = "Hash cleared.";
  sendString(msg);
  LOG__INFO(Logger::get().SEARCH_LOG, msg);
}

void Search::resizeTT() {
  if (isSearching()) {
    const std::string msg = "Can't resize hash while searching.";
    sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, msg);
    return;
  }
  tt = std::make_unique<TT>(0);// clear the old TT (is smart pointer and memory is freed)
  initialize();                // re-initialize
  sendString("Resized hash: " + tt->str());
}

////////////////////////////////////////////////
///// PRIVATE

void Search::run() {
  // check if there is already a search running
  // and if not grab the isRunning semaphore
  if (!isRunningSemaphore.get()) {
    LOG__ERROR(Logger::get().SEARCH_LOG, "Search already running");
    return;
  }

  LOG__INFO(Logger::get().SEARCH_LOG, "Searching " + position.strFen());

  // initialize search
  stopSearchFlag    = false;
  hasResultFlag     = false;
  timeLimit         = MilliSec{};
  extraTime         = MilliSec{};
  nodesVisited      = 0;
  statistics        = SearchStats{};
  lastUciUpdateTime = nowFast();
  npsTime           = lastUciUpdateTime;
  initialize();

  // setup and report search limits
  setupSearchLimits(position, searchLimits);

  // when not pondering and search is time controlled start timer
  if (searchLimits.timeControl && !searchLimits.ponder) {
    startTimer();
  }

  // age tt entries
  if (tt->getMaxNumberOfEntries()) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table: Using TT: " + tt->str());
    tt->ageEntries();
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table: Not using TT.");
  }

  // Initialize ply based data
  // move generators for each ply
  // pv move list for each ply
  // Each depth in search gets it own global
  // field to avoid object creation during search.
  for (int i = DEPTH_NONE; i < DEPTH_MAX; i++) {
    this->mg[i] = MoveGenerator{};
    if (SearchConfig::USE_HISTORY_COUNTER || SearchConfig::USE_HISTORY_MOVES) {
      this->mg[i].setHistoryData(&history);
    }
    pv[i].clear();
  }

  // release the init phase lock to signal the calling go routine
  // waiting in StartSearch() to return
  initSemaphore.release();

  // check for opening book move when we have a time controlled game
  Move bookMove = MOVE_NONE;
  if (book && SearchConfig::USE_BOOK && searchLimits.timeControl) {
    bookMove = book->getRandomMove(position.getZobristKey());
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Opening Book: Choosing book move " + str(bookMove));
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Opening Book: Not using book.");
  }

  LOG__INFO(Logger::get().SEARCH_LOG, fmt::format("Search using: PVS={} ASP={}", SearchConfig::USE_PVS, SearchConfig::USE_ASP));

  // If we have found a book move update result and omit search.
  // Otherwise start search with iterative deepening.
  SearchResult searchResult{};
  if (!bookMove) {
    searchResult = iterativeDeepening(position);
  }
  else {
    searchResult.bestMove = bookMove;
    searchResult.bookMove = true;
    hadBookMove           = true;
  }

  // If we arrive here during Ponder mode or Infinite mode and the search is not
  // stopped it means that the search was finished before it has been stopped
  // by stopSearchFlag or ponderhit,
  // We wait here until search has completed.
  if (!stopSearchFlag && (searchLimits.ponder || searchLimits.infinite)) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Search finished before stopped or ponderhit! Waiting for stop/ponderhit to send result");
    // relaxed busy wait
    while (!stopSearchFlag && (searchLimits.ponder || searchLimits.infinite)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }

  // Clean up
  // make sure timer stops as this could potentially still be running
  // when search finished without any stop signal/limit
  stopSearchFlag = true;

  // update search result with search time and pv
  searchResult.time  = now() - startSearchTime;
  searchResult.pv    = pv[0];
  searchResult.nodes = nodesVisited;

  // print stats to log
  LOG__INFO(Logger::get().SEARCH_LOG, "Search finished after {}", str(searchResult.time));
  LOG__INFO(Logger::get().SEARCH_LOG, "Search depth was {}({}) with {:n} nodes visited. NPS = {:n} nps", statistics.currentSearchDepth, statistics.currentExtraSearchDepth, nodesVisited, nps(nodesVisited, searchResult.time));
  LOG__DEBUG(Logger::get().SEARCH_LOG, "Search stats: {}", statistics.str());

  // print result to log
  if (searchLimits.mate && searchResult.mateFound) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Mate in {} found: {}", searchLimits.mate, str(pv[0].at(0)));
  }
  LOG__INFO(Logger::get().SEARCH_LOG, "Search result: {}", searchResult.str());

  // save result until overwritten by the next search
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
    std::string msg = "Search called on DRAW by Repetition or 50-moves-rule";
    sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, msg);
    searchResult.bestMoveValue = VALUE_DRAW;
    return searchResult;
  }

  // generate all legal root moves (get a copy)
  rootMoves = *mg[0].generateLegalMoves(p, GenAll);

  // check if there are legal moves - if not it's mate or stalemate
  if (rootMoves.empty()) {
    if (p.hasCheck()) {
      statistics.checkmates++;
      std::string msg = "Search called on a check mate position";
      sendString(msg);
      LOG__WARN(Logger::get().SEARCH_LOG, msg);
      searchResult.bestMoveValue = -VALUE_CHECKMATE;
    }
    else {
      statistics.stalemates++;
      std::string msg = "Search called on a stale mate position";
      sendString(msg);
      LOG__WARN(Logger::get().SEARCH_LOG, msg);
      searchResult.bestMoveValue = VALUE_DRAW;
    }
    return searchResult;
  }

  // add some extra time for the move after the last book move
  // hadBookMove move will be true at his point if we ever had
  // a book move.
  if (hadBookMove && searchLimits.timeControl && searchLimits.moveTime.count() == 0) {
    LOG__WARN(Logger::get().SEARCH_LOG, "First non-book move to search. Adding extra time: Before: {}, after: {}",
              str(timeLimit + extraTime), str(2 * timeLimit + extraTime));
    addExtraTime(2.0);
    hadBookMove = false;
  }

  // prepare max depth from search limits
  int maxDepth = DEPTH_MAX;
  if (searchLimits.depth) {
    maxDepth = searchLimits.depth;
  }

  // Max window search in preparation for aspiration window search
  // not needed yet
  Value alpha     = VALUE_MIN;
  Value beta      = VALUE_MAX;
  Value bestValue = VALUE_NONE;

  // ###########################################
  // ### BEGIN Iterative Deepening
  for (Depth iterationDepth = Depth{1}; iterationDepth <= maxDepth; ++iterationDepth) {
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

    // ###########################################
    // Start actual alpha beta search
    // ASPIRATION SEARCH
    if (SearchConfig::USE_ASP && iterationDepth > 3) {
      bestValue = aspirationSearch(p, iterationDepth, bestValue);
    }
    // PVS SEARCH (or pure ALPHA BETA when PVS deactivated)
    else {
      bestValue = rootSearch(p, iterationDepth, alpha, beta);
    }
    // ###########################################

    assert((bestValue == valueOf(pv[0].at(0)) || stopSearchFlag) && "bestValue should be equal value of pv[0].at(0)");

    // if mate search check if we found a mate within the mate limit
    if (searchLimits.mate && abs(valueOf(pv[0].at(0))) >= VALUE_CHECKMATE_THRESHOLD && searchLimits.mate * 2 - 1 == VALUE_CHECKMATE - valueOf(pv[0].at(0))) {
      searchResult.mateFound = true;
      break;
    }

    // check if we need to stop
    // doing this after the first iteration ensures that
    // we have done at least one complete search and have
    // a pv (best) move
    // If we only have one move to play also stop the search
    if (!stopConditions() && rootMoves.size() > 1) {
      // sort root moves for the next iteration
      std::stable_sort(rootMoves.begin(), rootMoves.end(), moveValueGreaterComparator());
      statistics.currentBestRootMove      = pv[0].at(0);
      statistics.currentBestRootMoveValue = valueOf(pv[0].at(0));
      assert(pv[0].at(0) == rootMoves.at(0) && "Best root move should be equal to pv[0].at(0)");
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
  // best move is pv[0][0] - we need to make sure this array entry exists at this time
  // best value is pv[0][0].valueOf
  searchResult.bestMove      = moveOf(pv[0].at(0));
  searchResult.bestMoveValue = valueOf(pv[0].at(0));
  searchResult.depth         = statistics.currentIterationDepth;
  searchResult.extraDepth    = statistics.currentExtraSearchDepth;
  searchResult.bookMove      = false;

  // see if we have a move we could ponder on
  if (pv[0].size() > 1) {
    searchResult.ponderMove = moveOf(pv[0].at(1));
  }
  else {
    // we do not have a ponder move in the pv list
    // so let's check the TT
    if (SearchConfig::USE_TT) {
      p.doMove(searchResult.bestMove);
      const auto* ttEntryPtr = tt->probe(p.getZobristKey());
      if (ttEntryPtr) {
        statistics.ttHit++;
        searchResult.ponderMove = static_cast<Move>(ttEntryPtr->move);
        LOG__DEBUG(Logger::get().SEARCH_LOG, "Using ponder move from hash table: {}", str(searchResult.ponderMove));
      }
      p.undoMove();
    }
  }

  return searchResult;
}

Value Search::aspirationSearch(Position& p, Depth depth, Value value) {
  // TODO implement
  LOG__CRITICAL(Logger::get().SEARCH_LOG, "Not implemented yet", __FUNCTION__);
  return VALUE_DRAW;
}

Value Search::rootSearch(Position& p, Depth depth, Value alpha, Value beta) {

  // In root search we search all moves and store the value
  // into the root moves themselves for sorting in the
  // next iteration
  // best move is stored in pv[0][0]
  // best value is stored in pv[0][0].value
  // The next iteration begins with the best move of the last
  // iteration so we can be sure pv[0][0] will be set with the
  // last best move from the previous iteration independent of
  // the value. Any better move found is really better and will
  // replace pv[0][0] and also will be sorted first in the
  // next iteration

  // prepare root node search
  const Depth ply{1};
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
        if (value > alpha && value < beta && !stopConditions()) {
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
    if (stopConditions() && depth > 1) {
      return VALUE_NONE;
    }

    // set the value into he root move to later be able to sort
    // root moves according to value
    setValueOf(moveRef, value);

    // Did we find a new best move?
    // For the first move with a full window (alpha=-inf)
    // this is always the case.
    if (value > bestNodeValue) {
      bestNodeValue = value;
      // we have a new best move and pv[0][0] - store pv+1 tp pv
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

Value Search::search(Position& p, Depth depth, Depth ply, Value alpha, Value beta, Node_Type isPv, Do_Null doNull) {
  //  LOG__DEBUG(Logger::get().SEARCH_LOG, "Search {} {} {}", depth, ply, str(statistics.currentVariation));

  // Enter quiescence search when depth == 0 or max ply has been reached
  if (depth == 0 || ply >= MAX_DEPTH) {
    return qsearch(p, ply, alpha, beta, isPv);
  }

  // check if search should be stopped
  if (stopConditions()) {
    return VALUE_NONE;
  }

  // Mate Distance Pruning
  // Did we already find a shorter mate then ignore
  // this one.
  if (SearchConfig::USE_MDP) {
    alpha = std::max(alpha, -VALUE_CHECKMATE + Value(ply));
    beta  = std::min(beta, VALUE_CHECKMATE - Value(ply));
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
  // stored result already was a precise result and we do not
  // need to search the position again. We can stop searching
  // this branch and return the value.
  // Alpha or Beta entries will only be used if they improve
  // the current values.
  const TT::Entry* ttEntryPtr;
  if (SearchConfig::USE_TT) {
    ttEntryPtr = tt->probe(p.getZobristKey());
    if (ttEntryPtr) {// tt hit
      statistics.ttHit++;
      ttMove = static_cast<Move>(ttEntryPtr->move);
      if (ttEntryPtr->depth >= depth) {
        const Value ttValue = valueFromTt(ttEntryPtr->value, ply);
        if (validValue(ttValue) &&
            (ttEntryPtr->type == EXACT ||
             (ttEntryPtr->type == ALPHA && ttValue <= alpha) ||
             (ttEntryPtr->type == BETA && ttValue >= beta)) &&
            SearchConfig::USE_TT_VALUE) {
          // get PV line from tt as we prune here
          // and wouldn't have one otherwise
          getPvLine(p, pv[ply], depth);
          statistics.TtCuts++;
          return ttValue;
        }
        statistics.TtNoCuts++;
      }
      // if we have a static eval stored we can reuse it
      if (SearchConfig::USE_EVAL_TT &&
          ttEntryPtr->eval != VALUE_NONE) {
        statistics.evalFromTT++;
        staticEval = ttEntryPtr->eval;
      }
    }
    else {
      statistics.ttMiss++;
    }
  }// use TT

  const bool hasCheck = p.hasCheck();

  // get an evaluation for the position
  if (!hasCheck && staticEval == VALUE_NONE) {
    staticEval = evaluate(p);
    // Storing this value might save us calls to eval on the same position.
    if (SearchConfig::USE_TT && SearchConfig::USE_EVAL_TT) {
      storeTt(p, DEPTH_NONE, DEPTH_NONE, MOVE_NONE, VALUE_NONE, NONE, staticEval, matethreat);
    }
  }


  // Razoring from Stockfish
  // When static eval is well below alpha at the last node
  // jump directly into qsearch
  if (SearchConfig::USE_RAZORING &&
      depth == 1 &&
      staticEval != VALUE_NONE &&
      staticEval <= alpha - SearchConfig::RAZOR_MARGIN) {

    statistics.razorings++;
    return qsearch(p, ply, alpha, beta, PV);
  }

  // Reverse Futility Pruning, (RFP, Static Null Move Pruning)
  // https://www.chessprogramming.org/Reverse_Futility_Pruning
  // Anticipate likely alpha low in the next ply by a beta cut
  // off before making and evaluating the move
  if (SearchConfig::USE_RFP &&
      doNull &&
      depth <= 3 &&
      !isPv &&
      !hasCheck) {
    auto margin = SearchConfig::rfp[depth];
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
    if (doNull &&
        !isPv &&
        depth >= SearchConfig::NMP_DEPTH &&
        p.getMaterialNonPawn(us) > 0 &&// to reduce risk of zugzwang
        !hasCheck) {
      // possible other criteria: eval > beta

      // determine depth reduction
      // ICCA Journal, Vol. 22, No. 3
      // Ernst A. Heinz, Adaptive Null-Move Pruning, postscript
      // http://people.csail.mit.edu/heinz/ps/adpt_null.ps.gz
      auto r = SearchConfig::NMP_REDUCTION;
      if (depth > 8 || (depth > 6 && p.getGamePhase() >= 3)) {
        ++r;
      }
      auto newDepth = depth - r - 1;
      // double check that depth does not get negative
      if (newDepth < 0) {
        newDepth = DEPTH_NONE;
      }

      // do null move search
      p.doNullMove();
      nodesVisited++;
      auto nValue = -search(p, newDepth, ply + 1, -beta, -beta + 1, Node_Type::NonPV, Do_Null::No_Null_Move);
      p.undoNullMove();

      // check if we should stop the search
      if (stopConditions()) {
        return VALUE_NONE;
      }

      // flag for mate threats
      if (nValue > VALUE_CHECKMATE_THRESHOLD) {
        // although this player did not make a move the value still is
        // a mate - very good! Just adjust the value to not return an
        // unproven mate
        nValue = VALUE_CHECKMATE_THRESHOLD;
      }
      else if (nValue < VALUE_CHECKMATE_THRESHOLD) {
        // the player did not move and got mated ==> mate threat
        matethreat = true;
      }

      // if the value is higher than beta even after not making
      // a move it is not worth searching as it will very likely
      // be above beta if we make a move
      if (nValue >= beta) {
        statistics.nullMoveCuts++;
        // Store TT
        if (SearchConfig::USE_TT) {
          storeTt(p, depth, ply, ttMove, nValue, BETA, staticEval, matethreat);
        }
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
    if (depth >= SearchConfig::IID_DEPTH &&
        ttMove != MOVE_NONE &&// no move from TT
        doNull &&             // avoid in null move search
        isPv) {

      // get the new depth and make sure it is >0
      auto newDepth = depth - SearchConfig::IID_REDUCTION;
      if (newDepth < 0) {
        newDepth = DEPTH_NONE;
      }

      // do the actual reduced search
      search(p, newDepth, ply, alpha, beta, isPv, doNull);
      statistics.iidSearches++;

      // check if we should stop the search
      if (stopConditions()) {
        return VALUE_NONE;
      }

      // get the best move from the reduced search if available
      if (!pv[ply].empty()) {
        statistics.iidMoves++;
        ttMove = moveOf(pv[ply][0]);
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
  // TT or IID we set it as PV move in the movegen so it will
  // be searched first.
  if (SearchConfig::USE_TT_PV_MOVE_SORT &&
      ttMove != MOVE_NONE) {
    statistics.TtMoveUsed++;
    myMg->setPV(ttMove);
  }
  else {
    statistics.NoTtMove++;
  }

  // prepare move loop
  Value value       = VALUE_NONE;
  Move move         = MOVE_NONE;
  int movesSearched = 0;// to detect mate situations

  // ///////////////////////////////////////////////////////
  // MOVE LOOP
  while ((move = myMg->getNextPseudoLegalMove(p, GenAll, hasCheck)) != MOVE_NONE) {
    const Square from     = fromSquare(move);
    const Square to       = toSquare(move);
    const bool givesCheck = p.givesCheck(move);

    // prepare newDepth
    Depth newDepth  = depth - DEPTH_ONE;
    Depth lmrDepth  = newDepth;
    Depth extension = DEPTH_NONE;

    // TODO implement extension and pruning

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
    // EVAL_PREFETCH;

    // we only count legal moves
    nodesVisited++;
    statistics.currentVariation.push_back(move);

    sendSearchUpdateToUci();

    // check repetition and 50 moves
    if (checkDrawRepAnd50(p, 2)) {
      value = VALUE_DRAW;
    }
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
        if (value > alpha && !stopConditions()) {
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
    if (stopConditions()) {
      return VALUE_NONE;
    }

    // Did we find a better move for this node (not ply)?
    // For the first move this is always the case.
    if (value > bestNodeValue) {
      // These "best" values are only valid for this node
      // not for all of the ply (not yet clear if >alpha)
      bestNodeValue = value;
      bestNodeMove  = move;

      // Did we find a better move than in previous nodes in ply
      // then this is our new PV and best move for this ply.
      // If we never find a better alpha this means all moves in
      // this node are worse then other moves in other nodes which
      // raised alpha - meaning we have a better move from another
      // node we would play. We will return alpha and store a alpha
      // node in TT.
      if (value > alpha) {
        // If we found a move that is better or equal than beta
        // this means that the opponent can/will avoid this
        // position altogether so we can stop search this node.
        // We will not know if our best move is really the
        // best move or how good it really is (value is a lower bound)
        // as we cut off the rest of the search of the node here.
        // We will safe the move as a killer to be able to search it
        // earlier in another node of the ply.
        if (value >= beta && SearchConfig::USE_ALPHABETA) {
          // Count beta cuts
          statistics.betaCuts++;
          // Count beta cuts on first move
          if (movesSearched == 1) {
            statistics.betaCuts1st++;
          }
          // store move which caused a beta cut off in this ply
          if (SearchConfig::USE_KILLER_MOVES && !p.isCapturingMove(move)) {
            myMg->storeKiller(move);
          }
          // counter for moves which caused a beta cut off
          // we use 1 << depth as an increment to favor deeper searches
          // a more repetitions
          if (SearchConfig::USE_HISTORY_COUNTER) {
            history.historyCount[us][from][to] += 1LL << depth;
          }
          // store a successful counter move to the previous opponent move
          if (SearchConfig::USE_HISTORY_MOVES) {
            Move lastMove = p.getLastMove();
            if (lastMove != MOVE_NONE) {
              history.counterMoves[fromSquare(lastMove)][toSquare(lastMove)] = move;
            }
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
      if (history.historyCount[us][from][to] < 0) {
        history.historyCount[us][from][to] = 0;
      }
    }
  }
  // MOVE LOOP
  // ///////////////////////////////////////////////////////

  // If we did not have at least one legal move
  // then we might have a mate or stalemate
  if (movesSearched == 0 && !stopConditions()) {
    if (hasCheck) {// mate
      statistics.checkmates++;
      bestNodeValue = -VALUE_CHECKMATE + Value(ply);
    }
    else {// stalemate
      statistics.stalemates++;
      bestNodeValue = VALUE_DRAW;
    }
    // this is in any case an exact value
    staticEval = bestNodeValue;
    ttType     = EXACT;
  }

  // Store TT
  // Store search result for this node into the transposition table
  if (SearchConfig::USE_TT) {
    storeTt(p, depth, ply, bestNodeMove, bestNodeValue, ttType, staticEval, matethreat);
  }

  return bestNodeValue;
}

Value Search::qsearch(Position& p, Depth ply, Value alpha, Value beta, Search::Node_Type isPv) {
  //  LOG__DEBUG(Logger::get().SEARCH_LOG, "QSearch {} {}", ply, str(statistics.currentVariation));

  if (statistics.currentExtraSearchDepth < ply) {
    statistics.currentExtraSearchDepth = ply;
  }

  // if we have deactivated qsearch or we have reached our maximum depth
  // we evaluate the position and return the value
  if (!SearchConfig::USE_QUIESCENCE || ply >= MAX_DEPTH) {
    statistics.perftNodeCount++;
    return evaluate(p);
  }

  // check if search should be stopped
  if (stopConditions()) {
    return VALUE_NONE;
  }

  // Mate Distance Pruning
  if (SearchConfig::USE_MDP) {
    alpha = std::max(alpha, -VALUE_CHECKMATE + Value(ply));
    beta  = std::min(beta, VALUE_CHECKMATE - Value(ply));
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
  const TT::Entry* ttEntryPtr;
  if (SearchConfig::USE_TT && SearchConfig::USE_QS_TT) {
    ttEntryPtr = tt->probe(p.getZobristKey());
    if (ttEntryPtr) {// tt hit
      statistics.ttHit++;
      ttMove              = static_cast<Move>(ttEntryPtr->move);
      const Value ttValue = valueFromTt(ttEntryPtr->value, ply);
      if (validValue(ttValue) &&
          (ttEntryPtr->type == EXACT ||
           (ttEntryPtr->type == ALPHA && ttValue <= alpha) ||
           (ttEntryPtr->type == BETA && ttValue >= beta)) &&
          SearchConfig::USE_TT_VALUE) {
        statistics.TtCuts++;
        return ttValue;
      }
      // if we have a static eval stored we can reuse it
      if (SearchConfig::USE_EVAL_TT &&
          ttEntryPtr->eval != VALUE_NONE) {
        statistics.evalFromTT++;
        staticEval = ttEntryPtr->eval;
      }
    }
    else {
      statistics.ttMiss++;
    }
  }// use TT

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
    if (SearchConfig::USE_QS_STANDPAT_CUT && staticEval > alpha) {
      if (staticEval >= beta) {
        statistics.standpatCuts++;
        // Storing this value might save us calls to eval on the same position.
        if (SearchConfig::USE_TT && SearchConfig::USE_QS_TT && SearchConfig::USE_EVAL_TT) {
          storeTt(p, DEPTH_NONE, ply, MOVE_NONE, VALUE_NONE, NONE, staticEval, false);
        }
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
  if (SearchConfig::USE_TT_PV_MOVE_SORT &&
      ttMove != MOVE_NONE) {
    statistics.TtMoveUsed++;
    myMg->setPV(ttMove);
  }
  else {
    statistics.NoTtMove++;
  }

  // prepare move loop
  Value value       = VALUE_NONE;
  Move move         = MOVE_NONE;
  int movesSearched = 0;// to detect mate situations

  // when in check generate all moves
  const GenMode genMode = hasCheck ? GenAll : GenNonQuiet;

  // ///////////////////////////////////////////////////////
  // MOVE LOOP
  while ((move = myMg->getNextPseudoLegalMove(p, genMode, hasCheck)) != MOVE_NONE) {
    const Square from     = fromSquare(move);
    const Square to       = toSquare(move);
    const bool givesCheck = p.givesCheck(move);

    // TODO implement pruning

    // reduce number of moves searched in quiescence
    // by looking at good captures only
    if (!hasCheck && !goodCapture(p, move)) {
      continue;
    }

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
    // EVAL_PREFETCH;

    // we only count legal moves
    nodesVisited++;
    statistics.currentVariation.push_back(move);
    sendSearchUpdateToUci();

    // check repetition and 50 moves
    if (checkDrawRepAnd50(p, 2)) {
      value = VALUE_DRAW;
    }
    else {
      value = -qsearch(p, ply + 1, -beta, -alpha, isPv);
    }

    movesSearched++;
    statistics.currentVariation.pop_back();
    p.undoMove();
    // UNDO MOVE
    // ///////////////////////////////////////////////////////

    // check if we should stop the search
    if (stopConditions()) {
      return VALUE_NONE;
    }

    // See search function above for documentation
    if (value > bestNodeValue) {
      bestNodeValue = value;
      bestNodeMove  = move;
      if (value > alpha) {
        if (value >= beta && SearchConfig::USE_ALPHABETA) {
          statistics.betaCuts++;
          if (movesSearched == 1) {
            statistics.betaCuts1st++;
          }
          if (SearchConfig::USE_KILLER_MOVES && !p.isCapturingMove(move)) {
            myMg->storeKiller(move);
          }
          if (SearchConfig::USE_HISTORY_COUNTER) {
            history.historyCount[us][from][to] += 1 << 1;
          }
          if (SearchConfig::USE_HISTORY_MOVES) {
            Move lastMove = p.getLastMove();
            if (lastMove != MOVE_NONE) {
              history.counterMoves[fromSquare(lastMove)][toSquare(lastMove)] = move;
            }
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
      history.historyCount[us][from][to] -= 1U << 1;
      if (history.historyCount[us][from][to] < 0) {
        history.historyCount[us][from][to] = 0;
      }
    }
  }
  // MOVE LOOP
  // ///////////////////////////////////////////////////////

  // If we did not have at least one legal move
  // then we might have a mate or stalemate
  if (movesSearched == 0 && !stopConditions()) {
    // if we have a mate we had a check before and therefore
    // generated all moves. We can be sure this is a mate.
    if (hasCheck) {// mate
      statistics.checkmates++;
      bestNodeValue = -VALUE_CHECKMATE + Value(ply);
      ttType        = EXACT;
    }
    // if we do not have mate we had no check and
    // therefore might have only quiet moves which
    // we did not generate.
    // We return the standpat value in this case
    // which we have set to bestNodeValue in the
    // static eval earlier
  }

  // Store TT
  // Store search result for this node into the transposition table
  if (SearchConfig::USE_TT && SearchConfig::USE_QS_TT) {
    storeTt(p, DEPTH_ONE, ply, bestNodeMove, bestNodeValue, ttType, staticEval, false);
  }

  return bestNodeValue;
}

inline Value Search::evaluate(Position& p) {
  statistics.leafPositionsEvaluated++;
  statistics.evaluations++;
  return evaluator->evaluate(p);
}

bool Search::goodCapture(Position& p, Move move) {
  if (SearchConfig::USE_QS_SEE) {
    // Check SEE score of higher value pieces to low value pieces
    return See::see(p, move) > 0;
  }
  return
    // all pawn captures - they never loose material
    // typeOf(position.getPiece(getFromSquare(move))) == PAWN

    // Lower value piece captures higher value piece
    // With a margin to also look at Bishop x Knight
    (valueOf(position.getPiece(fromSquare(move))) + 50) < valueOf(position.getPiece(toSquare(move)))

    // all recaptures should be looked at
    || (position.getLastMove() != MOVE_NONE && position.getLastCapturedPiece() != PIECE_NONE && toSquare(position.getLastMove()) == toSquare(move))

    // undefended pieces captures are good
    // If the defender is "behind" the attacker this will not be recognized
    // here This is not too bad as it only adds a move to qsearch which we
    // could otherwise ignore
    || !position.isAttacked(toSquare(move), ~position.getNextPlayer());
}

void Search::storeTt(Position& p, Depth depth, Depth ply, Move move, Value value, ValueType valueType, Value eval, bool mateThreat) {
  tt->put(p.getZobristKey(), depth, move, valueToTt(value, ply), valueType, eval, mateThreat);
}

void Search::savePV(Move move, MoveList& src, MoveList& dest) {
  dest.clear();
  dest.push_back(move);
  dest.insert(dest.end(), src.begin(), src.end());
}

Value Search::valueToTt(Value value, Depth ply) {
  if (isCheckMateValue(value)) {
    if (value > 0) {
      value = value + Value(ply);
    }
    else {
      value = value - Value(ply);
    }
  }
  return value;
}

Value Search::valueFromTt(Value value, Depth ply) {
  if (isCheckMateValue(value)) {
    if (value > 0) {
      value = value - Value(ply);
    }
    else {
      value = value + Value(ply);
    }
  }
  return value;
}


void Search::getPvLine(Position& p, MoveList& pvList, Depth depth) {
  // Recursion-less reading of the chain of pv moves
  pvList.clear();
  int counter  = 0;
  auto ttMatch = tt->getMatch(p.getZobristKey());
  while (ttMatch != nullptr && ttMatch->move != MOVE_NONE && counter < depth) {
    pvList.push_back(static_cast<Move>(ttMatch->move));
    p.doMove(static_cast<Move>(ttMatch->move));
    counter++;
    ttMatch = tt->getMatch(p.getZobristKey());
  }
  for (int i = 0; i < counter; ++i) {
    p.undoMove();
  }
}

void Search::initialize() {
  LOG__INFO(Logger::get().SEARCH_LOG, "Search initialization.");
  // init opening book
  if (SearchConfig::USE_BOOK) {
    if (!book) {// only initialize once
      book = std::make_unique<OpeningBook>(SearchConfig::BOOK_PATH, SearchConfig::BOOK_TYPE);
      book->initialize();
    }
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Opening Book disabled in configuration");
  }

  // init transposition table
  if (SearchConfig::USE_TT) {
    if (tt->getMaxNumberOfEntries() == 0) {// only initialize once
      tt = std::make_unique<TT>(SearchConfig::TT_SIZE_MB);
    }
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table disabled in configuration");
    tt = std::make_unique<TT>(0);
  }

  // init evaluator
  if (!evaluator) {// only initialize once
    evaluator = std::make_unique<Evaluator>();
  }
}

bool Search::stopConditions() {
  if (stopSearchFlag) return true;
  if (searchLimits.nodes > 0 && nodesVisited >= searchLimits.nodes) {
    stopSearchFlag = true;
  }
  return stopSearchFlag;
}

bool Search::checkDrawRepAnd50(Position& p, int numberOfRepetitions) {
  return p.checkRepetitions(numberOfRepetitions) || p.getHalfMoveClock() >= 100;
}

void Search::setupSearchLimits(Position& p, SearchLimits& sl) {
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
    timeLimit = setupTimeControl(p, sl);
    extraTime = MilliSec{0};
    if (sl.moveTime.count()) {
      LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Time Controlled: Time per Move {}", str(sl.moveTime));
    }
    else {
      LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Time Controlled: White = {} (inc {}) Black = {} (inc {}) Moves to go: {}",
                str(sl.whiteTime), str(sl.whiteInc), str(sl.blackTime), str(sl.blackInc), sl.movesToGo);
      LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Time limit: {}", str(timeLimit));
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
    LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Moves limited  : {}", str(sl.moves));
  }
}

MilliSec Search::setupTimeControl(Position& position, SearchLimits& sl) {
  if (sl.moveTime.count()) {// mode time per move
    // we need a little room for executing the code
    MilliSec duration = sl.moveTime - MilliSec{20};
    // if the duration is now negative return the original value and issue a warning
    if (duration.count() < 0) {
      LOG__WARN(Logger::get().SEARCH_LOG, "Very short move time: {} ms", sl.moveTime.count());
      return sl.moveTime;
    }
    return duration;
  }
  else {// mode is remaining time - estimated time per move
    // moves left
    int movesLeft = sl.movesToGo;
    if (!movesLeft) {// default
      // we estimate minimum 15 more moves in final game phases
      // in early game phases this grows up to 40
      movesLeft = 15 + static_cast<int>(25 * position.getGamePhaseFactor());
    }
    // time left for current player
    MilliSec timeLeft;
    if (position.getNextPlayer()) {
      timeLeft = sl.blackTime + (movesLeft * sl.blackInc);
    }
    else {
      timeLeft = sl.whiteTime + (movesLeft * sl.whiteInc);
    }
    // estimate time per move
    MilliSec tl = static_cast<MilliSec>(timeLeft.count() / movesLeft);
    // account for runtime of our code
    if (tl.count() < 100) {
      // limits for very short available time reduced by another 20%
      tl = static_cast<MilliSec>(uint64_t(0.8 * tl.count()));
    }
    else {
      // reduced by 10%
      tl = static_cast<MilliSec>(uint64_t(0.9 * tl.count()));
    }
    return tl;
  }
}

void Search::addExtraTime(double f) {
  if (searchLimits.timeControl && !searchLimits.moveTime.count()) {
    auto duration = uint64_t(timeLimit.count() * (f - 1.0));
    extraTime += MilliSec(duration);
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Time added/reduced by {} to {} ", str(MilliSec(duration)), str(timeLimit + extraTime));
  }
}

void Search::startTimer() {
  this->timerThread = std::thread([&] {
    startSearchTime = now();
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Timer started with time limit of {} ms", str(timeLimit));
    // relaxed busy wait
    while ((now() - startSearchTime) < (timeLimit + extraTime) && !stopSearchFlag) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!this->stopSearchFlag) {
      this->stopSearchFlag = true;
      LOG__INFO(Logger::get().SEARCH_LOG, "Stop search by Timer after wall time: {} (time limit {} and extra time {})", str(now() - startTime), str(timeLimit), str(extraTime));
    }
  });
}

void Search::sendReadyOk() const {
  if (uciHandler) {
    uciHandler->sendReadyOk();
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "uci >> readyok");
  }
}

void Search::sendString(const std::string& msg) const {
  if (uciHandler) {
    uciHandler->sendString(msg);
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "uci >> " + msg);
  }
}
void Search::sendResult(SearchResult& result) {
  if (uciHandler) {
    uciHandler->sendResult(result.bestMove, result.ponderMove);
  }
}

void Search::sendIterationEndInfoToUci() {
  const NanoSec& since = elapsedSince(startSearchTime);
  if (uciHandler) {
    uciHandler->sendIterationEndInfo(
      statistics.currentSearchDepth,
      statistics.currentExtraSearchDepth,
      statistics.currentBestRootMoveValue,
      nodesVisited,
      nps(nodesVisited, since),
      MILLISECONDS(since),
      pv[0]);
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "depth {} seldepth {} value {} nodes {:n} nps {:n} time {:n} pv {}",
              statistics.currentSearchDepth,
              statistics.currentExtraSearchDepth,
              str(statistics.currentBestRootMoveValue),
              nodesVisited,
              nps(nodesVisited, since),
              MILLISECONDS(since).count(),
              str(pv[0]));
  }
}

void Search::sendSearchUpdateToUci() {

  // to minimize performance impact we only check time every 1M nodes
  if ((nodesVisited - lastUciUpdateNodes < 1'000'000)) {
    return;
  }
  lastUciUpdateNodes = nodesVisited;

  // we only update every UCI_UPDATE_INTERVAL ns
  const uint64_t nowTime = nowFast();
  if (nowTime - lastUciUpdateTime < UCI_UPDATE_INTERVAL) {
    return;
  }
  lastUciUpdateTime = nowTime;

  // nps is calculated from the nodes and time since last update.
  // This might not be the same as the over all avg. nps which is shown
  // at the end of a search.
  const uint64_t nodesPerSec = nps(nodesVisited - npsNodes, nowTime - npsTime);
  npsTime                    = nowTime;
  npsNodes                   = nodesVisited;

  const int hashfull = tt->hashFull();

  const NanoSec& since = elapsedSince(startSearchTime);

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
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "depth {} seldepth {} nodes {:n} nps {:n} time {:n} hashful {:n}",
              statistics.currentSearchDepth,
              statistics.currentExtraSearchDepth,
              nodesVisited,
              nodesPerSec,
              MILLISECONDS(since).count(),
              hashfull);
  }
}
