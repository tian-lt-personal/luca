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

TEST(parser_tests, variable_bound) {
  auto r = parse_ok("(\\x : int . x) 42");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  EXPECT_EQ(as<ast::var>(*ab.body).index, 0);
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

// -- comparison ops -----------------------------------------------------------

TEST(parser_tests, comparison_eq) {
  auto r = parse_ok("1 = 2");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_eq>(b.op));
  EXPECT_EQ(as<ast::li_int>(*b.left).value, 1);
  EXPECT_EQ(as<ast::li_int>(*b.right).value, 2);
}

TEST(parser_tests, comparison_ne) {
  auto r = parse_ok("3 != 4");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_ne>(b.op));
}

TEST(parser_tests, comparison_gt) {
  auto r = parse_ok("5 > 3");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_gt>(b.op));
}

TEST(parser_tests, comparison_lt) {
  auto r = parse_ok("1 < 2");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_lt>(b.op));
}

TEST(parser_tests, comparison_prec_over_arithmetic) {
  auto r = parse_ok("1 + 2 < 3 * 4");
  const auto& b = as<ast::binop>(r.first);
  EXPECT_TRUE(std::holds_alternative<tk::op_lt>(b.op));
  const auto& lhs = as<ast::binop>(*b.left);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(lhs.op));
  const auto& rhs = as<ast::binop>(*b.right);
  EXPECT_TRUE(std::holds_alternative<tk::op_mul>(rhs.op));
}

TEST(parser_tests, comparison_as_if_cond) {
  auto r = parse_ok("if 1 < 2 then 3 else 4");
  const auto& ie = as<ast::ifexpr>(r.first);
  const auto& cmp = as<ast::binop>(*ie.cond);
  EXPECT_TRUE(std::holds_alternative<tk::op_lt>(cmp.op));
}

// -- let-in expressions -------------------------------------------------------

TEST(parser_tests, let_simple) {
  auto r = parse_ok("let x : int = 1 in x");
  // desugars to (\x : int . x) 1
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  EXPECT_TRUE(std::holds_alternative<ast::type_int>(ab.param_type));
  EXPECT_EQ(as<ast::var>(*ab.body).index, 0);
  EXPECT_EQ(as<ast::li_int>(*a.arg).value, 1);
}

TEST(parser_tests, let_with_binop_body) {
  auto r = parse_ok("let x : int = 1 in x + x");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  EXPECT_TRUE(std::holds_alternative<ast::type_int>(ab.param_type));
  const auto& body = as<ast::binop>(*ab.body);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(body.op));
}

TEST(parser_tests, let_nested) {
  auto r = parse_ok("let x : int = 1 in let y : int = 2 in x + y");
  // desugars to: (\x:int. (\y:int. x + y) 2) 1
  const auto& outer_app = as<ast::appl>(r.first);
  const auto& outer_abst = as<ast::abst>(*outer_app.func);
  EXPECT_EQ(as<ast::li_int>(*outer_app.arg).value, 1);
  // body of outer abst is the inner let: (\y:int. x + y) 2
  const auto& inner_app = as<ast::appl>(*outer_abst.body);
  const auto& inner_abst = as<ast::abst>(*inner_app.func);
  EXPECT_EQ(as<ast::li_int>(*inner_app.arg).value, 2);
  // body of inner abst is x + y (binop)
  const auto& body = as<ast::binop>(*inner_abst.body);
  // x bound by outer let (1 binder out), y bound by inner let (0)
  EXPECT_EQ(as<ast::var>(*body.left).index, 1);
  EXPECT_EQ(as<ast::var>(*body.right).index, 0);
}

TEST(parser_tests, let_bool_bound) {
  auto r = parse_ok("let x : bool = true in if x then 1 else 0");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  EXPECT_TRUE(std::holds_alternative<ast::type_bool>(ab.param_type));
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
  auto r = parse_ok("(\\f : int -> int . \\x : int . f x) (\\y : int . y) 5");
  const auto& app = as<ast::appl>(r.first);
  const auto& id_app = as<ast::appl>(*app.func);
  const auto& outer = as<ast::abst>(*id_app.func);
  const auto& inner = as<ast::abst>(*outer.body);
  const auto& a = as<ast::appl>(*inner.body);
  EXPECT_EQ(as<ast::var>(*a.func).index, 1);  // f: two binders out
  EXPECT_EQ(as<ast::var>(*a.arg).index, 0);   // x: innermost
}

