/*
 * MIT License
 *
 * Copyright (c) 2018-2020 Frank Kopp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
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

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <regex>
#include <thread>

#include "types/types.h"
#include "common/misc.h"
#include "common/Fifo.h"
#include "common/Logging.h"
#include "common/ThreadPool.h"
#include "chesscore/Position.h"
#include "PGN_Reader.h"
#include "OpeningBook.h"

// BOOST filesystem
#include <boost/filesystem.hpp>
namespace bfs = boost::filesystem;

// BOOST threads helper
#include <boost/thread/thread_functors.hpp>

// BOOST Serialization
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

// enable for parallel processing of input lines
#define PARALLEL_LINE_PROCESSING
// enable for parallel processing of games from pgn files
#define PARALLEL_GAME_PROCESSING
// enable for using fifo when reading pgn files and processing pgn games
// this allows to start processing games in parallel while still reading
//#define FIFO_PROCESSING

// not all C++17 compilers have this std library for parallelization
#ifdef HAS_EXECUTION_LIB
#include <execution>
#include <utility>
#endif

// the extension cache files use after the given opening book filename
static const char* const cacheExt = ".cache.bin";

// //////////////////////////////////////////////
// /// PUBLIC

OpeningBook::OpeningBook(std::string bookPath, const BookFormat &bFormat)
  :  bookFormat(bFormat), bookFilePath(std::move(bookPath)) {
  // std::thread::hardware_concurrency() is not reliable - on some platforms
  // it returns 0 - in this case we chose a default of 4
  numberOfThreads = std::thread::hardware_concurrency() == 0 ?
                    4 : std::thread::hardware_concurrency();

  // set root entry
  Position position;
  bookMap.emplace(position.getZobristKey(), BookEntry(position.getZobristKey(), position.strFen()));
}

Move OpeningBook::getRandomMove(Key zobrist) const {
  Move bookMove = MOVE_NONE;
  // Find the entry for this key (zobrist key of position) in the map and
  // choose a random move from the list of moves in the entry
  if (bookMap.find(zobrist) != bookMap.end()) {
    BookEntry bookEntry = bookMap.at(zobrist);
    if (!bookEntry.moves.empty()) {
      std::random_device rd;
      std::uniform_int_distribution<std::size_t> random(0, bookEntry.moves.size() - 1);
      bookMove = bookEntry.moves[random(rd)];
    }
  }
  return bookMove;
}

void OpeningBook::initialize() {
  if (isInitialized) return;
  LOG__INFO(Logger::get().BOOK_LOG, "Opening book initialization.");

  const auto start = std::chrono::high_resolution_clock::now();

  // if cache enabled check if we have a cache file and load from cache
  if (_useCache && !_recreateCache && hasCache()) {
    if (loadFromCache()) return;
  }

  readBookFromFile(bookFilePath);

  // safe the book to a cache
  if (_useCache && bookMap.size() > 1) {
    saveToCache();
  }

  const auto stop = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
  LOG__INFO(Logger::get().BOOK_LOG, "Opening book initialized in ({:L} ms). {:L} positions", elapsed.count(), bookMap.size());
  isInitialized = true;
}

void OpeningBook::reset() {
  const std::scoped_lock<std::mutex> lock(bookMutex);
  bookMap.clear();
  gamesTotal = 0;
  gamesProcessed = 0;
  isInitialized = false;
  LOG__DEBUG(Logger::get().TEST_LOG, "Opening book reset: {:L}", bookMap.size());
}

// //////////////////////////////////////////////
// /// PRIVATE

/* open the file a read all lines into a vector and process all lines */
void OpeningBook::readBookFromFile(const std::string &filePath) {
  if (!fileExists(filePath)) {
    LOG__ERROR(Logger::get().BOOK_LOG, "Open book '{}' not found.", filePath);
    return;
  }
  fileSize = getFileSize(filePath);
  std::ifstream file(filePath);
  if (file.is_open()) {
    LOG__DEBUG(Logger::get().BOOK_LOG, "Open book '{}' with {:L} kB successful.", filePath, fileSize / 1024);
    std::vector<std::string> lines = getLinesFromFile(file);
    file.close();
    processAllLines(lines);
  }
  else {
    LOG__ERROR(Logger::get().BOOK_LOG, "Open book '{}' failed.", filePath);
    return;
  }
}

