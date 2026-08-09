#pragma once

// std
#include <exception>
#include <memory>
#include <memory_resource>
#include <optional>
#include <stdexcept>
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
  std::optional<int> index;  // de bruijn index
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

struct parse_err : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct parse_err_unknown : parse_err {
  parse_err_unknown() : parse_err{"unknown parse error"} {}
};

struct parse_err_with_lexer_err : parse_err {
  lex_err err;
  explicit parse_err_with_lexer_err(lex_err e) : parse_err{"lexer error"}, err{std::move(e)} {}
};

using parse_result = std::pair<ast::term, ast::context>;
parse_result parse(const std::string& source);
