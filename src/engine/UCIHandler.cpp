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

#include <memory>
#include <thread>

#include "chesscore/MoveGenerator.h"
#include "chesscore/Perft.h"
#include "chesscore/Position.h"
#include "types/types.h"

#include "Logging.h"
#include "Search.h"
#include "SearchLimits.h"
#include "version.h"

#include "UCIHandler.h"

UCIHandler::UCIHandler() {
  pInputStream  = &std::cin;
  pOutputStream = &std::cout;

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
  std::string cmd;
  LOG__DEBUG(Logger::get().UCIHAND_LOG, "UCI Handler waiting for command:");
  do {
    // Block here waiting for input or EOF
    // only blocks on cin!!
    if (!getline(*pIstream, cmd)) cmd = "quit";

    // if handleCommand returns true it has received a quit.
    if (handleCommand(cmd)) return;

    LOG__DEBUG(Logger::get().UCIHAND_LOG, "UCI Handler waiting for command:");
  } while (1);
}

// handles a new command and returns true if received "quit"
bool UCIHandler::handleCommand(const std::string& cmd) {
  //  create the stream object
  std::istringstream inStream(cmd);
  LOG__INFO(Logger::get().UCI_LOG, "<< {}", inStream.str());
  LOG__DEBUG(Logger::get().UCIHAND_LOG, "UCI Handler received command: {}", inStream.str());

  // read word from stream delimiter is whitespace
  // to get line use inStream.str()
  std::string token;
  inStream >> std::skipws >> token;

  // @formatter:off
  if (token == "quit") { return true; }
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
  else if (token == "perft") { perftCommand(inStream); }
  else if (token == "noop") { /* noop */}
  else uciError(fmt::format("Unknown UCI command: {}", token));
  // @formatter:on

  LOG__DEBUG(Logger::get().UCIHAND_LOG, "UCI Handler processed command: {}", token);
  return false;
}

void UCIHandler::uciCommand() const {
  send("id name FrankyCPP v" + std::to_string(FrankyCPP_VERSION_MAJOR) + "." + std::to_string(FrankyCPP_VERSION_MINOR));
  send("id author Frank Kopp, Germany");
  // TODO UCIOptions
  uciError("Not yet implemented: UCI command: uci");
  send("uciok");
}

void UCIHandler::isReadyCommand() const {
  // TODO initialize tt and book
  uciError("Not yet implemented: UCI command: isready");
  send("readyok");
}

