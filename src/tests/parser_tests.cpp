// gtest
#include <gtest/gtest.h>
// luca
#include <parser.hpp>

namespace tests {
namespace {

auto parse_ok(const std::string& src) {
  try {
    return parse(src);
  } catch (const parse_err& e) {
    EXPECT_TRUE(false) << "parse failed for: " << src << ": " << e.what();
    throw;
  }
}

template <class T>
const T& as(const ast::term& t) {
  return std::get<T>(t);
}
template <class T>
bool is(const ast::term& t) {
  return std::holds_alternative<T>(t);
}

}  // namespace

// -- atoms -------------------------------------------------------------------

TEST(parser_tests, integer_literal) {
  auto r = parse_ok("42");
  EXPECT_TRUE(is<ast::li_int>(r.first));
  EXPECT_EQ(as<ast::li_int>(r.first).value, 42);
}

TEST(parser_tests, integer_zero) {
  auto r = parse_ok("0");
  EXPECT_EQ(as<ast::li_int>(r.first).value, 0);
}

TEST(parser_tests, boolean_true) {
  auto r = parse_ok("true");
  EXPECT_TRUE(is<ast::li_bool>(r.first));
  EXPECT_EQ(as<ast::li_bool>(r.first).value, true);
}

TEST(parser_tests, boolean_false) {
  auto r = parse_ok("false");
  EXPECT_TRUE(is<ast::li_bool>(r.first));
  EXPECT_EQ(as<ast::li_bool>(r.first).value, false);
}

TEST(parser_tests, variable) {
  auto r = parse_ok("x");
  EXPECT_TRUE(is<ast::var>(r.first));
  EXPECT_EQ(as<ast::var>(r.first).index, std::nullopt);  // free variable
}

// -- binary ops --------------------------------------------------------------

TEST(parser_tests, simple_addition) {
  auto r = parse_ok("1 + 2");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(b.op));
  EXPECT_EQ(as<ast::li_int>(*b.left).value, 1);
  EXPECT_EQ(as<ast::li_int>(*b.right).value, 2);
}

TEST(parser_tests, simple_multiplication) {
  auto r = parse_ok("3 * 4");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_mul>(b.op));
  EXPECT_EQ(as<ast::li_int>(*b.left).value, 3);
  EXPECT_EQ(as<ast::li_int>(*b.right).value, 4);
}

TEST(parser_tests, precedence_mul_over_add) {
  auto r = parse_ok("1 + 2 * 3");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(b.op));
  EXPECT_EQ(as<ast::li_int>(*b.left).value, 1);
  const auto& rhs = as<ast::binop>(*b.right);
  EXPECT_TRUE(std::holds_alternative<tk::op_mul>(rhs.op));
  EXPECT_EQ(as<ast::li_int>(*rhs.left).value, 2);
  EXPECT_EQ(as<ast::li_int>(*rhs.right).value, 3);
}

TEST(parser_tests, precedence_add_over_mul_left_to_right) {
  auto r = parse_ok("1 * 2 + 3");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(b.op));
  const auto& lhs = as<ast::binop>(*b.left);
  EXPECT_TRUE(std::holds_alternative<tk::op_mul>(lhs.op));
  EXPECT_EQ(as<ast::li_int>(*lhs.left).value, 1);
  EXPECT_EQ(as<ast::li_int>(*lhs.right).value, 2);
  EXPECT_EQ(as<ast::li_int>(*b.right).value, 3);
}

TEST(parser_tests, left_associative_add) {
  auto r = parse_ok("1 + 2 + 3");
  const auto& outer = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(outer.op));
  const auto& inner = as<ast::binop>(*outer.left);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(inner.op));
  EXPECT_EQ(as<ast::li_int>(*inner.left).value, 1);
  EXPECT_EQ(as<ast::li_int>(*inner.right).value, 2);
  EXPECT_EQ(as<ast::li_int>(*outer.right).value, 3);
}

TEST(parser_tests, left_associative_mul) {
  auto r = parse_ok("4 * 5 * 6");
  const auto& outer = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_mul>(outer.op));
  const auto& inner = as<ast::binop>(*outer.left);
  EXPECT_EQ(as<ast::li_int>(*inner.left).value, 4);
  EXPECT_EQ(as<ast::li_int>(*inner.right).value, 5);
  EXPECT_EQ(as<ast::li_int>(*outer.right).value, 6);
}

