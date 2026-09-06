#include <gtest/gtest.h>
// std
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
// luca
#include <coro.hpp>

namespace tests {
namespace {

lazy<int> make_int(int value) { co_return value; }

lazy<int> add_children(int left, int right) {
  auto left_result = make_int(left);
  auto right_result = make_int(right);
  co_return co_await left_result + co_await right_result;
}

lazy<int> deep_chain(int depth) {
  if (depth == 0) co_return 0;
  auto child = deep_chain(depth - 1);
  co_return 1 + co_await child;
}

struct test_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

lazy<int> throw_error() {
  throw test_error{"lazy failure"};
  co_return 0;
}

lazy<int> recover_from_error() {
  try {
    co_await throw_error();
  } catch (const test_error&) {
    co_return 42;
  }
  co_return 0;
}

lazy<std::unique_ptr<int>> make_pointer(int value) { co_return std::make_unique<int>(value); }

using nested_value = std::variant<std::monostate, int>;

lazy<nested_value> make_nested_unit() { co_return nested_value{std::monostate{}}; }

}  // namespace

static_assert(!std::is_copy_constructible_v<lazy<int>>);
static_assert(std::is_move_constructible_v<lazy<int>>);

TEST(coro_tests, get_returns_value) { EXPECT_EQ(make_int(42).get(), 42); }

TEST(coro_tests, co_await_composes_children) { EXPECT_EQ(add_children(20, 22).get(), 42); }

TEST(coro_tests, symmetric_transfer_handles_deep_non_tail_chain) {
  constexpr int depth = 100'000;
  EXPECT_EQ(deep_chain(depth).get(), depth);
}

TEST(coro_tests, exceptions_are_rethrown_from_get) {
  try {
    throw_error().get();
    FAIL() << "expected lazy failure";
  } catch (const test_error& error) {
    EXPECT_STREQ(error.what(), "lazy failure");
  }
}

TEST(coro_tests, awaited_exceptions_can_be_caught) { EXPECT_EQ(recover_from_error().get(), 42); }

TEST(coro_tests, move_transfers_frame_ownership) {
  auto source = make_int(41);
  auto destination = make_int(99);
  destination = std::move(source);
  EXPECT_EQ(destination.get(), 41);
}

TEST(coro_tests, move_only_result_is_returned) {
  auto result = make_pointer(42).get();
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(*result, 42);
}

TEST(coro_tests, nested_variant_value_can_contain_monostate) {
  auto result = make_nested_unit().get();
  EXPECT_TRUE(std::holds_alternative<std::monostate>(result));
}

}  // namespace tests
