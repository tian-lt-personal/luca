#pragma once

#include <optional>
#include <string_view>
#include <vector>
// luca
#include "parser.hpp"

class sema {
 public:
  explicit sema(ast::context& ctx) noexcept : ctx_{&ctx} {}

  std::optional<int> resolve_binding_index(std::string_view name) const;
  void push_binding(std::string_view name);
  void pop_binding();

  std::optional<ast::type> type_of(const ast::term& t) noexcept;

 private:
  ast::context* ctx_;
  std::vector<std::string_view> bindings_;
};
