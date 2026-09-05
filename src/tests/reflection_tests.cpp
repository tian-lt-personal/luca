// std
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
// 3rd-party
#include <nlohmann/json.hpp>
// gtest
#include <gtest/gtest.h>
// luca
#include <astdump.hpp>
#include <machine.hpp>
#include <parser.hpp>

namespace tests {

namespace {

// the shipped built-in library (src/stdlib): std.luca + std-ast.luca
const std::string k_stdlib_dir =
    (std::filesystem::path{__FILE__}.parent_path() / ".." / "stdlib").lexically_normal().generic_string();

// imports read real files: each import-based test gets its own scratch dir
std::filesystem::path scratch() {
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  auto dir = std::filesystem::temp_directory_path() / "luca_reflection_tests" / info->name();
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir);
  return dir;
}

std::filesystem::path write(const std::filesystem::path& dir, const std::string& name, const std::string& content) {
  auto p = dir / name;
  std::filesystem::create_directories(p.parent_path());
  std::ofstream{p} << content;
  return p;
}

std::string read(const std::filesystem::path& p) {
  std::ifstream ifs{p};
  return {std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};
}

parse_result parse_src(const std::string& src) { return parse(src, "", k_stdlib_dir); }

int eval_int_src(const std::string& src) { return std::get<int>(eval(parse_src(src).first).v); }

void expect_err(const std::string& src, const std::string& code) {
  try {
    parse_src(src);
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, code);
  }
}

}  // namespace

// -- evaluation: quote -> splice ----------------------------------------------

TEST(reflection_tests, quote_block_splices_and_runs) {
  EXPECT_EQ(eval_int_src("import \"std.luca\" in\n"
                         "let a = ${\\x:int. x * 2} in\n"
                         "let r = [| a |] 21 in\n"
                         "r\n"),
            42);
}

TEST(reflection_tests, quote_of_binding_captures_its_def) {
  EXPECT_EQ(eval_int_src("import \"std.luca\" in\n"
                         "let foo = \\x:int. x + 1 in\n"
                         "let ast = $foo in\n"
                         "[| ast |] 41\n"),
            42);
}

TEST(reflection_tests, store_load_splice_round_trip) {
  EXPECT_EQ(eval_int_src("import \"std.luca\" in\n"
                         "let ast = ${\\x:int. x + 1} in\n"
                         "let text = std-store-program ast in\n"
                         "let prog = std-load-program text in\n"
                         "[| prog |] 41\n"),
            42);
}

TEST(reflection_tests, quote_of_a_quote_binding_unwraps) {
  EXPECT_EQ(eval_int_src("import \"std.luca\" in\n"
                         "let inner = ${\\x:int. x + 1} in\n"
                         "let outer = $inner in\n"
                         "let f = [| outer |] in\n"
                         "f 41\n"),
            42);
}

TEST(reflection_tests, spliced_code_with_if_and_comparison) {
  EXPECT_EQ(eval_int_src("import \"std.luca\" in\n"
                         "let ast = ${\\n:int. if n < 2 then 1 else n * 2} in\n"
                         "let f = [| ast |] in\n"
                         "f 7\n"),
            14);
}

TEST(reflection_tests, spliced_fix_recursion) {
  EXPECT_EQ(eval_int_src("import \"std.luca\" in\n"
                         "let ast = ${\\f:int -> int. \\n:int. if n < 2 then 1 else n * f (n - 1)} in\n"
                         "let fact = fix [| ast |] in\n"
                         "fact 6\n"),
            720);
}

TEST(reflection_tests, spliced_match_single_name_and_nullary_arms) {
  EXPECT_EQ(eval_int_src("type shape = Circle of int | Square\n"
                         "import \"std.luca\" in\n"
                         "let ast = ${\\s:shape. match s with Circle r . r * r | Square . 100} in\n"
                         "let f = [| ast |] in\n"
                         "let {a1, a2} = (f (Circle 3), f Square) in\n"
                         "a1 + a2\n"),
            109);
}

TEST(reflection_tests, match_round_trips_through_store_and_load) {
  EXPECT_EQ(eval_int_src("type shape = Circle of int | Square\n"
                         "import \"std.luca\" in\n"
                         "let ast = ${\\s:shape. match s with Circle r . r * 2 | Square . 7} in\n"
                         "let prog = std-load-program (std-store-program ast) in\n"
                         "let f = [| prog |] in\n"
                         "let {a1, a2} = (f (Circle 5), f Square) in\n"
                         "a1 + a2\n"),
            17);
}

TEST(reflection_tests, splices_reused_and_chained) {
  EXPECT_EQ(eval_int_src("import \"std.luca\" in\n"
                         "let a = ${\\x:int. x + 1} in\n"
                         "let prog = std-load-program (std-store-program a) in\n"
                         "let f = [| prog |] in\n"
                         "let {r1, r2} = (f 5, f 6) in\n"
                         "r1 + r2\n"),
            13);
}

// -- dump fidelity: capture -> store -> load -> splice reparses to the same tree

