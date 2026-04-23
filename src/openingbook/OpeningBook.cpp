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

#include "openingbook/OpeningBook.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"
#include "common/Logging.h"
#include "common/stringutil.h"
#include "types/types.h"

using namespace book;
using namespace chess;
using namespace common;

namespace pgn = common::pgn;

// BOOST Serialization
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <future>
#include <memory>
#include <random>
#include <string>

// enable for parallel processing of input lines
#define PARALLEL_LINE_PROCESSING

// not all C++17 compilers have this std library for parallelization
// #undef HAS_EXECUTION_LIB
#ifdef HAS_EXECUTION_LIB
#include <execution>
#include <utility>
#endif

// //////////////////////////////////////////////
// /// PUBLIC

OpeningBook::OpeningBook(std::string bookPath, const BookFormat bFormat)
    : bookFormat(bFormat), bookFilePath(std::move(bookPath)), numberOfThreads(getNoOfThreads()) {
}

Move OpeningBook::getRandomMove(const ZobristKey zobrist) const {
  Move bookMove = MOVE_NONE;
  // Find the entry for this key (zobrist key of position) in the map and
  // choose a random move from the list of moves in the entry
  const auto iterator = bookMap.find(zobrist);
  if (iterator != bookMap.end() && !iterator->second.moves.empty()) {
    std::random_device rd;
    std::uniform_int_distribution<std::size_t> random(0, iterator->second.moves.size() - 1);
    bookMove = iterator->second.moves[random(rd)];
  }
  return bookMove;
}

Move OpeningBook::getBookMove(const ZobristKey zobrist, const int variety) const {
  const auto iterator = bookMap.find(zobrist);
  if (iterator == bookMap.end() || iterator->second.moves.empty()) {
    return MOVE_NONE;
  }

  const auto& entry = iterator->second;
  const auto numMoves = entry.moves.size();

  // Single move — no choice needed
  if (numMoves == 1) {
    return entry.moves[0];
  }

  // Look up destination-position frequency for each move
  std::vector<double> freqs(numMoves);
  for (std::size_t i = 0; i < numMoves; ++i) {
    freqs[i] = 1.0; // default if destination not found
    const auto destIt = bookMap.find(entry.nextPosition[i]);
    if (destIt != bookMap.end()) {
      freqs[i] = static_cast<double>(std::max(destIt->second.counter, 1));
    }
  }

  // variety=0: pick randomly among the moves tied for the highest frequency
  const int v = std::clamp(variety, 0, 100);
  if (v == 0) {
    const double maxFreq = *std::ranges::max_element(freqs);
    std::vector<std::size_t> bestIndices;
    for (std::size_t i = 0; i < numMoves; ++i) {
      if (freqs[i] >= maxFreq) {
        bestIndices.push_back(i);
      }
    }
    std::random_device rd;
    std::uniform_int_distribution<std::size_t> dist(0, bestIndices.size() - 1);
    return entry.moves[bestIndices[dist(rd)]];
  }

  // variety=100: pure uniform random (ignore frequencies)
  if (v == 100) {
    std::random_device rd;
    std::uniform_int_distribution<std::size_t> dist(0, numMoves - 1);
    return entry.moves[dist(rd)];
  }

  // Blend frequency weights with uniform: weight = (1 - t) * freq + t * 1.0
  const double t = v / 100.0;
  std::vector<double> weights(numMoves);
  for (std::size_t i = 0; i < numMoves; ++i) {
    weights[i] = (1.0 - t) * freqs[i] + t * 1.0;
  }

  std::random_device rd;
  std::discrete_distribution<std::size_t> dist(weights.begin(), weights.end());
  return entry.moves[dist(rd)];
}

