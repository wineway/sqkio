#include "ring.hpp"

int main(int argc, char* argv[]) {
    S_LOGGER_SETUP;
    {
        auto ring = Ring<
            int,
            RingSyncType::SQK_RING_SYNC_MT,
            RingSyncType::SQK_RING_SYNC_MT>::of(10);
        ring->enqueue(1);
        int i = 0xdd;
        ring->dequeue(i);
        std::remove_reference_t<decltype(*ring)>::free(ring);
        if (i != 1) {
            S_ERROR("i={}", i);
            return 1;
        }
    }
    {
        auto ring = Ring<
            int,
            RingSyncType::SQK_RING_SYNC_MT_HTS,
            RingSyncType::SQK_RING_SYNC_MT_HTS>::of(10);
        ring->enqueue(1);
        int i = 0xdd;
        ring->dequeue(i);
        std::remove_reference_t<decltype(*ring)>::free(ring);
        if (i != 1) {
            S_ERROR("i={}", i);
            return 1;
        }
    }
    {
        auto ring = Ring<
            int,
            RingSyncType::SQK_RING_SYNC_MT,
            RingSyncType::SQK_RING_SYNC_MT_HTS>::of(10);
        ring->enqueue(1);
        int i = 0xdd;
        ring->dequeue(i);
        std::remove_reference_t<decltype(*ring)>::free(ring);
        if (i != 1) {
            S_ERROR("i={}", i);
            return 1;
        }
    }
    {
        auto ring = Ring<
            int,
            RingSyncType::SQK_RING_SYNC_MT_HTS,
            RingSyncType::SQK_RING_SYNC_MT>::of(10);
        ring->enqueue(1);
        int i = 0xdd;
        ring->dequeue(i);
        std::remove_reference_t<decltype(*ring)>::free(ring);
        if (i != 1) {
            S_ERROR("i={}", i);
            return 1;
        }
    }

    return 0;
}
