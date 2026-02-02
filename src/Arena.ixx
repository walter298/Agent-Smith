module;

#include <boost/container/static_vector.hpp>

export module Chess.Arena;

export import std;
export import BS.thread_pool;

import Chess.Assert;

namespace chess {
	//namespace arena {
	//	export class MemoryRegion {
	//	private:
	//		void* m_begin = nullptr;
	//		void* m_nextObjectBegin = nullptr;
	//		size_t m_capacity = 0;
	//		size_t m_space = 0;
	//	public:
	//		MemoryRegion() = default;
	//		MemoryRegion(void* begin, size_t capacity) noexcept
	//			: m_begin{ begin }, m_nextObjectBegin{ begin }, m_capacity{ capacity }, m_space{ capacity }
	//		{
	//		}

	//		inline std::byte* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t)) noexcept {
	//			if (std::align(alignment, bytes, m_nextObjectBegin, m_space)) {
	//				auto temp = m_nextObjectBegin;
	//				m_nextObjectBegin = static_cast<std::byte*>(m_nextObjectBegin) + bytes;
	//				m_space -= bytes;
	//				return static_cast<std::byte*>(temp);
	//			} else {
	//				std::abort();
	//			}
	//			std::unreachable();
	//		}

	//		void* getOffset() const {
	//			return m_nextObjectBegin;
	//		}
	//		void resetToOffset(void* newOffset) {
	//			m_nextObjectBegin = newOffset;
	//			m_space = m_capacity - static_cast<size_t>(static_cast<std::byte*>(newOffset) - static_cast<std::byte*>(m_begin));
	//		}

	//		void reset() noexcept {
	//			m_space = m_capacity;
	//			m_nextObjectBegin = m_begin;
	//		}

	//		size_t getTotalCapacity() const noexcept {
	//			return m_capacity;
	//		}

	//		size_t getSpace() const noexcept {
	//			return m_space;
	//		}

	//		size_t bytesAvailable() const noexcept {
	//			return m_space;
	//		}
	//	};

	//	export MemoryRegion* getMemoryRegion();
	//	export void init();
	//	export void resetThread();
	//	export void resetAllThreads();
	//	export void registerThread(std::jthread::id id);
	//	
	//	void* allocateImpl(size_t byteCount, size_t alignment);

	//	export template<typename T>
	//	struct Allocator {
	//		std::array<T, 219> buff;

	//		using value_type = T;

	//		Allocator() = default;

	//		template<typename U>
	//		Allocator(Allocator<U>) {}

	//		T* allocate(size_t n) const {
	//			return static_cast<T*>(allocateImpl(n * sizeof(T), alignof(T)));
	//		}

	//		void deallocate(T*, size_t) const {}

	//		template<typename U>
	//		struct rebind {
	//			using other = Allocator<U>;
	//		};

	//		friend bool operator==(const Allocator&, const Allocator&) {
	//			return true;
	//		}
	//	};

	//	//export template<typename T>
	//	//using Vector = boost::container::static_vector<T, 219>;
	//}

	export template<typename T, size_t Capacity = 219>
	class StaticVector {
	private:
		using Buffer = std::array<T, Capacity>;
		Buffer m_data;
		size_t m_size = 0;
	public:
		StaticVector() = default;

		template<typename R>
		StaticVector(R&& range) {
			std::ranges::copy(range, m_data.begin());
			m_size = std::ranges::size(range);
		}

		constexpr size_t size() const {
			return m_size;
		}
		constexpr bool empty() const {
			return m_size == 0uz;
		}

		auto& operator[](this auto&& self, size_t n) {
			zAssert(n < self.m_size);
			return self.m_data[n];
		}

		template<typename... Ts>
		constexpr auto& add(Ts&&... ts) {
			zAssert(m_size < Capacity);

			auto& entry = m_data[m_size];
			entry = { std::forward<Ts>(ts)... };
			++m_size;
			return entry;
		}

		auto begin(this auto&& self) {
			return self.m_data.begin();
		}
		auto end(this auto&& self) {
			return self.m_data.begin() + self.m_size;
		}
	};
}