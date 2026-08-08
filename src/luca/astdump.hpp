#pragma once

// 3rd-parties
#include <nlohmann/json_fwd.hpp>
// luca
#include "parser.hpp"

nlohmann::json dump(const ast::term& term);
