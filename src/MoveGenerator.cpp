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

#include <regex>

#include "MoveGenerator.h"
#include "Position.h"
#include "Values.h"
#include "misc.h"
#include "types/types.h"

MoveGenerator::MoveGenerator() {
  pseudoLegalMoves.reserve(MAX_MOVES);
  legalMoves.reserve(MAX_MOVES);
  onDemandMoves.reserve(MAX_MOVES);
}

template<GenMode GM>
const MoveList* MoveGenerator::generatePseudoLegalMoves(const Position& p, bool evasion) {
  pseudoLegalMoves.clear();

  // when in check only generate moves either blocking or capturing the attacker
  if (evasion) {
    onDemandEvasionTargets = getEvasionTargets(p);
  }

  // first generate all non quiet moves
  if (GM == GenNonQuiet || GM == GenAll) {
    generatePawnMoves<GenNonQuiet>(p, &pseudoLegalMoves, evasion, onDemandEvasionTargets);
    generateMoves<GenNonQuiet>(p, &pseudoLegalMoves, evasion, onDemandEvasionTargets);
    generateKingMoves<GenNonQuiet>(p, &pseudoLegalMoves, evasion);
  }
  // second generate all other moves
  if (GM == GenQuiet || GM == GenAll) {
    generatePawnMoves<GenQuiet>(p, &pseudoLegalMoves, evasion, onDemandEvasionTargets);
    if (!evasion) {// no castling when in check
      generateCastling<GenQuiet>(p, &pseudoLegalMoves);
    }
    generateMoves<GenQuiet>(p, &pseudoLegalMoves, evasion, onDemandEvasionTargets);
    generateKingMoves<GenQuiet>(p, &pseudoLegalMoves, evasion);
  }

  // PV, Killer and history handling
  updateSortValues(p, &pseudoLegalMoves);

  // sort moves
  sort(pseudoLegalMoves.begin(), pseudoLegalMoves.end(), [](const Move lhs, const Move rhs) {
    return valueOf(lhs) > valueOf(rhs);
  });

  // remove internal sort value
  std::transform(pseudoLegalMoves.begin(), pseudoLegalMoves.end(),
                 pseudoLegalMoves.begin(), [](Move m) { return moveOf(m); });

  return &pseudoLegalMoves;
}

template<GenMode GM>
const MoveList* MoveGenerator::generateLegalMoves(const Position& p) {
  legalMoves.clear();
  generatePseudoLegalMoves<GM>(p, p.hasCheck());
  for (Move m : pseudoLegalMoves) {
    if (p.isLegalMove(m)) legalMoves.push_back(m);
  }
  return &legalMoves;
}

