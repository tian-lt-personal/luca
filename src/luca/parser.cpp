// luca
#include "parser.hpp"

#include "lexer.hpp"

namespace {

class parser {
 public:
  explicit parser(const std::string& source) noexcept : lex_(source), curtk_(lex_.next()), nextk_(lex_.next()) {}
  parse_result run_pass() && noexcept { std::terminate(); }

 private:
  void parse_bin_expr() {}

 private:
  lexer lex_;
  lex_result curtk_;
  lex_result nextk_;
};

}  // namespace

parse_result parse(const std::string& source) noexcept { return parser{source}.run_pass(); }