void UCIHandler::setOptionCommand(std::istringstream& inStream) const {
  std::string token, name, value;
  if (inStream >> token && token != "name") {
    uciError(fmt::format("Command setoption is malformed - expected 'name': {}", token));
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
//  uciError("Not yet implemented: UCI command: setoption");

  LOG__INFO(Logger::get().UCIHAND_LOG, "Set option {} = {}", name, value);

//  // find option entry
//  const auto optionIterator =
//    std::find_if(uciOptions.optionVector.begin(),
//                 uciOptions.optionVector.end(),
//                 [&](std::pair<std::string, UciOptions> p) {
//                   return name == p.first;
//                 });
//
//  if (optionIterator != uciOptions.optionVector.end()) {
//    optionIterator->second.pHandler();
//  } else {
//    uciError(fmt::format("Unknown option: %s", name.c_str()));
//  }
}

void UCIHandler::uciNewGameCommand() const {
  // TODO new game
  uciError("Not yet implemented: UCI command: ucinewgame");
}

void UCIHandler::positionCommand(std::istringstream& inStream) {

  // retrieve additional command parameter
  std::string token, fen;
  inStream >> token;

  // setup poistion whith startpos or fen
  fen = START_POSITION_FEN;
  if (token == "startpos") {// just keep default
    inStream >> token;
  }
  else if (token == "fen") {
    fen.clear();// reset to empty
    while (inStream >> token && token != "moves") {
      fen += token + " ";
    }
  }

  // TODO error handling when fen is invalid
  LOG__INFO(Logger::get().UCIHAND_LOG, "Set position to {}", fen);
  position = std::make_shared<Position>(fen);

  // if "moves" are given, read all and execute them to position
  if (token == "moves") {
    std::vector<std::string> moves;
    while (inStream >> token) {
      moves.push_back(token);
    }
    // create moves and execute moves on position
    for (const std::string& move : moves) {
      Move moveFromUci = mg->getMoveFromUci(*position, move);
      if (moveFromUci == MOVE_NONE) {
        uciError(fmt::format("Invalid move {}", move));
        return;
      }
      LOG__DEBUG(Logger::get().UCIHAND_LOG, "Do move {}", move);
      position->doMove(moveFromUci);
    }
  }
}


void UCIHandler::goCommand(std::istringstream& inStream) {
  std::string token, startFen;
  SearchLimits searchLimits{};

  while (inStream >> token) {
    if (token == "searchmoves") {
      MoveList searchMoves;
      while (inStream >> token) {
        Move move = mg->getMoveFromUci(*position, token);
        if (move != MOVE_NONE) {
          searchMoves.push_back(move);
        }
        else {
          break;
        }
      }
      if (!searchMoves.empty()) {
        searchLimits.moves = searchMoves;
      }
    }
    if (token == "ponder") {
      searchLimits.ponder = true;
    }
    if (token == "wtime") {
      inStream >> token;
      try {
        searchLimits.whiteTime = stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.whiteTime <= 0) {
        uciError(fmt::format("Invalid wtime: {}", token));
        return;
      }
    }
    if (token == "btime") {
      inStream >> token;
      try {
        searchLimits.blackTime = stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.blackTime <= 0) {
        uciError(fmt::format("Invalid btime: {}", token));
        return;
      }
    }
    if (token == "winc") {
      inStream >> token;
      try {
        searchLimits.whiteInc = std::stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.whiteInc < 0) {
        uciError(fmt::format("Invalid winc: {}", token));
        return;
      }
    }
    if (token == "binc") {
      inStream >> token;
      try {
        searchLimits.blackInc = std::stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.blackInc < 0) {
        uciError(fmt::format("Invalid binc: {}", token));
        return;
      }
    }
    if (token == "movestogo") {
      inStream >> token;
      try {
        searchLimits.movesToGo = std::stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.movesToGo <= 0) {
        uciError(fmt::format("Invalid movestogo: {}", token));
        return;
      }
    }
    if (token == "depth") {
      inStream >> token;
      try {
        searchLimits.depth = std::stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.depth <= 0 || searchLimits.depth > MAX_DEPTH) {
        uciError(fmt::format("depth not between 1 and {}. Was '{}'", MAX_DEPTH, token));
        return;
      }
    }
    if (token == "nodes") {
      inStream >> token;
      try {
        searchLimits.nodes = std::stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.nodes <= 0) {
        uciError(fmt::format("Invalid nodes: {}", token));
        return;
      }
    }
    if (token == "mate") {
      inStream >> token;
      try {
        searchLimits.mate = stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.mate <= 0 || searchLimits.mate > MAX_DEPTH) {
        uciError(fmt::format("mate not between 1 and {}. Was '{}'", MAX_DEPTH, token));
        return;
      }
    }
    if (token == "movetime" || token == "moveTime") {
      inStream >> token;
      try {
        searchLimits.moveTime = stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.moveTime <= 0) {
        uciError(fmt::format("Invalid movetime: {}", token));
        return;
      }
    }
    if (token == "infinite") {
      searchLimits.infinite = true;
    }
  }
  // start search
  // TODO: search
  uciError("Search not yet implemented");
}

void UCIHandler::stopCommand() const {
  perft->stop();
  // TODO: stop search
  uciError("StopSearch not yet implemented");
}

void UCIHandler::ponderHitCommand() const {
  // TODO: search
  uciError("Ponderhit not yet implemented");
}

void UCIHandler::perftCommand(std::istringstream& inStream) {
  std::string token;
  inStream >> token;
  int startDepth = 1;
  try {
    startDepth = stoi(token);
  } catch (...) { /* Ignore */
  }
  if (startDepth <= 0 || startDepth > MAX_DEPTH) {
    uciError(fmt::format("perft start depth not between 1 and {}. Was '{}'", MAX_DEPTH, token));
    return;
  }
  int endDepth = startDepth;
  if (inStream >> token) {
    try {
      endDepth = stoi(token);
    } catch (...) { /* Ignore */
    }
    if (endDepth <= 0 || endDepth > MAX_DEPTH) {
      uciError(fmt::format("perft end depth not between 1 and {}. Was '{}'", MAX_DEPTH, token));
    }
  }
  std::thread perftThread([&](int s, int e) {
    perft->perft(s, e, true);
    sendString("Perft finished.");
  },
                          startDepth, endDepth);
  perftThread.detach();
}

void UCIHandler::registerCommand() {
  LOG__WARN(Logger::get().UCIHAND_LOG, "UCI Protocol Command: register not implemented!");
}

void UCIHandler::debugCommand() {
  LOG__WARN(Logger::get().UCIHAND_LOG, "UCI Protocol Command: debug not implemented!");
}

void UCIHandler::send(const std::string& toSend) const {
  LOG__INFO(Logger::get().UCI_LOG, ">> {}", toSend);
  *pOutputStream << toSend << std::endl;
}

void UCIHandler::sendString(const std::string& anyString) const {
  send(fmt::format("info string {}", anyString));
}

void UCIHandler::sendResult(Move bestMove, Move ponderMove) const {
  send(fmt::format("bestmove {}{}", str(bestMove), (ponderMove ? " ponder " + str(ponderMove) : "")));
}

void UCIHandler::sendCurrentLine(const MoveList& moveList) const {
  send(fmt::format("currline {}", str(moveList)));
}

void UCIHandler::sendIterationEndInfo(int depth, int seldepth, Value value, uint64_t nodes,
                                      uint64_t nps, MilliSec time, const MoveList& pv) const {
  send(fmt::format("info depth {} seldepth {} multipv 1 score {} nodes {} nps {} time {} pv {}",
                   depth, seldepth, str(Value(value)), nodes, nps, time, str(pv)));
}

void UCIHandler::sendAspirationResearchInfo(int depth, int seldepth, Value value,
                                            const std::string& bound, uint64_t nodes, uint64_t nps,
                                            MilliSec time, const MoveList& pv) const {
  send(fmt::format("info depth {} seldepth {} multipv 1 score {} {} nodes {} nps {} time {} pv {}",
                   depth, seldepth, str(Value(value)), bound, nodes, nps, time, str(pv)));
}

void UCIHandler::sendCurrentRootMove(Move currmove, std::size_t movenumber) const {
  send(fmt::format("info currmove {} currmovenumber {}", str(currmove),
                   movenumber));
}

void UCIHandler::sendSearchUpdate(int depth, int seldepth, uint64_t nodes, uint64_t nps,
                                  MilliSec time, int hashfull) const {
  send(fmt::format("info depth {} seldepth {} nodes {} nps {} time {} hashfull {}",
                   depth, seldepth, nodes, nps, time, hashfull));
}
void UCIHandler::uciError(std::string const& msg) const {
  LOG__ERROR(Logger::get().UCIHAND_LOG, msg);
  sendString(msg);
}


