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

#include <ranges>
#include <regex>

#include "History.h"
#include "MoveGenerator.h"
#include "Values.h"
#include "chesscore/Position.h"
#include "types/types.h"

static constexpr bool REMOVE_SORT_VALUE = true;

MoveGenerator::MoveGenerator() :
  currentODStage(OD_NEW) {
  // StaticMoveList has fixed capacity - no reserve() needed
}

const MoveList* MoveGenerator::generatePseudoLegalMoves(const Position& p, const GenMode genMode, const bool evasion) {
  pseudoLegalMoves.clear();

  // when in check only generate moves either blocking or capturing the attacker
  if (evasion) {
    assert(p.hasCheck() && "move generator called with evasion true but not in check");
    onDemandEvasionTargets = getEvasionTargets(p);
  }

  // first generate all non quiet moves
  if (genMode & GenNonQuiet) {
    generatePawnMoves(p, &pseudoLegalMoves, GenNonQuiet, evasion, onDemandEvasionTargets);
    generateMoves(p, &pseudoLegalMoves, GenNonQuiet, evasion, onDemandEvasionTargets);
    generateKingMoves(p, &pseudoLegalMoves, GenNonQuiet, evasion);
  }
  // second generate all other moves
  if (genMode & GenQuiet) {
    generatePawnMoves(p, &pseudoLegalMoves, GenQuiet, evasion, onDemandEvasionTargets);
    if (!evasion) {// no castling when in check
      generateCastling(p, &pseudoLegalMoves, GenQuiet);
    }
    generateMoves(p, &pseudoLegalMoves, GenQuiet, evasion, onDemandEvasionTargets);
    generateKingMoves(p, &pseudoLegalMoves, GenQuiet, evasion);
  }

  // PV, Killer and history handling
  updateSortValues(p, &pseudoLegalMoves);

  // sort moves
  std::ranges::stable_sort(pseudoLegalMoves, moveValueGreaterComparator());

  // remove internal sort value
  if (REMOVE_SORT_VALUE) {
    std::ranges::for_each(pseudoLegalMoves, [](Move& m) { m = m.stripped(); });
  }

  return &pseudoLegalMoves;
}

const MoveList* MoveGenerator::generateLegalMoves(const Position& p, const GenMode genMode) {
  legalMoves.clear();
  generatePseudoLegalMoves(p, genMode, p.hasCheck());
  for (Move m : pseudoLegalMoves) {
    if (p.isLegalMove(m)) legalMoves.push_back(m);
  }
  return &legalMoves;
}

Move MoveGenerator::getNextPseudoLegalMove(const Position& p, const GenMode genMode, const bool evasion) {
  // if the position changes during iteration the iteration
  // will be reset and generation will be restarted with the
  // new position.
  if (p.getZobristKey() != currentODZobrist) {
    onDemandMoves.clear();
    onDemandEvasionTargets = BbZero;
    currentODStage         = OD_NEW;
    pvMovePushed           = false;
    takeIndex              = 0;
    currentODZobrist       = p.getZobristKey();
  }

  // when in check only generate moves either blocking or capturing the attacker
  if (evasion && !onDemandEvasionTargets) {
    onDemandEvasionTargets = getEvasionTargets(p);
  }

  // ad takeIndex
  // With the takeIndex we can take from the front of the vector
  // without removing the element from the vector which would
  // be expensive as all elements would have to be shifted.

  // If the list is currently empty, and we have not generated all moves yet
  // generate the next batch until we have new moves or all moves are generated
  // and there are no more moves to generate
  if (onDemandMoves.empty()) {
    fillOnDemandMoveList(p, genMode, evasion);
  }

  // If we have generated moves we will return the first move and
  // increase the takeIndex to the next move. If the list is emtpy
  // even after all stages of generating we have no more moves
  // and return MOVE_NONE
  // If we have pushed a pvMove into the list we will need to
  // skip this pvMove for each subsequent phases.
  if (!onDemandMoves.empty()) {
    // Handle PvMove
    // if we pushed a pv move and the list is not empty we check if the pv is the
    // next move in list and skip it.
    if (currentODStage != PAWN_CAPTURES && pvMovePushed && onDemandMoves[takeIndex].stripped() == pvMove.stripped()) {

      // skip pv move
      takeIndex++;

      // we found the pv move and skipped it
      // no need to check this for this generation cycle
      pvMovePushed = false;

      if (takeIndex >= onDemandMoves.size()) {
        // The pv move was the last move in this iterations list.
        // We will try to generate more moves. If no more moves
        // can be generated we will return MOVE_NONE.
        // Otherwise, we return the move below.
        takeIndex = 0;
        onDemandMoves.clear();
        fillOnDemandMoveList(p, genMode, evasion);
        // no more moves - return MOVE_NONE
        if (onDemandMoves.empty()) {
          return MOVE_NONE;
        }
      }
    }
    assert(!onDemandMoves.empty() && "OnDemandList should not be empty here");

    // we have at least one move in the list, and it is not the pvMove.
    // ReSharper disable once CppDFAUnreachableCode
    const Move move = REMOVE_SORT_VALUE ? onDemandMoves[takeIndex++].stripped() : onDemandMoves[takeIndex++];
    if (takeIndex >= onDemandMoves.size()) {
      takeIndex = 0;
      onDemandMoves.clear();
    }
    return move;
  }

  // no more moves to be generated
  takeIndex    = 0;
  pvMovePushed = false;
  return MOVE_NONE;
}

