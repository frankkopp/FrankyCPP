/*
 * MIT License
 *
 * Copyright (c) 2018 Frank Kopp
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

#include <cctype>
#include <fstream>
#include <memory>
#include <regex>
#include <string>
#include <xstring>
#include <random>

#include "types/types.h"
#include "openingbook2/OpeningBook2.h"

// BOOST Serialization
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

// enable for parallel processing of input lines
#define PARALLEL_LINE_PROCESSING
// enable for parallel processing of games from pgn files
#define PARALLEL_GAME_PROCESSING

// not all C++17 compilers have this std library for parallelization
//#undef HAS_EXECUTION_LIB
#ifdef HAS_EXECUTION_LIB
#include <execution>
#include <utility>
#endif

// //////////////////////////////////////////////
// /// PUBLIC

OpeningBook2::OpeningBook2(std::string bookPath, BookFormat bFormat)
    : bookFormat(bFormat), bookFilePath(std::move(bookPath)) {

  numberOfThreads = getNoOfThreads();

  // set root entry
  Position p{};
  const Key zobristKey = p.getZobristKey();
  const BookEntry2 rootEntry(zobristKey);
  bookMap.emplace(zobristKey, rootEntry);
}

Move OpeningBook2::getRandomMove(Key zobrist) const {
  Move bookMove = MOVE_NONE;
  // Find the entry for this key (zobrist key of position) in the map and
  // choose a random move from the list of moves in the entry
  if (bookMap.find(zobrist) != bookMap.end()) {
    BookEntry2 bookEntry = bookMap.at(zobrist);
    if (!bookEntry.moves.empty()) {
      std::random_device rd;
      std::uniform_int_distribution<std::size_t> random(0, bookEntry.moves.size() - 1);
      bookMove = bookEntry.moves[random(rd)];
    }
  }
  return bookMove;
}

void OpeningBook2::initialize() {
  if (isInitialized) return;
  LOG__INFO(Logger::get().BOOK_LOG, "Opening book initialization.");

  const auto start = std::chrono::high_resolution_clock::now();

  // if cache enabled check if we have a cache file and load from cache
  if (_useCache && !_recreateCache && hasCache()) {
    if (loadFromCache()) return;
  }

  // load the whole file into memory line by line
  auto lines = readFile(bookFilePath);

  // reads lines and retrieves game (lists of moves) and adds these to the book map
  readGames(lines);

  // release memory from initial file load
  data = nullptr;

  // safe the book to a cache
  if (_useCache && bookMap.size() > 1) {
    saveToCache();
  }

  const auto stop    = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

  isInitialized = true;
  LOG__INFO(Logger::get().BOOK_LOG, "Opening book initialized in ({:L} ms). {:L} positions", elapsed.count(), bookMap.size());
}

void OpeningBook2::reset() {
  const std::scoped_lock<std::mutex> lock(bookMutex);
  bookMap.clear();
  isInitialized = false;
  LOG__DEBUG(Logger::get().TEST_LOG, "Opening book reset: {:L}", bookMap.size());
}

std::string OpeningBook2::str(int level) const {
  return std::string();
}

// //////////////////////////////////////////////
// /// PRIVATE

std::vector<std::string_view> OpeningBook2::readFile(const std::string& filePath) {

  if (!fileExists(filePath)) {
    const std::string message = fmt::format("Opening Book '{}' not found", filePath);
    LOG__ERROR(Logger::get().BOOK_LOG, message);
    throw std::runtime_error(message);
  }

  std::vector<std::string_view> lines{};

  const auto start = std::chrono::high_resolution_clock::now();

  std::fstream file(filePath, std::ios::in | std::ios::binary);
  if (file.is_open()) {
    const uint64_t fileSize = getFileSize(filePath);
    LOG__DEBUG(Logger::get().BOOK_LOG, "Opened Opening Book '{}' with {:L} Byte successful.", filePath, fileSize);

    // fastest way to read all lines from a file into memory
    // https://stackoverflow.com/a/52699885/9161706
    file.seekg(0, std::ios::end);
    size_t data_size = file.tellg();
    data             = std::make_unique<char[]>(data_size);
    file.seekg(0, std::ios::beg);
    file.read(data.get(), data_size);
    lines.reserve(data_size / 40);
    for (size_t i = 0, dstart = 0; i < data_size; ++i) {
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
    throw std::runtime_error(message);
  }

  return lines;
}

void OpeningBook2::readGames(std::vector<std::string_view>& lines) {
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

void OpeningBook2::readGamesSimple(const std::vector<std::string_view>& lines) {
#ifdef PARALLEL_LINE_PROCESSING
  LOG__DEBUG(Logger::get().BOOK_LOG, "Using {} threads", getNoOfThreads());
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

void OpeningBook2::readOneGameSimple(const std::string_view& lineView) {
  MoveList game{};
  MoveGenerator mg;
  Position p;

  // trim line
  auto lineViewTrimmed = trimFast(lineView);

  // simple lines are in tuples of 4 per moveStr// read in 4 characters and check if they might
  // be moves (letter, digit, letter, digit)
  int index = 0;
  while (index < lineViewTrimmed.length()) {
    const auto moveStr = lineViewTrimmed.substr(index, 4);
    index += 4;
    if (!(isalpha(moveStr[0]) && isdigit(moveStr[1]) && isalpha(moveStr[2]) && isdigit(moveStr[3]))) continue;
    // create and validate the move against current position
    const Move move = mg.getMoveFromUci(p, std::string{moveStr});
    if (!move) {
      LOG__WARN(Logger::get().BOOK_LOG, "Not a valid move {} on this position {}", moveStr, p.strFen());
      return;
    }
    // add the validated move to the game and commit the move to the current position
    game.push_back(move);
    p.doMove(move);
  }

  // add game to book
  addGameToBook(game);
}

void OpeningBook2::readGamesSan(const std::vector<std::string_view>& lines) {
#ifdef PARALLEL_LINE_PROCESSING
  LOG__DEBUG(Logger::get().BOOK_LOG, "Using {} threads", getNoOfThreads());
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

void OpeningBook2::readOneGameSan(const std::string_view& lineView) {
  MoveList game{};

  // create a trimmed copy of the string as regex can't handle string_view :(
  const std::string line{trimFast(lineView)};

  // check if the line starts at least with a number or a character
  if (!isalnum(line[0])) return;

  MoveGenerator mg{};
  Position p{};// start position

  /*
  Iterate over all tokens, ignore move numbers and results
  Example:
  1. f4 d5 2. Nf3 Nf6 3. e3 g6 4. b3 Bg7 5. Bb2 O-O 6. Be2 c5 7. O-O Nc6 8. Ne5 Qc7 1/2-1/2
  1. f4 d5 2. Nf3 Nf6 3. e3 Bg4 4. Be2 e6 5. O-O Bd6 6. b3 O-O 7. Bb2 c5 1/2-1/2
  */

  std::vector<std::string> elements{};
  split(line, elements, ' ');

  for (const auto& e : elements) {
    if (e.empty() || e.length() == 1) continue;
    if (!isalpha(e[0])) continue;
    // create and validate the move
    Move move = mg.getMoveFromSan(p, e);
    if (!move) {
      LOG__WARN(Logger::get().BOOK_LOG, "Not a valid move {} on this position {} from line {}", e, p.strFen(), lineView);
      return;
    }
    // add the validated move to the game and commit the move to the current position
    game.push_back(move);
    p.doMove(move);
  }

  // add game to book
  addGameToBook(game);
}

