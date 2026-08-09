// std
#include <limits>
#include <stdexcept>
// luca
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
