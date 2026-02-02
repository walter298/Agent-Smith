//module;
//
//#include <boost/unordered/unordered_flat_map.hpp>
//
//module Chess.Arena;
//
//import Chess.Assert;
//import Chess.DebugPrint;
//
//namespace chess {
//	namespace arena {
//		size_t globalByteCount = 0;
//		std::unique_ptr<std::byte[]> buff;
//		boost::unordered_flat_map<std::jthread::id, MemoryRegion, std::hash<std::jthread::id>> threadRegions;
//
//		constexpr auto THREAD_BYTE_COUNT = 16'000'000uz;
//
//		void init() {
//			auto threadCount = static_cast<size_t>(std::thread::hardware_concurrency() + 1); //add one for main thread; STATIC CAST IS CRUCIAL!!!!!!
//			globalByteCount = THREAD_BYTE_COUNT * threadCount;
//			buff = std::make_unique<std::byte[]>(globalByteCount);
//
//			std::ofstream file{ "foo.txt" };
//			file << std::to_string(globalByteCount);
//		}
//
//		void resetThread() {
//			auto& region = threadRegions.at(std::this_thread::get_id());
//			region.reset();
//		}
//		void resetAllThreads() {
//			for (auto& [id, region] : threadRegions) {
//				region.reset();
//			}
//		}
//
//		//is only called once per thread
//		void registerThread(std::jthread::id id) {
//			static size_t threadIndex = 0;
//
//			debugPrint(std::format("registerThread called: {}", threadIndex));
//
//			auto offset = threadIndex * THREAD_BYTE_COUNT;
//			if (offset >= globalByteCount) {
//				debugPrint(std::format("Error: not enough space when registering {}", id));
//				debugPrint(std::format("Offset: {}; Space: {}", offset, globalByteCount));
//				debugPrintFlush();
//				std::exit(-1);
//			}
//			auto begin = buff.get() + (threadIndex * THREAD_BYTE_COUNT);
//			threadRegions.emplace(id, MemoryRegion{ begin, THREAD_BYTE_COUNT });
//			threadIndex++;
//		}
//
//		MemoryRegion* getMemoryRegion() {
//			return &threadRegions.at(std::this_thread::get_id());
//		}
//
//		void* allocateImpl(size_t byteCount, size_t alignment) {
//			thread_local auto& region = threadRegions.at(std::this_thread::get_id());
//			return region.allocate(byteCount, alignment);
//		}
//	}
//}