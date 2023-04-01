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

#include "UciHandler.h"
#include "Search.h"
#include "SearchConfig.h"
#include "SearchLimits.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Perft.h"
#include "chesscore/Position.h"
#include "common/Logging.h"
#include "types/types.h"
#include "version.h"

#include <exception>
#include <memory>
#include <thread>

UciHandler::UciHandler() {
  pInputStream  = &std::cin;
  pOutputStream = &std::cout;

  pPosition = std::make_shared<Position>();
  pMoveGen  = std::make_shared<MoveGenerator>();
  pPerft    = std::make_shared<Perft>();
  pSearch   = std::make_shared<Search>(this);
}

UciHandler::UciHandler(std::istream* pIstream, std::ostream* pOstream) : UciHandler::UciHandler() {
  pInputStream  = pIstream;
  pOutputStream = pOstream;
}

void UciHandler::loop() {
  loop(pInputStream);
}

void UciHandler::loop(std::istream* pIstream) {
  std::string cmd;
  LOG__DEBUG(Logger::get().UCIHAND_LOG, "UCI Handler waiting for command:");
  do {
    // Block here waiting for input or EOF
    // only blocks on cin!!
    if (!getline(*pIstream, cmd)) cmd = "quit";

    // if handleCommand returns true it has received a quit.
    if (handleCommand(cmd)) return;

    LOG__DEBUG(Logger::get().UCIHAND_LOG, "UCI Handler waiting for command:");
  } while (true);
}

