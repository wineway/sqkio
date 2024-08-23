#include <nanobench.h>

#include <core.hpp>

using namespace ankerl::nanobench;

struct SqkBench: public Bench {
    SqkBench& name(std::string name) {
        Bench::name(name);
        return *this;
    }

    SqkBench& minEpochIterations(uint64_t cnt) {
        Bench::minEpochIterations(cnt);
        return *this;
    }

    template<typename Op>
    sqk::Task<void> run(Op&& op) {
        detail::IterationLogic iterationLogic(*this);
        auto& pc = detail::performanceCounters();

        while (auto n = iterationLogic.numIters()) {
            pc.beginMeasure();
            Clock::time_point const before = Clock::now();
            while (n-- > 0) {
                co_await op();
            }
            Clock::time_point const after = Clock::now();
            pc.endMeasure();
            pc.updateResults(iterationLogic.numIters());
            iterationLogic.add(after - before, pc);
        }

        sqk::scheduler->stop();
    }
};

int i;

#define epochIterations (1000UL * 1000)

sqk::Task<int> run_bench() {
    co_await SqkBench()
        .name("sqk::scheduler benchmark")
        .minEpochIterations(epochIterations)
        .run([]() -> sqk::Task<void> {
            i++;
            doNotOptimizeAway(i);
            co_return;
        });
    co_return 0;
}

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90333
[[gnu::noinline]]
void bench_fn() {
    i++;
    doNotOptimizeAway(i);
    return;
}

int main(int argc, char* argv[]) {
    sqk::scheduler = new sqk::SQKScheduler;
    sqk::scheduler->enqueue(run_bench());
    sqk::scheduler->run();

    Bench()
        .name("function benchmark")
        .minEpochIterations(epochIterations)
        .run(bench_fn);

    Bench()
        .name("thread benchmark")
        .minEpochIterations(epochIterations / 1000)
        .run([]() {
            std::jthread thread([]() {
                i++;
                doNotOptimizeAway(i);
                return;
            });
            thread.join();
        });
    return 0;
}
