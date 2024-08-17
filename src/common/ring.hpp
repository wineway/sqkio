#ifndef SQK_COMMON_HPP_
#define SQK_COMMON_HPP_

#include <emmintrin.h>

#include <cstdint>

#include "log.hpp"

/**
 * Compiler barrier.
 *
 * Guarantees that operation reordering does not occur at compile time
 * for operations directly before and after the barrier.
 */
#define sqk_compiler_barrier()                                                 \
    do {                                                                       \
        asm volatile("" : : : "memory");                                       \
    } while (0)

#define sqk_smp_wmb() sqk_compiler_barrier()

static inline void sqk_pause(void) {
    _mm_pause();
}

#ifndef likely
    #define likely(x) __builtin_expect((x), 1)
#endif
#ifndef unlikely
    #define unlikely(x) __builtin_expect((x), 0)
#endif

#define SQK_CACHELINE_ALIGNED alignas(SQK_CACHE_LINESIZE)

/**
 * Force alignment
 */
#define __sqk_aligned(a) __attribute__((__aligned__(a)))

#ifdef SQK_ARCH_STRICT_ALIGN
typedef uint64_t unaligned_uint64_t __sqk_aligned(1);
typedef uint32_t unaligned_uint32_t __sqk_aligned(1);
typedef uint16_t unaligned_uint16_t __sqk_aligned(1);
#else
typedef uint64_t unaligned_uint64_t;
typedef uint32_t unaligned_uint32_t;
typedef uint16_t unaligned_uint16_t;
#endif

typedef struct {
    union {
        __extension__ __int128 int128;
    };
} __sqk_aligned(16) sqk_int128_t;

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no
 * bigger than the first parameter. Second parameter must be a
 * power-of-two value.
 */
#define SQK_ALIGN_FLOOR(val, align)                                            \
    (typeof(val))((val) & (~((typeof(val))((align) - 1))))

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no lower
 * than the first parameter. Second parameter must be a power-of-two
 * value.
 */
#define SQK_ALIGN_CEIL(val, align)                                             \
    SQK_ALIGN_FLOOR(((val) + ((typeof(val))(align) - 1)), align)

/**
 * Macro to align a value to a given power-of-two. The resultant
 * value will be of the same type as the first parameter, and
 * will be no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 * This function is the same as sqk_ALIGN_CEIL
 */
#define SQK_ALIGN(val, align) SQK_ALIGN_CEIL(val, align)

#define SQK_RING_SZ_MASK (0x7fffffffU) /**< Ring size mask */
/* true if x is a power of 2 */
#define POWEROF2(x) ((((x) - 1) & (x)) == 0)

inline void sqk_smp_rmb() {}

static inline uint32_t sqk_combine32ms1b(uint32_t x) {
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

    return x;
}

static inline uint32_t sqk_align32pow2(uint32_t x) {
    x--;
    x = sqk_combine32ms1b(x);

    return x + 1;
}

struct MemZone {
    uint8_t* addr_;
};

struct RingHeadTail {
    std::atomic<uint32_t> head_;
    std::atomic<uint32_t> tail_;
};

template<typename T, typename Allocator = std::allocator<uint8_t>>
    requires(sizeof(T) % 4 == 0)
struct MpscRing {
  private:
    uint32_t size; /**< Size of ring. */
    uint32_t mask; /**< Mask (size-1) of ring. */
    uint32_t capacity; /**< Usable size of ring */
    uint32_t htd_max_;
    MemZone memzone_;
    SQK_CACHELINE_ALIGNED uint8_t pad0;
    RingHeadTail prod_;
    SQK_CACHELINE_ALIGNED uint8_t pad1;
    RingHeadTail cons_;
    SQK_CACHELINE_ALIGNED uint8_t pad2;
    MpscRing() = delete;
    ~MpscRing() = delete;
    using allocator_traits = std::allocator_traits<Allocator>;
    /* by default set head/tail distance as 1/8 of ring capacity */
    static constexpr uint32_t HTD_MAX_DEF = 8;

    void init(uint32_t sz) {
        std::memset(this, 0, sizeof(MpscRing<T>));
        this->size = sz;
        this->mask = sz - 1;
        this->capacity = this->mask;
        this->htd_max_ = this->capacity / HTD_MAX_DEF;
    }

