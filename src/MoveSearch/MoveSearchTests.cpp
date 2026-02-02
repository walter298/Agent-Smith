module Chess.MoveSearch:MoveSearchTests;

import Chess.Position;
import Chess.PositionCommand;
import :MoveOrdering;

namespace chess {
	namespace tests {
		void printPriorities(const StaticVector<MovePriority>& priorities) {
			for (const auto& priority : priorities) {
				auto [move, depth] = std::tuple{ priority.getMove(), priority.getDepth() };
				std::println("[{}, {}]", move.getUCIString(), static_cast<unsigned int>(depth.get()));
			}
		}

		void testMoveOrdering() {
			//Position pos;
			//pos.setPos(parsePositionCommand("fen rnbq1k1r/3p1ppp/1p1b1n1Q/pBp1p3/4P2P/N2P3R/PPP2PP1/R1B1K1N1 b Q - 2 8"));
			//RepetitionMap rMap;

			//auto node = Node::makeRoot(pos, 1_su8, rMap);
			//auto priorities = getMovePriorities(node, Move::null());

			//if (priorities[0].getMove().to != Square::H6) {
			//	std::println("testMoveOrdering failed: queen capture is not the best move");
			//	printPriorities(priorities);
			//}
		}

		void testMoveOrdering2() {
//			Position pos;
//			pos.setPos(parsePositionCommand("fen rnbqkbnr/1ppp1ppp/4p3/p7/3P3B/2P2N2/PP2BPPP/R2QR1K1 b kq - 9 16"));
//			auto node = Node::makeRoot(pos, 1_su8, false);
//			const auto& legalMoves = node.getPositionData().legalMoves;
//			for (const auto& move : legalMoves) {
//				std::println("{}", move.getUCIString());
//			}
//			auto priorities = getMovePriorities(node, Move::null());
//			printPriorities(priorities);
		}

		void runInternalMoveSearchTests() {
			testMoveOrdering();
			testMoveOrdering2();
		}
	}
}