TEST(reflection_tests, splice_tree_equals_direct_parse) {
  // the fragment reparse must rebuild the identical tree: compare -d output of
  // the spliced program against a program written with the same term inline
  struct case_ {
    const char* captured;  // a closed lambda, quoted then spliced and applied
    const char* applied;
  };
  const case_ terms[] = {
      {"\\x:int. x", "5"},
      {"\\x:int. x * 2", "5"},
      {"\\x:int. (x + 1) * x", "5"},
      {"\\x:int. x + (1 + x)", "5"},
      {"\\x:int. x + (x * 3)", "5"},
      {"\\x:int. (x + 1) + x", "5"},
      {"\\x:int. x * (2 + x)", "5"},
      {"\\x:int. \\y:int. x", "1 2"},  // curried: the outer call leaves a lambda
      {"\\p:(int, int). p", "(1, 2)"},
      {"\\f:int -> int. f 5", "(\\y:int. y + 1)"},
  };
  for (const auto& t : terms) {
    std::string captured = t.captured;
    std::string applied = t.applied;
    auto spliced = parse_src("import \"std.luca\" in\n"
                             "let a = ${" + captured + "} in\n"
                             "let prog = std-load-program (std-store-program a) in\n"
                             "[| prog |] " + applied + "\n");
    auto direct = parse_src("import \"std.luca\" in\n"
                            "(" + captured + ") " + applied + "\n");
    EXPECT_EQ(dump(spliced.first), dump(direct.first)) << "term: " << captured << " applied " << applied;
  }
}

TEST(reflection_tests, comparison_operator_splices_directly) {
  // comparisons do not round-trip through std-store-program ("op": null), but a
  // direct quote -> splice keeps the token and must reparse identically
  const std::string captured = "\\x:int. if 2 < x then x + 1 else x - 1";
  auto spliced = parse_src("import \"std.luca\" in\n"
                           "let a = ${" + captured + "} in\n"
                           "[| a |] 5\n");
  auto direct = parse_src("import \"std.luca\" in\n"
                          "(" + captured + ") 5\n");
  EXPECT_EQ(dump(spliced.first), dump(direct.first));
}

// -- the drafted sample --------------------------------------------------------

TEST(reflection_tests, sample_reflection_luca_evaluates) {
  auto p = std::filesystem::path{__FILE__}.parent_path() / ".." / "sample" / "reflection.luca";
  auto r = parse(read(p), p.generic_string(), k_stdlib_dir);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(eval(r.first).v));
}

// -- a module can export a quote for importers to splice -----------------------

TEST(reflection_tests, module_exports_a_quote) {
  auto dir = scratch();
  write(dir, "m.luca",
        "import \"std.luca\" in\n"
        "let ast = ${\\x:int. x + 3} in\n"
        "export let m-ast = ast in ()\n");
  auto main = write(dir, "main.luca",
                    "import \"std.luca\" in\n"
                    "import \"m.luca\" in\n"
                    "let f = [| m-ast |] in\n"
                    "f 39\n");
  EXPECT_EQ(std::get<int>(eval(parse(read(main), main.generic_string(), k_stdlib_dir).first).v), 42);
}

// -- diagnostics ---------------------------------------------------------------

TEST(reflection_tests, c028_reflection_requires_std_term) {
  expect_err("let a = ${\\x:int. x} in ()\n", "C028");
  expect_err("std-store-program 1\n", "C028");
}

TEST(reflection_tests, c029_compile_time_values_cannot_run) {
  // quote used as runtime data
  expect_err("import \"std.luca\" in\nlet a = ${\\x:int. x} in (a, 1)\n", "C029");
  // intrinsic applied outside a let def
  expect_err("import \"std.luca\" in\nlet a = ${\\x:int. x} in std-store-program a\n", "C029");
  // static string used as the program result
  expect_err("import \"std.luca\" in\n"
             "let a = ${\\x:int. x} in\n"
             "let s = std-store-program a in\n"
             "s\n",
             "C029");
}

TEST(reflection_tests, c030_capture_target_must_be_a_binding) {
  // a lambda parameter has no def to capture
  expect_err("import \"std.luca\" in\nlet f = \\x:int. let a = $x in () in\nf 1\n", "C030");
  // a structured-binding name is a projection, not a def
  expect_err("import \"std.luca\" in\n"
             "let {p, q} = (1, 2) in\n"
             "let a = $p in ()\n",
             "C030");
}

TEST(reflection_tests, c031_reflected_code_must_be_closed) {
  // the quote refers to an enclosing binder
  expect_err("import \"std.luca\" in\n"
             "let f = \\y:int. ${\\x:int. x + y} in ()\n",
             "C031");
}

TEST(reflection_tests, c032_splice_operand_is_not_a_program) {
  // a runtime-constructed std-term value cannot be evaluated at compile time
  expect_err("import \"std.luca\" in\n"
             "let c = std-var (std-var-index 3) in\n"
             "[| c |]\n",
             "C032");
}

TEST(reflection_tests, c033_unrepresentable_terms) {
  // comparison operators serialize as "op": null
  expect_err("import \"std.luca\" in\n"
             "let a = ${\\x:int. x < 3} in\n"
             "let s = std-store-program a in ()\n",
             "C033");
  // multi-name payload patterns desugar to field projections (no surface syntax)
  expect_err("type shape = Rect of (int, int)\n"
             "import \"std.luca\" in\n"
             "let a = ${\\s:shape. match s with Rect (w, h). w} in\n"
             "[| a |]\n",
             "C033");
}

TEST(reflection_tests, c034_mixed_reflection_and_runtime_code) {
  // a static value captured inside a runtime lambda
  expect_err("import \"std.luca\" in\n"
             "let a = ${\\x:int. x} in\n"
             "let f = \\u:std-term. a in ()\n",
             "C034");
  // a quote smuggled inside a tuple in a def
  expect_err("import \"std.luca\" in\n"
             "let p = (${\\x:int. x}, 1) in ()\n",
             "C034");
}

TEST(reflection_tests, user_binding_shadows_an_intrinsic) {
  EXPECT_EQ(eval_int_src("import \"std.luca\" in\n"
                         "let std-store-program = \\x:int. x + 1 in\n"
                         "std-store-program 5\n"),
            6);
}

}  // namespace tests
