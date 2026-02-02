module;

#include <tracy/Tracy.hpp>

module Chess.Evaluation;

import std;

import Chess.Position.PieceState;

import :Constants;
import :Material;
import :PawnStructure;
import :PieceDevelopment;
import :KingSafety;

namespace chess {
	Rating calcAttackRating(const Position& pos, const PositionData& posData) {
		auto [white, black] = pos.getColorSides();
		
		auto getAttackedPiecesRating = [&](const PieceState& pieceState, Bitboard enemySquares) -> Rating {
			auto attackedPieces = pieceState.calcAllLocations() & enemySquares;
			auto attackedPieceCount = std::popcount(attackedPieces);
			return Rating{ static_cast<Rating::Int>(attackedPieceCount) } * ATTACKED_PIECE_RATING;
		};
		auto allWhiteSquares = posData.whiteSquares.destSquaresPinConsidered;
		auto allBlackSquares = posData.blackSquares.destSquaresPinConsidered;
		return getAttackedPiecesRating(white, allBlackSquares) - getAttackedPiecesRating(black, allWhiteSquares);
	}

	Rating calcCastleRating(const Position& pos) {
		auto [white, black] = pos.getColorSides();

		auto getCastleRating = [](const auto& pieceState) {
			return pieceState.castling.hasCastledKingside() || pieceState.castling.hasCastledQueenside() ? CASTLE_RATING : 0_rt;
		};
		return getCastleRating(white) - getCastleRating(black);
	}

	Rating staticEvaluation(const Position& pos, const PositionData& posData) {
		ZoneScoped;

		auto castleRating = calcCastleRating(pos);
		auto materialRating = calcMaterialRating(pos);
		auto pawnStructureRating = calcPawnStructureRating(pos);
		auto attackRating = calcAttackRating(pos, posData);
		auto kingSafetyRating = calcKingSafetyRating(pos, posData);
		auto developmentRating = calcPieceDevelopmentRating(pos, posData);

		return castleRating + materialRating + pawnStructureRating + attackRating + kingSafetyRating + developmentRating;
	}

	Rating getPieceRating(Piece piece) {
		return pieceRatings[piece];
	}
}
