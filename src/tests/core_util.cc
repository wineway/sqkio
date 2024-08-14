#include "core.hpp"

using double_t = double;

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
    A i = co_await f();
    std::cout << "assigned" << std::endl;
    co_await k();
    std::cout << "g()" << std::endl;
    exit(0);
    co_return 1;
}

int main() {
    sqk::scheduler = new sqk::SQKScheduler;
    sqk::scheduler->enqueue(g());
    sqk::scheduler->run();
}