void MoveGenerator::setPV(const Move move) {
  pvMove = move.stripped();
}

void MoveGenerator::storeKiller(const Move killerMove) {
  const Move m = killerMove.stripped();
  if (killerMoves[0] == m) {
    return;
  }
  killerMoves[1] = killerMoves[0];
  killerMoves[0] = m;
}

void MoveGenerator::setHistoryData(History* pHistory) {
  historyData = pHistory;
}

bool MoveGenerator::validateMove(const Position& position, const Move move) {
  const Move base = move.stripped();
  if (!base) return false;
  const MoveList* lm = generateLegalMoves(position, GenAll);
  return std::ranges::find_if(*lm, [&](const Move m) { return base == m.stripped(); }) != lm->end();
}

bool MoveGenerator::hasLegalMove(const Position& position) {
  // To determine if we have at least one legal move we only have to find
  // one legal move. We search for any KING, PAWN, KNIGHT, BISHOP, ROOK, QUEEN move
  // and return immediately if we found one.
  // The order of our search is from approx. the most likely to the least likely

  const Color us = position.getNextPlayer();

  // KING
  const Square kingSquare = position.getKingSquare(us);
  const Bitboard ourBb    = position.getOccupiedBb(us);
  Bitboard tmpMoves       = Attacks::attacks(KING, kingSquare, BbZero) & ~ourBb;
  while (tmpMoves) {
    const Square toSquare = tmpMoves.popLSB();
    if (position.isLegalMove(Move(kingSquare, toSquare))) return true;
  }

  const Color them        = ~us;
  const Bitboard ourPawns = position.getPieceBb(us, PAWN);
  const Bitboard occupied = position.getOccupiedBb();

  // PAWNS

  // pawns - check step one to unoccupied squares
  tmpMoves                = ourPawns.shifted(Direction::pawnPush(us)) & ~occupied;
  Bitboard tmpMovesDouble = (tmpMoves & Bitboards::rankBb[Rank::pawnDoubleFor(us)]).shifted(Direction::pawnPush(us)) & ~occupied;

  while (tmpMoves) {
    const Square toSquare   = tmpMoves.popLSB();
    const Square fromSquare = toSquare.pawnPush(them);
    if (position.isLegalMove(Move(fromSquare, toSquare))) return true;
  }

  // pawns double - check step two to unoccupied squares
  while (tmpMovesDouble) {
    const Square toSquare   = tmpMovesDouble.popLSB();
    const Square fromSquare = toSquare + 2 * Direction::pawnPush(them);
    if (position.isLegalMove(Move(fromSquare, toSquare))) return true;
  }

  const Bitboard theirBb = position.getOccupiedBb(them);

  // normal pawn captures to the west - promotions first
  tmpMoves = ourPawns.shifted(Direction::pawnPush(us) + WEST) & theirBb;
  while (tmpMoves) {
    const Square toSquare   = tmpMoves.popLSB();
    const Square fromSquare = toSquare.pawnPush(them) - WEST;
    if (position.isLegalMove(Move(fromSquare, toSquare))) return true;
  }

  // normal pawn captures to the east - promotions first
  tmpMoves = ourPawns.shifted(Direction::pawnPush(us) + EAST) & theirBb;
  while (tmpMoves) {
    const Square toSquare   = tmpMoves.popLSB();
    const Square fromSquare = toSquare.pawnPush(them) - EAST;
    if (position.isLegalMove(Move(fromSquare, toSquare))) return true;
  }

  // OFFICERS
  for (PieceType pt = KNIGHT; pt <= QUEEN; ++pt) {
    Bitboard pieces = position.getPieceBb(us, pt);
    while (pieces) {
      const Square fromSquare = pieces.popLSB();
      Bitboard moves          = Attacks::attacks(pt, fromSquare, occupied) & ~ourBb;
      while (moves) {
        const Square toSquare = moves.popLSB();
        if (position.isLegalMove(Move(fromSquare, toSquare))) return true;
      }
    }
  }

  // en passant captures
  const Square enPassantSquare = position.getEnPassantSquare();
  if (enPassantSquare != SQ_NONE) {
    for (const Direction dir : {WEST, EAST}) {
      tmpMoves = Bitboards::sqBb[enPassantSquare].shifted(Direction::pawnPush(them) + dir) & ourPawns;
      if (tmpMoves) {
        const Square fromSquare = tmpMoves.lsb();
        const Square toSquare   = fromSquare + Direction::pawnPush(us) - dir;
        if (position.isLegalMove(Move::enPassant(fromSquare, toSquare))) {
          return true;
        }
      }
    }
  }

  // no move found
  return false;
}

