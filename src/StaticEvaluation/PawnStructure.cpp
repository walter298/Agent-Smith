module Chess.Evaluation:PawnStructure;

import Chess.RankCalculator;
import Chess.PositionCommand;
import :Constants;

namespace chess {
	template<bool MovingDown = false>
	Rating calcPawnAdvancementRatingImpl(Bitboard pawns) {
		Rating ret = 0_rt;

		auto currSquare = Square::None;
		while (nextSquare(pawns, currSquare)) {
			auto rank = rankOf(currSquare) + 1;
			if constexpr (MovingDown) {
				rank = 9 - rank;
			}
			auto pawnValue = Rating{ static_cast<Rating::Int>(rank) } * PAWN_ADVANCEMENT_RATING;
			ret += pawnValue;
		}

		return ret;
	}

	Rating calcPawnAdvancementRating(const Position& pos) {
		auto [white, black] = pos.getColorSides();
		return calcPawnAdvancementRatingImpl(white[Pawn]) - calcPawnAdvancementRatingImpl<true>(black[Pawn]);
	}

	template<std::invocable<Bitboard> FilePred>
	std::optional<int> getNextFileIndex(int fileIndex, FilePred pred) {
		if (fileIndex > 7) {
			return std::nullopt;
		}
		auto nthFile = calcFile(fileIndex);
		if (pred(nthFile)) {
			return fileIndex;
		}
		return getNextFileIndex(fileIndex + 1, pred);
	}

	Rating calcPawnIslandRating(Bitboard pawns) {
		auto islandCount = 0;
		auto currFileIndex = 0;

		auto noPawnsOnFile = [&](Bitboard file) {
			return (file & pawns) == 0;
		};
		auto pawnsOnFile = std::not_fn(noPawnsOnFile);

		while (true) {
			auto leftIslandBorderIndex = getNextFileIndex(currFileIndex, pawnsOnFile);
			if (!leftIslandBorderIndex) {
				break;
			}
			islandCount++;

			auto nextRightEmptyBorderIndex = getNextFileIndex(*leftIslandBorderIndex + 1, noPawnsOnFile);
			if (!nextRightEmptyBorderIndex) {
				break;
			}
			currFileIndex = *nextRightEmptyBorderIndex + 1;
		}

		if (islandCount > 1) {
			return PAWN_ISLAND_PENALTY * Rating{ static_cast<Rating::Int>(islandCount) };
		} else {
			return 0_rt;
		}
	}

	Rating calcPawnIslandRating(const Position& pos) {
		auto [white, black] = pos.getColorSides();
		return calcPawnIslandRating(white[Pawn]) - calcPawnIslandRating(black[Pawn]);
	}

	Rating calcPawnStructureRating(const Position& pos) {
		return calcPawnAdvancementRating(pos) + calcPawnIslandRating(pos);
	}

	void testPawnIslandRating() {
		Position oneWhitePawnIsland;
		oneWhitePawnIsland.setPos(parsePositionCommand("fen 4k3/8/2n5/8/8/8/3PPP2/4K3 w - - 0 1"));
		auto onePawnIslandRating = calcPawnIslandRating(oneWhitePawnIsland);

		Position twoWhitePawnIslands;
		twoWhitePawnIslands.setPos(parsePositionCommand("fen 4k3/8/2n5/8/8/8/2P1PP2/4K3 w - - 0 1"));
		auto twoPawnIslandRating = calcPawnIslandRating(twoWhitePawnIslands);

		Position threeWhitePawnIslands;
		threeWhitePawnIslands.setPos(parsePositionCommand("fen 4k3/8/2n5/8/8/8/2P1P1P1/4K3 w - - 0 1"));
		auto threePawnIslandRating = calcPawnIslandRating(threeWhitePawnIslands);

		if (!(threePawnIslandRating < twoPawnIslandRating && twoPawnIslandRating < onePawnIslandRating)) {
			std::println("Pawn island rating test failed");
			std::println("Three pawn islands: {}", threePawnIslandRating.get());
			std::println("Two pawn islands: {}", twoPawnIslandRating.get());
			std::println("One pawn island {}", onePawnIslandRating.get());
		}
	}

	void runPawnStructureTests() {
		testPawnIslandRating();
	}
}