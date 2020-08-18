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

#include <string>

#include "Position.h"
#include "Values.h"
#include "common/stringutil.h"

Key Zobrist::pieces[PIECE_LENGTH][SQ_LENGTH];
Key Zobrist::castlingRights[CR_LENGTH];
Key Zobrist::enPassantFile[FILE_LENGTH];
Key Zobrist::nextPlayer;

////////////////////////////////////////////////
///// STATIC

bool Position::initialized = false;

void Position::init() {
  // Zobrist Key initialization
  PRNG random(1070372);// seed from Stockfish :) - they supposedly did some research
  for (Piece pc = PIECE_NONE; pc < PIECE_LENGTH; ++pc) {
    for (Square sq = SQ_A1; sq < SQ_LENGTH; ++sq) {
      Zobrist::pieces[pc][sq] = random.rand<Key>();
    }
  }
  for (CastlingRights cr = NO_CASTLING; cr <= ANY_CASTLING; ++cr) {
    Zobrist::castlingRights[cr] = random.rand<Key>();
  }
  for (File f = FILE_A; f <= FILE_H; ++f) {
    Zobrist::enPassantFile[f] = random.rand<Key>();
  }
  Zobrist::nextPlayer   = random.rand<Key>();
  Position::initialized = true;
  //  fprintln("Position initialized");
}

////////////////////////////////////////////////
///// CONSTRUCTORS

/** Default constructor creates a board with standard start setup */
Position::Position() : Position(START_POSITION_FEN) {}

/** Creates a board with setup from the given fen */
Position::Position(const char* fen) : Position(std::string{fen}) {}

/** Creates a board with setup from the given fen */
Position::Position(const std::string& fen) {
  if (!Position::initialized) {
    Position::init();
  }
  try {
    setupBoard(fen);
  } catch (std::invalid_argument&) {
    // std::cerr << "Invalid fen: " << exception.what() << std::endl;
    throw;
  }
}

////////////////////////////////////////////////
///// PUBLIC