    uint32_t move_prod_head(
        uint32_t n,
        uint32_t& old_head,
        uint32_t& new_head,
        uint32_t& free_entries
    ) {
        bool success {};
        uint32_t max = n;
        do {
            n = max;
            old_head = this->prod_.head_;
            /* add rmb barrier to avoid load/load reorder in weak
		     * memory model. It is noop on x86
		     */
            sqk_smp_rmb();
            /*
		     *  The subtraction is done between two unsigned 32bits value
		     * (the result is always modulo 32 bits even if we have
		     * *old_head > cons_tail). So 'free_entries' is always between 0
		     * and capacity (which is < size).
		     */
            free_entries = (this->capacity + this->cons_.tail_ - old_head);
            /* check that we have enough room in ring */
            if (unlikely(n > free_entries)) {
                n = free_entries;
            }
            if (n == 0) {
                return 0;
            }
            new_head = old_head + n;
            // TODO: review this
            success = std::atomic_compare_exchange_strong_explicit(
                &this->prod_.head_,
                &old_head,
                new_head,
                std::memory_order_release,
                std::memory_order_relaxed
            );
        } while (unlikely(success == 0));
        return n;
    }

    void enqueue_elems_32(
        uint32_t size,
        uint32_t idx,
        const void* entries,
        uint32_t n
    ) {
        unsigned int i;
        uint32_t* ring = (uint32_t*)&this[1];
        const uint32_t* obj = (const uint32_t*)entries;
        if (likely(idx + n < size)) {
            for (i = 0; i < (n & ~0x7); i += 8, idx += 8) {
                ring[idx] = obj[i];
                ring[idx + 1] = obj[i + 1];
                ring[idx + 2] = obj[i + 2];
                ring[idx + 3] = obj[i + 3];
                ring[idx + 4] = obj[i + 4];
                ring[idx + 5] = obj[i + 5];
                ring[idx + 6] = obj[i + 6];
                ring[idx + 7] = obj[i + 7];
            }
            switch (n & 0x7) {
                case 7:
                    ring[idx++] = obj[i++]; /* fallthrough */
                case 6:
                    ring[idx++] = obj[i++]; /* fallthrough */
                case 5:
                    ring[idx++] = obj[i++]; /* fallthrough */
                case 4:
                    ring[idx++] = obj[i++]; /* fallthrough */
                case 3:
                    ring[idx++] = obj[i++]; /* fallthrough */
                case 2:
                    ring[idx++] = obj[i++]; /* fallthrough */
                case 1:
                    ring[idx++] = obj[i++]; /* fallthrough */
            }
        } else {
            for (i = 0; idx < size; i++, idx++)
                ring[idx] = obj[i];
            /* Start at the beginning */
            for (idx = 0; i < n; i++, idx++)
                ring[idx] = obj[i];
        }
    }

    void
    enqueue_elems_128(uint32_t prod_head, const void* entries, uint32_t n) {
        unsigned int i;
        const uint32_t size = this->size;
        uint32_t idx = prod_head & this->mask;
        sqk_int128_t* ring = (sqk_int128_t*)&this[1];
        const sqk_int128_t* obj = (const sqk_int128_t*)entries;
        if (likely(idx + n < size)) {
            for (i = 0; i < (n & ~0x1); i += 2, idx += 2)
                memcpy((void*)(ring + idx), (const void*)(obj + i), 32);
            switch (n & 0x1) {
                case 1:
                    memcpy((void*)(ring + idx), (const void*)(obj + i), 16);
            }
        } else {
            for (i = 0; idx < size; i++, idx++)
                memcpy((void*)(ring + idx), (const void*)(obj + i), 16);
            /* Start at the beginning */
            for (idx = 0; i < n; i++, idx++)
                memcpy((void*)(ring + idx), (const void*)(obj + i), 16);
        }
    }

    void enqueue_elems_64(uint32_t prod_head, const void* entries, uint32_t n) {
        unsigned int i;
        const uint32_t size = this->size;
        uint32_t idx = prod_head & this->mask;
        uint64_t* ring = (uint64_t*)&this[1];
        const unaligned_uint64_t* obj = (const unaligned_uint64_t*)entries;
        if (likely(idx + n < size)) {
            for (i = 0; i < (n & ~0x3); i += 4, idx += 4) {
                ring[idx] = obj[i];
                ring[idx + 1] = obj[i + 1];
                ring[idx + 2] = obj[i + 2];
                ring[idx + 3] = obj[i + 3];
            }
            switch (n & 0x3) {
                case 3:
                    ring[idx++] = obj[i++]; /* fallthrough */
                case 2:
                    ring[idx++] = obj[i++]; /* fallthrough */
                case 1:
                    ring[idx++] = obj[i++];
            }
        } else {
            for (i = 0; idx < size; i++, idx++)
                ring[idx] = obj[i];
            /* Start at the beginning */
            for (idx = 0; i < n; i++, idx++)
                ring[idx] = obj[i];
        }
    }

