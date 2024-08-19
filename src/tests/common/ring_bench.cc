#include <nanobench.h>

#include <deque>
#include <list>

#include "ring.hpp"

using namespace sqk::common;

int main(int argc, char* argv[]) {
    constexpr uint64_t iterations = 1UL * 1000 * 1000 * 10;
    {
        RingGuard<MpscRing<int>> guard(9216);
        ankerl::nanobench::Bench()
            .minEpochIterations(iterations)
            .run("mpsc_ring enqueue", [&] {
                guard->enqueue(1);
                int i;
                guard->dequeue(i);
            });
    }
    {
        RingGuard<Ring<
            int,
            RingSyncType::SQK_RING_SYNC_ST,
            RingSyncType::SQK_RING_SYNC_ST>>
            guard(9216);
        ankerl::nanobench::Bench()
            .minEpochIterations(iterations)
            .run("spsc_ring enqueue", [&] {
                guard->enqueue(1);
                int i;
                guard->dequeue(i);
            });
    }
    {
        RingGuard<Ring<
            int,
            RingSyncType::SQK_RING_SYNC_MT_HTS,
            RingSyncType::SQK_RING_SYNC_ST>>
            guard(9216);
        ankerl::nanobench::Bench()
            .minEpochIterations(iterations)
            .run("hts mpsc_ring enqueue", [&] {
                guard->enqueue(1);
                int i;
                guard->dequeue(i);
            });
    }
    {
        std::deque<int> deq;
        ankerl::nanobench::Bench()
            .minEpochIterations(iterations)
            .run("deque enqueue", [&] {
                deq.push_back(1);
                int i = deq.front();
                deq.pop_front();
            });
    }
    {
        std::list<int> deq;
        ankerl::nanobench::Bench()
            .minEpochIterations(iterations)
            .run("list enqueue", [&] {
                deq.push_back(1);
                int i = deq.front();
                deq.pop_front();
            });
    }
    {
        RingGuard<MpscRing<uint64_t>> guard(9216);
        ankerl::nanobench::Bench()
            .minEpochIterations(iterations)
            .run("mpsc_ring enqueue", [&] {
                guard->enqueue(1);
                uint64_t i;
                guard->dequeue(i);
            });
    }
    {
        RingGuard<Ring<
            uint64_t,
            RingSyncType::SQK_RING_SYNC_ST,
            RingSyncType::SQK_RING_SYNC_ST>>
            guard(9216);
        ankerl::nanobench::Bench()
            .minEpochIterations(iterations)
            .run("spsc_ring enqueue", [&] {
                guard->enqueue(1);
                uint64_t i;
                guard->dequeue(i);
            });
    }
    {
        RingGuard<Ring<
            uint64_t,
            RingSyncType::SQK_RING_SYNC_MT_HTS,
            RingSyncType::SQK_RING_SYNC_ST>>
            guard(9216);
        ankerl::nanobench::Bench()
            .minEpochIterations(iterations)
            .run("hts mpsc_ring enqueue", [&] {
                guard->enqueue(1);
                uint64_t i;
                guard->dequeue(i);
            });
    }
    {
        std::deque<uint64_t> deq;
        ankerl::nanobench::Bench()
            .minEpochIterations(iterations)
            .run("deque enqueue", [&] {
                deq.push_back(1);
                uint64_t i = deq.front();
                deq.pop_front();
            });
    }
    {
        std::list<uint64_t> deq;
        ankerl::nanobench::Bench()
            .minEpochIterations(iterations)
            .run("list enqueue", [&] {
                deq.push_back(1);
                uint64_t i = deq.front();
                deq.pop_front();
            });
    }

    return 0;
}