void Position::doMove(Move move) {
  assert(validMove(move));
  assert(validSquare(fromSquare(move)));
  assert(validSquare(toSquare(move)));
  assert(getPiece(fromSquare(move)) != PIECE_NONE);
  assert(colorOf(getPiece(fromSquare(move))) == nextPlayer);
  assert((historyCounter < MAX_MOVES - 1) && "Can't have more move than MAX_MOVES");

  const Square fromSq = fromSquare(move);
  const Square toSq   = toSquare(move);

  // Save state of board for undo
  historyState[historyCounter++] = {zobristKey,
                                    pawnKey,
                                    move,
                                    board[fromSq],
                                    board[toSq],
                                    castlingRights,
                                    enPassantSquare,
                                    halfMoveClock,
                                    hasCheckFlag};

  // change the position data according to the move
  switch (typeOf(move)) {

    case NORMAL:
      // If we still have castling rights and the move touches castling squares then invalidate
      // the corresponding castling right
      if (castlingRights) {
        CastlingRights cr = Castling::castlingRights[fromSq] + Castling::castlingRights[toSq];
        if (cr) {
          zobristKey ^= Zobrist::castlingRights[castlingRights];// out
          castlingRights -= cr;
          zobristKey ^= Zobrist::castlingRights[castlingRights];// in
        }
      }
      clearEnPassant();
      if (getPiece(toSq) != PIECE_NONE) {// capture
        removePiece(toSq);
        halfMoveClock = 0;// reset half move clock because of capture
      }
      else if (typeOf(getPiece(fromSq)) == PAWN) {
        halfMoveClock = 0;                // reset half move clock because of pawn move
        if (distance(fromSq, toSq) == 2) {// pawn double - set en passant
          // set new en passant target field - always one "behind" the toSquare
          enPassantSquare = toSq + pawnPush(~colorOf(getPiece(fromSq)));
          zobristKey      = zobristKey ^ Zobrist::enPassantFile[fileOf(enPassantSquare)];// in
        }
      }
      else {
        halfMoveClock++;
      }
      movePiece(fromSq, toSq);
      break;

    case PROMOTION: {
      assert(getPiece(fromSquare(move)) == makePiece(colorOf(getPiece(fromSquare(move))), PAWN));
      assert(rankOf(toSq) == (colorOf(getPiece(fromSquare(move))) == WHITE ? RANK_8 : RANK_1));
      // capture
      if (getPiece(toSq) != PIECE_NONE) {
        removePiece(toSq);
      }
      // If we still have castling rights and the move touches castling squares then invalidate
      // the corresponding castling right
      if (castlingRights) {
        CastlingRights cr = Castling::castlingRights[fromSq] + Castling::castlingRights[toSq];
        if (cr) {
          zobristKey ^= Zobrist::castlingRights[castlingRights];// out
          castlingRights -= cr;
          zobristKey ^= Zobrist::castlingRights[castlingRights];// in
        }
      }
      clearEnPassant();
      removePiece(fromSq);
      putPiece(makePiece(nextPlayer, promotionTypeOf(move)), toSq);
      halfMoveClock = 0;// reset half move clock because of pawn move
      break;
    }

    case ENPASSANT: {
      assert(getPiece(fromSquare(move)) == makePiece(colorOf(getPiece(fromSquare(move))), PAWN));
      assert(enPassantSquare != SQ_NONE);
      const Square capSq = toSq + pawnPush(~colorOf(getPiece(fromSq)));
      assert(getPiece(capSq) == makePiece(~colorOf(getPiece(fromSquare(move))), PAWN));
      clearEnPassant();
      removePiece(capSq);
      movePiece(fromSq, toSq);
      halfMoveClock = 0;// reset half move clock because of pawn move
      break;
    }

    case CASTLING:
      assert(getPiece(fromSquare(move)) == makePiece(colorOf(getPiece(fromSquare(move))), KING));
      switch (toSq) {
        case SQ_G1:
          assert(castlingRights == WHITE_OO);
          assert(fromSq == SQ_E1);
          assert(getPiece(SQ_E1) == WHITE_KING);
          assert(getPiece(SQ_H1) == WHITE_ROOK);
          assert(!(getOccupiedBb() & Bitboards::intermediateBb[SQ_E1][SQ_H1]));
          movePiece(fromSq, toSq);                              // King
          movePiece(SQ_H1, SQ_F1);                              // Rook
          zobristKey ^= Zobrist::castlingRights[castlingRights];// out
          castlingRights -= WHITE_CASTLING;
          zobristKey ^= Zobrist::castlingRights[castlingRights];// in;
          break;
        case SQ_C1:
          assert(castlingRights == WHITE_OOO);
          assert(fromSq == SQ_E1);
          assert(getPiece(SQ_E1) == WHITE_KING);
          assert(getPiece(SQ_A1) == WHITE_ROOK);
          assert(!(getOccupiedBb() & Bitboards::intermediateBb[SQ_E1][SQ_A1]));
          movePiece(fromSq, toSq);                              // King
          movePiece(SQ_A1, SQ_D1);                              // Rook
          zobristKey ^= Zobrist::castlingRights[castlingRights];// out
          castlingRights -= WHITE_CASTLING;
          zobristKey ^= Zobrist::castlingRights[castlingRights];// in
          break;
        case SQ_G8:
          assert(castlingRights == BLACK_OO);
          assert(fromSq == SQ_E8);
          assert(getPiece(SQ_E8) == BLACK_KING);
          assert(getPiece(SQ_H8) == BLACK_ROOK);
          assert(!(getOccupiedBb() & Bitboards::intermediateBb[SQ_E8][SQ_H8]));
          movePiece(fromSq, toSq);                              // King
          movePiece(SQ_H8, SQ_F8);                              // Rook
          zobristKey ^= Zobrist::castlingRights[castlingRights];// out
          castlingRights -= BLACK_CASTLING;
          zobristKey ^= Zobrist::castlingRights[castlingRights];// in
          break;
        case SQ_C8:
          assert(castlingRights == BLACK_OOO);
          assert(fromSq == SQ_E8);
          assert(getPiece(SQ_E8) == BLACK_KING);
          assert(getPiece(SQ_A8) == BLACK_ROOK);
          assert(!(getOccupiedBb() & Bitboards::intermediateBb[SQ_E8][SQ_A8]));
          movePiece(fromSq, toSq);                              // King
          movePiece(SQ_A8, SQ_D8);                              // Rook
          zobristKey ^= Zobrist::castlingRights[castlingRights];// out
          castlingRights -= BLACK_CASTLING;
          zobristKey ^= Zobrist::castlingRights[castlingRights];// in
          break;
        default:
          throw std::invalid_argument("Invalid castle move!");
      }
      clearEnPassant();
      halfMoveClock++;
      break;
  }

  // update additional state info
  hasCheckFlag = FLAG_TBD;
  if (nextPlayer == BLACK) moveNumber++;
  nextPlayer = ~nextPlayer;
  zobristKey ^= Zobrist::nextPlayer;
}

