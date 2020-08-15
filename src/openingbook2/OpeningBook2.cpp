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

#include "openingbook2/BookEntry2.h"
#include "openingbook2/OpeningBook2.h"

// enable for parallel processing of input lines
#define PARALLEL_LINE_PROCESSING
// enable for parallel processing of games from pgn files
//#define PARALLEL_GAME_PROCESSING
// enable for using fifo when reading pgn files and processing pgn games
// this allows to start processing games in parallel while still reading
//#define FIFO_PROCESSING

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

void OpeningBook2::initialize() {
  if (isInitialized) return;
  LOG__INFO(Logger::get().BOOK_LOG, "Opening book initialization.");

  const auto start = std::chrono::high_resolution_clock::now();

  // TODO read book and then delete raw data

  const auto stop    = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

  isInitialized = true;
  LOG__INFO(Logger::get().BOOK_LOG, "Opening book initialized in ({:L} ms). {:L} positions", elapsed.count(), bookMap.size());
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

    // fastest way to read all lines from a file
    // https://stackoverflow.com/a/52699885/9161706
    file.seekg(0, std::ios::end);
    size_t data_size = file.tellg();
    data      = std::make_unique<char[]>(data_size);
    file.seekg(0, std::ios::beg);
    file.read(data.get(), data_size);
    lines.reserve(data_size / 40);
    for (size_t i = 0, dstart = 0; i < data_size; ++i) {
      if (data[i] == '\n' || i == data_size - 1) {// End of line, got string
        lines.emplace_back(data.get() + dstart, i - dstart);
        dstart = i + 1;
      }
    }

    /*
    // normal c++ way - quite slow
    lines.reserve(fileSize / 40);
    std::string line;
    while (std::getline(file, line)) {
      if (!line.empty()) lines.push_back(line);
    }
    */

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

std::vector<MoveList> OpeningBook2::readGames(std::vector<std::string_view>& lines) {
  LOG__DEBUG(Logger::get().BOOK_LOG, "Reading games...");

  const auto start = std::chrono::high_resolution_clock::now();

  std::vector<MoveList> games{};

  // process all lines from the opening book file depending on format
  switch (bookFormat) {
    case BookFormat::SIMPLE:
      readGamesSimple(lines, games);
      break;
    case BookFormat::SAN: {
      readGamesSan(lines, games);
      break;
    }
    case BookFormat::PGN:
      readGamesPgn(&lines, &games);
      break;
  }

  const auto stop    = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
  LOG__DEBUG(Logger::get().BOOK_LOG, "Found {:L} games in {:L} ms.", games.size(), elapsed.count());
  return games;
}

void OpeningBook2::readGamesSimple(const std::vector<std::string_view>& lines, std::vector<MoveList>& games) {
#ifdef PARALLEL_LINE_PROCESSING
  LOG__DEBUG(Logger::get().BOOK_LOG, "Using {} threads", getNoOfThreads());
#ifdef HAS_EXECUTION_LIB// use parallel lambda
  std::for_each(std::execution::par_unseq, lines.begin(), lines.end(),
                [&](auto&& line) {
                  const MoveList game = readOneGameSimple(line);
                  if (game.empty()) return;
                  const std::scoped_lock<std::mutex> lock(gamesMutex);
                  games.emplace_back(game);
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
        const MoveList game = readOneGameSimple(lines[i]);
        if (game.empty()) continue;
        const std::scoped_lock<std::mutex> lock(gamesMutex);
        games.emplace_back(game);
      }
    });
  }
  for (std::thread& th : threads) th.join();
#endif
#else// no parallel execution
  for (auto line : lines) {
    MoveList game = readOneGameSimple(line);
    if (game.empty()) continue;
    games.emplace_back(game);
  }
#endif
}

MoveList OpeningBook2::readOneGameSimple(const std::string_view& lineView) {
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
      return game;
    }
    // add the validated move to the game and commit the move to the current position
    game.push_back(move);
    p.doMove(move);
  }
  return game;
}

void OpeningBook2::readGamesSan(const std::vector<std::string_view>& lines, std::vector<MoveList>& games) {
#ifdef PARALLEL_LINE_PROCESSING
  LOG__DEBUG(Logger::get().BOOK_LOG, "Using {} threads", getNoOfThreads());
#ifdef HAS_EXECUTION_LIB// use parallel lambda
  std::for_each(std::execution::par_unseq, lines.begin(), lines.end(),
                [&](auto&& line) {
                  const MoveList game = readOneGameSan(line);
                  if (game.empty()) return;
                  const std::scoped_lock<std::mutex> lock(gamesMutex);
                  games.emplace_back(game);
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
        const MoveList game = readOneGameSan(lines[i]);
        if (game.empty()) continue;
        const std::scoped_lock<std::mutex> lock(gamesMutex);
        games.emplace_back(game);
      }
    });
  }
  for (std::thread& th : threads) th.join();
#endif
#else// no parallel execution
  for (auto line : lines) {
    MoveList game = readOneGameSan(line);
    if (game.empty()) continue;
    games.emplace_back(game);
  }
