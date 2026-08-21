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

TEST(machine_tests, int_literal) { EXPECT_EQ(std::get<int>(eval_ok("42").v), 42); }

TEST(machine_tests, bool_true) { EXPECT_EQ(std::get<bool>(eval_ok("true").v), true); }

TEST(machine_tests, bool_false) { EXPECT_EQ(std::get<bool>(eval_ok("false").v), false); }

// -- binary ops (arithmetic) -------------------------------------------------

struct machine_arith_theory : ::testing::TestWithParam<std::pair<std::string, int>> {};
TEST_P(machine_arith_theory, eval) { EXPECT_EQ(std::get<int>(eval_ok(GetParam().first).v), GetParam().second); }

INSTANTIATE_TEST_SUITE_P(arith, machine_arith_theory,
                         ::testing::Values(std::pair{"1 + 2", 3}, std::pair{"3 * 4", 12}, std::pair{"8 / 2", 4},
                                           std::pair{"10 - 3", 7}, std::pair{"2 + 3 * 4", 14},
                                           std::pair{"(1 + 2) * 3", 9}));

// -- binary ops (comparison) -------------------------------------------------

struct machine_cmp_theory : ::testing::TestWithParam<std::pair<std::string, bool>> {};
TEST_P(machine_cmp_theory, eval) { EXPECT_EQ(std::get<bool>(eval_ok(GetParam().first).v), GetParam().second); }

INSTANTIATE_TEST_SUITE_P(cmp, machine_cmp_theory,
                         ::testing::Values(std::pair{"1 = 1", true}, std::pair{"1 = 2", false},
                                           std::pair{"1 != 2", true}, std::pair{"1 != 1", false},
                                           std::pair{"3 > 2", true}, std::pair{"2 > 3", false},
                                           std::pair{"1 < 2", true}, std::pair{"2 < 1", false}));

// -- if expressions -----------------------------------------------------------

TEST(machine_tests, if_true_branch) { EXPECT_EQ(std::get<int>(eval_ok("if true then 1 else 2").v), 1); }
TEST(machine_tests, if_false_branch) { EXPECT_EQ(std::get<int>(eval_ok("if false then 1 else 2").v), 2); }
TEST(machine_tests, if_nested) { EXPECT_EQ(std::get<int>(eval_ok("if true then if false then 1 else 2 else 3").v), 2); }
TEST(machine_tests, if_with_comparison) { EXPECT_EQ(std::get<int>(eval_ok("if 1 < 2 then 10 else 20").v), 10); }

// -- lambda + application ----------------------------------------------------

TEST(machine_tests, identity) { EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . x) 42").v), 42); }
TEST(machine_tests, constant_fn) { EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . 99) 0").v), 99); }

TEST(machine_tests, multi_arg) { EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . \\y : int . x + y) 1 2").v), 3); }

TEST(machine_tests, multi_arg_three) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . \\y : int . \\z : int . x + y + z) 1 2 3").v), 6);
}

TEST(machine_tests, higher_order) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\f : int -> int . f 5) (\\x : int . x + 1)").v), 6);
}

TEST(machine_tests, shadowing) {
  // (\x:int. (\x:int. x) 2) 1 → inner x shadows outer → returns 2
  EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . (\\x : int . x) 2) 1").v), 2);
}

// -- let expressions ----------------------------------------------------------

TEST(machine_tests, let_simple) { EXPECT_EQ(std::get<int>(eval_ok("let x : int = 5 in x").v), 5); }
TEST(machine_tests, let_arithmetic) { EXPECT_EQ(std::get<int>(eval_ok("let x : int = 3 in x + 7").v), 10); }
TEST(machine_tests, let_nested) {
  EXPECT_EQ(std::get<int>(eval_ok("let x : int = 3 in let y : int = 4 in x * y").v), 12);
}

TEST(machine_tests, let_with_function) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\f : int -> int . f 5) (\\x : int . x + 1)").v), 6);
}

TEST(machine_tests, let_passed_through) {
  // (\f:int->int->int. (\g:int->int. g 2) (f 1)) (\x:int. \y:int. x + y)
  EXPECT_EQ(
      std::get<int>(
          eval_ok("(\\f : int -> int -> int . (\\g : int -> int . g 2) (f 1)) (\\x : int . \\y : int . x + y)").v),
      3);
}

// -- unary minus --------------------------------------------------------------

TEST(machine_tests, unary_minus) { EXPECT_EQ(std::get<int>(eval_ok("-5").v), -5); }
TEST(machine_tests, unary_minus_with_binop) { EXPECT_EQ(std::get<int>(eval_ok("-3 + 7").v), 4); }

