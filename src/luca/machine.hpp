#pragma once

// std
#include <memory>
#include <memory_resource>
#include <variant>
#include <vector>
// luca
#include "parser.hpp"

struct closure;
struct tuple_value;

using value = std::variant<std::monostate,  // unit
                           int,             // int
                           bool,            // bool
                           closure*,        // arrow type (internal only)
                           tuple_value*>;   // record type

struct closure {
  const ast::abst* abst;
  std::vector<value> captured_env;
};

struct tuple_value {
  std::vector<value> fields;
  std::vector<std::string> names;  // for printing only
};

struct eval_result {
  value v;
  std::unique_ptr<std::pmr::monotonic_buffer_resource> arena;
};

eval_result eval(const ast::term& t);
