// std
#include <limits>
#include <memory_resource>
#include <stdexcept>
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

int checked_add(int left, int right) {
  const auto result = static_cast<long long>(left) + right;
  if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
    throw eval_err{eval_status::unsafe, "unsafe integer addition"};
  return static_cast<int>(result);
}

int checked_sub(int left, int right) {
  const auto result = static_cast<long long>(left) - right;
  if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
    throw eval_err{eval_status::unsafe, "unsafe integer subtraction"};
  return static_cast<int>(result);
}

int checked_mul(int left, int right) {
  const auto result = static_cast<long long>(left) * right;
  if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max())
    throw eval_err{eval_status::unsafe, "unsafe integer multiplication"};
  return static_cast<int>(result);
}

int checked_div(int left, int right) {
  if (right == 0 || (left == std::numeric_limits<int>::min() && right == -1))
    throw eval_err{eval_status::unsafe, "unsafe integer division"};
  return left / right;
}

primitive apply_binop(const token& op, int left, int right) {
  return std::visit(
      overloaded{
          [&](tk::op_plus) -> primitive { return primitive{checked_add(left, right)}; },
          [&](tk::op_minus) -> primitive { return primitive{checked_sub(left, right)}; },
          [&](tk::op_mul) -> primitive { return primitive{checked_mul(left, right)}; },
          [&](tk::op_div) -> primitive { return primitive{checked_div(left, right)}; },
          [&](tk::op_eq) -> primitive { return primitive{left == right}; },
          [&](tk::op_ne) -> primitive { return primitive{left != right}; },
          [&](tk::op_gt) -> primitive { return primitive{left > right}; },
          [&](tk::op_lt) -> primitive { return primitive{left < right}; },
          [](auto) -> primitive { throw eval_err{eval_status::unsupported, "unsupported binary operator"}; },
      },
      op);
}

value runtime_eval(const ast::term& term, const std::vector<value>& env, std::pmr::monotonic_buffer_resource& arena);

value runtime_binop(const ast::binop& binop, const std::vector<value>& env,
                    std::pmr::monotonic_buffer_resource& arena) {
  auto left = runtime_eval(*binop.left, env, arena);
  auto right = runtime_eval(*binop.right, env, arena);
  return std::visit([](auto result) -> value { return result; },
                    apply_binop(binop.op, std::get<int>(left), std::get<int>(right)));
}

value runtime_eval(const ast::term& term, const std::vector<value>& env, std::pmr::monotonic_buffer_resource& arena) {
  return std::visit(overloaded{
                        [&](const ast::var& var) -> value { return env[env.size() - 1 - var.index]; },
                        [&](const ast::li_int& literal) -> value { return literal.value; },
                        [&](const ast::li_bool& literal) -> value { return literal.value; },
                        [&](const ast::abst& abst) -> value {
                          auto* closure =
                              std::pmr::polymorphic_allocator<::closure>{&arena}.new_object<::closure>(&abst, env);
                          return closure;
                        },
                        [&](const ast::appl& appl) -> value {
                          auto function = runtime_eval(*appl.func, env, arena);
                          auto argument = runtime_eval(*appl.arg, env, arena);
                          auto* closure = std::get<::closure*>(function);
                          auto call_env = closure->captured_env;
                          call_env.push_back(std::move(argument));
                          return runtime_eval(*closure->abst->body, call_env, arena);
                        },
                        [&](const ast::binop& binop) -> value { return runtime_binop(binop, env, arena); },
                        [&](const ast::ifexpr& ifexpr) -> value {
                          auto condition = runtime_eval(*ifexpr.cond, env, arena);
                          return runtime_eval(std::get<bool>(condition) ? *ifexpr.then : *ifexpr.els, env, arena);
                        },
                        [&](const ast::fix& fix) -> value {
                          auto generator = runtime_eval(*fix.body, env, arena);
                          auto* closure = std::get<::closure*>(generator);
                          auto& body_abst = std::get<ast::abst>(*closure->abst->body);
                          auto* recursive = std::pmr::polymorphic_allocator<::closure>{&arena}.new_object<::closure>(
                              &body_abst, closure->captured_env);
                          recursive->captured_env.push_back(recursive);
                          return recursive;
                        },
                        [](const ast::li_unit&) -> value { return std::monostate{}; },
                        [&](const ast::tup& tuple) -> value {
                          auto* result = std::pmr::polymorphic_allocator<tuple_value>{&arena}.new_object<tuple_value>();
                          result->fields.reserve(tuple.fields.size());
                          for (const auto* field : tuple.fields) {
                            auto value = runtime_eval(*field, env, arena);
                            result->fields.push_back(std::move(value));
                          }
                          return result;
                        },
                        [&](const ast::field& field) -> value {
                          auto base = runtime_eval(*field.base, env, arena);
                          return std::get<tuple_value*>(base)->fields[field.index];
                        },
                        [&](const ast::ctor& ctor) -> value {
                          auto* result = std::pmr::polymorphic_allocator<sum_value>{&arena}.new_object<sum_value>();
                          result->name = ctor.name;
                          result->tag = ctor.tag;
                          if (ctor.payload) {
                            auto payload = runtime_eval(*ctor.payload, env, arena);
                            result->payload = std::move(payload);
                          }
                          return result;
                        },
                        [&](const ast::case_pack& case_expr) -> value {
                          auto scrutinee = runtime_eval(*case_expr.scrutinee, env, arena);
                          auto* sum = std::get<sum_value*>(scrutinee);
                          auto arm = runtime_eval(*case_expr.arms[sum->tag].body, env, arena);
                          auto* closure = std::get<::closure*>(arm);
                          auto call_env = closure->captured_env;
                          call_env.push_back(std::move(sum->payload));
                          return runtime_eval(*closure->abst->body, call_env, arena);
                        },
                    },
                    term);
}

