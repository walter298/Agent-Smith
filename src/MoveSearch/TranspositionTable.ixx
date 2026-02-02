export module Chess.MoveSearch:TranspositionTable;

export import std;
export import Chess.Position;
export import Chess.Rating;
export import Chess.SafeInt;

export import :MovePriority;

namespace chess {
	enum WindowBound : std::uint8_t {
		InWindow,
		LowerBound,
		UpperBound
	};

	struct TTEntry {
		Move bestMove = Move::null();
		SafeInt<std::uint8_t> depth;
		WindowBound bound = InWindow; //uint8_t
		Rating rating = 0_rt; //int32_t
		SafeInt<std::uint8_t> age;
	};
	
	std::optional<TTEntry> getPositionEntry(const Position& pos, SafeInt<std::uint8_t> depth);
	void storePositionEntry(const Position& pos, const TTEntry& entry);

	void updateTTAge();
	export void resetTranspositionTable();
}