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

#include "Values.h"

namespace Values {

  inline int calcPosValueWhite(const Square& sq, int gamePhase, const int posMidTable[], const int posEndTable[]) {
    return (gamePhase * posMidTable[63 - sq] + (GAME_PHASE_MAX - gamePhase) * posEndTable[63 - sq]) / GAME_PHASE_MAX;
  }

  inline int calcPosValueBlack(const Square& sq, int gamePhase, const int posMidTable[], const int posEndTable[]) {
    return (gamePhase * posMidTable[sq] + (GAME_PHASE_MAX - gamePhase) * posEndTable[sq]) / GAME_PHASE_MAX;
  }

  void init() {
    // pre-compute piece on square values for mid and endgame and also for
    // all game phases
    for (Piece pc = WHITE_KING; pc <= BLACK_QUEEN; ++pc) {
      for (Square sq = SQ_A1; sq <= SQ_H8; ++sq) {
        for (int gp = GAME_PHASE_MAX; gp >= 0; gp--) {
          switch (pc) {
          case WHITE_KING:
            posMidValue[pc][sq]  = Value(kingMidGame[63 - sq]);
            posEndValue[pc][sq]  = Value(kingEndGame[63 - sq]);
            posValue[pc][sq][gp] = Value(calcPosValueWhite(sq, gp, kingMidGame, kingEndGame));
            break;
          case WHITE_PAWN:
            posMidValue[pc][sq]  = Value(pawnsMidGame[63 - sq]);
            posEndValue[pc][sq]  = Value(pawnsEndGame[63 - sq]);
            posValue[pc][sq][gp] = Value(calcPosValueWhite(sq, gp, pawnsMidGame, pawnsEndGame));
            break;
          case WHITE_KNIGHT:
            posMidValue[pc][sq]  = Value(knightMidGame[63 - sq]);
            posEndValue[pc][sq]  = Value(knightEndGame[63 - sq]);
            posValue[pc][sq][gp] = Value(calcPosValueWhite(sq, gp, knightMidGame, knightEndGame));
            break;
          case WHITE_BISHOP:
            posMidValue[pc][sq]  = Value(bishopMidGame[63 - sq]);
            posEndValue[pc][sq]  = Value(bishopEndGame[63 - sq]);
            posValue[pc][sq][gp] = Value(calcPosValueWhite(sq, gp, bishopMidGame, bishopEndGame));
            break;
          case WHITE_ROOK:
            posMidValue[pc][sq]  = Value(rookMidGame[63 - sq]);
            posEndValue[pc][sq]  = Value(rookEndGame[63 - sq]);
            posValue[pc][sq][gp] = Value(calcPosValueWhite(sq, gp, rookMidGame, rookEndGame));
            break;
          case WHITE_QUEEN:
            posMidValue[pc][sq]  = Value(queenMidGame[63 - sq]);
            posEndValue[pc][sq]  = Value(queenEndGame[63 - sq]);
            posValue[pc][sq][gp] = Value(calcPosValueWhite(sq, gp, queenMidGame, queenEndGame));
            break;
          case BLACK_KING:
            posMidValue[pc][sq]  = Value(kingMidGame[sq]);
            posEndValue[pc][sq]  = Value(kingEndGame[sq]);
            posValue[pc][sq][gp] = Value(calcPosValueBlack(sq, gp, kingMidGame, kingEndGame));
            break;
          case BLACK_PAWN:
            posMidValue[pc][sq]  = Value(pawnsMidGame[sq]);
            posEndValue[pc][sq]  = Value(pawnsEndGame[sq]);
            posValue[pc][sq][gp] = Value(calcPosValueBlack(sq, gp, pawnsMidGame, pawnsEndGame));
            break;
          case BLACK_KNIGHT:
            posMidValue[pc][sq]  = Value(knightMidGame[sq]);
            posEndValue[pc][sq]  = Value(knightEndGame[sq]);
            posValue[pc][sq][gp] = Value(calcPosValueBlack(sq, gp, knightMidGame, knightEndGame));
            break;
          case BLACK_BISHOP:
            posMidValue[pc][sq]  = Value(bishopMidGame[sq]);
            posEndValue[pc][sq]  = Value(bishopEndGame[sq]);
            posValue[pc][sq][gp] = Value(calcPosValueBlack(sq, gp, bishopMidGame, bishopEndGame));
            break;
          case BLACK_ROOK:
            posMidValue[pc][sq]  = Value(rookMidGame[sq]);
            posEndValue[pc][sq]  = Value(rookEndGame[sq]);
            posValue[pc][sq][gp] = Value(calcPosValueBlack(sq, gp, rookMidGame, rookEndGame));
            break;
          case BLACK_QUEEN:
            posMidValue[pc][sq]  = Value(queenMidGame[sq]);
            posEndValue[pc][sq]  = Value(queenEndGame[sq]);
            posValue[pc][sq][gp] = Value(calcPosValueBlack(sq, gp, queenMidGame, queenEndGame));
            break;
          case PIECE_NONE:
          case PIECE_LENGTH:
            break;
          }
        }
      }
    }
  }
} // namespace Values
