export module Chess.UCI.UciCommand;

export import Chess.Position;
export import Chess.PositionCommand;
export import Chess.Position.RepetitionMap;
export import Chess.SafeInt;

export namespace chess {
	enum class UciCommandType {
		Stop,
		Quit,
		Position,
		Go
	};

	struct PositionCommand {
		Position pos;
		RepetitionMap repetitionMap;

		PositionCommand() = default;

		PositionCommand(const PositionCommandParseResult& parseRes) {
			pos.setPos(parseRes);
			repetitionMap.push(pos);
			for (const auto& move : parseRes.moves) {
				pos.move(move);
				repetitionMap.push(pos);
			}
		}
	};
	struct GoCommand {
		SafeInt<std::uint8_t> depth;
		std::chrono::milliseconds whiteTime;
		std::chrono::milliseconds blackTime;
		bool ponder = false;
	};

	class UciCommand {
	private:
		UciCommandType m_type{};
		std::variant<std::monostate, PositionCommand, GoCommand> m_data;

		template<typename T>
		static UciCommand makeCommandImpl(UciCommandType type, T&& data) {
			UciCommand ret;
			ret.m_type = type;
			ret.m_data = std::move(data);
			return ret;
		}
	public:
		UciCommand() : m_data{ std::monostate{} } {}

		static UciCommand quit() {
			UciCommand ret;
			ret.m_type = UciCommandType::Quit;
			ret.m_data = std::monostate{};
			return ret;
		}
		static UciCommand stop() {
			UciCommand ret;
			ret.m_type = UciCommandType::Stop;
			ret.m_data = std::monostate{};
			return ret;
		}
		static UciCommand position(PositionCommand posCommandData) {
			return makeCommandImpl(UciCommandType::Position, std::move(posCommandData));
		}
		static UciCommand go(GoCommand goCommandData) {
			return makeCommandImpl(UciCommandType::Go, std::move(goCommandData));
		}

		UciCommandType getType() const {
			return m_type;
		}

		const PositionCommand& getPositionCommand() const {
			return std::get<PositionCommand>(m_data);
		}
		const GoCommand& getGoCommand() const {
			return std::get<GoCommand>(m_data);
		}
	};
}