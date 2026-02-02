module;

#include <windows.h>

#undef min
#undef max

#include <boost/unordered/concurrent_flat_map.hpp>
#include <tracy/Tracy.hpp>

module Chess.MoveSearch:TranspositionTable;

import Chess.Assert;
import Chess.Square;
import :MoveHasher;

namespace chess {
	constexpr SafeInt<std::uint8_t> MAX_AGE{ 0b11111 };

	struct alignas(16) PackedTTEntry {
		static constexpr auto FROM_POS            = 58uz; //[58, 63] 6 bits
		static constexpr auto TO_POS              = 52uz; //[52, 57] 6 bits
		static constexpr auto PROMOTION_PIECE_POS = 48uz; //[48, 51] 4 bits
		static constexpr auto DEPTH_POS           = 40uz; //[40, 47] 8 bits
		static constexpr auto BOUND_POS           = 37uz; //[37, 39] 3 bits
		static constexpr auto AGE_POS             = 32uz; //[32, 36] 5 bits
		static constexpr auto RATING_POS          = 0uz;  //[0, 31] 32 bits

		std::atomic<std::uint64_t> key  = 0;
		std::atomic<std::uint64_t> data = 0;

		PackedTTEntry() = default;

		void reassign(const TTEntry& entry, std::uint64_t newHash) {
			auto temp = 0uz;
			temp |= static_cast<std::uint64_t>(entry.bestMove.from) << FROM_POS;
			temp |= static_cast<std::uint64_t>(entry.bestMove.to) << TO_POS;
			temp |= static_cast<std::uint64_t>(entry.bestMove.promotionPiece) << PROMOTION_PIECE_POS;
			temp |= static_cast<std::uint64_t>(entry.depth.get()) << DEPTH_POS;
			temp |= static_cast<std::uint64_t>(entry.bound) << BOUND_POS;
			temp |= static_cast<std::uint64_t>(entry.age.get()) << AGE_POS;
			temp |= static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>((entry.rating.get()))) << RATING_POS; //double cast to prevent sign extension

			key.store(temp ^ newHash);
			data.store(temp);
		}

		static TTEntry unpack(const Position& pos, std::uint64_t data) {
			TTEntry ret;

			constexpr std::uint64_t SQUARE_MASK = 0b111111;
			ret.bestMove.from = static_cast<Square>((data >> FROM_POS) & SQUARE_MASK);
			ret.bestMove.to = static_cast<Square>((data >> TO_POS) & SQUARE_MASK);

			ret.bestMove.movedPiece = pos.getAllies().findPiece(ret.bestMove.from);
			if (!pos.setEnPassant(ret.bestMove)) { //mutates the move, not the position
				ret.bestMove.capturedPiece = pos.getEnemies().findPiece(ret.bestMove.to);
			}

			constexpr std::uint64_t PIECE_MASK = 0b1111;
			ret.bestMove.promotionPiece = static_cast<Piece>((data >> PROMOTION_PIECE_POS) & PIECE_MASK);

			constexpr std::uint64_t BOUND_MASK = 0b111;
			ret.bound = static_cast<WindowBound>((data >> BOUND_POS) & BOUND_MASK);

			ret.rating = Rating{ std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(data >> RATING_POS)) };
			ret.depth = getDepth(data);
			ret.age = getAge(data);

			return ret;
		}

		static SafeInt<std::uint8_t> getAge(std::uint64_t tempData) {
			constexpr std::uint64_t AGE_MASK = 0b11111;
			return SafeInt{ static_cast<std::uint8_t>((tempData >> AGE_POS) & AGE_MASK) };
		}
		static SafeInt<std::uint8_t> getDepth(std::uint64_t tempData) {
			auto temp = tempData >> DEPTH_POS;
			return SafeInt{ static_cast<std::uint8_t>(temp) };
		}
	};

	size_t availableMemory() {
		MEMORYSTATUSEX status;
		status.dwLength = sizeof(status);
		GlobalMemoryStatusEx(&status);
		return static_cast<size_t>(status.ullAvailPhys);
	}

	class TranspositionTable {
	private:
		size_t m_entryCount = 0;
		std::unique_ptr<PackedTTEntry[]> m_entries;
		SafeInt<std::uint8_t> m_age{ 0 };
	public:
		TranspositionTable() {
			constexpr auto MAX_TT_SIZE_BYTES = 3'500'000'000uz; //todo: make this customizable 

			auto backoffFactor = 0.2;
			auto allocationsFailed = 0;
			constexpr auto MAX_FAILED_ALLOCATIONS = 5;

			while (true) {
				if (allocationsFailed > MAX_FAILED_ALLOCATIONS) {
					std::println("Allocation failed too many times. Shutting down.");
					std::exit(EXIT_FAILURE);
				}
				try {
					auto availableRamBytes = static_cast<size_t>(static_cast<double>(availableMemory()) * backoffFactor);
					availableRamBytes = std::min(availableRamBytes, MAX_TT_SIZE_BYTES);
					auto maxEntries = availableRamBytes / sizeof(PackedTTEntry);
					m_entries = std::make_unique<PackedTTEntry[]>(maxEntries);
					m_entryCount = maxEntries;
					break;
				} catch (const std::bad_alloc&) {
					std::println("More than 25% of last checked available ram was taken! Retrying transposition table allocation");
					backoffFactor *= 0.8; //ask for 20% less memory next time
					allocationsFailed++;
				}
			}
		}

		std::optional<TTEntry> operator[](const Position& pos) const {
			auto hashIndex = pos.hash() % m_entryCount;
			auto& entry = m_entries[hashIndex];

			auto tempKey = entry.key.load();
			auto tempData = entry.data.load();
			
			if ((tempKey ^ tempData) == pos.hash()) { //insane operator precedence rules
				return PackedTTEntry::unpack(pos, tempData);
			}
			return std::nullopt;
		}

		void insert(const Position& pos, TTEntry entry) {
			entry.age = m_age;
			auto index = pos.hash() % m_entryCount;
			auto& existingEntry = m_entries[index];

			auto data = existingEntry.data.load();
			auto existingDepth = PackedTTEntry::getDepth(data);

			if (PackedTTEntry::getAge(data) != m_age) {
				existingEntry.reassign(entry, pos.hash()); //store new entry if this is a newer search
			} else if (existingDepth <= entry.depth) { //age is the same, positions may be different
				existingEntry.reassign(entry, pos.hash());
			}
		}

		void reset() {
			m_age = 0_su8;
		}

		void updateAge() {
			if (m_age + 1_su8 > MAX_AGE) {
				m_age = 0_su8;
			} else {
				++m_age;
			}
		}
	};

	TranspositionTable& getTT() {
		static TranspositionTable table;
		return table;
	}

	std::optional<TTEntry> getPositionEntry(const Position& pos, SafeInt<std::uint8_t> depth) {
		ZoneScoped;

		const auto& table = getTT();
		auto entry = table[pos];
		if (entry && entry->depth >= depth) {
			return *entry;
		}
		return std::nullopt;
	}

	void storePositionEntry(const Position& pos, const TTEntry& entry) {
		ZoneScoped;

		auto& table = getTT();
		table.insert(pos, entry);
	}

	void updateTTAge() {
		getTT().updateAge();
	}

	void resetTranspositionTable() { 
		getTT().reset();
	}
}