#pragma once

#include <compare>
#include <string_view>
#include <variant>

namespace tk {

struct id {
  std::string_view name;
  friend std::strong_ordering operator<=>(id, id) = default;
};
struct kw_lambda {
  friend std::strong_ordering operator<=>(kw_lambda, kw_lambda) = default;
};
struct kw_let {
  friend std::strong_ordering operator<=>(kw_let, kw_let) = default;
};
struct kw_in {
  friend std::strong_ordering operator<=>(kw_in, kw_in) = default;
};
struct kw_if {
  friend std::strong_ordering operator<=>(kw_if, kw_if) = default;
};
struct kw_then {
  friend std::strong_ordering operator<=>(kw_then, kw_then) = default;
};
struct kw_else {
  friend std::strong_ordering operator<=>(kw_else, kw_else) = default;
};
struct kw_true {
  friend std::strong_ordering operator<=>(kw_true, kw_true) = default;
};
struct kw_false {
  friend std::strong_ordering operator<=>(kw_false, kw_false) = default;
};
struct kw_bool {
  friend std::strong_ordering operator<=>(kw_bool, kw_bool) = default;
};
struct kw_int {
  friend std::strong_ordering operator<=>(kw_int, kw_int) = default;
};
struct kw_string {
  friend std::strong_ordering operator<=>(kw_string, kw_string) = default;
};
struct kw_fix {
  friend std::strong_ordering operator<=>(kw_fix, kw_fix) = default;
};
struct kw_type {
  friend std::strong_ordering operator<=>(kw_type, kw_type) = default;
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
};
struct op_minus {
  friend std::strong_ordering operator<=>(op_minus, op_minus) = default;
};
struct op_mul {
  friend std::strong_ordering operator<=>(op_mul, op_mul) = default;
};
struct op_div {
  friend std::strong_ordering operator<=>(op_div, op_div) = default;
};
struct op_ne {
  friend std::strong_ordering operator<=>(op_ne, op_ne) = default;
};
struct op_eq {
  friend std::strong_ordering operator<=>(op_eq, op_eq) = default;
};
struct op_gt {
  friend std::strong_ordering operator<=>(op_gt, op_gt) = default;
};
struct op_lt {
  friend std::strong_ordering operator<=>(op_lt, op_lt) = default;
};
struct op_comma {
  friend std::strong_ordering operator<=>(op_comma, op_comma) = default;
};
struct op_arrow {
  friend std::strong_ordering operator<=>(op_arrow, op_arrow) = default;
};
struct op_colon {
  friend std::strong_ordering operator<=>(op_colon, op_colon) = default;
};
struct op_dot {
  friend std::strong_ordering operator<=>(op_dot, op_dot) = default;
};
struct lparen {
  friend std::strong_ordering operator<=>(lparen, lparen) = default;
};
struct rparen {
  friend std::strong_ordering operator<=>(rparen, rparen) = default;
};
struct lbrace {
  friend std::strong_ordering operator<=>(lbrace, lbrace) = default;
};
struct rbrace {
  friend std::strong_ordering operator<=>(rbrace, rbrace) = default;
};

}  // namespace tk

using token =
    std::variant<tk::id, tk::kw_lambda, tk::kw_let, tk::kw_in, tk::kw_if, tk::kw_then, tk::kw_else, tk::kw_true,
                 tk::kw_false, tk::kw_bool, tk::kw_int, tk::kw_string, tk::kw_fix, tk::kw_type, tk::li_int, tk::li_str,
                 tk::op_plus, tk::op_minus, tk::op_mul, tk::op_div, tk::op_eq, tk::op_ne, tk::op_gt, tk::op_lt,
                 tk::op_arrow, tk::op_comma, tk::op_colon, tk::op_dot, tk::lparen, tk::rparen, tk::lbrace, tk::rbrace>;
