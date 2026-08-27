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

// -- product types (tuples) ---------------------------------------------------

TEST(machine_tests, tuple_literal_value) {
  auto v = eval_ok("(1, true)");  // hold the result: the arena keeps the value alive
  auto* tv = std::get<tuple_value*>(v.v);
  ASSERT_EQ(tv->fields.size(), 2u);
  EXPECT_EQ(std::get<int>(tv->fields[0]), 1);
  EXPECT_EQ(std::get<bool>(tv->fields[1]), true);
}

TEST(machine_tests, tuple_three_elements) {
  auto v = eval_ok("(1, 2, 3)");
  auto* tv = std::get<tuple_value*>(v.v);
  ASSERT_EQ(tv->fields.size(), 3u);
  EXPECT_EQ(std::get<int>(tv->fields[2]), 3);
}

TEST(machine_tests, tuple_nested) {
  auto v = eval_ok("((1, 2), (true, 3))");
  auto* tv = std::get<tuple_value*>(v.v);
  ASSERT_EQ(tv->fields.size(), 2u);
  auto* inner = std::get<tuple_value*>(tv->fields[0]);
  EXPECT_EQ(std::get<int>(inner->fields[0]), 1);
  EXPECT_EQ(std::get<int>(inner->fields[1]), 2);
}

TEST(machine_tests, tuple_as_argument) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\p : (int, int) . let {a, b} = p in a + b) (1, 2)").v), 3);
}

TEST(machine_tests, tuple_as_function_result) {
  // a function returns a product; the caller destructures it
  EXPECT_EQ(std::get<int>(eval_ok("let f : (int, int) -> (int, int) = "
                                  "\\p : (int, int) . let {a, b} = p in (a + 1, b - 1) in "
                                  "let {x, y} = f (1, 2) in x * y")
                              .v),
            2);
}

TEST(machine_tests, tuple_with_function_element) {
  // a closure inside a tuple must stay alive after eval returns
  auto v = eval_ok("(\\x : int . x + 1, 5)");
  auto* tv = std::get<tuple_value*>(v.v);
  ASSERT_EQ(tv->fields.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<closure*>(tv->fields[0]));
  EXPECT_EQ(std::get<int>(tv->fields[1]), 5);
}

TEST(machine_tests, tuple_elements_evaluate) {
  EXPECT_EQ(std::get<int>(eval_ok("let {a, b} = (1 + 1, 2 * 3) in a * b").v), 12);
}

TEST(machine_tests, unit_literal_value) {
  auto v = eval_ok("()");
  EXPECT_TRUE(std::holds_alternative<std::monostate>(v.v));
}

TEST(machine_tests, unit_in_let) {
  EXPECT_TRUE(std::holds_alternative<std::monostate>(eval_ok("let x : () = () in x").v));
}

// -- structured binding and annotated let --------------------------------------

TEST(machine_tests, structured_binding) { EXPECT_EQ(std::get<bool>(eval_ok("let {a, b} = (1, true) in b").v), true); }

TEST(machine_tests, structured_binding_multi_field_use) {
  EXPECT_EQ(std::get<int>(eval_ok("let {a, b} = (1, true) in a + (if b then 1 else 0)").v), 2);
}

TEST(machine_tests, structured_binding_three_elements) {
  EXPECT_EQ(std::get<int>(eval_ok("let {a, b, c} = (1, 2, 3) in a + b + c").v), 6);
}

TEST(machine_tests, structured_binding_nested) {
  EXPECT_EQ(std::get<int>(eval_ok("let {a, b} = (1, 2) in let {c, d} = (3, 4) in a + c").v), 4);
}

TEST(machine_tests, structured_binding_on_annotated_let) {
  EXPECT_EQ(std::get<int>(eval_ok("let p : (int, bool) = (1, true) in let {a, b} = p in a + (if b then 1 else 0)").v),
            2);
}

TEST(machine_tests, annotated_let_primitive) { EXPECT_EQ(std::get<int>(eval_ok("let x:int = 5 in x").v), 5); }

TEST(machine_tests, annotated_let_function) {
  EXPECT_EQ(std::get<int>(eval_ok("let f : int -> int = \\x : int . x + 1 in f 5").v), 6);
}

