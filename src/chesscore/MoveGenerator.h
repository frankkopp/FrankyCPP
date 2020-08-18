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

#ifndef FRANKYCPP_MOVEGENERATOR_H
#define FRANKYCPP_MOVEGENERATOR_H


#include "gtest/gtest_prod.h"
#include <types/types.h>

// forward declaration
class Position;
struct History;

// GenMode generation modes for on demand move generation.
//  GenZero     = 0b00
//  GenNonQuiet = 0b01
//  GenQuiet    = 0b10
//  GenAll      = 0b11
enum GenMode : uint8_t {
  GenZero     = 0b00,
  GenNonQuiet = 0b01,
  GenQuiet    = 0b10,
  GenAll      = 0b11
};

// Class MoveGenerator contains functionality to create moves on a
// chess position. It implements several variants like
// generate pseudo legal moves, legal moves or on demand
// generation of pseudo legal moves.
// A MoveGenerator is depended on the ply as it might store
// pv moves and killer moves which are bound to a ply.
// Usually one MoveGenerator will be pre generated for each
// possible ply to by used in.
class MoveGenerator {
  // internal move lists to not allocate memory during move generation
  MoveList pseudoLegalMoves = MoveList();
  MoveList legalMoves       = MoveList();
  MoveList onDemandMoves    = MoveList();

  // States for the on demand move generator
  Key currentODZobrist            = 0;
  Bitboard onDemandEvasionTargets = BbZero;
  std::size_t takeIndex           = 0;
  enum onDemandStage { OD_NEW,
                       PV,
                       OD1,
                       OD2,
                       OD3,
                       OD4,
                       OD5,
                       OD6,
                       OD7,
                       OD8,
                       OD_END };
  onDemandStage currentODStage;

  Move pvMove          = MOVE_NONE;
  bool pvMovePushed    = false;
  Move killerMoves[2]  = {MOVE_NONE, MOVE_NONE};
  History* historyData = nullptr;

public:
  // MoveGenerator() creates a new instance of a move generator
  // This is the only time when we allocate new memory. The instance
  // will not create any move lists during normal move generation
  // as it will reuse pre-created internal lists which will
  // be returned via pointer to a caller.
  // OBS: Be careful when trying to store the list of generated
  // moves as the underlying list will be changed when move gen
  // is called again. A deep copy is necessary if you need a
  // copy of the move list.
  MoveGenerator();

  ~MoveGenerator() = default;

  // GeneratePseudoLegalMoves generates pseudo moves for the next player. Does not check if
  // king is left in check or if it passes an attacked square when castling or has been in check
  // before castling.
  //
  // If a PV move is set with setPV(Move pv) this move will be returned first and will
  // not be returned at its normal place.
  //
  // Killer moves will be played as soon as possible after non quiet moves. As Killer moves
  // are stored for the whole ply a Killer move might not be valid for the current position.
  // Therefore we need to wait until they are generated. Killer moves will then be pushed
  // to the top of the the quiet moves.
  //
  // Evasion is a parameter given when the position is in check and only evasion moves should
  // be generated. For testing purposes this is a parameter but obviously we could determine
  // checks very quickly internally in this function.
  // The idea of evasion is to avoid generating moves which are obviously not getting the
  // king out of check. This may reduce the total number of generated moves but there might
  // still be a few non legal moves. This is the case if considering and calculating all
  // possible scenarios is more expensive than to just generate the move and dismiss it later.
  // Because of beta cuts off we quite often will never have to check the full legality
  // of these moves anyway.
  const MoveList* generatePseudoLegalMoves(const Position& position, GenMode genMode, bool evasion = false);

  // GenerateLegalMoves generates legal moves for the next player.
  // Uses GeneratePseudoLegalMoves and filters out illegal moves.
  // Usually only used for root moves generation as this is expensive. During
  // the AlphaBeta search we will only use pseudo legal move generation.
  // Other than generatePseudoLegalMoves this determines check and evasion itself.
  const MoveList* generateLegalMoves(const Position& p, GenMode genMode);

  // GetNextMove is the main function for phased generation of pseudo legal moves.
  // It returns the next move for the given position and will usually be called in a
  // loop during search. As we hope for an early beta cut this will save time as not
  // all moves will have been generated.
  //
  // To reuse this on the same position a call to ResetOnDemand() is necessary. This
  // is not necessary when a different position is called as this func will reset it self
  // in this case.
  //
  // If a PV move is set with setPV(Move pv) this will be returned first
  // and will not be returned at its normal place.
  //
  // Killer moves will be played as soon as possible. As Killer moves are stored for
  // the whole ply a Killer move might not be valid for the current position. Therefore
  // we need to wait until they are generated by the phased move generation. Killers will
  // then be pushed to the top of the list of the generation stage.
  //
  // Evasion is a parameter given when the position is in check and only evasion moves should
  // be generated. For testing purposes this is a parameter for now but obviously we could
  // determine checks very quickly internally in this function.
  // The idea of evasion is to avoid generating moves which are obviously not getting the
  // king out of check. This may reduce the total number of generated moves but there might
  // still be a few non legal moves. This is the case if considering and calculating all
  // possible scenarios is more expensive than to just generate the move and dismiss it later.
  // Because of beta cuts off we quite often will never have to check the full legality
  // of these moves anyway.
  Move getNextPseudoLegalMove(const Position& p, GenMode genMode, bool evasion = false);