    template<typename T2 = T>
        requires(sizeof(T2) == 8)
    void enqueue_elements(uint32_t prod_head, const void* entries, uint32_t n) {
        this->enqueue_elems_64(prod_head, entries, n);
    }

    template<typename T2 = T>
        requires(sizeof(T2) == 16)
    void enqueue_elements(uint32_t prod_head, const void* entries, uint32_t n) {
        this->enqueue_elems_128(prod_head, entries, n);
    }

    template<typename T2 = T>
    void enqueue_elements(uint32_t prod_head, const void* entries, uint32_t n) {
        uint32_t idx, scale, nr_idx, nr_num, nr_size;

        /* Normalize to uint32_t */
        scale = sizeof(T2) / sizeof(uint32_t);
        nr_num = n * scale;
        idx = prod_head & this->mask;
        nr_idx = idx * scale;
        nr_size = this->size * scale;
        this->enqueue_elems_32(nr_size, nr_idx, entries, nr_num);
    }

    void update_tail(
        uint32_t old_val,
        uint32_t new_val,
        uint32_t single,
        bool enqueue
    ) {
        if (enqueue)
            sqk_smp_wmb();
        else
            sqk_smp_rmb();
        /*
	 * If there are other enqueues/dequeues in progress that preceded us,
	 * we need to wait for them to complete
	 */
        if (!single)
            while (unlikely(this->prod_.tail_ != old_val))
                sqk_pause();

        this->prod_.tail_ = new_val;
    }

    uint32_t move_cons_head(
        unsigned int n,
        uint32_t& old_head,
        uint32_t& new_head,
        uint32_t& entries
    ) {
        unsigned int max = n;
        int success;

        /* move cons.head atomically */
        do {
            /* Restore n as it may change every loop */
            n = max;

            old_head = this->cons_.head_;

            /* add rmb barrier to avoid load/load reorder in weak
		 * memory model. It is noop on x86
		 */
            sqk_smp_rmb();

            /* The subtraction is done between two unsigned 32bits value
		 * (the result is always modulo 32 bits even if we have
		 * cons_head > prod_tail). So 'entries' is always between 0
		 * and size(ring)-1.
		 */
            entries = (this->prod_.tail_ - old_head);

            /* Set the actual entries for dequeue */
            if (n > entries)
                n = entries;

            if (unlikely(n == 0))
                return 0;

            new_head = old_head + n;
            this->cons_.head_ = new_head;
            sqk_smp_rmb();
            success = 1;
        } while (unlikely(success == 0));
        return n;
    }

    void dequeue_elems_64(uint32_t prod_head, void* entries, uint32_t n) {
        unsigned int i;
        const uint32_t size = this->size;
        uint32_t idx = prod_head & this->mask;
        uint64_t* ring = (uint64_t*)&this[1];
        unaligned_uint64_t* obj = (unaligned_uint64_t*)entries;
        if (likely(idx + n < size)) {
            for (i = 0; i < (n & ~0x3); i += 4, idx += 4) {
                obj[i] = ring[idx];
                obj[i + 1] = ring[idx + 1];
                obj[i + 2] = ring[idx + 2];
                obj[i + 3] = ring[idx + 3];
            }
            switch (n & 0x3) {
                case 3:
                    obj[i++] = ring[idx++]; /* fallthrough */
                case 2:
                    obj[i++] = ring[idx++]; /* fallthrough */
                case 1:
                    obj[i++] = ring[idx++]; /* fallthrough */
            }
        } else {
            for (i = 0; idx < size; i++, idx++)
                obj[i] = ring[idx];
            /* Start at the beginning */
            for (idx = 0; i < n; i++, idx++)
                obj[i] = ring[idx];
        }
    }

    void dequeue_elems_128(uint32_t prod_head, void* entries, uint32_t n) {
        unsigned int i;
        const uint32_t size = this->size;
        uint32_t idx = prod_head & this->mask;
        sqk_int128_t* ring = (sqk_int128_t*)&this[1];
        sqk_int128_t* obj = (sqk_int128_t*)entries;
        if (likely(idx + n < size)) {
            for (i = 0; i < (n & ~0x1); i += 2, idx += 2)
                memcpy((void*)(obj + i), (void*)(ring + idx), 32);
            switch (n & 0x1) {
                case 1:
                    memcpy((void*)(obj + i), (void*)(ring + idx), 16);
            }
        } else {
            for (i = 0; idx < size; i++, idx++)
                memcpy((void*)(obj + i), (void*)(ring + idx), 16);
            /* Start at the beginning */
            for (idx = 0; i < n; i++, idx++)
                memcpy((void*)(obj + i), (void*)(ring + idx), 16);
        }
    }

