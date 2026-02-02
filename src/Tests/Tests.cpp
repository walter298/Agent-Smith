module;

#include <magic_enum/magic_enum.hpp>

#define assert_equality(value, expected) \
	{ \
		auto getPrintableValue = [&](auto a) { \
			using namespace std::literals; \
			if constexpr (std::is_enum_v<decltype(a)>) { \
				return magic_enum::enum_name(a); \
			} else if constexpr (std::same_as<Bitboard, decltype(a)>) {\
				return std::format("{:#x}", a);\
			} else if constexpr (std::is_pointer_v<decltype(a)>) {\
				if (a == nullptr) { \
					return "nullptr"s; \
				} else { \
					return std::format("{}", reinterpret_cast<uintptr_t>(a)); \
				}\
			} else { \
				return a; \
			} \
		};\
		if ((value != expected)) { \
			std::print("Test failed on line {}: {} != {}.", std::stacktrace::current()[0].source_line(), #value, #expected); \
			std::println(" Expected value: {}; Actual value: {}", getPrintableValue(expected), getPrintableValue(value)); \
		} \
	} \

module Chess.Tests;

import std;

import Chess.Arena;
import Chess.BitboardImage;
import Chess.Evaluation;
import Chess.Position;
import Chess.PositionCommand;
import Chess.MoveGeneration;
import Chess.MoveSearch;
import Chess.Position.RepetitionMap;
import Chess.SafeInt;

import :Pipe;

namespace chess {
	namespace tests {
		AsyncSearch& getSearchFunction() {
			static AsyncSearch search;
			return search;
		}
		//illegal move!
		//2025-10-26 00:28:26.320-->1:position startpos moves e2e4 a7a5 d2d4 d7d5 e4d5 d8d5 c2c4

		void testStartPos() {
			Position pos;
			pos.setPos(parsePositionCommand("startpos"));

			auto turnData = pos.getTurnData();
			assert_equality(turnData.isWhite, true);

			auto [white, black] = pos.getColorSides();
			assert_equality(&turnData.allies, &white);
			assert_equality(&turnData.enemies, &black);

			//test pawn locations
			assert_equality(turnData.allies[Pawn], 0x000000000000FF00);
			assert_equality(turnData.enemies[Pawn], 0x00FF000000000000);

			//test king locations
			assert_equality(nextSquare(turnData.allies[King]), Square::E1);
			assert_equality(nextSquare(turnData.enemies[King]), Square::E8);

			//test castling privileges
			assert_equality(turnData.allies.castling.canCastleKingside(), true);
			assert_equality(turnData.allies.castling.canCastleQueenside(), true);
			assert_equality(turnData.enemies.castling.canCastleKingside(), true);
			assert_equality(turnData.enemies.castling.canCastleQueenside(), true);

			//test en pessant square
			assert_equality(turnData.enemies.doubleJumpedPawn, Square::None);

			//test rook locations
			assert_equality(turnData.allies[Rook], 0x0000000000000081);
			assert_equality(turnData.enemies[Rook], 0x8100000000000000);

			//test knight locations
			assert_equality(turnData.allies[Knight], 0x0000000000000042);
			assert_equality(turnData.enemies[Knight], 0x4200000000000000);

			//test bishop locations
			assert_equality(turnData.allies[Bishop], 0x0000000000000024);
			assert_equality(turnData.enemies[Bishop], 0x2400000000000000);
		}

		void testPawnLocations() {
			Position pos;
			auto [white, black] = pos.getColorSides();

			auto testPawnLocation = [&](const char* priorMove, const PieceState& pieces, Square square) {
				auto board = makeBitboard(square);
				if (!(pieces[Pawn] & board)) {
					std::println("Error: after {}: no pawn on {}!", priorMove, magic_enum::enum_name(square));
				}
			};

			pos.setPos(parsePositionCommand("startpos"));

			pos.move("e2e4");
			testPawnLocation("e2e4", white, Square::E4);

			pos.move("a7a5");
			testPawnLocation("a7a5", white, Square::E4);
			testPawnLocation("a7a5", black, Square::A5);

			pos.move("d2d4");
			testPawnLocation("d2d4", white, Square::E4);
			testPawnLocation("d2d4", black, Square::A5);

			pos.move("d7d5");
			testPawnLocation("d7d5", white, Square::E4);
			testPawnLocation("d7d5", black, Square::A5);

			pos.move("e4d5");
			testPawnLocation("e4d5", black, Square::A5);

			pos.move("d8d5");
			testPawnLocation("d8d5", black, Square::A5);

			pos.move("c2c4");
			testPawnLocation("c2c4", black, Square::A5);

			auto legalMoves = calcPositionData(pos);
			auto illegalMoveIt = std::ranges::find_if(legalMoves.legalMoves, [](const Move& move) {
				return move.from == Square::D5 && move.to == Square::A5;
			});
			if (illegalMoveIt != legalMoves.legalMoves.end()) {
				std::println("Error: found illegal move d5a5 in testPawnLocations!");
			}
		}

		template<bool Illegal = true, std::invocable<const Move&> Pred>
		void testMovesImpl(std::string_view testName, const std::string& fen, Pred pred) {
			Position pos;
			pos.setPos(parsePositionCommand(fen));

			auto legalMoves = calcPositionData(pos);
			auto moveIt = std::ranges::find_if(legalMoves.legalMoves, pred);
			if constexpr (Illegal) {
				if (moveIt != legalMoves.legalMoves.end()) {
					auto& move = *moveIt;
					std::println("{} failed: {} was able to move to {}", testName, magic_enum::enum_name(move.movedPiece), magic_enum::enum_name(move.to));
				}
			} else {
				if (moveIt == legalMoves.legalMoves.end()) {
					std::println("{} failed. Move did not exist", testName);
				}
			}
		}

		template<bool Illegal = true>
		void testMovesImpl(std::string_view testName, const std::string& fen, Piece piece, Square to) {
			auto testIllegality = [&](const Move& move) {
				return move.movedPiece == piece && move.to == to;
			};
			testMovesImpl<Illegal>(testName, fen, testIllegality);
		}

		void testIllegalKingSquares() {
			testMovesImpl("testIllegalKingSquares", "fen rn3b2/pp2p2p/4b3/k1pN1p2/P3pB2/1PP4P/5PP1/3R2K1 b KQkq - 0 1", King, Square::A4);
		}
		void testIndirectlyCheckedSquares() {
			testMovesImpl("testIndirectlyAttackedSquares", "fen 8/k7/8/4p2p/8/6p1/8/r3K3 w - - 2 57", King, Square::F1);
		}

		void testIllegalKingSquares2() {
			testMovesImpl("testIllegalKingSquares2", "fen 6Q1/1bN5/2p5/1k6/1p1Pn3/8/7P/5RK1 b - - 4 28", King, Square::C5);
		}

		void testIllegalPawnSquares() {
			testMovesImpl("testIllegalPawnSquares", "fen 8/1bN4Q/1kp5/8/1p1Pn3/8/7P/R5K1 b - - 8 30", Pawn, Square::D5);
		}

		void testIllegalRookSquares() {
			testMovesImpl("testIllegalRookSquares", "fen 1r3knr/p2n1pp1/P7/4p2p/2p1N3/2P5/1PP2PPP/2B2RK1 w - - 1 20", Rook, Square::A1);
		}

		void testCheck() {
			auto testIllegality = [](const Move& move) {
				return move.movedPiece != King || move.to == Square::F7;
			};
			testMovesImpl("testCheck", "fen 1rbqk2r/3p1P2/1pn5/p1p5/2P4p/2NQ3P/PP4P1/R4RK1 b k - 0 18", testIllegality);
		}

		void testCheck2() {
			testMovesImpl("testCheck2", "fen r1b1kb2/pp3pp1/n7/1NpPp3/6n1/2Q5/PP1PP3/R1BK1q2 w q - 0 15", [](const Move& move) {
				return move.from != Square::D1 && move.to != Square::D2;
			});
		}

		void testCheck3() {
			testMovesImpl("testCheck3", "fen 1b2n3/5N1p/1p1pBk1R/p1p1p3/P3P3/R2P4/1PP2P2/4K3 b - - 25 45", King, Square::E6);
		}

		void testThatLegalMovesExist() {
			constexpr auto POSITION_COMMAND = "fen r4b1r/p1p1p1pp/8/2Pp4/PP1PPBk1/6Q1/8/5RK1 b - - 1 31";
			Position pos;
			pos.setPos(parsePositionCommand(POSITION_COMMAND));

			auto moves = calcPositionData(pos);
			if (moves.legalMoves.empty()) {
				std::println("Error: no legal moves found in testThatLegalMovesExist!");
			}
		}

		void testThatLegalMovesExist2() {
			Position pos;
			pos.setPos(parsePositionCommand("startpos moves g1f3 a7a5 d2d4 d7d5 c1f4 b7b5 d1d3 c8a6 e2e3 b5b4 d3d2 a6f1 e1f1 f7f6 c2c3 g7g5 f4g3 h7h5 h2h4 a5a4 c3b4 g5g4 f3e1 d8d7 e1d3 e8d8 d2c2 f8h6 b1d2 f6f5 a1c1 b8a6 b4b5 d7b5 g3c7 a6c7 c2c7 d8e8 f1e2 b5a6 c7c3 e8d7 e2e1 d7d8 d3e5 d8e8 f2f3 e7e6 e1f2 g8e7 c1c2 a8c8 c3d3 a6d3 c2c8 e7c8 e5d3 h6g7 d3c5 g7d4 e3d4 c8b6 c5e6 e8e7 e6g5 f5f4 g2g3 h8f8 g3f4 f8f4 f2e3 f4f5 b2b3 a4a3 b3b4 b6c4 d2c4 d5c4 b4b5 f5b5 h1c1 b5b4 d4d5 e7d7 g5e4 g4f3 e3f3 b4b5 d5d6 b5b4 f3e3 d7c6 e3d4 c6b6 d6d7 b6c7 c1d1 c7d8 d4c3 b4b2 c3c4 b2h2 e4f6 h2h4 c4b3 h4h3 b3a4 h5h4 d1e1 d8c7 e1e8 h3d3 a4b4 d3d4 b4c3 d4d1 c3c2 d1d4 c2c3 d4d1 c3c4 d1c1 c4b3 c1b1 b3a3 b1d1 a3b2 d1d2 b2b3 d2d3 b3b4 d3d4 b4c5 d4d7 f6d7 c7d7 e8h8 d7e6 h8h4 e6f5 h4h1 f5e4 a2a4 e4f3 a4a5 f3g2 h1a1 g2f2 a5a6 f2e2 a6a7 e2d2 a7a8q d2c2 a8b8 c2d2 c5b6 d2c2 b6a7 c2d2 a7a8 d2c2 a1b1 c2d2 b8d8 d2c2 b1b4 c2c1 d8c7 c1d2 c7c4 d2e3 b4b3 e3d2 c4c3 d2d1 b3b1"));

			auto legalMoves = calcPositionData(pos);
			if (legalMoves.legalMoves.empty()) {
				std::println("Error: no legal moves found in testThatLegalMovesExist2!");
			}
		}

		void testThatLegalMovesExist3() {
			Position pos;
			pos.setPos(parsePositionCommand("fen KQ6/Q7/8/8/8/8/8/4k3 w - - 0 1 moves a7c7 e1d2 b8d8"));
			
			auto legalMoves = calcPositionData(pos);
			if (legalMoves.legalMoves.empty()) {
				std::println("Error: no legal moves found in testThatLegalMovesExist3!");
			}
		}

		void testThatLegalMovesExist4() {
			Position pos;
			pos.setPos(parsePositionCommand("startpos"));
			RepetitionMap rMap;

			std::string moves;
			for (int i = 0; i < 20; i++) {
				auto [bestMove, _] = getSearchFunction().findBestMove(pos, 6_su8, rMap);
				if (bestMove == Move::null()) {
					std::println("POTENTIAL Error: no best move found in testThatLegalMovesExist4 at ply {}!", i + 1);
					std::println("Moves: {}", moves);
					return;
				}
				pos.move(bestMove);
				moves += bestMove.getUCIString() + " ";
			}
		}

		void testThatLegalMovesExist5() {
			Position pos;
			testMovesImpl<false>("testThatLegalMovesExist5", "fen rnbq1k1r/3p1ppp/1p1b1n1Q/pBp1p3/4P2P/N2P3R/PPP2PP1/R1B1K1N1 b Q - 2 8", Pawn, Square::H6);
		}

		void testEnPassant() {
			Position pos;
			pos.setPos(parsePositionCommand("fen 8/8/8/2k5/5p2/8/6P1/3K4 w - - 0 1"));
			pos.move("g2g4");
			pos.move("f4g3");

			auto [white, black] = pos.getColorSides();

			assert_equality(nextSquare(white[Pawn]), Square::None);
			assert_equality(nextSquare(black[Pawn]), Square::G3);
		}

		void testEnPassant2() {
			Position pos;
			pos.setPos(parsePositionCommand("fen rnbqkbnr/pp1ppppp/8/8/2p5/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
			pos.move("d2d4");

			auto [white, black] = pos.getColorSides();

			//verify that white has moved the D pawn two squares forward
			assert_equality(white.doubleJumpedPawn, Square::D4);

			auto legalMoves = calcPositionData(pos);
			auto enPassantMoveIt = std::ranges::find_if(legalMoves.legalMoves, [](const Move& move) {
				return move.capturedPawnSquareEnPassant == Square::D4;
			});
			if (enPassantMoveIt == legalMoves.legalMoves.end()) {
				std::println("testEnPassant2 failed: en passant not among legal moves");
				return;
			}

			auto verifyStateAfterEnPassant = [](const Position& pos) {
				auto [white, black] = pos.getColorSides();
				//verify that black has captured the D pawn en passant
				if (containsSquare(white[Pawn], Square::D4)) {
					std::println("testEnPassant2 failed: white pawn still on d4 after en passant capture");
				}
				if (!containsSquare(black[Pawn], Square::D3)) {
					std::println("testEnPassant2 failed: black pawn not on d3 after en passant capture");
				}
			};

			auto temp = pos;

			pos.move("c4d3");
			verifyStateAfterEnPassant(pos);

			temp.move(*enPassantMoveIt);
			verifyStateAfterEnPassant(temp);

			if (temp.hash() != pos.hash()) {
				std::println("testEnPassant2 failed: positions after en passant do not match");
			}
		}

		void testPin() {
			Position pos;
			pos.setPos(parsePositionCommand("fen k7/8/8/3r4/8/8/8/rB1BK3 w - - 0 1"));

			auto legalMoveData = calcPositionData(pos);
			auto d1BishopMove = std::ranges::find_if(legalMoveData.legalMoves, [](const Move& move) {
				return move.from == Square::D1;
			});
			if (d1BishopMove == legalMoveData.legalMoves.end()) {
				std::println("testPin failed: d1 bishop is not pinned but it is not moveable");
			}
		}

		void testPin2() {
			auto pinRay = makeBitboard(Square::F7, Square::F6, Square::F5, Square::F4, Square::F3);
			testMovesImpl("testPin2", "fen 1nb2k1r/2b4p/1p1p1q1N/p1pBp2P/P3P1P1/3P1R2/1PP2P2/R3K1N1 b Q - 2 26", [&](const Move& move) {
				return move.movedPiece == Queen && !containsSquare(pinRay, move.to);
			});
		}

		void testPin3() {
			testMovesImpl("testPin3", "fen rn2kb1r/4pppp/2p5/p4n2/P2q1PbP/1Pp2N2/3N2P1/R1BKQB1R w kq - 0 15", Knight, Square::C4);
		}

		void testPinWithCheck() {
			Position pos;
			pos.setPos(parsePositionCommand("fen 8/8/1r2knR1/2b5/1p2pBBP/1P1p4/5PP1/6K1 b - - 4 44"));

			auto legalMoveData = calcPositionData(pos);
			auto illegalKnightMove = std::ranges::find_if(legalMoveData.legalMoves, [](const Move& move) {
				return move.from == Square::F6;
			});
			if (illegalKnightMove != legalMoveData.legalMoves.end()) {
				std::println("testPinWithCheck failed: illegal knight move: {}", illegalKnightMove->getUCIString());
			}
		}

		void testPinWithCheck2() {
			testMovesImpl("testPinWithCheck2", "fen 1n4kr/1bqp1ppp/r4b1N/ppp1p3/4P1Q1/P1PP2PB/1P1N1P1P/R4RK1 b - - 5 18", Pawn, Square::H6);
		}

		void testEnPassantPin() {
			testMovesImpl("testEnPassantPin", "fen 8/8/8/8/3k1pPR/8/8/3K4 b - g3 0 1", Pawn, Square::G3);
		}

		void testBitboardImageCreation() {
			Position pos;
			pos.setPos(parsePositionCommand("startpos"));

			auto [white, black] = pos.getColorSides();
			auto whitePieces = white.calcAllLocations();
			auto blackPieces = black.calcAllLocations();

			auto colorGetter = [&](Bitboard bitboard) -> RGB {
				if (bitboard & whitePieces) {
					return { 255, 255, 255 }; 
				} else if (bitboard & blackPieces) {
					return { 0, 0, 0 };
				} else {
					return { 240, 255, 91 }; 
				}
			};
			drawBitboardImage(colorGetter, "start_position.bmp");
		}

		void doUCIHandshake(const Pipe& pipe) {
			pipe.write("uci\n");
			std::println("{}", pipe.read("id name"));
			pipe.write("ucinewgame\n");
			pipe.write("isready\n");
			std::println("{}", pipe.read("readyok"));
		}

		void testUCIInput() {
			Pipe pipe;
			doUCIHandshake(pipe);

			pipe.write("position startpos moves e2e4\ngo\n");
			std::println("{}", pipe.read("bestmove"));
			
			pipe.write("ucinewgame\nposition startpos moves e2e4 h7h5 d2d4\ngo\n");
			std::println("{}", pipe.read("bestmove"));

			pipe.write("ucinewgame\nposition startpos moves e2e4 h7h5 d2d4 d7d5\ngo\n");
			std::println("{}", pipe.read("bestmove"));

			pipe.write("ucinewgame\nposition startpos moves e2e4 h7h5 d2d4 d7d5 e4d5\ngo\n");
			std::println("{}", pipe.read("bestmove"));
		}

		void testEnemySquareOutput() {
			Position pos;
			pos.setPos(parsePositionCommand("fen rnb1kbnr/pp1pppp1/2p5/q6p/2PPP3/8/PP1B1PPP/RN1QKBNR b KQkq - 1 1"));

			auto legalMoves = calcPositionData(pos);
			auto whiteSquares = legalMoves.whiteSquares.destSquaresPinConsidered;

			if (!containsSquare(whiteSquares, Square::A3, Square::C3, Square::B4, Square::A5)) {
				std::println("testEnemySquareOutput failed: either D3, D4, or D5 are not contained in the white squares");
				auto currSquare = Square::None;
				while (nextSquare(whiteSquares, currSquare)) {
					std::println("{}", magic_enum::enum_name(currSquare));
				}
			}
		}

		void testCastling() {
			testMovesImpl<false>("testCastling", "fen 3k4/3r4/8/8/8/8/4PPPP/4K2R w K - 0 1", King, Square::G1);
		}

		void testRepetition() {
			RepetitionMap rMap;
			Position startPos;
			startPos.setPos(parsePositionCommand("startpos"));
			rMap.push(startPos);

			assert_equality(rMap.getPositionCount(startPos), 1);

			Move whiteTo{Square::B1, Square::C3, Knight, Piece::None };
			Move whiteBack{Square::C3, Square::B1, Knight, Piece::None };

			Move blackTo{Square::B8, Square::C6, Knight, Piece::None };
			Move blackBack{Square::C6, Square::B8, Knight, Piece::None };

			Position p1{ startPos, whiteTo };
			rMap.push(p1);
			assert_equality(rMap.getPositionCount(p1), 1);

			Position p2{ p1, blackTo };
			rMap.push(p2);
			assert_equality(rMap.getPositionCount(p2), 1);
			
			Position p3{ p2, whiteBack };
			rMap.push(p3);
			assert_equality(rMap.getPositionCount(p3), 1);

			Position p4{ p3, blackBack };
			rMap.push(p4);
			assert_equality(rMap.getPositionCount(p4), 2);

			if (p4.hash() != startPos.hash()) {
				std::println("testRepetitionFailed: p4 is not equal to startPos");
			}
		}

		void testRepetition2() {
			RepetitionMap rMap;
			Position pos;
			pos.setPos(parsePositionCommand("startpos"));
			rMap.push(pos);
			assert_equality(rMap.getTotalPositionCount(), 1);

			//has side effect of storing positions in the repetition table, but positions should be popped
			getSearchFunction().findBestMove(pos, 6_su8, rMap);
			assert_equality(rMap.getTotalPositionCount(), 1);
		}

		void testCheckmate() {
			Position pos;
			pos.setPos(parsePositionCommand("fen rn2kbnr/p3ppp1/1p4p1/2p5/8/2NPBq1b/PPP2P1P/R4RK1 b kq - 0 1"));
			RepetitionMap rMap;
			rMap.push(pos);

			auto [bestMove, _] = getSearchFunction().findBestMove(pos, 6_su8, rMap);
			assert_equality(bestMove.from, Square::F3);
			assert_equality(bestMove.to, Square::G2);
		}

		void runAllTests() {
			std::println("Running tests...");

			getSearchFunction(); //registers thread

			testStartPos();
			testPawnLocations();
			testIllegalKingSquares();
			testIndirectlyCheckedSquares();
			testIllegalKingSquares2();
			testIllegalPawnSquares();
			testIllegalRookSquares();
			testCheck();
			testCheck2();
			testCheck3();
			testThatLegalMovesExist();
			testThatLegalMovesExist2();
			testThatLegalMovesExist3();
			//testThatLegalMovesExist4(); //really really long!
			testThatLegalMovesExist5();
			testEnPassant();
			testEnPassant2();
			testPin();
			testPin2();
			testPin3();
			testEnPassantPin();
			testPinWithCheck();
			testPinWithCheck2();
			testBitboardImageCreation();
			testEnemySquareOutput();
			testCastling();
			runInternalEvaluationTests();
			runInternalMoveSearchTests();
			testRepetition();
			testRepetition2();
			testCheckmate();
			std::println("Finished tests");
			//testUCIInput(); //long!
		}
	}
}