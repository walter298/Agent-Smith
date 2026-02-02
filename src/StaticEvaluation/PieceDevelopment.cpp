module;

#include <tracy/Tracy.hpp>

module Chess.Evaluation:PieceDevelopment;

import :Constants;

import Chess.DebugPrint;
import Chess.MoveGeneration;

namespace chess {
	Rating calcPieceDevelopmentRatingImpl(const SquareMap<PieceDestinationSquareData>& destSquareMap) {
		auto ret = 0.0;
		auto developedPieceCount = 0;

		for (auto square : SQUARE_ARRAY) {
			const auto& destSquareData = destSquareMap[square];
			if (destSquareData.piece == Piece::None) {
				continue;
			}
			auto squareCount = std::popcount(destSquareData.destSquares.all());
			auto mobilityScore = static_cast<double>((squareCount - optimalDestinationSquareCounts[destSquareData.piece]) * 
				static_cast<int>(MOBILITY_SQUARE_RATING.get()));
			ret += mobilityScore;
			if (mobilityScore > 0) {
				developedPieceCount++;
				ret *= static_cast<double>(developedPieceCount) * MOBILITY_DISTRIBUTION_RATING;
			}
		}

		return Rating{ static_cast<Rating::Int>(ret) };
	}

	Rating calcPieceDevelopmentRating(const Position& pos, const PositionData& posData) {
		ZoneScoped;

		auto [whiteDestSquareMap, blackDestSquareMap] = calcDestinationSquareMap(pos, posData);
		return calcPieceDevelopmentRatingImpl(whiteDestSquareMap) - calcPieceDevelopmentRatingImpl(blackDestSquareMap);
	}
}