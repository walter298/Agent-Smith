export module Chess.Position:Zobrist;

export import std;

export import Chess.PieceType;
export import Chess.Square;
export import Chess.SafeInt;

export namespace chess {
	class Position;

	std::uint64_t getZobristPieceCode(Square square, Piece piece, bool isWhite);
	std::uint64_t getZobristCastleCode(SafeInt<std::uint8_t> whiteCastling, SafeInt<std::uint8_t> blackCastling);
	std::uint64_t getZobristDoubleJumpSquareCode(Square doubleJumpedPawnSquare);
	std::uint64_t getZobristTurnCode(bool isWhite);
	std::uint64_t getStartingZobristHash(const Position& pos);
}