  // Resets the move generator to start fresh. Clears all lists (e.g. killers) and resets on demand iterator
  inline void reset() {
    pseudoLegalMoves.clear();
    legalMoves.clear();
    killerMoves[0] = MOVE_NONE;
    killerMoves[1] = MOVE_NONE;
    resetOnDemand();
  }

  // ResetOnDemand resets the move on demand generator to start fresh.
  // Also deletes PV moves.
  inline void resetOnDemand() {
    onDemandMoves.clear();
    onDemandEvasionTargets = BbZero;
    currentODStage         = OD_NEW;
    currentODZobrist       = 0;
    pvMove                 = MOVE_NONE;
    pvMovePushed           = false;
    takeIndex              = 0;
  }

  // SetPvMove sets a PV move which should be returned first by
  // the OnDemand MoveGenerator.
  void setPV(Move move);

  // StoreKiller provides the on demand move generator with a new killer move
  // which should be returned as soon as possible when generating moves with
  // the on demand generator.
  void storeKiller(Move killerMove);

  // SetHistoryData provides a pointer to the search's history data
  // for the move generator so it can optimize sorting.
  void setHistoryData(History* pHistory);

  // HasLegalMove determines if we have at least one legal move. We only have to find
  // one legal move. We search for any KING, PAWN, KNIGHT, BISHOP, ROOK, QUEEN move
  // and return immediately if we found one.
  // The order of our search is approx from the most likely to the least likely.
  static bool hasLegalMove(const Position& position);

  // GetMoveFromUci Generates all legal moves and matches the given UCI
  // move string against them. If there is a match the actual move is returned.
  // Otherwise MoveNone is returned.
  //
  // Uses getNextPseudoLegalMove which needs to be reset before use on same position.
  //
  // As this uses string creation and comparison this is not very efficient.
  // Use only when performance is not critical.
  Move getMoveFromUci(const Position& position, const std::string& uciMove);

  // GetMoveFromSan Generates all legal moves and matches the given SAN
  // move string against them. If there is a match the actual move is returned.
  // Otherwise MoveNone is returned.
  //
  // As this uses string creation and comparison this is not very efficient.
  // Use only when performance is not critical.
  Move getMoveFromSan(const Position& position, const std::string& sanMove);

  // ValidateMove validates if a move is a valid legal move on the given position
  bool validateMove(const Position& position, Move move);

  // str() returns a string representation of a MoveGen instance
  std::string str();

  // returns the current pv move
  [[nodiscard]] Move getPvMove() const {
    return pvMove;
  }

  // returns a pointer to the current killer move list
  [[nodiscard]] const Move* getKillerMoves() const {
    return killerMoves;
  }

private:
  // Fills on demand move list by generating moves according to phase
  void fillOnDemandMoveList(const Position& position, GenMode genMode, bool evasion);

  // Move order heuristics based on history data.
  void updateSortValues(const Position& position, MoveList* moveList);

  // getEvasionTargets returns the number of attackers and a Bitboard with target
  // squares for generated moves when the position has check against the next
  // player. Most of the moves will not even be generated as they will not
  // have these target squares. These target squares cover the attacking
  // (checker) piece and any squares in between the attacker and the king
  // in case of the attacker being a slider.
  // If we have more than one attacker we can skip everything apart from
  // king moves.
  static Bitboard getEvasionTargets(const Position& position);

  // Generates pseudo pawn moves for the next player. Does not check if king is left in check
  // @param genMode
  // @param pPosition
  // @param pMoves - generated moves will be added to this list
  static void generatePawnMoves(const Position& position, MoveList* pMoves, GenMode genMode, bool evasion, Bitboard evasionTargets);

  // Generates pseudo knight, bishop, rook and queen moves for the next player.
  // Does not check if king is left in check
  // @param genMode
  // @param pPosition
  // @param pMoves - generated moves will be added to this list
  void generateMoves(const Position& position, MoveList* pMoves, GenMode genMode, bool evasion, Bitboard evasionTargets);

  // Generates pseudo king moves for the next player. Does not check if king
  // lands on an attacked square.
  // @param genMode
  // @param pPosition
  // @param pMoves - generated moves will be added to this list
  void generateKingMoves(const Position& position, MoveList* pMoves, GenMode genMode, bool evasion);

  // Generates pseudo castling move for the next player. Does not check if king passes or lands on an
  // attacked square.
  // @param genMode
  // @param pPosition
  // @param pMoves - generated moves will be added to this list
  void generateCastling(const Position& position, MoveList* pMoves, GenMode genMode);

  FRIEND_TEST(MoveGenTest, pawnMoves);
  FRIEND_TEST(MoveGenTest, kingMoves);
  FRIEND_TEST(MoveGenTest, normalMoves);
  FRIEND_TEST(MoveGenTest, castlingMoves);
  FRIEND_TEST(MoveGenTest, storeKiller);
  FRIEND_TEST(MoveGenTest, sortValueTest);
};

#endif//FRANKYCPP_MOVEGENERATOR_H