Move MoveGenerator::getMoveFromUci(const Position& position, const std::string& uciMove) {
  // check move format
  if (!((uciMove.size() == 4 && islower(uciMove[0]) && isdigit(uciMove[1]) && islower(uciMove[2]) && isdigit(uciMove[3])) || (uciMove.size() == 5 && islower(uciMove[0]) && isdigit(uciMove[1]) && islower(uciMove[2]) && isdigit(uciMove[3]) && isalpha(uciMove[4])))) {
    return MOVE_NONE;
  }
  // create all moves on position and compare
  Move move;
  resetOnDemand();// in case this is called multiple times on the same position
  while ((move = getNextPseudoLegalMove(position, GenAll, position.hasCheck())) != MOVE_NONE) {
    // to lower case is necessary as UCI uses lower case characters for promotions
    // although "algebraic notation" defines upper case letters.
    if (toLowerCase(move.str()) == toLowerCase(uciMove) && position.isLegalMove(move)) {
      return move;
    }
  }
  return MOVE_NONE;
}

Move MoveGenerator::getMoveFromSan(const Position& position, const std::string& sanMove) {
  enum Part {
    PROM,
    TO_SQ,
    FROM,
    PIECE
  };

  // get the parts
  std::string pieceType;
  std::string disambFile;
  std::string disambRank;
  std::string toSq;
  std::string promotion;

  // backwards scan
  Part part                     = PROM;
  int index                     = static_cast<int>(sanMove.size()) - 1;
  const std::string nonrelevant = "x=!?+#.p ";

  while (index >= 0) {

    // skip non relevant characters
    if (nonrelevant.find(sanMove[index]) != std::string::npos) {
      index--;
      continue;
    }

    switch (part) {
      case PROM:
        // promotion is optional
        if (isupper(sanMove[index])) {
          promotion = sanMove[index--];
        }
        part = TO_SQ;
        break;
      case TO_SQ:
        // check if string is long enough is a letter following a digit
        {
          // check if string is long enough is a letter following a digit
          if (sanMove == "O-O-O" || sanMove == "O-O") {
            toSq  = sanMove;
            index = -1;
            break;
          }
          if (index >= 1 && isdigit(sanMove[index]) && islower(sanMove[index - 1])) {
            toSq += sanMove[index - 1];
            toSq += sanMove[index];
            index -= 2;
            part = FROM;
          }
          else if (sanMove[index] == 'e') {
            // if the move has e.p. at the end this is needs to be ignored here
            // . and p are ignored above - but e could also be a file and can only caught here
            index--;
          }
          else {
            // no target square - invalid
            return MOVE_NONE;
          }
        }
        break;
      case FROM:
        // check if the lower letter and digit or either one alone
        if (index >= 1 && isdigit(sanMove[index]) && islower(sanMove[index - 1])) {
          disambRank = sanMove[index--];
          disambFile = sanMove[index--];
        }
        else if (isdigit(sanMove[index])) {
          disambRank = sanMove[index--];
        }
        else if (islower(sanMove[index])) {
          disambFile = sanMove[index--];
        }
        part = PIECE;
        break;
      case PIECE:
        // piece type - empty for pawn
        if (isupper(sanMove[index])) {
          pieceType = sanMove[index--];
        }
        break;
    }
  }

  // Generate all legal moves and loop through them to search for a matching move
  // we can't return early if we found a move as we could find several moves which would
  // mean the provided move string is ambiguous.
  Move moveFromSAN{MOVE_NONE};
  int movesFound                = 0;
  const MoveList* legalMovesPtr = generateLegalMoves(position, GenAll);
  for (Move m : *legalMovesPtr) {

    // castling move
    if (m.type() == CASTLING) {
      const Square kingToSquare = m.to();
      std::string castlingString;
      switch (kingToSquare) {
        case SQ_G1:// white king side
        case SQ_G8:// black king side
          castlingString = "O-O";
          break;
        case SQ_C1:// white queen side
        case SQ_C8:// black queen side
          castlingString = "O-O-O";
          break;
        default:
          continue;
      }
      if (castlingString == toSq) {
        moveFromSAN = m;
        movesFound++;
        continue;
      }
    }

    // normal move
    const std::string& moveTarget = m.to().str();
    if (moveTarget == toSq) {
      // Find out piece
      PieceType movePieceType = typeOf(position.getPiece(m.from()));
      const std::string pieceTypeChar(1, ::str(movePieceType));
      // determine if piece types match - if not skip
      if ((pieceType.empty() || pieceTypeChar != pieceType) && (!pieceType.empty() || movePieceType != PAWN)) {
        continue;
      }
      // Disambiguation File
      if (!disambFile.empty() && std::string(1, m.from().file().str()) != disambFile) {
        continue;
      }
      // Disambiguation Rank
      if (!disambRank.empty() && std::string(1, m.from().rank().str()) != disambRank) {
        continue;
      }
      // promotion
      if (!promotion.empty()) {
        if (std::string(1, pieceToChar[m.promotionType()]) != promotion) {
          continue;
        }
      }
      // we should have our move if we end up here
      moveFromSAN = m;
      movesFound++;
    }
  }
  // we should only have one move here
  if (movesFound > 1 || (movesFound == 0 || !moveFromSAN.isValid())) {
    return MOVE_NONE;
  }
  return moveFromSAN;
}