    void dequeue_elems_32(
        const uint32_t size,
        uint32_t idx,
        void* entries,
        uint32_t n
    ) {
        unsigned int i;
        uint32_t* ring = (uint32_t*)&this[1];
        uint32_t* obj = (uint32_t*)entries;
        if (likely(idx + n < size)) {
            for (i = 0; i < (n & ~0x7); i += 8, idx += 8) {
                obj[i] = ring[idx];
                obj[i + 1] = ring[idx + 1];
                obj[i + 2] = ring[idx + 2];
                obj[i + 3] = ring[idx + 3];
                obj[i + 4] = ring[idx + 4];
                obj[i + 5] = ring[idx + 5];
                obj[i + 6] = ring[idx + 6];
                obj[i + 7] = ring[idx + 7];
            }
            switch (n & 0x7) {
                case 7:
                    obj[i++] = ring[idx++]; /* fallthrough */
                case 6:
                    obj[i++] = ring[idx++]; /* fallthrough */
                case 5:
                    obj[i++] = ring[idx++]; /* fallthrough */
                case 4:
                    obj[i++] = ring[idx++]; /* fallthrough */
                case 3:
                    obj[i++] = ring[idx++]; /* fallthrough */
                case 2:
                    obj[i++] = ring[idx++]; /* fallthrough */
                case 1:
                    obj[i++] = ring[idx++]; /* fallthrough */
            }
        } else {
            for (i = 0; idx < size; i++, idx++)
                obj[i] = ring[idx];
            /* Start at the beginning */
            for (idx = 0; i < n; i++, idx++)
                obj[i] = ring[idx];
        }
    }

    template<typename T2 = T>
        requires(sizeof(T) == 8)
    void dequeue_elements(
        uint32_t cons_head,
        void* entries,
        uint32_t num
    ) {
        this->dequeue_elems_64(cons_head, entries, num);
    }

    template<typename T2 = T>
        requires(sizeof(T) == 16)
    void dequeue_elements(
        uint32_t cons_head,
        void* entries,
        uint32_t num
    ) {
        this->dequeue_elems_128(cons_head, entries, num);
    }

    template<typename T2 = T>
    void dequeue_elements(
        uint32_t cons_head,
        void* entries,
        uint32_t num
    ) {
        uint32_t idx, scale, nr_idx, nr_num, nr_size;

        /* Normalize to uint32_t */
        scale = sizeof(T) / sizeof(uint32_t);
        nr_num = num * scale;
        idx = cons_head & this->mask;
        nr_idx = idx * scale;
        nr_size = this->size * scale;
        this->dequeue_elems_32(nr_size, nr_idx, entries, nr_num);
    }
  public:
    static inline Allocator allocator;

    static MpscRing* of(uint32_t cnt) {
        ssize_t sz;
        uint32_t esize = sizeof(T);
        auto count = sqk_align32pow2(cnt + 1);
        /* count must be a power of 2 */
        if ((!POWEROF2(count)) || (count > SQK_RING_SZ_MASK)) {
            S_ERROR(
                "Requested number of elements is invalid, must be power of 2, and not exceed {}",
                SQK_RING_SZ_MASK
            );
            throw std::system_error(EINVAL, std::system_category());
        }

        sz = sizeof(MpscRing) + count * esize;
        sz = SQK_ALIGN(sz, SQK_CACHE_LINESIZE);

        void* buf = allocator_traits::allocate(allocator, sz);
        auto ring = static_cast<MpscRing*>(buf);
        ring->memzone_.addr_ = static_cast<uint8_t*>(buf) + sizeof(MpscRing);
        ring->init(cnt);

        return ring;
    }

    template<typename T2>
    uint32_t enqueue(T2&& entry) {
        uint32_t prod_head, prod_next;
        uint32_t free_entries;
        uint32_t n;
        n = this->move_prod_head(1, prod_head, prod_next, free_entries);
        if (!n) {
            return 0;
        }
        this->enqueue_elements(prod_head, &entry, n);
        this->update_tail(prod_head, prod_next, 0, n);
        return n;
    }

    uint32_t dequeue(T& entry) {
        uint32_t cons_head, cons_next;
        uint32_t entries;

        uint32_t n = this->move_cons_head(1, cons_head, cons_next, entries);
        if (!n) {
            return 0;
        }
        this->dequeue_elements(cons_head, &entry, n);
        this->update_tail(cons_head, cons_next, 1, 0);
        return n;
    }
};

#endif // !SQK_COMMON_HPP_
