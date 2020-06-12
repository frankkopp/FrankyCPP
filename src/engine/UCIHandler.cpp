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

#include "version.h"
#include "Logging.h"
#include "chesscore/Position.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Perft.h"
#include "Search.h"
#include "UCIHandler.h"

UCIHandler::UCIHandler() {
  position = std::make_shared<Position>();
  mg       = std::make_shared<MoveGenerator>();
  perft    = std::make_shared<Perft>();
  search   = std::make_shared<Search>();

  search->setUciHandler(this);
}

UCIHandler::UCIHandler(std::istream* pIstream, std::ostream* pOstream) : UCIHandler::UCIHandler() {
  pInputStream  = pIstream;
  pOutputStream = pOstream;
}

void UCIHandler::loop() {
  loop(pInputStream);
}

void UCIHandler::loop(std::istream* pIstream) {
  std::string cmd, token;
  do {
    LOG__DEBUG(Logger::get().UCIHAND_LOG, "UCI Handler waiting for command:");

    // Block here waiting for input or EOF
    // only blocks on cin!!
    if (!getline(*pIstream, cmd)) cmd = "quit";

    //  create the stream object
    std::istringstream inStream(cmd);

    LOG__INFO(Logger::get().UCI_LOG, "<< {}", inStream.str());
    LOG__DEBUG(Logger::get().UCIHAND_LOG, "UCI Handler received command: {}", inStream.str());

    // clear possible previous entries
    token.clear();

    // read word from stream delimiter is whitespace
    // to get line use inStream.str()
    inStream >> std::skipws >> token;

    if (token == "quit") { break; }
    else if (token == "uci") { uciCommand(); }
    else if (token == "isready") { isReadyCommand(); }
    else if (token == "setoption") { setOptionCommand(inStream); }
    else if (token == "ucinewgame") { uciNewGameCommand(); }
    else if (token == "position") { positionCommand(inStream); }
    else if (token == "go") { goCommand(inStream); }
    else if (token == "stop") { stopCommand(); }
    else if (token == "ponderhit") { ponderHitCommand(); }
    else if (token == "register") { registerCommand(); }
    else if (token == "debug") { debugCommand(); }
    else if (token == "noop") { /* noop */}
    else
      LOG__WARN(Logger::get().UCIHAND_LOG, "Unknown UCI command: {}", token);

    LOG__DEBUG(Logger::get().UCIHAND_LOG, "UCI Handler processed command: {}", token);

  } while (token != "quit");
}

void UCIHandler::uciCommand() const {
  send("id name FrankyCPP v" + std::to_string(FrankyCPP_VERSION_MAJOR) + "." +
       std::to_string(FrankyCPP_VERSION_MINOR));
  send("id author Frank Kopp, Germany");
  // TODO UCIOption
  //  send(pEngine->str());
  send("uciok");
}

void UCIHandler::isReadyCommand() const {
  // TODO initialize tt and book
  send("readyok");
}

void UCIHandler::setOptionCommand(std::istringstream &inStream) const {
  std::string token, name, value;
  if (inStream >> token && token != "name") {
    LOG__WARN(Logger::get().UCIHAND_LOG, "Command setoption is malformed - expected 'name': {}", token);
    return;
  }
  // read name which could contain spaces
  while (inStream >> token && token != "value") {
    if (!name.empty()) {
      name += " ";
    }
    name += token;
  }
  // read value which could contain spaces
  while (inStream >> token) {
    if (!value.empty()) name += " ";
    value += token;
  }
  // TODO set option
}

void UCIHandler::uciNewGameCommand() const {
 // TODO new game
}


void UCIHandler::positionCommand(std::istringstream &inStream) {

  // retrieve additional command parameter
  std::string token, fen;
  inStream >> token;

  // default
  fen = START_POSITION_FEN;
  if (token == "startpos") { // just keep default
    inStream >> token;
  }
  else if (token == "fen") {
    fen.clear(); // reset to empty
    while (inStream >> token && token != "moves") {
      fen += token + " ";
    }
  }

  LOG__INFO(Logger::get().UCIHAND_LOG, "Set position to {}", fen);
  position = std::make_shared<Position>(fen);

  if (token == "moves") {
    std::vector<std::string> moves;
    // read all moves from uci
    while (inStream >> token) {
      moves.push_back(token);
    }
    // create moves and execute moves on position
    for (const std::string &move : moves) {
      Move moveFromUci = mg->getMoveFromUci(*position, move);
      if (moveFromUci == MOVE_NONE) {
        const std::string& msg = fmt::format("Invalid move {}", move);
        LOG__ERROR(Logger::get().UCIHAND_LOG, msg);
        sendString(msg);
        return;
      }
      LOG__INFO(Logger::get().UCIHAND_LOG, "Do move {}", move);
      position->doMove(moveFromUci);
    }
  }
}