void OpeningBook::initialize() {
  if (isInitialized) {
    LOG__WARN(Logger::get().BOOK_LOG, "Opening book already initialized. Call to initialize ignored.");
    return;
  }
  LOG__INFO(Logger::get().BOOK_LOG, "Opening book initialization.");

  const auto start = high_resolution_clock::now();

  // if cache enabled check if we have a cache file and load from the cache
  if (_useCache && !_recreateCache && hasCache()) {
    if (loadFromCache()) return;
  }

  // load the whole file into memory line by line
  const auto lines = readFile(bookFilePath);

  // set root entry
  auto [fst, _]       = bookMap.emplace(rootZobristKey, rootZobristKey);
  fst->second.counter = 0;

  // reads lines and retrieves game (lists of moves) and adds these to the book map
  readGames(lines);

  // release memory from initial file load
  data.reset(nullptr);

  // safe the book to a cache
  if (_useCache && bookMap.size() > 1) {
    saveToCache();
  }

  const auto stop    = high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<milliseconds>(stop - start);

  isInitialized = true;
  LOG__INFO(Logger::get().BOOK_LOG, "Opening book initialized in ({:L} ms). {:L} positions", elapsed.count(), bookMap.size());
}

void OpeningBook::reset() {
  bookMap.clear();
  data.reset(nullptr);
  isInitialized = false;
  LOG__DEBUG(Logger::get().TEST_LOG, "Opening book reset: {:L} entries", bookMap.size());
}

std::string OpeningBook::str(const int level) {
  const Position p{};
  const ZobristKey zobristKey = p.getZobristKey();
  const BookEntry* node       = &bookMap[zobristKey];
  return std::format(projectLocale, "Root ({:L})\n{}", bookMap[zobristKey].counter, getLevelStr(1, level, node));
}

std::string OpeningBook::getLevelStr(int level, const int maxLevel, const BookEntry* node) {
  std::string out;
  const size_t size = node->moves.size();
  for (int i = 0; i < size; i++) {
    const BookEntry* newNode = &bookMap[(node->nextPosition)[i]];
    out += std::format(projectLocale, "{:{}}{} ({:L})\n", "", level, node->moves[i].str(), newNode->counter);
    if (level < maxLevel) {
      out += getLevelStr(level + 1, maxLevel, newNode);
    }
  }
  return out;
}

// //////////////////////////////////////////////
// /// PRIVATE

std::vector<std::string_view> OpeningBook::readFile(const std::string& filePath) {

  std::vector<std::string_view> lines{};

  if (!std::filesystem::exists(filePath)) {
    LOG__ERROR(Logger::get().BOOK_LOG, "Opening Book '{}' not found. Using empty book.", filePath);
    return lines;
  }

  const auto start = high_resolution_clock::now();

  std::fstream file(filePath, std::ios::in | std::ios::binary);
  if (file.is_open()) {
    const uint64_t fileSize = std::filesystem::file_size(filePath);
    (void) fileSize;
    LOG__DEBUG(Logger::get().BOOK_LOG, "Opened Opening Book '{}' with {:L} Byte successful.", filePath, fileSize);

    // fast way to read all lines from a file into memory
    // https://stackoverflow.com/a/52699885/9161706
    file.seekg(0, std::ios::beg);
    file.seekg(0, std::ios::end);
    const std::streamsize data_size = file.tellg();
    file.seekg(0, std::ios::beg);
    data = std::make_unique<char[]>(data_size); // NOLINT(*-avoid-c-arrays)
    file.read(data.get(), data_size);
    lines.reserve(data_size / 20);
    for (std::streamsize i = 0, dstart = 0; i < data_size; ++i) {
      if (data[i] == '\n' || i == data_size - 1) { // End of line, got string
        lines.emplace_back(data.get() + dstart, i - dstart);
        dstart = i + 1;
      }
    }

    const auto stop    = high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<milliseconds>(stop - start);
    (void) elapsed;
    LOG__DEBUG(Logger::get().BOOK_LOG, "Read {:L} lines in {:L} ms.", lines.size(), elapsed.count());

    file.close();
  }
  else {
    LOG__ERROR(Logger::get().BOOK_LOG, "Could not open Opening Book '{}' ", filePath);
    return lines;
  }

  return lines;
}

