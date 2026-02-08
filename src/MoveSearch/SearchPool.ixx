export module Chess.MoveSearch:SearchPool;

export import std;

import Chess.UCI.UciCommand;
export import Chess.UCI.Signal;

export namespace chess {
	using SearchPoolHandle = std::move_only_function<Move(const PositionCommand&, const GoCommand&)>;
	SearchPoolHandle makeSearchPool(Signal* signal);
}