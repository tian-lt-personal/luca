// std
#include <limits>
#include <memory_resource>
#include <stdexcept>
// luca
#include "mp.hpp"
#include "sema.hpp"

namespace {

template <class T>
ast::type* make_type(ast::context& ctx, T value) {
  std::pmr::polymorphic_allocator<ast::type> alloc{ctx.arena.get()};
  return alloc.new_object<ast::type>(std::move(value));
}

}  // namespace

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

std::optional<ast::type> sema::type_of(const ast::term& t) noexcept {
  return std::visit(overloaded{
    [](const ast::li_int&) -> std::optional<ast::type> { return ast::type{ast::type_int{}}; },
    [](const ast::li_bool&) -> std::optional<ast::type> { return ast::type{ast::type_bool{}}; },
    [this](const ast::abst& a) -> std::optional<ast::type> {
      auto body_ty = type_of(*a.body);
      if (!body_ty.has_value()) return std::nullopt;
      return ast::type{ast::type_arrow{.from = make_type(*ctx_, a.param_type),
                                       .to = make_type(*ctx_, *std::move(body_ty))}};
    },
    [this](const ast::appl& a) -> std::optional<ast::type> {
      auto func_ty = type_of(*a.func);
      if (!func_ty.has_value()) return std::nullopt;
      auto* arrow = std::get_if<ast::type_arrow>(&*func_ty);
      return arrow ? std::optional<ast::type>{*arrow->to} : std::nullopt;
    },
    [this](const ast::binop& b) -> std::optional<ast::type> {
      auto lty = type_of(*b.left);
      auto rty = type_of(*b.right);
      if (lty.has_value() && rty.has_value() && std::holds_alternative<ast::type_int>(*lty) &&
          std::holds_alternative<ast::type_int>(*rty))
        return ast::type{ast::type_int{}};
      return std::nullopt;
    },
    [this](const ast::ifexpr& ie) -> std::optional<ast::type> {
      auto then_ty = type_of(*ie.then);
      auto else_ty = type_of(*ie.els);
      if (then_ty.has_value() && else_ty.has_value() && then_ty->index() == else_ty->index()) return then_ty;
      return std::nullopt;
    },
    [](const auto&) -> std::optional<ast::type> { return std::nullopt; },
  }, t);
}
