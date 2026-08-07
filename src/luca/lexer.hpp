#pragma once

// std
#include <expected>
#include <string>
#include <string_view>
#include <variant>

// luca
#include "token.hpp"

struct lex_err_eof {
  friend std::strong_ordering operator<=>(lex_err_eof, lex_err_eof) = default;
};
struct lex_err_unknown {
  friend std::strong_ordering operator<=>(lex_err_unknown, lex_err_unknown) = default;
};
using lex_err = std::variant<lex_err_eof, lex_err_unknown>;

class lexer {
 public:
  using lex_result = std::expected<token, lex_err>;
  explicit lexer(const std::string& source) : src_(source.data()), cur_(src_), lim_(src_ + source.size()) {}
  explicit lexer(std::string&&) = delete;
  lex_result next();

 private:
  std::string_view lexeme() const { return std::string_view{tok_, static_cast<size_t>(cur_ - tok_)}; }

  const char* src_;
  const char* cur_;
  const char* mark_ = nullptr;
  const char* lim_;
  const char* tok_ = nullptr;
};