TEST(parser_tests, application_left_associative) {
  auto r = parse_ok("(\\f : int -> int -> int . \\x : int . \\y : int . f x y) (\\a : int . \\b : int . a) 1 2");
  const auto& app = as<ast::appl>(r.first);
  const auto& id_app = as<ast::appl>(*app.func);
  const auto& ab_app = as<ast::appl>(*id_app.func);
  const auto& a0 = as<ast::abst>(*ab_app.func);
  const auto& a1 = as<ast::abst>(*a0.body);
  const auto& a2 = as<ast::abst>(*a1.body);
  const auto& outer = as<ast::appl>(*a2.body);
  const auto& inner = as<ast::appl>(*outer.func);
  EXPECT_TRUE(is<ast::var>(*inner.func));
  EXPECT_TRUE(is<ast::var>(*inner.arg));
  EXPECT_TRUE(is<ast::var>(*outer.arg));
}

TEST(parser_tests, application_with_literal) {
  auto r = parse_ok("(\\f : int -> int . f 42) (\\y : int . y)");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  const auto& inner = as<ast::appl>(*ab.body);
  EXPECT_EQ(as<ast::var>(*inner.func).index, 0);
  EXPECT_EQ(as<ast::li_int>(*inner.arg).value, 42);
}

TEST(parser_tests, application_prec_over_binop) {
  auto r = parse_ok("(\\f : int -> int . f 1 + 2) (\\y : int . y)");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  const auto& b = as<ast::binop>(*ab.body);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(b.op));
  const auto& inner = as<ast::appl>(*b.left);
  EXPECT_EQ(as<ast::var>(*inner.func).index, 0);
  EXPECT_EQ(as<ast::li_int>(*inner.arg).value, 1);
  EXPECT_EQ(as<ast::li_int>(*b.right).value, 2);
}

TEST(parser_tests, application_boolean_arg) {
  auto r = parse_ok("(\\f : bool -> int . f true) (\\y : bool . 1)");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  const auto& inner = as<ast::appl>(*ab.body);
  EXPECT_EQ(as<ast::var>(*inner.func).index, 0);
  EXPECT_EQ(as<ast::li_bool>(*inner.arg).value, true);
}

TEST(parser_tests, application_if_arg) {
  auto r = parse_ok("(\\f : int -> int . f if true then 1 else 2) (\\y : int . y)");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  const auto& inner = as<ast::appl>(*ab.body);
  EXPECT_EQ(as<ast::var>(*inner.func).index, 0);
  EXPECT_TRUE(is<ast::ifexpr>(*inner.arg));
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
  auto r = parse_ok("(\\f : int -> int -> int . \\x : int . \\y : int . (f x) y) (\\a : int . \\b : int . a) 1 2");
  const auto& app = as<ast::appl>(r.first);
  const auto& id_app = as<ast::appl>(*app.func);
  const auto& ab_app = as<ast::appl>(*id_app.func);
  const auto& a0 = as<ast::abst>(*ab_app.func);
  const auto& a1 = as<ast::abst>(*a0.body);
  const auto& a2 = as<ast::abst>(*a1.body);
  const auto& a = as<ast::appl>(*a2.body);
  EXPECT_TRUE(is<ast::var>(*a.arg));
  const auto& inner = as<ast::appl>(*a.func);
  EXPECT_TRUE(is<ast::var>(*inner.func));
  EXPECT_TRUE(is<ast::var>(*inner.arg));
}

// -- lambda abstractions -----------------------------------------------------

TEST(parser_tests, lambda_int_param) {
  auto r = parse_ok("(\\x : int . x) 42");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  EXPECT_TRUE(std::holds_alternative<ast::type_int>(ab.param_type));
  EXPECT_TRUE(is<ast::var>(*ab.body));
}

