#pragma once

// std
#include <memory>
#include <memory_resource>
#include <string>
#include <variant>
#include <vector>
// luca
#include "parser.hpp"

struct closure;
struct tuple_value;
struct sum_value;

using value = std::variant<std::monostate,  // unit
                           int,             // int
                           bool,            // bool
                           closure*,        // arrow type (internal only)
                           tuple_value*,    // product type
                           sum_value*>;     // variant type

struct tuple_value {
  std::vector<value> fields;
};

struct sum_value {
  std::string name;  // constructor name, for printing
  size_t tag;
  value payload;
};

enum class eval_strategy {
  runtime,
  compiletime,
  try_compiletime,
};

enum class eval_status {
  success,
  unsupported,
  unsafe,
  runtime_failure,
};

struct eval_result {
  eval_status status = eval_status::success;
  std::variant<std::monostate, value, ast::term> result;
  std::unique_ptr<std::pmr::monotonic_buffer_resource> arena;
  std::string message;
};

eval_result evaluate(const ast::term& term, eval_strategy strategy);