/* reads all lines from a file stream into a vector and returns them
   skips empty lines */
std::vector<std::string> OpeningBook::getLinesFromFile(std::ifstream &ifstream) const {
  LOG__DEBUG(Logger::get().BOOK_LOG, "Reading lines from book.");
  std::vector<std::string> lines;
  std::string line;
  const auto start = std::chrono::high_resolution_clock::now();
  lines.reserve(fileSize / 40);
  while (std::getline(ifstream, line)) {
    if (!line.empty()) lines.push_back(line);
  }
  const auto stop = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
  LOG__DEBUG(Logger::get().BOOK_LOG, "Read {:L} lines in {:L} ms.", lines.size(), elapsed.count());
  return lines;
}


/* processes the lines depending on file format.
   SIMPLE and SAN have all moves of a game in one single line.
   PGN has additional metadata and SA moves spread over several lines. */
void OpeningBook::processAllLines(std::vector<std::string> &lines) {
  LOG__DEBUG(Logger::get().BOOK_LOG, "Creating internal book...");

  const auto start = std::chrono::high_resolution_clock::now();

  // process all lines from the opening book file depending on format
  switch (bookFormat) {
    case BookFormat::SIMPLE:
    case BookFormat::SAN: {
#ifdef PARALLEL_LINE_PROCESSING
      LOG__DEBUG(Logger::get().BOOK_LOG, "Using {} threads", numberOfThreads);

#ifdef HAS_EXECUTION_LIB // use parallel lambda 
      std::for_each(std::execution::par_unseq, lines.begin(), lines.end(),
                    [&](auto &&item) { processLine(item); });
#else // no <execution> library (< C++17)
      const auto maxNumberOfEntries = lines.size();
      std::vector<std::thread> threads;
      threads.reserve(numberOfThreads);
      for (unsigned int t = 0; t < numberOfThreads; ++t) {
        threads.emplace_back([&, this, t]() {
          auto range = maxNumberOfEntries / numberOfThreads;
          auto startIter = t * range;
          auto end = startIter + range;
          if (t == numberOfThreads - 1) end = maxNumberOfEntries;
          for (std::size_t i = startIter; i < end; ++i) {
            processLine(lines[i]);
          }
        });
      }
      for (std::thread &th: threads) th.join();
#endif
#else // no parallel execution
      for (auto l : lines) {
        processLine(l);
      }
#endif
      break;
    }
    case BookFormat::PGN:
#ifdef FIFO_PROCESSING
      processPGNFileFifo(lines);
#else
      processPGNFile(lines);
#endif
      break;
  }

  const auto stop = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
  LOG__DEBUG(Logger::get().BOOK_LOG, "Internal book created {:L} positions in {:L} ms.", bookMap.size(), elapsed.count());
}

/* process each line depending on format */
void OpeningBook::processLine(std::string &line) {
  LOG__TRACE(Logger::get().BOOK_LOG, "Processing line: {}", line);
  const std::regex whiteSpaceTrim(R"(^\s*(.*)\s*$)"); // clean up line
  line = std::regex_replace(line, whiteSpaceTrim, "$1");
  switch (bookFormat) {
    case BookFormat::SIMPLE:
      processSimpleLine(line);
      break;
    case BookFormat::SAN:
      processSANLine(line);
      break;
    default:
      LOG__ERROR(Logger::get().BOOK_LOG, "Line processing only for SIMPLE or SAN file format");
      break;
  }
}

