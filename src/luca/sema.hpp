#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class sema {
 public:
  std::optional<int> resolve_binding_index(std::string_view name) const;
  void push_binding(std::string_view name);
  void pop_binding();

 private:
  std::vector<std::string_view> bindings_;
};
