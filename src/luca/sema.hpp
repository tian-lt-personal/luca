#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
// luca
#include "parser.hpp"

bool same_type(const ast::type& a, const ast::type& b) noexcept;

struct sema_err : parse_err {
  explicit sema_err(diagnostic d) : parse_err{std::move(d)} {}
};

struct ctor_info {
  std::string type_name;  // the variant type the constructor belongs to
  size_t tag;             // index in that type's constructor list
  ast::type payload_ty;   // type_unit for nullary constructors
};

class sema {
 public:
  explicit sema(ast::context& ctx) noexcept : ctx_{&ctx} {}

  int resolve_binding_index(src_range loc, std::string_view name) const;
  void push_binding(std::string_view name, ast::type ty);
  void pop_binding();
  size_t binding_count() const noexcept { return bindings_.size(); }

  std::optional<ast::type> type_of(const ast::term& t) noexcept;

  void declare_type(src_range loc, std::string_view name);
  void add_ctor(src_range loc, std::string_view type_name, std::string_view cname, ast::type payload);
  bool is_declared_type(std::string_view name) const;
  std::optional<ctor_info> lookup_ctor(std::string_view name) const;
  const std::vector<ast::sum_ctor>* lookup_type(std::string_view name) const;  // nullptr if unknown

  // declaring module of an imported type ("" when never imported)
  std::string type_provenance(std::string_view name) const;
  void set_type_provenance(std::string name, std::string from_path);

 private:
  ast::context* ctx_;
  std::vector<std::string_view> bindings_;
  std::vector<ast::type> binding_types_;
  std::map<std::string, std::vector<ast::sum_ctor>> types_;
  std::map<std::string, ctor_info> ctors_;
  std::map<std::string, std::string> type_prov_;
};
