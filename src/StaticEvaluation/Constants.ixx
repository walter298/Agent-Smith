export module Chess.Evaluation:Constants;

import Chess.Rating;
import Chess.PieceMap;

namespace chess {
	constexpr auto QUEEN_RATING = 9000_rt;
	constexpr auto ROOK_RATING = 5000_rt;
	constexpr auto BISHOP_RATING = 3000_rt;
	constexpr auto KNIGHT_RATING = 3000_rt;
	constexpr auto PAWN_RATING = 1000_rt;
	constexpr auto PAWN_ADVANCEMENT_RATING = 1_rt;
	constexpr auto ATTACKED_PIECE_RATING = 2_rt;
	constexpr auto PAWN_ISLAND_PENALTY = -2_rt;
	constexpr auto PIECE_PROXIMITY_FACTOR = 0.001;
	constexpr auto DESTINATION_SQUARE_PROXIMITY_FACTOR = 0.001;
	constexpr auto CASTLE_RATING = 2_rt;
	constexpr auto OPTIMAL_KNIGHT_SQUARES = 5;
	constexpr auto OPTIMAL_BISHOP_SQUARES = 6;
	constexpr auto OPTIMAL_QUEEN_SQUARES = 6;
	constexpr auto OPTIMAL_ROOK_SQUARES = 6;
	constexpr auto MOBILITY_SQUARE_RATING = 10_rt;
	constexpr auto MOBILITY_DISTRIBUTION_RATING = 1.1;

	const PieceMap<int> optimalDestinationSquareCounts{
		{
		{ Queen, OPTIMAL_QUEEN_SQUARES },
		{ Rook, OPTIMAL_ROOK_SQUARES },
		{ Bishop, OPTIMAL_BISHOP_SQUARES },
		{ Knight, OPTIMAL_KNIGHT_SQUARES },
		{ Pawn, OPTIMAL_ROOK_SQUARES }
		}
	};

	const PieceMap<Rating> pieceRatings{
		{
			{ Queen, QUEEN_RATING },
			{ Rook, ROOK_RATING },
			{ Bishop, BISHOP_RATING },
			{ Knight, KNIGHT_RATING },
			{ Pawn, PAWN_RATING }
		}
	};
}