TEST(parser_tests, lambda_bool_param) {
  auto r = parse_ok("(\\x : bool . true) false");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  EXPECT_TRUE(std::holds_alternative<ast::type_bool>(ab.param_type));
  EXPECT_EQ(as<ast::li_bool>(*ab.body).value, true);
}

TEST(parser_tests, lambda_string_param) {
  auto r = parse_ok("let f : string -> int = \\x : string . 0 in 1");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.arg);
  EXPECT_TRUE(std::holds_alternative<ast::type_string>(ab.param_type));
  EXPECT_EQ(as<ast::li_int>(*ab.body).value, 0);
}

TEST(parser_tests, lambda_unit_param) {
  auto r = parse_ok("let f : () -> int = \\x : () . 42 in 1");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.arg);
  EXPECT_TRUE(std::holds_alternative<ast::type_unit>(ab.param_type));
  EXPECT_EQ(as<ast::li_int>(*ab.body).value, 42);
}

TEST(parser_tests, lambda_keyword_syntax) {
  auto r = parse_ok("(lambda x : int . x) 42");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  EXPECT_TRUE(std::holds_alternative<ast::type_int>(ab.param_type));
}

TEST(parser_tests, lambda_body_extends_right) {
  auto r = parse_ok("(\\x : int -> int . \\y : int . x y) (\\a : int . a) 5");
  const auto& app = as<ast::appl>(r.first);
  const auto& id_app = as<ast::appl>(*app.func);
  const auto& outer = as<ast::abst>(*id_app.func);
  const auto& inner = as<ast::abst>(*outer.body);
  const auto& a = as<ast::appl>(*inner.body);
  EXPECT_EQ(as<ast::var>(*a.func).index, 1);  // x: one binder out
  EXPECT_EQ(as<ast::var>(*a.arg).index, 0);   // y: innermost
}

TEST(parser_tests, lambda_body_multi_app) {
  auto r = parse_ok("(\\x : int -> int -> int . \\y : int . \\z : int . x y z) (\\a : int . \\b : int . a) 1 2");
  const auto& app = as<ast::appl>(r.first);
  const auto& id_app = as<ast::appl>(*app.func);
  const auto& ab_app = as<ast::appl>(*id_app.func);
  const auto& a0 = as<ast::abst>(*ab_app.func);
  const auto& a1 = as<ast::abst>(*a0.body);
  const auto& a2 = as<ast::abst>(*a1.body);
  const auto& outer = as<ast::appl>(*a2.body);
  EXPECT_TRUE(is<ast::var>(*outer.arg));
  const auto& inner = as<ast::appl>(*outer.func);
  EXPECT_TRUE(is<ast::var>(*inner.func));
  EXPECT_TRUE(is<ast::var>(*inner.arg));
}

TEST(parser_tests, lambda_application_left) {
  auto r = parse_ok("(\\x : int -> int . x 5) (\\a : int . a)");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  const auto& inner = as<ast::appl>(*ab.body);
  EXPECT_EQ(as<ast::var>(*inner.func).index, 0);
  EXPECT_EQ(as<ast::li_int>(*inner.arg).value, 5);
}

TEST(parser_tests, lambda_de_bruijn_single) {
  auto r = parse_ok("(\\x : int . x) 42");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  EXPECT_EQ(as<ast::var>(*ab.body).index, 0);  // bound to innermost
}

TEST(parser_tests, lambda_de_bruijn_nested) {
  auto r = parse_ok("(\\x : bool -> int . \\y : bool . x y) (\\a : bool . 1) true");
  const auto& app = as<ast::appl>(r.first);
  const auto& id_app = as<ast::appl>(*app.func);
  const auto& outer = as<ast::abst>(*id_app.func);
  const auto& inner = as<ast::abst>(*outer.body);
  // in the inner body: x y
  const auto& appl = as<ast::appl>(*inner.body);
  EXPECT_EQ(as<ast::var>(*appl.func).index, 1);  // x: one binder out
  EXPECT_EQ(as<ast::var>(*appl.arg).index, 0);   // y: innermost binder
}