TEST(parser_tests, mixed_precedence_chain) {
  auto r = parse_ok("1 + 2 * 3 - 4 / 5");
  const auto& sub = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_minus>(sub.op));
  const auto& add = as<ast::binop>(*sub.left);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(add.op));
  EXPECT_EQ(as<ast::li_int>(*add.left).value, 1);
  // right of add: 2 * 3
  const auto& mul = as<ast::binop>(*add.right);
  EXPECT_TRUE(std::holds_alternative<tk::op_mul>(mul.op));
  EXPECT_EQ(as<ast::li_int>(*mul.left).value, 2);
  EXPECT_EQ(as<ast::li_int>(*mul.right).value, 3);
  // right of sub: 4 / 5
  const auto& div = as<ast::binop>(*sub.right);
  EXPECT_TRUE(std::holds_alternative<tk::op_div>(div.op));
  EXPECT_EQ(as<ast::li_int>(*div.left).value, 4);
  EXPECT_EQ(as<ast::li_int>(*div.right).value, 5);
}

// -- unary minus -------------------------------------------------------------

TEST(parser_tests, unary_minus_integer) {
  auto r = parse_ok("-5");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_minus>(b.op));
  EXPECT_EQ(as<ast::li_int>(*b.left).value, 0);
  EXPECT_EQ(as<ast::li_int>(*b.right).value, 5);
}

TEST(parser_tests, unary_minus_with_binary) {
  auto r = parse_ok("-5 + 3");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(b.op));
  const auto& neg = as<ast::binop>(*b.left);
  EXPECT_TRUE(std::holds_alternative<tk::op_minus>(neg.op));
  EXPECT_EQ(as<ast::li_int>(*neg.left).value, 0);
  EXPECT_EQ(as<ast::li_int>(*neg.right).value, 5);
  EXPECT_EQ(as<ast::li_int>(*b.right).value, 3);
}

TEST(parser_tests, unary_minus_parenthesized) {
  auto r = parse_ok("-(1 + 2)");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_minus>(b.op));
  EXPECT_EQ(as<ast::li_int>(*b.left).value, 0);
  const auto& inner = as<ast::binop>(*b.right);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(inner.op));
  EXPECT_EQ(as<ast::li_int>(*inner.left).value, 1);
  EXPECT_EQ(as<ast::li_int>(*inner.right).value, 2);
}

TEST(parser_tests, double_unary_minus) {
  auto r = parse_ok("--5");
  const auto& outer = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_minus>(outer.op));
  EXPECT_EQ(as<ast::li_int>(*outer.left).value, 0);
  const auto& inner = as<ast::binop>(*outer.right);
  EXPECT_TRUE(std::holds_alternative<tk::op_minus>(inner.op));
  EXPECT_EQ(as<ast::li_int>(*inner.right).value, 5);
}

// -- implicit application ----------------------------------------------------

TEST(parser_tests, application_single) {
  auto r = parse_ok("f x");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_TRUE(is<ast::var>(*a.func));
  EXPECT_TRUE(is<ast::var>(*a.arg));
}

TEST(parser_tests, application_left_associative) {
  auto r = parse_ok("f x y");
  const auto& outer = as<ast::appl>(r.first);
  EXPECT_TRUE(is<ast::var>(*outer.arg));
  const auto& inner = as<ast::appl>(*outer.func);
  EXPECT_TRUE(is<ast::var>(*inner.func));
  EXPECT_TRUE(is<ast::var>(*inner.arg));
}

TEST(parser_tests, application_with_literal) {
  auto r = parse_ok("f 42");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_TRUE(is<ast::var>(*a.func));
  EXPECT_EQ(as<ast::li_int>(*a.arg).value, 42);
}

TEST(parser_tests, application_prec_over_binop) {
  auto r = parse_ok("f 1 + 2");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(b.op));
  const auto& a = as<ast::appl>(*b.left);
  EXPECT_TRUE(is<ast::var>(*a.func));
  EXPECT_EQ(as<ast::li_int>(*a.arg).value, 1);
  EXPECT_EQ(as<ast::li_int>(*b.right).value, 2);
}

TEST(parser_tests, application_boolean_arg) {
  auto r = parse_ok("f true");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_TRUE(is<ast::var>(*a.func));
  EXPECT_EQ(as<ast::li_bool>(*a.arg).value, true);
}

TEST(parser_tests, application_if_arg) {
  auto r = parse_ok("f if true then 1 else 2");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_TRUE(is<ast::var>(*a.func));
  EXPECT_TRUE(is<ast::ifexpr>(*a.arg));
}

