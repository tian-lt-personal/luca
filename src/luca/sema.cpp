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
          [](const ast::li_unit&) -> std::optional<ast::type> { return ast::type{ast::type_unit{}}; },
          [&](const ast::tup& t) -> std::optional<ast::type> {
            ast::type_prod prod;
            for (const auto& f : t.fields) {
              auto fty = type_of_impl(*f, param_types, binding_types, ctx);
              if (!fty.has_value()) return std::nullopt;
              prod.fields.push_back(make_type(ctx, *std::move(fty)));
            }
            return ast::type{std::move(prod)};
          },
          [&](const ast::field& f) -> std::optional<ast::type> {
            auto base_ty = type_of_impl(*f.base, param_types, binding_types, ctx);
            if (!base_ty.has_value()) return std::nullopt;
            auto* prod = std::get_if<ast::type_prod>(&*base_ty);
            if (!prod || f.index >= prod->fields.size()) return std::nullopt;
            return std::optional<ast::type>{*prod->fields[f.index]};
          },
          [&](const ast::ctor& c) -> std::optional<ast::type> { return std::optional<ast::type>{*c.ty}; },
          [&](const ast::case_& cs) -> std::optional<ast::type> {
            auto sty = type_of_impl(*cs.scrutinee, param_types, binding_types, ctx);
            if (!sty.has_value() || !std::holds_alternative<ast::type_ref>(*sty)) return std::nullopt;
            std::optional<ast::type> result;
            for (const auto& arm : cs.arms) {
              auto fty = type_of_impl(*arm.body, param_types, binding_types, ctx);
              if (!fty.has_value()) return std::nullopt;
              auto* arrow = std::get_if<ast::type_arrow>(&*fty);
              if (!arrow) return std::nullopt;
              if (!result.has_value())
                result = std::optional<ast::type>{*arrow->to};
              else if (!same_type(*result, *arrow->to))
                return std::nullopt;
            }
            return result;
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
  if (const auto *la = std::get_if<ast::type_prod>(&a), *ra = std::get_if<ast::type_prod>(&b); la && ra) {
    if (la->fields.size() != ra->fields.size()) return false;
    for (size_t i = 0; i < la->fields.size(); ++i)
      if (!same_type(*la->fields[i], *ra->fields[i])) return false;
  }
  if (const auto *la = std::get_if<ast::type_ref>(&a), *ra = std::get_if<ast::type_ref>(&b); la && ra)
    return la->name == ra->name;  // nominal: same declared name → same type
  return true;
}

int sema::resolve_binding_index(src_range loc, std::string_view name) const {
  for (size_t i = 0; i < bindings_.size(); ++i) {
    if (bindings_[bindings_.size() - 1 - i] == name) {
      if (i > std::numeric_limits<int>::max()) throw std::logic_error{"de bruijn index is too large."};
      return static_cast<int>(i);
    }
  }
  throw sema_err{{loc, "C001", "unbound identifier '" + std::string{name} + "'",
                  "bind it with a lambda parameter or a let expression"}};
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
  // seed the param stack with the parser's binding types: a variable's de Bruijn
  // index counts the full stack (outer bindings + the term's own lambdas), so a
  // term that references an outer binding through an in-term lambda or a match
  // arm resolves correctly instead of falling off the end
  std::vector<ast::type> param_types = binding_types_;
  return type_of_impl(t, param_types, binding_types_, *ctx_);
}

void sema::declare_type(src_range loc, std::string_view name) {
  if (!types_.emplace(std::string{name}, std::vector<ast::sum_ctor>{}).second)
    throw sema_err{{loc, "C010", "duplicate type declaration '" + std::string{name} + "'", "use a different name"}};
}

void sema::add_ctor(src_range loc, std::string_view type_name, std::string_view cname, ast::type payload) {
  if (ctors_.contains(std::string{cname}))
    throw sema_err{{loc, "C019", "duplicate constructor '" + std::string{cname} + "'", "use a different name"}};
  auto it = types_.find(std::string{type_name});
  if (it == types_.end()) throw std::logic_error{"add_ctor: unknown type '" + std::string{type_name} + "'"};
  auto& ctors = it->second;
  ctors.push_back(ast::sum_ctor{std::string{cname}, std::move(payload)});
  ctors_.emplace(std::string{cname}, ctor_info{std::string{type_name}, ctors.size() - 1, ctors.back().payload_ty});
}

bool sema::is_declared_type(std::string_view name) const { return types_.contains(std::string{name}); }

std::string sema::type_provenance(std::string_view name) const {
  auto it = type_prov_.find(std::string{name});
  return it == type_prov_.end() ? std::string{} : it->second;
}

void sema::set_type_provenance(std::string name, std::string from_path) {
  type_prov_.emplace(std::move(name), std::move(from_path));
}

std::optional<ctor_info> sema::lookup_ctor(std::string_view name) const {
  auto it = ctors_.find(std::string{name});
  if (it == ctors_.end()) return std::nullopt;
  return it->second;
}

const std::vector<ast::sum_ctor>* sema::lookup_type(std::string_view name) const {
  auto it = types_.find(std::string{name});
  return it == types_.end() ? nullptr : &it->second;
}