void Position::undoMove() {
  assert(historyCounter > 0);

  // Restore state part 1
  historyCounter--;
  if (nextPlayer == WHITE) moveNumber--;
  nextPlayer = ~nextPlayer;

  const HistoryState& lastHistoryState = historyState[historyCounter];
  const Move move                      = lastHistoryState.move;

  // undo piece move / restore board
  switch (typeOf(move)) {

    case NORMAL:
      movePiece(toSquare(move), fromSquare(move));
      if (lastHistoryState.capturedPiece != PIECE_NONE) {
        putPiece(lastHistoryState.capturedPiece, toSquare(move));
      }
      break;

    case PROMOTION:
      removePiece(toSquare(move));
      putPiece(makePiece(nextPlayer, PAWN), fromSquare(move));
      if (lastHistoryState.capturedPiece != PIECE_NONE) {
        putPiece(lastHistoryState.capturedPiece, toSquare(move));
      }
      break;

    case ENPASSANT:
      // ignore Zobrist Key as it will be restored via history
      movePiece(toSquare(move), fromSquare(move));
      putPiece(makePiece(~nextPlayer, PAWN), toSquare(move) + pawnPush(~nextPlayer));
      break;

    case CASTLING:
      // ignore Zobrist Key as it will be restored via history
      // castling rights are restored via history
      movePiece(toSquare(move), fromSquare(move));// King
      switch (toSquare(move)) {
        case SQ_G1:
          movePiece(SQ_F1, SQ_H1);// Rook
          break;
        case SQ_C1:
          movePiece(SQ_D1, SQ_A1);// Rook
          break;
        case SQ_G8:
          movePiece(SQ_F8, SQ_H8);// Rook
          break;
        case SQ_C8:
          movePiece(SQ_D8, SQ_A8);// Rook
          break;
        default:
          throw std::invalid_argument("Invalid castle move!");
      }
      break;
  }

  // restore state part 2
  castlingRights  = lastHistoryState.castlingRights;
  enPassantSquare = lastHistoryState.enPassantSquare;
  halfMoveClock   = lastHistoryState.halfMoveClock;
  zobristKey      = lastHistoryState.zobristKey;
  pawnKey         = lastHistoryState.pawnKey;
  hasCheckFlag    = lastHistoryState.hasCheckFlag;
}

void Position::doNullMove() {
  // Save state of board for undo
  // update existing history entry to not create and allocate a new one
  // Save state of board for undo
  historyState[historyCounter++] = {zobristKey,
                                    pawnKey,
                                    MOVE_NONE,
                                    PIECE_NONE,
                                    PIECE_NONE,
                                    castlingRights,
                                    enPassantSquare,
                                    halfMoveClock,
                                    hasCheckFlag};
  // update state for null move
  hasCheckFlag = FLAG_TBD;
  clearEnPassant();
  if (nextPlayer == BLACK) moveNumber++;
  nextPlayer = ~nextPlayer;
  zobristKey ^= Zobrist::nextPlayer;
}

void Position::undoNullMove() {
  // Restore state
  historyCounter--;
  if (nextPlayer == WHITE) moveNumber--;
  nextPlayer                           = ~nextPlayer;
  const HistoryState& lastHistoryState = historyState[historyCounter];
  castlingRights                       = lastHistoryState.castlingRights;
  enPassantSquare                      = lastHistoryState.enPassantSquare;
  halfMoveClock                        = lastHistoryState.halfMoveClock;
  hasCheckFlag                         = lastHistoryState.hasCheckFlag;
  pawnKey                              = lastHistoryState.pawnKey;
  zobristKey                           = lastHistoryState.zobristKey;
}

bool Position::isAttacked(Square sq, Color by) const {
  // to test if a position is attacked we do a reverse attack from the
  // target square to see if we hit a piece of the same or similar type

  const Bitboard occupiedAll = getOccupiedBb();

  // non sliding
  if (Bitboards::pawnAttacks[~by][sq] & piecesBb[by][PAWN] ||
      getAttacksBb(KNIGHT, sq, BbZero) & piecesBb[by][KNIGHT] ||
      getAttacksBb(KING, sq, BbZero) & piecesBb[by][KING]) {
    return true;
  }

  // sliding
  // we do check a reverse attack and check if there is piece of the right color
  // in the reversed attack line. If yes they also could hit us which means
  // the square is attacked.
  if ((getAttacksBb(BISHOP, sq, occupiedAll) & piecesBb[by][BISHOP]) ||
      (getAttacksBb(ROOK, sq, occupiedAll) & piecesBb[by][ROOK]) ||
      (getAttacksBb(QUEEN, sq, occupiedAll) & piecesBb[by][QUEEN])) {
    return true;
  }

  // check en passant
  if (enPassantSquare != SQ_NONE) {
    if (by) {// BLACK
      // white is target
      if (board[enPassantSquare + NORTH] == WHITE_PAWN &&
          // this is indeed the en passant attacked square
          enPassantSquare + NORTH == sq) {
        // left
        if (board[(sq + WEST)] == BLACK_PAWN) {
          return true;
        }
        // right
        return board[(sq + EAST)] == BLACK_PAWN;
      }
    }
    else {// WHITE
      // black is target
      if (board[enPassantSquare + SOUTH] == BLACK_PAWN &&
          // this is indeed the en passant attacked square
          enPassantSquare + SOUTH == sq) {
        // left
        if (board[(sq + WEST)] == WHITE_PAWN) {
          return true;
        }
        // right
        return board[(sq + EAST)] == WHITE_PAWN;
      }
    }
  }
  return false;
}