void OpeningBook2::readGamesPgn(const std::vector<std::string_view>* lines) {

#ifdef PARALLEL_GAME_PROCESSING
  ThreadPool worker{getNoOfThreads()};
#else
  ThreadPool worker{1};
#endif

  std::vector<std::shared_ptr<std::future<bool>>> results{};

  // Get all lines belonging to one game and process this game asynchronously.
  // Iterate though all lines and look for pattern indicating for the next game
  // When a game start pattern is found ('^[')  we can assume the lines before
  // up to the line number marked in gameStart are part of one game.
  // This game (marked by line numbers for start and end will then be
  // send to be processed asynchronously
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
      auto future = std::make_shared<std::future<bool>>(worker.enqueue([=] {
        readOneGamePgn(lines, gameStart, gameEnd);
        return true;
      }));
      results.push_back(future);
      gameStart = gameEnd;
    }
    lastEmpty = false;
  }

  // Last game is not defined by the start pattern of the next game as the
  // there is none. We simply use the last line as end marker in this case.
  gameEnd = length;
  // process the previous found game asynchronously
  auto future = std::make_shared<std::future<bool>>(worker.enqueue([=] {
    readOneGamePgn(lines, gameStart, gameEnd);
    return true;
  }));
  results.push_back(future);

  // wait for completion of the asynchronous operations
  // future.get() is blocking if the async call has not returned
  int counter         = 0;
  const auto& iterEnd = results.end();
  for (auto iter = results.begin(); iter < iterEnd; iter++) {
    if (iter->get()->get()) counter++;
  }
}

