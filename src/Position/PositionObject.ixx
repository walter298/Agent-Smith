export module Chess.Position:PositionObject;

export import std;

export import Chess.Move;
import Chess.PositionCommand;
import Chess.Position.PieceState;
import Chess.RankCalculator;

export namespace chess {
	class Position {
	private:
		struct CastleMove {
			Square kingTo = Square::None;
			Square rookFrom = Square::None;
			Square rookTo = Square::None;
			Bitboard squaresBetweenRookAndKing = 0_bb;
			Bitboard mandatoryUncheckedSquares = 0_bb;
		};
		static constexpr CastleMove WHITE_KINGSIDE = {Square::G1, Square::H1, Square::F1, makeBitboard(Square::F1, Square::G1), makeBitboard(Square::F1, Square::G1) };
		static constexpr CastleMove WHITE_QUEENSIDE = {Square::C1, Square::A1, Square::D1, makeBitboard(Square::B1, Square::C1, Square::D1), makeBitboard(Square::C1, Square::D1) };
		static constexpr CastleMove BLACK_KINGSIDE = {Square::G8, Square::H8, Square::F8, makeBitboard(Square::F8, Square::G8), makeBitboard(Square::F8, Square::G8) };
		static constexpr CastleMove BLACK_QUEENSIDE = {Square::C8, Square::A8, Square::D8, makeBitboard(Square::B8, Square::C8, Square::D8), makeBitboard(Square::C8, Square::D8) };

		template<typename MaybeConstPieceState>
		struct TurnData {
			MaybeConstPieceState& allies;
			MaybeConstPieceState& enemies;
			const CastleMove& allyKingside;
			const CastleMove& allyQueenside;
			const CastleMove& enemyKingside;
			const CastleMove& enemyQueenside;
			bool isWhite = true;
			Bitboard allyPawnRank = 0;
			Bitboard jumpedAllyPawnRank = 0;

			operator TurnData<const PieceState>() const {
				return {
					allies, enemies, allyKingside, allyQueenside, enemyKingside, enemyQueenside,
					isWhite, allyPawnRank, jumpedAllyPawnRank
				};
			}
		};
	public:
		using MutableTurnData = TurnData<PieceState>;
		using ImmutableTurnData = TurnData<const PieceState>;
	private:
		PieceState m_whitePieces;
		PieceState m_blackPieces;
		bool m_isWhiteMoving = true;
		std::uint64_t m_zobristHash = 0;

		template<typename MaybeConstPieceState>
		TurnData<MaybeConstPieceState> getTurnDataImpl(this auto&& self) {
			if (self.m_isWhiteMoving) {
				return TurnData<MaybeConstPieceState>{
					self.m_whitePieces, self.m_blackPieces,
						WHITE_KINGSIDE, WHITE_QUEENSIDE,
						BLACK_KINGSIDE, BLACK_QUEENSIDE,
						self.m_isWhiteMoving,
						calcRank<2>(), calcRank<4>()
				};
			} else {
				return TurnData<MaybeConstPieceState>{
					self.m_blackPieces, self.m_whitePieces,
						BLACK_KINGSIDE, BLACK_QUEENSIDE,
						WHITE_KINGSIDE, WHITE_QUEENSIDE,
						self.m_isWhiteMoving,
						calcRank<7>(), calcRank<5>()
				};
			}
		}
		bool tryCastle(MutableTurnData& turnData, const Move& move);
		void movePawn(const MutableTurnData& turnData, const Move& move, Bitboard& pawns);
		void capturePiece(const MutableTurnData& turnData, const Move& move);
		void normalMove(MutableTurnData& turnData, const Move& move);
	public:
		Position() = default;
		Position(Position&&) noexcept = default;
		Position(const Position&) = default;
		Position& operator=(const Position&) = default;

		Position(const Position& pos, const Move& move) {
			*this = pos;
			this->move(move);
		}

		void setPos(const PositionCommand& positionCommand);

		void move(const Move& move);
		void move(std::string_view moveStr);

		size_t hash() const {
			return m_zobristHash;
		}

		TurnData<const PieceState> getTurnData() const {
			return getTurnDataImpl<const PieceState>();
		}
		TurnData<PieceState> getTurnData() {
			return getTurnDataImpl<PieceState>();
		}

		template<bool Maximizing = true>
		auto getColorSides(this auto&& self) {
			if constexpr (Maximizing) {
				return std::tie(self.m_whitePieces, self.m_blackPieces);
			}
			else {
				return std::tie(self.m_blackPieces, self.m_whitePieces);
			}
		}

		bool isWhite() const {
			return m_isWhiteMoving;
		}

		int pieceCount() const {
			auto [white, black] = getColorSides();
			auto whitePieces = white.calcAllLocations();
			auto blackPieces = black.calcAllLocations();
			return std::popcount(whitePieces) + std::popcount(blackPieces);
		}

		auto& getAllies(this auto&& self) {
			return self.m_isWhiteMoving ? self.m_whitePieces : self.m_blackPieces;
		}
		auto& getEnemies(this auto&& self) {
			return self.m_isWhiteMoving ? self.m_blackPieces : self.m_whitePieces;
		}

		bool setEnPassant(Move& move, const ImmutableTurnData& turnData) const;
		bool setEnPassant(Move& move) const;
	};

	struct PositionHasher {
		size_t operator()(const Position& pos) const {
			return pos.hash();
		}
	};
	struct PositionComp {
		bool operator()(const Position& p1, const Position& p2) const {
			return p1.hash() == p2.hash(); //possibility of a key duplicate is insanely unlikely 
		}
	};
}