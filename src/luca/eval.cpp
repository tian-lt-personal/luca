// std
#include <expected>
#include <limits>
#include <memory_resource>
#include <optional>
#include <string>
#include <utility>
// luca
#include "eval.hpp"
#include "mp.hpp"

struct closure {
  const ast::abst* abst;
  std::vector<value> captured_env;
};

namespace {

using primitive = std::variant<int, bool>;
using checked_int = std::expected<int, eval_status>;
using checked_primitive = std::expected<primitive, eval_status>;

checked_int checked_add(int left, int right) noexcept {
  const auto result = static_cast<long long>(left) + right;
  if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
    return std::unexpected(eval_status::unsafe);
  return static_cast<int>(result);
}

checked_int checked_sub(int left, int right) noexcept {
  const auto result = static_cast<long long>(left) - right;
  if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
    return std::unexpected(eval_status::unsafe);
  return static_cast<int>(result);
}

checked_int checked_mul(int left, int right) noexcept {
  const auto result = static_cast<long long>(left) * right;
  if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
    return std::unexpected(eval_status::unsafe);
  return static_cast<int>(result);
}

checked_int checked_div(int left, int right) noexcept {
  if (right == 0 || (left == std::numeric_limits<int>::min() && right == -1))
    return std::unexpected(eval_status::unsafe);
  return left / right;
}

checked_primitive apply_binop(const token& op, int left, int right) noexcept {
  return std::visit(overloaded{
                        [&](tk::op_plus) -> checked_primitive {
                          auto result = checked_add(left, right);
                          if (!result) return std::unexpected(result.error());
                          return primitive{*result};
                        },
                        [&](tk::op_minus) -> checked_primitive {
                          auto result = checked_sub(left, right);
                          if (!result) return std::unexpected(result.error());
                          return primitive{*result};
                        },
                        [&](tk::op_mul) -> checked_primitive {
                          auto result = checked_mul(left, right);
                          if (!result) return std::unexpected(result.error());
                          return primitive{*result};
                        },
                        [&](tk::op_div) -> checked_primitive {
                          auto result = checked_div(left, right);
                          if (!result) return std::unexpected(result.error());
                          return primitive{*result};
                        },
                        [&](tk::op_eq) -> checked_primitive { return primitive{left == right}; },
                        [&](tk::op_ne) -> checked_primitive { return primitive{left != right}; },
                        [&](tk::op_gt) -> checked_primitive { return primitive{left > right}; },
                        [&](tk::op_lt) -> checked_primitive { return primitive{left < right}; },
                        [](auto) -> checked_primitive { return std::unexpected(eval_status::unsupported); },
                    },
                    op);
}

struct runtime_result {
  eval_status status = eval_status::success;
  value result;
  std::string message;
};

runtime_result runtime_eval(const ast::term& term, const std::vector<value>& env,
                            std::pmr::monotonic_buffer_resource& arena);

runtime_result runtime_binop(const ast::binop& binop, const std::vector<value>& env,
                             std::pmr::monotonic_buffer_resource& arena) {
  auto left = runtime_eval(*binop.left, env, arena);
  if (left.status != eval_status::success) return left;
  auto right = runtime_eval(*binop.right, env, arena);
  if (right.status != eval_status::success) return right;
  auto result = apply_binop(binop.op, std::get<int>(left.result), std::get<int>(right.result));
  if (!result) {
    return {eval_status::runtime_failure,
            {},
            result.error() == eval_status::unsafe ? "unsafe runtime operation" : "unsupported binary operator"};
  }
  return {eval_status::success, std::visit([](auto v) -> value { return v; }, *result), {}};
}

runtime_result runtime_eval(const ast::term& term, const std::vector<value>& env,
                            std::pmr::monotonic_buffer_resource& arena) {
  return std::visit(
      overloaded{
          [&](const ast::var& var) -> runtime_result {
            return {eval_status::success, env[env.size() - 1 - var.index], {}};
          },
          [&](const ast::li_int& literal) -> runtime_result { return {eval_status::success, literal.value, {}}; },
          [&](const ast::li_bool& literal) -> runtime_result { return {eval_status::success, literal.value, {}}; },
          [&](const ast::abst& abst) -> runtime_result {
            auto* closure = std::pmr::polymorphic_allocator<::closure>{&arena}.new_object<::closure>(&abst, env);
            return {eval_status::success, closure, {}};
          },
          [&](const ast::appl& appl) -> runtime_result {
            auto function = runtime_eval(*appl.func, env, arena);
            if (function.status != eval_status::success) return function;
            auto argument = runtime_eval(*appl.arg, env, arena);
            if (argument.status != eval_status::success) return argument;
            auto* closure = std::get<::closure*>(function.result);
            auto call_env = closure->captured_env;
            call_env.push_back(std::move(argument.result));
            return runtime_eval(*closure->abst->body, call_env, arena);
          },
          [&](const ast::binop& binop) -> runtime_result { return runtime_binop(binop, env, arena); },
          [&](const ast::ifexpr& ifexpr) -> runtime_result {
            auto condition = runtime_eval(*ifexpr.cond, env, arena);
            if (condition.status != eval_status::success) return condition;
            return runtime_eval(std::get<bool>(condition.result) ? *ifexpr.then : *ifexpr.els, env, arena);
          },
          [&](const ast::fix& fix) -> runtime_result {
            auto generator = runtime_eval(*fix.body, env, arena);
            if (generator.status != eval_status::success) return generator;
            auto* closure = std::get<::closure*>(generator.result);
            auto& body_abst = std::get<ast::abst>(*closure->abst->body);
            auto* recursive = std::pmr::polymorphic_allocator<::closure>{&arena}.new_object<::closure>(
                &body_abst, closure->captured_env);
            recursive->captured_env.push_back(recursive);
            return {eval_status::success, recursive, {}};
          },
          [](const ast::li_unit&) -> runtime_result { return {eval_status::success, std::monostate{}, {}}; },
          [&](const ast::tup& tuple) -> runtime_result {
            auto* result = std::pmr::polymorphic_allocator<tuple_value>{&arena}.new_object<tuple_value>();
            result->fields.reserve(tuple.fields.size());
            for (const auto* field : tuple.fields) {
              auto value = runtime_eval(*field, env, arena);
              if (value.status != eval_status::success) return value;
              result->fields.push_back(std::move(value.result));
            }
            return {eval_status::success, result, {}};
          },
          [&](const ast::field& field) -> runtime_result {
            auto base = runtime_eval(*field.base, env, arena);
            if (base.status != eval_status::success) return base;
            return {eval_status::success, std::get<tuple_value*>(base.result)->fields[field.index], {}};
          },
          [&](const ast::ctor& ctor) -> runtime_result {
            auto* result = std::pmr::polymorphic_allocator<sum_value>{&arena}.new_object<sum_value>();
            result->name = ctor.name;
            result->tag = ctor.tag;
            if (ctor.payload) {
              auto payload = runtime_eval(*ctor.payload, env, arena);
              if (payload.status != eval_status::success) return payload;
              result->payload = std::move(payload.result);
            }
            return {eval_status::success, result, {}};
          },
          [&](const ast::case_pack& case_expr) -> runtime_result {
            auto scrutinee = runtime_eval(*case_expr.scrutinee, env, arena);
            if (scrutinee.status != eval_status::success) return scrutinee;
            auto* sum = std::get<sum_value*>(scrutinee.result);
            auto arm = runtime_eval(*case_expr.arms[sum->tag].body, env, arena);
            if (arm.status != eval_status::success) return arm;
            auto* closure = std::get<::closure*>(arm.result);
            auto call_env = closure->captured_env;
            call_env.push_back(std::move(sum->payload));
            return runtime_eval(*closure->abst->body, call_env, arena);
          },
      },
      term);
}

struct constant_result {
  eval_status status = eval_status::success;
  std::optional<ast::term> result;
  std::string message;
};

constant_result constant_eval(const ast::term& term);

constant_result constant_binop(const ast::binop& binop) {
  auto left = constant_eval(*binop.left);
  if (left.status != eval_status::success) return left;
  auto right = constant_eval(*binop.right);
  if (right.status != eval_status::success) return right;
  auto* left_literal = std::get_if<ast::li_int>(&*left.result);
  auto* right_literal = std::get_if<ast::li_int>(&*right.result);
  if (!left_literal || !right_literal)
    return {eval_status::unsupported, std::nullopt, "binary operator requires integer constants"};
  auto result = apply_binop(binop.op, left_literal->value, right_literal->value);
  if (!result) {
    return {result.error(), std::nullopt,
            result.error() == eval_status::unsafe ? "unsafe constant operation" : "unsupported binary operator"};
  }
  return {eval_status::success,
          std::visit(overloaded{
                         [](int value) -> ast::term { return ast::term{ast::li_int{value}}; },
                         [](bool value) -> ast::term { return ast::term{ast::li_bool{value}}; },
                     },
                     *result),
          {}};
}

constant_result constant_eval(const ast::term& term) {
  return std::visit(
      overloaded{
          [](const ast::li_int& literal) -> constant_result { return {eval_status::success, ast::term{literal}, {}}; },
          [](const ast::li_bool& literal) -> constant_result { return {eval_status::success, ast::term{literal}, {}}; },
          [](const ast::li_unit& literal) -> constant_result { return {eval_status::success, ast::term{literal}, {}}; },
          [&](const ast::binop& binop) -> constant_result { return constant_binop(binop); },
          [&](const ast::ifexpr& ifexpr) -> constant_result {
            auto condition = constant_eval(*ifexpr.cond);
            if (condition.status != eval_status::success) return condition;
            auto* literal = std::get_if<ast::li_bool>(&*condition.result);
            if (!literal) return {eval_status::unsupported, std::nullopt, "if condition is not a boolean constant"};
            return constant_eval(literal->value ? *ifexpr.then : *ifexpr.els);
          },
          [](const auto&) -> constant_result {
            return {eval_status::unsupported, std::nullopt, "expression is not compile-time evaluable"};
          },
      },
      term);
}

}  // namespace

eval_result evaluate(const ast::term& term, eval_strategy strategy) {
  if (strategy == eval_strategy::runtime) {
    auto arena = std::make_unique<std::pmr::monotonic_buffer_resource>();
    auto result = runtime_eval(term, {}, *arena);
    if (result.status != eval_status::success)
      return {result.status, std::monostate{}, std::move(arena), std::move(result.message)};
    if (std::holds_alternative<closure*>(result.result))
      return {eval_status::runtime_failure, std::monostate{}, std::move(arena),
              "top-level result must not be a closure"};
    return {eval_status::success, std::move(result.result), std::move(arena), {}};
  }

  auto result = constant_eval(term);
  if (result.status != eval_status::success)
    return {result.status, std::monostate{}, nullptr, std::move(result.message)};
  return {eval_status::success, std::move(*result.result), nullptr, {}};
}
