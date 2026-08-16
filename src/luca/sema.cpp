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

std::optional<ast::type> type_of_impl(const ast::term& t, std::vector<ast::type>& param_types,
                                      const std::vector<ast::type>& binding_types, ast::context& ctx) noexcept {
  return std::visit(
      overloaded{
          [](const ast::li_int&) -> std::optional<ast::type> { return ast::type{ast::type_int{}}; },
          [](const ast::li_bool&) -> std::optional<ast::type> { return ast::type{ast::type_bool{}}; },
          [&](const ast::var& v) -> std::optional<ast::type> {
            if (v.index >= 0) {
              if (static_cast<size_t>(v.index) < param_types.size())
                return param_types[param_types.size() - 1 - v.index];
              if (static_cast<size_t>(v.index) < binding_types.size())
                return binding_types[binding_types.size() - 1 - v.index];
            }
            return std::nullopt;
          },
          [&](const ast::abst& a) -> std::optional<ast::type> {
            param_types.push_back(a.param_type);
            auto body_ty = type_of_impl(*a.body, param_types, binding_types, ctx);
            param_types.pop_back();
            if (!body_ty.has_value()) return std::nullopt;
            return ast::type{
                ast::type_arrow{.from = make_type(ctx, a.param_type), .to = make_type(ctx, *std::move(body_ty))}};
          },
          [&](const ast::appl& a) -> std::optional<ast::type> {
            auto func_ty = type_of_impl(*a.func, param_types, binding_types, ctx);
            if (!func_ty.has_value()) return std::nullopt;
            auto* arrow = std::get_if<ast::type_arrow>(&*func_ty);
            return arrow ? std::optional<ast::type>{*arrow->to} : std::nullopt;
          },
          [&](const ast::binop& b) -> std::optional<ast::type> {
            auto lty = type_of_impl(*b.left, param_types, binding_types, ctx);
            auto rty = type_of_impl(*b.right, param_types, binding_types, ctx);
            if (!lty.has_value() || !rty.has_value()) return std::nullopt;
            if (!std::holds_alternative<ast::type_int>(*lty) || !std::holds_alternative<ast::type_int>(*rty))
              return std::nullopt;
            bool is_cmp = std::holds_alternative<tk::op_eq>(b.op) || std::holds_alternative<tk::op_ne>(b.op) ||
                          std::holds_alternative<tk::op_gt>(b.op) || std::holds_alternative<tk::op_lt>(b.op);
            return is_cmp ? ast::type{ast::type_bool{}} : ast::type{ast::type_int{}};
          },
          [&](const ast::ifexpr& ie) -> std::optional<ast::type> {
            auto then_ty = type_of_impl(*ie.then, param_types, binding_types, ctx);
            auto else_ty = type_of_impl(*ie.els, param_types, binding_types, ctx);
            if (then_ty.has_value() && else_ty.has_value() && same_type(*then_ty, *else_ty)) return then_ty;
            return std::nullopt;
          },
          [&](const ast::fix& fx) -> std::optional<ast::type> {
            auto body_ty = type_of_impl(*fx.body, param_types, binding_types, ctx);
            if (!body_ty.has_value()) return std::nullopt;
            auto* arrow = std::get_if<ast::type_arrow>(&*body_ty);
            if (!arrow || !same_type(*arrow->from, *arrow->to)) return std::nullopt;
            return ast::type{*arrow->from};
          },
          [](const auto&) -> std::optional<ast::type> { return std::nullopt; },
      },
      t);
}

}  // namespace

bool same_type(const ast::type& a, const ast::type& b) noexcept {
  if (a.index() != b.index()) return false;
  if (const auto *la = std::get_if<ast::type_arrow>(&a), *ra = std::get_if<ast::type_arrow>(&b); la && ra)
    return same_type(*la->from, *ra->from) && same_type(*la->to, *ra->to);
  return true;
}

std::optional<int> sema::resolve_binding_index(std::string_view name) const {
  for (size_t i = 0; i < bindings_.size(); ++i) {
    if (bindings_[bindings_.size() - 1 - i] == name) {
      if (i > std::numeric_limits<int>::max()) throw std::logic_error{"de bruijn index is too large."};
      return static_cast<int>(i);
    }
  }
  return std::nullopt;
}

void sema::push_binding(std::string_view name, ast::type ty) {
  bindings_.emplace_back(name);
  binding_types_.emplace_back(std::move(ty));
}
void sema::pop_binding() {
  bindings_.pop_back();
  binding_types_.pop_back();
}

std::optional<ast::type> sema::type_of(const ast::term& t) noexcept {
  std::vector<ast::type> param_types;
  return type_of_impl(t, param_types, binding_types_, *ctx_);
}