// -- parenthesized expressions -----------------------------------------------

TEST(parser_tests, parens_atom) {
  auto r = parse_ok("(42)");
  EXPECT_EQ(as<ast::li_int>(r.first).value, 42);
}

TEST(parser_tests, parens_override_precedence) {
  auto r = parse_ok("(1 + 2) * 3");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_mul>(b.op));
  const auto& inner = as<ast::binop>(*b.left);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(inner.op));
  EXPECT_EQ(as<ast::li_int>(*inner.left).value, 1);
  EXPECT_EQ(as<ast::li_int>(*inner.right).value, 2);
  EXPECT_EQ(as<ast::li_int>(*b.right).value, 3);
}

TEST(parser_tests, parens_application_rhs) {
  auto r = parse_ok("(f x) y");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_TRUE(is<ast::var>(*a.arg));
  const auto& inner = as<ast::appl>(*a.func);
  EXPECT_TRUE(is<ast::var>(*inner.func));
  EXPECT_TRUE(is<ast::var>(*inner.arg));
}

// -- lambda abstractions -----------------------------------------------------

TEST(parser_tests, lambda_int_param) {
  auto r = parse_ok("\\x : int . x");
  const auto& ab = as<ast::abst>(r.first);
  EXPECT_TRUE(std::holds_alternative<ast::type_int>(ab.param_type));
  EXPECT_TRUE(is<ast::var>(*ab.body));
}

TEST(parser_tests, lambda_bool_param) {
  auto r = parse_ok("\\x : bool . true");
  const auto& ab = as<ast::abst>(r.first);
  EXPECT_TRUE(std::holds_alternative<ast::type_bool>(ab.param_type));
  EXPECT_EQ(as<ast::li_bool>(*ab.body).value, true);
}

TEST(parser_tests, lambda_string_param) {
  auto r = parse_ok("\\x : string . 0");
  const auto& ab = as<ast::abst>(r.first);
  EXPECT_TRUE(std::holds_alternative<ast::type_string>(ab.param_type));
  EXPECT_EQ(as<ast::li_int>(*ab.body).value, 0);
}

TEST(parser_tests, lambda_unit_param) {
  auto r = parse_ok("\\x : () . 42");
  const auto& ab = as<ast::abst>(r.first);
  EXPECT_TRUE(std::holds_alternative<ast::type_unit>(ab.param_type));
  EXPECT_EQ(as<ast::li_int>(*ab.body).value, 42);
}

TEST(parser_tests, lambda_keyword_syntax) {
  auto r = parse_ok("lambda x : int . x");
  const auto& ab = as<ast::abst>(r.first);
  EXPECT_TRUE(std::holds_alternative<ast::type_int>(ab.param_type));
}

TEST(parser_tests, lambda_body_extends_right) {
  auto r = parse_ok("\\x : int . x y");
  const auto& ab = as<ast::abst>(r.first);
  const auto& a = as<ast::appl>(*ab.body);
  EXPECT_TRUE(is<ast::var>(*a.func));
  EXPECT_TRUE(is<ast::var>(*a.arg));
}

TEST(parser_tests, lambda_body_multi_app) {
  auto r = parse_ok("\\x : int . x y z");
  const auto& ab = as<ast::abst>(r.first);
  const auto& outer = as<ast::appl>(*ab.body);
  EXPECT_TRUE(is<ast::var>(*outer.arg));
  const auto& inner = as<ast::appl>(*outer.func);
  EXPECT_TRUE(is<ast::var>(*inner.func));
  EXPECT_TRUE(is<ast::var>(*inner.arg));
}

TEST(parser_tests, lambda_application_left) {
  auto r = parse_ok("\\x : int . x 5");
  const auto& ab = as<ast::abst>(r.first);
  const auto& a = as<ast::appl>(*ab.body);
  EXPECT_EQ(as<ast::var>(*a.func).index, 0);
  EXPECT_EQ(as<ast::li_int>(*a.arg).value, 5);
}

TEST(parser_tests, lambda_de_bruijn_single) {
  auto r = parse_ok("\\x : int . x");
  const auto& ab = as<ast::abst>(r.first);
  EXPECT_EQ(as<ast::var>(*ab.body).index, 0);  // bound to innermost
}

