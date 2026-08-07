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
struct kw_let {};     // let
struct op_plus {};    // +
struct op_minus {};   //-
struct op_mul {};     // *
struct op_div {};     // /
struct op_assign {};  // =

}  // namespace tk

using token = std::variant<tk::id, tk::li_int>;