// -- fix / recursion ----------------------------------------------------------

TEST(machine_tests, fix_factorial) {
  EXPECT_EQ(std::get<int>(eval_ok("(fix (\\f : int -> int . \\n : int . if n < 2 then 1 else n * f (n - 1))) 10").v),
            3628800);
}

TEST(machine_tests, fix_via_let) {
  EXPECT_EQ(std::get<int>(eval_ok("let fact : int -> int = "
                                  "fix (\\f : int -> int . \\n : int . if n < 2 then 1 else n * f (n - 1)) in fact 6")
                              .v),
            720);
}

TEST(machine_tests, fix_two_args) {
  // multi-parameter recursion: f stays in the captured env, n/m are lambda params
  EXPECT_EQ(std::get<int>(eval_ok("(fix (\\f : int -> int -> int . \\n : int . \\m : int . "
                                  "if n < m then f n (m - 1) else n)) 3 5")
                              .v),
            3);
}

TEST(machine_tests, fix_y_combinator) {
  // a monomorphic Y combinator over int -> int functions, expressed with fix
  EXPECT_EQ(std::get<int>(eval_ok("let y : ((int -> int) -> (int -> int)) -> (int -> int) = "
                                  "fix (\\y : ((int -> int) -> (int -> int)) -> (int -> int) . "
                                  "\\f : (int -> int) -> (int -> int) . \\n : int . f (y f) n) in "
                                  "let fact : int -> int = "
                                  "y (\\f : int -> int . \\n : int . if n < 2 then 1 else n * f (n - 1)) in fact 10")
                              .v),
            3628800);
}

// -- closure rejection at top level ------------------------------------------

TEST(machine_tests, closure_rejected_at_top_level) {
  EXPECT_THROW(parse("\\f : int -> int . \\x : int . f x"), parse_err);
}

// -- records -------------------------------------------------------------------

TEST(machine_tests, record_field_access) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\p : {x:int, y:bool} . p.x) {x:int = 5, y:bool = true}").v), 5);
}

TEST(machine_tests, record_field_chain) {
  EXPECT_EQ(
      std::get<int>(eval_ok("let f : {a:{b:int}} -> int = \\p : {a:{b:int}} . p.a.b in f {a:{b:int} = {b:int = 7}}").v),
      7);
}

TEST(machine_tests, record_field_annotation_init) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\p : {v:int} . p.v) {v:int = 42}").v), 42);
}

TEST(machine_tests, unary_minus_on_field) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\p : {x:int} . -p.x) {x:int = 3}").v), -3);
}

TEST(machine_tests, record_as_function_result) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\f : {x:int} -> {x:int} . (f {x:int = 1}).x) "
                                  "(\\p : {x:int} . {x:int = p.x + 1})")
                              .v),
            2);
}

TEST(machine_tests, record_literal_value) {
  auto v = eval_ok("{x:int = 1, y:bool = true}");  // hold the result: the arena keeps the value alive
  auto* tv = std::get<tuple_value*>(v.v);
  ASSERT_EQ(tv->fields.size(), 2u);
  EXPECT_EQ(std::get<int>(tv->fields[0]), 1);
  EXPECT_EQ(tv->names[0], "x");
  EXPECT_EQ(std::get<bool>(tv->fields[1]), true);
  EXPECT_EQ(tv->names[1], "y");
}

TEST(machine_tests, record_with_function_field) {
  // a closure inside a record must stay alive after eval returns
  auto v = eval_ok("{f:int -> int = \\x : int . x + 1}");
  auto* tv = std::get<tuple_value*>(v.v);
  ASSERT_EQ(tv->fields.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<closure*>(tv->fields[0]));
}

// -- nominal records: OCaml-style literal initialization ----------------------

TEST(machine_tests, literal_initializes_named_record) {
  EXPECT_EQ(std::get<int>(eval_ok("type point = {x:int, y:bool}\n"
                                  "(\\p : point . p.x) {x:int = 1, y:bool = true}")
                              .v),
            1);
}

TEST(machine_tests, literal_initializes_named_record_any_order) {
  EXPECT_EQ(std::get<int>(eval_ok("type point = {x:int, y:bool}\n"
                                  "(\\p : point . p.x) {y:bool = true, x:int = 1}")
                              .v),
            1);
}

