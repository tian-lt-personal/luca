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
#include <eval.hpp>
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
parse_result parse_file(const std::filesystem::path& p) { return parse(read(p), p.generic_string(), ""); }

parse_result parse_file_with(const std::filesystem::path& p, const std::string& stdlib_dir) {
  return parse(read(p), p.generic_string(), stdlib_dir);
}

int eval_int(const std::filesystem::path& p) {
  auto result = evaluate(parse_file(p).first, eval_strategy::runtime);
  return std::get<int>(std::get<value>(result.result));
}

int eval_int(const ast::term& term) {
  auto result = evaluate(term, eval_strategy::runtime);
  return std::get<int>(std::get<value>(result.result));
}

bool eval_unit(const ast::term& term) {
  auto result = evaluate(term, eval_strategy::runtime);
  return std::holds_alternative<std::monostate>(std::get<value>(result.result));
}

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
  EXPECT_TRUE(eval_unit(parse_file(dir / "a.luca").first));
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
  auto r = parse("type a = A type b = B let x = 1 in x", "", "");
  EXPECT_EQ(eval_int(r.first), 1);
}

// -- pass 2 on the final program ----------------------------------------------

TEST(module_tests, pass2_shakes_unused_import_binder) {
  // pass 2 runs on the final program: the unused import chain collapses
  auto dir = scratch();
  write(dir, "a.luca", "export let av = 99 in ()\n");
  auto b = write(dir, "b.luca", "import \"a.luca\" in 5\n");
  auto r = parse_file(b);
  EXPECT_EQ(dump(r.first), nlohmann::json::parse(R"({"li_int":{"value":5}})"));
  EXPECT_EQ(eval_int(r.first), 5);
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

TEST(module_tests, private_let_before_first_export_ok) {
  // a chain let before any export is private; the export may reference it
  auto dir = scratch();
  write(dir, "m.luca", "let y = 5 in export let x = y in ()\n");
  EXPECT_EQ(eval_int(write(dir, "root.luca", "import \"m.luca\" in x\n")), 5);
}

TEST(module_tests, c026_lambda_enclosed_let_binder) {
  // a let inside a lambda is not a module-chain name: still rejected
  try {
    parse("\\z:int. let y = 5 in export let x = y in ()", "", "");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C026");
  }
}

TEST(module_tests, c026_enclosing_lambda_binder) {
  try {
    parse("\\z:int. export let x = z in ()", "", "");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C026");
  }
}

TEST(module_tests, c026_nested_export_inside_export_def) {
  try {
    parse("export let a = 1 in export let b = (\\x:int. export let c = x in ()) in ()", "", "");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C026");
  }
}

TEST(module_tests, c026_export_structured_binding) {
  try {
    parse("export let {a, b} = (1, 2) in a", "", "");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C026");
  }
}

TEST(module_tests, c026_negative_export_at_module_level_ok) {
  // exports chain normally: each is inside the previous one's body, not its def
  auto r = parse("export let a = 1 in export let b = 2 in ()", "", "");
  EXPECT_TRUE(eval_unit(r.first));
}

// -- private names in the export chain (no linkage) -----------------------------

TEST(module_tests, private_let_in_chain_user_example) {
  // a plain let in the export chain is private: exports may reference it, importers cannot
  auto dir = scratch();
  write(dir, "m.luca", "export let a = 1 in let b = 2 in export let c = b + 1 in ()\n");
  EXPECT_EQ(eval_int(write(dir, "use-a.luca", "import \"m.luca\" in a\n")), 1);
  EXPECT_EQ(eval_int(write(dir, "use-c.luca", "import \"m.luca\" in c\n")), 3);
  try {
    parse_file(write(dir, "use-b.luca", "import \"m.luca\" in b\n"));
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C001");
  }
  // the module also evaluates standalone: () is the value
  EXPECT_TRUE(eval_unit(parse_file(dir / "m.luca").first));
}

TEST(module_tests, private_let_positions_preserved) {
  // lifted defs keep their indices across the private binder
  auto dir = scratch();
  write(dir, "m.luca", "export let a = 1 in let b = 2 in export let f = \\x:int. a + b + x in ()\n");
  EXPECT_EQ(eval_int(write(dir, "root.luca", "import \"m.luca\" in f 10\n")), 13);
}

TEST(module_tests, private_let_references_import) {
  auto dir = scratch();
  write(dir, "c.luca", "export let cv = 10 in ()\n");
  write(dir, "m.luca", "import \"c.luca\" in let priv = cv in export let x = priv + 1 in ()\n");
  EXPECT_EQ(eval_int(write(dir, "root.luca", "import \"m.luca\" in x\n")), 11);
  try {
    parse_file(write(dir, "bad.luca", "import \"m.luca\" in priv\n"));
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "C001");
  }
}

TEST(module_tests, importer_let_shadows_private_name) {
  // the importer's own `b` is unaffected by m's private `b`
  auto dir = scratch();
  write(dir, "m.luca", "export let a = 1 in let b = 2 in export let c = b + 1 in ()\n");
  EXPECT_EQ(eval_int(write(dir, "root.luca", "import \"m.luca\" in let b = 99 in b\n")), 99);
}

