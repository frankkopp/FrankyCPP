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

#ifndef FRANKYCPP_PGN_READER_H
#define FRANKYCPP_PGN_READER_H

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

#include <string>
#include <vector>
#include <map>

#include "types/types.h"
#include "common/Logging.h"
#include "common/Fifo.h"

typedef std::vector<std::string>::iterator VectorIterator;

/**
 * A PGN_Game holds moves as strings after reading a PGN file.
 * Tags reading not yet implemented. 
 */
struct PGN_Game {
  //  std::string pgnNotation{};
  //  std::map<std::string, std::string> tags{};
  std::vector<std::string> moves{};
};

/**
 * Reads pgn games from a vector of lines usually read from a file.
 * Games found can be retrieved by getGames()
 */
class PGN_Reader {

  static const int avgLinesPerGameTimesProgressSteps = 12 * 15;

  std::shared_ptr<std::vector<std::string>> inputLines{};
  std::vector<PGN_Game> games{};

public:
  /**
   * Creates an instance which reads pgn games from a vector of lines usually
   * read from a file.
   */
  PGN_Reader(std::vector<std::string> &lines);

  /**
   * Reads the input lines given in the constructor and stores found games
   * in the given fifo queue.
   * Returns true if successful.
   */
  bool process(Fifo<PGN_Game> &gamesFifo);

  /**
   * Reads the input lines given in the constructor and stores found games
   * in an internal vector which can be retrieved with getGames().
   * Returns true if successful.
   */
  bool process();

  /**
   * Returns the vector of games found after calling process()
   */
  std::vector<PGN_Game> & getGames() { return games; }

private:
  PGN_Game processOneGame(VectorIterator &iterator);
  void handleMoveSection(VectorIterator &iterator, PGN_Game &game);
};


#endif //FRANKYCPP_PGN_READER_H
