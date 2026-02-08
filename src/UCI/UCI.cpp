module;

#include <cstdio>

module Chess.UCI;

import std;

import Chess.DebugPrint;
import Chess.Evaluation;
import Chess.Position.RepetitionMap;
import Chess.PositionCommand;

import :Producer;

namespace chess {
	std::string getTokensAfterPosition(std::istringstream& iss) {
		auto buff = iss.str();
		auto currentPos = static_cast<size_t>(iss.tellg());
		return buff.substr(currentPos);
	}

	void playUCI(SafeInt<std::uint8_t> depth) {
		UciConsumer consumerThread;

		std::istringstream iss;
		std::string line;
		std::string token;

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
				PositionCommand posCommand{ parsePositionCommand(getTokensAfterPosition(iss)) };
				consumerThread(UciCommand::position(posCommand));
			} else if (token == "ucinewgame") {
				consumerThread(UciCommand::stop());
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
				std::println("Sending go command...");
				std::fflush(stdout);
				consumerThread(UciCommand::go({ depth })); 
			} else if (token == "stop") {
				consumerThread(UciCommand::stop());
			}
		}
	}
}