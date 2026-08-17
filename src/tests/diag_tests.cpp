// gtest
#include <gtest/gtest.h>
// luca
#include <diag.hpp>

namespace tests {

TEST(diag_tests, render_first_line_eof) {
  diagnostic d{{3, 3}, "B006", "unexpected end of input", "complete the expression"};
  EXPECT_EQ(render(d, "1 +", "t.luca"),
            "t.luca:1:4: error: unexpected end of input\n"
            "  1 +\n"
            "     ^\n"
            "hint: complete the expression\n");
}

TEST(diag_tests, render_second_line_caret) {
  diagnostic d{{4, 8},
               "C008",
               "operator '+' expects 'int' operands, found 'bool'",
               "arithmetic and comparison operators require int operands"};
  EXPECT_EQ(render(d, "1 +\ntrue", "t.luca"),
            "t.luca:2:1: error: operator '+' expects 'int' operands, found 'bool'\n"
            "  true\n"
            "  ^~~~\n"
            "hint: arithmetic and comparison operators require int operands\n");
}

TEST(diag_tests, render_without_filename) {
  diagnostic d{{0, 1}, "A001", "unexpected character", ""};
  EXPECT_EQ(render(d, "@"),
            "1:1: error: unexpected character\n"
            "  @\n"
            "  ^\n");
}

TEST(diag_tests, render_no_hint_line) {
  diagnostic d{{0, 2}, "B005", "expected ')', found 'if'", ""};
  EXPECT_EQ(render(d, "if"),
            "1:1: error: expected ')', found 'if'\n"
            "  if\n"
            "  ^~\n");
}

TEST(diag_tests, render_eof_after_trailing_newline) {
  diagnostic d{{4, 4}, "B006", "unexpected end of input", "complete the expression"};
  EXPECT_EQ(render(d, "1 +\n", "t.luca"),
            "t.luca:2:1: error: unexpected end of input\n"
            "  \n"
            "  ^\n"
            "hint: complete the expression\n");
}

}  // namespace tests
