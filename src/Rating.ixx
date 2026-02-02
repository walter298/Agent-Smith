export module Chess.Rating;

import std;

import Chess.Assert;
import Chess.SafeInt;

export namespace chess {
	using Rating = SafeInt<std::int32_t>;

	constexpr Rating operator""_rt(unsigned long long rating) {
		zAssert(rating <= std::numeric_limits<Rating::Int>::max());
		return Rating{ static_cast<Rating::Int>(rating) };
	}

	constexpr Rating operator""_rt(long double rating) {
		return Rating{ static_cast<Rating::Int>(rating) };
	}

	template<bool Maximizing>
	consteval Rating worstPossibleRating() {
		if constexpr (Maximizing) {
			return Rating{ std::numeric_limits<Rating::Int>::lowest() };
		} else {
			return Rating{ std::numeric_limits<Rating::Int>::max() };
		}
	}

	template<bool Maximizing>
	consteval Rating checkmatedRating() {
		return Maximizing ? worstPossibleRating<Maximizing>() + 1_rt : worstPossibleRating<Maximizing>() - 1_rt;
	}
}