#endif
}

MoveList OpeningBook2::readOneGameSan(const std::string_view& lineView) {
  MoveList game{};

  // create a trimmed copy of the string as regex can't handle string_view :(
  const std::string line{trimFast(lineView)};

  // check if the line starts at least with a number or a character
  if (!isalnum(line[0])) return game;

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
      LOG__WARN(Logger::get().BOOK_LOG, "Not a valid move {} on this position {}", e, p.strFen());
      return game;
    }
    // add the validated move to the game and commit the move to the current position
    game.push_back(move);
    p.doMove(move);
  }
  return game;
}

void OpeningBook2::readGamesPgn(const std::vector<std::string_view>* lines, std::vector<MoveList>* games) {

  ThreadPool worker{getNoOfThreads()};
  std::vector<std::shared_ptr<std::future<bool>>> results{};

  // Get all lines belonging to one game and process this game asynchronously.
  // Iterate though all lines and look for pattern indicating for the next game
  // When a game start pattern is found ('^[')  we can assume the lines before
  // up to the line number marked in gameStart are part of one game.
  // This game (marked by line numbers for start and end will then be
  // send to be processed asynchronously
  size_t gameStart  = 0;
  size_t gameEnd    = 0;
  bool lastEmpty = true;
  const auto length = lines->size();
  for (int lineNumber = 0; lineNumber < length; lineNumber++) {
    const auto trimmedLineView = trimFast((*lines)[lineNumber]);
    // skip emtpy lines
    if (trimmedLineView.empty()) {
      lastEmpty = true;
      continue;
    }
    // a new game (except the first) always starts with a newline and a tag-section ([tag-pair])
    if ((lastEmpty && trimmedLineView[0] == '[') || lineNumber == length - 1 ){
      gameEnd = lineNumber;
      // process the previous found game asynchronously
      auto future = std::make_shared<std::future<bool>>(worker.enqueue([=] {
        return readOneGamePgn(lines, gameStart, gameEnd, games);
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
    return readOneGamePgn(lines, gameStart, gameEnd, games);
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

bool OpeningBook2::readOneGamePgn(const std::vector<std::string_view>* lines, size_t gameStart, size_t gameEnd, std::vector<MoveList>* games) {// process previous game

  std::string moveLine;

  // join all lines but skip empty line and %-comment lines and tag line starting with [
  for (auto i = gameStart; i < gameEnd; i++) {
    const auto lineView = trimFast((*lines)[i]);
    if (lineView.empty() || lineView[0] == '[' || lineView[0] == '%') continue;
    moveLine.append(" ").append(removeTrailingComments(lineView));
  }

  // after cleanup skip games with no moves
  if (moveLine.empty()) return true;

  // cleanup unwanted parts of move section
  //fprintln("\nBefore: '{}'", moveLine);
  cleanUpPgnMoveSection(moveLine);
  //fprintln("After : '{}'\n", moveLine);

  // after cleanup skip games with no moves
  if (moveLine.empty()) return true;

  // find and check move from the clean line of moves
  // get each move string from the clean line
  std::vector<std::string> movesStrings{};
  split(moveLine, movesStrings, ' ');

  MoveGenerator mg{};
  Position p{}; // start position
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
      //      fprintln("Game move {} is UCI", moveStr);
      move = mg.getMoveFromUci(p, moveStr);
    }
    else {
//      fprintln("Game move {} is SAN", moveStr);
      move = mg.getMoveFromSan(p, moveStr);
    }

    // validate the move
    if (move == MOVE_NONE) {
      LOG__WARN(Logger::get().BOOK_LOG, "Game at line {:L}:{}: Not a valid move {} on this position {}", gameStart, moveNumber, moveStr, p.strFen());
      break;
    }
//    fprintln("Move found {}", str(move));

    game.emplace_back(move);
    p.doMove(move);
  }

  {// scoped lock
    const std::scoped_lock<std::mutex> lock(gamesMutex);
    games->emplace_back(game);
  }

  return true;
}

std::string OpeningBook2::removeTrailingComments(const std::string_view& stringView) {
  if (stringView.find_first_of(';') != std::string_view::npos) {
    return std::string{stringView.data(), stringView.find_first_of(';')};
  }
  return std::string{stringView};
}

void OpeningBook2::cleanUpPgnMoveSection(std::string& str) {
//  fprintln("'{}'", str);

  auto l     = str.length();
  char last = ' ';
  for (int a = 0; a < l;) {

//    fprintln(" {:>{}}", "^", a);

    // skip non ascii characters
    if (int(str[a]) < 0 || int(str[a]) > 255) {
      str[a++] = ' ';
    }
    // skip invalid characters
    else if (!(isalnum(str[a]) || str[a] == '$' || str[a] == '*' || str[a] == '('
               || str[a] == '{' || str[a] == '<' || str[a] == '/' || str[a] == '-')) {
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
//    fprintln("'{}'", str);
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

//  fprintln("'{}'", str);
  
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

//  fprintln("'{}'", str);
}