void OpeningBook2::readOneGamePgn(const std::vector<std::string_view>* lines, size_t gameStart, size_t gameEnd) {

  std::string moveLine;

  // join all lines but skip empty line and %-comment lines and tag lines starting with [
  for (auto i = gameStart; i < gameEnd; i++) {
    const auto lineView = trimFast((*lines)[i]);
    if (lineView.empty() || lineView[0] == '[' || lineView[0] == '%') continue;
    moveLine.append(" ").append(removeTrailingComments(lineView));
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
  split(moveLine, movesStrings, ' ');

  MoveGenerator mg{};
  Position p{};// start position
  MoveList game{};

  // iterate though each move
  int moveNumber = 0;
  for (const auto& moveStr : movesStrings) {
    Move move = MOVE_NONE;
    moveNumber++;

    // check the notation format
    // Per PGN it must be SAN but some files have UCI notation
    // As UCI is pattern wise a subset of SAN we test for UCI first.
    if ((moveStr.size() == 4 && islower(moveStr[0]) && isdigit(moveStr[1]) && islower(moveStr[2]) && isdigit(moveStr[3])) ||
        (moveStr.size() == 5 && islower(moveStr[0]) && isdigit(moveStr[1]) && islower(moveStr[2]) && isdigit(moveStr[3]) && isupper(moveStr[4]))) {
      move = mg.getMoveFromUci(p, moveStr);
    }
    else {
      move = mg.getMoveFromSan(p, moveStr);
    }

    // validate the move
    if (move == MOVE_NONE) {
      LOG__WARN(Logger::get().BOOK_LOG, "Game at line {:L}:{}: Not a valid move {} on this position {}", gameStart, moveNumber, moveStr, p.strFen());
      fprintln("Moveline: {}\n", moveLine);
      break;
    }

    // add move to game and execute it on position
    game.emplace_back(move);
    p.doMove(move);
  }

  // add game to book
  addGameToBook(game);
}

std::string OpeningBook2::removeTrailingComments(const std::string_view& stringView) {
  if (stringView.find_first_of(';') != std::string_view::npos) {
    return std::string{stringView.data(), stringView.find_first_of(';')};
  }
  return std::string{stringView};
}

void OpeningBook2::cleanUpPgnMoveSection(std::string& str) {

  auto l    = str.length();
  char last = ' ';
  for (int a = 0; a < l;) {

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
      while (a < l && isdigit(str[a])) {
        str[a++] = ' ';
      }
    }
    // remove curly bracket comments '\{[^{}]*\}'
    else if (str[a] == '{') {
      while (a < l && str[a] != '}') {
        str[a++] = ' ';
      }
      str[a++] = ' ';
    }
    // remove curly bracket comments '\{[^{}]*\}'
    else if (str[a] == '<') {
      while (a < l && str[a] != '>') {
        str[a++] = ' ';
      }
      str[a++] = ' ';
    }
    // remove bracket comments '\([^()]*\)'  - maybe recursive
    else if (str[a] == '(') {
      int open = 1;
      str[a++] = ' ';
      while (a < l && open > 0) {
        if (str[a] == ')')
          open--;
        else if (str[a] == '(')
          open++;
        str[a++] = ' ';
      }
    }
    // remove move numbering
    else if (isdigit(str[a]) && last == ' ') {
      str[a++] = ' ';
      while (a < l && (isdigit(str[a]) || str[a] == '.')) {
        str[a++] = ' ';
      }
    }
    // valid - move forward
    else {
      a++;
    }
    last = str[a - 1];
  }

  // remove result (1-0 0-1 1/2-1/2 *)
  auto a = l - 1;
  for (; a >= 0; a--) {
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
  }
  str.resize(a);

  // another run to remove all double spaces
  l              = str.length();
  int index      = 0;
  int copyTo     = 0;
  int spaceFound = 0;
  while (index < l) {
    if (str[index] == ' ') {
      spaceFound++;
      if (spaceFound <= 1) {
        str[copyTo++] = str[index++];
      }
      else {
        index++;
      }
    }
    else {
      str[copyTo++] = str[index++];
      spaceFound    = 0;
    }
  }
  // remove left over characters at the end
  str.erase(copyTo, index);
  // remove trailing and leading whitespace
  str = trimFast(str);
}

