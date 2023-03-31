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

#include "openingbook/OpeningBook.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"
#include "common/Logging.h"
#include "common/ThreadPool.h"
#include "common/misc.h"
#include "common/stringutil.h"
#include "types/types.h"

// BOOST Serialization
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <cctype>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <future>
#include <algorithm>

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

OpeningBook::OpeningBook(std::string bookPath, BookFormat bFormat)
    : bookFormat(bFormat), bookFilePath(std::move(bookPath)) {
  numberOfThreads = getNoOfThreads();
}

Move OpeningBook::getRandomMove(Key zobrist) const {
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

void OpeningBook::initialize() {
  if (isInitialized) {
    LOG__WARN(Logger::get().BOOK_LOG, "Opening book already initialized. Call to initialize ignored.");
    return;
  }
  LOG__INFO(Logger::get().BOOK_LOG, "Opening book initialization.");

  const auto start = std::chrono::high_resolution_clock::now();

  // if cache enabled check if we have a cache file and load from cache
  if (_useCache && !_recreateCache && hasCache()) {
    if (loadFromCache()) return;
  }

  // load the whole file into memory line by line
  const auto lines = readFile(bookFilePath);

  // set root entry
  auto iteratorPair = bookMap.emplace(rootZobristKey, rootZobristKey);
  iteratorPair.first->second.counter = 0;

  // reads lines and retrieves game (lists of moves) and adds these to the book map
  readGames(lines);

  // release memory from initial file load
  data.reset(nullptr);

  // safe the book to a cache
  if (_useCache && bookMap.size() > 1) {
    saveToCache();
  }

  const auto stop    = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

  isInitialized = true;
  LOG__INFO(Logger::get().BOOK_LOG, "Opening book initialized in ({:L} ms). {:L} positions", elapsed.count(), bookMap.size());
}

void OpeningBook::reset() {
  bookMap.clear();
  data.reset(nullptr);
  isInitialized = false;
  LOG__DEBUG(Logger::get().TEST_LOG, "Opening book reset: {:L} entries", bookMap.size());
}

std::string OpeningBook::str(int level) {
  Position p{};
  Key zobristKey        = p.getZobristKey();
  const BookEntry* node = &bookMap[zobristKey];
  return std::string{fmt::format(deLocale, "Root ({:L})\n{:s}", bookMap[zobristKey].counter, getLevelStr(1, level, node))};
}

std::string OpeningBook::getLevelStr(int level, int maxLevel, const BookEntry* node) {
  std::string out;
  const size_t size = node->moves.size();
  for (int i = 0; i < size; i++) {
    const BookEntry* newNode = &bookMap[(node->nextPosition)[i]];
    out += fmt::format(deLocale, "{:{}}{} ({:L})\n", "", level, node->moves[i], newNode->counter);
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
    const std::string message = fmt::format("Opening Book '{}' not found. Using empty book.", filePath);
    LOG__ERROR(Logger::get().BOOK_LOG, message);
    return lines;
  }

  const auto start = std::chrono::high_resolution_clock::now();

  std::fstream file(filePath, std::ios::in | std::ios::binary);
  if (file.is_open()) {
    const uint64_t fileSize = std::filesystem::file_size(filePath);
    LOG__DEBUG(Logger::get().BOOK_LOG, "Opened Opening Book '{}' with {:L} Byte successful.", filePath, fileSize);

    // fast way to read all lines from a file into memory
    // https://stackoverflow.com/a/52699885/9161706
    file.seekg(0, std::ios::beg);
    file.seekg(0, std::ios::end);
    std::streamsize data_size = file.tellg();
    file.seekg(0, std::ios::beg);
    data = std::make_unique<char[]>(data_size);
    file.read(data.get(), data_size);
    lines.reserve(data_size / 20);
    for (std::streamsize i = 0, dstart = 0; i < data_size; ++i) {
      if (data[i] == '\n' || i == data_size - 1) {// End of line, got string
        lines.emplace_back(data.get() + dstart, i - dstart);
        dstart = i + 1;
      }
    }

    const auto stop    = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    LOG__DEBUG(Logger::get().BOOK_LOG, "Read {:L} lines in {:L} ms.", lines.size(), elapsed.count());

    file.close();
  }
  else {
    const std::string message = fmt::format("Could not open Opening Book '{}' ", filePath);
    LOG__ERROR(Logger::get().BOOK_LOG, message);
    return lines;
  }

  return lines;
}

void OpeningBook::readGames(const std::vector<std::string_view>& lines) {
  LOG__DEBUG(Logger::get().BOOK_LOG, "Reading games...");

  const auto start = std::chrono::high_resolution_clock::now();

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
      readGamesPgn(&lines);
      break;
  }

  const auto stop    = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
  LOG__DEBUG(Logger::get().BOOK_LOG, "Read games in {:s}.", ::str(elapsed));
}