/* process a line from SIMPLE format and build internal book data structure */
void OpeningBook::processSimpleLine(std::string &line) {
  std::smatch matcher;

  // check if line starts with move
  const std::regex startPattern(R"(^[a-h][1-8][a-h][1-8].*$)");
  if (!std::regex_match(line, matcher, startPattern)) {
    LOG__TRACE(Logger::get().BOOK_LOG, "Line ignored: {}", line);
    return;
  }

  // iterate over all found pattern matches (aka moves)
  const std::regex movePattern(R"([a-h][1-8][a-h][1-8])");
  auto move_begin = std::sregex_iterator(line.begin(), line.end(), movePattern);
  auto move_end = std::sregex_iterator();
  LOG__TRACE(Logger::get().BOOK_LOG, "Found {} moves in line: {}", std::distance(move_begin, move_end), line);

  Position currentPosition; // start position
  for (auto i = move_begin; i != move_end; ++i) {
    const std::string &moveStr = (*i).str();
    LOG__TRACE(Logger::get().BOOK_LOG, "Moves {}", moveStr);

    // create and validate the move
    Move move = mg.getMoveFromUci(currentPosition, moveStr);
    if (!validMove(move)) {
      LOG__WARN(Logger::get().BOOK_LOG, "Not a valid move {} on this position {}", moveStr, currentPosition.strFen());
      return;
    }

    addToBook(currentPosition, move);
  }
}

/* process a line from SAN format and build internal book data structure */
void OpeningBook::processSANLine(std::string &line) {
  std::smatch matcher;

  // check if line starts valid
  const std::regex startLineRegex(R"(^\d+\. )");
  if (std::regex_match(line, matcher, startLineRegex)) {
    LOG__TRACE(Logger::get().BOOK_LOG, "Line ignored: {}", line);
    return;
  }

  /*
  Iterate over all tokens, ignore move numbers and results
  Example:
  1. f4 d5 2. Nf3 Nf6 3. e3 g6 4. b3 Bg7 5. Bb2 O-O 6. Be2 c5 7. O-O Nc6 8. Ne5 Qc7 1/2-1/2
  1. f4 d5 2. Nf3 Nf6 3. e3 Bg4 4. Be2 e6 5. O-O Bd6 6. b3 O-O 7. Bb2 c5 1/2-1/2
  */

  // ignore patterns
  const std::regex numberRegex(R"(^\d+\.)");
  const std::regex resultRegex(R"((1/2|1|0)-(1/2|1|0))");

  // split at every whitespace and iterate through items
  const std::regex splitRegex(R"(\s+)");
  const std::sregex_token_iterator iter(line.begin(), line.end(), splitRegex, -1);
  const std::sregex_token_iterator end;

  Position currentPosition; // start position
  LOG__TRACE(Logger::get().BOOK_LOG, "Found {} items in line: {}", std::distance(iter, end), line);
  for (auto i = iter; i != end; ++i) {
    const std::string &moveStr = (*i).str();
    LOG__TRACE(Logger::get().BOOK_LOG, "Item {}", moveStr);
    if (std::regex_match(moveStr, matcher, numberRegex)) { continue; }
    if (std::regex_match(moveStr, matcher, resultRegex)) { continue; }
    LOG__TRACE(Logger::get().BOOK_LOG, "SAN Move {}", moveStr);

    // create and validate the move
    Move move = mg.getMoveFromSan(currentPosition, moveStr);
    if (!validMove(move)) {
      LOG__WARN(Logger::get().BOOK_LOG, "Not a valid move {} on this position {}", moveStr, currentPosition.strFen());
      return;
    }
    LOG__TRACE(Logger::get().BOOK_LOG, "Move found {}", printMoveVerbose(move));

    addToBook(currentPosition, move);
  }
}

/* Reads lines to find multiline PGN games (metadata and moves)
   Every game found will be put into a fifo queue and a parallel thread pool
   of workers take the games to process them to build up internal book
   data structure. */
