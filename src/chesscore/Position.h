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

#ifndef FRANKYCPP_POSITION_H
#define FRANKYCPP_POSITION_H

#include <array>
#include <cstdint>

#include "types/types.h"
#include "gtest/gtest_prod.h"

namespace Zobrist {
  // zobrist key for pieces - piece, board
  extern Key pieces[PIECE_LENGTH][SQ_LENGTH];
  extern Key castlingRights[CR_LENGTH];
  extern Key enPassantFile[FILE_LENGTH];
  extern Key nextPlayer;
}// namespace Zobrist

// Flag for boolean states with undetermined state
enum Flag {
  FLAG_TBD,
  FLAG_FALSE,
  FLAG_TRUE
};

// HistoryState encapsulates a position state to  enable undo move. It
// acts as a stack by adding a position state after doMove and removing
// a state after undoMove.
struct HistoryState {
  Key zobristKey                = 0;
  Key pawnKey                   = 0;
  Move move                     = MOVE_NONE;
  Piece fromPiece               = PIECE_NONE;
  Piece capturedPiece           = PIECE_NONE;
  CastlingRights castlingRights = NO_CASTLING;
  Square enPassantSquare        = SQ_NONE;
  int halfMoveClock             = 0;
  Flag hasCheckFlag             = FLAG_TBD;
};

// This class represents the chess board and its position.<br>
// It uses a 8x8 piece board and bitboards, a stack for undo moves, zobrist keys
// for transposition tables, piece lists, material and positional value counter.
//
// Can be created with any FEN notation and as a copy from another Position.
class Position {

  // flag to indicate whether the class has been initialized
  static bool initialized;

  // The zobrist key to use as a hash key in transposition tables
  // The zobrist key will be updated incrementally every time one of the the
  // state variables change.
  Key zobristKey{};

  // We also maintain a zobrist key for all pawns to support a pawn
  // evaluation table
  Key pawnKey{};

  // **********************************************************
  // Board State
  // unique chess position (exception is 3-fold repetition
  // which is also not represented in a FEN string)

  Piece board[SQ_LENGTH]{};
  CastlingRights castlingRights{};
  Square enPassantSquare = SQ_NONE;
  int halfMoveClock      = 0;
  Color nextPlayer       = WHITE;
  int moveNumber         = 1;

  // Board State END ------------------------------------------
  // **********************************************************

  // **********************************************************
  // Extended Board State -------------------------------------
  // not necessary for a unique position

  // special for king squares
  Square kingSquare[COLOR_LENGTH]{};

  // piece bitboards
  Bitboard piecesBb[COLOR_LENGTH][PT_LENGTH]{};

  // occupied bitboard
  Bitboard occupiedBb[COLOR_LENGTH]{};

  // Extended Board State END ---------------------------------
  // **********************************************************

  // history information for undo and repetition detection
  constexpr static std::size_t MAX_HISTORY = MAX_MOVES;
  std::array<HistoryState, MAX_HISTORY> historyState{};
  int historyCounter                       = 0;

  // Calculated by doMove/undoMove

  // Material value will always be up to date
  int material[COLOR_LENGTH]{};
  int materialNonPawn[COLOR_LENGTH]{};

  // Positional value will always be up to date
  int psqMidValue[COLOR_LENGTH]{};
  int psqEndValue[COLOR_LENGTH]{};

  // Game phase value
  int gamePhase{};

  // caches a hasCheck and hasMate Flag for the current position. Will be set
  // after a call to hasCheck() and reset to TBD every time a move is made or
  // unmade.
  mutable Flag hasCheckFlag = FLAG_TBD;

public:
  // Initialize static Position class
  static void init();

  // Creates a standard board position and initializes it with standard chess setup.
  Position();

  // Creates a standard board position and initializes it with a fen position
  explicit Position(const char* fen);

  // Creates a standard board position and initializes it with a fen position
  explicit Position(const std::string& fen);

  // Copy constructor - creates a copy of the given Position
  Position(const Position& op) = default;

