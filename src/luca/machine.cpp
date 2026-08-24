// std
#include <memory_resource>
#include <stdexcept>
// luca
#include "machine.hpp"
#include "mp.hpp"

namespace {

static value eval(const ast::term& t, const std::vector<value>& env, std::pmr::monotonic_buffer_resource& arena) {
  return std::visit(overloaded{
                        [&](const ast::var& v) -> value { return env[env.size() - 1 - v.index]; },
                        [&](const ast::li_int& l) -> value { return l.value; },
                        [&](const ast::li_bool& b) -> value { return b.value; },
                        [&](const ast::abst& a) -> value {
                          auto* c = std::pmr::polymorphic_allocator<closure>{&arena}.new_object<closure>(&a, env);
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
                          auto* rec = std::pmr::polymorphic_allocator<closure>{&arena}.new_object<closure>(
                              &body_abst, c->captured_env);
                          rec->captured_env.push_back(rec);
                          return rec;
                        },
                        [&](const ast::li_unit&) -> value { return std::monostate{}; },
                        [&](const ast::tup& t) -> value {
                          auto* tv = std::pmr::polymorphic_allocator<tuple_value>{&arena}.new_object<tuple_value>();
                          tv->fields.reserve(t.fields.size());
                          for (const auto& f : t.fields) tv->fields.push_back(eval(*f, env, arena));
                          return tv;
                        },
                        [&](const ast::field& f) -> value {
                          auto base = eval(*f.base, env, arena);
                          return std::get<tuple_value*>(base)->fields[f.index];
                        },
                    },
                    t);
}

}  // namespace

eval_result eval(const ast::term& t) {
  // the arena keeps closures and records alive; the caller holds it via eval_result
  auto arena = std::make_unique<std::pmr::monotonic_buffer_resource>();
  std::vector<value> env;
  auto v = eval(t, env, *arena);
  if (std::holds_alternative<closure*>(v)) throw std::logic_error{"top-level result must not be a closure"};
  return {std::move(v), std::move(arena)};
}
