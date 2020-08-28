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

#include <chrono>
#include <iomanip>
#include <iostream>

#include "Perft.h"
#include "chesscore/MoveGenerator.h"
#include "chesscore/Position.h"

Perft::Perft() {
  fen = START_POSITION_FEN;
}

Perft::Perft(const std::string& f) {
  fen = f;
}

void Perft::perft(int maxDepth) {
  perft(maxDepth, false);
}

void Perft::stop() {
  stopFlag = true;
}

void Perft::perft(int startDepth, int endDepth, bool onDemand) {
  stopFlag = false;
  for (int depth = startDepth; depth <= endDepth; ++depth) {
    if (stopFlag) {
      std::cout << "Perft stopped.";
      return;
    }
    perft(depth, onDemand);
  }
}

void Perft::perft(int maxDepth, bool onDemand) {
  stopFlag = false;
  resetCounter();

  Position position;
  try {
    position = Position(fen);
  } catch (std::invalid_argument& e) {
    std::cerr << fmt::format("Fen for perft invalid: {}", e.what()) << std::endl;
    return;
  }
  MoveGenerator mg[MAX_DEPTH];
  std::ostringstream os;
  std::cout.imbue(deLocale);
  os.imbue(deLocale);
  os << std::setprecision(9);

  os << "Performing PERFT Test for Depth " << maxDepth << std::endl;
  os << "FEN: " << fen << std::endl;
  os << "-----------------------------------------" << std::endl;

  std::cout << os.str();
  std::cout.flush();
  os.str("");
  os.clear();

  uint64_t result;
  auto start = std::chrono::high_resolution_clock::now();

  if (onDemand) {
    result = miniMaxOD(maxDepth, position, mg);
  }
  else {
    result = miniMax(maxDepth, position, mg);
  }

  if (stopFlag) {
    std::cout << "Perft stopped.";
    return;
  }

  auto finish       = std::chrono::high_resolution_clock::now();
  uint64_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

  nodes = result;

  os << "Time         : " << duration << " ms" << std::endl;
  os << "NPS          : " << (result * 1'000) / (duration + 1) << " nps" << std::endl;
  os << "Results:" << std::endl;
  os << "   Nodes     : " << nodes << std::endl;
  os << "   Captures  : " << captureCounter << std::endl;
  os << "   EnPassant : " << enpassantCounter << std::endl;
  os << "   Checks    : " << checkCounter << std::endl;
  os << "   CheckMates: " << checkMateCounter << std::endl;
  os << "   Castles   : " << castleCounter << std::endl;
  os << "   Promotions: " << promotionCounter << std::endl;
  os << "-----------------------------------------" << std::endl;
  os << "Finished PERFT Test for Depth " << maxDepth << std::endl;

  std::cout << os.str() << std::endl;
}

uint64_t Perft::miniMaxOD(int depth, Position& position, MoveGenerator* pMg) {
  pMg[depth].reset();

  // Iterate over moves
  uint64_t totalNodes = 0;

  // moves to search recursively
  Move move;
  while (!stopFlag) {
    move = pMg[depth].getNextPseudoLegalMove(position, GenAll);
    if (move == MOVE_NONE) break;
    //    fprintln("Last: {:5s} Move: {:5s}   Fen: {:s} ", str(position.getLastMove()), str(move), position.strFen());
    if (depth > 1) {
      position.doMove(move);
      if (position.wasLegalMove()) {
        totalNodes += miniMaxOD(depth - 1, position, pMg);
      }
      position.undoMove();
    }
    else {
      position.doMove(move);
      if (position.wasLegalMove()) {
        totalNodes++;
        // enpassant
        if (typeOf(move) == ENPASSANT) {
          enpassantCounter++;
          captureCounter++;
        }
        // castling
        else if (typeOf(move) == CASTLING) {
          castleCounter++;
//          fprintln("No: {:2d} Last: {:5s} Move: {:5s}   Fen: {:s} ", castleCounter, str(position.getLastMove()), str(move), position.strFen());
        }
        else if (typeOf(move) == PROMOTION) {
          promotionCounter++;
        }
        // capture
        if (position.getLastCapturedPiece() != PIECE_NONE) {
          captureCounter++;
        }
        // check
        if (position.hasCheck()) {
          checkCounter++;
          //  mate
          if (!MoveGenerator::hasLegalMove(position)) {
            checkMateCounter++;
          }
        }
      }
      position.undoMove();
    }
  }
  return totalNodes;
}


uint64_t Perft::miniMax(int depth, Position& position, MoveGenerator* pMg) {

  // Iterate over moves
  uint64_t totalNodes = 0;

  //println(pPosition->str())

  // moves to search recursively
  MoveList moves = *pMg[depth].generatePseudoLegalMoves(position, GenAll);
  for (Move move : moves) {
    if (stopFlag) {
      return 0;
    }
    if (depth > 1) {
      position.doMove(move);
      if (position.wasLegalMove()) {
        totalNodes += miniMaxOD(depth - 1, position, pMg);
      }
      position.undoMove();
    }
    else {
      position.doMove(move);
      if (position.wasLegalMove()) {
        totalNodes++;
        // enpassant
        if (typeOf(move) == ENPASSANT) {
          enpassantCounter++;
          captureCounter++;
        }
        // castling
        else if (typeOf(move) == CASTLING) {
          castleCounter++;
        }
        else if (typeOf(move) == PROMOTION) {
          promotionCounter++;
        }
        // capture
        if (position.getLastCapturedPiece() != PIECE_NONE) {
          captureCounter++;
        }
        // check
        if (position.hasCheck()) {
          checkCounter++;
          //  mate
          if (!MoveGenerator::hasLegalMove(position)) {
            checkMateCounter++;
          }
        }
      }
      position.undoMove();
    }
  }
  return totalNodes;
}

void Perft::perft_divide(int maxDepth, bool onDemand) {
  resetCounter();

  Position position;
  try {
    position = Position(fen);
  } catch (std::invalid_argument& e) {
    std::cerr << fmt::format("Fen for perft invalid: {}", e.what()) << std::endl;
    return;
  }
  MoveGenerator mg[MAX_DEPTH];
  std::ostringstream os;
  std::cout.imbue(deLocale);
  os.imbue(deLocale);
  os << std::setprecision(9);

  os << "Testing at depth " << maxDepth << std::endl;
  std::cout << os.str();
  std::cout.flush();
  os.str("");
  os.clear();

  uint64_t result = 0;
  auto start      = std::chrono::high_resolution_clock::now();

  // moves to search recursively
  MoveList moves = *mg[maxDepth].generatePseudoLegalMoves(position, GenAll);
  for (Move move : moves) {
    if (stopFlag) {
      return;
    }
    //  Move move = createMove<PROMOTION>("c7c8n");
    // Iterate over moves
    uint64_t totalNodes = 0L;

    if (maxDepth > 1) {
      position.doMove(move);
      // only go into recursion if move was legal
      if (position.wasLegalMove()) {
        if (onDemand) { totalNodes = miniMaxOD(maxDepth - 1, position, mg); }
        else {
          totalNodes = miniMax(maxDepth - 1, position, mg);
        }
        result += totalNodes;
      }
      position.undoMove();
    }
    else {
      const bool cap = position.getPiece(toSquare(move)) != PIECE_NONE;
      const bool ep  = typeOf(move) == ENPASSANT;
      position.doMove(move);
      if (position.wasLegalMove()) {
        totalNodes++;
        if (ep) {
          enpassantCounter++;
          captureCounter++;
        }
        if (cap) captureCounter++;
        if (position.hasCheck()) checkCounter++;
        if (!MoveGenerator::hasLegalMove(position)) checkMateCounter++;
        result += totalNodes;
      }
      position.undoMove();
    }

    os << strVerbose(move) << " (" << totalNodes << ")" << std::endl;
    std::cout << os.str();
    std::cout.flush();
    os.str("");
    os.clear();
  }

  auto finish       = std::chrono::high_resolution_clock::now();
  uint64_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

  nodes = result;

  os << "Leaf Nodes: " << nodes
     << " Captures: " << captureCounter
     << " EnPassant: " << enpassantCounter
     << " Checks: " << checkCounter
     << " Mates: " << checkMateCounter
     << std::endl;

  os << "Duration: " << duration << " ms" << std::endl;
  os << "NPS: " << ((result * 1'000) / duration) << " nps" << std::endl;

  std::cout << os.str();
}

void Perft::resetCounter() {
  nodes            = 0;
  checkCounter     = 0;
  checkMateCounter = 0;
  captureCounter   = 0;
  enpassantCounter = 0;
  castleCounter    = 0;
  promotionCounter = 0;
}