Bitboard Position::attacksTo(Square square, Color color) const {
  assert(validSquare(square) && "Position::attacksTo needs a valid square");

  // prepare en passant attacks
  Bitboard epAttacks = BbZero;
  if (enPassantSquare != SQ_NONE) {
    const Square pawnSquare = enPassantSquare + pawnPush(~color);
    if (pawnSquare == square) {
      epAttacks |= Bitboards::neighbourFilesMask[pawnSquare] & Bitboards::sqToRankBb[pawnSquare] & piecesBb[color][PAWN];
    }
  }

  const Bitboard occupiedAll = getOccupiedBb();

  // this uses a reverse approach - it uses the target square as from square
  // to generate attacks for each type and then intersects the result with
  // the piece bitboard.

  //     Pawns
  return (Bitboards::pawnAttacks[~color][square] & piecesBb[color][PAWN]) |
         // Knight
         (getAttacksBb(KNIGHT, square, occupiedAll) & piecesBb[color][KNIGHT]) |
         // King
         (getAttacksBb(KING, square, occupiedAll) & piecesBb[color][KING]) |
         // Sliding rooks and queens
         (getAttacksBb(ROOK, square, occupiedAll) & (piecesBb[color][ROOK] | piecesBb[color][QUEEN])) |
         // Sliding bishops and queens
         (getAttacksBb(BISHOP, square, occupiedAll) & (piecesBb[color][BISHOP] | piecesBb[color][QUEEN])) |
         // consider en passant attacks
         epAttacks;
}

bool Position::hasCheck() const {
  if (hasCheckFlag != FLAG_TBD) {
    return (hasCheckFlag == FLAG_TRUE);
  }
  const bool check = isAttacked(kingSquare[nextPlayer], ~nextPlayer);
  hasCheckFlag     = check ? FLAG_TRUE : FLAG_FALSE;
  return check;
}

bool Position::givesCheck(Move move) const {
  const Color us   = nextPlayer;
  const Color them = ~us;

  // opponents king square
  const Square kingSq = kingSquare[them];

  // move details
  const Square fromSq     = fromSquare(move);
  const Piece fromPc      = board[fromSq];
  const MoveType moveType = typeOf(move);
  PieceType fromPt        = typeOf(fromPc);
  Square toSq             = toSquare(move);
  Square epTargetSq{};

  switch (moveType) {
    case PROMOTION:
      // promotion moves - use new piece type
      fromPt = promotionTypeOf(move);
      break;
    case CASTLING:
      // set the target square to the rook square and
      // piece type to ROOK. King can't give check
      // also no revealed check possible in castling
      fromPt = ROOK;
      switch (toSq) {
        case SQ_G1:// white king side castle
          toSq = SQ_F1;
          break;
        case SQ_C1:// white queen side castle
          toSq = SQ_D1;
          break;
        case SQ_G8:// black king side castle
          toSq = SQ_F8;
          break;
        case SQ_C8:// black queen side castle
          toSq = SQ_D8;
          break;
        default:
          break;
      }
      break;
    case ENPASSANT:
      // set en passant capture square
      epTargetSq = toSq + pawnPush(them);
      break;
    default:
      break;
  }

  // get all pieces to check occupied intermediate squares
  Bitboard boardAfterMove = getOccupiedBb();

  // adapt board by moving the piece on the bitboard
  boardAfterMove ^= fromSq;
  boardAfterMove |= toSq;
  if (moveType == ENPASSANT) {
    boardAfterMove ^= epTargetSq;
  }

  // Find direct checks
  switch (fromPt) {
    case PAWN:
      if (Bitboards::pawnAttacks[us][toSq] & kingSq)
        return true;
      break;
    case KING:
      // ignore - can't give check
      break;
    default:
      if (getAttacksBb(fromPt, toSq, boardAfterMove) & kingSq)
        return true;
      break;
  }

  // revealed checks
  // we only need to check for rook, bishop and queens
  // knight and pawn attacks can't be revealed
  // exception is en passant where the captured piece can reveal check
  if (getAttacksBb(BISHOP, kingSq, boardAfterMove) & piecesBb[us][BISHOP]) return true;
  if (getAttacksBb(ROOK, kingSq, boardAfterMove) & piecesBb[us][ROOK]) return true;
  if (getAttacksBb(QUEEN, kingSq, boardAfterMove) & piecesBb[us][QUEEN]) return true;

  // we did not find a check
  return false;
}