// handles a new command and returns true if received "quit"
bool UciHandler::handleCommand(const std::string& cmd) {
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

void UciHandler::uciCommand() const {
  send("id name FrankyCPP v" + std::to_string(FrankyCPP_VERSION_MAJOR) + "." + std::to_string(FrankyCPP_VERSION_MINOR));
  send("id author Frank Kopp, Germany");
  send(UciOptions::getInstance()->str());
  send("uciok");
}

void UciHandler::isReadyCommand() const {
  pSearch->isReady();
}

void UciHandler::setOptionCommand(std::istringstream& inStream) const {
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

  if (!UciOptions::getInstance()->setOption(const_cast<UciHandler*>(this), name, value)) {
    uciError(fmt::format("Unknown option: {}", name.c_str()));
  }
  LOG__INFO(Logger::get().UCIHAND_LOG, "Set option: {} = {}", name, value);
}

void UciHandler::uciNewGameCommand() const {
  LOG__INFO(Logger::get().UCIHAND_LOG, "New Game");
  if (pSearch->isSearching()) pSearch->stopSearch();
  pSearch->clearTT();
}

void UciHandler::positionCommand(std::istringstream& inStream) {

  // retrieve additional command parameter
  std::string token, fen;
  inStream >> token;

  // setup position with startpos or fen
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
  pPosition = std::make_shared<Position>(fen);

  // if "moves" are given, read all and execute them to position
  if (token == "moves") {
    std::vector<std::string> moves;
    while (inStream >> token) {
      moves.push_back(token);
    }
    // create moves and execute moves on position
    for (const std::string& move : moves) {
      Move moveFromUci = pMoveGen->getMoveFromUci(*pPosition, move);
      if (moveFromUci == MOVE_NONE) {
        uciError(fmt::format("Invalid move {}", move));
        return;
      }
      pPosition->doMove(moveFromUci);
    }
  }
}

void UciHandler::goCommand(std::istringstream& inStream) {

  SearchLimits searchLimits;

  if (!readSearchLimits(inStream, searchLimits)) {
    return;
  }

  // Sanity check search limits
  // sanity check / minimum settings
  if (!(searchLimits.infinite || searchLimits.ponder || searchLimits.depth > 0 || searchLimits.nodes > 0 || searchLimits.mate > 0 || searchLimits.timeControl)) {
    uciError(fmt::format("UCI command go malformed. No effective limits set {}", searchLimits.str()));
    return;
  }
  // sanity check time control
  if (searchLimits.timeControl && searchLimits.moveTime.count() == 0) {
    if (pPosition->getNextPlayer() == WHITE && searchLimits.whiteTime.count() == 0) {
      uciError(fmt::format("UCI command go invalid. White to move but time for white is zero! %s", searchLimits.str()));
      return;
    }
    else if (pPosition->getNextPlayer() == BLACK && searchLimits.blackTime.count() == 0) {
      uciError(fmt::format("UCI command go invalid. Black to move but time for white is zero! %s", searchLimits.str()));
      return;
    }
  }

  // start search
  LOG__INFO(Logger::get().UCIHAND_LOG, "Start Search");
  if (pSearch->isSearching()) {
    // Previous search was still running. Stopping to start new search!
    uciError("Already searching. Stopping search to start new search.");
    pSearch->stopSearch();
  }
  // do not start pondering if not ponder option is set
  if (searchLimits.ponder && !SearchConfig::USE_PONDER) {
    uciError("go ponder command but ponder option is set to false.");
    return;
  }
  pSearch->startSearch(*pPosition, searchLimits);
}

bool UciHandler::readSearchLimits(std::istringstream& inStream, SearchLimits& searchLimits) {
  std::string token;

  while (inStream >> token) {

    if (token == "searchmoves") {
      MoveList searchMoves;
      while (inStream >> token) {
        Move move = pMoveGen->getMoveFromUci(*pPosition, token);
        if (move != MOVE_NONE) {
          searchMoves.push_back(move);
        }
        else {
          break;
        }
      }
      if (!searchMoves.empty()) {
        searchLimits.moves = std::move(searchMoves);
      }
    }

    else if (token == "ponder") {
      searchLimits.ponder = true;
    }

    else if (token == "infinite") {
      searchLimits.infinite = true;
    }

    else if (token == "movetime" || token == "moveTime") {
      inStream >> token;
      try {
        searchLimits.moveTime    = milliseconds(stoi(token));
        searchLimits.timeControl = true;
      } catch (...) { /* ignored */
      }
      if (searchLimits.moveTime.count() <= 0) {
        uciError(fmt::format("Invalid movetime: {}", token));
        return false;
      }
    }

    else if (token == "wtime") {
      inStream >> token;
      try {
        searchLimits.whiteTime   = milliseconds(stoi(token));
        searchLimits.timeControl = true;
      } catch (...) { /* ignored */
      }
      if (searchLimits.whiteTime.count() <= 0) {
        uciError(fmt::format("Invalid wtime: {}", token));
        return false;
      }
    }

    else if (token == "btime") {
      inStream >> token;
      try {
        searchLimits.blackTime   = milliseconds(std::stoi(token));
        searchLimits.timeControl = true;
      } catch (...) { /* ignored */
      }
      if (searchLimits.blackTime.count() <= 0) {
        uciError(fmt::format("Invalid btime: {}", token));
        return false;
      }
    }

    else if (token == "winc") {
      inStream >> token;
      try {
        searchLimits.whiteInc = milliseconds(std::stoi(token));
      } catch (...) { /* ignored */
      }
      if (searchLimits.whiteInc.count() < 0) {
        uciError(fmt::format("Invalid winc: {}", token));
        return false;
      }
    }

    else if (token == "binc") {
      inStream >> token;
      try {
        searchLimits.blackInc = milliseconds(std::stoi(token));
      } catch (...) { /* ignored */
      }
      if (searchLimits.blackInc.count() < 0) {
        uciError(fmt::format("Invalid binc: {}", token));
        return false;
      }
    }

    else if (token == "movestogo") {
      inStream >> token;
      try {
        searchLimits.movesToGo = std::stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.movesToGo <= 0) {
        uciError(fmt::format("Invalid movestogo: {}", token));
        return false;
      }
    }

    else if (token == "depth") {
      inStream >> token;
      try {
        searchLimits.depth = std::stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.depth <= 0 || searchLimits.depth > MAX_DEPTH) {
        uciError(fmt::format("depth not between 1 and {}. Was '{}'", MAX_DEPTH, token));
        return false;
      }
    }

    else if (token == "nodes") {
      inStream >> token;
      try {
        searchLimits.nodes = std::stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.nodes <= 0) {
        uciError(fmt::format("Invalid nodes: {}", token));
        return false;
      }
    }

    else if (token == "mate") {
      inStream >> token;
      try {
        searchLimits.mate = stoi(token);
      } catch (...) { /* ignored */
      }
      if (searchLimits.mate <= 0 || searchLimits.mate > MAX_DEPTH) {
        uciError(fmt::format("mate not between 1 and {}. Was '{}'", MAX_DEPTH, token));
        return false;
      }
    }

    else {
      uciError(fmt::format("Unknown go subcommand. Was '{}'", token));
      return false;
    }
  }

  return true;
}

void UciHandler::stopCommand() const {
  LOG__INFO(Logger::get().UCIHAND_LOG, "Stop Search");
  pPerft->stop();
  pSearch->stopSearch();
}

void UciHandler::ponderHitCommand() const {
  LOG__INFO(Logger::get().UCIHAND_LOG, "Ponder Hit");
  pSearch->ponderhit();
}

void UciHandler::perftCommand(std::istringstream& inStream) {
  LOG__INFO(Logger::get().UCIHAND_LOG, "Start Perft Test");
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
    pPerft->perft(s, e, true);
    sendString("Perft finished.");
  }, startDepth, endDepth);
  perftThread.detach();
}

