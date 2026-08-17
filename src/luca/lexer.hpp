#pragma once

// std
#include <expected>
#include <string>
#include <string_view>
#include <variant>

// luca
#include "diag.hpp"
#include "token.hpp"

struct lex_err_eof {
  src_range loc{};
  friend std::strong_ordering operator<=>(lex_err_eof, lex_err_eof) = default;
};
struct lex_err_char {
  src_range loc{};
  friend std::strong_ordering operator<=>(lex_err_char, lex_err_char) = default;
};
struct lex_err_str {
  src_range loc{};
  friend std::strong_ordering operator<=>(lex_err_str, lex_err_str) = default;
};
struct lex_err_glued {
  src_range loc{};
  friend std::strong_ordering operator<=>(lex_err_glued, lex_err_glued) = default;
};
using lex_err = std::variant<lex_err_eof, lex_err_char, lex_err_str, lex_err_glued>;

struct token_span {
  token t;
  src_range loc;
  friend std::strong_ordering operator<=>(token_span, token_span) = default;
};

using lex_result = std::expected<token_span, lex_err>;
class lexer {
 public:
  explicit lexer(const std::string& source) noexcept : src_(source.data()), cur_(src_), lim_(src_ + source.size()) {}
  explicit lexer(std::string&&) = delete;
  lex_result next() noexcept;

 private:
  std::string_view lexeme() const noexcept { return std::string_view{tok_, static_cast<size_t>(cur_ - tok_)}; }
  src_range cur_span() const noexcept { return {static_cast<size_t>(tok_ - src_), static_cast<size_t>(cur_ - src_)}; }

  const char* src_;
  const char* cur_;
  const char* mark_ = nullptr;
  const char* lim_;
  const char* tok_ = nullptr;
};
