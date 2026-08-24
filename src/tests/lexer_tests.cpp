// std
#include <string>
// gtest
#include <gtest/gtest.h>
// luca
#include <lexer.hpp>

namespace tests {

namespace {

struct test_case {
  std::string source;
  std::expected<token, lex_err> expected;
};

}  // namespace

struct lexer_theory : ::testing::TestWithParam<test_case> {};

TEST_P(lexer_theory, next) {
  const auto& p = GetParam();
  lexer l{p.source};
  auto res = l.next();
  if (p.expected.has_value()) {
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->t, *p.expected);
    auto eof = l.next();
    ASSERT_FALSE(eof.has_value());
    EXPECT_EQ(eof.error().index(), lex_err{lex_err_eof{}}.index());
  } else {
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().index(), p.expected.error().index());
  }
}

INSTANTIATE_TEST_SUITE_P(empty_source, lexer_theory,
                         ::testing::Values(test_case{.source = "", .expected = std::unexpected{lex_err_eof{}}}));
INSTANTIATE_TEST_SUITE_P(
    keyword_tokens, lexer_theory,
    ::testing::Values(
        test_case{.source = "\\", .expected = tk::kw_lambda{}},
        test_case{.source = "lambda", .expected = tk::kw_lambda{}},
        test_case{.source = "let", .expected = tk::kw_let{}}, test_case{.source = "in", .expected = tk::kw_in{}},
        test_case{.source = "if", .expected = tk::kw_if{}}, test_case{.source = "then", .expected = tk::kw_then{}},
        test_case{.source = "else", .expected = tk::kw_else{}}, test_case{.source = "bool", .expected = tk::kw_bool{}},
        test_case{.source = "true", .expected = tk::kw_true{}},
        test_case{.source = "false", .expected = tk::kw_false{}}, test_case{.source = "int", .expected = tk::kw_int{}},
        test_case{.source = "string", .expected = tk::kw_string{}},
        test_case{.source = "fix", .expected = tk::kw_fix{}}, test_case{.source = "type", .expected = tk::kw_type{}},
        test_case{.source = "of", .expected = tk::kw_of{}}, test_case{.source = "match", .expected = tk::kw_match{}},
        test_case{.source = "with", .expected = tk::kw_with{}}));
INSTANTIATE_TEST_SUITE_P(id_tokens, lexer_theory,
                         ::testing::Values(
                             // valid: single characters
                             test_case{.source = "a", .expected = tk::id{.name = "a"}},
                             test_case{.source = "Z", .expected = tk::id{.name = "Z"}},
                             test_case{.source = "_", .expected = tk::id{.name = "_"}},
                             // valid: standard alphanumeric and mixed cases
                             test_case{.source = "abc", .expected = tk::id{.name = "abc"}},
                             test_case{.source = "abc123", .expected = tk::id{.name = "abc123"}},
                             test_case{.source = "CamelCaseID", .expected = tk::id{.name = "CamelCaseID"}},
                             test_case{.source = "MACRO_CASE", .expected = tk::id{.name = "MACRO_CASE"}},
                             // valid: underscore placements
                             test_case{.source = "_private", .expected = tk::id{.name = "_private"}},
                             test_case{.source = "trailing_under_", .expected = tk::id{.name = "trailing_under_"}},
                             test_case{.source = "_0", .expected = tk::id{.name = "_0"}},
                             test_case{.source = "a__b", .expected = tk::id{.name = "a__b"}},
                             // valid: hyphen placements
                             test_case{.source = "abc-123", .expected = tk::id{.name = "abc-123"}},
                             test_case{.source = "abc-123-def", .expected = tk::id{.name = "abc-123-def"}},
                             test_case{.source = "a-b", .expected = tk::id{.name = "a-b"}},
                             test_case{.source = "x--y", .expected = tk::id{.name = "x--y"}},
                             // valid: mixed hyphens and underscores
                             test_case{.source = "a1-b2_c3", .expected = tk::id{.name = "a1-b2_c3"}},
                             test_case{.source = "_foo-bar_", .expected = tk::id{.name = "_foo-bar_"}},
                             // invalid: glued errors (starts with digit, followed by id chars)
                             test_case{.source = "123abc", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "123-abc", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "0_foo", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "42-", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "9_", .expected = std::unexpected{lex_err_glued{}}}));