bool Position::wasLegalMove() const {
  // king attacked?
  if (isAttacked(kingSquare[~nextPlayer], nextPlayer)) return false;
  // look back and check if castling was legal
  if (historyCounter > 0) {
    const Move lastMove = historyState[historyCounter - 1].move;
    if (typeOf(lastMove) == CASTLING) {
      // no castling when in check
      if (isAttacked(fromSquare(lastMove), nextPlayer)) {
        return false;
      }
      // king is not allowed to pass a square which is attacked by opponent
      switch (toSquare(lastMove)) {
        case SQ_G1:
          if (isAttacked(SQ_F1, nextPlayer)) return false;
          break;
        case SQ_C1:
          if (isAttacked(SQ_D1, nextPlayer)) return false;
          break;
        case SQ_G8:
          if (isAttacked(SQ_F8, nextPlayer)) return false;
          break;
        case SQ_C8:
          if (isAttacked(SQ_D8, nextPlayer)) return false;
          break;
        default:
          break;
      }
    }
  }
  return true;
}

bool Position::isLegalMove(Move move) const {
  // king is not allowed to pass a square which is attacked by opponent
  if (typeOf(move) == CASTLING) {
    // castling not allowed when in check
    // we can simply check the from square of the castling move
    // and check if the current opponent attacks it. Castling would not
    // be possible if the attack would be influenced by the castling
    // itself.
    if (isAttacked(fromSquare(move), ~nextPlayer)) {
      return false;
    }
    // castling crossing attacked square?
    switch (toSquare(move)) {
      case SQ_G1:
        if (isAttacked(SQ_F1, ~nextPlayer)) return false;
        break;
      case SQ_C1:
        if (isAttacked(SQ_D1, ~nextPlayer)) return false;
        break;
      case SQ_G8:
        if (isAttacked(SQ_F8, ~nextPlayer)) return false;
        break;
      case SQ_C8:
        if (isAttacked(SQ_D8, ~nextPlayer)) return false;
        break;
      default:
        break;
    }
  }
  // make the move on the position
  // then check if the move leaves the king in check
  // This could probably be implemented a bit more efficient by
  // not having to call DoMove/UndoMove similar to GivesCheck() but
  // IsLegalMove is not used during normal search.
  // Used in generateLegalMoves and perft. 
  const_cast<Position*>(this)->doMove(move);
  const bool legal = !isAttacked(kingSquare[~nextPlayer], nextPlayer);
  const_cast<Position*>(this)->undoMove();
  return legal;
}

bool Position::checkRepetitions(int reps) const {
  /*
   [0]     3185849660387886977 << 1st
   [1]     447745478729458041
   [2]     3230145143131659788
   [3]     491763876012767476
   [4]     3185849660387886977 << 2nd
   [5]     447745478729458041
   [6]     3230145143131659788
   [7]     491763876012767476  <<< history
   [8]     3185849660387886977 <<< 3rd REPETITION from current zobrist
    */
  int counter      = 0;
  int i            = historyCounter - 2;
  int lastHalfMove = halfMoveClock;
  while (i >= 0) {
    // every time the half move clock gets reset (non reversible position) there
    // can't be any more repetition of positions before this position
    if (historyState[i].halfMoveClock >= lastHalfMove) {
      break;
    }
    else {
      lastHalfMove = historyState[i].halfMoveClock;
    }
    if (zobristKey == historyState[i].zobristKey) {
      counter++;
    }
    if (counter >= reps) {
      return true;
    }
    i -= 2;
  }
  return false;
}

int Position::countRepetitions() const {
  int counter      = 0;
  int i            = historyCounter - 2;
  int lastHalfMove = halfMoveClock;
  while (i >= 0) {
    // every time the half move clock gets reset (non reversible position) there
    // can't be any more repetition of positions before this position
    if (historyState[i].halfMoveClock >= lastHalfMove) {
      break;
    }
    else {
      lastHalfMove = historyState[i].halfMoveClock;
    }
    if (zobristKey == historyState[i].zobristKey) {
      counter++;
    }
    i -= 2;
  }
  return counter;
}

