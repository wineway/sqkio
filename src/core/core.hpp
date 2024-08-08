#include <coroutine>
#include <iostream>
#include <list>

namespace sqk {

template<typename T>
struct SuspendAlways;
template<typename T>
struct Promise;

template <typename T>
concept Awakable = requires (T a) { a.wake(); };

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

static inline SQKScheduler* scheduler;

struct Awaker {
    std::coroutine_handle<> handle_ {nullptr};

    constexpr bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        handle_ = handle;
    }

    void await_resume() const noexcept {}

    void wake() {
        if (handle_) {
            scheduler->enqueue(handle_);
        }
    }
};

template<typename T>
struct SuspendAlways {
    T val_;

    constexpr bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {}

    T await_resume() const noexcept {
        std::cout << "await_resume: " << std::endl;
        return val_;
    }

    SuspendAlways() : val_() {}
};

template<>
struct SuspendAlways<void> {
    constexpr bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) const noexcept {}

    void await_resume() const noexcept {}
};

template<typename T, typename S>
struct PromiseBase {
    std::suspend_always initial_suspend() noexcept {
        std::cout << "initial_suspend: " << get_return_object().address()
                  << std::endl;
        return {};
    }

    std::suspend_never final_suspend() noexcept {
        std::cout << "final_suspend: " << get_return_object().address()
                  << std::endl;
        return {};
    }

    void unhandled_exception() {}

    template<typename T2>
    SuspendAlways<T2> await_transform(Task<T2> task) {
        task.promise().caller = get_return_object();
        scheduler->enqueue(task);
        std::cout << "await_transform: " << task.address()
                  << " self: " << get_return_object().address() << std::endl;
        return task.promise().result_;
    }

    Task<T> get_return_object() {
        return {Task<T>::from_promise(*static_cast<S*>(this))};
    }

    std::coroutine_handle<> caller {nullptr};
    SuspendAlways<T> result_;
};

template<typename T>
struct Promise: PromiseBase<T, Promise<T>> {
    void return_value(T ret) {
        std::cout << "return_value" << std::endl;
        this->result_.val_ = ret;
        if (this->caller.address() != nullptr) {
            this->caller.resume();
        } else {
            std::cout << "caller=null " << this->get_return_object().address()
                      << std::endl;
        }
        std::cout << "return_value quit " << this->get_return_object().address()
                  << std::endl;
    }

  private:
};

template<>
struct Promise<void>: PromiseBase<void, Promise<void>> {
    void return_void() {}
};
} // namespace sqk
