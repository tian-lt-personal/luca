#pragma once

#include <compare>
#include <string_view>
#include <variant>

namespace tk {

struct id {
  std::string_view name;
  friend std::strong_ordering operator<=>(id, id) = default;
};
struct li_int {
  std::string_view value;
  friend std::strong_ordering operator<=>(li_int, li_int) = default;
};
struct li_str {
  std::string_view raw;
  friend std::strong_ordering operator<=>(li_str, li_str) = default;
};
struct op_plus {
  friend std::strong_ordering operator<=>(op_plus, op_plus) = default;
};  // +
struct op_minus {
  friend std::strong_ordering operator<=>(op_minus, op_minus) = default;
};  // -
struct op_mul {
  friend std::strong_ordering operator<=>(op_mul, op_mul) = default;
};  // *
struct op_div {
  friend std::strong_ordering operator<=>(op_div, op_div) = default;
};  // /
struct op_eq {
  friend std::strong_ordering operator<=>(op_eq, op_eq) = default;
};  // =

}  // namespace tk

using token =
    std::variant<tk::id, tk::li_int, tk::li_str, tk::op_plus, tk::op_minus, tk::op_mul, tk::op_div, tk::op_eq>;