bool Position::checkInsufficientMaterial() const {

  // no material
  // both sides have a bare king
  if (!(material[WHITE] + material[BLACK])) {
    return true;
  }

  // still a pawn, rook or a queen around
  if (popcount(piecesBb[WHITE][PAWN]) ||
      popcount(piecesBb[BLACK][PAWN]) ||
      popcount(piecesBb[WHITE][ROOK]) ||
      popcount(piecesBb[BLACK][ROOK]) ||
      popcount(piecesBb[WHITE][QUEEN]) ||
      popcount(piecesBb[BLACK][QUEEN])) {
    return false;
  }

  // no more pawns, rooks or queens

  // one side has a king and a minor piece against a bare king
  // both sides have a king and a minor piece each
  if (materialNonPawn[WHITE] < 400 && materialNonPawn[BLACK] < 400) {
    return true;
  }
  // the weaker side has a minor piece against two knights
  if ((materialNonPawn[WHITE] == 2 * valueOf(KNIGHT) && materialNonPawn[BLACK] <= valueOf(BISHOP)) ||
      (materialNonPawn[BLACK] == 2 * valueOf(KNIGHT) && materialNonPawn[WHITE] <= valueOf(BISHOP))) {
    return true;
  }
  // two bishops draw against a bishop
  if ((materialNonPawn[WHITE] == 2 * valueOf(BISHOP) && materialNonPawn[BLACK] == valueOf(BISHOP)) ||
      (materialNonPawn[BLACK] == 2 * valueOf(BISHOP) && materialNonPawn[WHITE] == valueOf(BISHOP))) {
    return true;
  }
  // one side has two bishops a mate can be forced
  if (materialNonPawn[WHITE] == 2 * valueOf(BISHOP) || materialNonPawn[BLACK] == 2 * valueOf(BISHOP)) {
    return false;
  }
  // two minor pieces against one draw, except when the stronger side has a bishop pair
  return (materialNonPawn[WHITE] < 2 * valueOf(BISHOP) && materialNonPawn[BLACK] <= valueOf(BISHOP)) ||
         (materialNonPawn[WHITE] <= valueOf(BISHOP) && materialNonPawn[BLACK] < 2 * valueOf(BISHOP));
}

////////////////////////////////////////////////
///// STR

std::string Position::str() const {
  std::ostringstream output;
  output << strBoard();
  output << strFen() << std::endl;
  output << "Check: "
         << (hasCheckFlag == FLAG_TBD ? "N/A" : hasCheckFlag == FLAG_TRUE ? "Check"
                                                                          : "No check")
         << std::endl;
  ;
  output << "Game Phase: " << gamePhase << std::endl;
  output << "Material: white=" << material[WHITE]
         << " black=" << material[BLACK] << std::endl;
  output << "Non Pawn: white=" << materialNonPawn[WHITE]
         << " black=" << materialNonPawn[BLACK] << std::endl;
  output << "PosValue: white=" << psqMidValue[WHITE]
         << " black=" << psqMidValue[BLACK] << std::endl;
  output << "Zobrist Key: " << zobristKey << std::endl;
  return output.str();
}

std::string Position::strBoard() const {
  const std::string ptc = " KONBRQ  k*nbrq   ";
  std::ostringstream output;
  output << "  +---+---+---+---+---+---+---+---+" << std::endl;
  for (Rank r = RANK_8;; --r) {
    output << (r + 1) << " |";
    for (File f = FILE_A; f <= FILE_H; ++f) {
      Piece pc = getPiece(squareOf(f, r));
      if (pc == PIECE_NONE) {
        output << "   |";
      }
      else {
        output << " " << ptc[pc] << " |";
      }
    }
    output << std::endl;
    output << "  +---+---+---+---+---+---+---+---+" << std::endl;
    if (r == 0) break;
  }
  output << "   ";
  for (File f = FILE_A; f <= FILE_H; ++f) {
    output << " " << char('A' + f) << "  ";
  }
  output << std::endl
         << std::endl;
  return output.str();
}

std::string Position::strFen() const {
  std::ostringstream fen;

  // pieces
  for (Rank r = RANK_8;; --r) {
    int emptySquares = 0;
    for (File f = FILE_A; f <= FILE_H; ++f) {
      Piece pc = getPiece(squareOf(f, r));
      if (pc == PIECE_NONE) {
        emptySquares++;
      }
      else {
        if (emptySquares) {
          fen << emptySquares;
          emptySquares = 0;
        }
        fen << pieceToChar[pc];
      }
    }
    if (emptySquares) {
      fen << emptySquares;
    }
    if (r > RANK_1) {
      fen << "/";
    }
    if (r == 0) break;
  }

  // next player
  fen << (nextPlayer ? " b " : " w ");

  // castling
  if (castlingRights == NO_CASTLING) {
    fen << "-";
  }
  else {
    if (castlingRights & WHITE_OO) {
      fen << "K";
    }
    if (castlingRights & WHITE_OOO) {
      fen << "Q";
    }
    if (castlingRights & BLACK_OO) {
      fen << "k";
    }
    if (castlingRights & BLACK_OOO) {
      fen << "q";
    }
  }

  // en passant
  if (enPassantSquare != SQ_NONE) {
    fen << " " << enPassantSquare << " ";
  }
  else {
    fen << " - ";
  }

  // half move clock
  fen << halfMoveClock << " ";

  // full move number
  fen << moveNumber;

  return fen.str();
}

std::ostream& operator<<(std::ostream& os, const Position& position) {
  os << position.strFen();
  return os;
}

////////////////////////////////////////////////
///// PRIVATE

inline void Position::movePiece(Square fromSq, Square toSq) {
  putPiece(removePiece(fromSq), toSq);
}