TEST(module_tests, same_private_name_in_two_tus) {
  // each TU's private `b` keeps its own binder (unique lifted names)
  auto dir = scratch();
  write(dir, "m1.luca", "export let a = 1 in let b = 2 in export let c1 = b + 1 in ()\n");
  write(dir, "m2.luca", "export let a = 3 in let b = 4 in export let c2 = b + 1 in ()\n");
  EXPECT_EQ(eval_int(write(dir, "root.luca", "import \"m1.luca\" in import \"m2.luca\" in c1 + c2\n")), 8);
}

TEST(module_tests, lambda_internal_let_not_lifted) {
  // y is inside the lambda, not on the module chain: the chain keeps exactly one binder
  auto r = parse("let a = 1 in (\\z:int. let y = 2 in z + y) a", "", "");
  auto j = dump(r.first);
  EXPECT_EQ(j["appl"]["arg"], nlohmann::json::parse(R"({"li_int":{"value":1}})"));
  EXPECT_EQ(j["appl"]["func"]["abst"]["body"]["appl"]["arg"], nlohmann::json::parse(R"({"var":{"index":0}})"));
}

// -- built-in library search (stdlib_dir) ---------------------------------------

TEST(module_tests, stdlib_fallback_resolves) {
  auto dir = scratch();
  auto lib = dir / "lib";
  write(lib, "std.luca", "export let std-abs-int = \\a:int. if a < 0 then 0 - a else a in ()\n");
  auto p = write(dir, "root.luca", "import \"std.luca\" in std-abs-int (-5)\n");
  auto result = parse_file_with(p, lib.generic_string());
  EXPECT_EQ(eval_int(result.first), 5);
}

TEST(module_tests, stdlib_importer_relative_wins) {
  auto dir = scratch();
  auto lib = dir / "lib";
  write(dir, "std.luca", "export let std-abs-int = \\a:int. a + 100 in ()\n");
  write(lib, "std.luca", "export let std-abs-int = \\a:int. if a < 0 then 0 - a else a in ()\n");
  auto p = write(dir, "root.luca", "import \"std.luca\" in std-abs-int 1\n");
  auto result = parse_file_with(p, lib.generic_string());
  EXPECT_EQ(eval_int(result.first), 101);
}

TEST(module_tests, stdlib_module_imports_sibling) {
  auto dir = scratch();
  auto lib = dir / "lib";
  write(lib, "std2.luca", "export let std-double-int = \\n:int. n + n in ()\n");
  write(lib, "std.luca",
        "import \"std2.luca\" in export let std-quad-int = \\n:int. std-double-int (std-double-int n) in ()\n");
  auto p = write(dir, "root.luca", "import \"std.luca\" in std-quad-int 3\n");
  auto result = parse_file_with(p, lib.generic_string());
  EXPECT_EQ(eval_int(result.first), 12);
}

TEST(module_tests, stdlib_self_import_cycle_b010) {
  auto dir = scratch();
  auto lib = dir / "lib";
  write(lib, "std.luca", "import \"std.luca\" in 1\n");
  auto p = write(dir, "root.luca", "import \"std.luca\" in 1\n");
  try {
    parse_file_with(p, lib.generic_string());
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "B010");
  }
}

TEST(module_tests, stdlib_missing_both_b008_hint) {
  auto dir = scratch();
  auto lib = dir / "lib";
  auto p = write(dir, "root.luca", "import \"nope.luca\" in 1\n");
  try {
    parse_file_with(p, lib.generic_string());
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "B008");
    EXPECT_NE(e.diag.hint.find("built-in library"), std::string::npos);
    EXPECT_NE(e.diag.hint.find(lib.generic_string()), std::string::npos);
  }
}

TEST(module_tests, stdlib_empty_path_b008) {
  auto dir = scratch();
  auto lib = dir / "lib";
  auto p = write(dir, "root.luca", "import \"\" in 1\n");
  try {
    parse_file_with(p, lib.generic_string());
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "B008");
  }
}

TEST(module_tests, stdlib_shipped_module_parses) {
  // pin the shipped built-in module: it must stay a parseable program evaluating to ()
  auto p = std::filesystem::path{__FILE__}.parent_path() / ".." / "stdlib" / "std.luca";
  if (!std::filesystem::is_regular_file(p)) GTEST_SKIP() << "src/stdlib/std.luca not found";
  EXPECT_TRUE(eval_unit(parse_file(p).first));
}

TEST(module_tests, c027_export_body_must_be_unit) {
  try {
    parse("export let x = 1 in 5", "", "");
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
    parse("import \"nope.luca\" in 1", "", "");
    FAIL() << "expected a diagnostic";
  } catch (const parse_err& e) {
    EXPECT_EQ(e.diag.code, "B008");
    EXPECT_TRUE(e.src.empty());
  }
}

}  // namespace tests
