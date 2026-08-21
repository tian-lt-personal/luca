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

TEST(astdump_tests, tuple_literal) {
  auto r = parse_ok("{x:int = 1, y:bool = true}");
  auto j = dump(r.first);
  EXPECT_EQ(j,
            nlohmann::json::parse(R"({"tup":{"fields":[{"name":"x","ann":{"int":{}},"value":{"li_int":{"value":1}}},)"
                                  R"({"name":"y","ann":{"bool":{}},"value":{"li_bool":{"value":true}}}]}})"));
}
TEST(astdump_tests, tuple_literal_without_annotation) {
  auto r = parse_ok("{x = 1}");
  auto j = dump(r.first);
  EXPECT_EQ(
      j, nlohmann::json::parse(R"({"tup":{"fields":[{"name":"x","ann":{"int":{}},"value":{"li_int":{"value":1}}}]}})"));
}
TEST(astdump_tests, field_access) {
  auto r = parse_ok("let f : {x:int} -> int = \\p : {x:int} . p.x in 1");
  auto j = dump(r.first);
  EXPECT_EQ(j["appl"]["arg"]["abst"]["body"],
            nlohmann::json::parse(R"({"field":{"base":{"var":{"index":0}},"index":0}})"));
}
TEST(astdump_tests, record_type_dump) {
  auto r = parse_ok("type point = {x:int, y:bool}\nlet f : point -> int = \\p : point . 1 in 2");
  auto j = dump(r.first);
  EXPECT_EQ(
      j["appl"]["arg"]["abst"]["param_type"],
      nlohmann::json::parse(
          R"({"rec":{"name":"point","fields":[{"name":"x","type":{"int":{}}},{"name":"y","type":{"bool":{}}}]}})"));
}

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

TEST(astdump_tests, bound_variable) {
  auto r = parse_ok("(\\x : int . x) 42");
  auto j = dump(r.first);
  EXPECT_EQ(j["appl"]["func"]["abst"]["body"], nlohmann::json::parse(R"({"var":{"index":0}})"));
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
  auto r = parse_ok("(\\f : int -> int . \\x : int . f x) (\\y : int . y) 5");
  auto j = dump(r.first);
  const auto& outer_abst = j["appl"]["func"]["appl"]["func"]["abst"];
  const auto& inner_abst = outer_abst["body"];
  const auto& body = inner_abst["abst"]["body"];
  EXPECT_EQ(body["appl"]["func"], nlohmann::json::parse(R"({"var":{"index":1}})"));
  EXPECT_EQ(body["appl"]["arg"], nlohmann::json::parse(R"({"var":{"index":0}})"));
}
TEST(astdump_tests, application_left_assoc) {
  auto r = parse_ok("(\\f : int -> int -> int . \\x : int . \\y : int . f x y) (\\a : int . \\b : int . a) 1 2");
  auto j = dump(r.first);
  const auto& a0 = j["appl"]["func"]["appl"]["func"]["appl"]["func"]["abst"];
  const auto& a1 = a0["body"];
  const auto& a2 = a1["abst"]["body"];
  const auto& body = a2["abst"]["body"];
  EXPECT_TRUE(body["appl"]["func"]["appl"].is_object());
  EXPECT_TRUE(body["appl"]["arg"]["var"].is_object());
}
TEST(astdump_tests, application_with_literal) {
  auto r = parse_ok("(\\f : int -> int . f 42) (\\y : int . y)");
  auto j = dump(r.first);
  const auto& body = j["appl"]["func"]["abst"]["body"];
  EXPECT_EQ(body["appl"]["func"], nlohmann::json::parse(R"({"var":{"index":0}})"));
  EXPECT_EQ(body["appl"]["arg"], nlohmann::json::parse(R"({"li_int":{"value":42}})"));
}

