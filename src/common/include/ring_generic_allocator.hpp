#ifndef SQK_COMMON_RING_GENERIC_ALLOCATOR_HPP_
#define SQK_COMMON_RING_GENERIC_ALLOCATOR_HPP_

#ifdef __linux__
    #include <sys/mman.h>
#endif

#include <memory>

namespace sqk::common {

template<typename T>
struct Allocator: std::allocator<T> {
#ifdef __linux__
    [[gnu::always_inline]]
    constexpr T* allocate(size_t n) {
        T* allocated = std::allocator<T>::allocate(n);
        madvise(allocated, sizeof(T) * n, MADV_POPULATE_WRITE);
        return allocated;
    }
#endif
};
} // namespace sqk::common

#endif // !SQK_COMMON_RING_GENERIC_ALLOCATOR_HPP_