void OpeningBook::processPGNFileFifo(std::vector<std::string> &lines) {
  LOG__DEBUG(Logger::get().BOOK_LOG, "Process lines from PGN file with FIFO ({} Threads)...", numberOfThreads);
  // reading pgn and get a list of games
  PGN_Reader pgnReader(lines);
  // prepare FIFO for storing the games
  Fifo<PGN_Game> gamesFifo;
  // prepare thread pool
  ThreadPool threadPool(numberOfThreads);
  // flag to signal if all pgn lines have been processed
  bool finished = false;
  // prepare worker for processing found games
  for (unsigned int i = 0; i < numberOfThreads; i++) {
    threadPool.enqueue([&] {
      while (!gamesFifo.isClosed()) {
        LOG__TRACE(Logger::get().BOOK_LOG, "Get game...");
        auto game = gamesFifo.pop_wait();
        if (game.has_value()) { // no value means pop_wait has been canceled
          LOG__TRACE(Logger::get().BOOK_LOG, "Got game...");
          processGame(game.value());
          LOG__TRACE(Logger::get().BOOK_LOG, "Processed game...Book now at {:L} entries.", bookMap.size());
        }
        else {
          LOG__TRACE(Logger::get().BOOK_LOG, "Game NULL");
        }
      }
    });
  }

  // start finding games asynchronously and put games into the FIFO
  std::future<bool> future = std::async(std::launch::async, [&] {
    LOG__DEBUG(Logger::get().BOOK_LOG, "Start finding games");
    finished = pgnReader.process(gamesFifo);
    LOG__DEBUG(Logger::get().BOOK_LOG, "Finished finding games {}", finished);
    return finished;
  });

  // wait until all games have been put into the FIFO
  if (future.get()) {
    // busy wait until FIFO is empty
    while (!gamesFifo.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    LOG__DEBUG(Logger::get().BOOK_LOG, "Finished processing games.");
    gamesFifo.close();
    LOG__DEBUG(Logger::get().BOOK_LOG, "Closed down ThreadPool");
  }
}

/* Reads lines to find multiline PGN games (metadata and moves)
   Every game found will be put into a vector. After reading all games a
   parallel thread pool of workers take the games to process them to build
   up internal book data structure. */
void OpeningBook::processPGNFile(std::vector<std::string> &lines) {
  LOG__DEBUG(Logger::get().BOOK_LOG, "Process lines from PGN file...");
  // reading pgn and get a list of games
  PGN_Reader pgnReader(lines);
  if (!pgnReader.process()) {
    LOG__ERROR(Logger::get().BOOK_LOG, "Could not process lines from PGN file.");
    return;
  }
  auto *ptrGames = &pgnReader.getGames();
  gamesTotal = ptrGames->size();
  // process all games
  processGames(ptrGames);
}

/* After reading all games (non fifo) a parallel thread pool of workers take the games
 * to process them to build up internal book data structure. */
void OpeningBook::processGames(std::vector<PGN_Game>* ptrGames) {// processing games
  LOG__DEBUG(Logger::get().BOOK_LOG, "Processing {:L} games", ptrGames->size());
  const auto startTime = std::chrono::high_resolution_clock::now();

#ifdef PARALLEL_GAME_PROCESSING
  LOG__DEBUG(Logger::get().BOOK_LOG, "Using {} threads", numberOfThreads);
#ifdef HAS_EXECUTION_LIB // parallel lambda
  std::for_each(std::execution::par_unseq, ptrGames->begin(), ptrGames->end(),
                [&](auto game) { processGame(game); });
#else // no <execution> library (< C++17)
  const auto noOfThreads = std::thread::hardware_concurrency() == 0 ?
                           4 : std::thread::hardware_concurrency();
  const unsigned int maxNumberOfEntries = ptrGames->size();
  std::vector<std::thread> threads;
  threads.reserve(noOfThreads);
  for (unsigned int t = 0; t < noOfThreads; ++t) {
    threads.emplace_back([&, this, t]() {
      auto range = maxNumberOfEntries / noOfThreads;
      auto start = t * range;
      auto end = start + range;
      if (t == noOfThreads - 1) end = maxNumberOfEntries;
      for (std::size_t i = start; i < end; ++i) {
        processGame((*ptrGames)[i]);
      }
    });
  }
  for (std::thread &th: threads) th.join();
#endif
#else // no parallel processing
  auto iterEnd = ptrGames->end();
  for (auto iter = ptrGames->begin(); iter < iterEnd; iter++) {
    processGame(*iter);
    LOG__TRACE(Logger::get().BOOK_LOG, "Process game finished - games={:L} book={:L}", gamesProcessed, bookMap.size());
  }
#endif

  const auto stopTime = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime);
  LOG__INFO(Logger::get().BOOK_LOG, "Processed {:L} games in {:L} ms", gamesTotal, elapsed.count());
}

