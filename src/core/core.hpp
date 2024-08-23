#ifndef SQK_CORE_HPP
#define SQK_CORE_HPP

#include <coroutine>

#include "log.hpp"
#include "ring.hpp"

namespace sqk {

template<typename T>
struct MaybeSuspend;
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

using sqk::common::MpscRing;
using sqk::common::RingGuard;

struct SQKScheduler {
    alignas(SQK_CACHE_LINESIZE) bool stopped_ {};
    RingGuard<MpscRing<std::coroutine_handle<>>> queue_;

  public:
    template<typename T>
    int enqueue(Task<T> handle) {
        queue_->enqueue(handle);
        return 0;
    }

    template<typename T>
    int enqueue(T handle) {
        queue_->enqueue(handle);
        return 0;
    }

    void stop() {
        stopped_ = 1;
    }

    int run() {
        std::coroutine_handle<> handle;
        for (;;) {
            if (likely(queue_->dequeue(handle))) {
                S_DBUG("resume: {}", handle.address());
                handle.resume();
                if (unlikely(stopped_)) {
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
        return awaked_;
    }

  protected:
    bool awaked_ {};
};

template<typename T>
struct CheckableAwaker: Awaker<T>, CheckableAwaker_Base {
    bool await_ready() const noexcept {
        return awaked_;
    }

    void wake(T&& ret) {
        Awaker<T>::wake(std::move(ret));
        awaked_ = true;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        Awaker<T>::await_suspend(handle);
        awaked_ = false;
    }
};

template<>
struct CheckableAwaker<void>: Awaker<void>, CheckableAwaker_Base {
    bool await_ready() const noexcept {
        return awaked_;
    }

    void wake() {
        Awaker<void>::wake();
        awaked_ = true;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        Awaker<void>::await_suspend(handle);
        awaked_ = false;
    }
};

template<typename T>
struct MaybeSuspend_Base {
    Promise<T>& promise_;
    bool await_ready() const noexcept;

    void await_suspend(std::coroutine_handle<> handle) noexcept {}

    MaybeSuspend_Base(Promise<T>& promise) : promise_(promise) {}
};

template<typename T>
struct MaybeSuspend: MaybeSuspend_Base<T> {
    T&& await_resume() noexcept;

    MaybeSuspend(Promise<T>& promise) : MaybeSuspend_Base<T>(promise) {}
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
struct MaybeSuspend<void>: MaybeSuspend_Base<void> {
    void await_resume() const noexcept;

    MaybeSuspend(Promise<void>& promise) : MaybeSuspend_Base<void>(promise) {}
};

/**
 * FinalSuspend aim to ensure all coro suspend at least once
 *
 * if `Task` has no caller_, it imply that this coro has run complete on await_transform
 *
 * or it was submit to scheduler
 *
 * FIXME: submitted task should be destroyed
 *
 */
struct FinalSuspend {
    bool ready_;

    FinalSuspend(bool ready) : ready_(ready) {}

    bool await_ready() const noexcept {
        return ready_;
    }

    constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}

    constexpr void await_resume() const noexcept {}
};

template<typename T, typename S>
struct PromiseBase {
    ~PromiseBase() {
        S_DBUG("~{}()", get_return_object().address());
    }

    PromiseBase() {
        S_DBUG("{}()", get_return_object().address());
    }

    std::suspend_always initial_suspend() noexcept {
        S_DBUG("initial_suspend: {}", get_return_object().address());
        return {};
    }

    FinalSuspend final_suspend() noexcept {
        S_DBUG("final_suspend: {}", get_return_object().address());
        return this->resume_caller();
    }

    void unhandled_exception() {
        S_INFO("unhandled exception");
        std::rethrow_exception(std::current_exception());
    }

    SuspendYield yield_value(std::nullptr_t) {
        return {};
    }

    template<typename T2>
    MaybeSuspend<T2> await_transform(Task<T2> task) {
        S_DBUG("task resume: {}", task.address());
        S_ASSERT(task.promise().caller_ == nullptr);
        task.resume(); // do (initial_suspend, first_suspend)
        if (!task.done()) { // exist suspend on coro body
            task.promise().caller_ = get_return_object();
        }
        S_DBUG(
            "await_transform: {}, done?={}, self: {}",
            task.address(),
            task.done(),
            get_return_object().address()
        );
        return MaybeSuspend(task.promise());
    }

    template</*typename T2, */ typename T1>
    T1& await_transform(T1& task) /*requires Awakable<T2, T1>*/ {
        return task;
    }

    Task<T> get_return_object() {
        return {Task<T>::from_promise(*static_cast<S*>(this))};
    }

    bool resume_caller() {
        if (this->caller_.address() != nullptr) {
            S_DBUG("resume_caller: {}", this->caller_.address());
            this->caller_.resume();
            // caller has resumed explicit; this coro can be destroyed
            // immediatly
            return true;
        } else {
            S_DBUG("caller=null {}", this->get_return_object().address());
            // there is no caller,
            return false;
        }
    }

    std::coroutine_handle<> caller_ {nullptr};
};

template<typename T>
struct Promise: PromiseBase<T, Promise<T>> {
    T result_;

    void return_value(T&& ret) {
        this->result_ = std::move(ret);
        S_DBUG("return_value quit {}", this->get_return_object().address());
    }

  private:
};

template<>
struct Promise<void>: PromiseBase<void, Promise<void>> {
    void return_void() {
        S_DBUG("return_void");
    }
};

template<typename T>
inline bool MaybeSuspend_Base<T>::await_ready() const noexcept {
    S_DBUG(
        "await_ready hdl: {}, done={}",
        promise_.get_return_object().address(),
        promise_.get_return_object().done()
    );
    return promise_.get_return_object().done();
}

template<typename T>
inline T&& MaybeSuspend<T>::await_resume() noexcept {
    S_DBUG(
        "await_resume hdl: {}, done={}",
        this->promise_.get_return_object().address(),
        this->promise_.get_return_object().done()
    );
    T&& ret = std::move(this->promise_.result_);
    // caller is null means this coro was directly resumed on co_await,
    // and now it was in final_suspend point,
    // in this case, we should call destroy explicit
    if (!this->promise_.caller_) {
        this->promise_.get_return_object().destroy();
    }
    return std::move(ret);
}

inline void MaybeSuspend<void>::await_resume() const noexcept {
    if (!this->promise_.caller_) {
        promise_.get_return_object().destroy();
    }
}

template<>
inline bool MaybeSuspend_Base<void>::await_ready() const noexcept {
    return promise_.get_return_object().done();
}

} // namespace sqk

#endif // !SQK_CORE_HPP
