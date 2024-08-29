#ifndef SQK_COMMON_TLSF_ALLOCATOR_HPP_
#define SQK_COMMON_TLSF_ALLOCATOR_HPP_

#include <tlsf.h>
#include "allocator.hpp"
namespace sqk::common {

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

void freeImpl(void* ptr) noexcept {
    tlsf_free(ptr);
}

void *
TlsfPoolAllocator::operator new(std::size_t size) {
	return newImpl<false>(size);
}

void *
TlsfPoolAllocator::operator new[](std::size_t size) {
	return newImpl<false>(size);
}

void *
TlsfPoolAllocator::operator new(std::size_t size, const std::nothrow_t &) noexcept {
	return newImpl<true>(size);
}

void *
TlsfPoolAllocator::operator new[](std::size_t size, const std::nothrow_t &) noexcept {
	return newImpl<true>(size);
}

void TlsfPoolAllocator::operator delete(void* ptr) noexcept {
    return freeImpl(ptr);
}

void TlsfPoolAllocator::operator delete[](void* ptr) noexcept {
    return freeImpl(ptr);
}

void
TlsfPoolAllocator::operator delete(void* ptr, const std::nothrow_t&) noexcept {
    return freeImpl(ptr);
}

void
TlsfPoolAllocator::operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    return freeImpl(ptr);
}

} // namespace sqk::common

#endif // !SQK_COMMON_ALLOCATOR_HPP_
