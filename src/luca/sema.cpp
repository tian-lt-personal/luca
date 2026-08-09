// std
#include <limits>
#include <stdexcept>
// luca
#include "mp.hpp"
#include "sema.hpp"

std::optional<int> sema::resolve_binding_index(std::string_view name) const {
  for (size_t i = 0; i < bindings_.size(); ++i) {
    if (bindings_[bindings_.size() - 1 - i] == name) {
      if (i > std::numeric_limits<int>::max()) throw std::logic_error{"de bruijn index is too large."};
      return static_cast<int>(i);
    }
  }
  return std::nullopt;
}

void sema::push_binding(std::string_view name) { bindings_.emplace_back(std::move(name)); }
void sema::pop_binding() { bindings_.pop_back(); }

std::optional<ast::type> type_of(const ast::term& t) noexcept {
  return std::visit(overloaded{
    [](const ast::li_int&) -> std::optional<ast::type> { return ast::type{ast::type_int{}}; },
    [](const ast::li_bool&) -> std::optional<ast::type> { return ast::type{ast::type_bool{}}; },
    [](const ast::abst&) -> std::optional<ast::type> { return std::nullopt; },
    [](const auto&) -> std::optional<ast::type> { return std::nullopt; },
  }, t);
}
