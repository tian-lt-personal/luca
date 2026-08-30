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
#include <diag.hpp>
#include <machine.hpp>
#include <parser.hpp>

namespace tests {

namespace {

// imports read real files: each test gets its own scratch dir
std::filesystem::path scratch() {
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  auto dir = std::filesystem::temp_directory_path() / "luca_module_tests" / info->name();
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

// parse a file on disk, returning the parse result
parse_result parse_file(const std::filesystem::path& p) { return parse(read(p), p.generic_string()); }

int eval_int(const std::filesystem::path& p) { return std::get<int>(eval(parse_file(p).first).v); }

}  // namespace

// -- the spec example, end to end --------------------------------------------

TEST(module_tests, spec_example_evaluates_to_2) {
  auto dir = scratch();
  write(dir, "a.luca",
        "type foo = Num of int\n"
        "export let inc-foo = \\x:foo. match x with Num n. n+1 in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in\nlet test = Num 1 in\ninc-foo test\n");
  EXPECT_EQ(eval_int(b), 2);
  // the module also evaluates standalone: its innermost body (()) is the value
  EXPECT_TRUE(std::holds_alternative<std::monostate>(eval(parse_file(dir / "a.luca").first).v));
}

// -- lifting: how exported definitions may reference their module -------------

TEST(module_tests, export_references_earlier_export) {
  auto dir = scratch();
  write(dir, "a.luca", "export let a = 1 in export let b = a + 1 in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in b\n");
  EXPECT_EQ(eval_int(b), 2);
}

TEST(module_tests, export_references_import) {
  auto dir = scratch();
  write(dir, "c.luca", "export let cv = 10 in ()\n");
  write(dir, "a.luca", "import \"c.luca\" in export let x = cv + 1 in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in x\n");
  EXPECT_EQ(eval_int(b), 11);
}

TEST(module_tests, export_references_earlier_export_through_lambda) {
  // C026 allows an earlier export referenced from inside the def's own lambda
  auto dir = scratch();
  write(dir, "a.luca", "export let a = 1 in export let f = \\x:int. a in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in f 5\n");
  EXPECT_EQ(eval_int(b), 1);
}

TEST(module_tests, import_without_exports_types_usable) {
  auto dir = scratch();
  write(dir, "a.luca", "type t = T of int\n5\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in match (T 5) with T n . n\n");
  EXPECT_EQ(eval_int(b), 5);
}

// -- transitivity and diamonds ------------------------------------------------

TEST(module_tests, transitive_import) {
  // c's exports are visible in b: required to evaluate a's definitions
  auto dir = scratch();
  write(dir, "c.luca", "export let cv = 7 in ()\n");
  write(dir, "a.luca", "import \"c.luca\" in export let x = cv + 1 in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in cv + x\n");
  EXPECT_EQ(eval_int(b), 15);
}

TEST(module_tests, diamond_imports_types_dedupe_by_provenance) {
  // x and y both import c: its type must not conflict (same declaring module)
  auto dir = scratch();
  write(dir, "c.luca", "type t = T of int\nexport let cval = T 5 in ()\n");
  write(dir, "x.luca", "import \"c.luca\" in export let xv = cval in ()\n");
  write(dir, "y.luca", "import \"c.luca\" in export let yv = cval in ()\n");
  auto b = write(dir, "b.luca",
                 "import \"x.luca\" in import \"y.luca\" in\n"
                 "(match xv with T p . p) + (match yv with T q . q)\n");
  EXPECT_EQ(eval_int(b), 10);
}

TEST(module_tests, double_import_same_module) {
  // same module imported twice: independent parses, lifted twice (no aliasing)
  auto dir = scratch();
  write(dir, "a.luca", "export let av = 3 in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in import \"a.luca\" in av + av\n");
  EXPECT_EQ(eval_int(b), 6);
}

TEST(module_tests, nested_imports) {
  auto dir = scratch();
  write(dir, "a.luca", "export let av = 4 in ()\n");
  write(dir, "b.luca", "export let bv = 5 in ()\n");
  auto c = write(dir, "c.luca", "import \"a.luca\" in import \"b.luca\" in av + bv\n");
  EXPECT_EQ(eval_int(c), 9);
}

// -- name resolution ----------------------------------------------------------

TEST(module_tests, same_name_exports_later_wins) {
  auto dir = scratch();
  write(dir, "a.luca", "export let x = 1 in ()\n");
  write(dir, "c.luca", "export let x = 2 in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in import \"c.luca\" in x\n");
  EXPECT_EQ(eval_int(b), 2);
}

TEST(module_tests, local_let_shadows_import) {
  auto dir = scratch();
  write(dir, "a.luca", "export let x = 1 in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in let x = 9 in x\n");
  EXPECT_EQ(eval_int(b), 9);
}

TEST(module_tests, import_is_an_expression_application_argument) {
  // `f import "a" in x` parses as f applied to the whole import term
  auto dir = scratch();
  write(dir, "a.luca", "export let av = 2 in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in (\\x:int. x + 1) import \"a.luca\" in av\n");
  EXPECT_EQ(eval_int(b), 3);
}

// -- paths --------------------------------------------------------------------

TEST(module_tests, import_from_subdirectory) {
  auto dir = scratch();
  write(dir, "sub/a.luca", "export let av = 6 in ()\n");
  auto b = write(dir, "b.luca", "import \"sub/a.luca\" in av\n");
  EXPECT_EQ(eval_int(b), 6);
}

TEST(module_tests, adjacent_type_decls_no_separator) {
  // type declarations are self-delimiting: no `;` is needed between them or
  // before the term
  auto r = parse("type a = A type b = B let x = 1 in x", "");
  EXPECT_EQ(std::get<int>(eval(r.first).v), 1);
}

// -- pass 2 on the final program ----------------------------------------------

TEST(module_tests, pass2_shakes_unused_import_binder) {
  // pass 2 runs on the final program: the unused import chain collapses
  auto dir = scratch();
  write(dir, "a.luca", "export let av = 99 in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in 5\n");
  auto r = parse_file(b);
  EXPECT_EQ(dump(r.first), nlohmann::json::parse(R"({"li_int":{"value":5}})"));
  EXPECT_EQ(std::get<int>(eval(r.first).v), 5);
}

TEST(module_tests, astdump_shows_lifted_chain) {
  auto dir = scratch();
  write(dir, "a.luca",
        "type foo = Num of int\n"
        "export let inc-foo = \\x:foo. match x with Num n. n+1 in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in let test = Num 1 in inc-foo test\n");
  auto j = dump(parse_file(b).first);
  // the chain binder's type is the def's computed type (foo -> int)
  EXPECT_EQ(j["appl"]["func"]["abst"]["param_type"],
            nlohmann::json::parse(R"({"arrow":{"from":{"ref":{"name":"foo"}},"to":{"int":{}}}})"));
  // the arg is the linked definition itself, untouched by this parse
  EXPECT_EQ(j["appl"]["arg"]["abst"]["param_type"], nlohmann::json::parse(R"({"ref":{"name":"foo"}})"));
}

// -- errors -------------------------------------------------------------------

TEST(module_tests, b008_missing_file) {
  auto dir = scratch();
  auto p = write(dir, "b.luca", "import \"nope.luca\" in 1\n");
  try {
    parse_file(p);
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "B008");
  }
}

TEST(module_tests, b008_empty_path) {
  auto dir = scratch();
  auto p = write(dir, "b.luca", "import \"\" in 1\n");
  try {
    parse_file(p);
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "B008");
  }
}

