#pragma once

// std
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <utility>
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
  unsupported,
  unsafe,
  runtime_failure,
};

struct eval_err : std::runtime_error {
  eval_status status;
  explicit eval_err(eval_status status, std::string message) : std::runtime_error{std::move(message)}, status{status} {}
};

struct eval_result {
  std::variant<value, ast::term> result;
  std::unique_ptr<std::pmr::monotonic_buffer_resource> arena;
};

eval_result evaluate(const ast::term& term, eval_strategy strategy);