void UciHandler::registerCommand() {
  uciError("UCI Protocol Command: register not implemented!");
}

void UciHandler::debugCommand() {
  uciError("UCI Protocol Command: debug not implemented!");
}

void UciHandler::send(const std::string& toSend) const {
  LOG__INFO(Logger::get().UCI_LOG, ">> {}", toSend);
  *pOutputStream << toSend << std::endl;
}

void UciHandler::sendString(const std::string& anyString) const {
  send(fmt::format("info string {}", anyString));
}

void UciHandler::sendReadyOk() const {
  send("readyok");
}

void UciHandler::sendResult(Move bestMove, Move ponderMove) const {
  send(fmt::format("bestmove {}{}", str(bestMove), (ponderMove ? " ponder " + str(ponderMove) : "")));
}

void UciHandler::sendCurrentLine(const MoveList& moveList) const {
  send(fmt::format("currline {}", str(moveList)));
}

void UciHandler::sendIterationEndInfo(int depth, int seldepth, Value value, uint64_t nodes,
                                      uint64_t nps, milliseconds time, const MoveList& pv) const {
  send(fmt::format("info depth {} seldepth {} multipv 1 score {} nodes {} nps {} time {} pv {}",
                   depth, seldepth, str(Value(value)), nodes, nps, time.count(), str(pv)));
}

void UciHandler::sendAspirationResearchInfo(int depth, int seldepth, Value value,
                                            const std::string& boundString, uint64_t nodes, uint64_t nps,
                                            milliseconds time, const MoveList& pv) const {
  send(fmt::format("info depth {} seldepth {} multipv 1 score {} {} nodes {} nps {} time {} pv {}",
                   depth, seldepth, str(Value(value)), boundString, nodes, nps, time.count(), str(pv)));
}

void UciHandler::sendCurrentRootMove(Move currmove, std::size_t movenumber) const {
  send(fmt::format("info currmove {} currmovenumber {}", str(currmove),
                   movenumber));
}

void UciHandler::sendSearchUpdate(int depth, int seldepth, uint64_t nodes, uint64_t nps,
                                  milliseconds time, int hashfull) const {
  send(fmt::format("info depth {} seldepth {} nodes {} nps {} time {} hashfull {}",
                   depth, seldepth, nodes, nps, time.count(), hashfull));
}

void UciHandler::uciError(const std::string& msg) const {
  LOG__ERROR(Logger::get().UCIHAND_LOG, msg);
  sendString(msg);
}
