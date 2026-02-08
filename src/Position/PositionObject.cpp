module;

#include <boost/functional/hash.hpp>

module Chess.Position:PositionObject;

import Chess.Position.RepetitionMap;

import Chess.Assert;
import :Parse;
import :Zobrist;

namespace chess {
    void Position::setPos(const PositionCommandParseResult& positionCommand) {
        m_whitePieces.clear();
        m_blackPieces.clear();

        parseBoard(positionCommand.board, m_whitePieces, m_blackPieces);
        m_isWhiteMoving = (positionCommand.color == 'w');
        parseCastlingPrivileges(positionCommand.castlingPrivileges, m_whitePieces, m_blackPieces);
        parseEnPessantSquare(positionCommand.enPessantSquare, m_isWhiteMoving, m_isWhiteMoving ? m_blackPieces : m_whitePieces);

        m_zobristHash = getStartingZobristHash(*this);
    }

    bool Position::tryCastle(Position::MutableTurnData& turnData, const Move& move) {
        //if we can't castle eitherway or we're just not moving the king, don't try to castle
        if ((!turnData.allies.castling.canCastleKingside() && !turnData.allies.castling.canCastleQueenside()) || move.movedPiece != King) {
            return false;
        }

        //as soon as we move the king, we can't castle anymore
        turnData.allies.castling.disallowQueensideCastling();
        turnData.allies.castling.disallowKingsideCastling();

        auto updateZobrist = [&move, this](Square rookFrom, Square rookTo) {
            m_zobristHash ^= getZobristPieceCode(move.from, King, m_isWhiteMoving);
            m_zobristHash ^= getZobristPieceCode(move.to, King, m_isWhiteMoving);
            m_zobristHash ^= getZobristPieceCode(rookFrom, Rook, m_isWhiteMoving);
            m_zobristHash ^= getZobristPieceCode(rookTo, Rook, m_isWhiteMoving);
        };
        if (turnData.allyKingside.kingTo == move.to) {
            moveSquare(turnData.allies[King], move.from, turnData.allyKingside.kingTo);
            moveSquare(turnData.allies[Rook], turnData.allyKingside.rookFrom, turnData.allyKingside.rookTo);
            updateZobrist(turnData.allyKingside.rookFrom, turnData.allyKingside.rookTo);
            return true;
        } else if (turnData.allyQueenside.kingTo == move.to) {
            moveSquare(turnData.allies[King], move.from, turnData.allyQueenside.kingTo);
            moveSquare(turnData.allies[Rook], turnData.allyQueenside.rookFrom, turnData.allyQueenside.rookTo);
            updateZobrist(turnData.allyQueenside.rookFrom, turnData.allyQueenside.rookTo);
            return true;
        }
        return false;
    }

    bool isPawnDoubleJump(const Position::MutableTurnData& turnData, const Move& move) {
        return (makeBitboard(move.from) & turnData.allyPawnRank) && (makeBitboard(move.to) & turnData.jumpedAllyPawnRank);
    }

    void Position::movePawn(const MutableTurnData& turnData, const Move& move, Bitboard& pawns) {
        if (isPawnDoubleJump(turnData, move)) {
            turnData.allies.doubleJumpedPawn = move.to;
            m_zobristHash ^= getZobristDoubleJumpSquareCode(move.to);
        }
        if (move.promotionPiece != Piece::None) {
            addSquare(turnData.allies[move.promotionPiece], move.to);
            m_zobristHash ^= getZobristPieceCode(move.to, move.promotionPiece, turnData.isWhite);
        } else {
            addSquare(pawns, move.to);
            m_zobristHash ^= getZobristPieceCode(move.to, move.movedPiece, turnData.isWhite);
        }
    }

    void Position::capturePiece(const MutableTurnData& turnData, const Move& move) {
        if (move.capturedPiece == Rook) {
            if (move.to == turnData.enemyKingside.rookFrom) {
                turnData.enemies.castling.disallowKingsideCastling();
            } else if (move.to == turnData.enemyQueenside.rookFrom) {
                turnData.enemies.castling.disallowQueensideCastling();
            }
        }
        if (move.capturedPawnSquareEnPassant == Square::None) {
            removeSquare(turnData.enemies[move.capturedPiece], move.to);
            m_zobristHash ^= getZobristPieceCode(move.to, move.capturedPiece, !turnData.isWhite);
        } else {
            removeSquare(turnData.enemies[move.capturedPiece], move.capturedPawnSquareEnPassant);
            m_zobristHash ^= getZobristPieceCode(move.capturedPawnSquareEnPassant, Pawn, !turnData.isWhite);
        }
    }