TEST(module_tests, b008_path_with_spaces) {
  auto dir = scratch();
  auto p = write(dir, "b.luca", "import \"my lib.luca\" in 1\n");
  try {
    parse_file(p);
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "B008");
  }
}

TEST(module_tests, b010_two_cycle) {
  auto dir = scratch();
  write(dir, "a.luca", "import \"b.luca\" in export let x = 1 in ()\n");
  write(dir, "b.luca", "import \"a.luca\" in export let y = 1 in ()\n");
  auto p = write(dir, "root.luca", "import \"a.luca\" in 1\n");
  try {
    parse_file(p);
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "B010");
  }
}

TEST(module_tests, b010_self_cycle) {
  auto dir = scratch();
  auto p = write(dir, "b.luca", "import \"b.luca\" in 1\n");
  try {
    parse_file(p);
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "B010");
  }
}

TEST(module_tests, c010_type_clash_local_first) {
  auto dir = scratch();
  write(dir, "a.luca", "type foo = Bar\n1\n");
  auto p = write(dir, "b.luca", "type foo = Num of int\nimport \"a.luca\" in 1\n");
  try {
    parse_file(p);
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C010");
  }
}

TEST(module_tests, c010_type_clash_between_imports) {
  auto dir = scratch();
  write(dir, "a.luca", "type t = A\n1\n");
  write(dir, "c.luca", "type t = B\n1\n");
  auto p = write(dir, "b.luca", "import \"a.luca\" in import \"c.luca\" in 1\n");
  try {
    parse_file(p);
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C010");
  }
}