void OpeningBook::readGames(const std::vector<std::string_view>& lines) {
  LOG__DEBUG(Logger::get().BOOK_LOG, "Reading games...");

  const auto start = high_resolution_clock::now();

  // process all lines from the opening book file depending on format
  switch (bookFormat) {
    case BookFormat::SIMPLE:
      readGamesSimple(lines);
      break;
    case BookFormat::SAN: {
      readGamesSan(lines);
      break;
    }
    case BookFormat::PGN:
      readGamesPgn(lines);
      break;
  }

  const auto stop    = high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<milliseconds>(stop - start);
  (void) elapsed;
  LOG__DEBUG(Logger::get().BOOK_LOG, "Read games in {}.", ::str(elapsed));
}

void OpeningBook::readGamesSimple(const std::vector<std::string_view>& lines) {


#ifdef PARALLEL_LINE_PROCESSING
  const unsigned int noOfThreads = getNoOfThreads();
  LOG__DEBUG(Logger::get().BOOK_LOG, "Using {} threads", noOfThreads);

#ifdef HAS_EXECUTION_LIB // use parallel lambda
  std::for_each(std::execution::par_unseq, lines.begin(), lines.end(),
                [&](auto&& line) {
                  readOneGameSimple(line);
                });
#else // no <execution> library (< C++17)
  const auto noOfLines = lines.size();
  std::vector<std::thread> threads;
  threads.reserve(noOfThreads);
  for (unsigned int t = 0; t < noOfThreads; ++t) {
    threads.emplace_back([&, this, t]() {
      auto range     = noOfLines / numberOfThreads;
      auto startIter = t * range;
      auto end       = startIter + range;
      if (t == numberOfThreads - 1) end = noOfLines;
      for (std::size_t i = startIter; i < end; ++i) {
        readOneGameSimple(lines[i]);
      }
    });
  }
  for (std::thread& th : threads) th.join();
#endif

#else // no parallel execution
  for (auto line : lines) {
    readOneGameSimple(line);
  }
#endif
}

void OpeningBook::readOneGameSimple(const std::string_view& lineView) {
  Moves game{};

  // trim line
  const auto lineViewTrimmed = trimFast(lineView);

  // simple lines are in tuples of 4 per move
  // read in 4 characters and check if they might
  // be moves (letter, digit, letter, digit)
  // checks if they are indeed valid moves happens when trying to add
  // the move to the book map
  int index = 0;
  while (index < lineViewTrimmed.length()) {
    const auto moveStr = lineViewTrimmed.substr(index, 4);
    index += 4;
    // check basic format
    if (!(isalpha(moveStr[0]) && isdigit(moveStr[1]) && isalpha(moveStr[2]) && isdigit(moveStr[3]))) {
      break;
    }
    // add the valid formatted moves strings to the game
    game.emplace_back(std::string{moveStr});
  }

  // add game to book
  if (!game.empty()) {
    addGameToBook(game);
  }
}

void OpeningBook::readGamesSan(const std::vector<std::string_view>& lines) {

#ifdef PARALLEL_LINE_PROCESSING
  const unsigned int noOfThreads = getNoOfThreads();
  LOG__DEBUG(Logger::get().BOOK_LOG, "Using {} threads", noOfThreads);

#ifdef HAS_EXECUTION_LIB // use parallel lambda
  std::for_each(std::execution::par_unseq, lines.begin(), lines.end(),
                [&](auto&& line) {
                  readOneGameSan(line);
                });
#else // no <execution> library (< C++17)
  const auto noOfLines = lines.size();
  std::vector<std::thread> threads;
  threads.reserve(noOfThreads);
  for (unsigned int t = 0; t < noOfThreads; ++t) {
    threads.emplace_back([&, this, t]() {
      auto range     = noOfLines / numberOfThreads;
      auto startIter = t * range;
      auto end       = startIter + range;
      if (t == numberOfThreads - 1) end = noOfLines;
      for (std::size_t i = startIter; i < end; ++i) {
        readOneGameSan(lines[i]);
      }
    });
  }
  for (std::thread& th : threads) th.join();
#endif

#else // no parallel execution
  for (auto line : lines) {
    readOneGameSan(line);
  }
#endif
}

