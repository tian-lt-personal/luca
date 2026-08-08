#pragma once

// std
#include <expected>
#include <memory>
#include <memory_resource>
#include <string>
#include <variant>
// luca
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
struct type : std::variant<type_unit, type_int, type_bool, type_string, type_arrow> {};

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
struct term : std::variant<var, abst, appl, binop, ifexpr, li_int, li_bool> {};

struct context {
  std::unique_ptr<std::pmr::monotonic_buffer_resource> arena;
};

}  // namespace ast

struct parse_err_unknown {};
using parse_err = std::variant<parse_err_unknown, lex_err>;

using parse_result = std::expected<std::pair<ast::term, ast::context>, parse_err>;
parse_result parse(const std::string& source);