TEST(astdump_tests, lambda_int) {
  auto r = parse_ok("(\\x : int . x) 42");
  auto j = dump(r.first);
  const auto& ab = j["appl"]["func"]["abst"];
  EXPECT_EQ(ab["param_type"], nlohmann::json::parse(R"({"int":{}})"));
  EXPECT_EQ(ab["body"], nlohmann::json::parse(R"({"var":{"index":0}})"));
}
TEST(astdump_tests, lambda_bool) {
  auto r = parse_ok("(\\x : bool . true) false");
  auto j = dump(r.first);
  const auto& ab = j["appl"]["func"]["abst"];
  EXPECT_EQ(ab["param_type"], nlohmann::json::parse(R"({"bool":{}})"));
  EXPECT_EQ(ab["body"], nlohmann::json::parse(R"({"li_bool":{"value":true}})"));
}
TEST(astdump_tests, lambda_unit) {
  auto r = parse_ok("let f : () -> int = \\x : () . 42 in 1");
  auto j = dump(r.first);
  EXPECT_EQ(j["appl"]["arg"]["abst"]["param_type"], nlohmann::json::parse(R"({"unit":{}})"));
}
TEST(astdump_tests, lambda_nested) {
  auto r = parse_ok("(\\x : bool -> int . \\y : bool . x y) (\\a : bool . 1) true");
  auto j = dump(r.first);
  const auto& outer = j["appl"]["func"]["appl"]["func"]["abst"];
  EXPECT_EQ(outer["param_type"], nlohmann::json::parse(R"({"arrow":{"from":{"bool":{}},"to":{"int":{}}}})"));
  const auto& inner = outer["body"];
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
  auto r = parse_ok("(if true then \\x : int . x else \\x : int . 0) 1");
  auto j = dump(r.first);
  const auto& ie = j["appl"]["func"]["ifexpr"];
  EXPECT_EQ(ie["then"]["abst"]["param_type"], nlohmann::json::parse(R"({"int":{}})"));
  EXPECT_EQ(ie["else"]["abst"]["param_type"], nlohmann::json::parse(R"({"int":{}})"));
}
TEST(astdump_tests, if_in_lambda) {
  auto r = parse_ok("(\\x : int . if x = 0 then 1 else 0) 1");
  auto j = dump(r.first);
  const auto& ab = j["appl"]["func"]["abst"];
  EXPECT_EQ(ab["param_type"], nlohmann::json::parse(R"({"int":{}})"));
  EXPECT_TRUE(ab["body"]["ifexpr"]["cond"]["binop"].is_object());
}
TEST(astdump_tests, complex_nesting) {
  auto r = parse_ok(
      "(\\x : int . \\f : int -> int . \\g : int -> int . \\y : int . if x = 0 then f x + 1 else g y * 2) 1 "
      "(\\a : int . a) (\\a : int . a) 5");
  auto j = dump(r.first);
  const auto& a0 = j["appl"]["func"]["appl"]["func"]["appl"]["func"]["appl"]["func"]["abst"];
  EXPECT_EQ(a0["param_type"], nlohmann::json::parse(R"({"int":{}})"));
  const auto& a1 = a0["body"];
  const auto& a2 = a1["abst"]["body"];
  const auto& a3 = a2["abst"]["body"];
  const auto& ie = a3["abst"]["body"]["ifexpr"];
  EXPECT_TRUE(ie["cond"]["binop"].is_object());
  EXPECT_EQ(ie["then"]["binop"]["op"], "+");
  EXPECT_EQ(ie["else"]["binop"]["op"], "*");
}
TEST(astdump_tests, full_roundtrip_shape) {
  auto r = parse_ok("(\\f : int -> int . \\g : int -> int . f (g 1)) (\\a : int . a) (\\a : int . a)");
  auto j = dump(r.first);
  const auto& ab = j["appl"]["func"]["appl"]["func"]["abst"];
  EXPECT_TRUE(ab.is_object());
  const auto& inner = ab["body"];
  EXPECT_TRUE(inner["abst"].is_object());
  const auto& body = inner["abst"]["body"];
  EXPECT_TRUE(body["appl"].is_object());
}