TEST(parser_tests, lambda_de_bruijn_nested) {
  auto r = parse_ok("\\x : int . \\y : bool . x y");
  const auto& outer = as<ast::abst>(r.first);
  const auto& inner = as<ast::abst>(*outer.body);
  // in the inner body: x y
  const auto& app = as<ast::appl>(*inner.body);
  EXPECT_EQ(as<ast::var>(*app.func).index, 1);  // x: one binder out
  EXPECT_EQ(as<ast::var>(*app.arg).index, 0);   // y: innermost binder
}

TEST(parser_tests, lambda_de_bruijn_shadow) {
  auto r = parse_ok("\\x : int . \\x : bool . x");
  const auto& outer = as<ast::abst>(r.first);
  const auto& inner = as<ast::abst>(*outer.body);
  EXPECT_EQ(as<ast::var>(*inner.body).index, 0);  // inner x shadows outer
}

// -- if expressions ----------------------------------------------------------

TEST(parser_tests, if_simple) {
  auto r = parse_ok("if true then 1 else 2");
  const auto& ie = as<ast::ifexpr>(r.first);
  EXPECT_EQ(as<ast::li_bool>(*ie.cond).value, true);
  EXPECT_EQ(as<ast::li_int>(*ie.then).value, 1);
  EXPECT_EQ(as<ast::li_int>(*ie.els).value, 2);
}

TEST(parser_tests, if_with_binop_cond) { EXPECT_THROW(parse("if 1 + 2 then 3 else 4"), parse_err); }

TEST(parser_tests, if_parenthesized_branches) {
  auto r = parse_ok("if true then 1 + 2 else 3 + 4");
  const auto& ie = as<ast::ifexpr>(r.first);
  EXPECT_TRUE(is<ast::binop>(*ie.then));
  EXPECT_TRUE(is<ast::binop>(*ie.els));
}

TEST(parser_tests, nested_if) {
  auto r = parse_ok("if true then if false then 1 else 2 else 3");
  const auto& outer = as<ast::ifexpr>(r.first);
  const auto& inner = as<ast::ifexpr>(*outer.then);
  EXPECT_EQ(as<ast::li_bool>(*inner.cond).value, false);
  EXPECT_EQ(as<ast::li_int>(*inner.then).value, 1);
  EXPECT_EQ(as<ast::li_int>(*inner.els).value, 2);
  EXPECT_EQ(as<ast::li_int>(*outer.els).value, 3);
}

// -- compositions ------------------------------------------------------------

TEST(parser_tests, lambda_applied_directly) {
  auto r = parse_ok("(\\x : int . x) 42");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_TRUE(is<ast::abst>(*a.func));
  EXPECT_EQ(as<ast::li_int>(*a.arg).value, 42);
}

TEST(parser_tests, lambda_in_if) {
  auto r = parse_ok("if true then \\x : int . x else \\x : bool . false");
  const auto& ie = as<ast::ifexpr>(r.first);
  EXPECT_TRUE(is<ast::abst>(*ie.then));
  EXPECT_TRUE(is<ast::abst>(*ie.els));
}

TEST(parser_tests, if_in_lambda_body) {
  auto r = parse_ok("\\x : int . if x then 1 else 0");
  const auto& ab = as<ast::abst>(r.first);
  EXPECT_TRUE(is<ast::ifexpr>(*ab.body));
}

TEST(parser_tests, complex_nesting) {
  auto r = parse_ok("\\x : int . if x then f x + 1 else g y * 2");
  const auto& ab = as<ast::abst>(r.first);
  const auto& ie = as<ast::ifexpr>(*ab.body);
  EXPECT_TRUE(is<ast::var>(*ie.cond));
  const auto& then_b = as<ast::binop>(*ie.then);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(then_b.op));
  const auto& else_b = as<ast::binop>(*ie.els);
  EXPECT_TRUE(std::holds_alternative<tk::op_mul>(else_b.op));
}

struct parser_error_theory : ::testing::TestWithParam<std::string> {};
TEST_P(parser_error_theory, reject) { EXPECT_THROW(parse(GetParam()), parse_err); }

INSTANTIATE_TEST_SUITE_P(general, parser_error_theory,
                         ::testing::Values("", ")", "(1 + 2", "1 +", "int", "1 + true"));
INSTANTIATE_TEST_SUITE_P(lambda, parser_error_theory,
                         ::testing::Values("\\", "\\int . x", "\\x int . x", "\\x : int", "\\x : int .",
                                           "\\x : if . x"));
INSTANTIATE_TEST_SUITE_P(if_expr, parser_error_theory,
                         ::testing::Values("if true 1 else 2", "if true then 1 2", "if 42 then 1 else 2",
                                           "if true then 1 else true"));

}  // namespace tests