TEST(machine_tests, annotated_let_product) {
  EXPECT_EQ(std::get<int>(eval_ok("let p : (int, int) = (1, 2) in let {a, b} = p in a * 10 + b").v), 12);
}

// -- inference: optional annotations ------------------------------------------

TEST(machine_tests, let_without_annotation) { EXPECT_EQ(std::get<int>(eval_ok("let x = 5 in x + 1").v), 6); }

TEST(machine_tests, let_without_annotation_function) {
  EXPECT_EQ(std::get<int>(eval_ok("let f = \\x : int . x + 1 in f 5").v), 6);
}

TEST(machine_tests, let_inferred_if) { EXPECT_EQ(std::get<int>(eval_ok("let x = if true then 1 else 2 in x").v), 1); }

TEST(machine_tests, let_product_inferred) {
  EXPECT_EQ(std::get<int>(eval_ok("let p = (1, true) in let {a, b} = p in if b then a else 0").v), 1);
}

TEST(machine_tests, structured_binding_on_inferred_literal) {
  EXPECT_EQ(std::get<int>(eval_ok("let {a, b} = (1, 2) in a + b").v), 3);
}

TEST(machine_tests, nested_tuple_inferred) {
  auto v = eval_ok("((1, true), 2)");
  auto* tv = std::get<tuple_value*>(v.v);
  ASSERT_EQ(tv->fields.size(), 2u);
  auto* inner = std::get<tuple_value*>(tv->fields[0]);
  ASSERT_EQ(inner->fields.size(), 2u);
  EXPECT_EQ(std::get<int>(inner->fields[0]), 1);
  EXPECT_EQ(std::get<bool>(inner->fields[1]), true);
}

// -- variant types ------------------------------------------------------------

TEST(machine_tests, variant_expr_eval) {
  // the canonical recursive variant: an expression AST evaluated with fix
  EXPECT_EQ(std::get<int>(eval_ok("type expr = Num of int | Add of (expr, expr)\n"
                                  "let eval = fix (\\eval : expr -> int . \\e : expr . match e with "
                                  "Num n . n | Add (l, r) . eval l + eval r) in "
                                  "eval (Add (Num 1, Add (Num 2, Num 3)))")
                              .v),
            6);
}

TEST(machine_tests, variant_nullary) {
  EXPECT_EQ(std::get<int>(eval_ok("type shape = Circle | Square\n"
                                  "let pick = \\s : shape . match s with Circle . 1 | Square . 2 in "
                                  "pick Square")
                              .v),
            2);
}

TEST(machine_tests, variant_single_payload) {
  EXPECT_EQ(std::get<int>(eval_ok("type box = Box of int\n"
                                  "let get = \\b : box . match b with Box n . n in "
                                  "get (Box 42)")
                              .v),
            42);
}

TEST(machine_tests, variant_arm_order_free) {
  EXPECT_EQ(std::get<int>(eval_ok("type shape = Circle of int | Square\n"
                                  "let pick = \\s : shape . match s with Square . 0 | Circle r . r * r in "
                                  "pick (Circle 3)")
                              .v),
            9);
}

TEST(machine_tests, variant_tree_depth) {
  EXPECT_EQ(std::get<int>(eval_ok("type tree = Leaf of int | Node of (tree, tree)\n"
                                  "let depth = fix (\\depth : tree -> int . \\t : tree . match t with "
                                  "Leaf n . 1 | Node (l, r) . 1 + (if depth l > depth r then depth l else depth r)) in "
                                  "depth (Node (Leaf 1, Node (Leaf 2, Leaf 3)))")
                              .v),
            3);
}

TEST(machine_tests, variant_in_tuple) {
  auto v = eval_ok("type box = Box of int\n(Box 5, 6)");
  auto* tv = std::get<tuple_value*>(v.v);
  auto* sv = std::get<sum_value*>(tv->fields[0]);
  EXPECT_EQ(sv->name, "Box");
  EXPECT_EQ(std::get<int>(sv->payload), 5);
  EXPECT_EQ(std::get<int>(tv->fields[1]), 6);
}

