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
struct rec_field {
  std::string name;
  const ast::type* ty;
};
struct type_rec {
  std::string name;               // "" = anonymous
  std::vector<rec_field> fields;  // {x:int, y:bool}
};
struct type : std::variant<type_unit, type_int, type_bool, type_string, type_arrow, type_rec> {};

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
// tuple literals persist in the AST (unlike tokens), so names are owning
struct tup_field {
  std::string name;
  const ast::type* ann;  // explicit field annotation; nullptr = infer
  ast::term* value;
};
struct tup {
  std::vector<tup_field> fields;
};
struct field {
  ast::term* base;
  size_t index;  // resolved against the record type at parse time
};
struct term : std::variant<var, abst, appl, binop, ifexpr, fix, li_int, li_bool, tup, field> {};

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
