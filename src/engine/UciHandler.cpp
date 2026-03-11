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

#include "UciHandler.h"
#include "Benchmark.h"
#include "Search.h"
#include "SearchLimits.h"
#include "UciOptions.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Perft.h"
#include "chesscore/Position.h"
#include "common/Logging.h"
#include "config/ConfigManager.h"
#include "types/types.h"
#include "version.h"

#include <memory>
#include <thread>

using namespace engine;
using namespace chess;
using namespace config;
using namespace common;

UciHandler::UciHandler()
    : pPosition(std::make_unique<Position>()),
      pMoveGen(std::make_unique<MoveGenerator>()),
      pPerft(std::make_unique<Perft>()),
      pSearch(std::make_unique<Search>(this)),
      pInputStream(&std::cin),
      pOutputStream(&std::cout) {}

UciHandler::UciHandler(std::istream* pIstream, std::ostream* pOstream) : UciHandler() {
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

    // if handleCommand returns true, it has received a quit-command.
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

  // clang-format off
  if      (token == "quit")            { return true; }
  if      (token == "uci")             { uciCommand(); }
  else if (token == "isready")         { isReadyCommand(); }
  else if (token == "setoption")       { setOptionCommand(inStream); }
  else if (token == "ucinewgame")      { uciNewGameCommand(); }
  else if (token == "position")        { positionCommand(inStream); }
  else if (token == "go")              { goCommand(inStream); }
  else if (token == "stop")            { stopCommand(); }
  else if (token == "ponderhit")       { ponderHitCommand(); }
  else if (token == "register")        { registerCommand(); }
  else if (token == "debug")           { debugCommand(); }
  else if (token == "perft")           { perftCommand(inStream); }
  else if (token == "bench")           { benchCommand(inStream); }
  else if (token == "getoptions")      { getOptionsCommand(); }
  else if (token == "extendedoptions") { getExtendedOptionsCommand(); }
  else if (token == "help")            { helpCommand(); }
  else if (token == "noop")            { /* noop */  }
  else
   uciError(std::format("Unknown UCI command: {}", token));
  // clang-format on

  LOG__DEBUG(Logger::get().UCIHAND_LOG, "UCI Handler processed command: {}", token);
  return false;
}

void UciHandler::uciCommand() const {
  std::string idName = "id name FrankyCPP v" + std::to_string(FrankyCPP_VERSION_MAJOR) + "." + std::to_string(FrankyCPP_VERSION_MINOR);
#ifdef FRANKYCPP_PRODUCTION
  idName.append(" (stripped)");
#endif
  send(idName);
  send("id author Frank Kopp, Germany");
  send(UciOptions::getInstance()->str());
  send("uciok");
}

void UciHandler::isReadyCommand() const {
  pSearch->isReady();
}

