// std
#include <memory_resource>
#include <stdexcept>
// luca
#include "machine.hpp"
#include "mp.hpp"

namespace {

struct eval_context {
  std::pmr::monotonic_buffer_resource mbr;
};

static value eval(const ast::term& t, const std::vector<value>& env, eval_context& arena) {
  return std::visit(overloaded{
                        [&](const ast::var& v) -> value { return env[env.size() - 1 - v.index]; },
                        [&](const ast::li_int& l) -> value { return l.value; },
                        [&](const ast::li_bool& b) -> value { return b.value; },
                        [&](const ast::abst& a) -> value {
                          auto* c = std::pmr::polymorphic_allocator<closure>{&arena.mbr}.new_object<closure>(&a, env);
                          return c;
                        },
                        [&](const ast::appl& a) -> value {
                          auto func = eval(*a.func, env, arena);
                          auto arg = eval(*a.arg, env, arena);
                          auto c = std::get<closure*>(func);
                          // fresh env per call, so recursive self-applications don't see the parent's argument
                          auto call_env = c->captured_env;
                          call_env.push_back(std::move(arg));
                          return eval(*c->abst->body, call_env, arena);
                        },
                        [&](const ast::binop& b) -> value {
                          int lv = std::get<int>(eval(*b.left, env, arena));
                          int rv = std::get<int>(eval(*b.right, env, arena));
                          return std::visit(overloaded{
                                                [=](tk::op_plus) -> value { return lv + rv; },
                                                [=](tk::op_minus) -> value { return lv - rv; },
                                                [=](tk::op_mul) -> value { return lv * rv; },
                                                [=](tk::op_div) -> value { return lv / rv; },
                                                [=](tk::op_eq) -> value { return lv == rv; },
                                                [=](tk::op_ne) -> value { return lv != rv; },
                                                [=](tk::op_gt) -> value { return lv > rv; },
                                                [=](tk::op_lt) -> value { return lv < rv; },
                                                [](auto) -> value { throw std::logic_error{"unexpected binop"}; },
                                            },
                                            b.op);
                        },
                        [&](const ast::ifexpr& ie) -> value {
                          bool cv = std::get<bool>(eval(*ie.cond, env, arena));
                          return eval(cv ? *ie.then : *ie.els, env, arena);
                        },
                        [&](const ast::fix& fx) -> value {
                          auto c = std::get<closure*>(eval(*fx.body, env, arena));
                          // absorb the first binder; rec occupies f's env slot so f resolves to itself
                          auto& body_abst = std::get<ast::abst>(*c->abst->body);
                          auto* rec = std::pmr::polymorphic_allocator<closure>{&arena.mbr}.new_object<closure>(
                              &body_abst, c->captured_env);
                          rec->captured_env.push_back(rec);
                          return rec;
                        },
                    },
                    t);
}

}  // namespace

value eval(const ast::term& t) {
  eval_context arena;
  std::vector<value> env;
  auto v = eval(t, env, arena);
  if (std::holds_alternative<closure*>(v)) throw std::logic_error{"top-level result must not be a closure"};
  return v;
}
