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

#include <iostream>
#include <string>

#include "common/misc.h"
#include "common/Fifo.h"
#include "PGN_Reader.h"

// BOOST regex
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/regex.hpp>
//using namespace boost;
using namespace std::string_literals;

static const boost::regex trailingComments(R"(;.*$)");
static const boost::regex tagPairs(R"(\[\w+ +".*?"\])");
static const boost::regex doubleWhiteSpace(R"(\s+)");
static const boost::regex moveSectionStart(R"(^(\d+.)|([KQRBN]?[a-h][1-8]))");
static const boost::regex moveSectionEnd(R"(.*((1-0)|(0-1)|(1/2-1/2)|\*)$)");
static const boost::regex nagAnnotation(R"((\$\d{1,3}))"); // no NAG annotation supported
static const boost::regex bracketComments(R"(\{[^{}]*\})"); // bracket comments
static const boost::regex reservedSymbols(R"(<[^<>]*>)"); // reserved symbols < >
static const boost::regex ravVariants(R"(\([^()]*\))"); // RAV variant comments < >
static const boost::regex resultsRgx(R"(((1-0)|(0-1)|(1/2-1/2)|\*))");
static const boost::regex moveNumbers(R"(\d{1,3}( )*(\.{1,3}))");
static const boost::regex moveRgx(R"(([NBRQK])?([a-h])?([1-8])?x?([a-h][1-8]|O-O-O|O-O)(=([NBRQ]))?([!?+#]*)?)");

// ///////////////////////
// PUBLIC

PGN_Reader::PGN_Reader(std::vector<std::string> &lines) {
  inputLines = std::make_shared<std::vector<std::string>>(lines);
}

bool PGN_Reader::process(Fifo<PGN_Game> &gamesFifo) {
  LOG__TRACE(Logger::get().BOOK_LOG, "Finding games in {:n} lines.", inputLines->size());
  const auto start = std::chrono::high_resolution_clock::now();
  // loop over all input lines
  VectorIterator linesIter = inputLines->begin();
  while (linesIter < inputLines->end()) {
    LOG__TRACE(Logger::get().BOOK_LOG, "Finding game {:n}", games.size() + 1);
    PGN_Game game = processOneGame(linesIter);
    games.push_back(game);
    gamesFifo.push(game);
    LOG__TRACE(Logger::get().BOOK_LOG, "Game Fifo has {:n} games", gamesFifo.size());
  }
  const auto stop = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
  LOG__INFO(Logger::get().BOOK_LOG, "Found {:n} games in {:n} ms", games.size(), elapsed.count());
  return true;
}

bool PGN_Reader::process() {
  LOG__TRACE(Logger::get().BOOK_LOG, "Processing {:n} lines.", inputLines->size());
  const auto start = std::chrono::high_resolution_clock::now();
  // loop over all input lines
  VectorIterator linesIter = inputLines->begin();
  while (linesIter < inputLines->end()) {
    LOG__TRACE(Logger::get().BOOK_LOG, "Processing game {:n}", games.size() + 1);
    games.push_back(processOneGame(linesIter));
  }
  const auto stop = std::chrono::high_resolution_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
  LOG__INFO(Logger::get().BOOK_LOG, "Found {:n} games in {:n} ms", games.size(), elapsed.count());
  return true;
}

// ///////////////////////
// PRIVATE

