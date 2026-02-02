module Chess.Evaluation:Material;

import Chess.Position.PieceState;

import :Constants;

namespace chess {
	Rating getPieceRating(const PieceState& pieces) {
		auto ret = 0_rt;

		for (const auto& piece : ALL_PIECE_TYPES | std::views::drop(1)) { //don't count the king
			ret += pieceRatings[piece] * Rating{ static_cast<Rating::Int>(std::popcount(pieces[piece])) };
		}

		return ret;
	}

	Rating calcMaterialRating(const Position& pos) {
		auto [white, black] = pos.getColorSides();
		auto whiteMaterial = getPieceRating(white);
		auto blackMaterial = getPieceRating(black);

		//std::println("White: {}; Black: {}", whiteMaterial.get(), blackMaterial.get());
		return whiteMaterial - blackMaterial;
	}
}