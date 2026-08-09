#pragma once

// std
#include <variant>
#include <vector>
// luca
#include "parser.hpp"

struct closure;

using value = std::variant<std::monostate,  // unit
                           int,             // int
                           bool,            // bool
                           closure*>;       // arrow type (internal only)

struct closure {
  const ast::abst* abst;
  std::vector<value> captured_env;
};

value eval(const ast::term& t);