inline PGN_Game PGN_Reader::processOneGame(VectorIterator &iterator) {
  bool gameEndReached = false;
  PGN_Game game{};
  do {
    LOG__TRACE(Logger::get().BOOK_LOG, "Process line: {}    (length={})", *iterator, iterator->size());
    // clean up line
    boost::trim(*iterator);
    // ignore comment lines
    if (boost::starts_with(*iterator, "%")) continue;
    // ignore meta data tags for now
    replace_all_regex(*iterator, tagPairs, " "s);
    // trailing comments
    erase_regex(*iterator, trailingComments);
    // eliminate double whitespace
    replace_all_regex(*iterator, doubleWhiteSpace, " "s);
    // clean up line
    boost::trim(*iterator);
    // process move section
    if (find_regex(*iterator, moveSectionStart)) {
      handleMoveSection(iterator, game);
      gameEndReached = true;
      if (iterator >= inputLines->end()) break;
    }
  } while (++iterator < inputLines->end() && !gameEndReached);
  const uint64_t dist = inputLines->size() - std::distance(iterator, inputLines->end());
  // x % 0 is undefined in c++
  // avgLinesPerGameTimesProgressSteps = 12*15 as 12 is avg game lines and 15 steps
  const uint64_t progressInterval = 1 + (inputLines->size() / avgLinesPerGameTimesProgressSteps);
  if (games.size() % progressInterval == 0) {
    LOG__DEBUG(Logger::get().BOOK_LOG, "Finding games: {:s}", printProgress(static_cast<double>(dist) / inputLines->size()));
  }
  return game;
}

inline void PGN_Reader::handleMoveSection(VectorIterator &iterator, PGN_Game &game) {
  LOG__TRACE(Logger::get().BOOK_LOG, "Move section line: {}    (length={})", *iterator, iterator->size());

  // read and concatenate all lines belonging to the move section of  one game
  std::ostringstream os;
  do {
    boost::trim(*iterator);
    // ignore comment lines
    if (boost::starts_with(*iterator, "%")) continue;
    // trailing comments
    erase_regex(*iterator, trailingComments);
    // append line
    os << *iterator << " ";
    // look for end pattern
    if (find_regex(*iterator, moveSectionEnd)) break;
  } while (++iterator < inputLines->end());
  std::string moveSection = os.str();
  LOG__TRACE(Logger::get().BOOK_LOG, "Move section: {} (length={})", moveSection, moveSection.size());

  // eliminate unwanted stuff
  replace_all_regex(moveSection, nagAnnotation, " "s);
  replace_all_regex(moveSection, bracketComments, " "s);
  replace_all_regex(moveSection, reservedSymbols, " "s);
  // handle nested RAV variation comments
  do { // no RAV variations supported (could be nested)
    replace_all_regex(moveSection, ravVariants, " "s);
  } while (find_regex(moveSection, ravVariants));
  // remove result from line
  replace_all_regex(moveSection, resultsRgx, ""s);
  // remove move numbers
  replace_all_regex(moveSection, moveNumbers, " "s);
  // eliminate double whitespace
  replace_all_regex(moveSection, doubleWhiteSpace, " "s);
  boost::trim(moveSection);
  LOG__TRACE(Logger::get().BOOK_LOG, "Move section clean (length={}): {} ", moveSection.size(), moveSection);

  // add to game
  std::vector<std::string> moves{};
  boost::split(moves, moveSection, boost::is_space(), boost::token_compress_on);
  LOG__TRACE(Logger::get().BOOK_LOG, "Moves extracted: {} ", moves.size());
  // move detection
  for (auto m : moves) {
    if (find_regex(m, moveRgx)) {
      LOG__TRACE(Logger::get().BOOK_LOG, "Move: {} ", m);
      game.moves.push_back(m);
    }
  }
}

/**
 * PGN Specification:
 * =============================================================================
    <PGN-database> ::= <PGN-game> <PGN-database>
                       <empty>

    <PGN-game> ::= <tag-section> <movetext-section>

    <tag-section> ::= <tag-pair> <tag-section>
                      <empty>

    <tag-pair> ::= [ <tag-name> <tag-value> ]

    <tag-name> ::= <identifier>

    <tag-value> ::= <string>

    <movetext-section> ::= <element-sequence> <game-termination>

    <element-sequence> ::= <element> <element-sequence>
                           <recursive-variation> <element-sequence>
                           <empty>

    <element> ::= <move-number-indication>
                  <SAN-move>
                  <numeric-annotation-glyph>

    <recursive-variation> ::= ( <element-sequence> )

    <game-termination> ::= 1-0
                           0-1
                           1/2-1/2
                           *

    <empty> ::=
 * =============================================================================
 */