void OpeningBook::readGamesSimple(const std::vector<std::string_view>& lines) {
  const unsigned int noOfThreads = getNoOfThreads();

#ifdef PARALLEL_LINE_PROCESSING
  LOG__DEBUG(Logger::get().BOOK_LOG, "Using {} threads", noOfThreads);

#ifdef HAS_EXECUTION_LIB// use parallel lambda
  std::for_each(std::execution::par_unseq, lines.begin(), lines.end(),
                [&](auto&& line) {
                  readOneGameSimple(line);
                });
#else// no <execution> library (< C++17)
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

#else// no parallel execution
  for (auto line : lines) {
    readOneGameSimple(line);
  }
#endif
}

void OpeningBook::readOneGameSimple(const std::string_view& lineView) {
  Moves game{};

  // trim line
  auto lineViewTrimmed = trimFast(lineView);

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
  const unsigned int noOfThreads = getNoOfThreads();

#ifdef PARALLEL_LINE_PROCESSING
  LOG__DEBUG(Logger::get().BOOK_LOG, "Using {} threads", noOfThreads);

#ifdef HAS_EXECUTION_LIB// use parallel lambda
  std::for_each(std::execution::par_unseq, lines.begin(), lines.end(),
                [&](auto&& line) {
                  readOneGameSan(line);
                });
#else// no <execution> library (< C++17)
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

#else// no parallel execution
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

  MoveGenerator mg{};
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

void OpeningBook::readGamesPgn(const std::vector<std::string_view>* lines) {
  std::vector<std::future<bool>> futures;

#ifdef PARALLEL_LINE_PROCESSING
  const std::launch asyncPolicy = std::launch::async;
#else
  const std::launch asyncPolicy = std::launch::deferred;
#endif

  // Get all lines belonging to one game and process this game asynchronously.
  // Iterate though all lines and look for pattern indicating for the next game
  // When a game start pattern is found ('^[')  we can assume the lines before
  // up to the line number marked in gameStart are part of one game.
  // This game (marked by line numbers for start and end will then be
  // sent to be processed asynchronously
  size_t gameStart = 0;
  size_t gameEnd;
  bool lastEmpty    = true;
  const auto length = lines->size();
  for (int lineNumber = 0; lineNumber < length; lineNumber++) {
    const auto trimmedLineView = trimFast((*lines)[lineNumber]);
    // skip emtpy lines
    if (trimmedLineView.empty()) {
      lastEmpty = true;
      continue;
    }
    // a new game (except in the first line) always starts with a newline and a tag-section ([tag-pair])
    if ((lastEmpty && trimmedLineView[0] == '[') || lineNumber == length - 1) {
      gameEnd = lineNumber;
      // process the previous found game asynchronously
      futures.push_back(std::async(asyncPolicy, [=, this] {
        readOneGamePgn(lines, gameStart, gameEnd);
        return true;
      }));
      gameStart = gameEnd;
    }
    lastEmpty = false;
  }

  // Last game is not defined by the start pattern of the next game as the
  // there is none. We simply use the last line as end marker in this case.
  gameEnd = length;
  // process the previous found game asynchronously
  futures.push_back(std::async(asyncPolicy, [=, this] {
    readOneGamePgn(lines, gameStart, gameEnd);
    return true;
  }));

  // wait for completion of the asynchronous operations
  // future.get() is blocking if the async call has not returned
  for (auto& future : futures) {
    future.get();
  }
}

void OpeningBook::readOneGamePgn(const std::vector<std::string_view>* lines, size_t gameStart, size_t gameEnd) {

  std::string moveLine;

  // join all lines but skip empty line and %-comment lines and tag lines starting with [
  for (auto i = gameStart; i < gameEnd; i++) {
    const auto lineView = trimFast((*lines)[i]);
    if (lineView.empty() || lineView[0] == '[' || lineView[0] == '%') continue;
    moveLine.append(" ").append(removeTrailingComments(lineView, ";"));
  }

  // after cleanup skip games with no moves
  if (moveLine.empty()) return;

  // cleanup unwanted parts of move section
  cleanUpPgnMoveSection(moveLine);

  // after cleanup skip games with no moves
  if (moveLine.empty()) return;

  // find and check move from the clean line of moves
  // get each move string from the clean line
  std::vector<std::string> movesStrings{};
  splitFast(moveLine, movesStrings, " ");

  // add game to book
  addGameToBook(movesStrings);
}

void OpeningBook::cleanUpPgnMoveSection(std::string& str) {
  std::size_t length = str.length();// explicit as the later loop test for <0
  if (length == 0) return;

  char lastChar = ' ';
  for (int a = 0; a < length;) {
    // skip non ascii characters
    if (int(str[a]) < 0 || int(str[a]) > 255) {
      str[a++] = ' ';
    }
    // skip invalid characters
    else if (!(isalnum(str[a]) || str[a] == '$' || str[a] == '*' || str[a] == '(' || str[a] == '{' || str[a] == '<' || str[a] == '/' || str[a] == '-' || str[a] == '=')) {
      str[a++] = ' ';
    }
    // nag annotation \$\d{1,3}
    else if (str[a] == '$') {
      str[a++] = ' ';
      while (a < length && isdigit(str[a])) {
        str[a++] = ' ';
      }
    }
    // remove curly bracket comments '\{[^{}]*\}'
    else if (str[a] == '{') {
      while (a < length && str[a] != '}') {
        str[a++] = ' ';
      }
      str[a++] = ' ';
    }
    // remove tag bracket comments '\{[^<>}*\}'
    else if (str[a] == '<') {
      while (a < length && str[a] != '>') {
        str[a++] = ' ';
      }
      str[a++] = ' ';
    }
    // remove bracket comments '\([^()]*\)'  - maybe recursive
    else if (str[a] == '(') {
      int open = 1;
      str[a++] = ' ';
      while (a < length && open > 0) {
        if (str[a] == ')')
          open--;
        else if (str[a] == '(')
          open++;
        str[a++] = ' ';
      }
    }
    // remove move numbering
    else if (isdigit(str[a]) && lastChar == ' ') {
      str[a++] = ' ';
      while (a < length && (isdigit(str[a]) || str[a] == '.')) {
        str[a++] = ' ';
      }
    }
    // valid - move forward
    else {
      a++;
    }
    lastChar = str[a - 1];
  }

  // remove result (1-0 0-1 1/2-1/2 *)
  std::size_t a = length - 1;
  do {
    if (str[a] == ' ') {
      continue;
    }
    if (str[a] == '*') {
      str[a] = ' ';
      break;
    }
    if (a >= 6 && str.substr(a - 6, 7) == " /2-1/2") {
      a -= 6;
      break;
    }
    if (a >= 2 && (str.substr(a - 2, 3) == " -0" || str.substr(a - 2, 3) == " -1")) {
      a -= 2;
      break;
    }
  } while (a-- > 0);

  str.resize(a);

  // Use the std::unique algorithm to remove consecutive spaces
  auto newEnd = std::unique(str.begin(), str.end(),
                            [](char a, char b) { return a == ' ' && b == ' '; });
  // Erase the extra spaces from the string
  str.resize(std::distance(str.begin(), newEnd));
  // remove trailing and leading whitespace
  str = trimFast(str);
}

void OpeningBook::addGameToBook(const Moves& game) {
  if (game.empty()) {
    return;
  }

  Position p{};
  MoveGenerator mg{};

  // initialize lastKey with start position (aka root position)
  Key lastKey = rootZobristKey;
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

void OpeningBook::writeToBook(Move move, Key currentKey, Key lastKey) {

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
}// lock released

/* Saves the bookMap data to a binary cache file for faster reading.
   Uses BOOST serialization to serialize the data to a binary file */
void OpeningBook::saveToCache() {
  {// save data to archive
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
    LOG__DEBUG(Logger::get().BOOK_LOG, "Book saved to binary cache in ({:L} ms) ({})", elapsed.count(), serCacheFile);
  }// archive and stream closed when destructors are called
  _recreateCache = false;
}

/* Loads the bookMap data from a binary data cache file. This is considerably
   faster than reading the text based game files again */
bool OpeningBook::loadFromCache() {
  std::unordered_map<Key, BookEntry> binMap;
  {// load data from archive
    const auto start               = std::chrono::high_resolution_clock::now();
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
    const auto stop    = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    LOG__INFO(Logger::get().BOOK_LOG,
              "Book loaded from cache with {:L} entries in ({:L} ms) ({})",
              binMap.size(), elapsed.count(), serCacheFile);
  }
  bookMap = std::move(binMap);
  return true;
}// archive and stream closed when destructors are called

/* checks if a cache file exists */
bool OpeningBook::hasCache() const {
  const std::basic_string<char> serCacheFile = bookFilePath + cacheExt;
  if (!std::filesystem::exists(serCacheFile)) {
    LOG__DEBUG(Logger::get().BOOK_LOG, "No cache file {} available", serCacheFile);
    return false;
  }
  uint64_t fsize = std::filesystem::file_size(serCacheFile);
  LOG__DEBUG(Logger::get().BOOK_LOG, "Cache file {} ({:L} kB) available", serCacheFile, fsize / 1'024);
  return true;
}
