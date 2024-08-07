#include <coroutine>
#include <iostream>
#include <list>

namespace sqk {

template<typename T>
struct Awaitable;
template<typename T>
struct Promise;

template<typename T>
struct Task: std::coroutine_handle<Promise<T>> {
    using promise_type = Promise<T>;
};

struct SQKScheduler {
    std::list<std::coroutine_handle<>> queue_;

  public:
    int enqueue(std::coroutine_handle<> handle) {
        queue_.push_back(handle);
        return 0;
    }

    [[noreturn]] int run(std::coroutine_handle<> handle) {
        enqueue(handle);
        for (;;) {
            while (!queue_.empty()) {
                std::cout << "run: " << handle.address() << std::endl;
                auto handle = queue_.front();
                handle.resume();
                queue_.pop_front();
            }
        }
    }
};

static __thread SQKScheduler* scheduler;

template<typename T>
struct Awaitable {
    T&& val_;
    std::coroutine_handle<> caller;
    std::coroutine_handle<> callee;

    constexpr bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        std::cout << "await_suspend: " << handle.address()
                  << " callee: " << callee.address() << std::endl;
        caller = handle;
        scheduler->enqueue(callee);
    }

    T&& await_resume() const noexcept {
        std::cout << "await_resume: " << callee.address() << std::endl;
        return val_;
    }

    Awaitable(T&& val, std::coroutine_handle<> callee) :
        val_(val),
        callee(callee) {}
};

template<>
struct Awaitable<void> {
    constexpr bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) const noexcept {
        std::cout << "await_suspend: " << handle.address() << std::endl;
        scheduler->enqueue(handle);
    }

    void await_resume() const noexcept {
        std::cout << "await_resume" << std::endl;
    }
};

template<typename T>
struct Promise {
    std::suspend_always initial_suspend() noexcept {
        std::cout << "initial_suspend: " << get_return_object().address()
                  << std::endl;
        return {};
    }

    std::suspend_never final_suspend() noexcept {
        std::cout << "final_suspend" << std::endl;
        if (caller.address() != nullptr) {
            caller.resume();
        }
        return {};
    }

    Task<T> get_return_object() {
        return {Task<T>::from_promise(*this)};
    }

    void return_value(T ret) {
        std::cout << "return_value" << std::endl;
        result_ = ret;
    }

    void unhandled_exception() {}

    Awaitable<T&> await_transform(Task<T> task) {
        Awaitable<T&> awaiter(result_, task);
        task.promise().caller = get_return_object();
        std::cout << "await_transform: " << task.address() << std::endl;
        return awaiter;
    }

    Promise() : caller(nullptr) {};
    ~Promise() {};

  private:
    std::coroutine_handle<> caller;
    T result_;
};

template<>
struct Promise<void> {
    std::suspend_always initial_suspend() noexcept {
        std::cout << "initial_suspend" << std::endl;
        return {};
    }

    std::suspend_never final_suspend() noexcept {
        std::cout << "final_suspend" << std::endl;
        return {};
    }

    Task<void> get_return_object() {
        return {Task<void>::from_promise(*this)};
    }

    void return_void() {}

    void unhandled_exception() {}

    Promise() {};
    ~Promise() {};

  private:
    Awaitable<void> awaiter_;
};
} // namespace sqk

sqk::Task<int> f() {
    std::cout << "f()" << std::endl;
    co_return 1;
}

sqk::Task<int> g() {
    int i = co_await f();
    std::cout << "g()" << i << std::endl;
    co_return 1;
}

int main() {
    sqk::scheduler = new sqk::SQKScheduler;
    sqk::scheduler->run(g());
}
