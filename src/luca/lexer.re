#include "lexer.hpp"

std::expected<token, lex_err> lexer::next() noexcept {
  for(;;) {
    tok_ = cur_;
  /*!re2c
    re2c:define:YYCTYPE     = "char";
    re2c:define:YYCURSOR    = "cur_";
    re2c:define:YYMARKER    = "mark_";
    re2c:define:YYLIMIT     = "lim_";
    re2c:yyfill:enable      = 0;
    re2c:eof                = 0;

    id_start     = [a-zA-Z_];
    id_char      = [a-zA-Z0-9_\-];
    id           = id_start id_char*;
    number       = [0-9]+;
    ws           = [ \t\r\n]+;
    glued_err    = number [a-zA-Z_\-] id_char*;

    str_char     = [^"\\\n\x00];
    str_esc      = [\\] [^\n\x00];
    string       = ["] (str_char | str_esc)* ["];
    str_err      = ["] (str_char | str_esc)*;

    * { return std::unexpected{lex_err_unknown{}}; }
    $ { return std::unexpected{lex_err_eof{}}; }

    "("        { return tk::lparen{}; }
    ")"        { return tk::rparen{}; }
    "+"        { return tk::op_plus{}; }
    "-"        { return tk::op_minus{}; }
    "*"        { return tk::op_mul{}; }
    "/"        { return tk::op_div{}; }
    "="        { return tk::op_eq{}; }
    ","        { return tk::op_comma{}; }
    ":"        { return tk::op_colon{}; }
    "."        { return tk::op_dot{}; }

    "\\"        { return tk::kw_lambda{}; }
    "lambda"    { return tk::kw_lambda{}; }

    id         { return tk::id{.name = lexeme()}; }
    number     { return tk::li_int{.value = lexeme()}; }
    string     {
      return tk::li_str{
        .raw = std::string_view{tok_ + 1, static_cast<size_t>(cur_ - tok_ -2)}
      };
    }

    str_err    { return std::unexpected{lex_err_unknown{}}; }
    glued_err  { return std::unexpected{lex_err_unknown{}}; }
    ws         { continue; }
  */
  }
}
