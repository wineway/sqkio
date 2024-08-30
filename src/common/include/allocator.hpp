#ifndef SQK_COMMON_ALLOCATOR_HPP_
#define SQK_COMMON_ALLOCATOR_HPP_

#include <cstdint>
#include <new>
#include <stack>

#include "log.hpp"
#include "utilty.h"

namespace sqk::common {

template <typename T>
struct PoolAllocatable {
    inline void* operator new(std::size_t size) {
        return T::operator new(size);
    }

    inline void* operator new[](std::size_t size) {
        return T::operator new[](size);
    }

    inline void*
    operator new(std::size_t size, const std::nothrow_t& nothrow) noexcept {
        return T::operator new(size, nothrow);
    }

    inline void*
    operator new[](std::size_t size, const std::nothrow_t& nothrow) noexcept {
        return T::operator new[](size, nothrow);
    }

    inline void operator delete(void* ptr) noexcept {
        T::operator delete(ptr);
    }

    inline void operator delete[](void* ptr) noexcept {
        T::operator delete[](ptr);
    }

    inline void operator delete(void* ptr, const std::nothrow_t& nothrow) noexcept {
        T::operator delete(ptr, nothrow);
    }

    inline void operator delete[](void* ptr, const std::nothrow_t& nothrow) noexcept {
        T::operator delete[](ptr, nothrow);
    }

    // void* operator new(std::size_t size, std::align_val_t);
    // void*
    // operator new(std::size_t size, std::align_val_t, const std::nothrow_t&) noexcept;
    // void* operator new[](std::size_t size, std::align_val_t);
    // void* operator new
    //     [](std::size_t size, std::align_val_t, const std::nothrow_t&) noexcept;
    // void operator delete(void* ptr, std::align_val_t) noexcept;
    // void
    // operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept;
    // void
    // operator delete(void* ptr, std::size_t size, std::align_val_t al) noexcept;
    // void operator delete[](void* ptr, std::align_val_t) noexcept;
    // void operator delete
    //     [](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept;
    // void operator delete[](
    //     void* ptr,
    //     std::size_t size,
    //     std::align_val_t al
    // ) noexcept;
};

struct TlsfPoolAllocator: public PoolAllocatable<TlsfPoolAllocator> {
    void* operator new(std::size_t size);
    void* operator new[](std::size_t size);
    void* operator new(std::size_t size, const std::nothrow_t&) noexcept;
    void* operator new[](std::size_t size, const std::nothrow_t&) noexcept;
    void operator delete(void* ptr) noexcept;
    void operator delete[](void* ptr) noexcept;
    void operator delete(void* ptr, const std::nothrow_t&) noexcept;
    void operator delete[](void* ptr, const std::nothrow_t&) noexcept;
};

#define ROUNDUP(_x, _v) ((((~(_x)) + 1) & ((_v) - 1)) + (_x))

struct SlabPoolAllocator: public PoolAllocatable<SlabPoolAllocator> {
    void* operator new(std::size_t size) {
        void *ptr;
        ptr = slab_alloc(size);
        if (unlikely(!ptr)) {
            throw std::bad_alloc();
        }
        return ptr;
    }
    void* operator new[](std::size_t size) { return ::operator new[](size); }
    void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
        return slab_alloc(size);
    }
    void* operator new[](std::size_t size, const std::nothrow_t& nothrow) noexcept { return ::operator new[](size, nothrow); }
    void operator delete(void* ptr) noexcept {
        slab_dealloc(ptr);
    }
    void operator delete[](void* ptr) noexcept { return ::operator delete[](ptr); }
    void operator delete(void* ptr, const std::nothrow_t&) noexcept {
        slab_dealloc(ptr);
    }
    void operator delete[](void* ptr, const std::nothrow_t& nothrow) noexcept { return ::operator delete[](ptr, nothrow); }
private:
    static constexpr uint16_t SLAB_MAGIC = 0b1010101001010101;
    static constexpr uint32_t MAX_CLASS_SHIFT = 9;
    struct SlabHeader {
        std::size_t ind_: MAX_CLASS_SHIFT + 1;
        std::size_t magic_: 16;
    };
    static void *slab_alloc(std::size_t size) noexcept {
        void* ptr;
        auto rsize = roundup(size);
        auto ind = size2ind(rsize);
        // fast path
        if (likely(ind < LOOKUP_MAX_CLASS && !tcache_[ind].empty())) {
            ptr = tcache_[ind].top();
            tcache_[ind].pop();
        } else {
            ptr = static_cast<uint8_t*>(::operator new(rsize + SLAB_OVERHEAD)) + SLAB_OVERHEAD;
            SlabHeader* header = ptr2header(ptr);
            header->ind_ = std::min(ind, LOOKUP_MAX_CLASS);
#ifndef NDEBUG
            header->magic_ = SLAB_MAGIC;
#endif // !NDEBUG
        }
        return ptr;
    }

    static void slab_dealloc(void* ptr) noexcept {
        SlabHeader* header = ptr2header(ptr);
        S_ASSERT(header->magic_ == SLAB_MAGIC);
        auto ind = header->ind_;
        if (likely(ind < LOOKUP_MAX_CLASS)) {
            tcache_[ind].push(ptr);
        } else {
            ::operator delete(header);
        }
    }
    constexpr static std::allocator<uint8_t> allocator {};
    static constexpr std::size_t MAX_FRAME_SIZE = 4UL << 10;
    static constexpr std::size_t BASE_SHIFT = 3;
    static constexpr std::size_t BASE_SIZE = 1UL << BASE_SHIFT;
    static constexpr std::size_t SLAB_OVERHEAD = sizeof(SlabHeader);
    static_assert(sizeof(SlabHeader) == sizeof(uint64_t), "");
    static constexpr uint32_t LOOKUP_MAX_CLASS = MAX_FRAME_SIZE >> BASE_SHIFT;
    static_assert(1UL << MAX_CLASS_SHIFT == LOOKUP_MAX_CLASS, "");
    static inline thread_local std::stack<void*> tcache_[LOOKUP_MAX_CLASS];
    static inline std::size_t roundup(std::size_t size) {
        return ROUNDUP(size, BASE_SIZE);
    }
    static inline uint32_t size2ind(std::size_t size) {
       return size >> BASE_SHIFT;
    }
    static inline SlabHeader* ptr2header(void* ptr) {
        return reinterpret_cast<SlabHeader*>(static_cast<uint8_t*>(ptr) - SLAB_OVERHEAD);
    }
};

} // namespace sqk::common
#endif // !SQK_COMMON_ALLOCATOR_HPP_
