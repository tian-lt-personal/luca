// std
#include <limits>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
// luca
#include "coro.hpp"
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

struct tail_call {
  struct awaiter {
    tail_call* call;

    bool await_ready() const noexcept { return true; }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    void await_resume() noexcept { call->apply(); }
  };

  const ast::term** current_term;
  const std::vector<value>** current_env;
  std::vector<value>* owned_env;
  const ast::term* next_term;
  std::vector<value> next_env;

  tail_call(const ast::term*& current_term, const ast::term* next_term) noexcept
      : current_term{&current_term}, current_env{nullptr}, owned_env{nullptr}, next_term{next_term} {}

  tail_call(const ast::term*& current_term, const std::vector<value>*& current_env, std::vector<value>& owned_env,
            const ast::term* next_term, std::vector<value>&& next_env)
      : current_term{&current_term},
        current_env{&current_env},
        owned_env{&owned_env},
        next_term{next_term},
        next_env{std::move(next_env)} {}

  awaiter tail_awaiter() noexcept { return awaiter{this}; }

  void apply() noexcept {
    *current_term = next_term;
    if (current_env) {
      *owned_env = std::move(next_env);
      *current_env = owned_env;
    }
  }
};

lazy<value> runtime_eval(const ast::term& term, const std::vector<value>& env,
                         std::pmr::monotonic_buffer_resource& arena);

lazy<value> runtime_binop(const ast::binop& binop, const std::vector<value>& env,
                          std::pmr::monotonic_buffer_resource& arena) {
  auto left = co_await runtime_eval(*binop.left, env, arena);
  auto right = co_await runtime_eval(*binop.right, env, arena);
  co_return std::visit([](auto result) -> value { return result; },
                       apply_binop(binop.op, std::get<int>(left), std::get<int>(right)));
}

lazy<value> runtime_eval(const ast::term& term, const std::vector<value>& env,
                         std::pmr::monotonic_buffer_resource& arena) {
  const ast::term* current_term = &term;
  const std::vector<value>* current_env = &env;
  std::vector<value> owned_env;

  enum class runtime_step { value, application, binop, ifexpr, fix, tuple, field, ctor, case_pack };
  bool cont = true;
  // Tail transitions reuse this evaluator frame; awaited children transfer directly to their continuation.
  while (cont) {
    cont = false;
    value result;
    const auto step = std::visit(
        overloaded{
            [&](const ast::var& var) {
              result = (*current_env)[current_env->size() - 1 - var.index];
              return runtime_step::value;
            },
            [&](const ast::li_int& literal) {
              result = literal.value;
              return runtime_step::value;
            },
            [&](const ast::li_bool& literal) {
              result = literal.value;
              return runtime_step::value;
            },
            [&](const ast::abst& abst) {
              result = std::pmr::polymorphic_allocator<::closure>{&arena}.new_object<::closure>(&abst, *current_env);
              return runtime_step::value;
            },
            [&](const ast::appl&) {
              cont = true;
              return runtime_step::application;
            },
            [&](const ast::binop&) { return runtime_step::binop; },
            [&](const ast::ifexpr&) {
              cont = true;
              return runtime_step::ifexpr;
            },
            [&](const ast::fix&) { return runtime_step::fix; },
            [&](const ast::li_unit&) {
              result = std::monostate{};
              return runtime_step::value;
            },
            [&](const ast::tup&) { return runtime_step::tuple; },
            [&](const ast::field&) { return runtime_step::field; },
            [&](const ast::ctor&) { return runtime_step::ctor; },
            [&](const ast::case_pack&) {
              cont = true;
              return runtime_step::case_pack;
            },
        },
        *current_term);

    switch (step) {
      case runtime_step::value:
        co_return result;
      case runtime_step::application: {
        const auto& appl = std::get<ast::appl>(*current_term);
        auto function = co_await runtime_eval(*appl.func, *current_env, arena);
        auto argument = co_await runtime_eval(*appl.arg, *current_env, arena);
        auto* closure = std::get<::closure*>(function);
        auto call_env = closure->captured_env;
        call_env.push_back(std::move(argument));
        co_await tail_call{current_term, current_env, owned_env, closure->abst->body, std::move(call_env)};
        break;
      }
      case runtime_step::binop:
        co_return co_await runtime_binop(std::get<ast::binop>(*current_term), *current_env, arena);
      case runtime_step::ifexpr: {
        const auto& ifexpr = std::get<ast::ifexpr>(*current_term);
        auto condition = co_await runtime_eval(*ifexpr.cond, *current_env, arena);
        co_await tail_call{current_term, std::get<bool>(condition) ? ifexpr.then : ifexpr.els};
        break;
      }
      case runtime_step::fix: {
        const auto& fix = std::get<ast::fix>(*current_term);
        auto generator = co_await runtime_eval(*fix.body, *current_env, arena);
        auto* closure = std::get<::closure*>(generator);
        auto& body_abst = std::get<ast::abst>(*closure->abst->body);
        auto* recursive =
            std::pmr::polymorphic_allocator<::closure>{&arena}.new_object<::closure>(&body_abst, closure->captured_env);
        recursive->captured_env.push_back(recursive);
        co_return recursive;
      }
      case runtime_step::tuple: {
        const auto& tuple = std::get<ast::tup>(*current_term);
        auto* tuple_result = std::pmr::polymorphic_allocator<tuple_value>{&arena}.new_object<tuple_value>();
        tuple_result->fields.reserve(tuple.fields.size());
        for (const auto* field : tuple.fields) {
          auto field_value = co_await runtime_eval(*field, *current_env, arena);
          tuple_result->fields.push_back(std::move(field_value));
        }
        co_return tuple_result;
      }
      case runtime_step::field: {
        const auto& field = std::get<ast::field>(*current_term);
        auto base = co_await runtime_eval(*field.base, *current_env, arena);
        co_return std::get<tuple_value*>(base)->fields[field.index];
      }
      case runtime_step::ctor: {
        const auto& ctor = std::get<ast::ctor>(*current_term);
        auto* sum_result = std::pmr::polymorphic_allocator<sum_value>{&arena}.new_object<sum_value>();
        sum_result->name = ctor.name;
        sum_result->tag = ctor.tag;
        if (ctor.payload) {
          auto payload = co_await runtime_eval(*ctor.payload, *current_env, arena);
          sum_result->payload = std::move(payload);
        }
        co_return sum_result;
      }
      case runtime_step::case_pack: {
        const auto& case_expr = std::get<ast::case_pack>(*current_term);
        auto scrutinee = co_await runtime_eval(*case_expr.scrutinee, *current_env, arena);
        auto* sum = std::get<sum_value*>(scrutinee);
        auto arm = co_await runtime_eval(*case_expr.arms[sum->tag].body, *current_env, arena);
        auto* closure = std::get<::closure*>(arm);
        auto call_env = closure->captured_env;
        call_env.push_back(std::move(sum->payload));
        co_await tail_call{current_term, current_env, owned_env, closure->abst->body, std::move(call_env)};
        break;
      }
    }
  }
}

