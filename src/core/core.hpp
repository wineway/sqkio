#ifndef SQK_CORE_HPP
#define SQK_CORE_HPP

#include <coroutine>
#include <list>

#include "log.hpp"
#include "ring.hpp"

namespace sqk {

template<typename T>
struct SuspendAlways;
template<typename T>
struct Promise;

template<typename T, typename S>
concept Awakable_Void = requires(T a) { a.wake(); } && std::is_void_v<S>;
template<typename T, typename S>
concept Awakable_T = requires(T a, S p) { a.wake(p); };
template<typename T, typename S>
concept Awakable = Awakable_T<T, S> || Awakable_Void<T, S>;

template<typename T>
struct Task: std::coroutine_handle<Promise<T>> {
    using promise_type = Promise<T>;
};

struct SQKScheduler {
    alignas(SQK_CACHE_LINESIZE) bool stopped {};
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

    void stop() {
        stopped = 1;
    }
    int run() {
        for (;;) {
            if (likely(!queue_.empty())) {
                auto handle = queue_.front();
                queue_.pop_front();
                handle.resume();
                if (unlikely(stopped)) {
                    return 0;
                }
            }
        }
    }
};

static inline SQKScheduler* scheduler;

struct Awaker_Base {
    std::coroutine_handle<> handle_ {nullptr};

    constexpr Awaker_Base() noexcept {}

    Awaker_Base(Awaker_Base&) = delete;
    Awaker_Base& operator=(Awaker_Base&) = delete;

    constexpr bool await_ready() const noexcept {
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
};

template<typename T>
struct Awaker: Awaker_Base {
    T ret_;

    T await_resume() noexcept {
        S_DBUG("await_resume: {}={}", fmt::ptr(this), fmt::ptr(&ret_));
        handle_ = nullptr;
        return std::move(ret_);
    }

    void wake(T&& ret) {
        S_DBUG(
            "wake: {}, {}={}",
            fmt::ptr(this),
            fmt::ptr(handle_.address()),
            fmt::ptr(&ret)
        );
        if (handle_) {
            S_DBUG(
                "enqueue: {}, {}",
                fmt::ptr(this),
                fmt::ptr(handle_.address())
            );
            scheduler->enqueue(handle_);
        }
        ret_ = std::move(ret);
    }
};

template<>
struct Awaker<void>: Awaker_Base {
    void await_resume() noexcept {
        S_DBUG("await_resume: {}", fmt::ptr(this));
        handle_ = nullptr;
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

struct CheckableAwaker_Base {
    bool is_awaked() const noexcept {
        return awaked;
    }

  protected:
    bool awaked {};
};

template<typename T>
struct CheckableAwaker: Awaker<T>, CheckableAwaker_Base {
    bool await_ready() const noexcept {
        return awaked;
    }

    void wake(T&& ret) {
        Awaker<T>::wake(std::move(ret));
        awaked = true;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        Awaker<T>::await_suspend(handle);
        awaked = false;
    }
};

template<>
struct CheckableAwaker<void>: Awaker<void>, CheckableAwaker_Base {
    bool await_ready() const noexcept {
        return awaked;
    }

    void wake() {
        Awaker<void>::wake();
        awaked = true;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        Awaker<void>::await_suspend(handle);
        awaked = false;
    }
};

template<typename T>
struct SuspendAlways {
    T val_;

    constexpr bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {}

    T&& await_resume() noexcept {
        S_DBUG("await_resume");
        return std::move(val_);
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

    void unhandled_exception() {
        S_INFO("unhandled exception");
        std::rethrow_exception(std::current_exception());
    }

    SuspendYield yield_value(std::nullptr_t) {
        return {};
    }

    template<typename T2>
    SuspendAlways<T2>&& await_transform(Task<T2> task) {
        task.promise().caller = get_return_object();
        scheduler->enqueue(task);
        S_DBUG(
            "await_transform: {} self: {}",
            task.address(),
            get_return_object().address()
        );
        return std::move(task.promise().result_);
    }

    template</*typename T2, */ typename T1>
    T1& await_transform(T1& task) /*requires Awakable<T2, T1>*/ {
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
    void return_value(T&& ret) {
        this->result_.val_ = std::move(ret);
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