void OpeningBook::readOneGameSan(const std::string_view& lineView) {
  Moves game{};

  // create a trimmed copy of the string
  const auto line{trimFast(lineView)};

  // check if the line starts at least with a number or a character
  if (!isalnum(line[0])) return;

  /*
  Iterate over all tokens, ignore move numbers and results
  Example:
  1. f4 d5 2. Nf3 Nf6 3. e3 g6 4. b3 Bg7 5. Bb2 O-O 6. Be2 c5 7. O-O Nc6 8. Ne5 Qc7 1/2-1/2
  1. f4 d5 2. Nf3 Nf6 3. e3 Bg4 4. Be2 e6 5. O-O Bd6 6. b3 O-O 7. Bb2 c5 1/2-1/2
  */

  std::vector<std::string_view> moveStrings{};
  splitFast(line, moveStrings, " ");

  for (const auto& moveStr : moveStrings) {
    if (moveStr.empty() || moveStr.length() == 1) continue;
    if (!isalpha(moveStr[0])) continue;
    // add the validated move to the game and commit the move to the current position
    game.emplace_back(std::string{moveStr});
  }

  // add game to book
  addGameToBook(game);
}

void OpeningBook::readGamesPgn(const std::vector<std::string_view>& lines) {
  // Use the shared PGN parser to parse games from lines
  pgn::PgnParser parser;

  // Collect all games first (parser is sequential)
  const auto games = parser.parseFromLines(lines);

  // Add each game's moves to the book (can be parallelized if needed)
#ifdef PARALLEL_LINE_PROCESSING
  // Process games in parallel using async
  std::vector<std::future<void>> futures;
  futures.reserve(games.size());

  for (const auto& game : games) {
    futures.push_back(std::async(std::launch::async, [this, &game] {
      addGameToBook(game.moves);
    }));
  }

  // Wait for all games to be added
  for (auto& future : futures) {
    future.get();
  }
#else
  // Sequential processing
  for (const auto& game : games) {
    addGameToBook(game.moves);
  }
#endif
}

void OpeningBook::addGameToBook(const Moves& game) {
  if (game.empty()) {
    return;
  }

  Position p{};
  MoveGenerator mg{};

  // initialize lastKey with start position (aka root position)
  ZobristKey lastKey = rootZobristKey;
  // increase counter for root entry for each game
  { // lock scope
#ifdef PARALLEL_LINE_PROCESSING
    std::lock_guard<std::mutex> bookLock(bookMutex);
#endif
    bookMap[lastKey].counter++;
  }

  // Loop through all move strings and try to find a matching move on the current position.
  // If found add the move to the book.
  for (const std::string& moveStr : game) {
    Move move;

    // check the notation format
    if ((moveStr.size() == 4 && islower(moveStr[0]) && isdigit(moveStr[1]) && islower(moveStr[2]) && isdigit(moveStr[3])) || (moveStr.size() == 5 && islower(moveStr[0]) && isdigit(moveStr[1]) && islower(moveStr[2]) && isdigit(moveStr[3]) && isupper(moveStr[4]))) {
      // UCI
      move = mg.getMoveFromUci(p, moveStr);
    }
    else {
      // SAN
      move = mg.getMoveFromSan(p, moveStr);
    }
    if (move == MOVE_NONE) {
      LOG__WARN(Logger::get().BOOK_LOG, "Not a valid move {} on this position {}", moveStr, p.strFen());
      break;
    }

    // and make move on position to get new position
    p.doMove(move);
    // writes move to book map takes care of concurrent locking
    writeToBook(move, p.getZobristKey(), lastKey);
    // remember previous position
    lastKey = p.getZobristKey();
  }
}

