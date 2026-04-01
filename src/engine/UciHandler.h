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

#ifndef FRANKYCPP_UCIHANDLER_H
#define FRANKYCPP_UCIHANDLER_H

#include <cstdint>
#include <iosfwd>
#include <memory>

#include "common/gtest_friends.h"

#include "SearchLimits.h"
#include "chesscore/fwd.h"
#include "types/staticmovelist.h"

// Forward-declare test classes at global scope so FRIEND_TEST inside namespace engine works
FRIEND_TEST_FWD_DECL(UCITest, positionTest);
FRIEND_TEST_FWD_DECL(UCITest, positionFenErrorTest);
FRIEND_TEST_FWD_DECL(UCITest, goCommand);
FRIEND_TEST_FWD_DECL(UCITest, goInfinite);
FRIEND_TEST_FWD_DECL(UCITest, goPonder);
FRIEND_TEST_FWD_DECL(UCITest, goMate);
FRIEND_TEST_FWD_DECL(UCITest, goError);

namespace engine {
  using namespace chess;

  class Search;

  /**
   * Main UCI protocol handler. Communicates through pipe streams with UCI user
   * interfaces or cmd line users. Starts a "listening" loop until command "quit"
   * is received.
   * Executes UCI commands (e.g. starts search, sets position, etc.)
   */
  class UciHandler {
    std::unique_ptr<Position> pPosition;
    std::unique_ptr<MoveGenerator> pMoveGen;
    std::unique_ptr<Perft> pPerft;
    std::shared_ptr<Search> pSearch;

    std::istream* pInputStream;
    std::ostream* pOutputStream;

  public:
    UciHandler();

    // The UciHandler can be provided with stream input and output objects to
    // simulate cin and cout. Useful for testing.
    UciHandler(std::istream* pIstream, std::ostream* pOstream);

    // Starts the handler loop with the istream provided when creating the instance
    void loop();

    // Starts the handler loop  with the given istream (mainly for testing)
    void loop(std::istream* pIstream);

    // send information to the UCI user interface through pipe streams
    void send(const std::string& toSend) const;
    void sendIterationEndInfo(int depth, int seldepth, Value value, uint64_t nodes, uint64_t nps, milliseconds time, const MoveList& pv) const;
    void sendAspirationResearchInfo(int depth, int seldepth, Value value, const std::string& boundString, uint64_t nodes, uint64_t nps, milliseconds time, const MoveList& pv) const;
    void sendCurrentRootMove(Move currmove, std::size_t movenumber) const;
    void sendSearchUpdate(int depth, int seldepth, uint64_t nodes, uint64_t nps, milliseconds time, int hashfull) const;
    void sendCurrentLine(const VariationStack& moveList) const;
    void sendResult(Move bestMove, Move ponderMove) const;
    void sendString(const std::string& anyString) const;
    void sendReadyOk() const;

    [[nodiscard]] const std::shared_ptr<Search>& getSearchPtr() const {
      return pSearch;
    }

  private:
    bool handleCommand(const std::string& cmd);
    void uciCommand() const;
    void isReadyCommand() const;
    void setOptionCommand(std::istringstream& inStream);
    void uciNewGameCommand() const;

    void positionCommand(std::istringstream& inStream);
    FRIEND_TEST_NS(UCITest, positionTest);
    FRIEND_TEST_NS(UCITest, positionFenErrorTest);

    void goCommand(std::istringstream& inStream) const;
    bool readSearchLimits(std::istringstream& inStream, SearchLimits& searchLimits) const;
    FRIEND_TEST_NS(UCITest, goCommand);
    FRIEND_TEST_NS(UCITest, goInfinite);
    FRIEND_TEST_NS(UCITest, goPonder);
    FRIEND_TEST_NS(UCITest, goMate);

    void stopCommand() const;
    void ponderHitCommand() const;
    void perftCommand(std::istringstream& inStream) const;
    void benchCommand(std::istringstream& inStream) const;
    void registerCommand() const;
    void debugCommand() const;
    void helpCommand() const;
    void getOptionsCommand() const;
    void getExtendedOptionsCommand() const;

    void uciError(const std::string& msg) const;
    FRIEND_TEST_NS(UCITest, goError);
  };

} // namespace engine


#endif // FRANKYCPP_UCIHANDLER_H
