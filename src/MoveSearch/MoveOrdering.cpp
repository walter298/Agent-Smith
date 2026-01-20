module Chess.MoveSearch:MoveOrdering;

import Chess.Assert;
import Chess.Rating;
import Chess.Evaluation;
import Chess.MoveGeneration;

import :PositionTable;

namespace chess {
	struct PieceData {
		Piece piece = Piece::None;
		Square square = Square::None;
	};

	template<typename NonPVMoves>
	auto orderCapturesAndEvasionsFirst(const PieceData& attackedPiece, const Position::ImmutableTurnData& turnData,
		Bitboard empty, NonPVMoves& priorities)
	{
		auto attackerData = calcAttackers(turnData.isWhite, turnData.enemies, empty, makeBitboard(attackedPiece.square));
		auto attackers = attackerData.attackers.calcAllLocations();

		auto pieceRating = getPieceRating(attackedPiece.piece);
		
		return std::ranges::stable_partition(priorities, [&](const MovePriority& p) {
			if (p.getExchangeRating() >= pieceRating) { //if we have a better capture, do it
				return true;
			}
			auto toBoard = makeBitboard(p.getMove().to);

			//if we are capturing one of the attackers or blocking it's rays
			return static_cast<bool>(toBoard & attackers) || static_cast<bool>(toBoard & attackerData.rays);
		});
	}

	std::generator<PieceData> getTargets(const PieceState& allies, Bitboard enemyDestSquares) {
		constexpr std::array MOST_VALUABLE_PIECES{ Queen, Rook, Bishop, Knight, Pawn };
		for (auto piece : MOST_VALUABLE_PIECES) {
			auto attackedAllies = allies[piece] & enemyDestSquares;
			if (!attackedAllies) {
				continue;
			}
			auto currSquare = Square::None;
			while (nextSquare(attackedAllies, currSquare)) {
				co_yield{ piece, currSquare };
			}
		}
	}

	template<typename NonMaterialMoves>
	auto orderKillerMovesFirst(std::span<const Move> killerMoves, NonMaterialMoves& nonMaterialMoves) {
		return std::ranges::partition(nonMaterialMoves, [&](const MovePriority& priority) {
			return std::ranges::any_of(killerMoves, [&](const Move& move) {
				return move == priority.getMove();
			});
		});
	}

	template<typename NonPVMoves>
	auto orderCapturesAndEvasionsFirst(const Node& node, Bitboard allEnemySquares, NonPVMoves& movePriorities) {
		std::ranges::subrange ret{ movePriorities.begin(), movePriorities.end() };

		auto turnData = node.getPos().getTurnData();
		
		auto empty = ~(turnData.enemies.calcAllLocations() | turnData.allies.calcAllLocations());
		for (auto attackedPiece : getTargets(turnData.allies, allEnemySquares)) {
			auto [pp, end] = orderCapturesAndEvasionsFirst(attackedPiece, turnData, empty, ret);
			ret = std::ranges::subrange{ pp, end };
		}

		return ret;
	}

	//returns non-PV moves
	auto movePVMoveToFront(arena::Vector<MovePriority>& priorities, Move pvMove) {
		if (pvMove != Move::null()) {
			auto pvMoveIt = std::ranges::find_if(priorities, [&](const MovePriority& p) {
				return p.getMove() == pvMove;
			});
			if (pvMoveIt != priorities.end()) {
				std::iter_swap(priorities.begin(), pvMoveIt);
				return std::ranges::subrange{ std::next(priorities.begin()), priorities.end() };
			}
		}
		return std::ranges::subrange{ priorities.begin(), priorities.end() };
	}

	arena::Vector<MovePriority> getMovePrioritiesImpl(const Node& node, const Move& pvMove, std::span<const Move> killerMoves) {
		zAssert(node.getRemainingDepth() > 0_su8);

		const auto& posData  = node.getPositionData();
		auto allEnemySquares = node.getPositionData().allEnemySquares().destSquaresPinConsidered;

		auto remainingDepth = node.getRemainingDepth();
		arena::Vector<MovePriority> priorities{ std::from_range, posData.legalMoves | std::views::transform([&](const Move& move) {
			return MovePriority{ move, allEnemySquares, remainingDepth };
		}) };

		std::ranges::sort(priorities, [](const MovePriority& a, const MovePriority& b) {
			return a.getExchangeRating() > b.getExchangeRating();
		});

		auto nonPVMoves = movePVMoveToFront(priorities, pvMove);
		auto nonMaterialMoves = orderCapturesAndEvasionsFirst(node, allEnemySquares, nonPVMoves);
		auto likelyBadMoves = orderKillerMovesFirst(killerMoves, nonMaterialMoves);

		if (remainingDepth - 1_su8 != 0_su8) {
			auto baseOffset = std::ranges::distance(priorities.begin(), likelyBadMoves.begin());
			for (auto&& [i, movePriority] : likelyBadMoves | std::views::enumerate) {
				SafeUnsigned indexOffset{ static_cast<std::uint8_t>(baseOffset + i) };
				movePriority.trim(indexOffset);
			}
		}
		
		zAssert(!priorities.empty());

		return priorities;
	}

	arena::Vector<MovePriority> getMovePriorities(const Node& node, const Move& pvMove, std::span<const Move> killerMoves) {
		return getMovePrioritiesImpl(node, pvMove, killerMoves);
	}
}