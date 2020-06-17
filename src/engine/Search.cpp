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

#include "Search.h"
#include "SearchConfig.h"

#include "Evaluator.h"

////////////////////////////////////////////////
///// CONSTRUCTORS

Search::Search() : Search(nullptr) {}

Search::Search(UciHandler* uciHandler) {
  this->uciHandler = uciHandler;
}

Search::~Search() {
  // necessary to avoid err message:
  // terminate called without an active exception
  if (searchThread.joinable()) { searchThread.join(); }
}

////////////////////////////////////////////////
///// PUBLIC

void Search::newGame() {
  stopSearch();
  if (tt) tt->clear();
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

  // move the received copy of position and search limits to instance variables
  this->position     = std::move(p);
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
  stopSearchFlag = true;
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
  if (tt) {
    tt->clear();
    const std::string msg = "Hash cleared.";
    sendString(msg);
    LOG__INFO(Logger::get().SEARCH_LOG, msg);
  }
}

void Search::resizeTT() {
  if (isSearching()) {
    const std::string msg = "Can't resize hash while searching.";
    sendString(msg);
    LOG__WARN(Logger::get().SEARCH_LOG, msg);
    return;
  }
  tt = nullptr;// clear the old TT (is smart pointer)
  initialize();// re-initialize
  if (tt) {
    sendString("Resized hash: " + tt->str());
  }
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

  // start search time
  startTime = std::chrono::high_resolution_clock::now();

  LOG__INFO(Logger::get().SEARCH_LOG, "Searching " + position.strFen());

  // initialize search
  stopSearchFlag    = false;
  hasResultFlag     = false;
  timeLimit         = MilliSec{0};
  extraTime         = MilliSec{0};
  nodesVisited      = 0;
  searchStats       = SearchStats{};
  lastUciUpdateTime = startTime;
  initialize();

  // setup and report search limits
  setupSearchLimits(position, searchLimits);

  // when not pondering and search is time controlled start timer
  if (searchLimits.timeControl && !searchLimits.ponder) {
    startTimer();
  }

  // check for opening book move when we have a time controlled game
  Move bookMove = MOVE_NONE;
  if (book && SearchConfig::USE_BOOK && searchLimits.timeControl) {
    bookMove = book->getRandomMove(position.getZobristKey());
    LOG__DEBUG(Logger::get().SEARCH_LOG, "Opening Book: Choosing book move " + str(bookMove));
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Opening Book: Not using book.");
  }

  // age tt entries
  if (tt) {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table: Using TT: " + tt->str());
    tt->ageEntries();
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table: Not using TT.");
  }

  LOG__INFO(Logger::get().SEARCH_LOG, fmt::format("Search using: PVS={} ASP={}", SearchConfig::USE_PVS, SearchConfig::USE_ASP));

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

  // update search result with search time and pv
  searchResult.time = std::chrono::high_resolution_clock::now() - startTime;
  searchResult.pv   = pv[0];

  // print stats to log
  //  LOG__INFO(Logger::get().SEARCH_LOG, "Search finished after {})",
  //            (searchResult.time % 1'000'000) / 1'000, (searchResult.time % 1'000));
  //  LOG__INFO(Logger::get().SEARCH_LOG, "Search depth was {}({}) with {} nodes visited. NPS = {} nps)",
  //            searchStats.currentSearchDepth, searchStats.currentExtraSearchDepth, nodesVisited,
  //            (nodesVisited * 1'000'000) / (searchResult.time + std::chrono::nanoseconds{1}).count());
  //  LOG__DEBUG(Logger::get().SEARCH_LOG, "Search stats: {}", searchStats.str());

  // print result to log
  LOG__INFO(Logger::get().SEARCH_LOG, "Search result: {}", searchResult.str());

  // save result until overwritten by the next search
  lastSearchResult = searchResult;
  hasResultFlag    = true;

  // Clean up
  // make sure timer stops as this could potentially still be running
  // when search finished without any stop signal/limit
  stopSearchFlag = true;

  // At the end of a search we send the result in any case even if
  // searched has been stopped.
  sendResult(searchResult);

  // release the running semaphore after the search has ended
  isRunningSemaphore.release();
}

SearchResult Search::iterativeDeepening(Position& position) {
  return SearchResult();
}

void Search::initialize() {
  // init opening book
  if (SearchConfig::USE_BOOK) {
    if (!book) {
      book = std::make_unique<OpeningBook>(SearchConfig::BOOK_PATH, SearchConfig::BOOK_TYPE);
      book->initialize();
    }
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Opening Book disabled in configuration");
  }

  // init transposition table
  if (SearchConfig::USE_TT) {
    if (!tt) {
      const int ttSizeMb = SearchConfig::TT_SIZE_MB;
      tt                 = std::make_unique<TT>(ttSizeMb);
    }
  }
  else {
    LOG__INFO(Logger::get().SEARCH_LOG, "Transposition Table disabled in configuration");
  }

  // init evaluator
  if (!evaluator) {
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

void Search::setupSearchLimits(Position& position, SearchLimits& sl) {
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
    timeLimit = setupTimeControl(position, sl);
    extraTime = MilliSec{0};
    if (sl.moveTime.count() > 0) {
      LOG__INFO(Logger::get().SEARCH_LOG, "Search mode: Time Controlled: Time per Move {}", sl.moveTime.count());
    }
    // TODO implement
  }
  // TODO implement
}

MilliSec Search::setupTimeControl(Position& position, SearchLimits& limits) {
  return MilliSec{};
}

void Search::startTimer() {
  // TODO implement
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