template<GenMode GM>
Move MoveGenerator::getNextPseudoLegalMove(const Position& p, bool evasion) {
  // if the position changes during iteration the iteration
  // will be reset and generation will be restart with the
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

  // If the list is currently empty and we have not generated all moves yet
  // generate the next batch until we have new moves or all moves are generated
  // and there are no more moves to generate
  if (onDemandMoves.empty()) {
    fillOnDemandMoveList<GM>(p, evasion);
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
    if (currentODStage != OD1 && pvMovePushed && moveOf(onDemandMoves[takeIndex]) == moveOf(pvMove)) {

      // skip pv move
      takeIndex++;

      // we found the pv move and skipped it
      // no need to check this for this generation cycle
      pvMovePushed = false;

      if (takeIndex >= onDemandMoves.size()) {
        // The pv move was the last move in this iterations list.
        // We will try to generate more moves. If no more moves
        // can be generated we will return MOVE_NONE.
        // Otherwise we return the move below.
        takeIndex = 0;
        onDemandMoves.clear();
        fillOnDemandMoveList<GM>(p, evasion);
        // no more moves - return MOVE_NONE
        if (onDemandMoves.empty()) {
          return MOVE_NONE;
        }
      }
    }
    assert(!onDemandMoves.empty() && "OnDemandList should not be empty here");

    // we have at least one move in the list and
    // it is not the pvMove.
    Move move = moveOf(onDemandMoves[takeIndex++]);
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

void MoveGenerator::setPV(Move move) {
  pvMove = moveOf(move);
}

void MoveGenerator::storeKiller(Move killerMove) {
  // check if already stored in first slot - if so return
  Move m = moveOf(killerMove);
  if (killerMoves[0] == m) {
    return;
  }
  else {// if in second slot or not there at all move it to first
    killerMoves[1] = killerMoves[0];
    killerMoves[0] = m;
  }
}

void MoveGenerator::setHistoryData(History* pHistory) {
  historyData = pHistory;
}

bool MoveGenerator::hasLegalMove(const Position& position) {
  // To determine if we have at least one legal move we only have to find
  // one legal move. We search for any KING, PAWN, KNIGHT, BISHOP, ROOK, QUEEN move
  // and return immediately if we found one.
  // The order of our search is from approx. the most likely to the least likely

  const Color us          = position.getNextPlayer();
  const Color them        = ~us;
  const Bitboard ourBb    = position.getOccupiedBb(us);
  const Bitboard theirBb  = position.getOccupiedBb(them);
  const Bitboard ourPawns = position.getPieceBb(us, PAWN);

  // KING
  const Square kingSquare = position.getKingSquare(us);
  Bitboard tmpMoves       = getAttacksBb(KING, kingSquare, BbZero) & ~ourBb;
  while (tmpMoves) {
    const Square toSquare = popLSB(tmpMoves);
    if (position.isLegalMove(createMove(kingSquare, toSquare))) return true;
  }

  // PAWN
  // pawn pushes - check step one to unoccupied squares
  // don't have to test double steps as they would be redundant to single steps
  // for the purpose of finding at least one legal move
  tmpMoves = shiftBb(pawnPush(us), ourPawns) & ~position.getOccupiedBb();
  while (tmpMoves) {
    const Square toSquare   = popLSB(tmpMoves);
    const Square fromSquare = toSquare + pawnPush(them);
    if (position.isLegalMove(createMove(fromSquare, toSquare))) return true;
  }

  // normal pawn captures to the west - promotions first
  tmpMoves = shiftBb(pawnPush(us) + WEST, ourPawns) & theirBb;
  while (tmpMoves) {
    const Square toSquare   = popLSB(tmpMoves);
    const Square fromSquare = toSquare + pawnPush(them) + EAST;
    if (position.isLegalMove(createMove(fromSquare, toSquare))) return true;
  }

  // normal pawn captures to the east - promotions first
  tmpMoves = shiftBb(pawnPush(us) + EAST, ourPawns) & theirBb;
  while (tmpMoves) {
    const Square toSquare   = popLSB(tmpMoves);
    const Square fromSquare = toSquare + pawnPush(them) + WEST;
    if (position.isLegalMove(createMove(fromSquare, toSquare))) return true;
  }

  // OFFICERS
  for (PieceType pt = KNIGHT; pt <= QUEEN; ++pt) {
    Bitboard pieces = position.getPieceBb(us, pt);
    while (pieces) {
      const Square fromSquare = popLSB(pieces);
      Bitboard moves          = getAttacksBb(pt, fromSquare, position.getOccupiedBb()) & ~ourBb;
      while (moves) {
        const Square toSquare = popLSB(moves);
        if (position.isLegalMove(createMove(fromSquare, toSquare))) return true;
      }
    }
  }

  // en passant captures
  const Square enPassantSquare = position.getEnPassantSquare();
  if (enPassantSquare != SQ_NONE) {
    // left
    tmpMoves = shiftBb(pawnPush(them) + WEST, Bitboards::sqBb[enPassantSquare]) & ourPawns;
    if (tmpMoves) {
      const Square fromSquare = lsb(tmpMoves);
      if (position.isLegalMove(createMove(fromSquare, fromSquare + pawnPush(us) + EAST, ENPASSANT))) {
        return true;
      }
    }
    // right
    tmpMoves = shiftBb(pawnPush(them) + EAST, Bitboards::sqBb[enPassantSquare]) & ourPawns;
    if (tmpMoves) {
      const Square fromSquare = lsb(tmpMoves);
      if (position.isLegalMove(createMove(fromSquare, fromSquare + pawnPush(us) + WEST, ENPASSANT))) {
        return true;
      }
    }
  }

  // no move found
  return false;
}

bool MoveGenerator::validateMove(const Position& position, const Move move) {
  const Move moveOf1 = moveOf(move);
  if (!moveOf1) return false;
  const MoveList* lm = generateLegalMoves<GenAll>(position);
  return std::find_if(lm->begin(), lm->end(), [&](Move m) { return (moveOf1 == moveOf(m)); }) != lm->end();
}


Move MoveGenerator::getMoveFromUci(const Position& position, std::string uciMove) {
  const std::regex regexUciMove(R"(([a-h][1-8][a-h][1-8])([NBRQnbrq])?)");
  std::smatch matcher;
  // Match the target string
  if (!std::regex_match(uciMove, matcher, regexUciMove)) {
    return MOVE_NONE;
  }
  // pattern is move
  std::string matchedMove = matcher.str(1);
  std::string promotion   = toUpperCase(matcher.str(2));
  // create all moves on position and compare
  MoveGenerator mg;
  const MoveList* legalMovesPtr = mg.generateLegalMoves<GenAll>(position);
  for (Move m : *legalMovesPtr) {
    if (::str(m) == matchedMove + promotion) {
      return m;
    }
  }
  return MOVE_NONE;
}

Move MoveGenerator::getMoveFromSan(const Position& position, std::string sanMove) {
  // Regex for short move notation (SAN)
  const std::regex regexPattern(R"(([NBRQK])?([a-h])?([1-8])?x?([a-h][1-8]|O-O-O|O-O)(=?([NBRQ]))?([!?+#]*)?)");
  std::smatch matcher;

  // Match the target string
  if (!std::regex_match(sanMove, matcher, regexPattern)) {
    return MOVE_NONE;
  }

  // get the parts
  std::string pieceType  = matcher.str(1);
  std::string disambFile = matcher.str(2);
  std::string disambRank = matcher.str(3);
  std::string toSq       = matcher.str(4);
  std::string promotion  = matcher.str(6);
  std::string checkSign  = matcher.str(7);

  // Generate all legal moves and loop through them to search for a matching move
  Move moveFromSAN{MOVE_NONE};
  int movesFound = 0;
  MoveGenerator mg;
  const MoveList* legalMovesPtr = mg.generateLegalMoves<GenAll>(position);
  for (Move m : *legalMovesPtr) {
    m = moveOf(m);
    // castling move
    if (typeOf(m) == CASTLING) {
      const Square kingToSquare = toSquare(m);
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
    const std::string& moveTarget = ::str(toSquare(m));
    if (moveTarget == toSq) {
      // Find out piece
      PieceType movePieceType = typeOf(position.getPiece(fromSquare(m)));
      const std::string pieceTypeChar(1, ::str(movePieceType));
      // determine if piece types match - if not skip
      if ((pieceType.empty() || pieceTypeChar != pieceType) && (!pieceType.empty() || movePieceType != PAWN)) {
        continue;
      }
      // Disambiguation File
      if (!disambFile.empty() && std::string(1, char('a' + fileOf(fromSquare(m)))) != disambFile) {
        continue;
      }
      // Disambiguation Rank
      if (!disambRank.empty() && std::string(1, char('1' + rankOf(fromSquare(m)))) != disambRank) {
        continue;
      }
      // promotion
      if (!promotion.empty()) {
        if (std::string(1, pieceToChar[promotionTypeOf(m)]) != promotion) {
          continue;
        }
      }
      // we should have our move if we end up here
      moveFromSAN = m;
      movesFound++;
    }
  }
  // we should only have one move here
  if (movesFound > 1) {
    return MOVE_NONE;
  }
  else if (movesFound == 0 || !validMove(moveFromSAN)) {
    return MOVE_NONE;
  }
  return moveFromSAN;
}

std::string MoveGenerator::str() {
  return std::string("To be implemented");
}

template<GenMode GM>
void MoveGenerator::fillOnDemandMoveList(const Position& position, bool evasion) {
  while (onDemandMoves.empty() && currentODStage < OD_END) {
    switch (currentODStage) {
      case OD_NEW:
        currentODStage = PV;
        // fall through
      case PV:
        // If a pvMove is set we return it first and filter it out before
        // returning a move
        assert(!pvMovePushed && "Stage PV should not have pvMovePushed set");
        if (pvMove) {
          switch (GM) {
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
          }
        }
        // decide which state we should continue with
        // captures or non captures or both
        if (GM == GenAll || GM == GenNonQuiet) {
          currentODStage = OD1;
        }
        else {
          currentODStage = OD4;
        }
        break;
      case OD1:// capture
        generatePawnMoves<GenNonQuiet>(position, &onDemandMoves, evasion, onDemandEvasionTargets);
        updateSortValues(position, &onDemandMoves);
        currentODStage = OD2;
        break;
      case OD2:
        generateMoves<GenNonQuiet>(position, &onDemandMoves, evasion, onDemandEvasionTargets);
        updateSortValues(position, &onDemandMoves);
        currentODStage = OD3;
        break;
      case OD3:
        generateKingMoves<GenNonQuiet>(position, &onDemandMoves, evasion);
        updateSortValues(position, &onDemandMoves);
        currentODStage = OD4;
        break;
      case OD4:
        if (GM == GenAll || GM == GenQuiet) {
          currentODStage = OD5;
        }
        else {
          currentODStage = OD_END;
        }
        break;
      case OD5:// non capture
        generatePawnMoves<GenQuiet>(position, &onDemandMoves, evasion, onDemandEvasionTargets);
        updateSortValues(position, &onDemandMoves);
        currentODStage = OD6;
        break;
      case OD6:
        if (!evasion) {
          generateCastling<GenQuiet>(position, &onDemandMoves);
          updateSortValues(position, &onDemandMoves);
        }
        currentODStage = OD7;
        break;
      case OD7:
        generateMoves<GenQuiet>(position, &onDemandMoves, evasion, onDemandEvasionTargets);
        updateSortValues(position, &onDemandMoves);
        currentODStage = OD8;
        break;
      case OD8:
        generateKingMoves<GenQuiet>(position, &onDemandMoves, evasion);
        updateSortValues(position, &onDemandMoves);
        currentODStage = OD_END;
        break;
      case OD_END:
        break;
    }
    // sort the list according to sort values encoded in the move
    if (!onDemandMoves.empty()) {
      sort(onDemandMoves.begin(), onDemandMoves.end(), [](const Move lhs, const Move rhs) {
        return valueOf(lhs) > valueOf(rhs);
      });
    }
  }// while onDemandMoves.empty()
}

void MoveGenerator::updateSortValues(const Position& p, MoveList* const moveList) {
  Color us = p.getNextPlayer();

  // iterate over all available moves and update the
  // sort value if the move is the PV or a Killer move.
  // Also update the sort value for history and counter
  // move significance.
  for (size_t i = 0; i < moveList->size(); i++) {
    Move* move = &(*moveList)[i];
    if (moveOf(*move) == pvMove)// PV move
      setValueOf(*move, VALUE_MAX);
    else if (moveOf(*move) == killerMoves[1])// Killer 2
      setValueOf(*move, static_cast<Value>(-4001));
    else if (moveOf(*move) == killerMoves[0])// Killer 1
      setValueOf(*move, static_cast<Value>(-4000));
    else if (historyData) {// historical search data

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
      // TODO:
      //       auto count = historyData.HistoryCount[us][move.From()][move.To()]
      //       value : = Value(count / 100)

      // Counter Move History
      // When we have a counter move which caused a beta cut off before we
      // bump up its sort value
      // TODO:
      //      if mg.historyData.CounterMoves[p.LastMove().From()][p.LastMove().To()] == move.MoveOf() {
      //        value += 500;
      //      }

      // update move sort value
      // TODO
      //      if (value > 0) {// only touch the value if it would be improved
      //        preValue = move.ValueOf()(*move).SetValue(preValue + value)
      //      }
    }
  }
}

Bitboard MoveGenerator::getEvasionTargets(const Position& p) const {
  const Color us       = p.getNextPlayer();
  const Square ourKing = p.getKingSquare(us);
  // find all target squares which either capture or block the attacker
  Bitboard evasionTargets = p.attacksTo(ourKing, ~us);
  // we can only block attacks of sliders of there is not more
  // than one attacker
  if (popcount(evasionTargets) == 1) {
    Square atck = lsb(evasionTargets);
    // sliding pieces
    if (typeOf(p.getPiece(atck)) > KNIGHT) {
      evasionTargets |= Bitboards::intermediateBb[atck][ourKing];
    }
  }
  return evasionTargets;
}

template<GenMode GM>
void MoveGenerator::generatePawnMoves(const Position& position, MoveList* const pMoves, bool evasion, Bitboard evasionTargets) {

  const Color nextPlayer = position.getNextPlayer();
  const Bitboard myPawns = position.getPieceBb(nextPlayer, PAWN);

  const Piece piece   = makePiece(nextPlayer, PAWN);
  const int gamePhase = position.getGamePhase();

  // captures
  if (GM == GenNonQuiet || GM == GenAll) {

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

    Bitboard tmpCaptures, promCaptures;

    for (Direction dir : {WEST, EAST}) {
      // normal pawn captures - promotions first
      tmpCaptures = shiftBb(pawnPush(nextPlayer) + dir, myPawns) & position.getOccupiedBb(~nextPlayer);

      // filter evasion targets if in check
      if (evasion) {
        tmpCaptures &= evasionTargets;
      }

      // normal pawn captures - promotions first
      promCaptures = tmpCaptures & Bitboards::rankBb[promotionRank(nextPlayer)];
      // promotion captures
      while (promCaptures) {
        const Square toSquare   = popLSB(promCaptures);
        const Square fromSquare = toSquare + pawnPush(~nextPlayer) - dir;
        // value is the delta of values from the two pieces involved minus the promotion value
        const Value value = valueOf(position.getPiece(toSquare)) - (2 * valueOf(PAWN));
        // add the possible promotion moves to the move list and also add value of the promoted piece type
        pMoves->push_back(createMove(fromSquare, toSquare, PROMOTION, QUEEN, value + valueOf(QUEEN)));
        pMoves->push_back(createMove(fromSquare, toSquare, PROMOTION, KNIGHT, value + valueOf(KNIGHT)));
        // rook and bishops are usually redundant to queen promotion (except in stale mate situations)
        // therefore we give them a lower sort order
        pMoves->push_back(createMove(fromSquare, toSquare, PROMOTION, ROOK, value + valueOf(ROOK) - static_cast<Value>(2000)));
        pMoves->push_back(createMove(fromSquare, toSquare, PROMOTION, BISHOP, value + valueOf(BISHOP) - static_cast<Value>(2000)));
      }

      tmpCaptures &= ~Bitboards::rankBb[promotionRank(nextPlayer)];
      while (tmpCaptures) {
        const Square toSquare   = popLSB(tmpCaptures);
        const Square fromSquare = toSquare + pawnPush(~nextPlayer) - dir;
        // value is the delta of values from the two pieces involved plus the positional value
        const Value value = valueOf(position.getPiece(toSquare)) - valueOf(position.getPiece(fromSquare)) + Values::posValue[piece][toSquare][gamePhase];
        pMoves->push_back(createMove(fromSquare, toSquare, NORMAL, value));
      }
    }

    // en passant captures
    const Square enPassantSquare = position.getEnPassantSquare();
    if (enPassantSquare != SQ_NONE) {
      for (Direction dir : {WEST, EAST}) {
        tmpCaptures = shiftBb(pawnPush(~nextPlayer) + dir, Bitboards::sqBb[enPassantSquare]) & myPawns;
        if (tmpCaptures) {
          Square fromSquare = lsb(tmpCaptures);
          Square toSquare   = fromSquare + pawnPush(nextPlayer) - dir;
          // value is the positional value of the piece at this game phase
          const Value value = Values::posValue[piece][toSquare][gamePhase];
          pMoves->push_back(createMove(fromSquare, toSquare, ENPASSANT, value));
        }
      }
    }

    // we treat Queen and Knight promotions as non quiet moves
    Bitboard promMoves = shiftBb(pawnPush(nextPlayer), myPawns) & ~position.getOccupiedBb() & Bitboards::rankBb[promotionRank(nextPlayer)];
    ;
    // filter evasion targets if in check
    if (evasion) {
      promMoves &= evasionTargets;
    }
    // single pawn steps - promotions first
    while (promMoves) {
      const Square toSquare   = popLSB(promMoves);
      const Square fromSquare = toSquare + pawnPush(~nextPlayer);
      // value for non captures is lowered by 10k
      const Value value = -valueOf(PAWN);
      // value is done manually for sorting of queen prom first, then knight and others
      pMoves->push_back(createMove(fromSquare, toSquare, PROMOTION, QUEEN, value + valueOf(QUEEN)));
      pMoves->push_back(createMove(fromSquare, toSquare, PROMOTION, KNIGHT, value + valueOf(KNIGHT)));
    }
  }

  // non captures
  if (GM == GenQuiet || GM == GenAll) {

    //  Move my pawns forward one step and keep all on not occupied squares
    //  Move pawns now on rank 3 (rank 6) another square forward to check for pawn doubles.
    //  Loop over pawns remaining on unoccupied squares and add moves.

    // When we are in check only evasion moves are generated. E.g. all moves need to
    // target these evasion squares. That is either capturing the attacker or blocking
    // a sliding attacker.

    // pawns - check step one to unoccupied squares
    Bitboard tmpMoves = shiftBb(pawnPush(nextPlayer), myPawns) & ~position.getOccupiedBb();

    // pawns double - check step two to unoccupied squares
    Bitboard tmpMovesDouble = shiftBb(pawnPush(nextPlayer), tmpMoves & Bitboards::rankBb[pawnDoubleRank(nextPlayer)]) & ~position.getOccupiedBb();

    // filter evasion targets if in check
    if (evasion) {
      tmpMoves &= evasionTargets;
      tmpMovesDouble &= evasionTargets;
    }

    // single pawn steps - promotions first
    Bitboard promMoves = tmpMoves & Bitboards::rankBb[promotionRank(nextPlayer)];
    while (promMoves) {
      const Square toSquare   = popLSB(promMoves);
      const Square fromSquare = toSquare + pawnPush(~nextPlayer);
      // value for non captures is lowered by 10k
      const Value value = -Value(12'000);
      // we treat Queen and Knight promotions as non quiet moves and they are generated above
      // rook and bishops are usually redundant to queen promotion (except in stale mate situations)
      // therefore we give them lower sort order
      pMoves->push_back(createMove(fromSquare, toSquare, PROMOTION, ROOK, value + valueOf(ROOK)));
      pMoves->push_back(createMove(fromSquare, toSquare, PROMOTION, BISHOP, value + valueOf(BISHOP)));
    }

    // double pawn steps
    while (tmpMovesDouble) {
      const Square toSquare = popLSB(tmpMovesDouble);
      // value is the positional value of the piece at this game phase
      const auto value = static_cast<Value>(-10'000) + Values::posValue[piece][toSquare][gamePhase];
      pMoves->push_back(createMove(toSquare + 2 * pawnPush(~nextPlayer), toSquare, NORMAL, value));
    }

    // normal single pawn steps
    tmpMoves = tmpMoves & ~Bitboards::rankBb[promotionRank(nextPlayer)];
    while (tmpMoves) {
      const Square toSquare   = popLSB(tmpMoves);
      const Square fromSquare = toSquare + pawnPush(~nextPlayer);
      // value is the positional value of the piece at this game phase
      const Value value = static_cast<Value>(-10000) + Values::posValue[piece][toSquare][gamePhase];
      pMoves->push_back(createMove(fromSquare, toSquare, NORMAL, value));
    }
  }
}

template<GenMode GM>
void MoveGenerator::generateMoves(const Position& position, MoveList* const pMoves, bool evasion, Bitboard evasionTargets) {
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
      const Square fromSquare    = popLSB(pieces);
      const Bitboard pseudoMoves = getAttacksBb(pt, fromSquare, occupiedBb);

      // captures
      if (GM == GenNonQuiet || GM == GenAll) {
        Bitboard captures = pseudoMoves & position.getOccupiedBb(~nextPlayer);
        if (evasion) {
          captures &= evasionTargets;
        }
        while (captures) {
          const Square toSquare = popLSB(captures);
          const Value value     = valueOf(position.getPiece(toSquare)) - valueOf(position.getPiece(fromSquare)) + Values::posValue[piece][toSquare][gamePhase];
          pMoves->push_back(createMove(fromSquare, toSquare, NORMAL, value));
        }
      }

      // non captures
      if (GM == GenQuiet || GM == GenAll) {
        Bitboard nonCaptures = pseudoMoves & ~occupiedBb;
        if (evasion) {
          nonCaptures &= evasionTargets;
        }
        while (nonCaptures) {
          const Square toSquare = popLSB(nonCaptures);
          const Value value     = static_cast<Value>(-10000) + Values::posValue[piece][toSquare][gamePhase];
          pMoves->push_back(createMove(fromSquare, toSquare, NORMAL, value));
        }
      }
    }
  }
}

template<GenMode GM>
void MoveGenerator::generateKingMoves(const Position& position, MoveList* const pMoves, bool evasion) {
  const Color nextPlayer = position.getNextPlayer();
  const Piece piece      = makePiece(nextPlayer, KING);
  const int gamePhase    = position.getGamePhase();
  Bitboard kingSquareBb  = position.getPieceBb(nextPlayer, KING);
  assert(popcount(kingSquareBb) == 1 && "Only exactly one king allowed!");
  const Square fromSquare = popLSB(kingSquareBb);

  // attacks include all moves no matter if the king would be in check
  const Bitboard pseudoMoves = getAttacksBb(KING, fromSquare, BbZero);

  // captures
  if (GM == GenNonQuiet || GM == GenAll) {
    Bitboard captures = pseudoMoves & position.getOccupiedBb(~nextPlayer);
    while (captures) {
      const Square toSquare = popLSB(captures);
      // value is the positional value of the piece at this game phase minus the
      // value of the captured piece
      if (!evasion || !position.attacksTo(toSquare, ~nextPlayer)) {
        const Value value = valueOf(position.getPiece(toSquare)) - valueOf(position.getPiece(fromSquare)) + Values::posValue[piece][toSquare][gamePhase];
        pMoves->push_back(createMove(fromSquare, toSquare, NORMAL, value));
      }
    }
  }

  // non captures
  if (GM == GenQuiet || GM == GenAll) {
    Bitboard nonCaptures = pseudoMoves & ~position.getOccupiedBb();
    while (nonCaptures) {
      const Square toSquare = popLSB(nonCaptures);
      // value is the positional value of the piece at this game phase
      if (!evasion || !position.attacksTo(toSquare, ~nextPlayer)) {
        const Value value = static_cast<Value>(-10000) + Values::posValue[piece][toSquare][gamePhase];
        pMoves->push_back(createMove(fromSquare, toSquare, NORMAL, value));
      }
    }
  }
}

template<GenMode GM>
void MoveGenerator::generateCastling(const Position& position, MoveList* const pMoves) {
  const Color nextPlayer    = position.getNextPlayer();
  const Bitboard occupiedBb = position.getOccupiedBb();

  // castling - pseudo castling - we will not check if we are in check after the move
  // or if we have passed an attacked square with the king or if the king has been in check

  if ((GM == GenQuiet || GM == GenAll) && position.getCastlingRights()) {

    const CastlingRights cr = position.getCastlingRights();
    if (nextPlayer == WHITE) {// white
      if (cr == WHITE_OO && !(Bitboards::intermediateBb[SQ_E1][SQ_H1] & occupiedBb)) {
        pMoves->push_back(createMove(SQ_E1, SQ_G1, CASTLING, static_cast<Value>(-5000)));
      }
      if (cr == WHITE_OOO && !(Bitboards::intermediateBb[SQ_E1][SQ_A1] & occupiedBb)) {
        pMoves->push_back(createMove(SQ_E1, SQ_C1, CASTLING, static_cast<Value>(-5000)));
      }
    }
    else {// black
      if (cr == BLACK_OO && !(Bitboards::intermediateBb[SQ_E8][SQ_H8] & occupiedBb)) {
        pMoves->push_back(createMove(SQ_E8, SQ_G8, CASTLING, static_cast<Value>(-5000)));
      }
      if (cr == BLACK_OOO && !(Bitboards::intermediateBb[SQ_E8][SQ_A8] & occupiedBb)) {
        pMoves->push_back(createMove(SQ_E8, SQ_C8, CASTLING, static_cast<Value>(-5000)));
      }
    }
  }
}

// explicitly instantiate all template definitions so other classes can see them
template const MoveList* MoveGenerator::generatePseudoLegalMoves<GenNonQuiet>(const Position& position, bool evasion);
template const MoveList* MoveGenerator::generatePseudoLegalMoves<GenQuiet>(const Position& position, bool evasion);
template const MoveList* MoveGenerator::generatePseudoLegalMoves<GenAll>(const Position& position, bool evasion);

template const MoveList* MoveGenerator::generateLegalMoves<GenNonQuiet>(const Position& position);
template const MoveList* MoveGenerator::generateLegalMoves<GenQuiet>(const Position& position);
template const MoveList* MoveGenerator::generateLegalMoves<GenAll>(const Position& position);

template Move MoveGenerator::getNextPseudoLegalMove<GenNonQuiet>(const Position& position, bool evasion);
template Move MoveGenerator::getNextPseudoLegalMove<GenQuiet>(const Position& position, bool evasion);
template Move MoveGenerator::getNextPseudoLegalMove<GenAll>(const Position& position, bool evasion);

template void MoveGenerator::generatePawnMoves<GenNonQuiet>(const Position& position, MoveList* const pMoves, bool evasion, Bitboard evasionTargets);
template void MoveGenerator::generatePawnMoves<GenQuiet>(const Position& position, MoveList* const pMoves, bool evasion, Bitboard evasionTargets);
template void MoveGenerator::generatePawnMoves<GenAll>(const Position& position, MoveList* const pMoves, bool evasion, Bitboard evasionTargets);

template void MoveGenerator::generateMoves<GenNonQuiet>(const Position& position, MoveList* const pMoves, bool evasion, Bitboard evasionTargets);
template void MoveGenerator::generateMoves<GenQuiet>(const Position& position, MoveList* const pMoves, bool evasion, Bitboard evasionTargets);
template void MoveGenerator::generateMoves<GenAll>(const Position& position, MoveList* const pMoves, bool evasion, Bitboard evasionTargets);

template void MoveGenerator::generateKingMoves<GenNonQuiet>(const Position& position, MoveList* const pMoves, bool evasion);
template void MoveGenerator::generateKingMoves<GenQuiet>(const Position& position, MoveList* const pMoves, bool evasion);
template void MoveGenerator::generateKingMoves<GenAll>(const Position& position, MoveList* const pMoves, bool evasion);

template void MoveGenerator::generateCastling<GenNonQuiet>(const Position& position, MoveList* const pMoves);
template void MoveGenerator::generateCastling<GenQuiet>(const Position& position, MoveList* const pMoves);
template void MoveGenerator::generateCastling<GenAll>(const Position& position, MoveList* const pMoves);