TEST(parser_tests, lambda_de_bruijn_shadow) {
  auto r = parse_ok("(\\x : int . \\x : bool . x) 1 true");
  const auto& outer_app = as<ast::appl>(r.first);
  const auto& inner_app = as<ast::appl>(*outer_app.func);
  const auto& outer = as<ast::abst>(*inner_app.func);
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
  auto r = parse_ok("(if true then \\x : int . x else \\x : int . 0) 1");
  const auto& a = as<ast::appl>(r.first);
  const auto& ie = as<ast::ifexpr>(*a.func);
  EXPECT_TRUE(is<ast::abst>(*ie.then));
  EXPECT_TRUE(is<ast::abst>(*ie.els));
}

TEST(parser_tests, if_in_lambda_body) {
  auto r = parse_ok("(\\x : int . if x = 0 then 1 else 0) 1");
  const auto& a = as<ast::appl>(r.first);
  const auto& ab = as<ast::abst>(*a.func);
  EXPECT_TRUE(is<ast::ifexpr>(*ab.body));
}

TEST(parser_tests, complex_nesting) {
  auto r = parse_ok(
      "(\\x : int . \\f : int -> int . \\g : int -> int . \\y : int . if x = 0 then f x + 1 else g y * 2) 1 "
      "(\\a : int . a) (\\a : int . a) 5");
  const auto& app1 = as<ast::appl>(r.first);
  const auto& app2 = as<ast::appl>(*app1.func);
  const auto& app3 = as<ast::appl>(*app2.func);
  const auto& app4 = as<ast::appl>(*app3.func);
  const auto& a0 = as<ast::abst>(*app4.func);
  const auto& a1 = as<ast::abst>(*a0.body);
  const auto& a2 = as<ast::abst>(*a1.body);
  const auto& a3 = as<ast::abst>(*a2.body);
  const auto& ie = as<ast::ifexpr>(*a3.body);
  EXPECT_TRUE(is<ast::binop>(*ie.cond));
  const auto& then_b = as<ast::binop>(*ie.then);
  EXPECT_TRUE(std::holds_alternative<tk::op_plus>(then_b.op));
  const auto& else_b = as<ast::binop>(*ie.els);
  EXPECT_TRUE(std::holds_alternative<tk::op_mul>(else_b.op));
}

// -- fix ----------------------------------------------------------------------

TEST(parser_tests, fix_simple) {
  auto r = parse_ok("(fix (\\f : int -> int . \\n : int . n + 1)) 5");
  const auto& a = as<ast::appl>(r.first);
  const auto& fx = as<ast::fix>(*a.func);
  const auto& ab = as<ast::abst>(*fx.body);
  EXPECT_TRUE(std::holds_alternative<ast::type_arrow>(ab.param_type));
  EXPECT_TRUE(is<ast::abst>(*ab.body));
}

TEST(parser_tests, fix_application_argument) {
  // the applied argument binds the generator's second lambda parameter
  auto r = parse_ok("(fix (\\f : int -> int . \\n : int . f n)) 5");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_EQ(as<ast::li_int>(*a.arg).value, 5);
}

TEST(parser_tests, fix_in_let) {
  auto r = parse_ok("let fact : int -> int = fix (\\f : int -> int . \\n : int . n * f (n - 1)) in fact 5");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_TRUE(is<ast::fix>(*a.arg));
}

TEST(parser_tests, strict_app_known_wrong) {
  // known non-arrow in function position must be rejected
  EXPECT_THROW(parse("(\\f : int . f 5) 1"), parse_err);
  // known argument/parameter mismatch must be rejected
  EXPECT_THROW(parse("(\\f : int -> int . f true) 1"), parse_err);
  // well-typed arrow application is accepted
  parse_ok("(\\f : int -> int . f 5) (\\x : int . x + 1)");
}

// -- product types (tuples) ---------------------------------------------------

