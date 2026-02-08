module;

#include <tracy/Tracy.hpp>

module Chess.Evaluation:KingSafety;

import std;

import Chess.Assert;
import Chess.Position.PieceState;
import Chess.SquareZone;
import :Constants;

namespace chess {
	class DistanceTable {
	private:
		std::uint8_t m_table[64][64];
	public:
		DistanceTable() {
			using std::views::cartesian_product;
			for (auto [a, b] : cartesian_product(SQUARE_ARRAY, SQUARE_ARRAY)) {
				auto [aFile, aRank] = fileRankOf(a);
				auto [bFile, bRank] = fileRankOf(b);
				auto dx = std::abs(aFile - bFile);
				auto dy = std::abs(aRank - bRank);
				m_table[static_cast<std::uint8_t>(a)][static_cast<std::uint8_t>(b)] = static_cast<std::uint8_t>(std::max(dx, dy));
			}
		}

		constexpr std::uint8_t operator()(Square a, Square b) const {
			zAssert(a != Square::None && b != Square::None);
			return m_table[static_cast<std::uint8_t>(a)][static_cast<std::uint8_t>(b)];
		}
	};

	DistanceTable distanceTable;

	double calcEnemyProximityPenaltyImpl(Square allyKingPos, Bitboard enemySquares, double penalty) {
		auto ret = 0.0;

		auto enemySquare = Square::None;
		while (nextSquare(enemySquares, enemySquare)) {
			auto distFromKing = distanceTable(allyKingPos, enemySquare);
			ret += penalty / std::max(static_cast<double>(distFromKing), 1.0);
		}

		return ret;
	}

	Rating calcEnemyProximityPenalty(Square allyKingPos, const PieceState& enemyPieces, Bitboard enemyDestSquares) {
		auto ret = 0.0;

		//penalize enemy pieces being close to the king
		auto pieceTypes = ALL_PIECE_TYPES | std::views::drop(1); //exclude king
		for (auto pieceType : pieceTypes) {
			auto enemyPieceLocations = enemyPieces[pieceType];
			auto penalty = static_cast<double>(pieceRatings[pieceType].get()) * PIECE_PROXIMITY_FACTOR;
			ret += calcEnemyProximityPenaltyImpl(allyKingPos, enemyPieceLocations, penalty);
		}

		//penalize enemy destination squares close to the king
		ret += calcEnemyProximityPenaltyImpl(allyKingPos, enemyDestSquares, DESTINATION_SQUARE_PROXIMITY_FACTOR);

		return Rating{ static_cast<Rating::Int>(ret) };
	}

	Rating calcKingSafetyRating(const Position& pos, const PositionData& posData) {
		ZoneScoped;

		auto ret = 0_rt;
		auto [white, black] = pos.getColorSides();

		auto whiteKingPos = nextSquare(white[King]);
		zAssert(whiteKingPos != Square::None);
		ret -= calcEnemyProximityPenalty(whiteKingPos, black, posData.blackSquares.destSquaresPinConsidered);

		auto blackKingPos = nextSquare(black[King]);
		zAssert(blackKingPos != Square::None);
		ret += calcEnemyProximityPenalty(blackKingPos, white, posData.whiteSquares.destSquaresPinConsidered);

		return ret;
	}
}