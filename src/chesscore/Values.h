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

#ifndef FRANKYCPP_EVALUATION_H
#define FRANKYCPP_EVALUATION_H

#include "types/types.h"
#include <array>

namespace Values {

  /// Tables are upright for easier reading - will be transposed in compile-time initialization

  // clang-format off
  // PAWN Tables
  constexpr int pawnsMidGame[SQ_LENGTH] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  5,  5,  5,  5,  5,  5,  0,
    5,  5, 10, 30, 30, 10,  5,  5,
    0,  0,  0, 30, 30,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-30,-30, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0
  };
  constexpr int pawnsEndGame[SQ_LENGTH] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    90, 90, 90, 90, 90, 90, 90, 90,
    40, 50, 50, 60, 60, 50, 50, 40,
    20, 30, 30, 40, 40, 30, 30, 20,
    10, 10, 20, 20, 20, 10, 10, 10,
     5, 10, 10, 10, 10, 10, 10,  5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
  };

  // KNIGHT Tables
  // Tweaked for Tier 0: slightly stronger rim penalties, clearer central peaks,
  // and a small 7th-rank bonus (for both sides via mirroring) baked into PSQT.
  constexpr int knightMidGame[SQ_LENGTH] = {
    -55,-45,-35,-35,-35,-35,-45,-55,
    -35,-10,  8, 12, 12,  8,-10,-35, // 7th rank bonus vs. symmetric row
    -30,  0, 12, 18, 18, 12,  0,-30,
    -30,  5, 18, 22, 22, 18,  5,-30,
    -30,  0, 18, 22, 22, 18,  0,-30,
    -30,  5, 12, 18, 18, 12,  5,-30,
    -40,-15,  5, 10, 10,  5,-15,-40,
    -55,-45,-35,-35,-35,-35,-45,-55,
  };
  constexpr int knightEndGame[SQ_LENGTH] = {
    -55,-45,-35,-35,-35,-35,-45,-55,
    -35,-15,  8,  8,  8,  8,-15,-35, // 7th rank bonus vs. symmetric row
    -30,  0, 12, 18, 18, 12,  0,-30,
    -30,  0, 18, 22, 22, 18,  0,-30,
    -30,  0, 18, 22, 22, 18,  0,-30,
    -30,  0, 12, 18, 18, 12,  0,-30,
    -40,-20,  5,  5,  5,  5,-20,-40,
    -55,-45,-35,-35,-35,-35,-45,-55,
  };

  // BISHOP Tables
  constexpr int bishopMidGame[SQ_LENGTH] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-40,-10,-10,-40,-10,-20,
  };
  constexpr int bishopEndGame[SQ_LENGTH] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
  };

  // ROOK Tables
  constexpr int rookMidGame [SQ_LENGTH] = {
    5,  5,  5,  5,  5,  5,  5,  5,
    10, 10, 10, 10, 10, 10, 10, 10,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    -15,-10, 15, 15, 15, 15,-10,-15,
  };
  constexpr int rookEndGame [SQ_LENGTH] = {
    5,  5,  5,  5,  5,  5,  5,  5,
    12, 12, 12, 12, 12, 12, 12, 12,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
  };

  // Queen Tables
  constexpr int queenMidGame[SQ_LENGTH] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
     -5,  0,  2,  2,  2,  2,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
  };
  constexpr int queenEndGame[SQ_LENGTH] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
  };

  // King Tables
  constexpr int kingMidGame [SQ_LENGTH] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-30,-30,-30,-20,-10,
      0,  0,-20,-20,-20,-20,  0,  0,
     20, 50,  0,-20,-20,  0, 50, 20
  };
  constexpr int kingEndGame [SQ_LENGTH] = {
    -50,-30,-30,-20,-20,-30,-30,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50
  };
  // clang-format on

  // Helper to compute mirrored/blended PSQT values
  constexpr int calcPosValueWhite(const Square sq, const int gamePhase, const int posMidTable[], const int posEndTable[]) {
    return (gamePhase * posMidTable[63 - sq] + (GAME_PHASE_MAX - gamePhase) * posEndTable[63 - sq]) / GAME_PHASE_MAX;
  }
  // Helper to compute mirrored/blended PSQT values
  constexpr int calcPosValueBlack(const Square sq, const int gamePhase, const int posMidTable[], const int posEndTable[]) {
    return (gamePhase * posMidTable[sq] + (GAME_PHASE_MAX - gamePhase) * posEndTable[sq]) / GAME_PHASE_MAX;
  }

  // Compute per-piece per-square midgame value
  constexpr Value computePosMid(const Piece pc, const Square sq) {
    switch (pc) {
      case WHITE_KING:   return static_cast<Value>(kingMidGame[63 - sq]);
      case WHITE_PAWN:   return static_cast<Value>(pawnsMidGame[63 - sq]);
      case WHITE_KNIGHT: return static_cast<Value>(knightMidGame[63 - sq]);
      case WHITE_BISHOP: return static_cast<Value>(bishopMidGame[63 - sq]);
      case WHITE_ROOK:   return static_cast<Value>(rookMidGame[63 - sq]);
      case WHITE_QUEEN:  return static_cast<Value>(queenMidGame[63 - sq]);
      case BLACK_KING:   return static_cast<Value>(kingMidGame[sq]);
      case BLACK_PAWN:   return static_cast<Value>(pawnsMidGame[sq]);
      case BLACK_KNIGHT: return static_cast<Value>(knightMidGame[sq]);
      case BLACK_BISHOP: return static_cast<Value>(bishopMidGame[sq]);
      case BLACK_ROOK:   return static_cast<Value>(rookMidGame[sq]);
      case BLACK_QUEEN:  return static_cast<Value>(queenMidGame[sq]);
      case PIECE_NONE:
      case PIECE_LENGTH:
      default: return VALUE_ZERO;
    }
  }

  // Compute per-piece per-square endgame value
  constexpr Value computePosEnd(const Piece pc, const Square sq) {
    switch (pc) {
      case WHITE_KING:   return static_cast<Value>(kingEndGame[63 - sq]);
      case WHITE_PAWN:   return static_cast<Value>(pawnsEndGame[63 - sq]);
      case WHITE_KNIGHT: return static_cast<Value>(knightEndGame[63 - sq]);
      case WHITE_BISHOP: return static_cast<Value>(bishopEndGame[63 - sq]);
      case WHITE_ROOK:   return static_cast<Value>(rookEndGame[63 - sq]);
      case WHITE_QUEEN:  return static_cast<Value>(queenEndGame[63 - sq]);
      case BLACK_KING:   return static_cast<Value>(kingEndGame[sq]);
      case BLACK_PAWN:   return static_cast<Value>(pawnsEndGame[sq]);
      case BLACK_KNIGHT: return static_cast<Value>(knightEndGame[sq]);
      case BLACK_BISHOP: return static_cast<Value>(bishopEndGame[sq]);
      case BLACK_ROOK:   return static_cast<Value>(rookEndGame[sq]);
      case BLACK_QUEEN:  return static_cast<Value>(queenEndGame[sq]);
      case PIECE_NONE:
      case PIECE_LENGTH:
      default: return VALUE_ZERO;
    }
  }

  // Compute per-piece per-square blended value for a given game phase
  constexpr Value computePosBlend(const Piece pc, const Square sq, const int gp) {
    switch (pc) {
      case WHITE_KING:   return static_cast<Value>(calcPosValueWhite(sq, gp, kingMidGame, kingEndGame));
      case WHITE_PAWN:   return static_cast<Value>(calcPosValueWhite(sq, gp, pawnsMidGame, pawnsEndGame));
      case WHITE_KNIGHT: return static_cast<Value>(calcPosValueWhite(sq, gp, knightMidGame, knightEndGame));
      case WHITE_BISHOP: return static_cast<Value>(calcPosValueWhite(sq, gp, bishopMidGame, bishopEndGame));
      case WHITE_ROOK:   return static_cast<Value>(calcPosValueWhite(sq, gp, rookMidGame, rookEndGame));
      case WHITE_QUEEN:  return static_cast<Value>(calcPosValueWhite(sq, gp, queenMidGame, queenEndGame));
      case BLACK_KING:   return static_cast<Value>(calcPosValueBlack(sq, gp, kingMidGame, kingEndGame));
      case BLACK_PAWN:   return static_cast<Value>(calcPosValueBlack(sq, gp, pawnsMidGame, pawnsEndGame));
      case BLACK_KNIGHT: return static_cast<Value>(calcPosValueBlack(sq, gp, knightMidGame, knightEndGame));
      case BLACK_BISHOP: return static_cast<Value>(calcPosValueBlack(sq, gp, bishopMidGame, bishopEndGame));
      case BLACK_ROOK:   return static_cast<Value>(calcPosValueBlack(sq, gp, rookMidGame, rookEndGame));
      case BLACK_QUEEN:  return static_cast<Value>(calcPosValueBlack(sq, gp, queenMidGame, queenEndGame));
      case PIECE_NONE:
      case PIECE_LENGTH:
      default: return VALUE_ZERO;
    }
  }

  // Holds precomputed posMidValue[Piece][Square] for all pieces and squares
  // Used to quickly update position value when a piece is added or removed
  // during move making and unmaking
  inline constexpr std::array<std::array<Value, SQ_LENGTH>, PIECE_LENGTH> posMidValue = [] {
    std::array<std::array<Value, SQ_LENGTH>, PIECE_LENGTH> arr{};
    for (int pc = 0; pc < PIECE_LENGTH; ++pc) {
      for (int sq = 0; sq < SQ_LENGTH; ++sq) {
        arr[pc][sq] = computePosMid(static_cast<Piece>(pc), static_cast<Square>(sq));
      }
    }
    return arr;
  }();

  // Holds precomputed posEndValue[Piece][Square] for all pieces and squares
  // Used to quickly update position value when a piece is added or removed
  // during move making and unmaking
  inline constexpr std::array<std::array<Value, SQ_LENGTH>, PIECE_LENGTH> posEndValue = [] {
    std::array<std::array<Value, SQ_LENGTH>, PIECE_LENGTH> arr{};
    for (int pc = 0; pc < PIECE_LENGTH; ++pc) {
      for (int sq = 0; sq < SQ_LENGTH; ++sq) {
        arr[pc][sq] = computePosEnd(static_cast<Piece>(pc), static_cast<Square>(sq));
      }
    }
    return arr;
  }();

  // Holds precomputed posValue[Piece][Square][GamePhase] for all pieces, squares and game phases
  // Used to quickly get the blended position value during evaluation
  // without having to do the blending calculation each time
  inline constexpr std::array<std::array<std::array<Value, GAME_PHASE_MAX + 1>, SQ_LENGTH>, PIECE_LENGTH> posValue = [] {
    std::array<std::array<std::array<Value, GAME_PHASE_MAX + 1>, SQ_LENGTH>, PIECE_LENGTH> arr{};
    for (int pc = 0; pc < PIECE_LENGTH; ++pc) {
      for (int sq = 0; sq < SQ_LENGTH; ++sq) {
        for (int gp = 0; gp <= GAME_PHASE_MAX; ++gp) {
          arr[pc][sq][gp] = computePosBlend(static_cast<Piece>(pc), static_cast<Square>(sq), gp);
        }
      }
    }
    return arr;
  }();

} // namespace Values

#endif //FRANKYCPP_EVALUATION_H