  // Copy assignment operator
  Position& operator=(const Position& other) = default;

  // Move constructor
  Position(Position&& other) = default;

  // Move assignment operator
  Position& operator=(Position&& other) = default;

  // Destructor
  ~Position() = default;

  // Returns a String representation of the chess position of this Position as
  // a FEN String.
  friend std::ostream& operator<<(std::ostream& os, Position& position);

  // return string showing the position as 8x8 matrix with additional
  // information about the object's state
  std::string str() const;


  // return string showing the position as a 8x8 matrix
  std::string strBoard() const;

  // Returns a String representation of the chess position of this Position as
  // a FEN String.
  std::string strFen() const;

  // DoMove commits a move to the board. Due to performance there is no check if this
  // move is legal on the current position. Legal check needs to be done
  // beforehand or after in case of pseudo legal moves. Usually the move will be
  // generated by a MoveGenerator and therefore the move will be assumed legal anyway.
  void doMove(Move move);

  // UndoMove resets the position to a state before the last move has been made
  // The history entry will be changed but the history counter reset. So in effect
  // the external view on the position is unchanged (e.g. fenBeforeDoMove == fenAfterUndoMove
  // and zobristBeforeDoMove == zobristAfterUndoMove but positionBeforeDoMove != positionAfterUndoMove
  // If positionBeforeDoMove == positionAfterUndoMove would be required this function would have
  // to be changed to reset the history entry as well. Currently this is not necessary
  // and therefore we spare the time to do this.
  void undoMove();

  // DoNullMove is used in Null Move Pruning. The position is basically unchanged but
  // the next player changes. The state before the null move will be stored to
  // history.
  // The history entry will be changed. So in effect after an UndoNullMove()
  // the external view on the position is unchanged (e.g. fenBeforeNull == fenAfterNull
  // and zobristBeforeNull == zobristAfterNull but positionBeforeNull != positionAfterNull.
  void doNullMove();

  // UndoNullMove restores the state of the position to before the DoNullMove() call.
  // The history entry will be changed but the history counter reset. So in effect
  // the external view on the position is unchanged (e.g. fenBeforeNull == fenAfterNull
  // and zobristBeforeNull == zobristAfterNull but positionBeforeNull != positionAfterNull
  // If positionBeforeNull != positionAfterNull would be required this function would have
  // to be changed to reset the history entry as well. Currently this is not necessary
  // and therefore we spare the time to do this.
  void undoNullMove();

  // This checks if a certain square is currently under attack by the player
  // of the given color. It does not matter who has the next move on this
  // position. It also is not checking if the actual attack can be done as a
  // legal move. E.g. a pinned piece could not actually make a capture on the
  // square.
  bool isAttacked(Square sq, Color by) const;

  // AttacksTo determines all attacks to the given square for the given color.
  Bitboard attacksTo(Square square, Color color) const;

  // HasCheck returns true if the next player is threatened by a check
  // (king is attacked).
  // This is cached for the current position. Multiple calls to this
  // on the same position are therefore very efficient.
  bool hasCheck() const;

  // Checks if move is giving check to the opponent.
  // This method is faster than making the move and checking for legality and
  // giving check. Needs to be a valid move for the position otherwise will
  // crash. For performance reason we do not want to check validity here. Does
  // NOT check if the move itself is legal (leaves the own king in check)
  bool givesCheck(Move move) const;

  // WasLegalMove tests if the last move was legal. Basically tests if
  // the king is now in check or if the king crossed an attacked square
  // during castling or if there was a castling although in check.
  // If the position does not have a last move (history empty) this
  // will only check if the king of the opponent is attacked e.g. could
  // now be captured by the next player.
  bool wasLegalMove() const;

  // IsLegalMove tests a move if it is legal on the current position.
  // Basically tests if the king would be left in check after the move
  // or if the king crosses an attacked square during castling.
  bool isLegalMove(Move move) const;

