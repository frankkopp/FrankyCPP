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

#ifndef FRANKYCPP_POSITION_H
#define FRANKYCPP_POSITION_H

//=============================================================================
// Position.h - Chess Board State Representation
//=============================================================================
//
// Position represents the complete state of a chess position, including
// board, side to move, castling rights, en passant, and move history.
// Depends on: types.h
//
// Board Representation (Hybrid):
//   - 8x8 piece array (Piece board[64]) for fast piece lookup
//   - Bitboards per piece type and color for fast attack/move generation
//   - Piece lists for fast iteration over pieces of a type
//
// State Components:
//   - Piece placement (board array + bitboards)
//   - Side to move (nextPlayer)
//   - Castling rights (4-bit bitmask)
//   - En passant square (if any)
//   - Half-move clock (for 50-move rule)
//   - Full move number
//
// Zobrist Hashing:
//   - Main zobrist key (for TT lookup)
//   - Pawn-specific key (for pawn hash table)
//   - Both updated incrementally on state changes
//
// Move Execution:
//   - doMove(move) executes a move, updating all state
//   - undoMove() restores previous state from history stack
//   - History stack stores all state needed for undo
//
// Incremental Updates:
//   - Material value (sum of piece values)
//   - Positional value (sum of piece-square table values)
//   - Game phase (for tapered evaluation)
//
// Key Methods:
//   Position(fen)           - Construct from FEN string
//   doMove(move)            - Execute move
//   undoMove()              - Undo last move
//   isLegalMove(move)       - Check move legality
//   hasCheck()              - Is side to move in check?
//   isAttacked(sq, color)   - Is square attacked by color?
//   getZobristKey()         - Hash key for TT
//   str() / strBoard()      - String representations
//
// Usage:
//   Position pos("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
//   pos.doMove(Move::normal(SQ_E7, SQ_E5));
//   bool inCheck = pos.hasCheck();
//   pos.undoMove();
//
//=============================================================================

#include "common/gtest_friends.h"
#include "types/types.h"

#include <array>

// Forward-declare test classes at global scope so FRIEND_TEST_NS inside namespace chess works
FRIEND_TEST_FWD_DECL(PositionTest, initialization);
FRIEND_TEST_FWD_DECL(PositionTest, HistoryStruct);
FRIEND_TEST_FWD_DECL(PositionTest, PosValue);

namespace chess {

  // Flag for boolean states with undetermined state
  enum Flag {
    FLAG_TBD,
    FLAG_FALSE,
    FLAG_TRUE
  };

  // HistoryState encapsulates a position state to enable undo move. It
  // acts as a stack by adding a position state after doMove and removing
  // a state after undoMove.
  struct HistoryState {
    ZobristKey zobristKey         = 0;
    ZobristKey pawnKey            = 0;
    Move move                     = MOVE_NONE;
    Piece fromPiece               = PIECE_NONE;
    Piece capturedPiece           = PIECE_NONE;
    CastlingRights castlingRights = NO_CASTLING;
    Square enPassantSquare        = SQ_NONE;
    int halfMoveClock             = 0;
    Flag hasCheckFlag             = FLAG_TBD;
  };

  class Position {

    // The zobrist key to use as a hash key in transposition tables
    // The zobrist key will be updated incrementally every time one of the
    // state variables changes.
    ZobristKey zobristKey{};

    // We also maintain a zobrist key for all pawns to support a pawn
    // evaluation table
    ZobristKey pawnKey{};

    // **********************************************************
    // Board State
    // unique chess position (exception is 3-fold repetition
    // which is also not represented in a FEN string)

    Piece board[SQ_LENGTH]{};
    Color nextPlayer = WHITE;
    CastlingRights castlingRights{};
    Square enPassantSquare = SQ_NONE;
    int halfMoveClock      = 0;
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
    int historyCounter = 0;

    // Material value will always be recalculated in doMove and undoMove
    int material[COLOR_LENGTH]{};
    int materialNonPawn[COLOR_LENGTH]{};

    // Positional value will always be recalculated in doMove and undoMove
    int psqMidValue[COLOR_LENGTH]{};
    int psqEndValue[COLOR_LENGTH]{};

