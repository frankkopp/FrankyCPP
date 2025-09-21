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

#ifndef FRANKYCPP_PERFT_H
#define FRANKYCPP_PERFT_H

// included dependencies
#include <string>
#include "types/types.h"

// forward declared dependencies
class Position;
class MoveGenerator;

class Perft {

  uint64_t nodes{};
  uint64_t checkCounter{};
  uint64_t checkMateCounter{};
  uint64_t staleMateCounter{};
  uint64_t captureCounter{};
  uint64_t enpassantCounter{};
  uint64_t castleCounter{};
  uint64_t promotionCounter{};
  std::string fen;
  bool stopFlag{};
  bool fullStats{true};

public:
  Perft();
  explicit Perft(const std::string &f);

  void perft(int maxDepth);
  void perft(int maxDepth, bool onDemand);
  void leaf_node(const Position& position, Move move);
  void perft(int startDepth, int endDepth, bool onDemand);
  void perft_divide(int maxDepth, bool onDemand);

  void stop();

  uint64_t getNodes() const { return nodes; }
  uint64_t getCaptureCounter() const { return captureCounter; }
  uint64_t getEnpassantCounter() const { return enpassantCounter; }
  uint64_t getCheckCounter() const { return checkCounter; }
  uint64_t getCheckMateCounter() const { return checkMateCounter; }
  uint64_t getStaleMateCounter() const { return staleMateCounter; }
  uint64_t getCastleCounter() const { return castleCounter; }
  uint64_t getPromotionCounter() const { return promotionCounter; }
  bool isFullStats() const { return fullStats; }
  void setFullStats(const bool fs) { fullStats = fs; }

private:
  void resetCounter();
  uint64_t miniMax(int depth, Position &position, MoveGenerator *pMg);
  uint64_t miniMaxOD(int depth, Position &position, MoveGenerator *pMg);
};


#endif //FRANKYCPP_PERFT_H
