// std
#include <cassert>
#include <map>
#include <optional>
#include <string>
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
  return holds_one_of<tk::id, tk::li_int, tk::kw_lambda, tk::kw_if, tk::kw_true, tk::kw_false, tk::lparen, tk::lbrace>(
      tk);
}

constexpr int prec_unary_minus = 40;
constexpr int prec_appl = 50;
constexpr int prec_dot = 60;
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

template <class T>
ast::type* make_type(ast::context& actx, T value) {
  std::pmr::polymorphic_allocator<ast::type> alloc{actx.arena.get()};
  return alloc.new_object<ast::type>(std::move(value));
}

struct parsed {
  ast::term term;
  src_range loc;
};

[[noreturn]] void fail(diagnostic d) { throw parse_err{std::move(d)}; }

std::string type_name(const ast::type& t) {
  return std::visit(overloaded{
                        [](const ast::type_unit&) { return std::string{"unit"}; },
                        [](const ast::type_int&) { return std::string{"int"}; },
                        [](const ast::type_bool&) { return std::string{"bool"}; },
                        [](const ast::type_string&) { return std::string{"string"}; },
                        [](const ast::type_arrow& a) { return type_name(*a.from) + " -> " + type_name(*a.to); },
                        [](const ast::type_rec& r) {
                          if (!r.name.empty()) return std::string{r.name};
                          std::string s = "{";
                          for (size_t i = 0; i < r.fields.size(); ++i) {
                            if (i) s += ", ";
                            s += std::string{r.fields[i].name} + ":" + type_name(*r.fields[i].ty);
                          }
                          return s + "}";
                        },
                    },
                    t);
}

std::string token_text(const token& t) {
  return std::visit(overloaded{
                        [](const tk::id& id) { return std::string{id.name}; },
                        [](const tk::li_int& n) { return std::string{n.value}; },
                        [](const tk::li_str&) { return std::string{"a string literal"}; },
                        [](const tk::kw_lambda&) { return std::string{"\\"}; },
                        [](const tk::kw_let&) { return std::string{"let"}; },
                        [](const tk::kw_in&) { return std::string{"in"}; },
                        [](const tk::kw_if&) { return std::string{"if"}; },
                        [](const tk::kw_then&) { return std::string{"then"}; },
                        [](const tk::kw_else&) { return std::string{"else"}; },
                        [](const tk::kw_true&) { return std::string{"true"}; },
                        [](const tk::kw_false&) { return std::string{"false"}; },
                        [](const tk::kw_bool&) { return std::string{"bool"}; },
                        [](const tk::kw_int&) { return std::string{"int"}; },
                        [](const tk::kw_string&) { return std::string{"string"}; },
                        [](const tk::kw_fix&) { return std::string{"fix"}; },
                        [](const tk::kw_type&) { return std::string{"type"}; },
                        [](const tk::op_plus&) { return std::string{"+"}; },
                        [](const tk::op_minus&) { return std::string{"-"}; },
                        [](const tk::op_mul&) { return std::string{"*"}; },
                        [](const tk::op_div&) { return std::string{"/"}; },
                        [](const tk::op_eq&) { return std::string{"="}; },
                        [](const tk::op_ne&) { return std::string{"!="}; },
                        [](const tk::op_gt&) { return std::string{">"}; },
                        [](const tk::op_lt&) { return std::string{"<"}; },
                        [](const tk::op_arrow&) { return std::string{"->"}; },
                        [](const tk::op_comma&) { return std::string{","}; },
                        [](const tk::op_colon&) { return std::string{":"}; },
                        [](const tk::op_dot&) { return std::string{"."}; },
                        [](const tk::lparen&) { return std::string{"("}; },
                        [](const tk::rparen&) { return std::string{")"}; },
                        [](const tk::lbrace&) { return std::string{"{"}; },
                        [](const tk::rbrace&) { return std::string{"}"}; },
                    },
                    t);
}

std::string found_text(const lex_result& cur) {
  if (cur.has_value()) return "'" + token_text(cur->t) + "'";
  if (std::holds_alternative<lex_err_eof>(cur.error())) return "end of input";
  return "an invalid character";
}

src_range err_loc(const lex_err& e) noexcept {
  return std::visit([](const auto& err) { return err.loc; }, e);
}