    // Game phase value
    int gamePhase{};

    // caches a hasCheck Flag for the current position. Will be set
    // after a call to hasCheck() and reset to TBD every time a move is made or
    // unmade.
    mutable Flag hasCheckFlag = FLAG_TBD;

  public:
    /// Creates a position initialized with the standard chess starting setup.
    Position();

    /// Creates a position initialized from a FEN string.
    /// @param fen  FEN string describing the position
    /// @throws std::invalid_argument if FEN is invalid
    explicit Position(const char* fen);

    /// Creates a position initialized from a FEN string.
    /// @param fen  FEN string describing the position
    /// @throws std::invalid_argument if FEN is invalid
    explicit Position(const std::string& fen);

    /// Copy constructor - creates a deep copy of the given Position.
    Position(const Position& op) = default;

    /// Copy assignment operator.
    Position& operator=(const Position& other) = default;

    /// Move constructor.
    Position(Position&& other) = default;

    /// Move assignment operator.
    Position& operator=(Position&& other) = default;

    /// Destructor.
    ~Position() = default;

    /// Outputs the position as a FEN string to the stream.
    friend std::ostream& operator<<(std::ostream& os, const Position& position);

    /// Returns a detailed string representation including board and state info.
    /// @return Multi-line string with 8x8 board and additional state
    std::string str() const;

    /// Returns a simple 8x8 board representation.
    /// @return Multi-line string showing piece placement
    std::string strBoard() const;

    /// Returns the position as a FEN string.
    /// @return FEN string representation
    std::string strFen() const;

    /// Executes a move on the board, updating all state incrementally.
    /// For performance, does not verify move legality - caller must ensure legality.
    /// Legal check needs to be done beforehand or after in case of pseudo-legal moves.
    /// Usually the move will be generated by a MoveGenerator, and therefore
    /// the move will be assumed legal anyway.
    /// @param move  The move to execute
    void doMove(Move move);

    /// Undoes the last move, restoring the previous position state.
    /// Uses the history stack to restore all state components.
    /// Note: The history entry will be changed, but the history counter reset.
    /// So in effect the external view on the position is unchanged
    /// (e.g., fenBeforeDoMove == fenAfterUndoMove and
    /// zobristBeforeDoMove == zobristAfterUndoMove but
    /// positionBeforeDoMove != positionAfterUndoMove).
    /// If positionBeforeDoMove == positionAfterDoMove would be required,
    /// this function would have to be changed to reset the history entry as well.
    /// Currently, this is not necessary, and therefore we spare the time to do this.
    void undoMove();

    /// Executes a null move (pass) for null-move pruning.
    /// The position is basically unchanged, but the next player changes.
    /// The state before the null move will be stored in history for undo.
    void doNullMove();

    /// Undoes a null move, restoring the previous state.
    /// Note: The history entry will be changed but the history counter reset.
    /// So in effect the external view on the position is unchanged
    /// (e.g., fenBeforeNull == fenAfterNull and
    /// zobristBeforeNull == zobristAfterNull but
    /// positionBeforeNull != positionAfterNull).
    /// If positionBeforeNull != positionAfterNull were required,
    /// this function would have to be changed to reset the history entry as well.
    /// Currently, this is not necessary, and therefore we spare the time to do this.
    void undoNullMove();

    /// Checks if a square is attacked by the given color.
    /// It does not matter who has the next move on this position.
    /// Does not check if the attack can be done as a legal move
    /// (e.g., a pinned piece could not actually capture on the square).
    /// @param sq  Square to check
    /// @param by  Attacking color
    /// @return    True if square is attacked
    bool isAttacked(Square sq, Color by) const;

    /// Returns a bitboard of all pieces of the given color attacking a square.
    /// @param square  Target square
    /// @param color   Attacking color
    /// @return        Bitboard of attacking pieces
    Bitboard attacksTo(Square square, Color color) const;

    /// Checks if the side to move is in check (king is attacked).
    /// Result is cached for the current position - multiple calls are efficient.
    /// The cache is reset every time a move is made or unmade.
    /// @return True if king is attacked
    bool hasCheck() const;