void Position::putPiece(Piece piece, Square square) {
  assert(getPiece(square) == PIECE_NONE);
  assert((piecesBb[colorOf(piece)][typeOf(piece)] & square) == 0);
  assert((occupiedBb[colorOf(piece)] & square) == 0);

  const PieceType pieceType = typeOf(piece);
  const Color color         = colorOf(piece);
  // piece board
  board[square] = piece;
  if (pieceType == KING) {
    kingSquare[color] = square;
  }
  // bitboards
  piecesBb[color][pieceType] |= square;
  occupiedBb[color] |= square;
  // zobrist
  zobristKey ^= Zobrist::pieces[piece][square];
  if (pieceType == PAWN) {
    pawnKey ^= Zobrist::pieces[piece][square];
  }
  // game phase
  gamePhase += gamePhaseValue(pieceType);
  if (gamePhase > GAME_PHASE_MAX) {
    gamePhase = GAME_PHASE_MAX;
  };
  // material
  material[color] += pieceTypeValue[pieceType];
  if (pieceType > PAWN) {
    materialNonPawn[color] += pieceTypeValue[pieceType];
  }
  // position value
  psqMidValue[color] += Values::posMidValue[piece][square];
  psqEndValue[color] += Values::posEndValue[piece][square];
}

Piece Position::removePiece(Square square) {
  assert(getPiece(square) != PIECE_NONE);
  assert(piecesBb[colorOf(getPiece(square))][typeOf(getPiece(square))] & square);
  assert(occupiedBb[colorOf(getPiece(square))] & square);

  const Piece removed       = getPiece(square);
  const Color color         = colorOf(removed);
  const PieceType pieceType = typeOf(removed);
  // piece board
  board[square] = PIECE_NONE;
  // bitboards
  piecesBb[color][pieceType] ^= square;
  occupiedBb[color] ^= square;
  // zobrist
  zobristKey ^= Zobrist::pieces[removed][square];
  if (pieceType == PAWN) {
    pawnKey ^= Zobrist::pieces[removed][square];
  }
  // game phase
  gamePhase -= phaseValue[pieceType];
  if (gamePhase < 0) {
    gamePhase = 0;
  }
  // material
  material[color] -= pieceTypeValue[pieceType];
  if (pieceType > PAWN) {
    materialNonPawn[color] -= pieceTypeValue[pieceType];
  }
  // position value
  psqMidValue[color] -= Values::posMidValue[removed][square];
  psqEndValue[color] -= Values::posEndValue[removed][square];
  // return the removed piece
  return removed;
}

inline void Position::clearEnPassant() {
  if (enPassantSquare != SQ_NONE) {
    zobristKey      = zobristKey ^ Zobrist::enPassantFile[fileOf(enPassantSquare)];// out
    enPassantSquare = SQ_NONE;
  }
}

void Position::initializeBoard() {

  std::fill_n(&board[0], sizeof(board), PIECE_NONE);

  castlingRights  = NO_CASTLING;
  enPassantSquare = SQ_NONE;
  halfMoveClock   = 0;

  historyState.fill(HistoryState());

  nextPlayer = WHITE;

  moveNumber = 1;

  for (Color color = WHITE; color <= BLACK; ++color) {// foreach color
    occupiedBb[color] = BbZero;
    std::fill_n(&piecesBb[color][0], sizeof(piecesBb[color]), BbZero);
    kingSquare[color]      = SQ_NONE;
    material[color]        = 0;
    materialNonPawn[color] = 0;
    psqMidValue[color]     = 0;
  }

  hasCheckFlag = FLAG_TBD;
  gamePhase    = 0;
}

