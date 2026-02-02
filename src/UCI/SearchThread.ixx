export module Chess.UCI:SearchThread;

export import std;

import Chess.Position;
import Chess.Position.RepetitionMap;
import Chess.MoveSearch;
import Chess.SafeInt;

namespace chess {
	struct GameState {
		Position pos;
		SafeInt<std::uint8_t> depth{ 6_su8 };
		RepetitionMap repetitionMap;
	};

	class SearchThread {
	private:
		std::mutex m_mutex;
		AsyncSearch m_searcher;
		GameState m_state;
		bool m_shouldPonder = false;
		bool m_calculationRequested = false;
		std::condition_variable_any m_cv;
		std::jthread m_thread; //thread is destroyed before all other members

		void think(std::stop_token stopToken);
		void run(std::stop_token stopToken);
	public:
		SearchThread();
		~SearchThread();

		void stop();
		void setPosition(GameState gameState);
		void go(SafeInt<std::uint8_t> depth);
	};
}