std::string MoveGenerator::str() {
  return {"To be implemented"};
}

void MoveGenerator::fillOnDemandMoveList(const Position& position, const GenMode genMode, const bool evasion) {
  while (onDemandMoves.empty() && currentODStage < OD_END) {
    switch (currentODStage) {
      case OD_NEW:
        currentODStage = PV_MOVE;
        [[fallthrough]];
      case PV_MOVE:
        // If a pvMove is set we return it first and filter it out before
        // returning a move
        assert(!pvMovePushed && "Stage PV should not have pvMovePushed set");
        if (pvMove) {
          switch (genMode) {
            case GenAll:
              pvMovePushed = true;
              onDemandMoves.push_back(pvMove);
              break;
            case GenNonQuiet:
              if (position.isCapturingMove(pvMove)) {
                pvMovePushed = true;
                onDemandMoves.push_back(pvMove);
              }
              break;
            case GenQuiet:
              if (!position.isCapturingMove(pvMove)) {
                pvMovePushed = true;
                onDemandMoves.push_back(pvMove);
              }
              break;
            default:
              break;
          }
        }
        // decide which state we should continue with
        // captures or non captures or both
        if (genMode & GenNonQuiet) {
          currentODStage = PAWN_CAPTURES;
        }
        else {
          currentODStage = QUIET_SWITCH;
        }
        break;
      case PAWN_CAPTURES:// capture
        generatePawnMoves(position, &onDemandMoves, GenNonQuiet, evasion, onDemandEvasionTargets);
        updateSortValues(position, &onDemandMoves);
        currentODStage = OFFICER_CAPTURES;
        break;
      case OFFICER_CAPTURES:
        generateMoves(position, &onDemandMoves, GenNonQuiet, evasion, onDemandEvasionTargets);
        updateSortValues(position, &onDemandMoves);
        currentODStage = KING_CAPTURES;
        break;
      case KING_CAPTURES:
        generateKingMoves(position, &onDemandMoves, GenNonQuiet, evasion);
        updateSortValues(position, &onDemandMoves);
        currentODStage = QUIET_SWITCH;
        break;
      case QUIET_SWITCH:
        if (genMode & GenQuiet) {
          currentODStage = PAWN_MOVES;
        }
        else {
          currentODStage = OD_END;
        }
        break;
      case PAWN_MOVES:// non capture
        generatePawnMoves(position, &onDemandMoves, GenQuiet, evasion, onDemandEvasionTargets);
        updateSortValues(position, &onDemandMoves);
        currentODStage = CASTLING_MOVES;
        break;
      case CASTLING_MOVES:
        if (!evasion) {
          generateCastling(position, &onDemandMoves, GenQuiet);
          updateSortValues(position, &onDemandMoves);
        }
        currentODStage = OFFICER_MOVES;
        break;
      case OFFICER_MOVES:
        generateMoves(position, &onDemandMoves, GenQuiet, evasion, onDemandEvasionTargets);
        updateSortValues(position, &onDemandMoves);
        currentODStage = KING_MOVES;
        break;
      case KING_MOVES:
        generateKingMoves(position, &onDemandMoves, GenQuiet, evasion);
        updateSortValues(position, &onDemandMoves);
        currentODStage = OD_END;
        break;
      case OD_END:
        break;
    }
    // sort the list according to sort values encoded in the move
    if (!onDemandMoves.empty()) {
      std::ranges::stable_sort(onDemandMoves, moveValueGreaterComparator());
    }
  }// while onDemandMoves.empty()
}