/* process a game from PGN SAN format and build internal book data structure */
void OpeningBook::processGame(const PGN_Game &game) {
  std::smatch matcher;
  const std::regex UCIRegex(R"(([a-h][1-8][a-h][1-8])([NBRQnbrq])?)");
  const std::regex SANRegex(R"(([NBRQK])?([a-h])?([1-8])?x?([a-h][1-8]|O-O-O|O-O)(=?([NBRQ]))?([!?+#]*)?)");

  Position currentPosition; // start position
  for (const auto& moveStr : game.moves) {
    Move move = MOVE_NONE;

    // check the notation format
    // Per PGN it must be SAN but some files have UCI notation
    // As UCI is pattern wise a subset of SAN we test for UCI first.  
    if (std::regex_match(moveStr, matcher, UCIRegex)) {
      LOG__TRACE(Logger::get().BOOK_LOG, "Game move {} is UCI", moveStr);
      move = mg.getMoveFromUci(currentPosition, moveStr);
    }
    else if (std::regex_match(moveStr, matcher, SANRegex)) {
      LOG__TRACE(Logger::get().BOOK_LOG, "Game move {} is SAN", moveStr);
      move = mg.getMoveFromSan(currentPosition, moveStr);
    }

    // validate the move
    if (move == MOVE_NONE) {
      LOG__WARN(Logger::get().BOOK_LOG, "Not a valid move {} on this position {}", moveStr, currentPosition.strFen());
      return;
    }
    LOG__TRACE(Logger::get().BOOK_LOG, "Move found {}", printMoveVerbose(move));

    addToBook(currentPosition, move);
  }
  gamesProcessed++;
#ifndef FIFO_PROCESSING
  // x % 0 is undefined in c++
  // avgLinesPerGameTimesProgressSteps = 12*15 as 12 is avg game lines and 15 steps
  const uint64_t progressInterval = 1 + (gamesTotal / 15);
  if (gamesProcessed % progressInterval == 0) {
    LOG__DEBUG(Logger::get().BOOK_LOG, "Process games: {:s}", printProgress(static_cast<double>(gamesProcessed) / gamesTotal));
  }
#endif
}

/* adds a position and adds the move and a pointer to the new position
   to the previous position's book entry. This is synchronized to be thread
   safe */
void OpeningBook::addToBook(Position &currentPosition, const Move &move) {
  // remember previous position
  const Key lastKey = currentPosition.getZobristKey();
  // make move on position to get new position
  currentPosition.doMove(move);
  const Key currentKey = currentPosition.getZobristKey();
  const std::string currentFen = currentPosition.strFen();

  // get the lock on the data map
  const std::scoped_lock<std::mutex> lock(bookMutex);

  // create or update book entry
  if (bookMap.count(currentKey)) {
    // pointer to entry already in book
    BookEntry* existingEntry = &bookMap.at(currentKey);
    existingEntry->counter++;
    LOG__TRACE(Logger::get().BOOK_LOG, "Position already existed {} times: {}", existingEntry->counter, existingEntry->fen);
  }
  else {
    // new position
    bookMap.emplace(currentKey, BookEntry(currentKey, currentFen));
    LOG__TRACE(Logger::get().BOOK_LOG, "Position new", currentKey);
  }

  // add move to the last book entry's move list
  BookEntry* lastEntry = &bookMap.at(lastKey);
  if (std::find(lastEntry->moves.begin(), lastEntry->moves.end(), move) == lastEntry->moves.end()) {
    lastEntry->moves.emplace_back(move);
    lastEntry->ptrNextPosition.emplace_back(std::make_shared<BookEntry>(bookMap.at(currentKey)));
    LOG__TRACE(Logger::get().BOOK_LOG, "Added move and pointer to last entry.");
  }
} // lock released

