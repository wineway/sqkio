#ifndef SQK_COMMON_ALLOCATOR_HPP_
#define SQK_COMMON_ALLOCATOR_HPP_

#include <new>

#include <tlsf.h>

namespace sqk::common {

struct PoolAllocatable {
    void* operator new(std::size_t size);
    void* operator new[](std::size_t size);
    void* operator new(std::size_t size, const std::nothrow_t&) noexcept;
    void* operator new[](std::size_t size, const std::nothrow_t&) noexcept;
    void operator delete(void* ptr) noexcept;
    void operator delete[](void* ptr) noexcept;
    void operator delete(void* ptr, const std::nothrow_t&) noexcept;
    void operator delete[](void* ptr, const std::nothrow_t&) noexcept;

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

template <bool IsNoExcept>
void *
newImpl(std::size_t size) noexcept(IsNoExcept) {
    void *ptr = tlsf_malloc(size);
    if constexpr (IsNoExcept) {
        return ptr;
    } else if (ptr) {
        return ptr;
    } else {
        throw std::bad_alloc();
    }
}

inline void freeImpl(void* ptr) noexcept {
    tlsf_free(ptr);
}

inline void *
PoolAllocatable::operator new(std::size_t size) {
	return newImpl<false>(size);
}

inline void *
PoolAllocatable::operator new[](std::size_t size) {
	return newImpl<false>(size);
}

inline void *
PoolAllocatable::operator new(std::size_t size, const std::nothrow_t &) noexcept {
	return newImpl<true>(size);
}

inline void *
PoolAllocatable::operator new[](std::size_t size, const std::nothrow_t &) noexcept {
	return newImpl<true>(size);
}

inline void PoolAllocatable::operator delete(void* ptr) noexcept {
    return freeImpl(ptr);
}

inline void PoolAllocatable::operator delete[](void* ptr) noexcept {
    return freeImpl(ptr);
}

inline void
PoolAllocatable::operator delete(void* ptr, const std::nothrow_t&) noexcept {
    return freeImpl(ptr);
}

inline void
PoolAllocatable::operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    return freeImpl(ptr);
}

} // namespace sqk::common

#endif // !SQK_COMMON_ALLOCATOR_HPP_