void UCIHandler::goCommand(std::istringstream &inStream) {
  std::string token, startFen;

  // resetting search mode
  UCISearchMode searchMode = UCISearchMode();

  while (inStream >> token) {
    if (token == "searchmoves") {
      MoveList searchMoves;
      while (inStream >> token) {
        Move move = createMove(token.c_str());
        if (isMove(move)) { searchMoves.push_back(move); }
        else { break; }
      }
      if (!searchMoves.empty()) searchMode.moves = searchMoves;
    }
    if (token == "ponder") {
      searchMode.ponder = true;
    }
    if (token == "wtime") {
      inStream >> token;
      try {
        searchMode.whiteTime = stoi(token);
      } catch (...) {
        LOG__ERROR(Logger::get().UCIHAND_LOG, "Given string is not a valid int: {} ({}:{})", token, __FILENAME__, __LINE__);
      }
      if (searchMode.whiteTime <= 0) {
        LOG__WARN(Logger::get().UCIHAND_LOG, "Invalid wtime. Was '{}'", token);
        return;
      }
    }
    if (token == "btime") {
      inStream >> token;
      try {
        searchMode.blackTime = stoi(token);
      } catch (...) {
        LOG__ERROR(Logger::get().UCIHAND_LOG, "Given string is not a valid int: {} ({}:{})", token, __FILENAME__, __LINE__);
      }
      if (searchMode.blackTime <= 0) {
        LOG__WARN(Logger::get().UCIHAND_LOG, "Invalid btime. Was '{}'", token);
        return;
      }
    }
    if (token == "winc") {
      inStream >> token;
      try {
        searchMode.whiteInc = std::stoi(token);
      } catch (...) {
        LOG__ERROR(Logger::get().UCIHAND_LOG, "Given string is not a valid int: {} ({}:{})", token, __FILENAME__, __LINE__);
      }
      if (searchMode.whiteInc < 0) {
        LOG__WARN(Logger::get().UCIHAND_LOG, "Invalid winc. Was '{}'", token);
        return;
      }
    }
    if (token == "binc") {
      inStream >> token;
      try {
        searchMode.blackInc = std::stoi(token);
      } catch (...) {
        LOG__ERROR(Logger::get().UCIHAND_LOG,
                   "Given string is not a valid int: {} ({}:{})", token, __FILENAME__, __LINE__);
      }
      if (searchMode.blackInc < 0) {
        LOG__WARN(Logger::get().UCIHAND_LOG, "Invalid binc. Was '{}'", token);
        return;
      }
    }
    if (token == "movestogo") {
      inStream >> token;
      try {
        searchMode.movesToGo = std::stoi(token);
      } catch (...) {
        LOG__ERROR(Logger::get().UCIHAND_LOG,
                   "Given string is not a valid int: {} ({}:{})", token, __FILENAME__, __LINE__);
      }
      if (searchMode.movesToGo <= 0) {
        LOG__WARN(Logger::get().UCIHAND_LOG, "Invalid movestogo. Was '{}'", token);
        return;
      }
    }
    if (token == "depth") {
      inStream >> token;
      try {
        searchMode.depth = std::stoi(token);
      } catch (...) {
        LOG__ERROR(Logger::get().UCIHAND_LOG,
                   "Given string is not a valid int: {} ({}:{})", token, __FILENAME__, __LINE__);
      }
      if (searchMode.depth <= 0 || searchMode.depth > PLY_MAX) {
        LOG__WARN(Logger::get().UCIHAND_LOG, "depth not between 1 and {}. Was '{}'", PLY_MAX, token);
        return;
      }
    }
    if (token == "nodes") {
      inStream >> token;
      try {
        searchMode.nodes = std::stoi(token);
      } catch (...) {
        LOG__ERROR(Logger::get().UCIHAND_LOG,
                   "Given string is not a valid int: {} ({}:{})", token, __FILENAME__, __LINE__);
      }
      if (searchMode.nodes <= 0) {
        LOG__WARN(Logger::get().UCIHAND_LOG, "Invalid nodes. Was '{}'", token);
        return;
      }
    }
    if (token == "mate") {
      inStream >> token;
      try {
        searchMode.mate = stoi(token);
      } catch (...) {
        LOG__ERROR(Logger::get().UCIHAND_LOG,
                   "Given string is not a valid int: {} ({}:{})", token, __FILENAME__, __LINE__);
      }
      if (searchMode.mate <= 0 || searchMode.mate > PLY_MAX) {
        LOG__WARN(Logger::get().UCIHAND_LOG, "mate not between 1 and {}. Was '{}'", PLY_MAX, token);
        return;
      }
    }
    if (token == "movetime") {
      inStream >> token;
      try {
        searchMode.movetime = stoi(token);
      } catch (...) {
        LOG__ERROR(Logger::get().UCIHAND_LOG,
                   "Given string is not a valid int: {} ({}:{})", token, __FILENAME__, __LINE__);
      }
      if (searchMode.movetime <= 0) {
        LOG__WARN(Logger::get().UCIHAND_LOG, "Invalid movetime. Was '{}'", token);
        return;
      }
    }
    if (token == "infinite") {
      searchMode.infinite = true;
    }
    if (token == "perft") {
      searchMode.perft = true;
      inStream >> token;
      try {
        searchMode.depth = stoi(token);
      } catch (...) {
        LOG__ERROR(Logger::get().UCIHAND_LOG,
                   "Given string is not a valid int: {} ({}:{})", token, __FILENAME__, __LINE__);
      }
      if (searchMode.depth <= 0 || searchMode.depth > PLY_MAX) {
        LOG__WARN(Logger::get().UCIHAND_LOG, "perft depth not between 1 and {}. Was '{}'", PLY_MAX, token);
        return;
      }
    }
  }

  // start search in engine
  pEngine->startSearch(searchMode);
}

