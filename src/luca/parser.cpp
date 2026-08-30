// std
#include <cassert>
#include <climits>
#include <filesystem>
#include <fstream>
#include <iterator>
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
  return holds_one_of<tk::id, tk::li_int, tk::kw_lambda, tk::kw_if, tk::kw_match, tk::kw_true, tk::kw_false,
                      tk::kw_import, tk::lparen>(tk);
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

template <class T>
ast::type* make_type(ast::context& actx, T value) {
  std::pmr::polymorphic_allocator<ast::type> alloc{actx.arena.get()};
  return alloc.new_object<ast::type>(std::move(value));
}

struct parsed {
  ast::term term;
  src_range loc;
};

struct module_entry {
  std::string name;  // copied: push_binding stores views into this module's source
  ast::term* def;    // the desugared let's arg, in this module's arena
};

// a type declaration tagged with its declaring module, for provenance-based merge
struct module_type_decl {
  std::string declaring_path;
  std::string name;
  std::vector<ast::sum_ctor> ctors;
};

struct module_result {
  ast::term term;                      // the module's own (unoptimized) term
  ast::context actx;                   // retained by importers
  std::vector<module_entry> skeleton;  // ambient binding chain, outermost first
  std::vector<module_type_decl> types;
};

// canonical identity for cycle detection and type-provenance keys (weakly_canonical when missing)
std::string canonical_path(const std::filesystem::path& p) {
  std::error_code ec;
  auto c = std::filesystem::canonical(p, ec);
  if (ec) c = std::filesystem::weakly_canonical(p);
  return c.generic_string();
}

bool same_path(std::string_view a, std::string_view b) noexcept {
#ifdef _WIN32
  // Windows paths compare case-insensitively
  auto fold = [](std::string s) {
    for (char& ch : s)
      if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    return s;
  };
  return fold(std::string{a}) == fold(std::string{b});
#else
  return a == b;
#endif
}

[[noreturn]] void fail(diagnostic d) { throw parse_err{std::move(d)}; }