void MoveGenerator::updateSortValues(const Position& p, MoveList* const moveList) const {
  const Color us = p.getNextPlayer();

  // iterate over all available moves and update the
  // sort value if the move is the PV or a Killer move.
  // Also update the sort value for history and counter
  // move significance.
  const auto size = moveList->size();
  for (size_t i = 0; i < size; i++) {
    Move* move = &(*moveList)[i];
    if (move->stripped() == pvMove) { // PV move
      move->setValue(VALUE_MAX);
    } else if (move->stripped() == killerMoves[1]) { // Killer 2
      move->setValue(static_cast<Value>(1000));
    } else if (move->stripped() == killerMoves[0]) { // Killer 1
      move->setValue(static_cast<Value>(1001));
    } else if (historyData) {// historical search data

      // History Count
      // Moves that cause a beta cut in the search get an increasing value
      // which favors many repetitions and deep searches.
      // We use the history count to improve the sort value of a move
      // If and how much a sort value has to be improved for a move is
      // difficult to predict - this needs testing and experimentation.
      // The current way is a hard cut for values <1000 and then 1 point
      // per 1000 count points.
      // It is also yet unclear if the history count table should be
      // reused for several consecutive searches or just for one search.
      // TODO: Testing
      const auto count = historyData->historyCount[us][move->from()][move->to()];
      auto value       = static_cast<Value>(count / 100);

      // Counter Move History
      // When we have a counter move which caused a beta cut off before we
      // bump up its sort value
      // TODO: Testing
      if (historyData->counterMoves[p.getLastMove().from()][p.getLastMove().to()] == move->stripped()) {
        value = value + 500;
      }

      // update move sort value
      if (value > 0) {// only touch the value if it would be improved
        move->setValue(move->value() + value);
      }
    }
  }
}

Bitboard MoveGenerator::getEvasionTargets(const Position& p) {
  const Color us       = p.getNextPlayer();
  const Square ourKing = p.getKingSquare(us);
  // find all target squares that either capture or block the attacker
  Bitboard evasionTargets = p.attacksTo(ourKing, ~us);
  assert(evasionTargets != BbZero && "evasion target should not be empty");
  // we can only block attacks of sliders if there is not more
  // than one attacker
  const int popCount = evasionTargets.popcount();
  if (popCount == 1) {
    const Square atck = evasionTargets.lsb();
    // sliding pieces
    if (typeOf(p.getPiece(atck)) > KNIGHT) {
      evasionTargets |= Bitboards::intermediateBb[atck][ourKing];
    }
    return evasionTargets;
  }
  if (popCount > 1) {
    return BbZero;
  }
  return evasionTargets;
}