/* Saves the bookMap data to a binary cache file for faster reading.
   Uses BOOST serialization to serialize the data to a binary file */
void OpeningBook::saveToCache() {
  const std::scoped_lock<std::mutex> lock(bookMutex);
  { // save data to archive
    const auto start = std::chrono::high_resolution_clock::now();
    const std::string serCacheFile = bookFilePath + cacheExt;
    LOG__DEBUG(Logger::get().BOOK_LOG, "Saving book to cache file {}", serCacheFile);
    // create and open a binary archive for output
    std::ofstream ofsBin(serCacheFile, std::fstream::binary | std::fstream::out);
    boost::archive::binary_oarchive oa(ofsBin);
    // write class instance to archive
    oa << BOOST_SERIALIZATION_NVP(bookMap);
    const auto stop = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    LOG__DEBUG(Logger::get().BOOK_LOG, "Book saved to binary cache in ({:L} ms) ({})", elapsed.count(), serCacheFile);
  } // archive and stream closed when destructors are called
  // reset recreation flag
  _recreateCache = false;
  // this is redundant for testing purposes
  /*  { // save data to archive
      const auto start = std::chrono::high_resolution_clock::now();

      const std::basic_string<char> &txtCacheFile = bookFilePath + ".cache.txt";
      LOG__DEBUG(Logger::get().BOOK_LOG, "Saving to cache file {}", txtCacheFile);

      // create and open a character archive for output
      std::ofstream ofsTXT(txtCacheFile);
      boost::archive::text_oarchive oa(ofsTXT);
      // write class instance to archive
      oa << BOOST_SERIALIZATION_NVP(bookMap);

      const auto stop = std::chrono::high_resolution_clock::now();
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
      LOG__INFO(Logger::get().BOOK_LOG, "Opening book save to txt cache in ({:L} ms) ({})", elapsed.count(), txtCacheFile);
    } // archive and stream closed when destructors are called

    // this is redundant for testing purposes
    { // save data to archive
      const auto start = std::chrono::high_resolution_clock::now();

      const std::basic_string<char> &xmlCacheFile = bookFilePath + ".cache.xml";
      LOG__DEBUG(Logger::get().BOOK_LOG, "Saving to cache file {}", xmlCacheFile);

      // create and open a character archive for output
      std::ofstream ofsXML(xmlCacheFile);
      boost::archive::xml_oarchive oa(ofsXML);
      // write class instance to archive
      oa << BOOST_SERIALIZATION_NVP(bookMap);

      const auto stop = std::chrono::high_resolution_clock::now();
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
      LOG__INFO(Logger::get().BOOK_LOG, "Opening book saved to xml cache in ({:L} ms) ({})", elapsed.count(), xmlCacheFile);
    } // archive and stream closed when destructors are called*/
}

/* Loads the bookMap data from a binary data cache file. This is considerably
   faster than reading the text based game files again */