TEST(machine_tests, variant_function_payload) {
  auto v = eval_ok("type fn = Fn of (int -> int)\nFn (\\x : int . x + 1)");
  auto* sv = std::get<sum_value*>(v.v);
  EXPECT_EQ(sv->name, "Fn");
  EXPECT_TRUE(std::holds_alternative<closure*>(sv->payload));
}

TEST(machine_tests, variant_match_of_match) {
  // the outer pattern binds a variant; an inner match consumes it
  EXPECT_EQ(std::get<int>(eval_ok("type box = Box of int\n"
                                  "type pair = Pair of (box, int)\n"
                                  "let get = \\p : pair . match p with Pair (b, n) . "
                                  "(match b with Box m . m + n) in "
                                  "get (Pair (Box 3, 4))")
                              .v),
            7);
}

// -- pass-2 optimizer regression ----------------------------------------------
// parse() runs the optimizer after building the AST; these tests pin that it
// preserves evaluation semantics while tree-shaking unused bindings (with
// de Bruijn renumbering) and folding constants.

TEST(machine_tests, opt_unused_let) { EXPECT_EQ(std::get<int>(eval_ok("let x : int = 42 in 7").v), 7); }

TEST(machine_tests, opt_unused_let_chain) {
  EXPECT_EQ(std::get<int>(eval_ok("let x : int = 1 in let y : int = 2 in 3").v), 3);
}

TEST(machine_tests, opt_shake_outer_keep_inner) {
  // \y is dropped; the free x is renumbered from index 1 to 0
  EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . (\\y : int . x) 7) 5").v), 5);
}

TEST(machine_tests, opt_shake_shadowed) {
  // the outer \x is shadowed and unused; the inner \x survives
  EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . (\\x : int . x) 2) 1").v), 2);
}

TEST(machine_tests, opt_deep_shadowing_no_shake) {
  EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . \\x : int . x) 1 2").v), 2);
}

TEST(machine_tests, opt_shake_through_match_arm) {
  // dropping \y renumbers x inside the arm closure (index 2 -> 1)
  EXPECT_EQ(std::get<int>(eval_ok("type box = Box of int\n"
                                  "(\\x : int . (\\y : int . match (Box 7) with Box n . n + x) 3) 5")
                              .v),
            12);
}

TEST(machine_tests, opt_shake_through_fix) {
  // dropping \y renumbers x inside the fix body (index 3 -> 2)
  EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . (\\y : int . (fix (\\f : int -> int . \\n : int . n + x)) 7) 3) 5").v),
            12);
}

TEST(machine_tests, opt_structured_binding_partial_drop) {
  // only b is used; the a/c binders (and their field projections) are shaken
  EXPECT_EQ(std::get<int>(eval_ok("let {a, b, c} = (1, 2, 3) in b").v), 2);
}

TEST(machine_tests, opt_structured_binding_all_dropped) {
  EXPECT_EQ(std::get<int>(eval_ok("let {a, b} = (1, 2) in 5").v), 5);
}

TEST(machine_tests, opt_match_arm_unused_payload) {
  // the payload closure stays (match arms are exempt from shaking)
  EXPECT_EQ(std::get<int>(eval_ok("type box = Box of int\nmatch (Box 7) with Box n . 3").v), 3);
}

TEST(machine_tests, opt_fold_arith_chain) { EXPECT_EQ(std::get<int>(eval_ok("1 + 2 * 3 - 4").v), 3); }

TEST(machine_tests, opt_fold_cmp_feeds_if) { EXPECT_EQ(std::get<int>(eval_ok("if 1 < 2 then 10 else 20").v), 10); }

TEST(machine_tests, opt_fold_unary_minus_nested) { EXPECT_EQ(std::get<int>(eval_ok("- (2 + 3)").v), -5); }

TEST(machine_tests, opt_fold_arg_not_shake) {
  // the arg folds but x is used, so the binder survives
  EXPECT_EQ(std::get<int>(eval_ok("(\\x : int . x + 2) (3 + 4)").v), 9);
}

TEST(machine_tests, opt_dead_branch_lazy) {
  // the dead branch's 1 / 0 is never folded and never evaluated
  EXPECT_EQ(std::get<int>(eval_ok("if false then 1 / 0 else 5").v), 5);
}

}  // namespace tests
