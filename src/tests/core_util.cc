#include <iostream>

#include "core.hpp"

using double_t = double;
#define ST_ASSERT(e)                                                           \
    if (!(e))                                                                  \
    exit(1)

class A {
  public:
    A() {
        std::cout << "A() " << this << std::endl;
    }

    ~A() {
        std::cout << "~A() " << this << std::endl;
    }
};

sqk::Task<A> f() {
    std::cout << "f()" << std::endl;
    int a[5];
    std::cout << a << std::endl;
    co_return A();
}

sqk::Task<double_t> k() {
    std::cout << "k()" << std::endl;
    co_return 1.1;
}

sqk::Task<void> j() {
    std::cout << "j()" << std::endl;
    co_return;
}

sqk::Task<int> g() {
    auto v = f();
    std::cout << "v: size=" << sizeof(v) << std::endl;
    A i = co_await v;
    std::cout << "assigned" << std::endl;
    co_await k();
    std::cout << "g()" << std::endl;
    exit(0);
    co_return 1;
}

sqk::Task<int> throws() {
    throw std::exception();
}

sqk::Task<int> no_catch() {
    co_await throws();
    co_return 1;
}

sqk::Task<int> catch_throws() {
    try {
        co_await no_catch();
    } catch (std::exception& e) {
        exit(0);
    }
    exit(1);
}

sqk::Task<int> run_test(char* argv[]) {
    if (!strcmp(argv[1], "simple")) {
        return g();
    } else if (!strcmp(argv[1], "exception_propagation")) {
        return catch_throws();
    }
    ST_ASSERT(0);
}

int main(int argc, char* argv[]) {
    S_LOGGER_SETUP;
    ST_ASSERT(argc == 2);
    sqk::scheduler = new sqk::SQKScheduler;
    sqk::scheduler->enqueue(run_test(argv));
    sqk::scheduler->run();
}
