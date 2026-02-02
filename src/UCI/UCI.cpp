module;

#include <cstdio>

module Chess.UCI;

import std;

import Chess.DebugPrint;
import Chess.Evaluation;
import Chess.Position.RepetitionMap;
import Chess.PositionCommand;

import :SearchThread;

namespace chess {
	std::string getTokensAfterPosition(std::istringstream& iss) {
		auto buff = iss.str();
		auto currentPos = static_cast<size_t>(iss.tellg());
		return buff.substr(currentPos);
	}

	GameState makeGameState(const std::string& commandStr, SafeInt<std::uint8_t> depth) {
		GameState ret;
		
		auto command = parsePositionCommand(commandStr);
		ret.pos.setPos(command);
		ret.repetitionMap.push(ret.pos);

		for (const auto& move : command.moves) {
			ret.pos.move(move);
			ret.repetitionMap.push(ret.pos);
		}

		ret.depth = depth;

		return ret;
	}

	void playUCI(SafeInt<std::uint8_t> depth) {
		SearchThread searchThread;

		std::istringstream iss;
		std::string line;
		std::string token;

		GameState lastGameState;

		while (true) {
			if (!std::getline(std::cin, line)) {
				std::println("No more input. EOF: {}", std::cin.eof());
				break;
			}

			debugPrint(std::format("{}", line)); //DOES NOT SEND TO stdout
			
			iss.clear();
			iss.str(line);
			if (!(iss >> token)) {
				continue;
			}
			
			if (token == "quit") {
				break;
			} else if (token == "position") {
				lastGameState = makeGameState(getTokensAfterPosition(iss), depth);
				searchThread.setPosition(lastGameState);
			} else if (token == "ucinewgame") {
				searchThread.stop();
			} else if (token == "isready") {
				debugPrint("readyok");
				std::printf("readyok\n");
				std::fflush(stdout);
			} else if (token == "uci") {
				constexpr auto ENGINE_INFO = "id name Agent Smith\n"
											 "id author Walter Stein-Smith\n"
											 "uciok\n";
				debugPrint(ENGINE_INFO);
				std::printf(ENGINE_INFO);
				std::fflush(stdout);
			} else if (token == "go") {
				searchThread.go(depth); 
			} else if (token == "stop") {
				searchThread.stop();
			}
		}
	}
}