ast::term constant_eval(const ast::term& term);

ast::term constant_binop(const ast::binop& binop) {
  auto left = constant_eval(*binop.left);
  auto right = constant_eval(*binop.right);
  auto* left_literal = std::get_if<ast::li_int>(&left);
  auto* right_literal = std::get_if<ast::li_int>(&right);
  if (!left_literal || !right_literal)
    throw eval_err{eval_status::unsupported, "binary operator requires integer constants"};
  return std::visit(overloaded{
                        [](int value) -> ast::term { return ast::term{ast::li_int{value}}; },
                        [](bool value) -> ast::term { return ast::term{ast::li_bool{value}}; },
                    },
                    apply_binop(binop.op, left_literal->value, right_literal->value));
}

ast::term constant_eval(const ast::term& term) {
  return std::visit(overloaded{
                        [](const ast::li_int& literal) -> ast::term { return ast::term{literal}; },
                        [](const ast::li_bool& literal) -> ast::term { return ast::term{literal}; },
                        [](const ast::li_unit& literal) -> ast::term { return ast::term{literal}; },
                        [&](const ast::binop& binop) -> ast::term { return constant_binop(binop); },
                        [&](const ast::ifexpr& ifexpr) -> ast::term {
                          auto condition = constant_eval(*ifexpr.cond);
                          auto* literal = std::get_if<ast::li_bool>(&condition);
                          if (!literal)
                            throw eval_err{eval_status::unsupported, "if condition is not a boolean constant"};
                          return constant_eval(literal->value ? *ifexpr.then : *ifexpr.els);
                        },
                        [](const auto&) -> ast::term {
                          throw eval_err{eval_status::unsupported, "expression is not compile-time evaluable"};
                        },
                    },
                    term);
}

}  // namespace

eval_result evaluate(const ast::term& term, eval_strategy strategy) {
  if (strategy == eval_strategy::runtime) {
    auto arena = std::make_unique<std::pmr::monotonic_buffer_resource>();
    try {
      auto result = runtime_eval(term, {}, *arena);
      if (std::holds_alternative<closure*>(result))
        throw eval_err{eval_status::runtime_failure, "top-level result must not be a closure"};
      return {std::move(result), std::move(arena)};
    } catch (const eval_err& error) {
      if (error.status == eval_status::runtime_failure) throw;
      throw eval_err{eval_status::runtime_failure, error.what()};
    }
  }

  return {constant_eval(term), nullptr};
}
