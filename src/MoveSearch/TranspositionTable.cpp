module;

#include <windows.h>

#undef min
#undef max

#include <boost/unordered/concurrent_flat_map.hpp>
#include <tracy/Tracy.hpp>

module Chess.MoveSearch:TranspositionTable;

import Chess.Assert;
import Chess.EnvironmentVariable;
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
		static int calcScore(std::uint64_t tempData) {
			return 1 << getDepth(tempData).get();
		}
	};

	using EntryBucket = std::array<PackedTTEntry, 3>;
	
	size_t availableMemory() {
		MEMORYSTATUSEX status;
		status.dwLength = sizeof(status);
		GlobalMemoryStatusEx(&status);
		return static_cast<size_t>(status.ullAvailPhys);
	}

	class TranspositionTable {
	private:
		size_t m_entryCount = 0;
		std::unique_ptr<EntryBucket[]> m_entries;
		SafeInt<std::uint8_t> m_age{ 0 };
		//std::atomic_int m_cacheMissCount = 0;
		//std::atomic_int m_cacheHitCount = 0;
		//std::atomic_int m_writeCount = 0;
	public:
		TranspositionTable() {
			auto backoffFactor = 0.8;
			auto allocationsFailed = 0;
			constexpr auto MAX_FAILED_ALLOCATIONS = 5;

			while (true) {
				if (allocationsFailed > MAX_FAILED_ALLOCATIONS) {
					std::println("Allocation failed too many times. Shutting down.");
					std::exit(EXIT_FAILURE);
				}
				try {
					auto availableRamBytes = static_cast<size_t>(static_cast<double>(availableMemory()) * backoffFactor);
					auto maxEntries = std::bit_floor(availableRamBytes / sizeof(EntryBucket)); //make size a power of 2
					m_entries = std::make_unique<EntryBucket[]>(maxEntries); //could fail
					m_entryCount = maxEntries;
					break;
				} catch (const std::bad_alloc&) {
					std::println("More than {}% of last checked available ram was taken! Retrying transposition table allocation", backoffFactor);
					backoffFactor *= 0.8; //ask for 20% less memory next time
					allocationsFailed++;
				}
			}

			reset();
		}

		void flushData() const {
			//std::ofstream file{ getAssetDirectoryPath() / "transposition_table_data.txt" };
			//zAssert(file.is_open());
			//auto hitCount = m_cacheHitCount.load();
			//auto missCount = m_cacheMissCount.load();
			//auto writeCount = m_writeCount.load();
			//file << "Cache hit count: " << hitCount << '\n';
			//file << "Cache miss count: " << missCount << '\n';
			//file << "Cache miss percentage: " << static_cast<double>(missCount) / static_cast<double>(missCount + hitCount) << '\n';
			//file << "Write count: " << writeCount << '\n';
			//file << "Read percentage: " << static_cast<double>(writeCount) / static_cast<double>(missCount + hitCount) << '\n';
		}

		std::optional<TTEntry> operator[](const Position& pos) const {
			auto hashIndex = pos.hash() & (m_entryCount - 1);
			auto& bucket = m_entries[hashIndex];

			std::uint64_t bestEntry = 0;
			auto bestScore = 0;

			for (const auto& entry : bucket) {
				auto tempKey = entry.key.load();
				auto tempData = entry.data.load();
				if ((tempKey ^ tempData) == pos.hash()) { //insane operator precedence rules
					auto entryScore = PackedTTEntry::calcScore(tempData);
					if (entryScore > bestScore) {
						bestScore = entryScore;
						bestEntry = tempData;
					}
				}
			}

			if (bestEntry != 0) {
				return PackedTTEntry::unpack(pos, bestEntry);
			}

			return std::nullopt;
		}

		void insert(const Position& pos, TTEntry entry) {
			//++m_writeCount;

			entry.age = m_age;
			auto index = pos.hash() & (m_entryCount - 1);
			auto& bucket = m_entries[index];

			std::ptrdiff_t worstIndex = -1;
			auto worstScore = std::numeric_limits<int>::max();
			
			for (auto&& [i, packedEntry] : bucket | std::views::enumerate) {
				auto tempKey = packedEntry.key.load();
				auto tempData = packedEntry.data.load();

				if ((tempKey ^ tempData) == pos.hash()) {
					if (PackedTTEntry::getAge(tempData) == m_age && PackedTTEntry::getDepth(tempData) > entry.depth) {
						return; 
					}
					packedEntry.reassign(entry, pos.hash());
					return;
				}

				if (tempKey == 0 && tempData == 0) {
					worstIndex = i;
				}

				auto score = PackedTTEntry::calcScore(tempData);
				if (score < worstScore) {
					worstScore = score;
					worstIndex = i;
				}
			}

			if (worstIndex != -1) {
				bucket[worstIndex].reassign(entry, pos.hash());
			}
		}

		void reset() {
			m_age = 0_su8;

			std::span span{ m_entries.get(), m_entries.get() + m_entryCount };
			for (auto& entry : std::views::join(span)) {
				entry.key.store(0);
				entry.data.store(0);
			}
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

	std::optional<TTEntry> getPositionEntry(const Position& pos) {
		ZoneScoped;

		auto& table = getTT();
		auto entry = table[pos];
		if (entry) {
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

	void flushTranspositionTableData() {
		getTT().flushData();
	}

	void resetTranspositionTable() { 
		getTT().reset();
	}
}