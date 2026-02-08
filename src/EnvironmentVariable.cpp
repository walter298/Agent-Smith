module;

#include <cstdlib>
#include <cstdio>

module Chess.EnvironmentVariable;

namespace chess {
	std::filesystem::path getEnvironmentVariableImpl(std::string_view var) {
		constexpr auto MAX_ENV_LEN = 256;
		std::array<char, MAX_ENV_LEN> buff;

		auto envLen = 0uz;
		auto res = getenv_s(&envLen, buff.data(), MAX_ENV_LEN, var.data());
		if (res != 0 || envLen == 0) {
			std::println("Error getting {} environment variable", var);
			std::fflush(stdout);
			std::exit(EXIT_FAILURE);
		}
		return std::filesystem::path{ std::string_view{ buff.data(), envLen - 1 } }; //subtract 1 for null terminator
	}

	std::filesystem::path getAssetDirectoryPath() {
		return getEnvironmentVariableImpl("CHESS_ASSET_DIR");
	}
}