export module Chess.Position:PositionData;

export import std;

import Chess.Arena;
export import Chess.Bitboard;
export import Chess.Move;
export import Chess.MoveGen;
export import Chess.PieceMap;
export import Chess.Rating;
export import Chess.Position.PieceState;

export namespace chess {
	using MoveVector = StaticVector<Move>;

	struct DestinationSquareData {
		Bitboard destSquaresPinConsidered = 0;
		Bitboard allDestSquares = 0;
	};
	struct PositionData {
	private:
		DestinationSquareData* m_allySquares = nullptr;
		DestinationSquareData* m_enemySquares = nullptr;
	public:
		MoveVector legalMoves;
		DestinationSquareData whiteSquares;
		DestinationSquareData blackSquares;
		
		bool isCheck = false;
		
		constexpr PositionData(bool isWhite) {
			if (isWhite) {
				m_allySquares = &whiteSquares;
				m_enemySquares = &blackSquares;
			} else {
				m_allySquares = &blackSquares;
				m_enemySquares = &whiteSquares;
			}
		}
		auto& getAllySquares(this auto&& self) {
			return *self.m_allySquares;
		}
		auto& getEnemySquares(this auto&& self) {
			return *self.m_enemySquares;
		}

		DestinationSquareData allAllySquares() const {
			return *m_allySquares;
		}
		DestinationSquareData allEnemySquares() const {
			return *m_enemySquares;
		}
		bool isCheckmate() const {
			return legalMoves.empty() && isCheck;
		}
	};
}