void Position::setupBoard(const std::string& fen) {
  // also sets defaults if fen is short
  initializeBoard();

  // clean up and check fen
  std::string myFen = trimFast(fen);

  std::vector<std::string> fenParts{};

  // split into parts and check if at least the position is available
  split(myFen, fenParts, ' ');
  if (fenParts.empty()) {
    throw std::invalid_argument("FEN must not be empty: " + myFen);
  }

  // check if position fen part is valid
  static const std::string allowedChars{"12345678pPnNbBrRqQkK/"};
  auto l = fenParts[0].length();
  for (int i = 0; i < l; i++) {
    if (allowedChars.find(fenParts[0][i]) == std::string::npos) {
      throw std::invalid_argument("FEN contains illegal characters: " + fenParts[0]);
    }
  }

  // a fen start at A8 - which is file==0 and rank==7
  int file = 0;
  int rank = 7;

  // pieces
  unsigned char token;
  std::istringstream iss(fenParts[0]);
  iss >> std::noskipws;
  while (iss >> token) {
    if (std::isdigit(token)) {// number
      file += token - '0';
      if (file > 8) {
        throw std::invalid_argument(fmt::format("FEN has too many squares ({}) in rank {}:  {}", file, rank + 1, fenParts[0]));
      }
    }
    else if (token == '/') {// rank separator
      if (file < 8) {
        throw std::invalid_argument(fmt::format("FEN has not enough squares ({}) in rank {}:  {}", file, rank + 1, fenParts[0]));
      }
      if (file > 8) {
        throw std::invalid_argument(fmt::format("too many squares ({}) in rank {}:  {}", file, rank + 1, fenParts[0]));
      }
      // reset file counter and decrease rank
      file = 0;
      rank--;
      if (rank < 0) {
        throw std::invalid_argument(fmt::format("FEN has too many ranks ({}):  %s", 8 - rank, fenParts[0]));
      }
    }
    else {// find piece type from piece symbol
      Piece piece = makePiece(token);
      if (piece == PIECE_NONE) {// redundant
        throw std::invalid_argument(fmt::format("FEN has invalid piece character '{}' in {}", token, fenParts[0]));
      }
      if (file > 7) {
        throw std::invalid_argument(fmt::format("FEN has too many squares ({}) in rank {}:  {}", file, rank + 1, fenParts[0]));
      }
      Square currentSquare = squareOf(File(file), Rank(rank));
      if (currentSquare == SQ_NONE) {
        throw std::invalid_argument(fmt::format("FEN has invalid square {} ({}): {}", currentSquare, ::str(currentSquare), fenParts[0]));
      }
      putPiece(piece, currentSquare);
      file++;
    }
  }
  if (file != 8 || rank != 0) {
    throw std::invalid_argument(fmt::format("FEN not complete. Did not reached last square (file={}, rank={}) after reading fen", file, rank));
  }

  // set defaults
  moveNumber      = 1;
  enPassantSquare = SQ_NONE;

  // everything below is optional as we can apply defaults

  // next player
  if (fenParts.size() >= 2) {
    if (fenParts[1] == "b") {
      nextPlayer = BLACK;
      zobristKey ^= Zobrist::nextPlayer;
    }
    else if (fenParts[1] != "w") {
      throw std::invalid_argument(fmt::format("FEN next player is invalid: {}", fenParts[1]));
    }
  }

  // castling rights
  if (fenParts.size() >= 3) {
    static const std::string allowedCastlingChars{"KkQq-"};
    l = fenParts[2].length();
    for (int i = 0; i < l; i++) {
      if (allowedCastlingChars.find(fenParts[2][i]) == std::string::npos) {
        throw std::invalid_argument("FEN castling rights contains illegal characters: " + fenParts[2]);
      }
    }
    // are there  rights to be encoded?
    if (fenParts[2] != "-") {
      for (char c : fenParts[2]) {
        if (c == 'K' && !(castlingRights == WHITE_OO)) {
          castlingRights += WHITE_OO;
        }
        else if (c == 'Q' && !(castlingRights == WHITE_OOO)) {
          castlingRights += WHITE_OOO;
        }
        else if (c == 'k' && !(castlingRights == BLACK_OO)) {
          castlingRights += BLACK_OO;
        }
        else if (c == 'q' && !(castlingRights == BLACK_OOO)) {
          castlingRights += BLACK_OOO;
        }
        else {
          throw std::invalid_argument("FEN castling rights has invalid structure: " + fenParts[2]);
        }
      }
    }
    zobristKey ^= Zobrist::castlingRights[castlingRights];
  }

  // en passant
  if (fenParts.size() >= 4) {
    if (!(fenParts[3] == "-" || (islower(fenParts[3][0]) && isdigit(fenParts[3][1])))) {
      throw std::invalid_argument("FEN en passant contains invalid characters: " + fenParts[3]);
    }
    if (fenParts[3] != "-") {
      enPassantSquare = makeSquare(fenParts[3]);
      if (enPassantSquare == SQ_NONE) {
        throw std::invalid_argument("FEN en passant invalid square: " + fenParts[3]);
      }
    }
  }

  // half move clock (50 moves rule)
  if (fenParts.size() >= 5) {
    char* p;
    int tmp = strtol(fenParts[4].c_str(), &p, 10);
    if (*p) {
      throw std::invalid_argument("FEN half move clock is not a number: " + fenParts[4]);
    }
    halfMoveClock = tmp;
  }

  // move number
  if (fenParts.size() >= 6) {
    char* p;
    int tmp = strtol(fenParts[5].c_str(), &p, 10);
    if (*p) {
      throw std::invalid_argument("FEN move number is not a number: " + fenParts[5]);
    }
    if (tmp < 0) {
      throw std::invalid_argument("FEN move number is negative: " + fenParts[5]);
    }
    moveNumber = tmp;
  }
  // if move number is 0 set it to 1
  if (moveNumber == 0) moveNumber = 1;
}