TEST(parser_tests, product_type_annotation) {
  auto r = parse_ok("let f : (int, bool) -> int = \\p : (int, bool) . 1 in 2");
  const auto& a = as<ast::appl>(r.first);
  const auto& prod = std::get<ast::type_prod>(as<ast::abst>(*a.arg).param_type);
  ASSERT_EQ(prod.fields.size(), 2);
  EXPECT_TRUE(std::holds_alternative<ast::type_int>(*prod.fields[0]));
  EXPECT_TRUE(std::holds_alternative<ast::type_bool>(*prod.fields[1]));
}

TEST(parser_tests, product_type_inside_arrow) {
  auto r = parse_ok("let id : (int, bool) -> (int, bool) = \\p : (int, bool) . p in 1");
  const auto& a = as<ast::appl>(r.first);
  const auto& arrow = std::get<ast::type_arrow>(as<ast::abst>(*a.func).param_type);
  const auto& prod = std::get<ast::type_prod>(*arrow.from);
  ASSERT_EQ(prod.fields.size(), 2);
  EXPECT_TRUE(std::holds_alternative<ast::type_int>(*prod.fields[0]));
  EXPECT_TRUE(std::holds_alternative<ast::type_bool>(*prod.fields[1]));
}

TEST(parser_tests, parenthesized_type_is_transparent) {
  auto r = parse_ok("let f : (int) -> int = \\x : int . x in 1");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_TRUE(std::holds_alternative<ast::type_int>(as<ast::abst>(*a.arg).param_type));
}

// -- tuple literals -----------------------------------------------------------

TEST(parser_tests, tuple_literal) {
  auto r = parse_ok("(1, true)");
  const auto& t = as<ast::tup>(r.first);
  ASSERT_EQ(t.fields.size(), 2);
  EXPECT_EQ(as<ast::li_int>(*t.fields[0]).value, 1);
  EXPECT_EQ(as<ast::li_bool>(*t.fields[1]).value, true);
}

TEST(parser_tests, tuple_literal_three_elements) {
  auto r = parse_ok("(1, 2, 3)");
  const auto& t = as<ast::tup>(r.first);
  ASSERT_EQ(t.fields.size(), 3);
  EXPECT_EQ(as<ast::li_int>(*t.fields[2]).value, 3);
}

TEST(parser_tests, tuple_literal_nested) {
  auto r = parse_ok("((1, 2), (true, (3, 4)))");
  const auto& t = as<ast::tup>(r.first);
  ASSERT_EQ(t.fields.size(), 2);
  EXPECT_TRUE(is<ast::tup>(*t.fields[0]));
  const auto& inner = as<ast::tup>(*t.fields[1]);
  ASSERT_EQ(inner.fields.size(), 2);
  EXPECT_EQ(as<ast::li_bool>(*inner.fields[0]).value, true);
  EXPECT_TRUE(is<ast::tup>(*inner.fields[1]));
}

TEST(parser_tests, tuple_literal_in_application) {
  auto r = parse_ok("(\\p : (int, bool) . 1) (1, true)");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_TRUE(is<ast::tup>(*a.arg));
}

TEST(parser_tests, tuple_elements_are_full_expressions) {
  auto r = parse_ok("(1 + 2, if true then 3 else 4)");
  const auto& t = as<ast::tup>(r.first);
  ASSERT_EQ(t.fields.size(), 2);
  EXPECT_TRUE(is<ast::binop>(*t.fields[0]));
  EXPECT_TRUE(is<ast::ifexpr>(*t.fields[1]));
}

TEST(parser_tests, parenthesized_expr_is_not_tuple) {
  auto r = parse_ok("(1)");
  EXPECT_TRUE(is<ast::li_int>(r.first));
}

TEST(parser_tests, unit_literal) {
  auto r = parse_ok("()");
  EXPECT_TRUE(is<ast::li_unit>(r.first));
}

TEST(parser_tests, unit_literal_in_let) {
  auto r = parse_ok("let x : () = () in x");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_TRUE(std::holds_alternative<ast::type_unit>(as<ast::abst>(*a.func).param_type));
  EXPECT_TRUE(is<ast::li_unit>(*a.arg));
}