diagnostic lex_err_diag(const lex_err& e) {
  auto [code, msg, hint] = std::visit(
      overloaded{
          [](const lex_err_eof&) { return std::tuple{"A001", "unexpected end of input", ""}; },
          [](const lex_err_char&) { return std::tuple{"A001", "unexpected character", ""}; },
          [](const lex_err_str&) {
            return std::tuple{"A002", "unterminated string literal", "close the string with a double quote"};
          },
          [](const lex_err_glued&) { return std::tuple{"A003", "identifiers cannot start with a digit", ""}; },
      },
      e);
  return {err_loc(e), code, msg, hint};
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
    while (curtok_.has_value() && std::holds_alternative<tk::kw_type>(curtok_->t)) parse_type_decl();
    auto t = parse_tem();
    return {std::move(t.term), std::move(actx_)};
  }

 private:
  void parse_type_decl() {
    auto start = curtok_->loc;
    advance();
    check_curtok();
    auto* id = std::get_if<tk::id>(&peek());
    if (!id)
      fail({curtok_->loc, "B009", "expected an identifier after 'type', found " + found_text(curtok_),
            "write type name = {field : type, ...}"});
    auto name = id->name;
    advance();
    if (!curtok_.has_value() || !std::holds_alternative<tk::op_eq>(curtok_->t))
      fail({curtok_.has_value() ? curtok_->loc : err_loc(curtok_.error()), "B009",
            "expected '=' in the type declaration, found " + found_text(curtok_),
            "write type name = {field : type, ...}"});
    advance();
    if (!(curtok_.has_value() && std::holds_alternative<tk::lbrace>(curtok_->t)))
      fail({curtok_.has_value() ? curtok_->loc : err_loc(curtok_.error()), "B009",
            "expected a record type after '=', found " + found_text(curtok_), "write type name = {field : type, ...}"});
    advance();
    auto ty = parse_record_type();
    std::get<ast::type_rec>(ty).name = name;
    if (!types_.emplace(name, std::move(ty)).second)
      fail({start, "C010", "duplicate type declaration '" + std::string{name} + "'", "use a different name"});
  }
  ast::type parse_record_type() {
    if (curtok_.has_value() && std::holds_alternative<tk::rbrace>(curtok_->t))
      fail({curtok_->loc, "B008", "empty record type", "write {field : type, ...}"});
    ast::type_rec rec;
    for (;;) {
      check_curtok();
      auto* id = std::get_if<tk::id>(&peek());
      if (!id)
        fail(
            {curtok_->loc, "B005", "expected a field name, found " + found_text(curtok_), "write {field : type, ...}"});
      auto name = id->name;
      auto loc = curtok_->loc;
      advance();
      expect<tk::op_colon>("':'");
      auto fty = parse_type();
      for (const auto& f : rec.fields)
        if (f.name == name)
          fail({loc, "C014", "duplicate field '" + std::string{name} + "' in a record type", "use a different name"});
      rec.fields.push_back(ast::rec_field{std::string{name}, make_type(actx_, std::move(fty))});
      if (curtok_.has_value() && std::holds_alternative<tk::op_comma>(curtok_->t)) {
        advance();
        continue;
      }
      expect<tk::rbrace>("'}'");
      break;
    }
    return ast::type{std::move(rec)};
  }
  // OCaml-style initialization: a record literal whose fields match the expected
  // record type by name is rebuilt in the expected field order and accepted in its
  // place (a literal initializes a declared type; any other value must match exactly)
  std::optional<ast::term> lift_to(const ast::term& t, const ast::type& expected) {
    auto* rec = std::get_if<ast::type_rec>(&expected);
    auto* tup = std::get_if<ast::tup>(&t);
    if (!rec || !tup || tup->fields.size() != rec->fields.size()) return std::nullopt;
    ast::tup rebuilt;
    rebuilt.fields.reserve(rec->fields.size());
    for (const auto& ef : rec->fields) {
      const ast::tup_field* lf = nullptr;
      for (const auto& f : tup->fields)
        if (f.name == ef.name) {
          lf = &f;
          break;
        }
      if (!lf) return std::nullopt;
      if (!same_type(*ef.ty, lf->ann)) {
        auto nested = lift_to(*lf->value, *ef.ty);
        if (!nested.has_value()) return std::nullopt;
        rebuilt.fields.push_back(ast::tup_field{lf->name, lf->ann, make_term(actx_, std::move(*nested))});
        continue;
      }
      rebuilt.fields.push_back(ast::tup_field{lf->name, lf->ann, lf->value});
    }
    return ast::term{std::move(rebuilt)};
  }
  parsed parse_tem() {
    auto t = parse_expr(0);
    if (auto ty = sema_.type_of(t.term); ty.has_value() && std::holds_alternative<ast::type_arrow>(*ty))
      fail({t.loc, "C009", "top-level expression must have a value type, got '" + type_name(*ty) + "'",
            "apply the function to an argument"});
    return t;
  }
  parsed parse_expr(int precedence) {
    auto left = parse_prefix();
    while (true) {
      if (!curtok_.has_value()) {
        if (std::holds_alternative<lex_err_eof>(curtok_.error())) break;
        fail(lex_err_diag(curtok_.error()));
      }
      if (auto prec = infix_precedence(peek()); prec.has_value() && *prec > precedence) {
        auto op = peek();
        auto op_loc = curtok_->loc;
        advance();
        left = parse_infix(std::move(left), op, op_loc);
        continue;
      }
      if (std::holds_alternative<tk::op_dot>(peek()) && prec_dot > precedence) {
        advance();
        check_curtok();
        auto* id = std::get_if<tk::id>(&peek());
        if (!id)
          fail({curtok_->loc, "B005", "expected a field name after '.', found " + found_text(curtok_),
                "write .field to read a record field"});
        auto name = id->name;
        auto field_loc = curtok_->loc;
        advance();
        auto base_ty = sema_.type_of(left.term);
        auto* rec = base_ty ? std::get_if<ast::type_rec>(&*base_ty) : nullptr;
        if (!rec)
          fail({left.loc, "C012",
                "cannot access field '" + std::string{name} + "' of a value of type '" +
                    (base_ty ? type_name(*base_ty) : std::string{"an unknown type"}) + "'",
                "field access requires a record value"});
        size_t index = rec->fields.size();
        for (size_t i = 0; i < rec->fields.size(); ++i)
          if (rec->fields[i].name == name) {
            index = i;
            break;
          }
        if (index == rec->fields.size()) {
          std::string fields = "the record's fields are ";
          for (size_t i = 0; i < rec->fields.size(); ++i) fields += (i ? ", " : "") + rec->fields[i].name;
          fail({field_loc, "C013", "record has no field '" + std::string{name} + "'", fields});
        }
        left = parsed{ast::term{ast::field{.base = make_term(actx_, std::move(left.term)), .index = index}},
                      {left.loc.begin, field_loc.end}};
        continue;
      }
      if (is_start_of_expr(peek()) && prec_appl > precedence) {
        auto rhs = parse_expr(prec_appl);
        if (auto fty = sema_.type_of(left.term); fty.has_value()) {
          auto* arrow = std::get_if<ast::type_arrow>(&*fty);
          if (!arrow)
            fail({left.loc, "C002", "cannot apply value of type '" + type_name(*fty) + "'",
                  "only functions can be applied"});
          if (auto aty = sema_.type_of(rhs.term); aty.has_value() && !same_type(*arrow->from, *aty)) {
            if (auto lifted = lift_to(rhs.term, *arrow->from); lifted.has_value()) {
              rhs.term = std::move(*lifted);
            } else {
              bool lit_vs_named = std::holds_alternative<ast::tup>(rhs.term) &&
                                  std::get_if<ast::type_rec>(&*arrow->from) != nullptr &&
                                  !std::get<ast::type_rec>(*arrow->from).name.empty();
              if (lit_vs_named)
                fail({rhs.loc, "C015",
                      "cannot initialize '" + type_name(*arrow->from) + "' with '" + type_name(*aty) + "'",
                      "the literal's fields must match the record's fields by name and type"});
              fail({rhs.loc, "C003",
                    "cannot pass value of type '" + type_name(*aty) + "' to parameter of type '" +
                        type_name(*arrow->from) + "'",
                    "pass an argument of the parameter type"});
            }
          }
        }
        left = parsed{ast::term{ast::appl{.func = make_term(actx_, std::move(left.term)),
                                          .arg = make_term(actx_, std::move(rhs.term))}},
                      {left.loc.begin, rhs.loc.end}};
        continue;
      }
      break;
    }
    return left;
  }
  parsed parse_prefix() {
    if (!curtok_.has_value()) {
      if (std::holds_alternative<lex_err_eof>(curtok_.error()))
        fail({err_loc(curtok_.error()), "B006", "unexpected end of input", "complete the expression"});
      fail(lex_err_diag(curtok_.error()));
    }
    return std::visit(overloaded{
                          [this](tk::kw_lambda) { return parse_lambda(); },
                          [this](tk::kw_let) { return parse_let(); },
                          [this](tk::kw_if) { return parse_if(); },
                          [this](tk::kw_fix) { return parse_fix(); },
                          [this](tk::lbrace) { return parse_tuple_literal(); },
                          [this](tk::lparen) -> parsed {
                            advance();
                            auto inner = parse_expr(0);
                            expect<tk::rparen>("')'");
                            return inner;
                          },
                          [this](tk::op_minus) -> parsed {
                            auto loc = curtok_->loc;
                            advance();
                            auto rhs = parse_expr(prec_unary_minus);
                            return {ast::term{ast::binop{.op = tk::op_minus{},
                                                         .left = make_term(actx_, ast::li_int{0}),
                                                         .right = make_term(actx_, std::move(rhs.term))}},
                                    {loc.begin, rhs.loc.end}};
                          },
                          [this](auto) -> parsed { return parse_atom(); },
                      },
                      curtok_->t);
  }
  parsed parse_atom() {
    auto tok = peek();
    auto loc = curtok_->loc;
    advance();
    return std::visit(overloaded{
                          [this, loc](tk::id id) -> parsed {
                            auto idx = sema_.resolve_binding_index(id.name);
                            if (!idx.has_value())
                              fail({loc, "C001", "unbound identifier '" + std::string{id.name} + "'",
                                    "bind it with a lambda parameter or a let expression"});
                            return {ast::term{ast::var{*idx}}, loc};
                          },
                          [loc](tk::li_int lit) -> parsed {
                            int val = 0;
                            for (char c : lit.value) val = val * 10 + (c - '0');
                            return {ast::term{ast::li_int{val}}, loc};
                          },
                          [loc](tk::kw_true) -> parsed { return {ast::term{ast::li_bool{true}}, loc}; },
                          [loc](tk::kw_false) -> parsed { return {ast::term{ast::li_bool{false}}, loc}; },
                          [loc, tok](auto) -> parsed {
                            fail({loc, "B001", "unexpected token '" + token_text(tok) + "'", "expected an expression"});
                          },
                      },
                      tok);
  }
  parsed parse_tuple_literal() {
    auto start = curtok_->loc;
    advance();
    if (curtok_.has_value() && std::holds_alternative<tk::rbrace>(curtok_->t))
      fail({curtok_->loc, "B008", "empty tuple literal", "write {name = value, ...}"});
    ast::tup tup;
    for (;;) {
      check_curtok();
      auto* id = std::get_if<tk::id>(&peek());
      if (!id)
        fail(
            {curtok_->loc, "B005", "expected a field name, found " + found_text(curtok_), "write {name = value, ...}"});
      auto name = id->name;
      auto name_loc = curtok_->loc;
      advance();
      ast::type ann;
      bool annotated = false;
      if (curtok_.has_value() && std::holds_alternative<tk::op_colon>(curtok_->t)) {
        advance();
        ann = parse_type();
        annotated = true;
      }
      expect<tk::op_eq>("'='");
      auto value = parse_expr(0);
      for (const auto& f : tup.fields)
        if (f.name == name)
          fail({name_loc, "C014", "duplicate field '" + std::string{name} + "' in a tuple literal",
                "use a different name"});
      // type_of cannot fail here: every failure mode is rejected earlier during parsing
      auto actual = *sema_.type_of(value.term);
      if (annotated && !same_type(ann, actual)) {
        auto lifted = lift_to(value.term, ann);
        if (lifted.has_value())
          value.term = std::move(*lifted);
        else
          fail({value.loc, "C015",
                "field '" + std::string{name} + "' of type '" + type_name(ann) +
                    "' cannot be initialized with a value of type '" + type_name(actual) + "'",
                "match the value with the field's declared type"});
      }
      if (!annotated) ann = std::move(actual);
      tup.fields.push_back(ast::tup_field{std::string{name}, std::move(ann), make_term(actx_, std::move(value.term))});
      if (curtok_.has_value() && std::holds_alternative<tk::op_comma>(curtok_->t)) {
        advance();
        continue;
      }
      auto close = curtok_.has_value() ? curtok_->loc : src_range{0, 0};
      expect<tk::rbrace>("'}'");
      return {ast::term{std::move(tup)}, {start.begin, close.end}};
    }
  }
  ast::type parse_type() {
    if (!curtok_.has_value()) {
      if (std::holds_alternative<lex_err_eof>(curtok_.error()))
        fail({err_loc(curtok_.error()), "B006", "unexpected end of input", "complete the expression"});
      fail(lex_err_diag(curtok_.error()));
    }
    auto base = std::visit(overloaded{
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
                                 if (curtok_.has_value() && std::holds_alternative<tk::rparen>(curtok_->t)) {
                                   advance();
                                   return ast::type{ast::type_unit{}};
                                 }
                                 auto inner = parse_type();
                                 expect<tk::rparen>("')'");
                                 return inner;
                               },
                               [this](tk::lbrace) -> ast::type {
                                 advance();
                                 return parse_record_type();
                               },
                               [this](tk::id id) -> ast::type {
                                 auto loc = curtok_->loc;
                                 advance();
                                 auto it = types_.find(id.name);
                                 if (it == types_.end())
                                   fail({loc, "C011", "unknown type '" + std::string{id.name} + "'",
                                         "declare it with 'type name = {field : type, ...}' before use"});
                                 return it->second;
                               },
                               [this](auto) -> ast::type {
                                 fail({curtok_->loc, "B002", "expected a type, found " + found_text(curtok_),
                                       "types are int, bool, string, (), {field : type, ...} and declared names"});
                               },
                           },
                           peek());
    if (curtok_.has_value() && std::holds_alternative<tk::op_arrow>(curtok_->t)) {
      advance();
      return ast::type{
          ast::type_arrow{.from = make_type(actx_, std::move(base)), .to = make_type(actx_, parse_type())}};
    }
    return base;
  }
  parsed parse_lambda() {
    auto start = curtok_->loc;
    advance();
    check_curtok();

    auto* id = std::get_if<tk::id>(&peek());
    if (!id)
      fail({curtok_->loc, "B003", "expected an identifier after '\\', found " + found_text(curtok_),
            "write \\name : type . body"});
    auto name = id->name;
    advance();
    expect<tk::op_colon>("':'");
    auto param_type = parse_type();

    expect<tk::op_dot>("'.'");
    sema_.push_binding(name, param_type);
    auto body = parse_expr(0);
    sema_.pop_binding();
    return {ast::term{ast::abst{.param_type = std::move(param_type), .body = make_term(actx_, std::move(body.term))}},
            {start.begin, body.loc.end}};
  }
  parsed parse_if() {
    auto start = curtok_->loc;
    advance();
    auto cond = parse_expr(0);
    if (auto ty = sema_.type_of(cond.term); ty.has_value() && !std::holds_alternative<ast::type_bool>(*ty))
      fail({cond.loc, "C004", "if condition must be 'bool', found '" + type_name(*ty) + "'", ""});
    expect<tk::kw_then>("'then'");
    auto then_expr = parse_expr(0);
    expect<tk::kw_else>("'else'");
    auto else_expr = parse_expr(0);
    if (auto then_ty = sema_.type_of(then_expr.term), else_ty = sema_.type_of(else_expr.term);
        then_ty.has_value() && else_ty.has_value() && !same_type(*then_ty, *else_ty))
      fail({else_expr.loc, "C005",
            "if branches must have the same type: '" + type_name(*then_ty) + "' vs '" + type_name(*else_ty) + "'",
            "make both branches produce the same type"});
    return {ast::term{ast::ifexpr{.cond = make_term(actx_, std::move(cond.term)),
                                  .then = make_term(actx_, std::move(then_expr.term)),
                                  .els = make_term(actx_, std::move(else_expr.term))}},
            {start.begin, else_expr.loc.end}};
  }
  parsed parse_let() {
    auto start = curtok_->loc;
    advance();
    check_curtok();
    if (std::holds_alternative<tk::lbrace>(curtok_->t)) return parse_let_binding(start);

    auto* id = std::get_if<tk::id>(&peek());
    if (!id)
      fail({curtok_->loc, "B004", "expected an identifier after 'let', found " + found_text(curtok_),
            "write let name : type = expr in body"});
    auto name = id->name;
    advance();

    // the annotation is optional; absent → the bound expression's type (monomorphic)
    ast::type ann;
    bool annotated = false;
    if (curtok_.has_value() && std::holds_alternative<tk::op_colon>(curtok_->t)) {
      advance();
      ann = parse_type();
      annotated = true;
    }

    expect<tk::op_eq>("'='");
    auto bound_expr = parse_expr(0);
    // type_of cannot fail here: every failure mode is rejected earlier during parsing
    auto ty = *sema_.type_of(bound_expr.term);
    if (annotated && !same_type(ann, ty)) {
      auto lifted = lift_to(bound_expr.term, ann);
      if (lifted.has_value())
        bound_expr.term = std::move(*lifted);
      else
        fail({bound_expr.loc, "C015",
              "cannot initialize '" + type_name(ann) + "' with a value of type '" + type_name(ty) + "'",
              "the value must have the annotated type"});
    }
    if (!annotated) ann = std::move(ty);

    expect<tk::kw_in>("'in'");
    sema_.push_binding(name, ann);
    auto body = parse_expr(0);
    sema_.pop_binding();

    return {ast::term{ast::appl{
                .func = make_term(actx_, ast::term{ast::abst{.param_type = std::move(ann),
                                                             .body = make_term(actx_, std::move(body.term))}}),
                .arg = make_term(actx_, std::move(bound_expr.term))}},
            {start.begin, body.loc.end}};
  }
  // let {a, b} = E in body  desugars to  let $t = E in let a = $t.field(0) in
  // let b = $t.field(1) in body; $t cannot collide with user ids ('$' is not lexed)
  parsed parse_let_binding(src_range start) {
    advance();  // '{'
    std::vector<std::pair<std::string, src_range>> names;
    for (;;) {
      check_curtok();
      auto* id = std::get_if<tk::id>(&peek());
      if (!id)
        fail({curtok_->loc, "B005", "expected a binding name, found " + found_text(curtok_),
              "write let {a, b} = expr in body"});
      auto name = id->name;
      auto loc = curtok_->loc;
      advance();
      for (const auto& [n, _] : names)
        if (n == name)
          fail({loc, "C018", "duplicate name '" + std::string{name} + "' in the binding pattern",
                "use different names"});
      names.emplace_back(std::string{name}, loc);
      if (curtok_.has_value() && std::holds_alternative<tk::op_comma>(curtok_->t)) {
        advance();
        continue;
      }
      expect<tk::rbrace>("'}'");
      break;
    }
    expect<tk::op_eq>("'='");
    auto bound_expr = parse_expr(0);
    // type_of cannot fail here: every failure mode is rejected earlier during parsing
    auto bound_ty = *sema_.type_of(bound_expr.term);
    auto* rec = std::get_if<ast::type_rec>(&bound_ty);
    if (!rec)
      fail({bound_expr.loc, "C016", "cannot bind names from a value of type '" + type_name(bound_ty) + "'",
            "structured binding requires a record value"});
    if (rec->fields.size() != names.size())
      fail({bound_expr.loc, "C017",
            "cannot bind " + std::to_string(names.size()) + " name(s) to a record of " +
                std::to_string(rec->fields.size()) + " field(s)",
            "the number of names must match the record's fields"});

    expect<tk::kw_in>("'in'");
    // bindings: [$t, a, b]; the k-th field access is the arg of the (k+1)-th lambda,
    // evaluated with k binders above $t, so its de Bruijn index is k
    sema_.push_binding("$t", bound_ty);
    std::vector<ast::term> field_terms;
    field_terms.reserve(names.size());
    for (size_t k = 0; k < names.size(); ++k) {
      auto idx = sema_.resolve_binding_index("$t");
      field_terms.push_back(ast::term{ast::field{.base = make_term(actx_, ast::term{ast::var{*idx}}), .index = k}});
      sema_.push_binding(names[k].first, *rec->fields[k].ty);
    }
    auto body = parse_expr(0);
    for (size_t k = 0; k < names.size(); ++k) sema_.pop_binding();
    sema_.pop_binding();

    ast::term inner = std::move(body.term);
    for (size_t k = names.size(); k-- > 0;)
      inner = ast::term{
          ast::appl{.func = make_term(actx_, ast::term{ast::abst{.param_type = *rec->fields[k].ty,
                                                                 .body = make_term(actx_, std::move(inner))}}),
                    .arg = make_term(actx_, std::move(field_terms[k]))}};
    return {
        ast::term{ast::appl{.func = make_term(actx_, ast::term{ast::abst{.param_type = std::move(bound_ty),
                                                                         .body = make_term(actx_, std::move(inner))}}),
                            .arg = make_term(actx_, std::move(bound_expr.term))}},
        {start.begin, body.loc.end}};
  }
  parsed parse_fix() {
    auto start = curtok_->loc;
    advance();
    auto operand = parse_expr(prec_appl);
    auto* op_abst = std::get_if<ast::abst>(&operand.term);
    if (!op_abst || !std::get_if<ast::abst>(op_abst->body))
      fail({operand.loc, "B007", "fix expects a lambda whose body is a lambda",
            "write fix (\\f : τ -> τ . \\x : σ . body)"});
    if (auto ty = sema_.type_of(operand.term); ty.has_value()) {
      auto* arrow = std::get_if<ast::type_arrow>(&*ty);
      if (!arrow || !same_type(*arrow->from, *arrow->to))
        fail({operand.loc, "C007", "fix generator must be of type τ -> τ, found '" + type_name(*ty) + "'",
              "the generator's parameter type must equal its result type"});
    }
    return {ast::term{ast::fix{.body = make_term(actx_, std::move(operand.term))}}, {start.begin, operand.loc.end}};
  }
  parsed parse_infix(parsed left, token op, src_range op_loc) {
    int prec = *infix_precedence(op);
    auto right = parse_expr(prec);
    if (auto lty = sema_.type_of(left.term), rty = sema_.type_of(right.term);
        lty.has_value() && rty.has_value() &&
        (!std::holds_alternative<ast::type_int>(*lty) || !std::holds_alternative<ast::type_int>(*rty))) {
      bool left_ok = std::holds_alternative<ast::type_int>(*lty);
      fail(
          {left_ok ? right.loc : left.loc, "C008",
           "operator '" + token_text(op) + "' expects 'int' operands, found '" + type_name(left_ok ? *rty : *lty) + "'",
           "arithmetic and comparison operators require int operands"});
    }
    return {ast::term{ast::binop{.op = std::move(op),
                                 .left = make_term(actx_, std::move(left.term)),
                                 .right = make_term(actx_, std::move(right.term))}},
            {left.loc.begin, right.loc.end}};
  }
  const token& peek() const noexcept {
    assert(curtok_.has_value());
    return curtok_->t;
  }
  void advance() noexcept {
    curtok_ = nextok_;
    nextok_ = lex_.next();
  }
  void check_curtok() const {
    if (!curtok_.has_value()) {
      if (std::holds_alternative<lex_err_eof>(curtok_.error()))
        fail({err_loc(curtok_.error()), "B006", "unexpected end of input", "complete the expression"});
      fail(lex_err_diag(curtok_.error()));
    }
  }
  template <class T>
  void expect(std::string_view what) {
    if (!curtok_.has_value()) {
      if (std::holds_alternative<lex_err_eof>(curtok_.error()))
        fail({err_loc(curtok_.error()), "B005", "expected " + std::string{what} + ", found end of input", ""});
      fail(lex_err_diag(curtok_.error()));
    }
    if (!std::holds_alternative<T>(curtok_->t))
      fail({curtok_->loc, "B005", "expected " + std::string{what} + ", found " + found_text(curtok_), ""});
    advance();
  }

 private:
  lexer lex_;
  ast::context actx_;
  sema sema_;
  std::map<std::string_view, ast::type> types_;
  lex_result curtok_;
  lex_result nextok_;
};

}  // namespace

parse_result parse(const std::string& source) { return parser{source}.run_pass(); }