lazy<ast::term> constant_eval(const ast::term& term);

lazy<ast::term> constant_binop(const ast::binop& binop) {
  auto left = co_await constant_eval(*binop.left);
  auto right = co_await constant_eval(*binop.right);
  auto* left_literal = std::get_if<ast::li_int>(&left);
  auto* right_literal = std::get_if<ast::li_int>(&right);
  if (!left_literal || !right_literal)
    throw eval_err{eval_status::unsupported, "binary operator requires integer constants"};
  co_return std::visit(overloaded{
                           [](int value) -> ast::term { return ast::term{ast::li_int{value}}; },
                           [](bool value) -> ast::term { return ast::term{ast::li_bool{value}}; },
                       },
                       apply_binop(binop.op, left_literal->value, right_literal->value));
}

lazy<ast::term> constant_eval(const ast::term& term) {
  const ast::term* current_term = &term;
  enum class constant_step { value, binop, ifexpr, unsupported };
  bool cont = true;
  while (cont) {
    cont = false;
    ast::term result{ast::li_unit{}};
    const auto step = std::visit(overloaded{
                                     [&](const ast::li_int& literal) {
                                       result = ast::term{literal};
                                       return constant_step::value;
                                     },
                                     [&](const ast::li_bool& literal) {
                                       result = ast::term{literal};
                                       return constant_step::value;
                                     },
                                     [&](const ast::li_unit& literal) {
                                       result = ast::term{literal};
                                       return constant_step::value;
                                     },
                                     [&](const ast::binop&) { return constant_step::binop; },
                                     [&](const ast::ifexpr&) {
                                       cont = true;
                                       return constant_step::ifexpr;
                                     },
                                     [](const auto&) { return constant_step::unsupported; },
                                 },
                                 *current_term);

    switch (step) {
      case constant_step::value:
        co_return result;
      case constant_step::binop:
        co_return co_await constant_binop(std::get<ast::binop>(*current_term));
      case constant_step::ifexpr: {
        const auto& ifexpr = std::get<ast::ifexpr>(*current_term);
        auto condition = co_await constant_eval(*ifexpr.cond);
        auto* literal = std::get_if<ast::li_bool>(&condition);
        if (!literal) throw eval_err{eval_status::unsupported, "if condition is not a boolean constant"};
        co_await tail_call{current_term, literal->value ? ifexpr.then : ifexpr.els};
        break;
      }
      case constant_step::unsupported:
        throw eval_err{eval_status::unsupported, "expression is not compile-time evaluable"};
    }
  }
}

}  // namespace

eval_result evaluate(const ast::term& term, eval_strategy strategy) {
  if (strategy == eval_strategy::runtime) {
    auto arena = std::make_unique<std::pmr::monotonic_buffer_resource>();
    try {
      auto result = runtime_eval(term, {}, *arena).get();
      if (std::holds_alternative<closure*>(result))
        throw eval_err{eval_status::runtime_failure, "top-level result must not be a closure"};
      return {std::move(result), std::move(arena)};
    } catch (const eval_err& error) {
      if (error.status == eval_status::runtime_failure) throw;
      throw eval_err{eval_status::runtime_failure, error.what()};
    }
  }

  return {constant_eval(term).get(), nullptr};
}