TEST(parser_tests, let_without_annotation) {
  auto r = parse_ok("let x = 5 in x");
  const auto& a = as<ast::appl>(r.first);
  EXPECT_TRUE(std::holds_alternative<ast::type_int>(as<ast::abst>(*a.func).param_type));
}

TEST(parser_tests, structured_binding_desugar_shape) {
  auto r = parse_ok("let {a, b} = (1, true) in a");
  const auto& outer = as<ast::appl>(r.first);  // (\$t : T . ...) E
  const auto& t_lambda = as<ast::abst>(*outer.func);
  const auto& a_lambda = as<ast::appl>(*t_lambda.body);
  const auto& field0 = as<ast::field>(*a_lambda.arg);  // $t.field(0)
  EXPECT_EQ(field0.index, 0u);
  EXPECT_EQ(as<ast::var>(*field0.base).index, 0);
  const auto& b_lambda = as<ast::appl>(*as<ast::abst>(*a_lambda.func).body);
  const auto& field1 = as<ast::field>(*b_lambda.arg);  // $t.field(1)
  EXPECT_EQ(field1.index, 1u);
  EXPECT_EQ(as<ast::var>(*field1.base).index, 1);
  EXPECT_EQ(as<ast::var>(*as<ast::abst>(*b_lambda.func).body).index, 1);  // body: a
}

TEST(parser_tests, structured_binding_on_annotated_let) {
  auto r = parse_ok("let p : (int, bool) = (1, true) in let {a, b} = p in a");
  const auto& outer = as<ast::appl>(r.first);
  const auto& prod = std::get<ast::type_prod>(as<ast::abst>(*outer.func).param_type);
  ASSERT_EQ(prod.fields.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<ast::type_int>(*prod.fields[0]));
  EXPECT_TRUE(std::holds_alternative<ast::type_bool>(*prod.fields[1]));
}

TEST(parser_tests, structured_binding_on_inferred_let) {
  auto r = parse_ok("let p = (1, true) in let {a, b} = p in b");
  const auto& outer = as<ast::appl>(r.first);
  const auto& prod = std::get<ast::type_prod>(as<ast::abst>(*outer.func).param_type);
  ASSERT_EQ(prod.fields.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<ast::type_bool>(*prod.fields[1]));
}

// -- variant types ------------------------------------------------------------

TEST(parser_tests, type_decl_constructors) {
  auto r = parse_ok("type shape = Circle of int | Square | Rect of (int, int)\n1");
  // constructors become usable in expressions
  parse_ok("type shape = Circle of int | Square\nCircle 5");
  parse_ok("type shape = Circle of int | Square\nSquare");
  parse_ok("type shape = Rect of (int, int)\nRect (3, 4)");
}

TEST(parser_tests, type_decl_self_recursive) {
  parse_ok("type expr = Num of int | Add of (expr, expr)\n1");
  parse_ok("type tree = Leaf of int | Node of (tree, tree)\n1");
}

TEST(parser_tests, type_ref_annotation) {
  auto r = parse_ok("type shape = Circle of int\nlet f : shape -> int = \\s : shape . 1 in 2");
  const auto& a = as<ast::appl>(r.first);
  const auto& ref = std::get<ast::type_ref>(as<ast::abst>(*a.arg).param_type);
  EXPECT_EQ(ref.name, "shape");
}

TEST(parser_tests, ctor_nullary) {
  auto r = parse_ok("type shape = Circle | Square\nSquare");
  const auto& c = as<ast::ctor>(r.first);
  EXPECT_EQ(c.name, "Square");
  EXPECT_EQ(c.payload, nullptr);
  EXPECT_EQ(c.tag, 1u);
}

TEST(parser_tests, ctor_with_payload) {
  auto r = parse_ok("type shape = Circle of int | Square\nCircle 5");
  const auto& c = as<ast::ctor>(r.first);
  EXPECT_EQ(c.name, "Circle");
  EXPECT_EQ(c.tag, 0u);
  ASSERT_NE(c.payload, nullptr);
  EXPECT_EQ(as<ast::li_int>(*c.payload).value, 5);
}