bool OpeningBook::loadFromCache() {
  const std::scoped_lock<std::mutex> lock(bookMutex);
  std::unordered_map<Key, BookEntry> binMap;
  { // load data from archive
    const auto start = std::chrono::high_resolution_clock::now();
    const std::string serCacheFile = bookFilePath + cacheExt;
    LOG__DEBUG(Logger::get().BOOK_LOG, "Loading from cache file {} ({:L} kB)", serCacheFile, getFileSize(serCacheFile) / 1'024);
    // create and open a binary archive for input
    std::ifstream ifsBin(serCacheFile, std::fstream::binary | std::fstream::in);
    if (!ifsBin.is_open() || !ifsBin.good()) {
      LOG__ERROR(Logger::get().BOOK_LOG, "Loading from cache file {} failed", serCacheFile);
      return false;
    }
    boost::archive::binary_iarchive ia(ifsBin);
    // write archive to class instance
    ia >> BOOST_SERIALIZATION_NVP(binMap);
    const auto stop = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    LOG__INFO(Logger::get().BOOK_LOG,
              "Book loaded from cache with {:L} entries in ({:L} ms) ({})",
              binMap.size(), elapsed.count(), serCacheFile);
  }
  /*std::unordered_map<Key, BookEntry> txtMap;
  {
    const auto start = std::chrono::high_resolution_clock::now();

    // create and open a txt archive for input
    const std::basic_string<char> &txtCacheFile = bookFilePath + ".cache.txt";
    LOG__DEBUG(Logger::get().BOOK_LOG, "Loading from cache file {}", txtCacheFile);
    std::ifstream ifsTXT(txtCacheFile);
    if (!ifsTXT.good()) {
      LOG__ERROR(Logger::get().BOOK_LOG, "Loading from cache file {} failed", txtCacheFile);
      return;
    }
    boost::archive::text_iarchive ia(ifsTXT);
    // write archive to class instance
    ia >> BOOST_SERIALIZATION_NVP(txtMap);

    const auto stop = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    LOG__INFO(Logger::get().BOOK_LOG,
              "Opening book loaded from txt cache with {:L} entries in ({:L} ms) ({})",
              txtMap.size(), elapsed.count(), txtCacheFile);
  }

  std::unordered_map<Key, BookEntry> xmlMap;
  {
    const auto start = std::chrono::high_resolution_clock::now();

    // create and open a xml archive for input
    const std::basic_string<char> &xmlCacheFile = bookFilePath + ".cache.xml";
    LOG__DEBUG(Logger::get().BOOK_LOG, "Loading from cache file {}", xmlCacheFile);
    std::ifstream ifsXML(xmlCacheFile);
    if (!ifsXML.good()) {
      LOG__ERROR(Logger::get().BOOK_LOG, "Loading from cache file {} failed", xmlCacheFile);
      return;
    }
    boost::archive::xml_iarchive ia(ifsXML);
    // write archive to class instance
    ia >> BOOST_SERIALIZATION_NVP(xmlMap);

    const auto stop = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    LOG__INFO(Logger::get().BOOK_LOG,
              "Opening book loaded from xml cache with {:L} entries in ({:L} ms) ({})",
              xmlMap.size(), elapsed.count(), xmlCacheFile);
  }*/
  bookMap = std::move(binMap);
  return true;
} // archive and stream closed when destructors are called

/* checks if a cache file exists */
bool OpeningBook::hasCache() const {
  const std::basic_string<char> serCacheFile = bookFilePath + cacheExt;
  if (!fileExists(serCacheFile)) {
    LOG__DEBUG(Logger::get().BOOK_LOG, "No cache file {} available", serCacheFile);
    return false;
  }
  uint64_t fsize = getFileSize(serCacheFile);
  LOG__DEBUG(Logger::get().BOOK_LOG, "Cache file {} ({:L} kB) available", serCacheFile, fsize / 1'024);
  return true;
}

/** Checks of file exists and encapsulates platform differences for
 * filesystem operations */
bool OpeningBook::fileExists(const std::string& filePath) {
  bfs::path p{filePath};
  return bfs::exists(filePath);
}

/** Returns files size in bytes and encapsulates platform differences for
 * filesystem operations */
uint64_t OpeningBook::getFileSize(const std::string &filePath) {
  bfs::path p{filePath};
  uint64_t fsize = bfs::file_size(bfs::canonical(p));
  return fsize;
}