std::string type_name(const ast::type& t) {
  return std::visit(overloaded{
                        [](const ast::type_unit&) { return std::string{"()"}; },
                        [](const ast::type_int&) { return std::string{"int"}; },
                        [](const ast::type_bool&) { return std::string{"bool"}; },
                        [](const ast::type_string&) { return std::string{"string"}; },
                        [](const ast::type_arrow& a) { return type_name(*a.from) + " -> " + type_name(*a.to); },
                        [](const ast::type_prod& p) {
                          std::string s = "(";
                          for (size_t i = 0; i < p.fields.size(); ++i) {
                            if (i) s += ", ";
                            s += type_name(*p.fields[i]);
                          }
                          return s + ")";
                        },
                        [](const ast::type_ref& r) { return std::string{r.name}; },
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
                        [](const tk::kw_of&) { return std::string{"of"}; },
                        [](const tk::kw_match&) { return std::string{"match"}; },
                        [](const tk::kw_with&) { return std::string{"with"}; },
                        [](const tk::kw_import&) { return std::string{"import"}; },
                        [](const tk::kw_export&) { return std::string{"export"}; },
                        [](const tk::op_plus&) { return std::string{"+"}; },
                        [](const tk::op_minus&) { return std::string{"-"}; },
                        [](const tk::op_mul&) { return std::string{"*"}; },
                        [](const tk::op_div&) { return std::string{"/"}; },
                        [](const tk::op_eq&) { return std::string{"="}; },
                        [](const tk::op_ne&) { return std::string{"!="}; },
                        [](const tk::op_gt&) { return std::string{">"}; },
                        [](const tk::op_lt&) { return std::string{"<"}; },
                        [](const tk::op_bar&) { return std::string{"|"}; },
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

// -- pass 2: AST optimization ---------------------------------------------------
//
// After pass 1 has built the fully type-checked tree, rewrite it in place,
// preserving evaluation semantics:
//   * tree-shaking: appl(abst(x, body), arg) where x is unused in body becomes
//     body with free-variable de Bruijn indices decremented by 1 (this is what
//     `let x = e in b` desugars to; the arg is dropped — the language is pure);
//   * constant folding: binop of two int literals becomes the literal result
//     (x / 0 is never folded: the machine's raw C++ division is UB at runtime,
//     and the lazy `if` means a /0 in a dead branch is never evaluated);
//   * dead-branch elimination: if <bool literal> then A else B becomes A/B.
//
// A binder x (the fn of an appl) is used iff its body contains a var{k} at
// nesting depth d (counting abst binders only) with k == d: x sits exactly one
// binder above the body root. Match arm bodies are absts but never the fn of
// an appl, so their payload closures are never dropped (the machine applies
// each arm body to the payload); the same holds for fix's abst.

bool uses_binder(const ast::term& t, int depth) noexcept {
  return std::visit(
      overloaded{
          [&](const ast::var& v) { return v.index == depth; },
          [&](const ast::abst& a) { return uses_binder(*a.body, depth + 1); },
          [&](const ast::appl& a) { return uses_binder(*a.func, depth) || uses_binder(*a.arg, depth); },
          [&](const ast::binop& b) { return uses_binder(*b.left, depth) || uses_binder(*b.right, depth); },
          [&](const ast::ifexpr& i) {
            return uses_binder(*i.cond, depth) || uses_binder(*i.then, depth) || uses_binder(*i.els, depth);
          },
          [&](const ast::fix& f) { return uses_binder(*f.body, depth); },
          [&](const ast::tup& tu) {
            for (const auto* el : tu.fields)
              if (uses_binder(*el, depth)) return true;
            return false;
          },
          [&](const ast::field& f) { return uses_binder(*f.base, depth); },
          [&](const ast::ctor& c) { return c.payload && uses_binder(*c.payload, depth); },
          [&](const ast::case_& cs) {
            if (uses_binder(*cs.scrutinee, depth)) return true;
            for (const auto& arm : cs.arms)
              if (uses_binder(*arm.body, depth)) return true;
            return false;
          },
          [](const auto&) { return false; },  // li_int / li_bool / li_unit
      },
      t);
}

// One binder was dropped outside t: renumber every var{k} with k > depth to
// var{k-1}. Precondition: no var{k} with k == depth occurs (the dropped
// binder was unused). Mutates t in place.
void shift_down(ast::term& t, int depth) noexcept {
  std::visit(overloaded{
                 [&](ast::var& v) {
                   if (v.index > depth) v.index -= 1;
                 },
                 [&](ast::abst& a) { shift_down(*a.body, depth + 1); },
                 [&](ast::appl& a) {
                   shift_down(*a.func, depth);
                   shift_down(*a.arg, depth);
                 },
                 [&](ast::binop& b) {
                   shift_down(*b.left, depth);
                   shift_down(*b.right, depth);
                 },
                 [&](ast::ifexpr& i) {
                   shift_down(*i.cond, depth);
                   shift_down(*i.then, depth);
                   shift_down(*i.els, depth);
                 },
                 [&](ast::fix& f) { shift_down(*f.body, depth); },
                 [&](ast::tup& tu) {
                   for (auto* el : tu.fields) shift_down(*el, depth);
                 },
                 [&](ast::field& f) { shift_down(*f.base, depth); },
                 [&](ast::ctor& c) {
                   if (c.payload) shift_down(*c.payload, depth);
                 },
                 [&](ast::case_& cs) {
                   shift_down(*cs.scrutinee, depth);
                   for (auto& arm : cs.arms) shift_down(*arm.body, depth);
                 },
                 [](auto&) {},
             },
             t);
}

// Fold binop(op, li_int, li_int) in place, mirroring the machine's eval
// (machine.cpp) exactly: int results for + - * /, bool results for = != > <.
// Returns true when the node was replaced by a literal. Division by zero and
// int overflow are never folded (UB at runtime; leave the node untouched).
bool fold_literal_binop(ast::term& t) noexcept {
  auto& b = std::get<ast::binop>(t);
  auto* l = std::get_if<ast::li_int>(b.left);
  auto* r = std::get_if<ast::li_int>(b.right);
  if (!l || !r) return false;
  const int lv = l->value;
  const int rv = r->value;
  std::optional<ast::term> folded =
      std::visit(overloaded{
                     [&](tk::op_plus) -> std::optional<ast::term> {
                       const long long res = static_cast<long long>(lv) + rv;
                       if (res < INT_MIN || res > INT_MAX) return std::nullopt;
                       return ast::term{ast::li_int{static_cast<int>(res)}};
                     },
                     [&](tk::op_minus) -> std::optional<ast::term> {
                       const long long res = static_cast<long long>(lv) - rv;
                       if (res < INT_MIN || res > INT_MAX) return std::nullopt;
                       return ast::term{ast::li_int{static_cast<int>(res)}};
                     },
                     [&](tk::op_mul) -> std::optional<ast::term> {
                       const long long res = static_cast<long long>(lv) * rv;
                       if (res < INT_MIN || res > INT_MAX) return std::nullopt;
                       return ast::term{ast::li_int{static_cast<int>(res)}};
                     },
                     [&](tk::op_div) -> std::optional<ast::term> {
                       if (rv == 0) return std::nullopt;
                       return ast::term{ast::li_int{lv / rv}};
                     },
                     [&](tk::op_eq) -> std::optional<ast::term> { return ast::term{ast::li_bool{lv == rv}}; },
                     [&](tk::op_ne) -> std::optional<ast::term> { return ast::term{ast::li_bool{lv != rv}}; },
                     [&](tk::op_gt) -> std::optional<ast::term> { return ast::term{ast::li_bool{lv > rv}}; },
                     [&](tk::op_lt) -> std::optional<ast::term> { return ast::term{ast::li_bool{lv < rv}}; },
                     [](auto) -> std::optional<ast::term> { return std::nullopt; },  // non-binop token
                 },
                 b.op);
  if (folded.has_value()) t = std::move(*folded);
  return folded.has_value();
}

// Bottom-up rewrite of t; returns the node that must replace t (t itself, or
// a descendant of t). Children are rewritten before t is decided, so folds,
// dead-branch eliminations, and newly exposed shakes cascade in one pass.
// Never allocates; dropped nodes simply become garbage in the arena.
ast::term* optimize_node(ast::term& t) noexcept {
  if (auto* a = std::get_if<ast::abst>(&t)) {
    a->body = optimize_node(*a->body);
    return &t;
  }
  if (auto* a = std::get_if<ast::appl>(&t)) {
    a->func = optimize_node(*a->func);
    a->arg = optimize_node(*a->arg);
    if (auto* ab = std::get_if<ast::abst>(a->func); ab && !uses_binder(*ab->body, 0)) {
      shift_down(*ab->body, 0);  // x unused: drop the binder, renumber free vars
      return ab->body;
    }
    return &t;
  }
  if (auto* b = std::get_if<ast::binop>(&t)) {
    b->left = optimize_node(*b->left);
    b->right = optimize_node(*b->right);
    fold_literal_binop(t);
    return &t;
  }
  if (auto* ie = std::get_if<ast::ifexpr>(&t)) {
    ie->cond = optimize_node(*ie->cond);
    ie->then = optimize_node(*ie->then);
    ie->els = optimize_node(*ie->els);
    if (auto* c = std::get_if<ast::li_bool>(ie->cond); c) return c->value ? ie->then : ie->els;
    return &t;
  }
  if (auto* f = std::get_if<ast::fix>(&t)) {
    f->body = optimize_node(*f->body);
    return &t;
  }
  if (auto* tu = std::get_if<ast::tup>(&t)) {
    for (auto*& el : tu->fields) el = optimize_node(*el);
    return &t;
  }
  if (auto* f = std::get_if<ast::field>(&t)) {
    f->base = optimize_node(*f->base);
    return &t;
  }
  if (auto* c = std::get_if<ast::ctor>(&t)) {
    if (c->payload) c->payload = optimize_node(*c->payload);
    return &t;
  }
  if (auto* cs = std::get_if<ast::case_>(&t)) {
    cs->scrutinee = optimize_node(*cs->scrutinee);
    for (auto& arm : cs->arms) arm.body = optimize_node(*arm.body);
    return &t;
  }
  return &t;  // var / li_int / li_bool / li_unit
}

class parser {
 public:
  explicit parser(const std::string& source, std::string path, std::vector<std::string>& import_stack)
      : source_(source),
        self_path_(path.empty() ? std::string{} : canonical_path(path)),
        import_stack_(import_stack),
        lex_(source),
        actx_{std::make_unique<std::pmr::monotonic_buffer_resource>()},
        sema_(actx_),
        curtok_(lex_.next()),
        nextok_(lex_.next()) {}
  module_result run_pass() && {
    while (curtok_.has_value() && std::holds_alternative<tk::kw_type>(curtok_->t)) parse_type_decl();
    auto t = parse_term();
    if (curtok_.has_value())
      fail({curtok_->loc, "B001", "unexpected token '" + token_text(curtok_->t) + "' after the top-level expression",
            ""});
    return {std::move(t.term), std::move(actx_), std::move(skeleton_), std::move(types_)};
  }

 private:
  void parse_type_decl() {
    auto start = curtok_->loc;
    advance();  // 'type'
    check_curtok();
    auto* id = std::get_if<tk::id>(&peek());
    if (!id)
      fail({curtok_->loc, "B009", "expected a type name after 'type', found " + found_text(curtok_),
            "write type name = C1 of T1 | C2 of T2 | ..."});
    auto name = id->name;
    advance();
    expect<tk::op_eq>("'='");
    sema_.declare_type(start, name);
    module_type_decl decl;
    decl.declaring_path = self_path_;
    decl.name = std::string{name};
    for (;;) {
      check_curtok();
      auto* cid = std::get_if<tk::id>(&peek());
      if (!cid)
        fail({curtok_->loc, "B009", "expected a constructor name, found " + found_text(curtok_),
              "write type name = C1 of T1 | C2 of T2 | ..."});
      auto cname = cid->name;
      auto cname_loc = curtok_->loc;
      advance();
      ast::type payload = ast::type{ast::type_unit{}};  // nullary
      if (curtok_.has_value() && std::holds_alternative<tk::kw_of>(curtok_->t)) {
        advance();
        payload = parse_type();
      }
      // add_ctor copies the payload; keep one for the module's decl list
      sema_.add_ctor(cname_loc, name, cname, payload);
      decl.ctors.push_back(ast::sum_ctor{std::string{cname}, std::move(payload)});
      if (curtok_.has_value() && std::holds_alternative<tk::op_bar>(curtok_->t)) {
        advance();
        continue;
      }
      break;
    }
    types_.push_back(std::move(decl));
  }
  parsed parse_term() {
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
      if (is_start_of_expr(peek()) && prec_appl > precedence) {
        auto rhs = parse_expr(prec_appl);
        if (auto fty = sema_.type_of(left.term); fty.has_value()) {
          auto* arrow = std::get_if<ast::type_arrow>(&*fty);
          if (!arrow)
            fail({left.loc, "C002", "cannot apply value of type '" + type_name(*fty) + "'",
                  "only functions can be applied"});
          if (auto aty = sema_.type_of(rhs.term); aty.has_value() && !same_type(*arrow->from, *aty))
            fail({rhs.loc, "C003",
                  "cannot pass value of type '" + type_name(*aty) + "' to parameter of type '" +
                      type_name(*arrow->from) + "'",
                  "pass an argument of the parameter type"});
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
                          [this](tk::kw_let) { return parse_let(nullptr); },
                          [this](tk::kw_if) { return parse_if(); },
                          [this](tk::kw_match) { return parse_match(); },
                          [this](tk::kw_fix) { return parse_fix(); },
                          [this](tk::kw_import) { return parse_import(); },
                          [this](tk::kw_export) { return parse_export(); },
                          [this](tk::lparen) { return parse_tuple_literal(); },
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
                            if (auto info = sema_.lookup_ctor(id.name); info.has_value())
                              return parse_ctor(id, *info, loc);
                            auto idx = sema_.resolve_binding_index(loc, id.name);
                            // C026: a def may only reference the module's chain or its own binders
                            if (in_export_def_) {
                              size_t pos = sema_.binding_count() - 1 - static_cast<size_t>(idx);
                              if (export_m_ <= pos && pos < export_B_)
                                fail({loc, "C026",
                                      "exported definition references '" + std::string{id.name} +
                                          "', which is bound outside the module scope",
                                      "an exported definition may only reference the module's imports and "
                                      "earlier exports"});
                            }
                            return {ast::term{ast::var{idx}}, loc};
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
  // constructor application: Zero (nullary) or Num 5 / Add (e1, e2)
  parsed parse_ctor(tk::id id, ctor_info info, src_range loc) {
    auto* ty = make_type(actx_, ast::type{ast::type_ref{info.type_name}});
    if (std::holds_alternative<ast::type_unit>(info.payload_ty))
      return {ast::term{ast::ctor{.payload = nullptr, .tag = info.tag, .ty = ty, .name = std::string{id.name}}}, loc};
    auto arg = parse_expr(prec_appl);
    // type_of cannot fail here: every failure mode is rejected earlier during parsing
    auto aty = *sema_.type_of(arg.term);
    if (!same_type(aty, info.payload_ty))
      fail({arg.loc, "C024",
            "cannot construct '" + std::string{id.name} + "' with a value of type '" + type_name(aty) +
                "', expected '" + type_name(info.payload_ty) + "'",
            "pass a value of the constructor's payload type"});
    return {
        ast::term{ast::ctor{
            .payload = make_term(actx_, std::move(arg.term)), .tag = info.tag, .ty = ty, .name = std::string{id.name}}},
        {loc.begin, arg.loc.end}};
  }
  // () is the unit literal; (e) is just e; (e1, e2, ...) is a product literal.
  // Element types are inferred — type_of cannot fail here: every failure mode is
  // rejected earlier during parsing.
  parsed parse_tuple_literal() {
    auto start = curtok_->loc;
    advance();
    if (curtok_.has_value() && std::holds_alternative<tk::rparen>(curtok_->t)) {
      auto close = curtok_->loc;
      advance();
      return {ast::term{ast::li_unit{}}, {start.begin, close.end}};
    }
    auto first = parse_expr(0);
    if (!(curtok_.has_value() && std::holds_alternative<tk::op_comma>(curtok_->t))) {
      expect<tk::rparen>("')'");
      return first;
    }
    ast::tup tup;
    tup.fields.push_back(make_term(actx_, std::move(first.term)));
    while (curtok_.has_value() && std::holds_alternative<tk::op_comma>(curtok_->t)) {
      advance();
      auto el = parse_expr(0);
      tup.fields.push_back(make_term(actx_, std::move(el.term)));
    }
    auto close = curtok_.has_value() ? curtok_->loc : src_range{0, 0};
    expect<tk::rparen>("')'");
    return {ast::term{std::move(tup)}, {start.begin, close.end}};
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
                               [this](tk::lparen) -> ast::type { return parse_product_type(); },
                               [this](tk::id id) -> ast::type {
                                 auto loc = curtok_->loc;
                                 advance();
                                 if (!sema_.is_declared_type(id.name))
                                   fail({loc, "C011", "unknown type '" + std::string{id.name} + "'",
                                         "declare it with 'type name = C1 of T1 | ...' before use"});
                                 return ast::type{ast::type_ref{std::string{id.name}}};
                               },
                               [this](auto) -> ast::type {
                                 fail({curtok_->loc, "B002", "expected a type, found " + found_text(curtok_),
                                       "types are int, bool, string, (), (T, T, ...), declared names and T -> S"});
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
  // () is the unit type; (T) is just T; (T1, T2, ...) is a product type.
  ast::type parse_product_type() {
    advance();
    if (curtok_.has_value() && std::holds_alternative<tk::rparen>(curtok_->t)) {
      advance();
      return ast::type{ast::type_unit{}};
    }
    auto first = parse_type();
    if (!(curtok_.has_value() && std::holds_alternative<tk::op_comma>(curtok_->t))) {
      expect<tk::rparen>("')'");
      return first;
    }
    ast::type_prod prod;
    prod.fields.push_back(make_type(actx_, std::move(first)));
    while (curtok_.has_value() && std::holds_alternative<tk::op_comma>(curtok_->t)) {
      advance();
      auto el = parse_type();
      prod.fields.push_back(make_type(actx_, std::move(el)));
    }
    expect<tk::rparen>("')'");
    return ast::type{std::move(prod)};
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
    auto name_loc = curtok_->loc;
    advance();
    if (sema_.lookup_ctor(name).has_value())
      fail({name_loc, "C025", "cannot bind '" + std::string{name} + "': it is a constructor name",
            "use a different name"});
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
  // match e with C1 x . e1 | C2 . e2 | C3 (a, b) . e3
  // The scrutinee must have a declared variant type; every constructor must appear
  // exactly once. Each arm's body is desugared into a closure over the runtime
  // payload, which the machine applies to the constructor's payload.
  parsed parse_match() {
    auto start = curtok_->loc;
    advance();  // 'match'
    auto scrutinee = parse_expr(0);
    expect<tk::kw_with>("'with'");
    auto sty = sema_.type_of(scrutinee.term);
    auto* ref = sty.has_value() ? std::get_if<ast::type_ref>(&*sty) : nullptr;
    if (!ref)
      fail({scrutinee.loc, "C021",
            "match requires a variant value, found '" +
                (sty.has_value() ? type_name(*sty) : std::string{"an unknown type"}) + "'",
            "match over a value of a declared variant type"});
    auto* ctors = sema_.lookup_type(ref->name);
    if (!ctors)
      fail({scrutinee.loc, "C021", "match requires a variant value, found '" + type_name(*sty) + "'",
            "match over a value of a declared variant type"});
    std::vector<bool> used(ctors->size(), false);
    // arms are stored by constructor tag (the machine dispatches arms[tag]);
    // exhaustiveness guarantees every slot is filled regardless of source order
    std::vector<ast::case_arm> arms(ctors->size());
    std::optional<ast::type> result_ty;
    src_range last_loc = start;
    for (;;) {
      check_curtok();
      auto* cid = std::get_if<tk::id>(&peek());
      if (!cid)
        fail({curtok_->loc, "B005", "expected a constructor name, found " + found_text(curtok_),
              "write C x . body or C (x, y) . body"});
      auto cname = cid->name;
      auto cname_loc = curtok_->loc;
      advance();
      auto info = sema_.lookup_ctor(cname);
      if (!info.has_value() || info->type_name != ref->name)
        fail({cname_loc, "C020", "unknown constructor '" + std::string{cname} + "' for type '" + ref->name + "'",
              "use a constructor of '" + ref->name + "'"});
      if (used[info->tag])
        fail({cname_loc, "C022", "constructor '" + std::string{cname} + "' appears more than once in this match",
              "each constructor may appear at most once"});
      used[info->tag] = true;

      // payload pattern: C | C x | C (x, y, ...)
      std::vector<std::string> names;
      std::vector<ast::type> elem_tys;
      std::vector<ast::term> field_terms;  // $t.field(k), built while bindings are pushed
      bool use_temp = false;
      if (curtok_.has_value() && std::holds_alternative<tk::lparen>(curtok_->t)) {
        advance();  // '('
        for (;;) {
          check_curtok();
          auto* pid = std::get_if<tk::id>(&peek());
          if (!pid)
            fail({curtok_->loc, "B005", "expected a binding name, found " + found_text(curtok_),
                  "write C (x, y) . body"});
          auto pname = pid->name;
          auto pname_loc = curtok_->loc;
          advance();
          if (sema_.lookup_ctor(pname).has_value())
            fail({pname_loc, "C025", "cannot bind '" + std::string{pname} + "': it is a constructor name",
                  "use a different name"});
          for (const auto& n : names)
            if (n == pname)
              fail({pname_loc, "C018", "duplicate name '" + std::string{pname} + "' in the binding pattern",
                    "use different names"});
          names.emplace_back(std::string{pname});
          if (curtok_.has_value() && std::holds_alternative<tk::op_comma>(curtok_->t)) {
            advance();
            continue;
          }
          expect<tk::rparen>("')'");
          break;
        }
        if (names.size() == 1) {
          elem_tys.push_back(info->payload_ty);  // (x) is just a single binding
        } else {
          use_temp = true;
          auto* prod = std::get_if<ast::type_prod>(&info->payload_ty);
          if (!prod || prod->fields.size() != names.size())
            fail({cname_loc, "C022",
                  "pattern binds " + std::to_string(names.size()) + " name(s) but '" + std::string{cname} +
                      "' has payload '" + type_name(info->payload_ty) + "'",
                  "bind one name per payload element"});
          for (const auto& t : prod->fields) elem_tys.push_back(*t);
        }
      } else if (curtok_.has_value() && std::holds_alternative<tk::id>(curtok_->t)) {
        auto pname = std::get<tk::id>(curtok_->t).name;
        auto pname_loc = curtok_->loc;
        advance();
        if (sema_.lookup_ctor(pname).has_value())
          fail({pname_loc, "C025", "cannot bind '" + std::string{pname} + "': it is a constructor name",
                "use a different name"});
        names.emplace_back(std::string{pname});
        elem_tys.push_back(info->payload_ty);
      }
      expect<tk::op_dot>("'.'");

      // push bindings, parse the body
      if (use_temp) sema_.push_binding("$t", info->payload_ty);
      for (size_t k = 0; k < names.size(); ++k) {
        if (use_temp) {
          auto idx = sema_.resolve_binding_index(cname_loc, "$t");
          field_terms.push_back(ast::term{ast::field{.base = make_term(actx_, ast::term{ast::var{idx}}), .index = k}});
        }
        sema_.push_binding(names[k], elem_tys[k]);
      }
      auto body = parse_expr(0);
      for (size_t k = 0; k < names.size(); ++k) sema_.pop_binding();
      if (use_temp) sema_.pop_binding();

      // desugar: closure over the payload — (\$t : payload . ...) the machine applies
      ast::term inner = std::move(body.term);
      if (use_temp)
        for (size_t k = names.size(); k-- > 0;)
          inner = ast::term{ast::appl{
              .func = make_term(
                  actx_, ast::term{ast::abst{.param_type = elem_tys[k], .body = make_term(actx_, std::move(inner))}}),
              .arg = make_term(actx_, std::move(field_terms[k]))}};
      auto arm_body = make_term(
          actx_, ast::term{ast::abst{.param_type = info->payload_ty, .body = make_term(actx_, std::move(inner))}});
      // arm result types must agree; type_of cannot fail here (the arm fn is a lambda)
      auto arm_ty = *sema_.type_of(*arm_body);
      auto* arm_arrow = std::get_if<ast::type_arrow>(&arm_ty);
      if (result_ty.has_value() && !same_type(*result_ty, *arm_arrow->to))
        fail({body.loc, "C023",
              "match arms must have the same type: '" + type_name(*result_ty) + "' vs '" + type_name(*arm_arrow->to) +
                  "'",
              "make every arm produce the same type"});
      if (!result_ty.has_value()) result_ty = *arm_arrow->to;
      arms[info->tag] = ast::case_arm{.payload_ty = make_type(actx_, info->payload_ty), .body = arm_body};
      last_loc = body.loc;

      if (curtok_.has_value() && std::holds_alternative<tk::op_bar>(curtok_->t)) {
        advance();
        continue;
      }
      break;
    }
    for (size_t i = 0; i < used.size(); ++i)
      if (!used[i])
        fail({start, "C022", "match does not handle constructor '" + std::string{(*ctors)[i].name} + "'",
              "handle every constructor of the type"});
    return {ast::term{ast::case_{.scrutinee = make_term(actx_, std::move(scrutinee.term)), .arms = std::move(arms)}},
            {start.begin, last_loc.end}};
  }
  // exported != nullptr: report the binding back for the module skeleton
  parsed parse_let(module_entry* exported) {
    auto start = curtok_->loc;
    advance();
    check_curtok();
    if (std::holds_alternative<tk::lbrace>(curtok_->t)) {
      if (exported)
        fail({curtok_->loc, "C026", "cannot export a structured binding",
              "export each name with its own 'export let'"});
      return parse_let_binding(start);
    }

    auto* id = std::get_if<tk::id>(&peek());
    if (!id)
      fail({curtok_->loc, "B004", "expected an identifier after 'let', found " + found_text(curtok_),
            "write let name : type = expr in body"});
    auto name = id->name;
    auto name_loc = curtok_->loc;
    advance();
    if (exported) exported->name = std::string{name};
    if (sema_.lookup_ctor(name).has_value())
      fail({name_loc, "C025", "cannot bind '" + std::string{name} + "': it is a constructor name",
            "use a different name"});
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
    if (annotated && !same_type(ann, ty))
      fail({bound_expr.loc, "C015",
            "cannot annotate a value of type '" + type_name(ty) + "' as '" + type_name(ann) + "'",
            "the value must have the annotated type"});
    if (!annotated) ann = std::move(ty);
    auto* def_node = make_term(actx_, std::move(bound_expr.term));
    if (exported) {
      // record the completed def; the body (next chain level) may hold nested exports
      exported->def = def_node;
      in_export_def_ = false;
      skeleton_.push_back(std::move(*exported));
    }

    expect<tk::kw_in>("'in'");
    sema_.push_binding(name, ann);
    auto body = parse_expr(0);
    sema_.pop_binding();
    // the innermost expression of an export chain is ignored on import; it must be ()
    if (exported) {
      if (auto bty = sema_.type_of(body.term); bty.has_value() && !std::holds_alternative<ast::type_unit>(*bty))
        fail({body.loc, "C027",
              "the body of an exported definition must be '()', got '" + type_name(*bty) + "'",
              "the innermost expression of an export chain is ignored on import; end it with ()"});
    }

    return {ast::term{ast::appl{
                .func = make_term(actx_, ast::term{ast::abst{.param_type = std::move(ann),
                                                             .body = make_term(actx_, std::move(body.term))}}),
                .arg = def_node}},
            {start.begin, body.loc.end}};
  }
  // export let ...: like `let`, and the binding joins the module's skeleton
  parsed parse_export() {
    auto start = curtok_->loc;
    advance();  // 'export'
    check_curtok();
    if (in_export_def_)
      fail({start, "C026", "cannot nest an export inside an exported definition",
            "an export must be at the module's top level"});
    if (!std::holds_alternative<tk::kw_let>(curtok_->t))
      fail({curtok_->loc, "B005", "expected 'let' after 'export', found " + found_text(curtok_),
            "write export let name : type = expr in body"});
    module_entry entry;
    in_export_def_ = true;
    export_m_ = skeleton_.size();
    export_B_ = sema_.binding_count();
    return parse_let(&entry);
  }
  // import "path" in body: parse the module unoptimized (importers may use
  // its exports), merge its types, and lift its skeleton as binders
  parsed parse_import() {
    auto start = curtok_->loc;
    advance();  // 'import'
    check_curtok();
    auto* s = std::get_if<tk::li_str>(&peek());
    if (!s)
      fail({curtok_->loc, "B005", "expected an imported file path, found " + found_text(curtok_),
            "write import \"file.luca\" in body"});
    auto path_loc = curtok_->loc;
    std::string raw_path{s->raw};
    advance();
    expect<tk::kw_in>("'in'");

    // relative to this file's directory (CWD when unknown)
    auto base = self_path_.empty() ? std::filesystem::path{} : std::filesystem::path{self_path_}.parent_path();
    auto resolved = base / std::filesystem::path{raw_path};
    auto canonical = canonical_path(resolved);
    for (const auto& in_progress : import_stack_)
      if (same_path(in_progress, canonical)) {
        std::string chain;
        for (const auto& p : import_stack_) chain += p + " -> ";
        chain += canonical;
        fail({path_loc, "B010", "import cycle detected: " + chain,
              "a module cannot import itself, directly or transitively"});
      }

    // stream exceptions: missing → open throws, directory/unreadable → read throws (B008)
    std::ifstream ifs;
    ifs.exceptions(std::ios::failbit | std::ios::badbit);
    std::string content;
    try {
      ifs.open(resolved);
      content.assign(std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{});
    } catch (const std::ios_base::failure&) {
      fail({path_loc, "B008", "cannot open imported file '" + raw_path + "'",
            "check that the file exists next to the importing file"});
    }

    import_stack_.push_back(canonical);
    module_result imported;
    try {
      imported = parser{content, resolved.generic_string(), import_stack_}.run_pass();
    } catch (parse_err& e) {
      import_stack_.pop_back();
      if (e.src.empty()) {  // deepest wins: attach the file the diagnostic refers to
        e.src = std::move(content);
        e.filename = raw_path;
      }
      throw;
    }
    import_stack_.pop_back();

    // retain the module's arenas (its own and its imports'): the lifted defs live in them
    actx_.deps.push_back(std::move(imported.actx.arena));
    for (auto& dep : imported.actx.deps) actx_.deps.push_back(std::move(dep));

    // merge the transitive type declarations; dedupe by provenance (diamonds)
    for (const auto& td : imported.types) {
      if (sema_.is_declared_type(td.name)) {
        auto prov = sema_.type_provenance(td.name);
        if (!prov.empty() && same_path(prov, td.declaring_path)) continue;  // diamond
        fail({path_loc, "C010",
              "importing '" + raw_path + "' introduces type '" + td.name +
                  "', which conflicts with an existing declaration",
              "the name is declared locally or in another imported module"});
      }
      sema_.declare_type(path_loc, td.name);
      sema_.set_type_provenance(td.name, td.declaring_path);
      for (const auto& c : td.ctors) {
        if (sema_.lookup_ctor(c.name).has_value())
          fail({path_loc, "C019",
                "importing '" + raw_path + "' introduces constructor '" + c.name +
                    "', which conflicts with an existing declaration",
                "constructor names are globally unique"});
        sema_.add_ctor(path_loc, td.name, c.name, c.payload_ty);
      }
    }
    types_.insert(types_.end(), std::make_move_iterator(imported.types.begin()),
                  std::make_move_iterator(imported.types.end()));

    // lift: type each def before pushing its binder (defs may reference earlier ones)
    std::vector<ast::type> slot_tys;
    slot_tys.reserve(imported.skeleton.size());
    for (const auto& entry : imported.skeleton) {
      auto ty = sema_.type_of(*entry.def);
      if (!ty.has_value()) throw std::logic_error{"imported definition cannot be typed"};
      slot_tys.push_back(std::move(*ty));
      sema_.push_binding(entry.name, slot_tys.back());
    }
    // join the ambient chain before the body parses; imports in export defs stay local
    if (!in_export_def_)
      skeleton_.insert(skeleton_.end(), imported.skeleton.begin(), imported.skeleton.end());
    auto body = parse_expr(0);
    for (size_t i = 0; i < imported.skeleton.size(); ++i) sema_.pop_binding();

    ast::term chain = std::move(body.term);
    for (size_t i = imported.skeleton.size(); i-- > 0;)
      chain = ast::term{ast::appl{
          .func = make_term(actx_, ast::term{ast::abst{.param_type = slot_tys[i],
                                                       .body = make_term(actx_, std::move(chain))}}),
          .arg = imported.skeleton[i].def}};
    return {std::move(chain), {start.begin, body.loc.end}};
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
      if (sema_.lookup_ctor(name).has_value())
        fail(
            {loc, "C025", "cannot bind '" + std::string{name} + "': it is a constructor name", "use a different name"});
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
    auto* prod = std::get_if<ast::type_prod>(&bound_ty);
    if (!prod)
      fail({bound_expr.loc, "C016", "cannot bind names from a value of type '" + type_name(bound_ty) + "'",
            "structured binding requires a product value"});
    if (prod->fields.size() != names.size())
      fail({bound_expr.loc, "C017",
            "cannot bind " + std::to_string(names.size()) + " name(s) to a product of " +
                std::to_string(prod->fields.size()) + " element(s)",
            "the number of names must match the product's elements"});

    expect<tk::kw_in>("'in'");
    // bindings: [$t, a, b]; the k-th field access is the arg of the (k+1)-th lambda,
    // evaluated with k binders above $t, so its de Bruijn index is k
    sema_.push_binding("$t", bound_ty);
    std::vector<ast::term> field_terms;
    field_terms.reserve(names.size());
    for (size_t k = 0; k < names.size(); ++k) {
      auto idx = sema_.resolve_binding_index(bound_expr.loc, "$t");
      field_terms.push_back(ast::term{ast::field{.base = make_term(actx_, ast::term{ast::var{idx}}), .index = k}});
      sema_.push_binding(names[k].first, *prod->fields[k]);
    }
    auto body = parse_expr(0);
    for (size_t k = 0; k < names.size(); ++k) sema_.pop_binding();
    sema_.pop_binding();

    ast::term inner = std::move(body.term);
    for (size_t k = names.size(); k-- > 0;)
      inner = ast::term{ast::appl{
          .func = make_term(
              actx_, ast::term{ast::abst{.param_type = *prod->fields[k], .body = make_term(actx_, std::move(inner))}}),
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
  const std::string& source_;
  std::string self_path_;                  // canonical ("" = unknown)
  std::vector<std::string>& import_stack_;  // canonical paths in progress (cycle detection)
  bool in_export_def_ = false;
  size_t export_m_ = 0;                    // C026: skeleton size at the export def's start
  size_t export_B_ = 0;                    // C026: binding count at the export def's start
  std::vector<module_entry> skeleton_;     // ambient binding chain, outermost first
  std::vector<module_type_decl> types_;    // transitive type declarations

  lexer lex_;
  ast::context actx_;
  sema sema_;
  lex_result curtok_;
  lex_result nextok_;
};

}  // namespace

parse_result parse(const std::string& source, const std::string& path) {
  // seed the import stack with this file so a self-import is a detectable cycle
  std::vector<std::string> import_stack;
  if (!path.empty()) import_stack.push_back(canonical_path(path));
  auto r = parser{source, path, import_stack}.run_pass();
  // pass 2: the final program only — imported modules stay unoptimized, since
  // their exports may be referenced from other TUs
  auto* root = optimize_node(r.term);
  if (root != &r.term) r.term = std::move(*root);
  return {std::move(r.term), std::move(r.actx)};
}