TEST(parser_tests, ctor_with_product_payload) {
  auto r = parse_ok("type shape = Rect of (int, int)\nRect (3, 4)");
  const auto& c = as<ast::ctor>(r.first);
  EXPECT_EQ(c.name, "Rect");
  ASSERT_NE(c.payload, nullptr);
  EXPECT_TRUE(is<ast::tup>(*c.payload));
}

TEST(parser_tests, match_node_shape) {
  auto r = parse_ok(
      "type shape = Circle of int | Square\n"
      "let pick = \\s : shape . match s with Circle r . r | Square . 0 in 1");
  const auto& a = as<ast::appl>(r.first);
  const auto& body = as<ast::case_>(*as<ast::abst>(*a.arg).body);
  EXPECT_TRUE(is<ast::var>(*body.scrutinee));
  ASSERT_EQ(body.arms.size(), 2u);
  // each arm body is a closure over the payload
  EXPECT_TRUE(is<ast::abst>(*body.arms[0].body));
  EXPECT_TRUE(is<ast::abst>(*body.arms[1].body));
}

TEST(parser_tests, match_arm_order_free) {
  parse_ok(
      "type shape = Circle of int | Square\n"
      "(\\s : shape . match s with Square . 0 | Circle r . r) (Circle 5)");
}

struct parser_error_theory : ::testing::TestWithParam<std::string> {};
TEST_P(parser_error_theory, reject) { EXPECT_THROW(parse(GetParam()), parse_err); }

INSTANTIATE_TEST_SUITE_P(general, parser_error_theory,
                         ::testing::Values("", ")", "(1 + 2", "1 +", "int", "1 + true", "true = 1", "x",
                                           "\\x : int . 42"));
INSTANTIATE_TEST_SUITE_P(lambda, parser_error_theory,
                         ::testing::Values("\\", "\\int . x", "\\x int . x", "\\x : int", "\\x : int .",
                                           "\\x : if . x"));
INSTANTIATE_TEST_SUITE_P(if_expr, parser_error_theory,
                         ::testing::Values("if true 1 else 2", "if true then 1 2", "if 42 then 1 else 2",
                                           "if true then 1 else true"));

INSTANTIATE_TEST_SUITE_P(let_expr, parser_error_theory,
                         ::testing::Values("let x : int = f y in x", "let 1 = 2 in 3", "let x : int = 1 2 in x"));

INSTANTIATE_TEST_SUITE_P(fix, parser_error_theory,
                         ::testing::Values("fix 42", "fix (\\f : int . f)", "fix (\\f : int -> int . f)",
                                           "fix (\\f : int -> int . \\x : bool . 1)",
                                           "fix (\\f : int -> int . \\n : int . n + 1)"));

// -- diagnostics -------------------------------------------------------------

struct parser_diag_theory : ::testing::TestWithParam<std::pair<std::string, std::string>> {};
TEST_P(parser_diag_theory, code) {
  const auto& [src, code] = GetParam();
  try {
    parse(src);
    FAIL() << "expected a diagnostic for: " << src;
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, code);
  }
}