int OpeningBook2::addGameToBook(const MoveList& game) {
  Position currentPosition{};
  MoveGenerator mg{};
  int counter = 0;

  for (Move move : game) {
    counter++;
    // remember previous position
    const Key lastKey = currentPosition.getZobristKey();
    // make move on position to get new position
    currentPosition.doMove(move);
    const Key currentKey = currentPosition.getZobristKey();

    // get the lock on the data map
    const std::scoped_lock<std::mutex> lock(bookMutex);

    // create or update book entry
    if (bookMap.count(currentKey)) {
      // pointer to entry already in book
      BookEntry2* existingEntry = &bookMap.at(currentKey);
      existingEntry->counter++;
      LOG__TRACE(Logger::get().BOOK_LOG, "Position already existed {} times: {}", existingEntry->counter, existingEntry->fen);
    }
    else {
      // new position
      bookMap.emplace(currentKey, BookEntry2(currentKey));
      LOG__TRACE(Logger::get().BOOK_LOG, "Position new", currentKey);
    }

    // add move to the last book entry's move list
    BookEntry2* lastEntry = &bookMap.at(lastKey);
    if (std::find(lastEntry->moves.begin(), lastEntry->moves.end(), move) == lastEntry->moves.end()) {
      lastEntry->moves.push_back(move);
      lastEntry->ptrNextPosition.emplace_back(std::make_shared<BookEntry2>(bookMap.at(currentKey)));
      LOG__TRACE(Logger::get().BOOK_LOG, "Added move and pointer to last entry.");
    }
  }

  return counter;
}// lock released

/* Saves the bookMap data to a binary cache file for faster reading.
   Uses BOOST serialization to serialize the data to a binary file */
void OpeningBook2::saveToCache() {
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
  _recreateCache = false;
}

/* Loads the bookMap data from a binary data cache file. This is considerably
   faster than reading the text based game files again */
bool OpeningBook2::loadFromCache() {
  const std::scoped_lock<std::mutex> lock(bookMutex);
  std::unordered_map<Key, BookEntry2> binMap;
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
  bookMap = std::move(binMap);
  return true;
} // archive and stream closed when destructors are called

/* checks if a cache file exists */
bool OpeningBook2::hasCache() const {
  const std::basic_string<char> serCacheFile = bookFilePath + cacheExt;
  if (!fileExists(serCacheFile)) {
    LOG__DEBUG(Logger::get().BOOK_LOG, "No cache file {} available", serCacheFile);
    return false;
  }
  uint64_t fsize = getFileSize(serCacheFile);
  LOG__DEBUG(Logger::get().BOOK_LOG, "Cache file {} ({:L} kB) available", serCacheFile, fsize / 1'024);
  return true;
}