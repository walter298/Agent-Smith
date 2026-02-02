export module Chess.Position.Castling;

import std;

import Chess.SafeInt;

export namespace chess {
	class CastlingPrivileges {
	private:
		static constexpr auto CAN_KINGSIDE_INDEX = 0;
		static constexpr auto HAS_KINGSIDE_INDEX = 1;
		static constexpr auto CAN_QUEENSIDE_INDEX = 2;
		static constexpr auto HAS_QUEENSIDE_INDEX = 3;
		static constexpr SafeInt<std::uint8_t> DEFAULT{ 0b0101 };

		SafeInt<std::uint8_t> m_data{ DEFAULT };

		template<auto Index>
		constexpr bool getBit() const {
			return ((SafeInt<std::uint8_t>{ 1 } << Index) & m_data) != 0_su8;
		}
		template<auto Index>
		constexpr void setBit() {
			m_data |= (SafeInt<std::uint8_t>{ 1 } << Index);
		}
		template<auto Index>
		constexpr void unsetBit() {
			m_data &= ~(SafeInt<std::uint8_t>{ 1 } << Index);
		}
	public:
		SafeInt<std::uint8_t> get() const {
			return m_data;
		}
		constexpr bool canCastleKingside() const {
			return getBit<CAN_KINGSIDE_INDEX>();
		}
		constexpr bool hasCastledKingside() const {
			return getBit<HAS_KINGSIDE_INDEX>();
		}
		constexpr void disallowKingsideCastling() {
			unsetBit<CAN_KINGSIDE_INDEX>();
		}
		constexpr bool canCastleQueenside() const {
			return getBit<CAN_QUEENSIDE_INDEX>();
		}
		constexpr bool hasCastledQueenside() const {
			return getBit<HAS_QUEENSIDE_INDEX>();
		}
		constexpr void disallowQueensideCastling() {
			unsetBit<CAN_QUEENSIDE_INDEX>();
		}
		void castleKingside() {
			disallowQueensideCastling();
			disallowKingsideCastling();
			setBit<HAS_KINGSIDE_INDEX>();
		}
		void castleQueenside() {
			disallowKingsideCastling();
			disallowQueensideCastling();
			setBit<HAS_QUEENSIDE_INDEX>();
		}
		void reset() {
			m_data = DEFAULT;
		}
	};
}