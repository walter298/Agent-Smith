export module Chess.MoveSearch:MovePriority;

import std;

export import Chess.Bitboard;
export import Chess.Evaluation;
export import Chess.Move;
export import Chess.SafeInt;
export import Chess.Rating;

namespace chess {
	Rating calcExchangeRating(const Move& m, Bitboard enemySquares) {
		auto ret = 0_rt;

		if (makeBitboard(m.to) & enemySquares) { //enemySquares are not piece locations - they are where the enemies can move to
			ret -= getPieceRating(m.movedPiece);
		}
		if (m.promotionPiece != Piece::None) {
			ret += getPieceRating(m.promotionPiece);
		}
		if (m.capturedPiece != Piece::None) {
			ret += getPieceRating(m.capturedPiece);
		}
		return ret;
	}

	export class MovePriority {
	private:
		Move m_move;
		Rating m_exchangeRating = 0_rt;
		SafeUnsigned<std::uint8_t> m_recommendedDepth{ 0 };
		bool m_trimmed = false;
	public:
		MovePriority() = default;

		MovePriority(const Move& move, Bitboard enemySquares, SafeUnsigned<std::uint8_t> depth) :
			m_move{ move }, m_recommendedDepth{ depth - 1_su8 },
			m_exchangeRating{ calcExchangeRating(move, enemySquares) }
		{
		}
		MovePriority(const Move& move, SafeUnsigned<std::uint8_t> depth)
			: m_move{ move }, m_recommendedDepth { depth }
		{
		}
	private:
		static constexpr auto MAX_MOVE_COUNT = 219z; //actual value is 218, but we want to make up for illegal zero indexing

		static constexpr std::array<double, MAX_MOVE_COUNT> makeLog2Table() {
			std::array<double, MAX_MOVE_COUNT> table{};
			for (auto&& [i, n] : std::views::enumerate(table) | std::views::drop(1)) { //drop 0 because log2(0) is undefined
				n = std::log2(static_cast<double>(i));
			}
			return table;
		}
	public:
		void trim(SafeUnsigned<std::uint8_t> moveIndex) {
			m_trimmed = true;

			static const auto LOG_TABLE = makeLog2Table();
			auto logDepth = LOG_TABLE[(m_recommendedDepth + 1_su8).get()];
			auto logI = LOG_TABLE[(moveIndex + 1_su8).get()];
			SafeUnsigned reducedDepth{ static_cast<std::uint8_t>(0.99 + (logDepth * logI / 3.14)) }; //add 0.5 to round up

			auto subbed = m_recommendedDepth + 1_su8; //m_recommended depth is actually the original depth - 1
			subbed.subToMin(reducedDepth, 0_su8);
			
			m_recommendedDepth = std::min(m_recommendedDepth, subbed);
		}

		bool isTrimmed() const {
			return m_trimmed;
		}

		const Move& getMove() const {
			return m_move;
		}
		SafeUnsigned<std::uint8_t> getDepth() const {
			return m_recommendedDepth;
		}
		Rating getExchangeRating() const {
			return m_exchangeRating;
		}
		std::string getString() const {
			return std::format("[{}, {}]", m_move.getUCIString(), static_cast<unsigned int>(m_recommendedDepth.get()));
		}
	};
}