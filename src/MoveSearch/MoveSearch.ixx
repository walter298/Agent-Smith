export module Chess.MoveSearch;

export import std;

import Chess.Position;
import Chess.SafeInt;
import Chess.Position.RepetitionMap;

export import :MoveSearchTests;
export import :TranspositionTable;

namespace chess {
	struct AsyncSearchState;

	struct SearchResult {
		Move move = Move::null();
		Rating rating = 0_rt;
	};

	export class AsyncSearch {
	private:
		std::shared_ptr<AsyncSearchState> m_state;
	public:
		AsyncSearch();

		SearchResult findBestMove(const Position& pos, SafeInt<std::uint8_t> depth, const RepetitionMap& repetitionMap);
		void cancel();
	};
}