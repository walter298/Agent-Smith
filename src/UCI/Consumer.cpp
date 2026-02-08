module;

#include <cstdio>

module Chess.UCI:Producer;

import std;

import Chess.Assert;
import Chess.BitboardImage;
import Chess.MoveSearch;
import Chess.SafeInt;

import Chess.UCI.UciCommand;
import Chess.UCI.Signal;

namespace chess {
	const auto THREAD_COUNT = std::thread::hardware_concurrency();
	constexpr auto MAIN_THREAD_INDEX = 0uz;

	struct EngineState {
		Signal signal;
		SearchPoolHandle pool;
		PositionCommand posCommand;

		EngineState() {
			pool = makeSearchPool(&signal);
		}
	};

	void runSearchThread(std::stop_token stopToken, std::shared_ptr<EngineState> engineState) {
		bool running = true;

		while (running) {
			auto uciCommand = engineState->signal.waitForUciCommand(stopToken);
			switch (uciCommand.getType()) {
			case UciCommandType::Quit:
				running = false;
				break;
			case UciCommandType::Position:
				std::println("Received a position command!");
				std::fflush(stdout);

				engineState->posCommand = uciCommand.getPositionCommand();
				engineState->posCommand.pos.verify();
				break;
			case UciCommandType::Go:
				std::println("Received a go command!");
				std::fflush(stdout);

				engineState->signal.signalSearchStart();
				engineState->posCommand.pos.verify();
				auto bestMove = engineState->pool(engineState->posCommand, uciCommand.getGoCommand());
				if (!engineState->signal.isStopRequested()) {
					std::println("{}", bestMove.getUCIString());
					std::fflush(stdout);
				}
				engineState->signal.signalSearchEnd();

				flushTranspositionTableData();

				break;
			}
		}
	}

	UciConsumer::UciConsumer() : m_state{ std::make_shared<EngineState>() } {
		m_thread = std::jthread{ runSearchThread, m_state };
	}

	void UciConsumer::operator()(const UciCommand& uciCommand) {
		std::println("Setting a command!");
		std::fflush(stdout);
		m_state->signal.setCommand(uciCommand);
	}
}