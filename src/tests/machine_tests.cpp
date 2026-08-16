// gtest
#include <gtest/gtest.h>
// luca
#include <machine.hpp>
#include <parser.hpp>

namespace tests {
namespace {

auto eval_ok(const std::string& src) {
  auto r = parse(src);
  return eval(r.first);
}

}  // namespace

// -- atoms -------------------------------------------------------------------

TEST(machine_tests, int_literal) { EXPECT_EQ(std::get<int>(eval_ok("42")), 42); }

TEST(machine_tests, bool_true) { EXPECT_EQ(std::get<bool>(eval_ok("true")), true); }

TEST(machine_tests, bool_false) { EXPECT_EQ(std::get<bool>(eval_ok("false")), false); }

// -- binary ops (arithmetic) -------------------------------------------------

struct machine_arith_theory : ::testing::TestWithParam<std::pair<std::string, int>> {};
TEST_P(machine_arith_theory, eval) { EXPECT_EQ(std::get<int>(eval_ok(GetParam().first)), GetParam().second); }

INSTANTIATE_TEST_SUITE_P(arith, machine_arith_theory,
                         ::testing::Values(std::pair{"1 + 2", 3}, std::pair{"3 * 4", 12}, std::pair{"8 / 2", 4},
                                           std::pair{"10 - 3", 7}, std::pair{"2 + 3 * 4", 14},
                                           std::pair{"(1 + 2) * 3", 9}));

// -- binary ops (comparison) -------------------------------------------------

struct machine_cmp_theory : ::testing::TestWithParam<std::pair<std::string, bool>> {};
TEST_P(machine_cmp_theory, eval) { EXPECT_EQ(std::get<bool>(eval_ok(GetParam().first)), GetParam().second); }

INSTANTIATE_TEST_SUITE_P(cmp, machine_cmp_theory,
                         ::testing::Values(std::pair{"1 = 1", true}, std::pair{"1 = 2", false},
                                           std::pair{"1 != 2", true}, std::pair{"1 != 1", false},
                                           std::pair{"3 > 2", true}, std::pair{"2 > 3", false},
                                           std::pair{"1 < 2", true}, std::pair{"2 < 1", false}));

// -- if expressions -----------------------------------------------------------

TEST(machine_tests, if_true_branch) { EXPECT_EQ(std::get<int>(eval_ok("if true then 1 else 2")), 1); }
TEST(machine_tests, if_false_branch) { EXPECT_EQ(std::get<int>(eval_ok("if false then 1 else 2")), 2); }
TEST(machine_tests, if_nested) { EXPECT_EQ(std::get<int>(eval_ok("if true then if false then 1 else 2 else 3")), 2); }
TEST(machine_tests, if_with_comparison) { EXPECT_EQ(std::get<int>(eval_ok("if 1 < 2 then 10 else 20")), 10); }

// -- lambda + application ----------------------------------------------------

TEST(machine_tests, identity) { EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . x) 42")), 42); }
TEST(machine_tests, constant_fn) { EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . 99) 0")), 99); }

TEST(machine_tests, multi_arg) { EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . \\y : int . x + y) 1 2")), 3); }

TEST(machine_tests, multi_arg_three) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . \\y : int . \\z : int . x + y + z) 1 2 3")), 6);
}

TEST(machine_tests, higher_order) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\f : int -> int . f 5) (\\x : int . x + 1)")), 6);
}

TEST(machine_tests, shadowing) {
  // (\x:int. (\x:int. x) 2) 1 → inner x shadows outer → returns 2
  EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . (\\x : int . x) 2) 1")), 2);
}

// -- let expressions ----------------------------------------------------------

TEST(machine_tests, let_simple) { EXPECT_EQ(std::get<int>(eval_ok("let x = 5 in x")), 5); }
TEST(machine_tests, let_arithmetic) { EXPECT_EQ(std::get<int>(eval_ok("let x = 3 in x + 7")), 10); }
TEST(machine_tests, let_nested) { EXPECT_EQ(std::get<int>(eval_ok("let x = 3 in let y = 4 in x * y")), 12); }

TEST(machine_tests, let_with_function) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\f : int -> int . f 5) (\\x : int . x + 1)")), 6);
}

TEST(machine_tests, let_passed_through) {
  // (\f:int->int->int. (\g:int->int. g 2) (f 1)) (\x:int. \y:int. x + y)
  EXPECT_EQ(std::get<int>(
                eval_ok("(\\f : int -> int -> int . (\\g : int -> int . g 2) (f 1)) (\\x : int . \\y : int . x + y)")),
            3);
}

// -- unary minus --------------------------------------------------------------

TEST(machine_tests, unary_minus) { EXPECT_EQ(std::get<int>(eval_ok("-5")), -5); }
TEST(machine_tests, unary_minus_with_binop) { EXPECT_EQ(std::get<int>(eval_ok("-3 + 7")), 4); }

// -- fix / recursion ----------------------------------------------------------

TEST(machine_tests, fix_factorial) {
  EXPECT_EQ(std::get<int>(eval_ok("(fix (\\f : int -> int . \\n : int . if n < 2 then 1 else n * f (n - 1))) 10")),
            3628800);
}

TEST(machine_tests, fix_via_let) {
  EXPECT_EQ(std::get<int>(eval_ok(
                "let fact = fix (\\f : int -> int . \\n : int . if n < 2 then 1 else n * f (n - 1)) in fact 6")),
            720);
}

TEST(machine_tests, fix_two_args) {
  // multi-parameter recursion: f stays in the captured env, n/m are lambda params
  EXPECT_EQ(std::get<int>(eval_ok("(fix (\\f : int -> int -> int . \\n : int . \\m : int . "
                                  "if n < m then f n (m - 1) else n)) 3 5")),
            3);
}

TEST(machine_tests, fix_y_combinator) {
  // a monomorphic Y combinator over int -> int functions, expressed with fix
  EXPECT_EQ(std::get<int>(
                eval_ok("let y = fix (\\y : ((int -> int) -> (int -> int)) -> (int -> int) . "
                        "\\f : (int -> int) -> (int -> int) . \\n : int . f (y f) n) in "
                        "let fact = y (\\f : int -> int . \\n : int . if n < 2 then 1 else n * f (n - 1)) in fact 10")),
            3628800);
}

// -- closure rejection at top level ------------------------------------------

TEST(machine_tests, closure_rejected_at_top_level) {
  EXPECT_THROW(parse("\\f : int -> int . \\x : int . f x"), parse_err);
}

}  // namespace tests
