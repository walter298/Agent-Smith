export module Chess.SafeInt;

import std;

import Chess.Assert;

namespace chess {
	template<std::integral T>
	constexpr T customAbs(T value) {
		if constexpr (std::unsigned_integral<T>) {
			return value;
		}
		return std::cmp_less(value, 0) ? static_cast<T>(-value) : value;
	}

	export template<std::integral T>
	class SafeInt {
	public:
		static constexpr T MAX_VALUE = std::numeric_limits<T>::max();
		static constexpr T MIN_VALUE = std::numeric_limits<T>::min();
	private:
		T m_value;
	public:
		using Int = T;

		constexpr SafeInt() : m_value{ 0 } {}
		explicit constexpr SafeInt(T t) : m_value{ t } {}

		constexpr T get() const {
			return m_value;
		}

		constexpr SafeInt& operator=(const SafeInt&) = default;
		constexpr SafeInt& operator++() {
			zAssert(m_value != MAX_VALUE);
			++m_value;
			return *this;
		}
		constexpr SafeInt& operator--() {
			zAssert(m_value > MIN_VALUE);
			--m_value;
			return *this;
		}
	private:
		static constexpr bool isValidDivision(const SafeInt& a, const SafeInt& b) {
			if (b.m_value == 0) {
				return false;
			}
			if constexpr (std::signed_integral<T>) {
				if (a.m_value == MIN_VALUE) {
					return b.m_value != -1;
				} 
			}
			return true;
		}
		static constexpr bool isValidMultiplication(const SafeInt& a, const SafeInt& b) {
			if (a.m_value == MAX_VALUE) {
				if constexpr (std::signed_integral<T>) {
					return b.m_value == 0 || b.m_value == 1 || b.m_value == -1;
				} else {
					return b.m_value == 0 || b.m_value == 1;
				}
			}
			if (a.m_value == MIN_VALUE) { 
				if constexpr (std::signed_integral<T>) {
					return b.m_value == 0 || b.m_value == 1;
				} else {
					return true; //min value for unsigned types is 0
				}
			}
			if (b.m_value == MAX_VALUE) {
				if constexpr (std::signed_integral<T>) {
					return a.m_value == 0 || a.m_value == 1 || a.m_value == -1;
				} else {
					return a.m_value == 0 || a.m_value == 1;
				}
			}
			if (b.m_value == MIN_VALUE) {
				if constexpr (std::signed_integral<T>) {
					return a.m_value == 0 || a.m_value == 1;
				} else {
					return true; //min value for unsigned types is 0
				}
			}
			if (a.m_value == 0) {
				return true;
			}
			return SafeInt{ MAX_VALUE } / SafeInt{ customAbs(a.m_value) } >= SafeInt{ customAbs(b.m_value) };
		}
		static constexpr bool isValidSubtraction(const SafeInt& a, const SafeInt& subtractand) {
			if (subtractand.m_value == 0) {
				return true;
			} else if (subtractand.m_value > 0) {
				return a.m_value >= MIN_VALUE + subtractand.m_value;
			} else {
				return a.m_value <= MAX_VALUE + subtractand.m_value;
			}
		}
		static constexpr bool isValidAddition(const SafeInt& a, const SafeInt& addend) {
			if (addend.m_value == 0) {
				return true;
			} else if (addend.m_value > 0) {
				return a.m_value <= MAX_VALUE - addend.m_value;
			} else {
				return a.m_value >= MIN_VALUE - addend.m_value;
			}
		}
	public:
		constexpr SafeInt& operator+=(const SafeInt& t) {
			zAssert(isValidAddition(*this, t));
			m_value += t.m_value;
			return *this;
		}
		constexpr SafeInt& operator-=(const SafeInt& t) {
			zAssert(isValidSubtraction(*this, t));
			m_value -= t.m_value;
			return *this;
		}
		constexpr SafeInt& operator*=(const SafeInt& t) {
			zAssert(isValidMultiplication(*this, t));
			m_value *= t.m_value;
			return *this;
		}
		constexpr SafeInt& operator/=(const SafeInt& t) {
			zAssert(isValidDivision(*this, t));
			m_value /= t.m_value;
			return *this;
		}
		constexpr SafeInt& operator|=(const SafeInt& t) {
			m_value |= t.m_value;
			return *this;
		}
		constexpr SafeInt& operator&=(const SafeInt& t) {
			m_value &= t.m_value;
			return *this;
		}
		template<std::integral U>
		constexpr SafeInt& operator>>=(U s) {
			*this = *this >> s;
			return *this;
		}
		template<std::integral U>
		constexpr SafeInt& operator<<=(U s) {
			*this = *this << s;
			return *this;
		}

