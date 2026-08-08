// std
#include <cassert>
// luca
#include "lexer.hpp"
#include "mp.hpp"
#include "parser.hpp"

namespace {

using term_result = std::expected<ast::term, parse_err>;

template <class... Ts>
constexpr bool holds_one_of(const token& tok) noexcept {
  return (std::holds_alternative<Ts>(tok) || ...);
}

constexpr bool is_start_of_expr(const token& tk) noexcept {
  return holds_one_of<tk::id, tk::li_int, tk::li_str, tk::kw_lambda, tk::lparen>(tk);
}

int infix_precedence(const token& tok) noexcept {
  return std::visit(overloaded{
                        [](const tk::op_plus&) { return 10; },
                        [](const tk::op_minus&) { return 10; },
                        [](const tk::op_mul&) { return 20; },
                        [](const tk::op_div&) { return 20; },
                        [](const auto&) -> int { throw std::logic_error{"no matched precedence."}; },
                    },
                    tok);
}

class parser {
 public:
  explicit parser(const std::string& source) : lex_(source), curtok_(lex_.next()), nextok_(lex_.next()) {}
  parse_result run_pass() && {
    return parse_tem().transform([&](ast::term term) { return std::pair{term, std::move(actx_)}; });
  }

 private:
  term_result parse_tem() { return parse_expr(0); }
  term_result parse_expr(int precedence) {
    auto left = parse_prefix();
    if (!left.has_value()) return std::unexpected{left.error()};

    throw;
  }
  term_result parse_prefix() {
    return curtok_.transform_error([](lex_err lerr) -> parse_err { return lerr; })
        .and_then([this](token tok) -> term_result {
          return std::visit(overloaded{
                                [this](tk::kw_lambda) { return parse_lambda(); },
                                [this](tk::lparen) -> term_result {
                                  advance();
                                  auto inner = parse_expr(0);
                                  if (!inner.has_value()) return std::unexpected{parse_err_unknown{}};
                                  if (!expect<tk::rparen>()) return std::unexpected{parse_err_unknown{}};
                                  return inner;
                                },
                                [this](auto) -> term_result { return parse_atom(); },
                            },
                            tok);
          throw;
        });
  }
  term_result parse_lambda() { throw; }
  term_result parse_atom() { throw; }
  const token& peek() const noexcept {
    assert(curtok_.has_value());
    return *curtok_;
  }
  void advance() noexcept {
    curtok_ = nextok_;
    nextok_ = lex_.next();
  }
  template <class T>
  bool expect() noexcept {
    if (!curtok_.has_value()) return false;
    if (!std::holds_alternative<T>(*curtok_)) return false;
    advance();
    return true;
  }

 private:
  lexer lex_;
  ast::context actx_;
  lex_result curtok_;
  lex_result nextok_;
};

}  // namespace

parse_result parse(const std::string& source) { return parser{source}.run_pass(); }