TEST(astdump_tests, full_json_output) {
  auto r = parse_ok("(if true then \\x : int . x + 1 else \\x : int . 0) 1");
  auto j = dump(r.first);
  auto expected = nlohmann::json::parse(R"(
    {
      "appl": {
        "arg": {"li_int": {"value": 1}},
        "func": {
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
      }
    }
  )");
  EXPECT_EQ(j, expected);
}

TEST(astdump_tests, full_json_output_y_combinator) {
  auto r = parse_ok(
      "let y : ((int -> int) -> (int -> int)) -> (int -> int) = "
      "fix (\\y : ((int -> int) -> (int -> int)) -> (int -> int) . "
      "\\f : (int -> int) -> (int -> int) . \\n : int . f (y f) n) in "
      "let fact : int -> int = "
      "y (\\f : int -> int . \\n : int . if n < 2 then 1 else n * f (n - 1)) in fact 10");
  auto j = dump(r.first);
  auto expected = nlohmann::json::parse(R"(
    {
      "appl": {
        "arg": {
          "fix": {
            "body": {
              "abst": {
                "param_type": {
                  "arrow": {
                    "from": {
                      "arrow": {
                        "from": {"arrow": {"from": {"int": {}}, "to": {"int": {}}}},
                        "to": {"arrow": {"from": {"int": {}}, "to": {"int": {}}}}
                      }
                    },
                    "to": {"arrow": {"from": {"int": {}}, "to": {"int": {}}}}
                  }
                },
                "body": {
                  "abst": {
                    "param_type": {
                      "arrow": {
                        "from": {"arrow": {"from": {"int": {}}, "to": {"int": {}}}},
                        "to": {"arrow": {"from": {"int": {}}, "to": {"int": {}}}}
                      }
                    },
                    "body": {
                      "abst": {
                        "param_type": {"int": {}},
                        "body": {
                          "appl": {
                            "func": {
                              "appl": {
                                "func": {"var": {"index": 1}},
                                "arg": {
                                  "appl": {
                                    "func": {"var": {"index": 2}},
                                    "arg": {"var": {"index": 1}}
                                  }
                                }
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
        },
        "func": {
          "abst": {
            "param_type": {
              "arrow": {
                "from": {
                  "arrow": {
                    "from": {"arrow": {"from": {"int": {}}, "to": {"int": {}}}},
                    "to": {"arrow": {"from": {"int": {}}, "to": {"int": {}}}}
                  }
                },
                "to": {"arrow": {"from": {"int": {}}, "to": {"int": {}}}}
              }
            },
            "body": {
              "appl": {
                "func": {
                  "abst": {
                    "param_type": {"arrow": {"from": {"int": {}}, "to": {"int": {}}}},
                    "body": {
                      "appl": {
                        "func": {"var": {"index": 0}},
                        "arg": {"li_int": {"value": 10}}
                      }
                    }
                  }
                },
                "arg": {
                  "appl": {
                    "func": {"var": {"index": 0}},
                    "arg": {
                      "abst": {
                        "param_type": {"arrow": {"from": {"int": {}}, "to": {"int": {}}}},
                        "body": {
                          "abst": {
                            "param_type": {"int": {}},
                            "body": {
                              "ifexpr": {
                                "cond": {
                                  "binop": {
                                    "op": null,
                                    "left": {"var": {"index": 0}},
                                    "right": {"li_int": {"value": 2}}
                                  }
                                },
                                "then": {"li_int": {"value": 1}},
                                "else": {
                                  "binop": {
                                    "op": "*",
                                    "left": {"var": {"index": 0}},
                                    "right": {
                                      "appl": {
                                        "func": {"var": {"index": 1}},
                                        "arg": {
                                          "binop": {
                                            "op": "-",
                                            "left": {"var": {"index": 0}},
                                            "right": {"li_int": {"value": 1}}
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

TEST(astdump_tests, fix_node) {
  auto r = parse_ok("(fix (\\f : int -> int . \\n : int . n + 1)) 5");
  auto j = dump(r.first);
  EXPECT_EQ(j["appl"]["func"]["fix"]["body"]["abst"]["param_type"],
            nlohmann::json::parse(R"({"arrow":{"from":{"int":{}},"to":{"int":{}}}})"));
  EXPECT_TRUE(j["appl"]["func"]["fix"]["body"]["abst"]["body"].contains("abst"));
}

TEST(astdump_tests, let_simple) {
  auto r = parse_ok("let x : int = 1 in x");
  auto j = dump(r.first);
  auto expected = nlohmann::json::parse(R"(
    {
      "appl": {
        "func": {
          "abst": {
            "param_type": {"int": {}},
            "body": {"var": {"index": 0}}
          }
        },
        "arg": {"li_int": {"value": 1}}
      }
    }
  )");
  EXPECT_EQ(j, expected);
}

TEST(astdump_tests, let_chain) {
  auto r = parse_ok("(\\g : int -> int . let x : int = 1 in let y : int = 2 in (g x) + y) (\\a : int . a)");
  // desugars to: (\g:int->int. (\x:int. (\y:int. (g x) + y) 2) 1) (\a:int. a)
  // g = index 2, x = index 1, y = index 0
  auto j = dump(r.first);
  auto expected = nlohmann::json::parse(R"(
    {
      "appl": {
        "func": {
          "abst": {
            "param_type": {"arrow": {"from": {"int": {}}, "to": {"int": {}}}},
            "body": {
              "appl": {
                "func": {
                  "abst": {
                    "param_type": {"int": {}},
                    "body": {
                      "appl": {
                        "func": {
                          "abst": {
                            "param_type": {"int": {}},
                            "body": {
                              "binop": {
                                "op": "+",
                                "left": {
                                  "appl": {
                                    "func": {"var": {"index": 2}},
                                    "arg": {"var": {"index": 1}}
                                  }
                                },
                                "right": {"var": {"index": 0}}
                              }
                            }
                          }
                        },
                        "arg": {"li_int": {"value": 2}}
                      }
                    }
                  }
                },
                "arg": {"li_int": {"value": 1}}
              }
            }
          }
        },
        "arg": {
          "abst": {
            "param_type": {"int": {}},
            "body": {"var": {"index": 0}}
          }
        }
      }
    }
  )");
  EXPECT_EQ(j, expected);
}

}  // namespace tests