    void Position::normalMove(MutableTurnData& turnData, const Move& move) {
        //disallow castling on one side if we are moving a rook from its starting square
        if (move.movedPiece == Rook) {
            if (move.from == turnData.allyKingside.rookFrom) {
                turnData.allies.castling.disallowKingsideCastling();
            } else if (move.from == turnData.allyQueenside.rookFrom) {
                turnData.allies.castling.disallowQueensideCastling();
            }
        } else if (move.movedPiece == King) {
            turnData.allies.castling.disallowKingsideCastling();
            turnData.allies.castling.disallowQueensideCastling();
        }

        //move the piece (destination square handled with pawn promotions)
        auto& movedPiecePos = turnData.allies[move.movedPiece];
        removeSquare(turnData.allies[move.movedPiece], move.from);
        m_zobristHash ^= getZobristPieceCode(move.from, move.movedPiece, m_isWhiteMoving);

        //capture the piece!
        if (move.capturedPiece != Piece::None) {
            capturePiece(turnData, move);
        }
       
        if (move.movedPiece == Pawn) {
            movePawn(turnData, move, turnData.allies[Pawn]);
        } else {
            addSquare(movedPiecePos, move.to);
            m_zobristHash ^= getZobristPieceCode(move.to, move.movedPiece, m_isWhiteMoving);
        }
    }


    void Position::verify() const {
        auto [white, black] = getColorSides();

        auto verifyPieceCounts = [](const PieceState& pieces) {
            zAssert(std::popcount(pieces[King]) == 1);
            zAssert(std::popcount(pieces[Pawn]) < 9);
            zAssert(std::popcount(pieces[Knight]) < 11);
            zAssert(std::popcount(pieces[Rook]) < 11);
            zAssert(std::popcount(pieces[Bishop]) < 11);
            zAssert(std::popcount(pieces[Queen]) < 11);
            zAssert(std::popcount(pieces.calcAllLocations()) < 17);
        };
        verifyPieceCounts(white);
        verifyPieceCounts(black);
    }

    void Position::move(const Move& move) {
        auto [white, black] = getColorSides();
        auto oldCastlingZobristCode = getZobristCastleCode(white.castling.get(), black.castling.get());
        auto oldPlayerMover = m_isWhiteMoving;

        auto turnData = getTurnData();
        if (!tryCastle(turnData, move)) {
            normalMove(turnData, move);
        }

        //reset enemy jumped pawn
        if (turnData.enemies.doubleJumpedPawn != Square::None) {
            m_zobristHash ^= getZobristDoubleJumpSquareCode(turnData.enemies.doubleJumpedPawn); 
            turnData.enemies.doubleJumpedPawn = Square::None;
        }

        //alternate turns
        m_isWhiteMoving = !m_isWhiteMoving; 
        m_zobristHash ^= getZobristTurnCode(oldPlayerMover);
        m_zobristHash ^= getZobristTurnCode(m_isWhiteMoving);

        //update castling hash
        m_zobristHash ^= oldCastlingZobristCode;
        m_zobristHash ^= getZobristCastleCode(white.castling.get(), black.castling.get());
    }

    bool isEnPessant(const Move& move) {
        if (move.movedPiece != Pawn || move.capturedPiece != Piece::None) {
            return false;
        }
        auto fromFile = calcFile(fileOf(move.from));
        auto destFile = calcFile(fileOf(move.to));
        return fromFile != destFile;
    }

    bool Position::setEnPassant(Move& move, const ImmutableTurnData& turnData) const {
        if (isEnPessant(move)) {
            move.capturedPawnSquareEnPassant = turnData.enemies.doubleJumpedPawn;
            move.capturedPiece = Pawn;
            return true;
        }
        return false;
    }

    bool Position::setEnPassant(Move& move) const {
        auto turnData = getTurnData();
        return setEnPassant(move, turnData);
    }

    void Position::move(std::string_view moveStr) {
        zAssert(moveStr.size() == 4 || moveStr.size() == 5);

        auto move = Move::null();

        auto from = parseSquare(moveStr.substr(0, 2));
        auto to   = parseSquare(moveStr.substr(2, 2));
        zAssert(from && to);
        move.from = *from;
        move.to = *to;

        auto turnData = getTurnData();

        move.movedPiece = turnData.allies.findPiece(*from);
        zAssert(move.movedPiece != Piece::None);

        move.capturedPiece = turnData.enemies.findPiece(*to);

        //detect en passant capture
        setEnPassant(move, turnData);

        if (moveStr.size() == 5) { //if there is a pawn promotion
            move.promotionPiece = parsePiece(moveStr[4]);
        }
        this->move(move);
    }
}