INSTANTIATE_TEST_SUITE_P(literal_int_tokens, lexer_theory,
                         ::testing::Values(
                             // single digits
                             test_case{.source = "0", .expected = tk::li_int{.value = "0"}},
                             test_case{.source = "1", .expected = tk::li_int{.value = "1"}},
                             test_case{.source = "9", .expected = tk::li_int{.value = "9"}},
                             // standard multi-digit numbers
                             test_case{.source = "42", .expected = tk::li_int{.value = "42"}},
                             test_case{.source = "1024", .expected = tk::li_int{.value = "1024"}},
                             // numbers with leading zeros
                             test_case{.source = "00", .expected = tk::li_int{.value = "00"}},
                             test_case{.source = "007", .expected = tk::li_int{.value = "007"}},
                             test_case{.source = "012345", .expected = tk::li_int{.value = "012345"}},
                             // long numeric sequences
                             test_case{.source = "9876543210", .expected = tk::li_int{.value = "9876543210"}},
                             test_case{.source = "18446744073709551615",
                                       .expected = tk::li_int{.value = "18446744073709551615"}},
                             // unsupported numeric bases (e.g., hex, binary)
                             // these will trigger glued_err because "0" is matched as number,
                             // and "x" or "b" triggers the error capture.
                             test_case{.source = "0x00", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "0x1A", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "0b10", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "0o77", .expected = std::unexpected{lex_err_glued{}}},
                             // unsupported digit separators (e.g., underscores in numbers)
                             test_case{.source = "1_000", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "123_456", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "42_", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "0_", .expected = std::unexpected{lex_err_glued{}}},
                             // glued letters
                             test_case{.source = "1a", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "123abc", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "99bottles", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "0xyz", .expected = std::unexpected{lex_err_glued{}}},
                             // glued hyphens
                             test_case{.source = "456-", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "1-2", .expected = std::unexpected{lex_err_glued{}}},
                             test_case{.source = "0-", .expected = std::unexpected{lex_err_glued{}}}));
INSTANTIATE_TEST_SUITE_P(
    literal_string_tokens, lexer_theory,
    ::testing::Values(
        // valid: empty string
        test_case{.source = "\"\"", .expected = tk::li_str{.raw = ""}},
        // valid: standard alphanumeric and symbols
        test_case{.source = "\"hello\"", .expected = tk::li_str{.raw = "hello"}},
        test_case{.source = "\"hello world\"", .expected = tk::li_str{.raw = "hello world"}},
        test_case{.source = "\"123-abc_!@#\"", .expected = tk::li_str{.raw = "123-abc_!@#"}},
        test_case{.source = "\"  padding  \"", .expected = tk::li_str{.raw = "  padding  "}},
        test_case{.source = "  \"outer padding\"   ", .expected = tk::li_str{.raw = "outer padding"}},
        // valid: escaped sequences
        // lexer captures the exact raw characters, so "\\n" in source becomes "\\n" in the view.
        test_case{.source = "\"newline \\n test\"", .expected = tk::li_str{.raw = "newline \\n test"}},
        test_case{.source = "\"tab \\t test\"", .expected = tk::li_str{.raw = "tab \\t test"}},
        test_case{.source = "\"escaped \\\" quotes\"", .expected = tk::li_str{.raw = "escaped \\\" quotes"}},
        test_case{.source = "\"escaped \\\\ slash\"", .expected = tk::li_str{.raw = "escaped \\\\ slash"}},
        // invalid: unclosed strings ending at EOF
        test_case{.source = "\"", .expected = std::unexpected{lex_err_str{}}},
        test_case{.source = "\"unclosed EOF", .expected = std::unexpected{lex_err_str{}}},
        test_case{.source = "\"unclosed with \\\" quote", .expected = std::unexpected{lex_err_str{}}},
        // invalid: unclosed strings hitting a newline
        // the rule forbids raw newlines inside the quotes.
        test_case{.source = "\"unclosed\n\"", .expected = std::unexpected{lex_err_str{}}},
        test_case{.source = "\"line1\nline2\"", .expected = std::unexpected{lex_err_str{}}},
        // invalid: trailing escape character at EOF or Newline
        // prevents matching an escape sequence that eats the null-terminator.
        test_case{.source = "\"trailing escape \\", .expected = std::unexpected{lex_err_str{}}},
        test_case{.source = "\"trailing escape \\\n", .expected = std::unexpected{lex_err_str{}}}));