    /// Checks if a move gives check to the opponent.
    /// This method is faster than making the move and checking hasCheck().
    /// The move must be valid for the position, otherwise behavior is undefined.
    /// For performance reasons we do not check validity here.
    /// Does NOT verify that the move itself is legal (may leave own king in check).
    /// @param move  Move to check
    /// @return      True if move gives check
    bool givesCheck(Move move) const;

    /// Tests if the last move was legal.
    /// Basically tests if the king is now in check, or if the king crossed
    /// an attacked square during castling, or if there was castling while in check.
    /// If the position does not have a last move (history empty), this will only
    /// check if the king of the opponent is attacked (could be captured).
    /// Note: wasLegalMove does not check if the move was actually valid on the
    /// position but only if a pseudoMove that was assumed valid was legal.
    /// @return True if last move was legal
    bool wasLegalMove() const;

    /// Tests if a move is legal in the current position.
    /// Basically tests if the king is left in check after the move
    /// or if the king crosses an attacked square during castling.
    /// @param move  Move to validate
    /// @return      True if move is legal
    bool isLegalMove(Move move) const;

    /// Checks for draw by repetition.
    /// To detect a 3-fold repetition, the given position must occur at least
    /// 2 times before: checkRepetitions(2) checks for 3-fold repetitions.
    /// The three occurrences need not occur on consecutive moves.
    /// Note: FIDE rules make no mention of perpetual check; this is merely
    /// a specific type of draw by threefold repetition.
    /// @param reps  Number of prior occurrences to check for (2 = threefold)
    /// @return      True if position has occurred reps times before
    bool checkRepetitions(int reps) const;

    /// Counts how many times the current position has occurred.
    /// @return Number of repetitions (0 = first occurrence)
    int countRepetitions() const;

    /// Checks for insufficient mating material on both sides.
    /// Does not exclude combinations where a helpmate would be possible
    /// (e.g., the opponent needs to support a mate by mistake).
    /// @return True if neither side can force checkmate
    bool checkInsufficientMaterial() const;

    /// Returns the last move made, or MOVE_NONE if no history.
    /// @return Last move from history stack
    Move getLastMove() const {
      if (historyCounter <= 0) return MOVE_NONE;
      return historyState[historyCounter - 1].move;
    };

    /// Determines if a move is a capturing move (including en passant).
    /// @param move  Move to check
    /// @return      True if move captures a piece
    bool isCapturingMove(const Move& move) const { return (occupiedBb[~nextPlayer] & move.to()) || move.type() == ENPASSANT; };

    /// Returns the piece captured by the last move, or PIECE_NONE.
    /// Note: Does not return a pawn captured by en passant.
    /// @return Captured piece from last move
    inline Piece getLastCapturedPiece() const {
      if (historyCounter <= 0) return PIECE_NONE;
      return historyState[historyCounter - 1].capturedPiece;
    };

  private:
    FRIEND_TEST_NS(PositionTest, initialization);
    FRIEND_TEST_NS(PositionTest, HistoryStruct);
    FRIEND_TEST_NS(PositionTest, PosValue);

    /// Initializes board data structure to an empty board.
    void initializeBoard();

    /// Sets up the board from a FEN string.
    /// @param fen  FEN string to parse
    void setupBoard(const std::string& fen);

    /// Moves a piece from one square to another (no capture handling).
    /// @param fromSq  Source square
    /// @param toSq    Destination square
    void movePiece(Square fromSq, Square toSq);

    /// Places a piece on a square, updating all data structures.
    /// @param piece   Piece to place
    /// @param square  Target square
    void putPiece(Piece piece, Square square);

    /// Removes a piece from a square, updating all data structures.
    /// @param square  Square to clear
    /// @return        The removed piece
    Piece removePiece(Square square);

    /// Clears the en passant square and updates zobrist key.
    void clearEnPassant();

  public:
    // //////////////////////////////////////////////
    // /// GETTER / SETTER

    /// Returns the piece on the given square.
    /// @param square  Square to query
    /// @return        Piece on square, or PIECE_NONE if empty
    Piece getPiece(const Square square) const { return board[square]; }

