export module Chess.MoveSearch:Node;

import Chess.Arena;
export import Chess.Position;
export import Chess.Position.RepetitionMap;
export import Chess.Rating;
export import Chess.Evaluation;
export import Chess.MoveGeneration;
export import Chess.SafeInt;
export import :MovePriority;

export namespace chess {
	class Node {
	private:
		Position m_pos;
		PositionData m_positionData;
		std::reference_wrapper<RepetitionMap> m_repetitionMap;
		SafeInt<std::uint8_t> m_level{ 0 };
		SafeInt<std::uint8_t> m_levelsToSearch{ 0 };
		bool m_inAttackSequence = false;
		Rating m_materialExchanged = 0_rt;
		Rating m_materialSignSwap = 1_rt;
		bool m_isChild = true;
		
		Node(const Position& pos, RepetitionMap& repetitionMap)
			: m_pos{ pos }, m_positionData{ calcPositionData(pos) }, m_repetitionMap{ repetitionMap }
		{
		};
	public:
		Node(const Position& root, SafeInt<std::uint8_t> maxDepth, RepetitionMap& repetitionMap)
			: Node{ root, repetitionMap }
		{
			m_levelsToSearch = maxDepth;
			m_isChild = false;
			if (!root.isWhite()) {
				m_materialSignSwap *= -1_rt;
			}
		}
		Node(const Node& parent, const MovePriority& movePriority)
			: Node{ Position{ parent.m_pos, movePriority.getMove() }, parent.m_repetitionMap }
		{
			m_repetitionMap.get().push(m_pos);
			m_positionData = calcPositionData(m_pos);
			m_level = parent.m_level + 1_su8;
			m_materialSignSwap *= -1_rt;
			m_levelsToSearch = movePriority.getDepth();
		}

		~Node() {
			if (m_isChild) {
				m_repetitionMap.get().pop(m_pos);
			}
		}

		bool inAttackSequence() const {
			return m_inAttackSequence;
		}

		bool inWinningAttackSequence() const {
			return m_materialSignSwap > 0_rt ? m_materialExchanged > 0_rt : m_materialExchanged < 0_rt;
		}

		bool inLosingAttackSequence() const {
			return m_materialSignSwap > 0_rt ? m_materialExchanged < 0_rt : m_materialExchanged > 0_rt;
		}

		const RepetitionMap& getRepetitionMap() const {
			return m_repetitionMap.get();
		}

		const Position& getPos() const {
			return m_pos;
		}
		const PositionData& getPositionData() const {
			return m_positionData;
		}

		SafeInt<std::uint8_t> getLevel() const {
			return m_level;
		}

		SafeInt<std::uint8_t> getRemainingDepth() const {
			return m_levelsToSearch;
		}

		bool isDone() const {
			return m_levelsToSearch == 0_su8;
		}

		Rating getRating() const {
			return staticEvaluation(m_pos, m_positionData);
		}

		const PieceState& getAllies() const {
			auto [white, black] = m_pos.getColorSides();
			return m_pos.isWhite() ? white : black;
		}
	};
}