void UciHandler::setOptionCommand(std::istringstream& inStream) {
  std::string token;
  std::string name;
  std::string value;
  if (inStream >> token && token != "name") {
    uciError(std::format("Command setoption is malformed - expected 'name': {}", token));
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

  if (!UciOptions::getInstance()->setOption(this, name, value)) {
    uciError(std::format("Could not set option: {} = {}", name.c_str(), value.c_str()));
  }
  LOG__INFO(Logger::get().UCIHAND_LOG, "Set option: {} = {}", name, value);
}

// TODO: check if we need to clear more state here!
void UciHandler::uciNewGameCommand() const {
  LOG__INFO(Logger::get().UCIHAND_LOG, "New Game");
  pSearch->newGame(); // Clears TT, History, and recreates Evaluator (clears PawnTT)
}

void UciHandler::positionCommand(std::istringstream& inStream) {

  // retrieve additional command parameter
  std::string token;
  inStream >> token;

  // setup position with startpos or fen
  std::string fen = START_POSITION_FEN;
  if (token == "startpos") { // just keep default
    inStream >> token;
  }
  else if (token == "fen") {
    fen.clear(); // reset to empty
    while (inStream >> token && token != "moves") {
      fen += token + " ";
    }
  }

  // TODO error handling when fen is invalid

  LOG__INFO(Logger::get().UCIHAND_LOG, "Set position to {}", fen);
  pPosition = std::make_unique<Position>(fen);

  // if "moves" are given, read all and execute them to position
  if (token == "moves") {
    std::vector<std::string> moves;
    while (inStream >> token) {
      moves.push_back(token);
    }
    // create moves and execute moves on position
    for (const std::string& move : moves) {
      const Move moveFromUci = pMoveGen->getMoveFromUci(*pPosition, move);
      if (moveFromUci == MOVE_NONE) {
        uciError(std::format("Invalid move {}", move));
        return;
      }
      pPosition->doMove(moveFromUci);
    }
  }
}

void UciHandler::goCommand(std::istringstream& inStream) const {

  SearchLimits searchLimits;

  if (!readSearchLimits(inStream, searchLimits)) {
    return;
  }

  // Sanity check search limits
  // sanity check / minimum settings
  if (!searchLimits.infinite && !searchLimits.ponder && searchLimits.depth <= 0 && searchLimits.nodes <= 0 && searchLimits.mate <= 0 && !searchLimits.timeControl) {
    uciError(std::format("UCI command go malformed. No effective limits set: {}", searchLimits.str()));
    return;
  }
  // sanity check time control
  if (searchLimits.timeControl && searchLimits.moveTime.count() == 0) {
    if (pPosition->getNextPlayer() == WHITE && searchLimits.whiteTime.count() == 0) {
      uciError(std::format("UCI command go invalid. White to move but time for white is zero! {}", searchLimits.str()));
      return;
    }
    else if (pPosition->getNextPlayer() == BLACK && searchLimits.blackTime.count() == 0) {
      uciError(std::format("UCI command go invalid. Black to move but time for white is zero! {}", searchLimits.str()));
      return;
    }
  }

  // start search
  LOG__INFO(Logger::get().UCIHAND_LOG, "Start Search");
  if (pSearch->isSearching()) {
    // The previous search was still running. Stopping to start a new search!
    uciError("Already searching. Stopping search to start new search.");
    pSearch->stopSearch();
  }
  // do not start pondering if not a ponder option is set
  if (searchLimits.ponder && !SEARCH_CONFIG.USE_PONDER) {
    uciError("go ponder command but ponder option is set to false.");
    return;
  }
  pSearch->startSearch(*pPosition, searchLimits);
}

bool UciHandler::readSearchLimits(std::istringstream& inStream, SearchLimits& searchLimits) const {
  std::string token;

  auto readIntToken = [&](int& target) {
    inStream >> token;
    try {
      target = std::stoi(token);
    } catch (...) { /* ignored */
    }
  };

  auto readUint64tToken = [&](uint64_t& target) {
    inStream >> token;
    try {
      target = std::stoi(token);
    } catch (...) { /* ignored */
    }
  };

  auto readMillisToken = [&](milliseconds& target, const bool setTimeControl) {
    inStream >> token;
    try {
      target = milliseconds(std::stoi(token));
      if (setTimeControl) {
        searchLimits.timeControl = true;
      }
    } catch (...) { /* ignored */
    }
  };

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
        searchLimits.moves = searchMoves;
      }
    }

    else if (token == "ponder") {
      searchLimits.ponder = true;
    }

    else if (token == "infinite") {
      searchLimits.infinite = true;
    }

    else if (token == "movetime" || token == "moveTime") {
      readMillisToken(searchLimits.moveTime, true);
      if (searchLimits.moveTime.count() <= 0) {
        uciError(std::format("Invalid movetime: {}", token));
        return false;
      }
    }

    else if (token == "wtime") {
      readMillisToken(searchLimits.whiteTime, true);
      if (searchLimits.whiteTime.count() <= 0) {
        uciError(std::format("Invalid wtime: {}", token));
        return false;
      }
    }

    else if (token == "btime") {
      readMillisToken(searchLimits.blackTime, true);
      if (searchLimits.blackTime.count() <= 0) {
        uciError(std::format("Invalid btime: {}", token));
        return false;
      }
    }

    else if (token == "winc") {
      readMillisToken(searchLimits.whiteInc, false);
      if (searchLimits.whiteInc.count() < 0) {
        uciError(std::format("Invalid winc: {}", token));
        return false;
      }
    }

    else if (token == "binc") {
      readMillisToken(searchLimits.blackInc, false);
      if (searchLimits.blackInc.count() < 0) {
        uciError(std::format("Invalid binc: {}", token));
        return false;
      }
    }

    else if (token == "movestogo") {
      readIntToken(searchLimits.movesToGo);
      if (searchLimits.movesToGo <= 0) {
        uciError(std::format("Invalid movestogo: {}", token));
        return false;
      }
    }

    else if (token == "depth") {
      readIntToken(searchLimits.depth);
      if (searchLimits.depth <= 0 || searchLimits.depth > MAX_DEPTH) {
        uciError(std::format("depth not between 1 and {}. Was '{}'", MAX_DEPTH, token));
        return false;
      }
    }

    else if (token == "nodes") {
      readUint64tToken(searchLimits.nodes);
      if (searchLimits.nodes <= 0) {
        uciError(std::format("Invalid nodes: {}", token));
        return false;
      }
    }

    else if (token == "mate") {
      readIntToken(searchLimits.mate);
      if (searchLimits.mate <= 0 || searchLimits.mate > MAX_DEPTH) {
        uciError(std::format("mate not between 1 and {}. Was '{}'", MAX_DEPTH, token));
        return false;
      }
    }

    else {
      uciError(std::format("Unknown go subcommand. Was '{}'", token));
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

void UciHandler::perftCommand(std::istringstream& inStream) const {
  LOG__INFO(Logger::get().UCIHAND_LOG, "Start Perft Test");
  std::string token;
  inStream >> token;
  int startDepth = 0;
  try {
    startDepth = stoi(token);
  } catch (...) { /* Ignore */
    startDepth = 1;
  }
  if (startDepth <= 0 || startDepth > MAX_DEPTH) {
    uciError(std::format("perft start depth not between 1 and {}. Was '{}'", MAX_DEPTH, token));
    return;
  }
  int endDepth = startDepth;
  if (inStream >> token) {
    try {
      endDepth = stoi(token);
    } catch (...) { /* Ignore */
    }
    if (endDepth <= 0 || endDepth > MAX_DEPTH) {
      uciError(std::format("perft end depth not between 1 and {}. Was '{}'", MAX_DEPTH, token));
    }
  }
  std::thread perftThread([&](const int s, const int e) {
    pPerft->perft(pPosition->strFen(), s, e, true);
    sendString("Perft finished.");
  },
                          startDepth, endDepth);
  perftThread.detach();
}

void UciHandler::benchCommand(std::istringstream& inStream) const {
  LOG__INFO(Logger::get().UCIHAND_LOG, "Start Benchmark");

  // Parse optional arguments: bench [depth] [hash] [threads]
  engine::BenchConfig config;
  std::string token;

  if (inStream >> token) {
    try {
      config.depth = std::stoi(token);
      if (config.depth <= 0 || config.depth > MAX_DEPTH) {
        uciError(std::format("bench depth must be between 1 and {}. Was '{}'", MAX_DEPTH, token));
        return;
      }
    } catch (...) {
      uciError(std::format("bench depth invalid: '{}'", token));
      return;
    }
  }

  if (inStream >> token) {
    try {
      config.hashSizeMB = std::stoi(token);
      if (config.hashSizeMB <= 0 || config.hashSizeMB > 65536) {
        uciError(std::format("bench hash must be between 1 and 65536 MB. Was '{}'", token));
        return;
      }
    } catch (...) {
      uciError(std::format("bench hash invalid: '{}'", token));
      return;
    }
  }

  if (inStream >> token) {
    try {
      config.threads = std::stoi(token);
      if (config.threads <= 0 || config.threads > 256) {
        uciError(std::format("bench threads must be between 1 and 256. Was '{}'", token));
        return;
      }
    } catch (...) {
      uciError(std::format("bench threads invalid: '{}'", token));
      return;
    }
  }

  // Run the benchmark
  const auto result = engine::Benchmark::run(config);
  engine::Benchmark::printResults(result);

  LOG__INFO(Logger::get().UCIHAND_LOG, "Benchmark finished - NPS: {}", static_cast<uint64_t>(result.nps));
}

void UciHandler::registerCommand() const {
  uciError("UCI Protocol Command: register not implemented!");
}

void UciHandler::debugCommand() const {
  uciError("UCI Protocol Command: debug not implemented!");
}

void UciHandler::helpCommand() const {

  // Provide a compact but comprehensive help for manual UCI use (logging suppressed)
  auto out = [&](const std::string& s) {
    *pOutputStream << std::format("{}", s) << std::endl;
  };

  out("FrankyCPP UCI help");
  out("Commands are case-sensitive and follow the UCI protocol. Each command is a separate line.");

  out("uci");
  out("  Identify engine and list available options, then prints 'uciok'.");

  out("isready");
  out("  Probes readiness; engine answers 'readyok' when ready.");

  out("setoption name <Name> [value <Value>]");
  out("  Sets an engine option. Use 'uci' to list options and their types/defaults.");

  out("ucinewgame");
  out("  Starts a new game. Stops any search and clears transposition table.");

  out("position [startpos | fen <FEN>] [moves <m1> <m2> ...]");
  out("  Sets the current position. 'startpos' for initial setup or 'fen' for custom.");
  out("  Optional 'moves' applies a space-separated list of UCI moves to the position.");

  out("go [subcommands...]");
  out("  Starts a search from the current position. Subcommands:");
  out("    searchmoves <m1> <m2> ...   Limit root search to given UCI moves.");
  out("    ponder                        Search in ponder mode (requires option Ponder).");
  out(std::format("      Ponder option is currently {}.", SEARCH_CONFIG.USE_PONDER ? "enabled" : "disabled"));
  out("    infinite                      Search until 'stop'.");
  out("    movetime <ms>                 Fixed time for the whole move in milliseconds.");
  out("    wtime <ms> btime <ms>         Remaining time for each side in milliseconds.");
  out("    winc <ms> binc <ms>           Increment per move in milliseconds (optional).");
  out("    movestogo <n>                 Moves to the next time control (optional).");
  out(std::format("    depth <n>                      Search depth 1..{} (plies).", MAX_DEPTH));
  out("    nodes <n>                      Search until node count reached.");
  out(std::format("    mate <n>                       Search for mate in 1..{} (plies).", MAX_DEPTH));
  out("  Notes:");
  out("    - Time control becomes active if any of movetime/wtime/btime is given.");
  out("    - If using wtime/btime without movetime, the side to move must have non-zero time.");
  out("    - 'go ponder' will be rejected if the Ponder option is disabled.");

  out("stop");
  out("  Stops an ongoing search or perft.");

  out("ponderhit");
  out("  Informs engine that opponent played the pondered move; converts ponder into normal search.");

  out("perft <startDepth> [endDepth]");
  out(std::format("  Runs a perft from the current position for depths {}..{}. If endDepth omitted, only startDepth is used.", 1, MAX_DEPTH));

  out("bench [depth] [hash] [threads]");
  out("  Runs a standardized benchmark to measure NPS (nodes per second).");
  out("  Default: bench 10 128 1");
  out("    depth    Search depth (1-127, default 10)");
  out("    hash     Hash table size in MB (1-65536, default 128)");
  out("    threads  Number of threads (1-256, default 1, reserved for future SMP)");

  out("getoptions");
  out("  Non-standard extension: Lists all options with their current values (for testing).");
  out("  Format: 'option name <name> type <type> current <value>' followed by 'optionsok'.");

  out("extendedoptions");
  out("  Non-standard extension: Lists all options with current values, defaults, and domain.");
  out("  Format: 'option name <name> type <type> default <default> current <current> [min <min> max <max>] domain <domain>'");
  out("  Followed by 'optionsok'.");

  out("register");
  out("  Not implemented.");

  out("debug");
  out("  Not implemented.");

  out("help");
  out("  Prints this help.");

  out("quit");
  out("  Exit the engine.");

  out("Examples (enter each on its own line):");
  out("  position startpos");
  out("  go movetime 1000");
  out("  stop");
}

void UciHandler::getOptionsCommand() const {
  send(UciOptions::getInstance()->strWithCurrentValues());
  send("optionsok");
}

void UciHandler::getExtendedOptionsCommand() const {
  send(UciOptions::getInstance()->strExtended());
  send("optionsok");
}

void UciHandler::send(const std::string& toSend) const {
  LOG__INFO(Logger::get().UCI_LOG, ">> {}", toSend);
  *pOutputStream << toSend << std::endl;
}

void UciHandler::sendString(const std::string& anyString) const {
  send(std::format("info string {}", anyString));
}

void UciHandler::sendReadyOk() const {
  send("readyok");
}

void UciHandler::sendResult(const Move bestMove, const Move ponderMove) const {
  send(std::format("bestmove {}{}", bestMove.str(), (ponderMove ? " ponder " + ponderMove.str() : "")));
}

void UciHandler::sendCurrentLine(const VariationStack& moveList) const {
  send(std::format("currline {}", moveList.str()));
}

void UciHandler::sendIterationEndInfo(int depth, int seldepth, const Value value, uint64_t nodes,
                                      uint64_t nps, const milliseconds time, const MoveList& pv) const {
  send(std::format("info depth {} seldepth {} multipv 1 score {} nodes {} nps {} time {} pv {}",
                   depth, seldepth, value.str(), nodes, nps, time.count(), pv.str()));
}

void UciHandler::sendAspirationResearchInfo(int depth, int seldepth, const Value value,
                                            const std::string& boundString, uint64_t nodes, uint64_t nps,
                                            const milliseconds time, const MoveList& pv) const {
  send(std::format("info depth {} seldepth {} multipv 1 score {} {} nodes {} nps {} time {} pv {}",
                   depth, seldepth, value.str(), boundString, nodes, nps, time.count(), pv.str()));
}

void UciHandler::sendCurrentRootMove(const Move currmove, std::size_t movenumber) const {
  send(std::format("info currmove {} currmovenumber {}", currmove.str(),
                   movenumber));
}

void UciHandler::sendSearchUpdate(int depth, int seldepth, uint64_t nodes, uint64_t nps,
                                  const milliseconds time, int hashfull) const {
  send(std::format("info depth {} seldepth {} nodes {} nps {} time {} hashfull {}",
                   depth, seldepth, nodes, nps, time.count(), hashfull));
}

void UciHandler::uciError(const std::string& msg) const {
  LOG__ERROR(Logger::get().UCIHAND_LOG, "{}", msg);
  sendString(msg);
}