  // CheckRepetitions
  // Repetition of a position:.
  // To detect a 3-fold repetition the given position must occur at least 2
  // times before:<br/> <code>position.checkRepetitions(2)</code> checks for 3
  // fold-repetition <p> 3-fold repetition: This most commonly occurs when
  // neither side is able to avoid repeating moves without incurring a
  // disadvantage. The three occurrences of the position need not occur on
  // consecutive moves for a claim to be valid. FIDE rules make no mention of
  // perpetual check; this is merely a specific type of draw by threefold
  // repetition.
  //
  // Return true if this position has been played reps times before
  bool checkRepetitions(int reps) const;
  int countRepetitions() const;


  // HasInsufficientMaterial returns true if no side has enough material to
  // force a mate (does not exclude combination where a helpmate would be
  // possible, e.g. the opponent needs to support a mate by mistake)
  bool checkInsufficientMaterial() const;

  // Returns the last move. Returns Move.NOMOVE if there is no last move.
  inline Move getLastMove() const {
    if (historyCounter <= 0) return MOVE_NONE;
    return historyState[historyCounter - 1].move;
  };

  /**
   * Determines if a move on this position is a capturing move
   * @param move
   * @return true if move captures (incl. en passant)
   */
  bool isCapturingMove(const Move& move) const {
    return (occupiedBb[~nextPlayer] & toSquare(move)) || typeOf(move) == ENPASSANT;
  };

  // LastCapturedPiece returns the captured piece of the the last
  // move made on the position or MoveNone if the move was
  // non-capturing or the position has no history of earlier moves.
  // Does not return a pawn captured by en passant.
  inline Piece getLastCapturedPiece() const {
    if (historyCounter <= 0) return PIECE_NONE;
    return historyState[historyCounter - 1].capturedPiece;
  };

private:
  FRIEND_TEST(PositionTest, initialization);
  FRIEND_TEST(PositionTest, HistoryStruct);
  FRIEND_TEST(PositionTest, PosValue);

  // initialization of board data structure to an empty board
  void initializeBoard();
  void setupBoard(const char* fen);
  void movePiece(Square fromSq, Square toSq);
  void putPiece(Piece piece, Square square);
  Piece removePiece(Square square);
  void clearEnPassant();

public:
  ////////////////////////////////////////////////
  ///// GETTER / SETTER

  inline Piece getPiece(const Square square) const { return board[square]; }
  inline Key getZobristKey() const { return zobristKey; }
  inline Key getPawnZobristKey() const { return pawnKey; }
  inline Color getNextPlayer() const { return nextPlayer; }
  inline Square getEnPassantSquare() const { return enPassantSquare; }
  inline Square getKingSquare(const Color color) const { return kingSquare[color]; };
  inline Bitboard getPieceBb(const Color c, const PieceType pt) const { return piecesBb[c][pt]; }
  inline Bitboard getOccupiedBb() const { return occupiedBb[WHITE] | occupiedBb[BLACK]; }
  inline Bitboard getOccupiedBb(const Color c) const { return occupiedBb[c]; }
  inline int getMaterial(const Color c) const { return material[c]; }
  inline int getMaterialNonPawn(const Color c) const { return materialNonPawn[c]; }
  inline int getMidPosValue(const Color c) const { return psqMidValue[c]; }
  inline int getEndPosValue(const Color c) const { return psqEndValue[c]; }
  inline int getPosValue(const Color c) const {
    return static_cast<int>(getGamePhaseFactor() * psqMidValue[c] +
                            (1 - getGamePhaseFactor()) * psqEndValue[c]);
  }
  inline CastlingRights getCastlingRights() const { return castlingRights; }
  inline int getHalfMoveClock() const { return halfMoveClock; }
  inline int getMoveNumber() const { return moveNumber; }
  // 24 for beginning, 0 at the end
  inline int getGamePhase() const { return gamePhase; }
  // 1.0 for beginning to 0.0 t the end)
  inline double getGamePhaseFactor() const { return double(gamePhase) / GAME_PHASE_MAX; }
};


#endif//FRANKYCPP_POSITION_H