INSTANTIATE_TEST_SUITE_P(
    operator_tokens, lexer_theory,
    ::testing::Values(
        test_case{.source = "+", .expected = tk::op_plus{}}, test_case{.source = "-", .expected = tk::op_minus{}},
        test_case{.source = "*", .expected = tk::op_mul{}}, test_case{.source = "/", .expected = tk::op_div{}},
        test_case{.source = "=", .expected = tk::op_eq{}}, test_case{.source = "!=", .expected = tk::op_ne{}},
        test_case{.source = ">", .expected = tk::op_gt{}}, test_case{.source = "<", .expected = tk::op_lt{}},
        test_case{.source = ":", .expected = tk::op_colon{}}, test_case{.source = "->", .expected = tk::op_arrow{}},
        test_case{.source = ".", .expected = tk::op_dot{}}, test_case{.source = ",", .expected = tk::op_comma{}},
        test_case{.source = "|", .expected = tk::op_bar{}}, test_case{.source = "(", .expected = tk::lparen{}},
        test_case{.source = ")", .expected = tk::rparen{}}, test_case{.source = "{", .expected = tk::lbrace{}},
        test_case{.source = "}", .expected = tk::rbrace{}}));
INSTANTIATE_TEST_SUITE_P(
    comment_tokens, lexer_theory,
    ::testing::Values(test_case{.source = "// this is a comment", .expected = std::unexpected{lex_err_eof{}}},
                      test_case{.source = "42 // the answer", .expected = tk::li_int{.value = "42"}},
                      test_case{.source = "//", .expected = std::unexpected{lex_err_eof{}}},
                      test_case{.source = "   // indented comment", .expected = std::unexpected{lex_err_eof{}}}));
TEST(lexer_tests, multiple_tokens) {
  std::string source = "  foo_bar \n = \t 123 + \"hello\" -   42  ";
  lexer l{source};
  std::expected<token, lex_err> expected_seq[] = {
      tk::id{.name = "foo_bar"},  tk::op_eq{},    tk::li_int{.value = "123"}, tk::op_plus{},
      tk::li_str{.raw = "hello"}, tk::op_minus{}, tk::li_int{.value = "42"},  std::unexpected{lex_err_eof{}},
  };
  for (size_t i = 0; i < std::size(expected_seq); ++i) {
    auto actual = l.next();
    if (actual.has_value()) {
      ASSERT_TRUE(expected_seq[i].has_value());
      EXPECT_EQ(actual->t, *expected_seq[i]) << "mismatch at " << i;
    } else {
      ASSERT_FALSE(expected_seq[i].has_value());
      EXPECT_EQ(actual.error().index(), expected_seq[i].error().index()) << "mismatch at " << i;
    }
  }
  auto actual = l.next();
  ASSERT_FALSE(actual.has_value());
}
TEST(lexer_tests, comment_skips_to_newline) {
  std::string source = "// first line comment\n42 // inline comment\nlet\n";
  lexer l{source};
  std::expected<token, lex_err> expected_seq[] = {
      tk::li_int{.value = "42"},
      tk::kw_let{},
      std::unexpected{lex_err_eof{}},
  };
  for (size_t i = 0; i < std::size(expected_seq); ++i) {
    auto actual = l.next();
    if (actual.has_value()) {
      ASSERT_TRUE(expected_seq[i].has_value());
      EXPECT_EQ(actual->t, *expected_seq[i]) << "mismatch at " << i;
    } else {
      ASSERT_FALSE(expected_seq[i].has_value());
      EXPECT_EQ(actual.error().index(), expected_seq[i].error().index()) << "mismatch at " << i;
    }
  }
}

TEST(lexer_tests, token_spans) {
  std::string source = "  foo \n123+";
  lexer l{source};
  auto id_tok = l.next();
  ASSERT_TRUE(id_tok.has_value());
  EXPECT_EQ(id_tok->loc, (src_range{2, 5}));
  auto num = l.next();
  ASSERT_TRUE(num.has_value());
  EXPECT_EQ(num->loc, (src_range{7, 10}));
  auto plus = l.next();
  ASSERT_TRUE(plus.has_value());
  EXPECT_EQ(plus->loc, (src_range{10, 11}));
  auto eof = l.next();
  ASSERT_FALSE(eof.has_value());
  EXPECT_EQ(std::visit([](const auto& e) { return e.loc; }, eof.error()), (src_range{11, 11}));
}

TEST(lexer_tests, string_span_includes_quotes) {
  std::string source = "\"hi\"";
  lexer l{source};
  auto res = l.next();
  ASSERT_TRUE(res.has_value());
  EXPECT_EQ(res->t, token{tk::li_str{.raw = "hi"}});
  EXPECT_EQ(res->loc, (src_range{0, 4}));
}

TEST(lexer_tests, glued_error_span) {
  std::string source = "1_000";
  lexer l{source};
  auto res = l.next();
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error().index(), lex_err{lex_err_glued{}}.index());
  EXPECT_EQ(std::visit([](const auto& e) { return e.loc; }, res.error()), (src_range{0, 5}));
}

}  // namespace tests
