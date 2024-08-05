#include <coroutine>
#include <list>
#include <iostream>

namespace sqk {

struct SQKAwaitable;

template <typename Promise>
struct SQKCoroutine : std::coroutine_handle<Promise> {
	using promise_type = Promise;
};

template <typename Promise> struct BASE_SQKPromise {
  SQKCoroutine<Promise> get_return_object() {
    return {SQKCoroutine<Promise>::from_promise(*static_cast<Promise*>(this))};
  }
  void return_void() {}
  void unhandled_exception() {}
  BASE_SQKPromise(){};
  ~BASE_SQKPromise(){};
};


struct SQKScheduler {
	std::list<std::coroutine_handle<>> queue_;
	public:

	int enqueue(std::coroutine_handle<> handle) {
		queue_.push_back(handle);
		return 0;
	}

	[[noreturn]] int run() {
		for (;;) {
			while (!queue_.empty()) {
				auto handle = queue_.front();
				handle.resume();
				queue_.pop_front();
			}
		}
	}
};
static __thread SQKScheduler *scheduler;
struct SQKAwaitable {

    constexpr bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) const noexcept {
		std::cout << "await_suspend" << std::endl;
		scheduler->enqueue(handle);
		
	}
    void await_resume() const noexcept {
		std::cout << "await_resume" << std::endl;
	}
};

struct DummyPromise : BASE_SQKPromise<DummyPromise> {
	SQKAwaitable initial_suspend() noexcept {
		std::cout << "initial_suspend" << std::endl;
		return {};
	}
	std::suspend_never final_suspend() noexcept {
		std::cout << "final_suspend" << std::endl;
		return {};
	}
};

SQKCoroutine<DummyPromise> f() {
	co_await SQKAwaitable {};
	co_return;
}
void run() {
	scheduler = new SQKScheduler();
	f();
	scheduler->run();
}

}
int main() {
	sqk::run();
}