		constexpr auto operator<=>(const SafeInt& t) const = default;

		template<std::integral U>
		constexpr SafeInt operator<<(U s) const {
			zAssert(std::cmp_less(s, sizeof(T) * 8));
			return SafeInt{ static_cast<T>(m_value << s) };
		}
		template<std::integral U>
		constexpr SafeInt operator>>(U s) const {
			zAssert(std::cmp_less(s, sizeof(T) * 8));
			return SafeInt{ static_cast<T>(m_value >> s) };
		}

		constexpr SafeInt operator-() const {
			static_assert(std::signed_integral<T>);
			zAssert(m_value != MIN_VALUE);
			return SafeInt{ static_cast<T>(-m_value) };
		}
		constexpr SafeInt operator~() const {
			return SafeInt{ static_cast<T>(~m_value) };
		}

		void subToMin(SafeInt subbed, SafeInt min) {
			zAssert(min <= *this && subbed.m_value >= 0);
			if (!isValidSubtraction(*this, subbed)) {
				m_value = min.get();
			} else {
				auto diff = *this - subbed;
				*this = diff < min ? min : diff;
			}
		}

		void incMod() requires(std::unsigned_integral<T>) {
			++m_value;
		}

		constexpr friend SafeInt operator+(SafeInt a, SafeInt b) {
			zAssert(isValidAddition(a, b));
			return SafeInt{ static_cast<T>(a.m_value + b.m_value) }; //cast to stop integer promotion from kicking in
		}
		constexpr friend SafeInt operator-(SafeInt a, SafeInt b) {
			zAssert(isValidSubtraction(a, b));
			return SafeInt{ static_cast<T>(a.m_value - b.m_value) };
		}
		constexpr friend SafeInt operator*(SafeInt a, SafeInt b) {
			zAssert(isValidMultiplication(a, b));
			return SafeInt{ static_cast<T>(a.m_value * b.m_value) };
		}
		constexpr friend SafeInt operator/(SafeInt a, SafeInt b) {
			zAssert(isValidDivision(a, b));
			return SafeInt{ static_cast<T>(a.m_value / b.m_value) };
		}
		constexpr friend SafeInt operator&(SafeInt a, SafeInt b) {
			return SafeInt{ static_cast<T>(a.m_value & b.m_value) };
		}
		constexpr friend SafeInt operator|(SafeInt a, SafeInt b) {
			return SafeInt{ static_cast<T>(a.m_value | b.m_value) };
		}
		constexpr friend SafeInt operator%(SafeInt a, SafeInt b) {
			zAssert(isValidDivision(a, b));
			return SafeInt{ static_cast<T>(a.m_value % b.m_value) };
		}
	};

	template<std::unsigned_integral U, std::integral C>
	constexpr SafeInt<U> convertImpl(C v) {
		zAssert(std::cmp_greater_equal(v, 0) && std::cmp_less_equal(v, std::numeric_limits<U>::max()));
		return SafeInt{ static_cast<U>(v) };
	};

	export constexpr SafeInt<std::uint8_t> operator""_su8(unsigned long long v) {
		return convertImpl<std::uint8_t>(v);
	}

	template<std::integral T>
	SafeInt<T> safePow(SafeInt<T> a, SafeInt<T> b) {
		if (b < SafeInt<T>{ 0 }) {
			return SafeInt<T>{ 0 };
		}
		SafeInt<T> ret{ 1 };
		for (auto i = SafeInt<T>{ 0 }; i < b; ++i) {
			ret *= a;
		}
		return ret;
	}
}