void OpeningBook::writeToBook(const Move move, ZobristKey currentKey, const ZobristKey lastKey) {

#ifdef PARALLEL_LINE_PROCESSING
  // get the lock on the data map
  std::lock_guard<std::mutex> bookLock(bookMutex);
#endif

  // create or update book entry
  if (bookMap.contains(currentKey)) {
    // pointer to entry already in book
    bookMap[currentKey].counter++;
    // return as we do not need to update the predecessor
    return;
  }
  else {
    // new position
    bookMap.emplace(currentKey, currentKey);
  }
  // add move to the last book entry's move list
  BookEntry* lastEntry = &bookMap[lastKey];
  lastEntry->moves.push_back(move);
  lastEntry->nextPosition.push_back(currentKey);
} // lock released

/* Saves the bookMap data to a binary cache file for faster reading.
   Uses BOOST serialization to serialize the data to a binary file */
void OpeningBook::saveToCache() {
  { // save data to archive
    const auto start               = std::chrono::high_resolution_clock::now();
    const std::string serCacheFile = bookFilePath + cacheExt;
    LOG__DEBUG(Logger::get().BOOK_LOG, "Saving book to cache file {}", serCacheFile);
    // create and open a binary archive for output
    std::ofstream ofsBin(serCacheFile, std::fstream::binary | std::fstream::out);
    boost::archive::binary_oarchive oa(ofsBin);
    // write class instance to archive
    oa << BOOST_SERIALIZATION_NVP(bookMap);
    const auto stop    = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    (void) elapsed;
    LOG__DEBUG(Logger::get().BOOK_LOG, "Book saved to binary cache in ({:L} ms) ({})", elapsed.count(), serCacheFile);
  } // archive and stream closed when destructors are called
  _recreateCache = false;
}

/* Loads the bookMap data from a binary data cache file. This is considerably
   faster than reading the text based game files again */
bool OpeningBook::loadFromCache() {
  std::unordered_map<ZobristKey, BookEntry> binMap;
  try {
    // load data from archive
    const auto start               = high_resolution_clock::now();
    const std::string serCacheFile = bookFilePath + cacheExt;
    LOG__DEBUG(Logger::get().BOOK_LOG, "Loading from cache file {} ({:L} kB)", serCacheFile, std::filesystem::file_size(serCacheFile) / 1'024);
    // create and open a binary archive for input
    std::ifstream ifsBin(serCacheFile, std::fstream::binary | std::fstream::in);
    if (!ifsBin.is_open() || !ifsBin.good()) {
      LOG__ERROR(Logger::get().BOOK_LOG, "Loading from cache file {} failed", serCacheFile);
      return false;
    }
    boost::archive::binary_iarchive ia(ifsBin);
    // write archive to class instance
    ia >> BOOST_SERIALIZATION_NVP(binMap);
    const auto stop    = high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<milliseconds>(stop - start);
    LOG__INFO(Logger::get().BOOK_LOG,
              "Book loaded from cache with {:L} entries in ({:L} ms) ({})",
              binMap.size(), elapsed.count(), serCacheFile);
  } catch (const boost::archive::archive_exception& e) {
    // Cache file is incompatible (e.g., old version or corrupted)
    // Note: Since v0.7, platform-specific cache files are used (win/linux/macos)
    LOG__WARN(Logger::get().BOOK_LOG, "Cache file incompatible or corrupted: {} - will recreate", e.what());
    return false;
  } catch (const std::exception& e) {
    LOG__ERROR(Logger::get().BOOK_LOG, "Error loading cache file: {}", e.what());
    return false;
  }
  bookMap = std::move(binMap);
  return true;
} // archive and stream closed when destructors are called

/* checks if a cache file exists */
bool OpeningBook::hasCache() const {
  const std::string serCacheFile = bookFilePath + cacheExt;
  if (!std::filesystem::exists(serCacheFile)) {
    LOG__DEBUG(Logger::get().BOOK_LOG, "No cache file {} available", serCacheFile);
    return false;
  }
  const uint64_t fsize = std::filesystem::file_size(serCacheFile);
  (void) fsize;
  LOG__DEBUG(Logger::get().BOOK_LOG, "Cache file {} ({:L} kB) available", serCacheFile, fsize / 1'024);
  return true;
}
