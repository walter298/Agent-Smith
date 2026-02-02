module Chess.Position:Zobrist;

import Chess.Assert;
import Chess.EasyRandom;
import Chess.PieceMap;

import :PositionObject;

namespace chess {
	struct PieceCodes {
		PieceMap<std::uint64_t> whiteCodes;
		PieceMap<std::uint64_t> blackCodes;
	};
	
	struct Codes {
		SquareMap<PieceCodes> pieceCodeMap;
		SquareMap<std::uint64_t> doubleJumpedPawnCodes;
		std::array<std::uint64_t, 256> castleCodeMap;
		std::uint64_t whiteToMoveCode = 0;
		std::uint64_t blackToMoveCode = 0;
	};

	Codes loadCodeMap() {
		Codes ret;

		auto randomFunc = [] {
			return makeRandomNum(0uz, std::numeric_limits<std::uint64_t>::max());
		};

		//make random numbers for the pieces
		for (auto& codes : ret.pieceCodeMap.get()) {
			std::ranges::generate(codes.whiteCodes, randomFunc);
			std::ranges::generate(codes.blackCodes, randomFunc);
		}

		//make random numbers for the castling map
		std::ranges::generate(ret.castleCodeMap, randomFunc);

		//make random numbers for en passant codes
		std::ranges::generate(ret.doubleJumpedPawnCodes.get(), randomFunc);

		//make random numbers for sides to move
		ret.whiteToMoveCode = randomFunc();
		ret.blackToMoveCode = randomFunc();

		return ret;
	}

	const auto codeMap = loadCodeMap();

	std::uint64_t getZobristPieceCode(Square square, Piece piece, bool white) {
		auto& codes = white ? codeMap.pieceCodeMap[square].whiteCodes : codeMap.pieceCodeMap[square].blackCodes;
		return codes[piece];
	}

	//NOTE: each castling # contains 4 pieces of information, whether the king has castle kingside/queenside and whether it CAN castle kingside/queenside
	std::uint64_t getZobristCastleCode(SafeInt<std::uint8_t> whiteCastling, SafeInt<std::uint8_t> blackCastling) {
		whiteCastling <<= 4;
		auto index = whiteCastling | blackCastling;
		return codeMap.castleCodeMap[static_cast<size_t>(index.get())];
	}

	std::uint64_t getZobristDoubleJumpSquareCode(Square doubleJumpedPawnSquare) {
		zAssert(doubleJumpedPawnSquare != Square::None);
		return codeMap.doubleJumpedPawnCodes[doubleJumpedPawnSquare];
	}

	std::uint64_t getZobristTurnCode(bool isWhite) {
		return isWhite ? codeMap.whiteToMoveCode : codeMap.blackToMoveCode;
	}

	std::uint64_t getStartingZobristHash(const Position& pos) {
		std::uint64_t hash = 0;

		//hash all the pieces on the board
		auto addPieceHashes = [&](Bitboard pieceLocations, Piece pieceType, bool isWhite) {
			auto square = Square::None;
			while (nextSquare(pieceLocations, square)) {
				hash ^= getZobristPieceCode(square, pieceType, isWhite);
			}
		};
		auto [white, black] = pos.getColorSides();
		for (auto piece : ALL_PIECE_TYPES) {
			addPieceHashes(white[piece], piece, true);
			addPieceHashes(black[piece], piece, false);
		}

		//hash the en passant square
		auto turnData = pos.getTurnData();
		if (turnData.enemies.doubleJumpedPawn != Square::None) {
			hash ^= codeMap.doubleJumpedPawnCodes[turnData.enemies.doubleJumpedPawn];
		}

		//hash castling rights
		hash ^= getZobristCastleCode(white.castling.get(), black.castling.get());

		//hash the side to move
		hash ^= getZobristTurnCode(turnData.isWhite);

		return hash;
	}
}