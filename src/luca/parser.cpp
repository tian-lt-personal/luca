// std
#include <cassert>
#include <optional>
// luca
#include "lexer.hpp"
#include "mp.hpp"
#include "parser.hpp"
#include "sema.hpp"

namespace {

template <class... Ts>
constexpr bool holds_one_of(const token& tok) noexcept {
  return (std::holds_alternative<Ts>(tok) || ...);
}

constexpr bool is_start_of_expr(const token& tk) noexcept {
  return holds_one_of<tk::id, tk::li_int, tk::kw_lambda, tk::kw_if, tk::kw_true, tk::kw_false, tk::lparen>(tk);
}

constexpr int prec_unary_minus = 40;
constexpr int prec_appl = 50;
std::optional<int> infix_precedence(const token& tok) noexcept {
  return std::visit(overloaded{
                        [](const tk::op_eq&) -> std::optional<int> { return 10; },
                        [](const tk::op_ne&) -> std::optional<int> { return 10; },
                        [](const tk::op_gt&) -> std::optional<int> { return 10; },
                        [](const tk::op_lt&) -> std::optional<int> { return 10; },
                        [](const tk::op_plus&) -> std::optional<int> { return 20; },
                        [](const tk::op_minus&) -> std::optional<int> { return 20; },
                        [](const tk::op_mul&) -> std::optional<int> { return 30; },
                        [](const tk::op_div&) -> std::optional<int> { return 30; },
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
  explicit parser(const std::string& source)
      : lex_(source),
        actx_{std::make_unique<std::pmr::monotonic_buffer_resource>()},
        sema_(actx_),
        curtok_(lex_.next()),
        nextok_(lex_.next()) {}
  parse_result run_pass() && {
    auto term = parse_tem();
    return {std::move(term), std::move(actx_)};
  }

 private:
  ast::term parse_tem() { return parse_expr(0); }
  ast::term parse_expr(int precedence) {
    auto left = parse_prefix();
    while (true) {
      if (!curtok_.has_value()) {
        if (std::holds_alternative<lex_err_eof>(curtok_.error())) break;
        throw parse_err_unknown{};
      }
      if (auto prec = infix_precedence(peek()); prec.has_value() && *prec > precedence) {
        auto op = peek();
        advance();
        left = parse_infix(std::move(left), op);
        continue;
      }
      if (is_start_of_expr(peek()) && prec_appl > precedence) {
        auto rhs = parse_expr(prec_appl);
        left = ast::term{ast::appl{.func = make_term(actx_, std::move(left)), .arg = make_term(actx_, std::move(rhs))}};
        continue;
      }
      break;
    }
    return left;
  }
  ast::term parse_prefix() {
    if (!curtok_.has_value()) throw parse_err_with_lexer_err{curtok_.error()};
    return std::visit(overloaded{
                          [this](tk::kw_lambda) { return parse_lambda(); },
                          [this](tk::kw_let) { return parse_let(); },
                          [this](tk::kw_if) { return parse_if(); },
                          [this](tk::lparen) -> ast::term {
                            advance();
                            auto inner = parse_expr(0);
                            expect<tk::rparen>();
                            return inner;
                          },
                          [this](tk::op_minus) -> ast::term {
                            advance();
                            auto rhs = parse_expr(prec_unary_minus);
                            return ast::term{ast::binop{.op = tk::op_minus{},
                                                        .left = make_term(actx_, ast::li_int{0}),
                                                        .right = make_term(actx_, std::move(rhs))}};
                          },
                          [this](auto) -> ast::term { return parse_atom(); },
                      },
                      *curtok_);
  }
  ast::term parse_atom() {
    auto tok = peek();
    advance();
    return std::visit(
        overloaded{
            [this](tk::id id) -> ast::term { return ast::term{ast::var{sema_.resolve_binding_index(id.name)}}; },
            [](tk::li_int lit) -> ast::term {
              int val = 0;
              for (char c : lit.value) val = val * 10 + (c - '0');
              return ast::term{ast::li_int{val}};
            },
            [](tk::kw_true) -> ast::term { return ast::term{ast::li_bool{true}}; },
            [](tk::kw_false) -> ast::term { return ast::term{ast::li_bool{false}}; },
            [](auto) -> ast::term { throw parse_err_unknown{}; },
        },
        tok);
  }
  ast::term parse_lambda() {
    advance();
    check_curtok();

    auto* id = std::get_if<tk::id>(&peek());
    if (!id) throw parse_err_unknown{};
    auto name = id->name;
    advance();
    expect<tk::op_colon>();
    auto param_type = std::visit(overloaded{
                                     [this](tk::kw_int) -> ast::type {
                                       advance();
                                       return ast::type{ast::type_int{}};
                                     },
                                     [this](tk::kw_bool) -> ast::type {
                                       advance();
                                       return ast::type{ast::type_bool{}};
                                     },
                                     [this](tk::kw_string) -> ast::type {
                                       advance();
                                       return ast::type{ast::type_string{}};
                                     },
                                     [this](tk::lparen) -> ast::type {
                                       advance();
                                       expect<tk::rparen>();
                                       return ast::type{ast::type_unit{}};
                                     },
                                     [](auto) -> ast::type { throw parse_err_unknown{}; },
                                 },
                                 peek());

    expect<tk::op_dot>();
    sema_.push_binding(name);
    auto body = parse_expr(0);
    sema_.pop_binding();
    return ast::term{ast::abst{.param_type = std::move(param_type), .body = make_term(actx_, std::move(body))}};
  }
  ast::term parse_if() {
    advance();
    auto cond = parse_expr(0);
    if (auto ty = sema_.type_of(cond); ty.has_value() && !std::holds_alternative<ast::type_bool>(*ty))
      throw parse_err_unknown{};
    expect<tk::kw_then>();
    auto then_expr = parse_expr(0);
    expect<tk::kw_else>();
    auto else_expr = parse_expr(0);
    if (auto then_ty = sema_.type_of(then_expr), else_ty = sema_.type_of(else_expr);
        then_ty.has_value() && else_ty.has_value() && then_ty->index() != else_ty->index())
      throw parse_err_unknown{};
    return ast::term{ast::ifexpr{.cond = make_term(actx_, std::move(cond)),
                                 .then = make_term(actx_, std::move(then_expr)),
                                 .els = make_term(actx_, std::move(else_expr))}};
  }
  ast::term parse_let() {
    advance();
    check_curtok();

    auto* id = std::get_if<tk::id>(&peek());
    if (!id) throw parse_err_unknown{};
    auto name = id->name;
    advance();

    expect<tk::op_eq>();
    auto bound_expr = parse_expr(0);
    auto bound_ty = sema_.type_of(bound_expr);
    if (!bound_ty.has_value()) throw parse_err_unknown{};

    expect<tk::kw_in>();
    sema_.push_binding(name);
    auto body = parse_expr(0);
    sema_.pop_binding();

    return ast::term{ast::appl{
        .func = make_term(
            actx_, ast::term{ast::abst{.param_type = *std::move(bound_ty), .body = make_term(actx_, std::move(body))}}),
        .arg = make_term(actx_, std::move(bound_expr))}};
  }
  ast::term parse_infix(ast::term left, token op) {
    int prec = *infix_precedence(op);
    auto right = parse_expr(prec);
    if (auto lty = sema_.type_of(left), rty = sema_.type_of(right);
        lty.has_value() && rty.has_value() &&
        (!std::holds_alternative<ast::type_int>(*lty) || !std::holds_alternative<ast::type_int>(*rty)))
      throw parse_err_unknown{};
    return ast::term{ast::binop{
        .op = std::move(op), .left = make_term(actx_, std::move(left)), .right = make_term(actx_, std::move(right))}};
  }
  const token& peek() const noexcept {
    assert(curtok_.has_value());
    return *curtok_;
  }
  void advance() noexcept {
    curtok_ = nextok_;
    nextok_ = lex_.next();
  }
  void check_curtok() const {
    if (!curtok_.has_value()) throw parse_err_unknown{};
  }
  template <class T>
  void expect() {
    if (!curtok_.has_value() || !std::holds_alternative<T>(*curtok_)) throw parse_err_unknown{};
    advance();
  }

 private:
  lexer lex_;
  ast::context actx_;
  sema sema_;
  lex_result curtok_;
  lex_result nextok_;
};

}  // namespace

parse_result parse(const std::string& source) { return parser{source}.run_pass(); }
