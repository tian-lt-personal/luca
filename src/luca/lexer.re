#include "lexer.hpp"

lex_result lexer::next() noexcept {
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
    id_char      = [a-zA-Z0-9_];
    id           = id_start (id_char | "-"+ id_char+)*;
    number       = [0-9]+;
    ws           = [ \t\r\n]+;
    comment      = "//" [^\n\x00]*;
    glued_err    = number [a-zA-Z_\-] id_char*;

    str_char     = [^"\\\n\x00];
    str_esc      = [\\] [^\n\x00];
    string       = ["] (str_char | str_esc)* ["];
    str_err      = ["] (str_char | str_esc)*;

    *          { return std::unexpected{lex_err_char{.loc = cur_span()}}; }
    $          {
      // point end-of-input at the end of the last line with content
      size_t off = static_cast<size_t>(cur_ - src_);
      while (off > 0 && src_[off - 1] == '\n') --off;
      return std::unexpected{lex_err_eof{.loc = {off, off}}};
    }

    "("        { return token_span{tk::lparen{}, cur_span()}; }
    ")"        { return token_span{tk::rparen{}, cur_span()}; }
    "{"        { return token_span{tk::lbrace{}, cur_span()}; }
    "}"        { return token_span{tk::rbrace{}, cur_span()}; }
    "+"        { return token_span{tk::op_plus{}, cur_span()}; }
    "-"        { return token_span{tk::op_minus{}, cur_span()}; }
    "*"        { return token_span{tk::op_mul{}, cur_span()}; }
    "/"        { return token_span{tk::op_div{}, cur_span()}; }
    "="        { return token_span{tk::op_eq{}, cur_span()}; }
    "!="       { return token_span{tk::op_ne{}, cur_span()}; }
    ">"        { return token_span{tk::op_gt{}, cur_span()}; }
    "<"        { return token_span{tk::op_lt{}, cur_span()}; }
    "|"        { return token_span{tk::op_bar{}, cur_span()}; }
    "->"       { return token_span{tk::op_arrow{}, cur_span()}; }
    ","        { return token_span{tk::op_comma{}, cur_span()}; }
    ":"        { return token_span{tk::op_colon{}, cur_span()}; }
    "."        { return token_span{tk::op_dot{}, cur_span()}; }

    "\\"        { return token_span{tk::kw_lambda{}, cur_span()}; }
    "lambda"    { return token_span{tk::kw_lambda{}, cur_span()}; }
    "let"       { return token_span{tk::kw_let{}, cur_span()}; }
    "in"        { return token_span{tk::kw_in{}, cur_span()}; }
    "if"        { return token_span{tk::kw_if{}, cur_span()}; }
    "then"      { return token_span{tk::kw_then{}, cur_span()}; }
    "else"      { return token_span{tk::kw_else{}, cur_span()}; }
    "bool"      { return token_span{tk::kw_bool{}, cur_span()}; }
    "true"      { return token_span{tk::kw_true{}, cur_span()}; }
    "false"     { return token_span{tk::kw_false{}, cur_span()}; }
    "int"       { return token_span{tk::kw_int{}, cur_span()}; }
    "string"    { return token_span{tk::kw_string{}, cur_span()}; }
    "fix"       { return token_span{tk::kw_fix{}, cur_span()}; }
    "type"      { return token_span{tk::kw_type{}, cur_span()}; }
    "of"        { return token_span{tk::kw_of{}, cur_span()}; }
    "match"     { return token_span{tk::kw_match{}, cur_span()}; }
    "with"      { return token_span{tk::kw_with{}, cur_span()}; }
    "import"    { return token_span{tk::kw_import{}, cur_span()}; }
    "export"    { return token_span{tk::kw_export{}, cur_span()}; }

    id         { return token_span{tk::id{.name = lexeme()}, cur_span()}; }
    number     { return token_span{tk::li_int{.value = lexeme()}, cur_span()}; }
    string     {
      return token_span{
        tk::li_str{.raw = std::string_view{tok_ + 1, static_cast<size_t>(cur_ - tok_ -2)}},
        cur_span()
      };
    }

    str_err    { return std::unexpected{lex_err_str{.loc = cur_span()}}; }
    glued_err  { return std::unexpected{lex_err_glued{.loc = cur_span()}}; }
    ws         { continue; }
    comment    { continue; }
  */
  }
}
