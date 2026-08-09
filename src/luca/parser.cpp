// std
#include <cassert>
#include <optional>
// luca
#include "lexer.hpp"
#include "mp.hpp"
#include "parser.hpp"
#include "sema.hpp"

namespace {

using term_result = std::expected<ast::term, parse_err>;

template <class... Ts>
constexpr bool holds_one_of(const token& tok) noexcept {
  return (std::holds_alternative<Ts>(tok) || ...);
}

constexpr bool is_start_of_expr(const token& tk) noexcept {
  return holds_one_of<tk::id, tk::li_int, tk::kw_lambda, tk::kw_if, tk::kw_true, tk::kw_false, tk::lparen>(tk);
}

constexpr int prec_unary_minus = 30;
constexpr int prec_appl = 40;
std::optional<int> infix_precedence(const token& tok) noexcept {
  return std::visit(overloaded{
                        [](const tk::op_plus&) -> std::optional<int> { return 10; },
                        [](const tk::op_minus&) -> std::optional<int> { return 10; },
                        [](const tk::op_mul&) -> std::optional<int> { return 20; },
                        [](const tk::op_div&) -> std::optional<int> { return 20; },
                        [](const auto&) -> std::optional<int> { return std::nullopt; },
                    },
                    tok);
}

template <class T>
ast::term* make_term(ast::context& actx, T value) {
  std::pmr::polymorphic_allocator<ast::term> alloc{actx.arena.get()};
  return alloc.new_object<ast::term>(std::move(value));
}

class parser {
 public:
  explicit parser(const std::string& source, sema& s)
      : lex_(source),
        sema_(s),
        actx_{std::make_unique<std::pmr::monotonic_buffer_resource>()},
        curtok_(lex_.next()),
        nextok_(lex_.next()) {}
  parse_result run_pass() && {
    return parse_tem().transform([&](ast::term term) { return std::pair{term, std::move(actx_)}; });
  }

