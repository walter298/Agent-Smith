export module Chess.PositionCommand;

import std;

export namespace chess {
	constexpr std::string_view STARTING_FEN_STRING = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

	struct PositionCommandParseResult {
		std::string board;
		char color = 'w';
		std::string castlingPrivileges;
		std::string enPessantSquare;
		std::vector<std::string> moves;
	};

	PositionCommandParseResult parsePositionCommand(const std::string& uciCommandStr);
}