TEST(machine_tests, nested_literal_lift) {
  // a literal field annotated with a declared type lifts its nested literal
  EXPECT_EQ(std::get<int>(eval_ok("type point = {x:int, y:bool}\n"
                                  "(\\r : {p:point} . r.p.x) {p:point = {y:bool = true, x:int = 1}}")
                              .v),
            1);
}

TEST(machine_tests, named_record_value_from_function) {
  // a non-literal value of the named type flows through an application
  EXPECT_EQ(std::get<int>(eval_ok("type point = {x:int, y:bool}\n"
                                  "(\\p : point . p.x) ((\\q : point . q) {x:int = 1, y:bool = true})")
                              .v),
            1);
}

// -- structured binding and annotated let --------------------------------------

TEST(machine_tests, structured_binding) {
  EXPECT_EQ(std::get<bool>(eval_ok("let {a, b} = {x:int = 1, y:bool = true} in b").v), true);
}

TEST(machine_tests, structured_binding_multi_field_use) {
  EXPECT_EQ(std::get<int>(eval_ok("let {a, b} = {x:int = 1, y:bool = true} in a + (if b then 1 else 0)").v), 2);
}

TEST(machine_tests, structured_binding_on_named_record) {
  EXPECT_EQ(std::get<int>(eval_ok("type point = {x:int, y:bool}\n"
                                  "let q:point = {x:int = 1, y:bool = true} in let {a, b} = q in a")
                              .v),
            1);
}

TEST(machine_tests, structured_binding_nested) {
  EXPECT_EQ(
      std::get<int>(
          eval_ok("let {a, b} = {x:int = 1, y:bool = true} in let {c, d} = {z:int = 5, w:bool = false} in a + c").v),
      6);
}

TEST(machine_tests, annotated_let_user_example) {
  EXPECT_EQ(std::get<int>(eval_ok("type point = {x:int, y:int}\n"
                                  "let p:point = {x:int = 1, y:int = 2} in (\\p:point . p.x + p.y) p")
                              .v),
            3);
}

TEST(machine_tests, annotated_let_primitive) { EXPECT_EQ(std::get<int>(eval_ok("let x:int = 5 in x").v), 5); }

TEST(machine_tests, annotated_let_function) {
  EXPECT_EQ(std::get<int>(eval_ok("let f : int -> int = \\x : int . x + 1 in f 5").v), 6);
}

TEST(machine_tests, annotated_let_literal_reordered) {
  EXPECT_EQ(std::get<int>(eval_ok("type point = {x:int, y:int}\nlet p:point = {y:int = 2, x:int = 1} in p.x").v), 1);
}

// -- inference: optional annotations ------------------------------------------

TEST(machine_tests, let_without_annotation) { EXPECT_EQ(std::get<int>(eval_ok("let x = 5 in x + 1").v), 6); }

TEST(machine_tests, let_without_annotation_function) {
  EXPECT_EQ(std::get<int>(eval_ok("let f = \\x : int . x + 1 in f 5").v), 6);
}

TEST(machine_tests, let_record_inferred) {
  EXPECT_EQ(std::get<bool>(eval_ok("let p = {x = 1, y = true} in p.y").v), true);
}

TEST(machine_tests, let_inferred_if) { EXPECT_EQ(std::get<int>(eval_ok("let x = if true then 1 else 2 in x").v), 1); }

TEST(machine_tests, structured_binding_on_inferred_literal) {
  EXPECT_EQ(std::get<int>(eval_ok("let {a, b} = {x = 1, y = 2} in a + b").v), 3);
}

TEST(machine_tests, literal_without_annotation_initializes_named_record) {
  EXPECT_EQ(std::get<int>(eval_ok("type point = {x:int, y:bool}\n"
                                  "(\\p : point . p.x) {y = true, x = 1}")
                              .v),
            1);
}

TEST(machine_tests, annotated_let_with_inferred_fields) {
  EXPECT_EQ(std::get<int>(eval_ok("type point = {x:int, y:int}\n"
                                  "let p:point = {x = 1, y = 2} in (\\p:point . p.x + p.y) p")
                              .v),
            3);
}

TEST(machine_tests, nested_literal_without_annotation) {
  auto v = eval_ok("{a = {b = true}}");  // hold the result: the arena keeps the value alive
  auto* tv = std::get<tuple_value*>(v.v);
  ASSERT_EQ(tv->fields.size(), 1u);
  auto* inner = std::get<tuple_value*>(tv->fields[0]);
  EXPECT_EQ(inner->names[0], "b");
  EXPECT_EQ(std::get<bool>(inner->fields[0]), true);
}

}  // namespace tests
