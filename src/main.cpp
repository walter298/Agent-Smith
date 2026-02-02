import std;

import Chess.Arena;
import Chess.BitboardImage;
import Chess.Evaluation;
import Chess.MoveGeneration;
import Chess.UCI;
import Chess.Move;
import Chess.SafeInt;
import Chess.Tests;
import Chess.PositionCommand;
import Chess.MoveSearch;
import Chess.Position.RepetitionMap;

namespace chess {
	void handleBitboardInput(const char** argv, int argc) {
		if (argc != 5) {
			std::println("Error: draw_bitboard requires 3 arguments: [bitboard, base, filename]");
			return;
		}
		constexpr auto BITBOARD_INDEX = 2;
		constexpr auto BASE_INDEX = 3;
		constexpr auto FILENAME_INDEX = 4;

		int base = 0;
		auto baseStr = argv[BASE_INDEX];
		auto baseStrEnd = argv[BASE_INDEX] + std::strlen(argv[BASE_INDEX]);
		auto baseRes = std::from_chars(baseStr, baseStrEnd, base, 10);
		if (baseRes.ec != std::errc{}) {
			std::println("Error: could not parse base argument");
			return;
		}

		Bitboard bitboard = 990;
		auto bitboardStr = argv[BITBOARD_INDEX];
		auto bitboardStrEnd = argv[BITBOARD_INDEX] + std::strlen(argv[BITBOARD_INDEX]);
		auto bitboardRes = std::from_chars(bitboardStr, bitboardStrEnd, bitboard, base);
		if (bitboardRes.ec != std::errc{}) {
			std::println("Error: could not parse bitboard argument");
			return;
		}

		auto colorGetter = [bitboard](Bitboard bit) -> RGB {
			if (bitboard & bit) {
				return { 97, 10, 255 };
			} else {
				return { 255, 255, 0 };
			}
		};

		drawBitboardImage(colorGetter, argv[FILENAME_INDEX]);
	}

	void playUCIWithDepth(const char** argv, int argc) {
		if (argc != 3) {
			std::println("Error: uci with depth requires 1 argument: [depth]");
			return;
		}
		SafeInt<std::uint8_t> depth{ 0 };
		auto depthStr = argv[2];
		auto depthStrEnd = depthStr + std::strlen(depthStr);

		auto temp = depth.get();
		auto depthRes = std::from_chars(depthStr, depthStrEnd, temp, 10);
		if (depthRes.ec != std::errc{}) {
			std::println("Error: could not parse depth argument");
			return;
		}
		depth = SafeInt{ temp };

		if (depth < SafeInt<std::uint8_t>{ 1 }) {
			std::println("Error: depth must be at least 1");
			return;
		}

		playUCI(depth);
	}

	void printCommandLineArgumentOptions() {
		std::println("Options:");
		std::println("(none)\t\t\t\t\t\t- Start the engine in UCI mode (default depth = 6)");
		std::println("uci [depth]\t\t\t\t\t- Start the engine in UCI mode with specified depth");
		std::println("test\t\t\t\t\t\t- Run all tests");
		std::println("draw_bitboard [bitboard, base, filename]\t- Draw a bitboard image");
		std::println("generate_bmi_table");
		std::println("see_move_priorities [fen]");
		std::println("evaluate [depth] [fen]");
	}

	void printPositionEvaluation(const char** argv, int argc) {
		if (argc < 3) {
			std::println("Error: invalid FEN");
			return;
		}

		std:uint8_t depthTemp = 0;
		auto depthParseRes = std::from_chars(argv[2], argv[2] + std::strlen(argv[2]), depthTemp);
		if (depthParseRes.ec != std::error_code{}) {
			std::println("Error parsing depth: {}", std::make_error_code(depthParseRes.ec).value());
			return;
		}
		SafeInt depth{ depthTemp };

		//parse fen
		using namespace std::literals;
		std::string fen = argv[3];
		for (int i = 3; i < argc; i++) {
			fen += " "s + argv[i];
		}

		std::println("Fen: {}", fen);

		Position pos;
		pos.setPos(parsePositionCommand(std::format("fen {}", fen)));
		auto posData = calcPositionData(pos);

		RepetitionMap map;
		AsyncSearch search;
		auto [bestMove, rating] = search.findBestMove(pos, depth, map);
		std::println("{} {}", bestMove.getUCIString(), rating.get());
	}
}

int main(int argc, const char** argv) {
	//chess::arena::init();

	if (argc == 1) {
		constexpr chess::SafeInt<std::uint8_t> DEFAULT_DEPTH{ 8 };
		chess::playUCI(DEFAULT_DEPTH);
	} else if (std::strcmp(argv[1], "uci") == 0) {
		chess::playUCIWithDepth(argv, argc);
	} else if (std::strcmp(argv[1], "test") == 0) {
		chess::tests::runAllTests();
	} else if (std::strcmp(argv[1], "draw_bitboard") == 0) {
		chess::handleBitboardInput(argv, argc);
	} else if (std::strcmp(argv[1], "help") == 0) {
		chess::printCommandLineArgumentOptions();
	} else if (std::strcmp(argv[1], "generate_bmi_table") == 0) {
		chess::storeBMITable();
	} else if (std::strcmp(argv[1], "evaluate") == 0) {
		chess::printPositionEvaluation(argv, argc);
	} else {
		std::print("Invalid command line arguments. ");
		chess::printCommandLineArgumentOptions();
	}
	return 0;
}