#ifndef SQK_CORE_HPP
#define SQK_CORE_HPP

#include <coroutine>
#include <iostream>
#include <list>

#include "log.hpp"

namespace sqk {

template<typename T>
struct SuspendAlways;
template<typename T>
struct Promise;

template<typename T>
concept Awakable = requires(T a) { a.wake(); };

template<typename T>
struct Task: std::coroutine_handle<Promise<T>> {
    using promise_type = Promise<T>;
};

struct SQKScheduler {
    std::list<std::coroutine_handle<>> queue_;

  public:
    template<typename T>
    int enqueue(Task<T> handle) {
        queue_.push_back(handle);
        return 0;
    }

    template<typename T>
    int enqueue(T handle) {
        queue_.push_back(handle);
        return 0;
    }

    template<typename T>
    [[noreturn]] int run(Task<T> handle) {
        enqueue(handle);
        for (;;) {
            while (!queue_.empty()) {
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

    Awaker() {}

    Awaker(Awaker&) = delete;
    Awaker& operator=(Awaker&) = delete;

    bool await_ready() {
        S_DBUG("await_ready: {}", fmt::ptr(this));
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) {
        S_DBUG(
            "await_suspend: {}, {}",
            fmt::ptr(this),
            fmt::ptr(handle.address())
        );
        handle_ = handle;
    }

    void await_resume() const noexcept {
        S_DBUG("await_resume: {}", fmt::ptr(this));
    }

    void wake() {
        S_DBUG("wake: {}, {}", fmt::ptr(this), fmt::ptr(handle_.address()));
        if (handle_) {
            S_DBUG(
                "enqueue: {}, {}",
                fmt::ptr(this),
                fmt::ptr(handle_.address())
            );
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
        S_DBUG("await_resume");
        return val_;
    }

    SuspendAlways() : val_() {}
};

struct SuspendYield {
    constexpr bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        scheduler->enqueue(handle);
    }

    void await_resume() const noexcept {}
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
        S_DBUG("initial_suspend: {}", get_return_object().address());
        return {};
    }

    std::suspend_never final_suspend() noexcept {
        S_DBUG("final_suspend: {}", get_return_object().address());
        return {};
    }

    void unhandled_exception() {}

    SuspendYield yield_value(std::nullptr_t) {
        return {};
    }

    template<typename T2>
    SuspendAlways<T2> await_transform(Task<T2> task) {
        task.promise().caller = get_return_object();
        scheduler->enqueue(task);
        S_DBUG(
            "await_transform: {} self: {}",
            task.address(),
            get_return_object().address()
        );
        return task.promise().result_;
    }

    template<Awakable T2>
    T2& await_transform(T2& task) {
        return task;
    }

    Task<T> get_return_object() {
        return {Task<T>::from_promise(*static_cast<S*>(this))};
    }

    void resume_caller() {
        if (this->caller.address() != nullptr) {
            this->caller.resume();
        } else {
            S_DBUG("caller=null {}", this->get_return_object().address());
        }
    }

    std::coroutine_handle<> caller {nullptr};
    SuspendAlways<T> result_;
};

template<typename T>
struct Promise: PromiseBase<T, Promise<T>> {
    void return_value(T ret) {
        std::cout << "return_value" << std::endl;
        this->result_.val_ = ret;
        this->resume_caller();
        S_DBUG("return_value quit {}", this->get_return_object().address());
    }

  private:
};

template<>
struct Promise<void>: PromiseBase<void, Promise<void>> {
    void return_void() {
        S_DBUG("return_void");
        this->resume_caller();
    }
};
} // namespace sqk

#endif // !SQK_CORE_HPP
