#pragma once

#include <compare>
#include <string_view>
#include <variant>

namespace tk {

struct id {
  std::string_view name;
  friend std::strong_ordering operator<=>(const id&, const id&) = default;
};
struct kw_let {};     // let
struct op_plus {};    // +
struct op_minus {};   //-
struct op_mul {};     // *
struct op_div {};     // /
struct op_assign {};  // =
struct literal_int {
  std::string_view value;
  friend std::strong_ordering operator<=>(const literal_int&, const literal_int&) = default;
};

}  // namespace tk

using token =
    std::variant<tk::id,
                 tk::literal_int /*, tk::kw_let, tk::op_plus, tk::op_minus, tk::op_mul, tk::op_div, tk::op_assign*/>;
