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

#include <fstream>
#include <regex>

#include "openingbook2/BookEntry2.h"
#include "openingbook2/OpeningBook2.h"

// enable for parallel processing of input lines
//#define PARALLEL_LINE_PROCESSING
// enable for parallel processing of games from pgn files
//#define PARALLEL_GAME_PROCESSING
// enable for using fifo when reading pgn files and processing pgn games
// this allows to start processing games in parallel while still reading
//#define FIFO_PROCESSING

// not all C++17 compilers have this std library for parallelization
#undef HAS_EXECUTION_LIB
#ifdef HAS_EXECUTION_LIB
#include <execution>
#include <utility>
#endif

// //////////////////////////////////////////////
// /// PUBLIC

OpeningBook2::OpeningBook2(std::string bookPath, BookFormat bFormat)
    : bookFormat(bFormat), bookFilePath(std::move(bookPath)) {

  // std::thread::hardware_concurrency() is not reliable - on some platforms
  // it returns 0 - in this case we chose a default of 4
  numberOfThreads = std::thread::hardware_concurrency() == 0 ? 4 : std::thread::hardware_concurrency();

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
    data             = std::make_unique<char[]>(data_size);
    file.seekg(0, std::ios::beg);
    file.read(data.get(), data_size);
    lines.reserve(data_size / 40);
    for (size_t i = 0, dstart = 0; i < data_size; ++i) {
      if ((data[i] == '\n' || i == data_size - 1) && i - dstart > 1) {// End of line, got string
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
      readGamesPgn(lines, games);
      break;
  }

  const auto stop    = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
  LOG__DEBUG(Logger::get().BOOK_LOG, "Found {:L} games in {:L} ms.", games.size(), elapsed.count());
  return games;
}

void OpeningBook2::readGamesSimple(const std::vector<std::string_view>& lines, std::vector<MoveList>& games) {
#ifdef PARALLEL_LINE_PROCESSING
  LOG__DEBUG(Logger::get().BOOK_LOG, "Using {} threads", numberOfThreads);
  std::mutex gamesMutex;
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
  threads.reserve(numberOfThreads);
  for (unsigned int t = 0; t < numberOfThreads; ++t) {
    threads.emplace_back([&, this, t]() {
      auto range     = noOfLines / numberOfThreads;
      auto startIter = t * range;
      auto end       = startIter + range;
      if (t == numberOfThreads - 1) end = noOfLines;
      for (std::size_t i = startIter; i < end; ++i) {
        const MoveList game = readOneGameSimple(lines[i]);
        if (game.empty()) return;
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
  std::smatch matcher;
  MoveList game{};

  // create a trimmed copy of the string as regex can't handle string_view :(
  static const std::regex trimWhiteSpace(R"(^\s+|\s+$)");
  const std::string line = std::regex_replace(std::string{lineView}, trimWhiteSpace, "");

  // check if line starts with move
  static const std::regex startPattern(R"(^[a-h][1-8][a-h][1-8].*$)");
  if (!std::regex_match(line, matcher, startPattern)) {
    LOG__TRACE(Logger::get().BOOK_LOG, "Line ignored: {}", lineView);
    return game;
  }

  // iterate over all found pattern matches (aka moves)
  const std::regex movePattern(R"([a-h][1-8][a-h][1-8])");
  auto move_begin = std::sregex_iterator(line.begin(), line.end(), movePattern);
  auto move_end   = std::sregex_iterator();
  LOG__TRACE(Logger::get().BOOK_LOG, "Found {} moves in line: {}", std::distance(move_begin, move_end), lineView);
  MoveGenerator mg;
  Position p;
  for (auto i = move_begin; i != move_end; ++i) {
    const std::string& moveStr = (*i).str();
    LOG__TRACE(Logger::get().BOOK_LOG, "Moves {}", moveStr);
    // create and validate the move
    const Move move = mg.getMoveFromUci(p, moveStr);
    if (!validMove(move)) {
      LOG__WARN(Logger::get().BOOK_LOG, "Not a valid move {} on this position {}", moveStr, p.strFen());
      return game;
    }
    // add the validated move to the game
    game.push_back(move);
    // do the move and loop
    p.doMove(move);
  }

  return game;
}

void OpeningBook2::readGamesSan(const std::vector<std::string_view>& lines, std::vector<MoveList>& games) {
}
void OpeningBook2::readGamesPgn(const std::vector<std::string_view>& lines, std::vector<MoveList>& games) {
}
