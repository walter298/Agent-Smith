module;

#include <magic_enum/magic_enum.hpp>

module Chess.Move;

import Chess.Square;

namespace chess {
	std::string Move::getUCIString() const { 
		auto fromName = magic_enum::enum_name(from);
		auto toName   = magic_enum::enum_name(to);

		std::string ret;
		ret.append(std::string{ fromName.data(), fromName.size() });
		ret.append(std::string{ toName.data(), toName.size() });
		
		ret[0] = static_cast<char>(std::tolower(ret[0]));
		ret[2] = static_cast<char>(std::tolower(ret[2]));

		if (promotionPiece != Piece::None) {
			if (promotionPiece == Knight) { //Knight doesn't start with n, so we need a special branch
				ret.push_back('n');
			} else { 
				auto promotedPieceName = magic_enum::enum_name(promotionPiece);
				ret.push_back(static_cast<char>(std::tolower(promotedPieceName[0])));
			}
		}

		return "bestmove " + ret;
	}
}