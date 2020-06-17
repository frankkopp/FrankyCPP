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

  // reset stop flag
  _stopSearchFlag = false;

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
  _stopSearchFlag = false;
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

void Search::run() {
  LOG__TRACE(Logger::get().SEARCH_LOG, "Search thread started.");
}
void Search::startTimer() {
}

void Search::sendReadyOk() const {
  if (uciHandler) {
    uciHandler->sendReadyOk();
  }
  else {
    LOG__DEBUG(Logger::get().SEARCH_LOG, "uci >> readyok");
  }
}

void Search::sendString(const std::string& msg) const {
  if (uciHandler) {
    uciHandler->sendString(msg);
  }
  else {
    LOG__DEBUG(Logger::get().SEARCH_LOG, "uci >> " + msg);
  }
}