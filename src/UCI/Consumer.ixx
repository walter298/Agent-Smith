export module Chess.UCI:Producer;

import std;

import Chess.UCI.UciCommand;

namespace chess {
	struct EngineState;
	
	class UciConsumer {
	private:
		std::shared_ptr<EngineState> m_state;
		std::jthread m_thread;
	public:
		UciConsumer();

		void operator()(const UciCommand& command);
	};
}