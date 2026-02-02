export module Chess.UCI;

import std;

export import Chess.SafeInt;

namespace chess {
	export void playUCI(SafeInt<std::uint8_t> depth);
}