void MoveGenerator::generatePawnMoves(const Position& position, MoveList* const pMoves, const GenMode genMode, const bool evasion, const Bitboard evasionTargets) {

  const Color nextPlayer = position.getNextPlayer();
  const Bitboard myPawns = position.getPieceBb(nextPlayer, PAWN);

  const Piece piece   = makePiece(nextPlayer, PAWN);
  const int gamePhase = position.getGamePhase();

  // captures
  if (genMode & GenNonQuiet) {

    // This algorithm shifts the own pawn bitboard in the direction of pawn captures
    // and ANDs it with the opponents pieces. With this we get all possible captures
    // and can easily create the moves by using a loop over all captures and using
    // the backward shift for the from-Square.
    // All moves get sort values so that sort order should be:
    //   captures: most value victim least value attacker - promotion piece value
    //   non captures: promotions, castling, normal moves (position value)

    // When we are in check only evasion moves are generated. E.g. all moves need to
    // target these evasion squares. That is either capturing the attacker or blocking
    // a sliding attacker.

    Bitboard tmpCaptures;

    for (const Direction dir : {WEST, EAST}) {
      // normal pawn captures
      tmpCaptures = myPawns.shifted(Direction::pawnPush(nextPlayer) + dir) & position.getOccupiedBb(~nextPlayer);

      // filter evasion targets if in check
      if (evasion) {
        tmpCaptures &= evasionTargets;
      }

      // normal pawn captures - promotions first
      Bitboard promCaptures = tmpCaptures & Bitboards::rankBb[Rank::promotionFor(nextPlayer)];
      // promotion captures
      while (promCaptures) {
        const Square toSquare   = promCaptures.popLSB();
        const Square fromSquare = toSquare + Direction::pawnPush(~nextPlayer) - dir;
        // value is the delta of values from the two pieces involved minus the promotion value
        const Value value = valueOf(position.getPiece(toSquare)) - (2 * valueOf(PAWN));
        // add the possible promotion moves to the move list and also add value of the promoted piece type
        pMoves->push_back(Move::promotion(fromSquare, toSquare, QUEEN, value + valueOf(QUEEN) + 5000));
        pMoves->push_back(Move::promotion(fromSquare, toSquare, KNIGHT, value + valueOf(KNIGHT) + 1500));
        pMoves->push_back(Move::promotion(fromSquare, toSquare, ROOK, value + valueOf(ROOK) - 5000));
        pMoves->push_back(Move::promotion(fromSquare, toSquare, BISHOP, value + valueOf(BISHOP) - 5000));
      }

      tmpCaptures &= ~Bitboards::rankBb[Rank::promotionFor(nextPlayer)];
      while (tmpCaptures) {
        const Square toSquare   = tmpCaptures.popLSB();
        const Square fromSquare = toSquare + Direction::pawnPush(~nextPlayer) - dir;
        // value is the delta of values from the two pieces involved plus the positional value
        const Value value = valueOf(position.getPiece(toSquare)) - valueOf(position.getPiece(fromSquare)) + Values::posValue[piece][toSquare][gamePhase];
        pMoves->push_back(Move::normal(fromSquare, toSquare, value));
      }
    }

    // en passant captures
    const Square enPassantSquare = position.getEnPassantSquare();
    if (enPassantSquare != SQ_NONE) {
      for (const Direction dir : {WEST, EAST}) {
        tmpCaptures = Bitboards::sqBb[enPassantSquare].shifted(Direction::pawnPush(~nextPlayer) + dir) & myPawns;
        if (tmpCaptures) {
          const Square fromSquare = tmpCaptures.lsb();
          const Square toSquare   = fromSquare + Direction::pawnPush(nextPlayer) - dir;// target square behind the captured pawn
          const Value value       = Values::posValue[piece][toSquare][gamePhase];
          pMoves->push_back(Move::enPassant(fromSquare, toSquare, value));
        }
      }
    }

    // we treat Queen and Knight promotions as non-quiet moves
    Bitboard promMoves = myPawns.shifted(Direction::pawnPush(nextPlayer)) & ~position.getOccupiedBb() & Bitboards::rankBb[Rank::promotionFor(nextPlayer)];

    // filter evasion targets if in check
    if (evasion) {
      promMoves &= evasionTargets;
    }
    // single pawn steps - promotions first
    while (promMoves) {
      const Square toSquare   = promMoves.popLSB();
      const Square fromSquare = toSquare + Direction::pawnPush(~nextPlayer);
      // value is done manually for sorting of queen prom first, then knight and others
      pMoves->push_back(Move::promotion(fromSquare, toSquare, QUEEN, 2000 - valueOf(PAWN) + valueOf(QUEEN)));
      pMoves->push_back(Move::promotion(fromSquare, toSquare, KNIGHT, 1500 - valueOf(PAWN) + valueOf(KNIGHT)));
    }
  }

  // non captures
  if (genMode & GenQuiet) {

    //  Move my pawns forward one step and keep all on not occupied squares
    //  Move pawns now on rank 3 (rank 6) another square forward to check for pawn doubles.
    //  Loop over pawns remaining on unoccupied squares and add moves.

    // When we are in check only evasion moves are generated. E.g. all moves need to
    // target these evasion squares. That is either capturing the attacker or blocking
    // a sliding attacker.

    // pawns - check step one to unoccupied squares
    Bitboard tmpMoves = myPawns.shifted(Direction::pawnPush(nextPlayer)) & ~position.getOccupiedBb();

    // pawns double - check step two to unoccupied squares
    Bitboard tmpMovesDouble = (tmpMoves & Bitboards::rankBb[Rank::pawnDoubleFor(nextPlayer)]).shifted(Direction::pawnPush(nextPlayer)) & ~position.getOccupiedBb();

    // filter evasion targets if in check
    if (evasion) {
      tmpMoves &= evasionTargets;
      tmpMovesDouble &= evasionTargets;
    }

    // single pawn steps - promotions first
    Bitboard promMoves = tmpMoves & Bitboards::rankBb[Rank::promotionFor(nextPlayer)];
    while (promMoves) {
      const Square toSquare   = promMoves.popLSB();
      const Square fromSquare = toSquare + Direction::pawnPush(~nextPlayer);
      // value for non captures is lowered
      // we treat Queen and Knight promotions as non-quiet moves, and they are generated above
      // rook and bishops are usually redundant to queen promotion (except in stalemate situations)
      // therefore we give them lower sort order
      pMoves->push_back(Move::promotion(fromSquare, toSquare, ROOK, valueOf(ROOK) - 6'000));
      pMoves->push_back(Move::promotion(fromSquare, toSquare, BISHOP, valueOf(BISHOP) - 6'000));
    }

    // double pawn steps
    while (tmpMovesDouble) {
      const Square toSquare = tmpMovesDouble.popLSB();
      // value is the positional value of the piece at this game phase
      const auto value = Values::posValue[piece][toSquare][gamePhase] - 2'000;
      pMoves->push_back(Move::normal(toSquare + 2 * Direction::pawnPush(~nextPlayer), toSquare, value));
    }

    // normal single pawn steps
    tmpMoves = tmpMoves & ~Bitboards::rankBb[Rank::promotionFor(nextPlayer)];
    while (tmpMoves) {
      const Square toSquare   = tmpMoves.popLSB();
      const Square fromSquare = toSquare + Direction::pawnPush(~nextPlayer);
      // value is the positional value of the piece at this game phase
      const Value value = Values::posValue[piece][toSquare][gamePhase] - 2'000;
      pMoves->push_back(Move::normal(fromSquare, toSquare, value));
    }
  }
}

void MoveGenerator::generateMoves(const Position& position, MoveList* const pMoves, const GenMode genMode, const bool evasion, const Bitboard evasionTargets) {
  const Color nextPlayer    = position.getNextPlayer();
  const Bitboard occupiedBb = position.getOccupiedBb();
  const int gamePhase       = position.getGamePhase();

  // Loop through all piece types, get attacks for the piece.
  // When we are in check (evasion=true) only evasion moves are generated. E.g. all
  // moves need to target these evasion squares. That is either capturing the
  // attacker or blocking a sliding attacker.

  for (PieceType pt = KNIGHT; pt <= QUEEN; ++pt) {
    Bitboard pieces   = position.getPieceBb(nextPlayer, pt);
    const Piece piece = makePiece(nextPlayer, pt);

    while (pieces) {
      const Square fromSquare    = pieces.popLSB();
      const Bitboard pseudoMoves = Attacks::attacks(pt, fromSquare, occupiedBb);

      // captures
      if (genMode & GenNonQuiet) {
        Bitboard captures = pseudoMoves & position.getOccupiedBb(~nextPlayer);
        if (evasion) {
          captures &= evasionTargets;
        }
        while (captures) {
          const Square toSquare = captures.popLSB();
          const Value value     = 2000 + valueOf(position.getPiece(toSquare)) - valueOf(position.getPiece(fromSquare)) + Values::posValue[piece][toSquare][gamePhase];
          pMoves->push_back(Move::normal(fromSquare, toSquare, value));
        }
      }

      // non captures
      if (genMode & GenQuiet) {
        Bitboard nonCaptures = pseudoMoves & ~occupiedBb;
        if (evasion) {
          nonCaptures &= evasionTargets;
        }
        while (nonCaptures) {
          const Square toSquare = nonCaptures.popLSB();
          const Value value     = Values::posValue[piece][toSquare][gamePhase] - 2000;
          pMoves->push_back(Move::normal(fromSquare, toSquare, value));
        }
      }
    }
  }
}

void MoveGenerator::generateKingMoves(const Position& position, MoveList* const pMoves, const GenMode genMode, const bool evasion) {
  const Color nextPlayer = position.getNextPlayer();
  const Piece piece      = makePiece(nextPlayer, KING);
  const int gamePhase    = position.getGamePhase();
  Bitboard kingSquareBb  = position.getPieceBb(nextPlayer, KING);
  assert(kingSquareBb.popcount() == 1 && "Only exactly one king allowed!");
  const Square fromSquare = kingSquareBb.popLSB();

  // attacks include all moves no matter if the king would be in check
  const Bitboard pseudoMoves = Attacks::attacks(KING, fromSquare, BbZero);

  // captures
  if (genMode & GenNonQuiet) {
    Bitboard captures = pseudoMoves & position.getOccupiedBb(~nextPlayer);
    while (captures) {
      const Square toSquare = captures.popLSB();
      if (!evasion) {
        const Value value = 2000 + valueOf(position.getPiece(toSquare)) - valueOf(position.getPiece(fromSquare)) + Values::posValue[piece][toSquare][gamePhase];
        pMoves->push_back(Move::normal(fromSquare, toSquare, value));
      }
      // when evasion only move to non attacked squares - will not check for x-ray attacks
      else if (!position.attacksTo(toSquare, ~nextPlayer)) {
        const Value value = 2000 + valueOf(position.getPiece(toSquare)) - valueOf(position.getPiece(fromSquare)) + Values::posValue[piece][toSquare][gamePhase];
        pMoves->push_back(Move::normal(fromSquare, toSquare, value));
      }
    }
  }

  // non captures
  if (genMode & GenQuiet) {
    Bitboard nonCaptures = pseudoMoves & ~position.getOccupiedBb();
    while (nonCaptures) {
      const Square toSquare = nonCaptures.popLSB();
      if (!evasion) {
        const Value value = Values::posValue[piece][toSquare][gamePhase] - 2'000;
        pMoves->push_back(Move::normal(fromSquare, toSquare, value));
      }
      // when evasion only move to non attacked squares - will not check for x-ray attacks
      else if (!position.attacksTo(toSquare, ~nextPlayer)) {
        const Value value = Values::posValue[piece][toSquare][gamePhase] - 2'000;
        pMoves->push_back(Move::normal(fromSquare, toSquare, value));
      }
    }
  }
}

void MoveGenerator::generateCastling(const Position& position, MoveList* const pMoves, const GenMode genMode) {
  const Color nextPlayer    = position.getNextPlayer();
  const Bitboard occupiedBb = position.getOccupiedBb();

  // castling - pseudo castling - we will not check if we are in check after the move
  // or if we have passed an attacked square with the king or if the king has been in check

  if ((genMode & GenQuiet) && position.getCastlingRights()) {

    const CastlingRights cr = position.getCastlingRights();
    if (nextPlayer == WHITE) {// white
      if (cr == WHITE_OO && !(Bitboards::intermediateBb[SQ_E1][SQ_H1] & occupiedBb)) {
        pMoves->push_back(Move::castling(SQ_E1, SQ_G1, VALUE_ZERO));
      }
      if (cr == WHITE_OOO && !(Bitboards::intermediateBb[SQ_E1][SQ_A1] & occupiedBb)) {
        pMoves->push_back(Move::castling(SQ_E1, SQ_C1, VALUE_ZERO));
      }
    }
    else {// black
      if (cr == BLACK_OO && !(Bitboards::intermediateBb[SQ_E8][SQ_H8] & occupiedBb)) {
        pMoves->push_back(Move::castling(SQ_E8, SQ_G8, VALUE_ZERO));
      }
      if (cr == BLACK_OOO && !(Bitboards::intermediateBb[SQ_E8][SQ_A8] & occupiedBb)) {
        pMoves->push_back(Move::castling(SQ_E8, SQ_C8, VALUE_ZERO));
      }
    }
  }
}
