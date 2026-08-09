// gtest
#include <gtest/gtest.h>
// 3rd-parties
#include <nlohmann/json.hpp>
// luca
#include <astdump.hpp>
#include <parser.hpp>

namespace {

auto parse_ok(const std::string& src) {
  try {
    return parse(src);
  } catch (const parse_err& e) {
    EXPECT_TRUE(false) << "parse failed for: " << src << ": " << e.what();
    throw;
  }
}

}  // namespace

namespace tests {

TEST(astdump_tests, integer_literal) {
  auto r = parse_ok("42");
  auto j = dump(r.first);
  EXPECT_EQ(j, nlohmann::json::parse(R"({"li_int":{"value":42}})"));
}
TEST(astdump_tests, boolean_true) {
  auto r = parse_ok("true");
  auto j = dump(r.first);
  EXPECT_EQ(j, nlohmann::json::parse(R"({"li_bool":{"value":true}})"));
}
TEST(astdump_tests, boolean_false) {
  auto r = parse_ok("false");
  auto j = dump(r.first);
  EXPECT_EQ(j, nlohmann::json::parse(R"({"li_bool":{"value":false}})"));
}

TEST(astdump_tests, variable) {
  auto r = parse_ok("x");
  auto j = dump(r.first);
  EXPECT_EQ(j, nlohmann::json::parse(R"({"var":{"index":null}})"));
}
TEST(astdump_tests, bound_variable) {
  auto r = parse_ok("\\x : int . x");
  auto j = dump(r.first);
  EXPECT_EQ(j["abst"]["body"], nlohmann::json::parse(R"({"var":{"index":0}})"));
}

TEST(astdump_tests, binop_add) {
  auto r = parse_ok("1 + 2");
  auto j = dump(r.first);
  EXPECT_EQ(j["binop"]["op"], "+");
  EXPECT_EQ(j["binop"]["left"], nlohmann::json::parse(R"({"li_int":{"value":1}})"));
  EXPECT_EQ(j["binop"]["right"], nlohmann::json::parse(R"({"li_int":{"value":2}})"));
}
TEST(astdump_tests, binop_mul) {
  auto r = parse_ok("3 * 4");
  auto j = dump(r.first);
  EXPECT_EQ(j["binop"]["op"], "*");
  EXPECT_EQ(j["binop"]["left"], nlohmann::json::parse(R"({"li_int":{"value":3}})"));
  EXPECT_EQ(j["binop"]["right"], nlohmann::json::parse(R"({"li_int":{"value":4}})"));
}
TEST(astdump_tests, binop_div) {
  auto r = parse_ok("8 / 2");
  auto j = dump(r.first);
  EXPECT_EQ(j["binop"]["op"], "/");
}
TEST(astdump_tests, binop_nested) {
  auto r = parse_ok("1 + 2 * 3");
  auto j = dump(r.first);
  EXPECT_EQ(j["binop"]["op"], "+");
  EXPECT_EQ(j["binop"]["left"], nlohmann::json::parse(R"({"li_int":{"value":1}})"));
  EXPECT_EQ(j["binop"]["right"]["binop"]["op"], "*");
}

TEST(astdump_tests, unary_minus) {
  auto r = parse_ok("-5");
  auto j = dump(r.first);
  EXPECT_EQ(j["binop"]["op"], "-");
  EXPECT_EQ(j["binop"]["left"], nlohmann::json::parse(R"({"li_int":{"value":0}})"));
  EXPECT_EQ(j["binop"]["right"], nlohmann::json::parse(R"({"li_int":{"value":5}})"));
}

TEST(astdump_tests, application_single) {
  auto r = parse_ok("f x");
  auto j = dump(r.first);
  EXPECT_EQ(j["appl"]["func"], nlohmann::json::parse(R"({"var":{"index":null}})"));
  EXPECT_EQ(j["appl"]["arg"], nlohmann::json::parse(R"({"var":{"index":null}})"));
}
TEST(astdump_tests, application_left_assoc) {
  auto r = parse_ok("f x y");
  auto j = dump(r.first);
  EXPECT_TRUE(j["appl"]["func"]["appl"].is_object());
  EXPECT_TRUE(j["appl"]["arg"]["var"].is_object());
}
TEST(astdump_tests, application_with_literal) {
  auto r = parse_ok("f 42");
  auto j = dump(r.first);
  EXPECT_EQ(j["appl"]["func"], nlohmann::json::parse(R"({"var":{"index":null}})"));
  EXPECT_EQ(j["appl"]["arg"], nlohmann::json::parse(R"({"li_int":{"value":42}})"));
}

TEST(astdump_tests, lambda_int) {
  auto r = parse_ok("\\x : int . x");
  auto j = dump(r.first);
  EXPECT_EQ(j["abst"]["param_type"], nlohmann::json::parse(R"({"int":{}})"));
  EXPECT_EQ(j["abst"]["body"], nlohmann::json::parse(R"({"var":{"index":0}})"));
}
TEST(astdump_tests, lambda_bool) {
  auto r = parse_ok("\\x : bool . true");
  auto j = dump(r.first);
  EXPECT_EQ(j["abst"]["param_type"], nlohmann::json::parse(R"({"bool":{}})"));
  EXPECT_EQ(j["abst"]["body"], nlohmann::json::parse(R"({"li_bool":{"value":true}})"));
}
TEST(astdump_tests, lambda_unit) {
  auto r = parse_ok("\\x : () . 42");
  auto j = dump(r.first);
  EXPECT_EQ(j["abst"]["param_type"], nlohmann::json::parse(R"({"unit":{}})"));
}
TEST(astdump_tests, lambda_nested) {
  auto r = parse_ok("\\x : int . \\y : bool . x y");
  auto j = dump(r.first);
  EXPECT_EQ(j["abst"]["param_type"], nlohmann::json::parse(R"({"int":{}})"));
  const auto& inner = j["abst"]["body"];
  EXPECT_EQ(inner["abst"]["param_type"], nlohmann::json::parse(R"({"bool":{}})"));
  EXPECT_EQ(inner["abst"]["body"]["appl"]["func"], nlohmann::json::parse(R"({"var":{"index":1}})"));
  EXPECT_EQ(inner["abst"]["body"]["appl"]["arg"], nlohmann::json::parse(R"({"var":{"index":0}})"));
}

TEST(astdump_tests, if_simple) {
  auto r = parse_ok("if true then 1 else 2");
  auto j = dump(r.first);
  EXPECT_EQ(j["ifexpr"]["cond"], nlohmann::json::parse(R"({"li_bool":{"value":true}})"));
  EXPECT_EQ(j["ifexpr"]["then"], nlohmann::json::parse(R"({"li_int":{"value":1}})"));
  EXPECT_EQ(j["ifexpr"]["else"], nlohmann::json::parse(R"({"li_int":{"value":2}})"));
}
TEST(astdump_tests, nested_if) {
  auto r = parse_ok("if true then if false then 1 else 2 else 3");
  auto j = dump(r.first);
  EXPECT_EQ(j["ifexpr"]["else"], nlohmann::json::parse(R"({"li_int":{"value":3}})"));
  const auto& inner = j["ifexpr"]["then"];
  EXPECT_EQ(inner["ifexpr"]["cond"], nlohmann::json::parse(R"({"li_bool":{"value":false}})"));
  EXPECT_EQ(inner["ifexpr"]["then"], nlohmann::json::parse(R"({"li_int":{"value":1}})"));
  EXPECT_EQ(inner["ifexpr"]["else"], nlohmann::json::parse(R"({"li_int":{"value":2}})"));
}

TEST(astdump_tests, parens_override_precedence) {
  auto r = parse_ok("(1 + 2) * 3");
  auto j = dump(r.first);
  EXPECT_EQ(j["binop"]["op"], "*");
  EXPECT_EQ(j["binop"]["left"]["binop"]["op"], "+");
  EXPECT_EQ(j["binop"]["right"], nlohmann::json::parse(R"({"li_int":{"value":3}})"));
}

TEST(astdump_tests, lambda_applied) {
  auto r = parse_ok("(\\x : int . x) 42");
  auto j = dump(r.first);
  EXPECT_TRUE(j["appl"]["func"]["abst"].is_object());
  EXPECT_EQ(j["appl"]["arg"], nlohmann::json::parse(R"({"li_int":{"value":42}})"));
}
TEST(astdump_tests, lambda_in_if) {
  auto r = parse_ok("if true then \\x : int . x else \\x : bool . false");
  auto j = dump(r.first);
  EXPECT_EQ(j["ifexpr"]["then"]["abst"]["param_type"], nlohmann::json::parse(R"({"int":{}})"));
  EXPECT_EQ(j["ifexpr"]["else"]["abst"]["param_type"], nlohmann::json::parse(R"({"bool":{}})"));
}
TEST(astdump_tests, if_in_lambda) {
  auto r = parse_ok("\\x : int . if x then 1 else 0");
  auto j = dump(r.first);
  EXPECT_EQ(j["abst"]["param_type"], nlohmann::json::parse(R"({"int":{}})"));
  EXPECT_EQ(j["abst"]["body"]["ifexpr"]["cond"], nlohmann::json::parse(R"({"var":{"index":0}})"));
}
TEST(astdump_tests, complex_nesting) {
  auto r = parse_ok("\\x : int . if x then f x + 1 else g y * 2");
  auto j = dump(r.first);
  EXPECT_EQ(j["abst"]["param_type"], nlohmann::json::parse(R"({"int":{}})"));
  const auto& ie = j["abst"]["body"]["ifexpr"];
  EXPECT_EQ(ie["cond"], nlohmann::json::parse(R"({"var":{"index":0}})"));
  EXPECT_EQ(ie["then"]["binop"]["op"], "+");
  EXPECT_EQ(ie["else"]["binop"]["op"], "*");
}
TEST(astdump_tests, full_roundtrip_shape) {
  auto r = parse_ok("\\f : int . \\g : int . f (g 1)");
  auto j = dump(r.first);
  EXPECT_TRUE(j["abst"].is_object());
  const auto& inner = j["abst"]["body"];
  EXPECT_TRUE(inner["abst"].is_object());
  const auto& body = inner["abst"]["body"];
  EXPECT_TRUE(body["appl"].is_object());
}

TEST(astdump_tests, full_json_output) {
  auto r = parse_ok("if true then \\x : int . x + 1 else \\x : int . 0");
  auto j = dump(r.first);
  auto expected = nlohmann::json::parse(R"(
    {
      "ifexpr": {
        "cond": {"li_bool": {"value": true}},
        "then": {
          "abst": {
            "param_type": {"int": {}},
            "body": {
              "binop": {
                "op": "+",
                "left": {"var": {"index": 0}},
                "right": {"li_int": {"value": 1}}
              }
            }
          }
        },
        "else": {
          "abst": {
            "param_type": {"int": {}},
            "body": {"li_int": {"value": 0}}
          }
        }
      }
    }
  )");
  EXPECT_EQ(j, expected);
}

TEST(astdump_tests, full_json_output_y_combinator) {
  auto r = parse_ok("\\f : () . (\\x : () . f (\\y : () . (x x) y)) (\\x : () . f (\\y : () . (x x) y))");
  auto j = dump(r.first);
  auto expected = nlohmann::json::parse(R"(
    {
      "abst": {
        "param_type": {"unit": {}},
        "body": {
          "appl": {
            "func": {
              "abst": {
                "param_type": {"unit": {}},
                "body": {
                  "appl": {
                    "func": {"var": {"index": 1}},
                    "arg": {
                      "abst": {
                        "param_type": {"unit": {}},
                        "body": {
                          "appl": {
                            "func": {
                              "appl": {
                                "func": {"var": {"index": 1}},
                                "arg": {"var": {"index": 1}}
                              }
                            },
                            "arg": {"var": {"index": 0}}
                          }
                        }
                      }
                    }
                  }
                }
              }
            },
            "arg": {
              "abst": {
                "param_type": {"unit": {}},
                "body": {
                  "appl": {
                    "func": {"var": {"index": 1}},
                    "arg": {
                      "abst": {
                        "param_type": {"unit": {}},
                        "body": {
                          "appl": {
                            "func": {
                              "appl": {
                                "func": {"var": {"index": 1}},
                                "arg": {"var": {"index": 1}}
                              }
                            },
                            "arg": {"var": {"index": 0}}
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  )");
  EXPECT_EQ(j, expected);
}

}  // namespace tests