    /// Returns the main Zobrist hash key for transposition table lookup.
    /// @return 64-bit Zobrist key
    ZobristKey getZobristKey() const { return zobristKey; }

    /// Returns the pawn-specific Zobrist key for pawn hash table lookup.
    /// @return 64-bit pawn Zobrist key
    ZobristKey getPawnZobristKey() const { return pawnKey; }

    /// Returns the side to move.
    /// @return WHITE or BLACK
    Color getNextPlayer() const { return nextPlayer; }

    /// Returns the en passant target square, or SQ_NONE if none.
    /// @return En passant square (the square a pawn can capture to)
    Square getEnPassantSquare() const { return enPassantSquare; }

    /// Returns the square of the king for the given color.
    /// @param color  Color of the king
    /// @return       Square where the king is located
    Square getKingSquare(const Color color) const { return kingSquare[color]; };

    /// Returns bitboard of pieces of given color and type.
    /// @param c   Color of pieces
    /// @param pt  Piece type
    /// @return    Bitboard with bits set for each piece location
    Bitboard getPieceBb(const Color c, const PieceType pt) const { return piecesBb[c][pt]; }

    /// Returns bitboard of all occupied squares (both colors).
    /// @return Bitboard with bits set for all pieces
    Bitboard getOccupiedBb() const { return occupiedBb[WHITE] | occupiedBb[BLACK]; }

    /// Returns bitboard of squares occupied by the given color.
    /// @param c  Color
    /// @return   Bitboard with bits set for pieces of that color
    Bitboard getOccupiedBb(const Color c) const { return occupiedBb[c]; }

    /// Returns total material value for the given color (includes pawns).
    /// @param c  Color
    /// @return   Material value in centipawns
    int getMaterial(const Color c) const { return material[c]; }

    /// Returns non-pawn material value for the given color.
    /// @param c  Color
    /// @return   Material value excluding pawns, in centipawns
    int getMaterialNonPawn(const Color c) const { return materialNonPawn[c]; }

    /// Returns midgame piece-square table value for the given color.
    /// @param c  Color
    /// @return   Positional value for midgame
    int getMidPosValue(const Color c) const { return psqMidValue[c]; }

    /// Returns endgame piece-square table value for the given color.
    /// @param c  Color
    /// @return   Positional value for endgame
    int getEndPosValue(const Color c) const { return psqEndValue[c]; }

    /// Returns interpolated positional value based on game phase.
    /// Combines midgame and endgame PST values using getGamePhaseFactor().
    /// @param c  Color
    /// @return   Tapered positional value
    int getPosValue(const Color c) const {
      return static_cast<int>(getGamePhaseFactor() * psqMidValue[c] + (1 - getGamePhaseFactor()) * psqEndValue[c]);
    }

    /// Returns the current castling rights.
    /// @return CastlingRights bitmask (WHITE_OO, WHITE_OOO, BLACK_OO, BLACK_OOO)
    CastlingRights getCastlingRights() const { return castlingRights; }

    /// Returns the half-move clock (plies since last pawn move or capture).
    /// Used for 50-move rule detection.
    /// @return Half-move clock value
    int getHalfMoveClock() const { return halfMoveClock; }

    /// Returns the full move number (starts at 1, increments after Black moves).
    /// @return Move number
    int getMoveNumber() const { return moveNumber; }

    /// Returns the game phase value for tapered evaluation.
    /// Ranges from 24 (all pieces = opening) to 0 (no pieces = endgame).
    /// Note: Phase is calculated from piece counts (N=1, B=1, R=2, Q=4).
    /// @return Game phase value (0-24)
    int getGamePhase() const { return gamePhase; }

    /// Returns the game phase as a factor for interpolation.
    /// Ranges from 1.0 (opening/midgame) to 0.0 (endgame).
    /// @return Game phase factor for tapered eval
    double getGamePhaseFactor() const { return static_cast<double>(gamePhase) / GAME_PHASE_MAX; }
  };

}// namespace chess

#endif// FRANKYCPP_POSITION_H
