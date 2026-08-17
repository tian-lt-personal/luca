#pragma once

// std
#include <cstddef>
#include <string>
#include <string_view>

struct src_range {
  size_t begin;  // half-open byte offsets into the source string
  size_t end;
  size_t len() const noexcept { return end - begin; }
  friend bool operator==(src_range, src_range) = default;
};

struct diagnostic {
  src_range loc;
  std::string code;     // stable id: A* lexing, B* parsing, C* sema, e.g. "C004"
  std::string message;  // main error text
  std::string hint;     // optional "hint:" line; empty = no hint
};

std::string render(const diagnostic& d, std::string_view source, std::string_view filename = {});
