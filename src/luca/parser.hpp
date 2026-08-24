#pragma once

// std
#include <exception>
#include <memory>
#include <memory_resource>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
// luca
#include "diag.hpp"
#include "lexer.hpp"

namespace ast {

struct type;
struct type_unit {};
struct type_int {};
struct type_bool {};
struct type_string {};
struct type_arrow {
  type* from;
  type* to;
};
struct type_prod {
  std::vector<const ast::type*> fields;  // (int, bool)
};
struct type_ref {
  std::string name;  // reference to a declared variant type (never expanded)
};
struct type : std::variant<type_unit, type_int, type_bool, type_string, type_arrow, type_prod, type_ref> {};

struct term;
struct var {
  int index;  // de bruijn index
};
struct abst {
  type param_type;
  term* body;
};
struct appl {
  term* func;
  term* arg;
};
struct li_int {
  int value;
};
struct li_bool {
  bool value;
};
struct li_unit {};  // ()
struct binop {
  token op;
  term* left;
  term* right;
};
struct ifexpr {
  term* cond;
  term* then;
  term* els;
};
struct fix {
  term* body;
};
struct tup {
  std::vector<ast::term*> fields;  // (1, true)
};
struct field {
  ast::term* base;
  size_t index;  // projection index; produced only by the binding desugar
};
struct ctor {
  ast::term* payload;   // null = nullary constructor
  size_t tag;           // constructor index in its type's declaration
  const ast::type* ty;  // the variant type (a type_ref)
  std::string name;     // constructor name, for printing
};
struct case_arm {
  const ast::type* payload_ty;
  ast::term* body;  // desugared closure over the payload: (\$t : payload . ...)
};
struct case_ {
  ast::term* scrutinee;
  std::vector<case_arm> arms;
};
struct term : std::variant<var, abst, appl, binop, ifexpr, fix, li_int, li_bool, li_unit, tup, field, ctor, case_> {};

// the sema-owned definition of one constructor of a declared variant type
struct sum_ctor {
  std::string name;
  ast::type payload_ty;  // type_unit for nullary constructors
};

struct context {
  std::unique_ptr<std::pmr::monotonic_buffer_resource> arena;
};

}  // namespace ast

struct parse_err : std::runtime_error {
  diagnostic diag;
  explicit parse_err(diagnostic d) : std::runtime_error{d.message}, diag{std::move(d)} {}
};

using parse_result = std::pair<ast::term, ast::context>;
parse_result parse(const std::string& source);
