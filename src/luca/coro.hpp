#pragma once

// std
#include <coroutine>
#include <exception>
#include <utility>
#include <variant>

template <class T>
class lazy_promise;

template <class T>
class lazy {
  friend class lazy_promise<T>;

 public:
  lazy(const lazy&) = delete;
  lazy& operator=(const lazy&) = delete;
  lazy(lazy&& other) noexcept : coro_{std::exchange(other.coro_, {})} {}
  lazy& operator=(lazy&& other) noexcept {
    if (this != &other) {
      if (coro_) coro_.destroy();
      coro_ = std::exchange(other.coro_, {});
    }
    return *this;
  }
  ~lazy() {
    if (coro_) coro_.destroy();
  }

  class awaiter;
  awaiter operator co_await() noexcept;

  T get();

 private:
  explicit lazy(std::coroutine_handle<lazy_promise<T>> coro) : coro_{coro} {}
  std::coroutine_handle<lazy_promise<T>> coro_;
};

template <class T>
class lazy_promise {
  friend class lazy<T>::awaiter;

 public:
  lazy<T> get_return_object() noexcept { return lazy<T>{std::coroutine_handle<lazy_promise>::from_promise(*this)}; }
  constexpr auto initial_suspend() noexcept { return std::suspend_always{}; }
  constexpr auto final_suspend() noexcept {
    struct final_awaiter : std::suspend_always {
      std::coroutine_handle<> await_suspend(std::coroutine_handle<lazy_promise> handle) noexcept {
        auto cont = handle.promise().cont_;
        return cont ? cont : std::noop_coroutine();
      }
    };
    return final_awaiter{};
  }
  template <class U>
  void return_value(U&& value) {
    state_.template emplace<T>(std::forward<U>(value));
  }
  void unhandled_exception() noexcept { state_ = std::current_exception(); }

 private:
  std::coroutine_handle<> cont_;
  std::variant<std::monostate, T, std::exception_ptr> state_;
};

template <class T>
class lazy<T>::awaiter : public std::suspend_always {
 public:
  explicit awaiter(std::coroutine_handle<lazy_promise<T>> coro) : coro_{coro} {}

  std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont) noexcept {
    coro_.promise().cont_ = cont;
    return coro_;
  }

  T await_resume() {
    if (auto* value = std::get_if<T>(&coro_.promise().state_)) return std::move(*value);
    if (auto* exception = std::get_if<std::exception_ptr>(&coro_.promise().state_))
      std::rethrow_exception(std::move(*exception));
    std::terminate();
  }

  T get() {
    coro_();
    return await_resume();
  }

 private:
  std::coroutine_handle<lazy_promise<T>> coro_;
};

template <class T>
typename lazy<T>::awaiter lazy<T>::operator co_await() noexcept {
  return lazy<T>::awaiter{coro_};
}

template <class T>
T lazy<T>::get() {
  return lazy<T>::awaiter{coro_}.get();
}

namespace std {

template <class T, class... Args>
struct coroutine_traits<lazy<T>, Args...> {
  using promise_type = lazy_promise<T>;
};

}  // namespace std