TEST(module_tests, c019_ctor_clash) {
  auto dir = scratch();
  write(dir, "a.luca", "type t1 = Foo | Bar\n1\n");
  write(dir, "c.luca", "type t2 = Foo | Baz\n1\n");
  auto p = write(dir, "b.luca", "import \"a.luca\" in import \"c.luca\" in 1\n");
  try {
    parse_file(p);
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C019");
  }
}

TEST(module_tests, c026_enclosing_let_binder) {
  try {
    parse("let y = 5 in export let x = y in ()", "");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C026");
  }
}

TEST(module_tests, c026_enclosing_lambda_binder) {
  try {
    parse("\\z:int. export let x = z in ()", "");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C026");
  }
}

TEST(module_tests, c026_nested_export_inside_export_def) {
  try {
    parse("export let a = 1 in export let b = (\\x:int. export let c = x in ()) in ()", "");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C026");
  }
}

TEST(module_tests, c026_export_structured_binding) {
  try {
    parse("export let {a, b} = (1, 2) in a", "");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C026");
  }
}

TEST(module_tests, c026_negative_export_at_module_level_ok) {
  // exports chain normally: each is inside the previous one's body, not its def
  auto r = parse("export let a = 1 in export let b = 2 in ()", "");
  EXPECT_TRUE(std::holds_alternative<std::monostate>(eval(r.first).v));
}

TEST(module_tests, c027_export_body_must_be_unit) {
  try {
    parse("export let x = 1 in 5", "");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C027");
  }
}

// -- diagnostics from imported files ------------------------------------------

TEST(module_tests, imported_error_renders_with_imported_file) {
  auto dir = scratch();
  write(dir, "a.luca", "type foo = Num of int\nexport let bad = \\x:foo. nosuch in ()\n");
  auto p = write(dir, "b.luca", "import \"a.luca\" in 1\n");
  try {
    parse_file(p);
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C001");
    ASSERT_FALSE(e.src.empty());
    EXPECT_EQ(e.filename, "a.luca");
    EXPECT_EQ(render(e.diag, e.src, e.filename),
              "a.luca:2:26: error: unbound identifier 'nosuch'\n"
              "  export let bad = \\x:foo. nosuch in ()\n"
              "                           ^~~~~~\n"
              "hint: bind it with a lambda parameter or a let expression\n");
  }
}

TEST(module_tests, deepest_wins_imported_error) {
  // b imports a imports c; c's error must render with c's file, not a's
  auto dir = scratch();
  write(dir, "c.luca", "export let cv = 10 in bad-name\n");
  write(dir, "a.luca", "import \"c.luca\" in export let x = cv in ()\n");
  auto p = write(dir, "b.luca", "import \"a.luca\" in x\n");
  try {
    parse_file(p);
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C001");
    ASSERT_FALSE(e.src.empty());
    EXPECT_EQ(e.filename, "c.luca");
  }
}

TEST(module_tests, import_error_in_imported_file_without_path) {
  // the top-level file's own diagnostics carry no source attachment
  try {
    parse("import \"nope.luca\" in 1", "");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "B008");
    EXPECT_TRUE(e.src.empty());
  }
}

}  // namespace tests