INSTANTIATE_TEST_SUITE_P(
    codes, parser_diag_theory,
    ::testing::Values(std::pair{"x", "C001"}, std::pair{"int", "B001"}, std::pair{"\\x : if . x", "B002"},
                      std::pair{"\\int . x", "B003"}, std::pair{"let 1 = 2 in 3", "B004"}, std::pair{"(1 + 2", "B005"},
                      std::pair{"\\x : int", "B005"}, std::pair{"1 +", "B006"}, std::pair{"\\", "B006"},
                      std::pair{"(\\f : int . f 5) 1", "C002"}, std::pair{"(\\f : int -> int . f true) 1", "C003"},
                      std::pair{"if 42 then 1 else 2", "C004"}, std::pair{"if true then 1 else true", "C005"},
                      std::pair{"fix 42", "B007"}, std::pair{"fix (\\f : int -> int . \\x : bool . 1)", "C007"},
                      std::pair{"1 + true", "C008"}, std::pair{"\\x : int -> int . x", "C009"}, std::pair{"@", "A001"},
                      std::pair{"\"unclosed", "A002"}, std::pair{"123abc", "A003"},
                      // 'type' is a keyword again: old record declarations are rejected as malformed
                      std::pair{"type 42 = {x:int}\n1", "B009"}, std::pair{"type a\n1", "B005"},
                      std::pair{"type a = 42\n1", "B009"}, std::pair{"type a = {x:int}\ntype a = {y:int}\n1", "B009"},
                      std::pair{"type a = {x:int, x:bool}\n1", "B009"},
                      // named record syntax is gone: braces and unknown type names are rejected
                      std::pair{"\\p : point . 1", "C011"}, std::pair{"\\p : {} . 1", "B002"},
                      std::pair{"\\p : {x} . 1", "B002"}, std::pair{"{}", "B001"}, std::pair{"{1}", "B001"},
                      std::pair{"{x:int = 1, x:bool = 2}", "B001"}, std::pair{"{x:int = true}", "B001"},
                      std::pair{"42.x", "B001"}, std::pair{"1 . 2", "B001"},
                      // products: arity and element types are checked positionally
                      std::pair{"(1, 2) + 1", "C008"}, std::pair{"(1, true) 2", "C002"},
                      std::pair{"(\\p : (int, bool) . 1) (1, 2)", "C003"},
                      std::pair{"let x : (int, bool) = (1, 2) in x", "C015"}, std::pair{"let {a, b} = 42 in a", "C016"},
                      std::pair{"let {a} = (1, true) in a", "C017"}, std::pair{"let {a, a} = (1, true) in a", "C018"},
                      // variants: declarations, constructors, match
                      std::pair{"type expr = Num of int | Num of bool\n1", "C019"},
                      std::pair{"type a = A\ntype b = A\n1", "C019"}, std::pair{"type a = A\ntype a = B\n1", "C010"},
                      std::pair{"\\x : foo . 1", "C011"}, std::pair{"type a = A\nlet A = 1 in A", "C025"},
                      std::pair{"type a = A of int\n(\\x : a . \\A : int . x) (A 1) 2", "C025"},
                      std::pair{"type a = A of int\nA true", "C024"}, std::pair{"type a = A of int\nA", "B006"},
                      std::pair{"type a = A\nmatch 42 with A . 1", "C021"},
                      std::pair{"type a = A of int | B\nmatch (A 1) with A n . 1", "C022"},
                      std::pair{"type a = A of int | B\nmatch (A 1) with A n . 1 | A n . 2", "C022"},
                      std::pair{"type a = A of int\nmatch (A 1) with B n . 1", "C020"},
                      std::pair{"type a = A of int\nmatch (A 1) with A (x, y) . 1", "C022"},
                      std::pair{"type a = A of int | B\nmatch (A 1) with A n . 1 | B . true", "C023"},
                      std::pair{"type a = A of int\n(\\x : int . x) (A 1)", "C003"}));

TEST(parser_tests, diag_position_multiline) {
  try {
    parse("1 +\ntrue");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C008");
    EXPECT_EQ(e.diag.loc, (src_range{4, 8}));
    EXPECT_EQ(render(e.diag, "1 +\ntrue", "t.luca"),
              "t.luca:2:1: error: operator '+' expects 'int' operands, found 'bool'\n"
              "  true\n"
              "  ^~~~\n"
              "hint: arithmetic and comparison operators require int operands\n");
  }
}

TEST(parser_tests, diag_position_eof) {
  try {
    parse("1 +");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "B006");
    EXPECT_EQ(e.diag.loc, (src_range{3, 3}));
  }
}

TEST(parser_tests, diag_position_arg_mismatch) {
  // "true" is on line 2, columns 3-6; the caret underlines it
  try {
    parse("let f : int -> int = \\x : int . x + 1 in\nf true");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C003");
    EXPECT_EQ(e.diag.loc, (src_range{43, 47}));
  }
}

}  // namespace tests