 private:
  term_result parse_tem() { return parse_expr(0); }
  term_result parse_expr(int precedence) {
    auto left = parse_prefix();
    if (!left.has_value()) return std::unexpected{left.error()};
    while (true) {
      if (!curtok_.has_value()) {
        if (std::holds_alternative<lex_err_eof>(curtok_.error())) break;
        return std::unexpected{parse_err_unknown{}};
      }
      if (auto prec = infix_precedence(peek()); prec.has_value() && *prec > precedence) {
        // handle the infix expression
        auto op = peek();
        advance();
        left = parse_infix(std::move(*left), op);
        continue;
      }
      if (is_start_of_expr(peek()) && prec_appl > precedence) {
        // handle the implicit aplication
        auto rhs = parse_expr(prec_appl);
        if (!rhs.has_value()) return std::unexpected{parse_err_unknown{}};
        left =
            ast::term{ast::appl{.func = make_term(actx_, *std::move(left)), .arg = make_term(actx_, *std::move(rhs))}};
        continue;
      }
      break;
    }
    return left;
  }
  term_result parse_prefix() {
    return curtok_.transform_error([](lex_err lerr) -> parse_err { return lerr; })
        .and_then([this](token tok) -> term_result {
          return std::visit(overloaded{
                                [this](tk::kw_lambda) { return parse_lambda(); },
                                [this](tk::kw_if) { return parse_if(); },
                                [this](tk::lparen) -> term_result {  // handle the parenthesis expression
                                  advance();
                                  auto inner = parse_expr(0);
                                  if (!inner.has_value()) return std::unexpected{parse_err_unknown{}};
                                  if (!expect<tk::rparen>()) return std::unexpected{parse_err_unknown{}};
                                  return inner;
                                },
                                [this](tk::op_minus) -> term_result {  // handle the unary minus operator
                                  advance();
                                  auto rhs = parse_expr(prec_unary_minus);
                                  if (!rhs.has_value()) return std::unexpected{parse_err_unknown{}};
                                  return ast::term{ast::binop{.op = tk::op_minus{},
                                                              .left = make_term(actx_, ast::li_int{0}),
                                                              .right = make_term(actx_, *std::move(rhs))}};
                                },
                                [this](auto) -> term_result { return parse_atom(); },
                            },
                            tok);
        });
  }
  term_result parse_atom() {
    auto tok = peek();
    advance();
    return std::visit(
        overloaded{
            [this](tk::id id) -> term_result { return ast::term{ast::var{sema_.resolve_binding_index(id.name)}}; },
            [](tk::li_int lit) -> term_result {
              int val = 0;
              for (char c : lit.value) val = val * 10 + (c - '0');
              return ast::term{ast::li_int{val}};
            },
            [](tk::kw_true) -> term_result { return ast::term{ast::li_bool{true}}; },
            [](tk::kw_false) -> term_result { return ast::term{ast::li_bool{false}}; },
            [](auto) -> term_result { return std::unexpected{parse_err_unknown{}}; },
        },
        tok);
  }
  term_result parse_lambda() {
    advance();
    if (!curtok_.has_value()) return std::unexpected{parse_err_unknown{}};

    auto* id = std::get_if<tk::id>(&peek());
    if (!id) return std::unexpected{parse_err_unknown{}};
    auto name = id->name;
    advance();

    if (!expect<tk::op_colon>()) return std::unexpected{parse_err_unknown{}};

    auto param_type =
        std::visit(overloaded{
                       [this](tk::kw_int) -> std::expected<ast::type, parse_err> {
                         advance();
                         return ast::type{ast::type_int{}};
                       },
                       [this](tk::kw_bool) -> std::expected<ast::type, parse_err> {
                         advance();
                         return ast::type{ast::type_bool{}};
                       },
                       [this](tk::kw_string) -> std::expected<ast::type, parse_err> {
                         advance();
                         return ast::type{ast::type_string{}};
                       },
                       [this](tk::lparen) -> std::expected<ast::type, parse_err> {
                         advance();
                         if (!expect<tk::rparen>()) return std::unexpected{parse_err_unknown{}};
                         return ast::type{ast::type_unit{}};
                       },
                       [](auto) -> std::expected<ast::type, parse_err> { return std::unexpected{parse_err_unknown{}}; },
                   },
                   peek());
    if (!param_type.has_value()) return std::unexpected{parse_err_unknown{}};

    if (!expect<tk::op_dot>()) return std::unexpected{parse_err_unknown{}};
    sema_.push_binding(name);
    auto body = parse_expr(0);
    sema_.pop_binding();
    if (!body.has_value()) return std::unexpected{parse_err_unknown{}};
    return ast::term{ast::abst{.param_type = *param_type, .body = make_term(actx_, *std::move(body))}};
  }
  term_result parse_if() {
    advance();
    auto cond = parse_expr(0);
    if (!cond.has_value()) return std::unexpected{parse_err_unknown{}};
    if (auto ty = type_of(*cond); ty.has_value() && !std::holds_alternative<ast::type_bool>(*ty))
      return std::unexpected{parse_err_unknown{}};
    if (!expect<tk::kw_then>()) return std::unexpected{parse_err_unknown{}};
    auto then_expr = parse_expr(0);
    if (!then_expr.has_value()) return std::unexpected{parse_err_unknown{}};
    if (!expect<tk::kw_else>()) return std::unexpected{parse_err_unknown{}};
    auto else_expr = parse_expr(0);
    if (!else_expr.has_value()) return std::unexpected{parse_err_unknown{}};
    return ast::term{ast::ifexpr{.cond = make_term(actx_, *std::move(cond)),
                                 .then = make_term(actx_, *std::move(then_expr)),
                                 .els = make_term(actx_, *std::move(else_expr))}};
  }
  term_result parse_infix(ast::term left, token op) {
    int prec = *infix_precedence(op);
    auto right = parse_expr(prec);
    if (!right.has_value()) return std::unexpected{parse_err_unknown{}};
    return ast::term{ast::binop{
        .op = std::move(op), .left = make_term(actx_, std::move(left)), .right = make_term(actx_, *std::move(right))}};
  }
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
  sema& sema_;
  ast::context actx_;
  lex_result curtok_;
  lex_result nextok_;
};

}  // namespace

parse_result parse(const std::string& source) {
  sema s;
  return parser{source, s}.run_pass();
}
