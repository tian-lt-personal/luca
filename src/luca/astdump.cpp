#include "astdump.hpp"

#include <nlohmann/json.hpp>

#include "mp.hpp"

void to_json(nlohmann::json& j, const ast::type& t);
void to_json(nlohmann::json& j, const ast::type_arrow& t);
void to_json(nlohmann::json& j, const ast::type_prod& t);
void to_json(nlohmann::json& j, const ast::type_ref& t);
void to_json(nlohmann::json& j, const ast::term& t);
void to_json(nlohmann::json& j, const ast::abst& a);
void to_json(nlohmann::json& j, const ast::appl& a);
void to_json(nlohmann::json& j, const ast::binop& b);
void to_json(nlohmann::json& j, const ast::ifexpr& i);
void to_json(nlohmann::json& j, const ast::fix& f);
void to_json(nlohmann::json& j, const ast::li_unit& u);
void to_json(nlohmann::json& j, const ast::tup& t);
void to_json(nlohmann::json& j, const ast::field& f);
void to_json(nlohmann::json& j, const ast::ctor& c);
void to_json(nlohmann::json& j, const ast::case_& cs);
void to_json(nlohmann::json& j, const ast::quote& q);
void to_json(nlohmann::json& j, const ast::store_program& s);
void to_json(nlohmann::json& j, const ast::load_program& l);

void to_json(nlohmann::json& j, const ast::type_unit&) { j["unit"] = nlohmann::json::object(); }
void to_json(nlohmann::json& j, const ast::type_int&) { j["int"] = nlohmann::json::object(); }
void to_json(nlohmann::json& j, const ast::type_bool&) { j["bool"] = nlohmann::json::object(); }
void to_json(nlohmann::json& j, const ast::type_string&) { j["string"] = nlohmann::json::object(); }
void to_json(nlohmann::json& j, const ast::type_arrow& t) {
  j["arrow"] = nlohmann::json::object();
  to_json(j["arrow"]["from"], *t.from);
  to_json(j["arrow"]["to"], *t.to);
}
void to_json(nlohmann::json& j, const ast::type_prod& t) {
  j["prod"] = nlohmann::json::object();
  j["prod"]["fields"] = nlohmann::json::array();
  for (const auto& f : t.fields) {
    nlohmann::json fj;
    to_json(fj["type"], *f);
    j["prod"]["fields"].push_back(std::move(fj));
  }
}
void to_json(nlohmann::json& j, const ast::type_ref& t) {
  j["ref"] = nlohmann::json::object();
  j["ref"]["name"] = t.name;
}
void to_json(nlohmann::json& j, const ast::type& t) {
  std::visit([&j](const auto& v) { to_json(j, v); }, t);
}
void to_json(nlohmann::json& j, const ast::var& v) { j["var"]["index"] = v.index; }
void to_json(nlohmann::json& j, const ast::li_int& v) { j["li_int"]["value"] = v.value; }
void to_json(nlohmann::json& j, const ast::li_bool& v) { j["li_bool"]["value"] = v.value; }
void to_json(nlohmann::json& j, const ast::term& t) {
  std::visit([&j](const auto& v) { to_json(j, v); }, t);
}
void to_json(nlohmann::json& j, const ast::abst& a) {
  j["abst"] = nlohmann::json::object();
  to_json(j["abst"]["param_type"], a.param_type);
  to_json(j["abst"]["body"], *a.body);
}
void to_json(nlohmann::json& j, const ast::appl& a) {
  j["appl"] = nlohmann::json::object();
  to_json(j["appl"]["func"], *a.func);
  to_json(j["appl"]["arg"], *a.arg);
}
void to_json(nlohmann::json& j, const ast::binop& b) {
  j["binop"] = nlohmann::json::object();
  j["binop"]["op"] = std::visit(overloaded{
                                    [](tk::op_plus) -> nlohmann::json { return "+"; },
                                    [](tk::op_minus) -> nlohmann::json { return "-"; },
                                    [](tk::op_mul) -> nlohmann::json { return "*"; },
                                    [](tk::op_div) -> nlohmann::json { return "/"; },
                                    [](const auto&) -> nlohmann::json { return nullptr; },
                                },
                                b.op);
  to_json(j["binop"]["left"], *b.left);
  to_json(j["binop"]["right"], *b.right);
}
void to_json(nlohmann::json& j, const ast::ifexpr& i) {
  j["ifexpr"] = nlohmann::json::object();
  to_json(j["ifexpr"]["cond"], *i.cond);
  to_json(j["ifexpr"]["then"], *i.then);
  to_json(j["ifexpr"]["else"], *i.els);
}
void to_json(nlohmann::json& j, const ast::fix& f) {
  j["fix"] = nlohmann::json::object();
  to_json(j["fix"]["body"], *f.body);
}
void to_json(nlohmann::json& j, const ast::li_unit&) { j["li_unit"] = nlohmann::json::object(); }
void to_json(nlohmann::json& j, const ast::tup& t) {
  j["tup"] = nlohmann::json::object();
  j["tup"]["fields"] = nlohmann::json::array();
  for (const auto& f : t.fields) {
    nlohmann::json fj;
    to_json(fj["value"], *f);
    j["tup"]["fields"].push_back(std::move(fj));
  }
}
void to_json(nlohmann::json& j, const ast::field& f) {
  j["field"] = nlohmann::json::object();
  to_json(j["field"]["base"], *f.base);
  j["field"]["index"] = f.index;
}
void to_json(nlohmann::json& j, const ast::ctor& c) {
  j["ctor"] = nlohmann::json::object();
  j["ctor"]["name"] = c.name;
  j["ctor"]["tag"] = c.tag;
  if (c.payload)
    to_json(j["ctor"]["payload"], *c.payload);
  else
    j["ctor"]["payload"] = nullptr;
}
void to_json(nlohmann::json& j, const ast::case_& cs) {
  j["case"] = nlohmann::json::object();
  to_json(j["case"]["scrutinee"], *cs.scrutinee);
  j["case"]["arms"] = nlohmann::json::array();
  for (const auto& a : cs.arms) {
    nlohmann::json aj;
    to_json(aj["body"], *a.body);
    j["case"]["arms"].push_back(std::move(aj));
  }
}
// reflection nodes are compile-time only (parser removes them); dump defensively:
// a quote is its captured term, intrinsics are their names
void to_json(nlohmann::json& j, const ast::quote& q) { to_json(j, *q.data); }
void to_json(nlohmann::json& j, const ast::store_program&) {
  j["std-store-program"] = nlohmann::json::object();
}
void to_json(nlohmann::json& j, const ast::load_program&) {
  j["std-load-program"] = nlohmann::json::object();
}

nlohmann::json dump(const ast::term& term) {
  nlohmann::json j;
  to_json(j, term);
  return j;
}
