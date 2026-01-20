export module Chess.SafeInt;

import std;

import Chess.Assert;

namespace chess {
	export template<std::unsigned_integral T>
	class SafeUnsigned {
	private:
		static constexpr T MAX_VALUE = std::numeric_limits<T>::max();
		T m_value;
	public:
		constexpr SafeUnsigned() : m_value{ 0 } {}
		explicit constexpr SafeUnsigned(T t) : m_value{ t } {}

		constexpr T get() const {
			return m_value;
		}

		constexpr SafeUnsigned& operator=(const SafeUnsigned&) = default;
		constexpr SafeUnsigned& operator++() {
			zAssert(MAX_VALUE - m_value > 0);
			++m_value;
			return *this;
		}
		constexpr SafeUnsigned& operator--() {
			zAssert(m_value > 0);
			--m_value;
			return *this;
		}
		constexpr SafeUnsigned& operator+=(const SafeUnsigned& t) {
			zAssert(MAX_VALUE - m_value >= t.m_value);
			m_value += t.m_value;
			return *this;
		}
		constexpr SafeUnsigned& operator-=(const SafeUnsigned& t) {
			zAssert(m_value >= t.m_value);
			m_value -= t.m_value;
			return *this;
		}
		constexpr SafeUnsigned& operator*=(const SafeUnsigned& t) {
			if (m_value != 0) {
				zAssert(MAX_VALUE / m_value >= t.m_value);
			}
			m_value *= t.m_value;
			return *this;
		}
		constexpr SafeUnsigned& operator/=(const SafeUnsigned& t) {
			zAssert(t.m_value != 0);
			m_value /= t.m_value;
			return *this;
		}
		constexpr SafeUnsigned& operator|=(const SafeUnsigned& t) {
			m_value |= t.m_value;
			return *this;
		}
		constexpr SafeUnsigned& operator&=(const SafeUnsigned& t) {
			m_value &= t.m_value;
			return *this;
		}
		template<std::integral U>
		constexpr SafeUnsigned& operator>>=(U s) {
			*this = *this >> s;
			return *this;
		}
		template<std::integral U>
		constexpr SafeUnsigned& operator<<=(U s) {
			*this = *this << s;
			return *this;
		}

		constexpr auto operator<=>(const SafeUnsigned& t) const = default;

		template<std::integral U>
		constexpr SafeUnsigned operator<<(U s) const {
			zAssert(std::cmp_less(s, sizeof(T) * 8));
			return SafeUnsigned{ static_cast<T>(m_value << s) };
		}
		template<std::integral U>
		constexpr SafeUnsigned operator>>(U s) const {
			zAssert(std::cmp_less(s, sizeof(T) * 8));
			return SafeUnsigned{ static_cast<T>(m_value >> s) };
		}

		constexpr SafeUnsigned operator~() const {
			return SafeUnsigned{ static_cast<T>(~m_value) };
		}

		void subToMin(SafeUnsigned subbed, SafeUnsigned min) {
			zAssert(min <= *this);

			if (m_value < subbed.m_value) {
				m_value = min.get();
			} else {
				auto diff = *this - subbed;
				*this = diff < min ? min : diff;
			}
		}

		constexpr friend SafeUnsigned operator+(SafeUnsigned a, SafeUnsigned b) {
			zAssert(MAX_VALUE - a.m_value >= b.m_value);
			return SafeUnsigned{ static_cast<T>(a.m_value + b.m_value) }; //cast to stop integer promotion from kicking in
		}
		constexpr friend SafeUnsigned operator-(SafeUnsigned a, SafeUnsigned b) {
			zAssert(a.m_value >= b.m_value);
			return SafeUnsigned{ static_cast<T>(a.m_value - b.m_value) }; 
		}
		constexpr friend SafeUnsigned operator*(SafeUnsigned a, SafeUnsigned b) {
			if (a.m_value != 0) {
				zAssert(MAX_VALUE / a.m_value >= b.m_value);
			}
			return SafeUnsigned{ static_cast<T>(a.m_value * b.m_value) }; 
		}
		constexpr friend SafeUnsigned operator/(SafeUnsigned a, SafeUnsigned b) {
			zAssert(b.m_value != 0);
			return SafeUnsigned{ static_cast<T>(a.m_value / b.m_value) };
		}
		constexpr friend SafeUnsigned operator&(SafeUnsigned a, SafeUnsigned b) {
			return SafeUnsigned{ static_cast<T>(a.m_value & b.m_value) };
		}
		constexpr friend SafeUnsigned operator|(SafeUnsigned a, SafeUnsigned b) {
			return SafeUnsigned{ static_cast<T>(a.m_value | b.m_value) };
		}
		constexpr friend SafeUnsigned operator%(SafeUnsigned a, SafeUnsigned b) {
			zAssert(b.m_value != 0);
			return SafeUnsigned{ static_cast<T>(a.m_value % b.m_value) };
		}
	};

	template<std::unsigned_integral U, std::integral C>
	constexpr SafeUnsigned<U> convertImpl(C v) {
		zAssert(std::cmp_greater_equal(v, 0) && std::cmp_less_equal(v, std::numeric_limits<U>::max()));
		return SafeUnsigned{ static_cast<U>(v) };
	};

	export constexpr SafeUnsigned<std::uint8_t> operator""_su8(unsigned long long v) {
		return convertImpl<std::uint8_t>(v);
	}
}